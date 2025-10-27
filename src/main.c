#include "app.h"
#include "app_bootstrap.h"
#include "app_cli.h"
#include "assets.h"
#include "at_log.h"
#include "camera_controller.h"
#include "detail_content_builder.h"
#include "detail_view.h"
#include "event.h"
#include "expansion.h"
#include "file_dialog.h"
#include "graphics.h"
#include "interaction.h"
#include "layout.h"
#include "path_utils.h"
#include "persistence.h"
#include "person.h"
#include "render.h"
#include "settings.h"
#include "settings_runtime.h"
#include "shortcuts.h"
#include "status_messages.h"
#include "tree.h"
#include "ui.h"

#include <ctype.h>
#include <math.h>
#include <stdbool.h>
#include <stdio.h>
#include <string.h>
#include <time.h>

#if defined(ANCESTRYTREE_HAVE_RAYLIB)
#include <raylib.h>
#endif

#define APP_DEFAULT_SAVE_PATH "assets/manual_save.json"
#define APP_SETTINGS_PATH "assets/settings.cfg"
#define APP_AUTO_SAVE_PATH "assets/auto_save.json"
#define APP_LOG_PATH "ancestrytree.log"

#if defined(ANCESTRYTREE_HAVE_RAYLIB)

static bool app_try_find_asset(const char *relative_path, char *resolved_path, size_t capacity)
{
    if (!relative_path || !resolved_path || capacity == 0U)
    {
        return false;
    }

    const unsigned int level_candidates[] = {0U, 1U, 2U, 3U};
    const char *application_dir           = GetApplicationDirectory();
    for (size_t index = 0U; index < sizeof(level_candidates) / sizeof(level_candidates[0]); ++index)
    {
        if (path_join_relative(application_dir, level_candidates[index], relative_path,
                               resolved_path, capacity) &&
            FileExists(resolved_path))
        {
            return true;
        }
    }

    char buffer_one[512];
    char buffer_two[512];
    snprintf(buffer_one, sizeof(buffer_one), "../%s", relative_path);
    snprintf(buffer_two, sizeof(buffer_two), "../../%s", relative_path);
    const char *fallbacks[] = {relative_path, buffer_one, buffer_two};

    for (size_t index = 0U; index < sizeof(fallbacks) / sizeof(fallbacks[0]); ++index)
    {
        const char *candidate = fallbacks[index];
        if (!candidate)
        {
            continue;
        }
        size_t length = strlen(candidate);
        if (length >= capacity)
        {
            continue;
        }
        memcpy(resolved_path, candidate, length + 1U);
        if (FileExists(resolved_path))
        {
            return true;
        }
    }
    return false;
}

static bool layout_compute_center(const LayoutResult *layout, float out_center[3])
{
    if (!layout || !out_center || layout->count == 0U)
    {
        return false;
    }
    out_center[0] = 0.0f;
    out_center[1] = 0.0f;
    out_center[2] = 0.0f;
    for (size_t index = 0U; index < layout->count; ++index)
    {
        out_center[0] += layout->nodes[index].position[0];
        out_center[1] += layout->nodes[index].position[1];
        out_center[2] += layout->nodes[index].position[2];
    }
    float count = (float)layout->count;
    out_center[0] /= count;
    out_center[1] /= count;
    out_center[2] /= count;
    return true;
}

static FamilyTree *app_create_placeholder_tree(void);
static FamilyTree *app_auto_save_tree_supplier(void *user_data);
static void app_apply_settings(const Settings *settings, RenderState *render_state,
                               CameraController *camera, PersistenceAutoSave *auto_save);

static void app_file_state_clear(AppFileState *state)
{
    if (!state)
    {
        return;
    }
    state->current_path[0] = '\0';
}

static void app_file_state_set(AppFileState *state, const char *path)
{
    if (!state)
    {
        return;
    }
    if (!path || path[0] == '\0')
    {
        app_file_state_clear(state);
        return;
    }
#if defined(_MSC_VER)
    (void)strncpy_s(state->current_path, sizeof(state->current_path), path, _TRUNCATE);
#else
    (void)snprintf(state->current_path, sizeof(state->current_path), "%s", path);
#endif
}

static void app_report_status(UIContext *ui, AtLogger *logger, const char *message)
{
    if (!message)
    {
        return;
    }
    (void)ui_notify_status(ui, message);
    if (logger)
    {
        AT_LOG(logger, AT_LOG_INFO, "%s", message);
    }
}

static void app_report_error(UIContext *ui, AtLogger *logger, const char *message)
{
    if (!message)
    {
        return;
    }
    if (!ui_show_error_dialog(ui, "Application Error", message))
    {
        (void)ui_notify_status(ui, message);
    }
    if (logger)
    {
        AT_LOG(logger, AT_LOG_ERROR, "%s", message);
    }
}

static bool app_path_has_prefix_ci(const char *value, const char *prefix)
{
    if (!value || !prefix)
    {
        return false;
    }
    for (size_t index = 0U; prefix[index] != '\0'; ++index)
    {
        char lhs = value[index];
        char rhs = prefix[index];
        if (lhs == '\0')
        {
            return false;
        }
        lhs = (char)tolower((unsigned char)lhs);
        rhs = (char)tolower((unsigned char)rhs);
        if (lhs != rhs)
        {
            return false;
        }
    }
    return true;
}

static bool app_path_relativize_if_under_assets(const char *path, char *output, size_t capacity)
{
    if (!path || !output || capacity == 0U)
    {
        return false;
    }

    char normalized[512];
    size_t write = 0U;
    for (size_t index = 0U; path[index] != '\0'; ++index)
    {
        char c = path[index];
        if (c == '\\')
        {
            c = '/';
        }
        if (write + 1U >= sizeof(normalized))
        {
            return false;
        }
        normalized[write++] = c;
    }
    if (write == 0U)
    {
        return false;
    }
    normalized[write] = '\0';

    bool has_drive        = strchr(normalized, ':') != NULL;
    bool starts_with_root = normalized[0] == '/' || normalized[0] == '\\';

    size_t offset = 0U;
    while (normalized[offset] == '.' && normalized[offset + 1U] == '/')
    {
        offset += 2U;
    }

    const char *candidate = &normalized[offset];
    if (*candidate == '\0')
    {
        return false;
    }

    if (app_path_has_prefix_ci(candidate, "assets/"))
    {
        candidate += 7U;
        if (*candidate == '\0')
        {
            return false;
        }
        size_t length = strlen(candidate);
        if (length + 1U > capacity)
        {
            return false;
        }
        memcpy(output, candidate, length + 1U);
        return true;
    }

    if (!has_drive && !starts_with_root)
    {
        size_t length = strlen(candidate);
        if (length + 1U > capacity)
        {
            return false;
        }
        memcpy(output, candidate, length + 1U);
        return true;
    }

    return false;
}

static void app_append_message(char *buffer, size_t capacity, const char *message)
{
    if (!buffer || capacity == 0U || !message || message[0] == '\0')
    {
        return;
    }
    size_t existing = strlen(buffer);
    if (existing > 0U && existing + 1U < capacity)
    {
        buffer[existing++] = ' ';
        buffer[existing]   = '\0';
    }
    size_t available = (capacity - 1U > existing) ? (capacity - 1U - existing) : 0U;
    if (available == 0U)
    {
        return;
    }
    size_t length = strlen(message);
    if (length > available)
    {
        length = available;
    }
    memcpy(buffer + existing, message, length);
    buffer[existing + length] = '\0';
}

static bool app_prepare_asset_reference(const char *input_path, const char *subdirectory,
                                        const char *prefix, char *output, size_t capacity,
                                        const char **out_relative, bool *out_copied,
                                        char *error_buffer, size_t error_buffer_size)
{
    if (out_relative)
    {
        *out_relative = NULL;
    }
    if (out_copied)
    {
        *out_copied = false;
    }
    if (error_buffer && error_buffer_size > 0U)
    {
        error_buffer[0] = '\0';
    }
    if (!input_path || input_path[0] == '\0')
    {
        if (output && capacity > 0U)
        {
            output[0] = '\0';
        }
        return true;
    }

    if (app_path_relativize_if_under_assets(input_path, output, capacity))
    {
        if (out_relative)
        {
            *out_relative = output;
        }
        return true;
    }

    AssetCopyRequest request;
    request.source_path  = input_path;
    request.asset_root   = "assets";
    request.subdirectory = subdirectory;
    request.name_prefix  = prefix;
    if (!asset_copy(&request, output, capacity, error_buffer, error_buffer_size))
    {
        if (out_relative)
        {
            *out_relative = NULL;
        }
        return false;
    }
    if (out_relative)
    {
        *out_relative = output;
    }
    if (out_copied)
    {
        *out_copied = true;
    }
    return true;
}

static void app_remove_copied_asset(const char *relative_path)
{
    if (!relative_path || relative_path[0] == '\0')
    {
        return;
    }
    char absolute[512];
    if (snprintf(absolute, sizeof(absolute), "assets/%s", relative_path) >= (int)sizeof(absolute))
    {
        return;
    }
    for (size_t index = 0U; absolute[index] != '\0'; ++index)
    {
        if (absolute[index] == '\\')
        {
            absolute[index] = '/';
        }
    }
    (void)remove(absolute);
}

static void app_focus_camera_on_layout(CameraController *camera, const LayoutResult *layout)
{
    if (!camera || !layout)
    {
        return;
    }
    float center[3];
    if (layout_compute_center(layout, center))
    {
        float radius = camera->config.default_radius;
        if (!(radius > 0.0f))
        {
            radius = 14.0f;
        }
        camera_controller_focus(camera, center, radius);
    }
}

static bool app_generate_timestamped_path(char *buffer, size_t capacity)
{
    if (!buffer || capacity == 0U)
    {
        return false;
    }
    time_t now = time(NULL);
    if (now == (time_t)-1)
    {
        return false;
    }
    struct tm time_info;
#if defined(_WIN32)
    if (localtime_s(&time_info, &now) != 0)
    {
        return false;
    }
#else
    if (!localtime_r(&now, &time_info))
    {
        return false;
    }
#endif
    return strftime(buffer, capacity, "assets/manual_save_%Y%m%d_%H%M%S.json", &time_info) != 0;
}

static bool app_save_tree_to_path(const FamilyTree *tree, const char *path, char *error_buffer,
                                  size_t error_buffer_size)
{
    if (!tree || !path || path[0] == '\0')
    {
        return false;
    }
    return persistence_tree_save(tree, path, error_buffer, error_buffer_size);
}

static bool app_handle_save(FamilyTree *tree, AppFileState *files, char *error_buffer,
                            size_t error_buffer_size)
{
    if (!tree || !files)
    {
        return false;
    }
    const char *target_path =
        files->current_path[0] != '\0' ? files->current_path : APP_DEFAULT_SAVE_PATH;
    if (!app_save_tree_to_path(tree, target_path, error_buffer, error_buffer_size))
    {
        return false;
    }
    app_file_state_set(files, target_path);
    return true;
}

static bool app_handle_save_as(FamilyTree *tree, AppFileState *files, const char *requested_path,
                               char *error_buffer, size_t error_buffer_size, char *out_path,
                               size_t out_path_capacity)
{
    if (!tree || !files || !out_path || out_path_capacity == 0U)
    {
        return false;
    }
    const char *target = requested_path;
    char generated_path[512];
    if (!target || target[0] == '\0')
    {
        if (!app_generate_timestamped_path(generated_path, sizeof(generated_path)))
        {
            return false;
        }
        target = generated_path;
    }
    if (!app_save_tree_to_path(tree, target, error_buffer, error_buffer_size))
    {
        return false;
    }
    app_file_state_set(files, target);
#if defined(_MSC_VER)
    (void)strncpy_s(out_path, out_path_capacity, target, _TRUNCATE);
#else
    (void)snprintf(out_path, out_path_capacity, "%s", target);
#endif
    return true;
}

static LayoutAlgorithm app_select_layout_algorithm(const AppState *app_state,
                                                   const Settings *settings)
{
    if (app_state)
    {
        return app_state->active_layout_algorithm;
    }
    if (settings && settings->default_layout_algorithm == SETTINGS_LAYOUT_ALGORITHM_FORCE_DIRECTED)
    {
        return LAYOUT_ALGORITHM_FORCE_DIRECTED;
    }
    return LAYOUT_ALGORITHM_HIERARCHICAL;
}

static bool app_swap_tree(FamilyTree **tree, LayoutResult *layout, FamilyTree *replacement,
                          LayoutAlgorithm algorithm)
{
    if (!tree || !layout || !replacement)
    {
        return false;
    }
    LayoutResult new_layout = layout_calculate_with_algorithm(replacement, algorithm);
    if (replacement->person_count > 0U && (!new_layout.nodes || new_layout.count == 0U))
    {
        layout_result_destroy(&new_layout);
        return false;
    }
    layout_result_destroy(layout);
    if (*tree)
    {
        family_tree_destroy(*tree);
    }
    *tree   = replacement;
    *layout = new_layout;
    return true;
}

static void app_on_tree_changed(LayoutResult *layout, InteractionState *interaction_state,
                                RenderState *render_state, CameraController *camera,
                                PersistenceAutoSave *auto_save)
{
    if (!layout || !interaction_state || !render_state)
    {
        return;
    }
    interaction_clear_selection(interaction_state);
    interaction_state_set_pick_radius(interaction_state, render_state->config.sphere_radius);
    app_focus_camera_on_layout(camera, layout);
    if (auto_save)
    {
        persistence_auto_save_mark_dirty(auto_save);
    }
}

static bool app_prompt_open_tree_path(const AppFileState *file_state, char *out_path,
                                      size_t capacity, char *error_buffer, size_t error_capacity)
{
    if (!out_path || capacity == 0U)
    {
        if (error_buffer && error_capacity > 0U)
        {
#if defined(_MSC_VER)
            (void)strncpy_s(error_buffer, error_capacity, "Invalid output buffer for open dialog.",
                            _TRUNCATE);
#else
            (void)snprintf(error_buffer, error_capacity, "%s",
                           "Invalid output buffer for open dialog.");
#endif
        }
        return false;
    }
    const char *default_path = NULL;
    if (file_state && file_state->current_path[0] != '\0')
    {
        default_path = file_state->current_path;
    }
    else
    {
        default_path = "assets/example_tree.json";
    }
    FileDialogFilter filters[] = {{.label = "Family Trees", .pattern = "*.json"},
                                  {.label = "All Files", .pattern = "*.*"}};
    FileDialogOptions options  = {.title        = "Open Family Tree",
                                  .default_path = default_path,
                                  .filters      = filters,
                                  .filter_count = sizeof(filters) / sizeof(filters[0])};
    return file_dialog_open(&options, out_path, capacity, error_buffer, error_capacity);
}

static bool app_prompt_save_tree_path(const AppFileState *file_state, char *out_path,
                                      size_t capacity, char *error_buffer, size_t error_capacity)
{
    if (!out_path || capacity == 0U)
    {
        if (error_buffer && error_capacity > 0U)
        {
#if defined(_MSC_VER)
            (void)strncpy_s(error_buffer, error_capacity, "Invalid output buffer for save dialog.",
                            _TRUNCATE);
#else
            (void)snprintf(error_buffer, error_capacity, "%s",
                           "Invalid output buffer for save dialog.");
#endif
        }
        return false;
    }
    const char *default_path = APP_DEFAULT_SAVE_PATH;
    if (file_state && file_state->current_path[0] != '\0')
    {
        default_path = file_state->current_path;
    }
    FileDialogFilter filters[] = {{.label = "Family Trees", .pattern = "*.json"},
                                  {.label = "All Files", .pattern = "*.*"}};
    FileDialogOptions options  = {.title        = "Save Family Tree",
                                  .default_path = default_path,
                                  .filters      = filters,
                                  .filter_count = sizeof(filters) / sizeof(filters[0])};
    if (!file_dialog_save(&options, out_path, capacity, error_buffer, error_capacity))
    {
        return false;
    }
    if (!file_dialog_ensure_extension(out_path, capacity, ".json"))
    {
        if (error_buffer && error_capacity > 0U)
        {
#if defined(_MSC_VER)
            (void)strncpy_s(error_buffer, error_capacity,
                            "Failed to append .json extension to selected save path.", _TRUNCATE);
#else
            (void)snprintf(error_buffer, error_capacity, "%s",
                           "Failed to append .json extension to selected save path.");
#endif
        }
        return false;
    }
    return true;
}

static void app_process_ui_event(const UIEvent *event, UIContext *ui, AppFileState *file_state,
                                 FamilyTree **tree, LayoutResult *layout,
                                 InteractionState *interaction_state, RenderState *render_state,
                                 CameraController *camera, AtLogger *logger, Settings *settings,
                                 Settings *persisted_settings, const char *settings_path,
                                 PersistenceAutoSave *auto_save,
                                 unsigned int *settings_applied_revision, AppState *app_state)
{
    if (!event || !tree || !layout || !interaction_state || !render_state)
    {
        return;
    }

    UIEventType type = event->type;
    char error_buffer[256];
    error_buffer[0] = '\0';

    switch (type)
    {
    case UI_EVENT_NEW_TREE:
    {
        FamilyTree *replacement = app_create_placeholder_tree();
        if (!replacement)
        {
            app_report_error(ui, logger, "Failed to create new tree.");
            return;
        }
        LayoutAlgorithm algorithm = app_select_layout_algorithm(app_state, settings);
        if (!app_swap_tree(tree, layout, replacement, algorithm))
        {
            family_tree_destroy(replacement);
            app_report_error(ui, logger, "Unable to swap in new tree data.");
            return;
        }
        if (app_state)
        {
            app_state_on_tree_replaced(app_state);
            app_state->active_layout_algorithm = algorithm;
        }
        app_file_state_clear(file_state);
        app_on_tree_changed(layout, interaction_state, render_state, camera, auto_save);
        if (app_state)
        {
            app_state_reset_history(app_state);
            app_state_mark_tree_dirty(app_state);
        }
        app_report_status(ui, logger, "New placeholder tree created.");
    }
    break;
    case UI_EVENT_OPEN_TREE:
    {
        char dialog_error[256];
        dialog_error[0] = '\0';
        char chosen_path[512];
        if (!app_prompt_open_tree_path(file_state, chosen_path, sizeof(chosen_path), dialog_error,
                                       sizeof(dialog_error)))
        {
            if (dialog_error[0] != '\0')
            {
                char message[320];
                (void)snprintf(message, sizeof(message), "Open dialog failed: %s", dialog_error);
                app_report_error(ui, logger, message);
            }
            else
            {
                app_report_status(ui, logger, "Open request canceled.");
            }
            return;
        }
        bool progress_started = false;
        if (ui)
        {
            ui_progress_begin(ui, "Loading holographic archive...");
            progress_started = true;
            ui_progress_update(ui, 0.15f);
        }
        FamilyTree *loaded = persistence_tree_load(chosen_path, error_buffer, sizeof(error_buffer));
        if (!loaded)
        {
            char message[256];
            (void)snprintf(message, sizeof(message), "Load failed: %s", error_buffer);
            if (progress_started)
            {
                ui_progress_complete(ui, false, message);
            }
            app_report_error(ui, logger, message);
            return;
        }
        if (progress_started)
        {
            ui_progress_update(ui, 0.55f);
        }
        LayoutAlgorithm algorithm = app_select_layout_algorithm(app_state, settings);
        if (!app_swap_tree(tree, layout, loaded, algorithm))
        {
            family_tree_destroy(loaded);
            if (progress_started)
            {
                ui_progress_complete(ui, false, "Unable to replace current tree with loaded data.");
            }
            app_report_error(ui, logger, "Unable to replace current tree with loaded data.");
            return;
        }
        if (app_state)
        {
            app_state_on_tree_replaced(app_state);
            app_state->active_layout_algorithm = algorithm;
        }
        app_file_state_set(file_state, chosen_path);
        app_on_tree_changed(layout, interaction_state, render_state, camera, auto_save);
        if (app_state)
        {
            app_state_reset_history(app_state);
            app_state_clear_tree_dirty(app_state);
        }
        if (progress_started)
        {
            ui_progress_update(ui, 0.9f);
        }
        char status_message[320];
        (void)snprintf(status_message, sizeof(status_message), "Loaded tree from %s", chosen_path);
        if (progress_started)
        {
            ui_progress_update(ui, 1.0f);
            ui_progress_complete(ui, true, status_message);
        }
        app_report_status(ui, logger, status_message);
    }
    break;
    case UI_EVENT_SAVE_TREE:
        if (!*tree)
        {
            app_report_error(ui, logger, "No tree available to save.");
            return;
        }
        if (file_state->current_path[0] == '\0')
        {
            char dialog_error[256];
            dialog_error[0] = '\0';
            char destination[512];
            if (!app_prompt_save_tree_path(file_state, destination, sizeof(destination),
                                           dialog_error, sizeof(dialog_error)))
            {
                if (dialog_error[0] != '\0')
                {
                    char message[320];
                    (void)snprintf(message, sizeof(message), "Save dialog failed: %s",
                                   dialog_error);
                    app_report_error(ui, logger, message);
                }
                else
                {
                    app_report_status(ui, logger, "Save request canceled.");
                }
                return;
            }
            char saved_path[512];
            bool progress_started = false;
            if (ui)
            {
                ui_progress_begin(ui, "Saving holographic archive...");
                progress_started = true;
                ui_progress_update(ui, 0.2f);
            }
            if (!app_handle_save_as(*tree, file_state, destination, error_buffer,
                                    sizeof(error_buffer), saved_path, sizeof(saved_path)))
            {
                char message[320];
                (void)snprintf(message, sizeof(message), "Save failed: %s", error_buffer);
                if (progress_started)
                {
                    ui_progress_complete(ui, false, message);
                }
                app_report_error(ui, logger, message);
                return;
            }
            if (app_state)
            {
                app_state_clear_tree_dirty(app_state);
            }
            char message[320];
            (void)snprintf(message, sizeof(message), "Saved tree to %s", saved_path);
            if (progress_started)
            {
                ui_progress_update(ui, 0.85f);
                ui_progress_complete(ui, true, message);
            }
            app_report_status(ui, logger, message);
        }
        else
        {
            bool progress_started = false;
            if (ui)
            {
                ui_progress_begin(ui, "Saving holographic archive...");
                progress_started = true;
                ui_progress_update(ui, 0.2f);
            }
            if (!app_handle_save(*tree, file_state, error_buffer, sizeof(error_buffer)))
            {
                char message[256];
                (void)snprintf(message, sizeof(message), "Save failed: %s", error_buffer);
                if (progress_started)
                {
                    ui_progress_complete(ui, false, message);
                }
                app_report_error(ui, logger, message);
                return;
            }
            if (app_state)
            {
                app_state_clear_tree_dirty(app_state);
            }
            char message[256];
            (void)snprintf(message, sizeof(message), "Saved tree to %s", file_state->current_path);
            if (progress_started)
            {
                ui_progress_update(ui, 0.85f);
                ui_progress_complete(ui, true, message);
            }
            app_report_status(ui, logger, message);
        }
        break;
    case UI_EVENT_SAVE_TREE_AS:
        if (!*tree)
        {
            app_report_error(ui, logger, "No tree available to save.");
            return;
        }
        else
        {
            char destination[512];
            char dialog_error[256];
            dialog_error[0] = '\0';
            if (!app_prompt_save_tree_path(file_state, destination, sizeof(destination),
                                           dialog_error, sizeof(dialog_error)))
            {
                if (dialog_error[0] != '\0')
                {
                    char message[320];
                    (void)snprintf(message, sizeof(message), "Save As dialog failed: %s",
                                   dialog_error);
                    app_report_error(ui, logger, message);
                }
                else
                {
                    app_report_status(ui, logger, "Save As request canceled.");
                }
                return;
            }
            char saved_path[512];
            bool progress_started = false;
            if (ui)
            {
                ui_progress_begin(ui, "Saving holographic archive...");
                progress_started = true;
                ui_progress_update(ui, 0.2f);
            }
            if (!app_handle_save_as(*tree, file_state, destination, error_buffer,
                                    sizeof(error_buffer), saved_path, sizeof(saved_path)))
            {
                char message[256];
                (void)snprintf(message, sizeof(message), "Save As failed: %s", error_buffer);
                if (progress_started)
                {
                    ui_progress_complete(ui, false, message);
                }
                app_report_error(ui, logger, message);
                return;
            }
            if (app_state)
            {
                app_state_clear_tree_dirty(app_state);
            }
            char message[256];
            (void)snprintf(message, sizeof(message), "Saved tree to %s", saved_path);
            if (progress_started)
            {
                ui_progress_update(ui, 0.85f);
                ui_progress_complete(ui, true, message);
            }
            app_report_status(ui, logger, message);
        }
        break;
    case UI_EVENT_DELETE_PERSON:
    {
        uint32_t person_id = event->param_u32;
        if (!app_state || person_id == 0U)
        {
            app_report_error(ui, logger, "Invalid delete request.");
            break;
        }
        char name_buffer[128];
        name_buffer[0] = '\0';
        if (tree && *tree)
        {
            Person *target = family_tree_find_person(*tree, person_id);
            if (target && !person_format_display_name(target, name_buffer, sizeof(name_buffer)))
            {
                (void)snprintf(name_buffer, sizeof(name_buffer), "Person %u", target->id);
            }
        }
        AppCommand *command = app_command_create_delete_person(person_id);
        if (!command)
        {
            app_report_error(ui, logger, "Failed to build delete command.");
            break;
        }
        char delete_error[256];
        delete_error[0] = '\0';
        if (!app_state_push_command(app_state, command, delete_error, sizeof(delete_error)))
        {
            char message[256];
            if (delete_error[0] != '\0')
            {
                (void)snprintf(message, sizeof(message), "Unable to delete person: %s",
                               delete_error);
            }
            else
            {
                (void)snprintf(message, sizeof(message),
                               "Unable to delete person due to an unknown error.");
            }
            app_report_error(ui, logger, message);
            break;
        }
        if (interaction_state)
        {
            interaction_clear_selection(interaction_state);
        }
        if (auto_save)
        {
            persistence_auto_save_mark_dirty(auto_save);
        }
        char status_message[192];
        if (name_buffer[0] != '\0')
        {
            (void)snprintf(status_message, sizeof(status_message), "Deleted profile for %s.",
                           name_buffer);
        }
        else
        {
            (void)snprintf(status_message, sizeof(status_message), "Deleted person #%u.",
                           person_id);
        }
        app_report_status(ui, logger, status_message);
        if (logger)
        {
            AT_LOG(logger, AT_LOG_INFO, "Deleted person %u via Edit Person panel", person_id);
        }
    }
    break;
    case UI_EVENT_SAVE_SETTINGS:
    {
        if (!settings || !settings_path)
        {
            app_report_error(ui, logger,
                             "Settings context unavailable; unable to save configuration.");
            break;
        }
        char settings_error[256];
        settings_error[0] = '\0';
        if (!settings_save(settings, settings_path, settings_error, sizeof(settings_error)))
        {
            char message[256];
            if (settings_error[0] != '\0')
            {
                (void)snprintf(message, sizeof(message), "Failed to save settings: %s",
                               settings_error);
            }
            else
            {
                (void)snprintf(message, sizeof(message), "Failed to save settings to %s",
                               settings_path);
            }
            app_report_error(ui, logger, message);
        }
        else
        {
            if (persisted_settings)
            {
                *persisted_settings = *settings;
            }
            app_report_status(ui, logger, "Settings saved.");
        }
    }
    break;
    case UI_EVENT_RELOAD_SETTINGS:
    {
        if (!settings || !settings_path)
        {
            app_report_error(ui, logger,
                             "Settings context unavailable; unable to reload configuration.");
            break;
        }
        Settings reloaded;
        settings_init_defaults(&reloaded);
        char settings_error[256];
        settings_error[0] = '\0';
        if (!settings_try_load(&reloaded, settings_path, settings_error, sizeof(settings_error)))
        {
            char message[256];
            if (settings_error[0] != '\0')
            {
                (void)snprintf(message, sizeof(message), "Failed to reload settings: %s",
                               settings_error);
            }
            else
            {
                (void)snprintf(message, sizeof(message), "Failed to reload settings from %s",
                               settings_path);
            }
            app_report_error(ui, logger, message);
        }
        else
        {
            *settings = reloaded;
            if (persisted_settings)
            {
                *persisted_settings = reloaded;
            }
            app_apply_settings(settings, render_state, camera, auto_save);
            if (settings_applied_revision)
            {
                *settings_applied_revision = settings_get_revision(settings);
            }
            app_report_status(ui, logger, "Settings reloaded from disk.");
        }
    }
    break;
    case UI_EVENT_UNDO:
    {
        if (!app_state)
        {
            app_report_error(ui, logger, "Undo unavailable; application state not configured.");
            break;
        }
        char command_error[256];
        command_error[0] = '\0';
        if (!app_state_undo(app_state, command_error, sizeof(command_error)))
        {
            if (command_error[0] != '\0')
            {
                app_report_status(ui, logger, command_error);
            }
            else
            {
                app_report_status(ui, logger, "Nothing to undo.");
            }
        }
        else
        {
            app_report_status(ui, logger, "Undo applied.");
        }
    }
    break;
    case UI_EVENT_REDO:
    {
        if (!app_state)
        {
            app_report_error(ui, logger, "Redo unavailable; application state not configured.");
            break;
        }
        char command_error[256];
        command_error[0] = '\0';
        if (!app_state_redo(app_state, command_error, sizeof(command_error)))
        {
            if (command_error[0] != '\0')
            {
                app_report_status(ui, logger, command_error);
            }
            else
            {
                app_report_status(ui, logger, "Nothing to redo.");
            }
        }
        else
        {
            app_report_status(ui, logger, "Redo applied.");
        }
    }
    break;
    case UI_EVENT_RESET_CAMERA:
        camera_controller_reset(camera);
        app_report_status(ui, logger, "Camera reset to default orbit.");
        break;
    case UI_EVENT_ESCAPE:
    {
        const Person *selected_before = interaction_get_selected(interaction_state);
        if (selected_before)
        {
            interaction_clear_selection(interaction_state);
        }
        bool dismissed = ui_handle_escape(ui);
        if (dismissed && selected_before)
        {
            app_report_status(ui, logger, "Selection cleared and dialogs dismissed.");
        }
        else if (dismissed)
        {
            app_report_status(ui, logger, "Dialogs dismissed.");
        }
        else if (selected_before)
        {
            app_report_status(ui, logger, "Selection cleared.");
        }
    }
    break;
    case UI_EVENT_REQUEST_EXIT:
#if defined(ANCESTRYTREE_HAVE_RAYLIB)
        CloseWindow();
#else
        (void)camera;
#endif
        app_report_status(ui, logger, "Exit requested.");
        break;
    case UI_EVENT_FOCUS_PERSON:
    {
        uint32_t target_id = event->param_u32;
        if (target_id == 0U || !*tree)
        {
            app_report_error(ui, logger, "Focus request failed; tree unavailable.");
            break;
        }
        Person *target = family_tree_find_person(*tree, target_id);
        if (!target)
        {
            char message[160];
            (void)snprintf(message, sizeof(message), "Focus failed: person %u not found.",
                           target_id);
            app_report_error(ui, logger, message);
            break;
        }
        if (interaction_state)
        {
            interaction_select_person(interaction_state, target);
        }
        bool has_position       = false;
        float focus_position[3] = {0.0f, 0.0f, 0.0f};
        for (size_t index = 0U; index < layout->count; ++index)
        {
            if (layout->nodes[index].person == target)
            {
                focus_position[0] = layout->nodes[index].position[0];
                focus_position[1] = layout->nodes[index].position[1];
                focus_position[2] = layout->nodes[index].position[2];
                has_position      = true;
                break;
            }
        }
        if (camera && has_position)
        {
            camera_controller_focus(camera, focus_position, camera->config.default_radius);
        }
        char name_buffer[128];
        if (!person_format_display_name(target, name_buffer, sizeof(name_buffer)))
        {
            (void)snprintf(name_buffer, sizeof(name_buffer), "Person %u", target->id);
        }
        char message[192];
        if (has_position)
        {
            (void)snprintf(message, sizeof(message), "Focused on %s.", name_buffer);
        }
        else
        {
            (void)snprintf(message, sizeof(message),
                           "Selected %s; focus will update after layout refresh.", name_buffer);
        }
        app_report_status(ui, logger, message);
    }
    break;
    case UI_EVENT_NONE:
    default:
        break;
    }
}

static void app_handle_pending_ui_events(
    UIContext *ui, AppFileState *file_state, FamilyTree **tree, LayoutResult *layout,
    InteractionState *interaction_state, RenderState *render_state, CameraController *camera,
    AtLogger *logger, Settings *settings, Settings *persisted_settings, const char *settings_path,
    PersistenceAutoSave *auto_save, unsigned int *settings_applied_revision, AppState *app_state)
{
    if (!ui)
    {
        return;
    }
    UIEvent events[UI_EVENT_QUEUE_CAPACITY];
    size_t count = ui_poll_events(ui, events, UI_EVENT_QUEUE_CAPACITY);
    for (size_t index = 0U; index < count; ++index)
    {
        app_process_ui_event(&events[index], ui, file_state, tree, layout, interaction_state,
                             render_state, camera, logger, settings, persisted_settings,
                             settings_path, auto_save, settings_applied_revision, app_state);
    }
}

static void app_process_add_person_requests(UIContext *ui, AppState *app_state, FamilyTree **tree,
                                            LayoutResult *layout,
                                            InteractionState *interaction_state,
                                            CameraController *camera,
                                            PersistenceAutoSave *auto_save, AtLogger *logger)
{
    if (!ui)
    {
        return;
    }

    UIAddPersonRequest request;
    while (ui_consume_add_person_request(ui, &request))
    {
        if (!app_state || !tree || !layout || !*tree)
        {
            app_report_error(ui, logger, "Cannot create person: application state unavailable.");
            continue;
        }

        AppPersonCreateData data;
        memset(&data, 0, sizeof(data));
        data.first          = request.first;
        data.middle         = (request.middle[0] != '\0') ? request.middle : NULL;
        data.last           = request.last;
        data.birth_date     = request.birth_date;
        data.birth_location = (request.birth_location[0] != '\0') ? request.birth_location : NULL;
        data.is_alive       = request.is_alive;
        data.death_date =
            (!request.is_alive && request.death_date[0] != '\0') ? request.death_date : NULL;
        data.death_location = (!request.is_alive && request.death_location[0] != '\0')
                                  ? request.death_location
                                  : NULL;
        data.father_id      = request.father_id;
        data.mother_id      = request.mother_id;
        data.spouse_id      = request.spouse_id;

        char profile_buffer[256];
        const char *profile_path = NULL;
        bool profile_copied      = false;
        char asset_error[256];
        asset_error[0] = '\0';

        if (!app_prepare_asset_reference(request.profile_image_path, "profiles", "profile",
                                         profile_buffer, sizeof(profile_buffer), &profile_path,
                                         &profile_copied, asset_error, sizeof(asset_error)))
        {
            char message[512];
            if (asset_error[0] != '\0')
            {
                (void)snprintf(message, sizeof(message), "Profile import failed: %s", asset_error);
            }
            else
            {
                (void)snprintf(message, sizeof(message), "Profile import failed for path '%s'.",
                               request.profile_image_path);
            }
            app_report_error(ui, logger, message);
            if (profile_copied)
            {
                app_remove_copied_asset(profile_buffer);
            }
            continue;
        }
        data.profile_image_path = profile_path;

        char certificate_buffers[APP_PERSON_CREATE_MAX_CERTIFICATES][256];
        bool certificate_copied[APP_PERSON_CREATE_MAX_CERTIFICATES];
        memset(certificate_copied, 0, sizeof(certificate_copied));
        data.certificate_count = request.certificate_count;
        bool creation_failed   = false;
        char failure_message[512];
        failure_message[0] = '\0';

        for (size_t index = 0U; index < data.certificate_count; ++index)
        {
            const char *source_path = request.certificate_paths[index];
            const char *relative    = NULL;
            asset_error[0]          = '\0';
            if (!app_prepare_asset_reference(
                    source_path, "certificates", "certificate", certificate_buffers[index],
                    sizeof(certificate_buffers[index]), &relative, &certificate_copied[index],
                    asset_error, sizeof(asset_error)))
            {
                if (asset_error[0] != '\0')
                {
                    (void)snprintf(failure_message, sizeof(failure_message),
                                   "Certificate import failed: %s", asset_error);
                }
                else
                {
                    (void)snprintf(failure_message, sizeof(failure_message),
                                   "Certificate import failed for path '%s'.", source_path);
                }
                creation_failed = true;
                break;
            }
            data.certificate_paths[index] = relative;
        }

        if (creation_failed)
        {
            if (profile_copied)
            {
                app_remove_copied_asset(profile_buffer);
            }
            for (size_t cleanup = 0U; cleanup < data.certificate_count; ++cleanup)
            {
                if (certificate_copied[cleanup])
                {
                    app_remove_copied_asset(certificate_buffers[cleanup]);
                }
            }
            app_report_error(ui, logger, failure_message);
            continue;
        }

        for (size_t index = data.certificate_count; index < APP_PERSON_CREATE_MAX_CERTIFICATES;
             ++index)
        {
            data.certificate_paths[index] = NULL;
        }

        data.timeline_count = request.timeline_count;
        for (size_t index = 0U; index < data.timeline_count; ++index)
        {
            data.timeline_entries[index].type = request.timeline_entries[index].type;
            data.timeline_entries[index].date = (request.timeline_entries[index].date[0] != '\0')
                                                    ? request.timeline_entries[index].date
                                                    : NULL;
            data.timeline_entries[index].description = request.timeline_entries[index].description;
            data.timeline_entries[index].location =
                (request.timeline_entries[index].location[0] != '\0')
                    ? request.timeline_entries[index].location
                    : NULL;
        }
        for (size_t index = data.timeline_count; index < APP_PERSON_CREATE_MAX_TIMELINE_ENTRIES;
             ++index)
        {
            data.timeline_entries[index].type        = TIMELINE_EVENT_CUSTOM;
            data.timeline_entries[index].date        = NULL;
            data.timeline_entries[index].description = NULL;
            data.timeline_entries[index].location    = NULL;
        }

        char error_buffer[256];
        error_buffer[0]        = '\0';
        uint32_t new_person_id = 0U;
        if (!app_state_create_person(app_state, &data, &new_person_id, error_buffer,
                                     sizeof(error_buffer)))
        {
            if (profile_copied)
            {
                app_remove_copied_asset(profile_buffer);
            }
            for (size_t cleanup = 0U; cleanup < data.certificate_count; ++cleanup)
            {
                if (certificate_copied[cleanup])
                {
                    app_remove_copied_asset(certificate_buffers[cleanup]);
                }
            }

            char message[512];
            if (error_buffer[0] != '\0')
            {
                (void)snprintf(message, sizeof(message), "Unable to create person: %s",
                               error_buffer);
            }
            else
            {
                (void)snprintf(message, sizeof(message),
                               "Unable to create person due to an unknown error.");
            }
            app_report_error(ui, logger, message);
            continue;
        }

        Person *created_person = family_tree_find_person(*tree, new_person_id);
        if (interaction_state)
        {
            interaction_select_person(interaction_state, created_person);
        }

        bool has_position       = false;
        float focus_position[3] = {0.0f, 0.0f, 0.0f};
        if (layout && layout->nodes)
        {
            for (size_t index = 0U; index < layout->count; ++index)
            {
                if (layout->nodes[index].person == created_person)
                {
                    focus_position[0] = layout->nodes[index].position[0];
                    focus_position[1] = layout->nodes[index].position[1];
                    focus_position[2] = layout->nodes[index].position[2];
                    has_position      = true;
                    break;
                }
            }
        }
        if (camera && has_position)
        {
            float radius = camera->config.default_radius;
            if (!(radius > 0.0f))
            {
                radius = 14.0f;
            }
            camera_controller_focus(camera, focus_position, radius);
        }

        if (auto_save)
        {
            persistence_auto_save_mark_dirty(auto_save);
        }

        char name_buffer[128];
        if (!created_person ||
            !person_format_display_name(created_person, name_buffer, sizeof(name_buffer)))
        {
            (void)snprintf(name_buffer, sizeof(name_buffer), "Person %u", new_person_id);
        }
        char status_message[192];
        (void)snprintf(status_message, sizeof(status_message),
                       "Created holographic profile for %s (ID %u).", name_buffer, new_person_id);
        app_report_status(ui, logger, status_message);
        if (logger)
        {
            AT_LOG(logger, AT_LOG_INFO, "Created person %u via Add Person panel", new_person_id);
        }
    }
}

static void app_process_edit_person_requests(UIContext *ui, AppState *app_state, FamilyTree **tree,
                                             LayoutResult *layout,
                                             InteractionState *interaction_state,
                                             CameraController *camera,
                                             PersistenceAutoSave *auto_save, AtLogger *logger)
{
    if (!ui)
    {
        return;
    }

    UIEditPersonRequest request;
    while (ui_consume_edit_person_request(ui, &request))
    {
        if (!app_state || !tree || !layout || !interaction_state || !*tree)
        {
            app_report_error(ui, logger, "Cannot edit person: application state unavailable.");
            continue;
        }
        if (request.person_id == 0U)
        {
            app_report_error(ui, logger, "Invalid edit request (missing person identifier).");
            continue;
        }

        Person *person = family_tree_find_person(*tree, request.person_id);
        if (!person)
        {
            char message[256];
            (void)snprintf(message, sizeof(message), "Person %u not found in current tree.",
                           request.person_id);
            app_report_error(ui, logger, message);
            continue;
        }

        AppPersonEditData edit_data;
        memset(&edit_data, 0, sizeof(edit_data));
        edit_data.first      = request.first;
        edit_data.middle     = (request.middle[0] != '\0') ? request.middle : NULL;
        edit_data.last       = request.last;
        edit_data.birth_date = request.birth_date;
        edit_data.birth_location =
            (request.birth_location[0] != '\0') ? request.birth_location : NULL;
        if (request.has_death)
        {
            edit_data.clear_death = false;
            edit_data.death_date  = request.death_date;
            edit_data.death_location =
                (request.death_location[0] != '\0') ? request.death_location : NULL;
        }
        else
        {
            edit_data.clear_death    = true;
            edit_data.death_date     = NULL;
            edit_data.death_location = NULL;
        }

        edit_data.relationships.apply_father  = request.update_father;
        edit_data.relationships.father_id     = request.father_id;
        edit_data.relationships.apply_mother  = request.update_mother;
        edit_data.relationships.mother_id     = request.mother_id;
        edit_data.relationships.apply_spouses = request.update_spouses;
        size_t spouse_count                   = request.spouse_count;
        if (spouse_count > APP_PERSON_EDIT_MAX_SPOUSES)
        {
            spouse_count = APP_PERSON_EDIT_MAX_SPOUSES;
        }
        edit_data.relationships.spouse_count = spouse_count;
        for (size_t index = 0U; index < spouse_count; ++index)
        {
            edit_data.relationships.spouse_ids[index] = request.spouse_ids[index];
        }
        for (size_t index = spouse_count; index < APP_PERSON_EDIT_MAX_SPOUSES; ++index)
        {
            edit_data.relationships.spouse_ids[index] = 0U;
        }

        AppCommand *command = app_command_create_edit_person(request.person_id, &edit_data);
        if (!command)
        {
            app_report_error(ui, logger, "Failed to build edit command.");
            continue;
        }

        char error_buffer[256];
        error_buffer[0] = '\0';
        if (!app_state_push_command(app_state, command, error_buffer, sizeof(error_buffer)))
        {
            char message[256];
            if (error_buffer[0] != '\0')
            {
                (void)snprintf(message, sizeof(message), "Unable to edit person: %s", error_buffer);
            }
            else
            {
                (void)snprintf(message, sizeof(message),
                               "Unable to edit person due to an unknown error.");
            }
            app_report_error(ui, logger, message);
            continue;
        }

        Person *updated = family_tree_find_person(*tree, request.person_id);
        if (interaction_state)
        {
            interaction_select_person(interaction_state, updated ? updated : person);
        }

        if (camera && layout && layout->nodes && updated)
        {
            for (size_t index = 0U; index < layout->count; ++index)
            {
                if (layout->nodes[index].person == updated)
                {
                    float radius = camera->config.default_radius;
                    if (!(radius > 0.0f))
                    {
                        radius = 14.0f;
                    }
                    camera_controller_focus(camera, layout->nodes[index].position, radius);
                    break;
                }
            }
        }

        if (auto_save)
        {
            persistence_auto_save_mark_dirty(auto_save);
        }

        char name_buffer[128];
        Person *label_person = updated ? updated : person;
        if (!label_person ||
            !person_format_display_name(label_person, name_buffer, sizeof(name_buffer)))
        {
            (void)snprintf(name_buffer, sizeof(name_buffer), "Person %u", request.person_id);
        }
        char status_message[192];
        (void)snprintf(status_message, sizeof(status_message), "Updated profile for %s.",
                       name_buffer);
        app_report_status(ui, logger, status_message);
        if (logger)
        {
            AT_LOG(logger, AT_LOG_INFO, "Edited person %u via Edit Person panel",
                   request.person_id);
        }
    }
}

static void app_handle_shortcut_input(UIContext *ui, AppFileState *file_state, FamilyTree **tree,
                                      LayoutResult *layout, InteractionState *interaction_state,
                                      RenderState *render_state, CameraController *camera,
                                      AtLogger *logger, Settings *settings,
                                      Settings *persisted_settings, const char *settings_path,
                                      PersistenceAutoSave *auto_save,
                                      unsigned int *settings_applied_revision, AppState *app_state,
                                      ExpansionState *expansion, DetailViewSystem *detail_view);

typedef struct EventShortcutPayload
{
    UIContext *ui;
    AppFileState *file_state;
    FamilyTree **tree;
    LayoutResult *layout;
    InteractionState *interaction_state;
    RenderState *render_state;
    CameraController *camera;
    AtLogger *logger;
    Settings *settings;
    Settings *persisted_settings;
    const char *settings_path;
    PersistenceAutoSave *auto_save;
    unsigned int *settings_revision;
    AppState *app_state;
    ExpansionState *expansion;
    DetailViewSystem *detail_view;
} EventShortcutPayload;

typedef struct EventQueuePayload
{
    UIContext *ui;
    AppFileState *file_state;
    FamilyTree **tree;
    LayoutResult *layout;
    InteractionState *interaction_state;
    RenderState *render_state;
    CameraController *camera;
    AtLogger *logger;
    Settings *settings;
    Settings *persisted_settings;
    const char *settings_path;
    PersistenceAutoSave *auto_save;
    unsigned int *settings_revision;
    AppState *app_state;
} EventQueuePayload;

static void event_shortcut_handler(void *user_data, float delta_seconds)
{
    (void)delta_seconds;
    EventShortcutPayload *payload = (EventShortcutPayload *)user_data;
    if (!payload)
    {
        return;
    }
    app_handle_shortcut_input(
        payload->ui, payload->file_state, payload->tree, payload->layout,
        payload->interaction_state, payload->render_state, payload->camera, payload->logger,
        payload->settings, payload->persisted_settings, payload->settings_path, payload->auto_save,
        payload->settings_revision, payload->app_state, payload->expansion, payload->detail_view);
}

static void event_queue_handler(void *user_data, float delta_seconds)
{
    (void)delta_seconds;
    EventQueuePayload *payload = (EventQueuePayload *)user_data;
    if (!payload)
    {
        return;
    }
    app_handle_pending_ui_events(payload->ui, payload->file_state, payload->tree, payload->layout,
                                 payload->interaction_state, payload->render_state, payload->camera,
                                 payload->logger, payload->settings, payload->persisted_settings,
                                 payload->settings_path, payload->auto_save,
                                 payload->settings_revision, payload->app_state);
    app_process_add_person_requests(payload->ui, payload->app_state, payload->tree, payload->layout,
                                    payload->interaction_state, payload->camera, payload->auto_save,
                                    payload->logger);
    app_process_edit_person_requests(payload->ui, payload->app_state, payload->tree,
                                     payload->layout, payload->interaction_state, payload->camera,
                                     payload->auto_save, payload->logger);
}

static bool app_start_detail_view_for_selection(const Person *selected, LayoutResult *layout,
                                                ExpansionState *expansion,
                                                DetailViewSystem *detail_view,
                                                CameraController *camera, AppState *app_state,
                                                AtLogger *logger)
{
    if (!selected || !layout || layout->count == 0U || !expansion || !detail_view)
    {
        return false;
    }

    bool expansion_active      = expansion_is_active(expansion);
    bool expansion_detail_mode = expansion_active && expansion_is_in_detail_mode(expansion);

    bool retargeting = expansion_detail_mode;

    DetailViewContent content;
    if (!detail_view_content_build(selected, &content))
    {
        if (logger)
        {
            AT_LOG(logger, AT_LOG_WARN, "Failed to build detail view content for selection %u.",
                   selected->id);
        }
        return false;
    }
    if (!detail_view_set_content(detail_view, &content))
    {
        if (logger)
        {
            AT_LOG(logger, AT_LOG_WARN, "Detail view content rejected for selection %u.",
                   selected->id);
        }
        return false;
    }
    bool expansion_success = false;
    if (retargeting)
    {
        expansion_success = expansion_retarget(expansion, layout, selected, camera);
        if (!expansion_success && !expansion_is_active(expansion))
        {
            expansion_success = expansion_start(expansion, layout, selected, camera);
            retargeting       = false;
        }
    }
    else
    {
        expansion_success = expansion_start(expansion, layout, selected, camera);
    }
    if (!expansion_success)
    {
        if (logger)
        {
            if (retargeting)
            {
                AT_LOG(logger, AT_LOG_WARN, "Expansion retarget failed for selection %u.",
                       selected->id);
            }
            else
            {
                AT_LOG(logger, AT_LOG_WARN, "Expansion launch failed for selection %u.",
                       selected->id);
            }
        }
        return false;
    }

    if (app_state)
    {
        app_state->interaction_mode = APP_INTERACTION_MODE_DETAIL_VIEW;
        app_state->selected_person  = (Person *)selected;
    }
    if (logger)
    {
        char name_buffer[128];
        if (!person_format_display_name(selected, name_buffer, sizeof(name_buffer)))
        {
            (void)snprintf(name_buffer, sizeof(name_buffer), "Person %u", selected->id);
        }
        const char *action = retargeting ? "Switching detail view to" : "Entering detail view for";
        AT_LOG(logger, AT_LOG_INFO, "%s %s.", action, name_buffer);
    }
    return true;
}

static void app_handle_shortcut_input(UIContext *ui, AppFileState *file_state, FamilyTree **tree,
                                      LayoutResult *layout, InteractionState *interaction_state,
                                      RenderState *render_state, CameraController *camera,
                                      AtLogger *logger, Settings *settings,
                                      Settings *persisted_settings, const char *settings_path,
                                      PersistenceAutoSave *auto_save,
                                      unsigned int *settings_applied_revision, AppState *app_state,
                                      ExpansionState *expansion, DetailViewSystem *detail_view)
{
    if (!ui)
    {
        return;
    }
#if defined(ANCESTRYTREE_HAVE_RAYLIB)
    ShortcutState state;
    state.ctrl_down            = IsKeyDown(KEY_LEFT_CONTROL) || IsKeyDown(KEY_RIGHT_CONTROL);
    state.shift_down           = IsKeyDown(KEY_LEFT_SHIFT) || IsKeyDown(KEY_RIGHT_SHIFT);
    state.key_new_pressed      = IsKeyPressed(KEY_N);
    state.key_open_pressed     = IsKeyPressed(KEY_O);
    state.key_save_pressed     = IsKeyPressed(KEY_S);
    state.key_undo_pressed     = IsKeyPressed(KEY_Z);
    state.key_redo_pressed     = IsKeyPressed(KEY_Y);
    state.key_space_pressed    = IsKeyPressed(KEY_SPACE);
    state.key_escape_pressed   = IsKeyPressed(KEY_ESCAPE);
    bool key_enter_pressed     = IsKeyPressed(KEY_ENTER);
    bool key_c_pressed         = IsKeyPressed(KEY_C);
    bool key_x_pressed         = IsKeyPressed(KEY_X);
    bool key_backspace_pressed = IsKeyPressed(KEY_BACKSPACE);

    ShortcutResult result;
    shortcuts_evaluate(&state, &result);
    if (result.event != UI_EVENT_NONE)
    {
        if (!ui_event_enqueue(ui, result.event))
        {
            UIEvent fallback_event;
            fallback_event.type      = result.event;
            fallback_event.param_u32 = 0U;
            app_process_ui_event(&fallback_event, ui, file_state, tree, layout, interaction_state,
                                 render_state, camera, logger, settings, persisted_settings,
                                 settings_path, auto_save, settings_applied_revision, app_state);
        }
    }

    bool in_detail_mode =
        app_state && app_state->interaction_mode == APP_INTERACTION_MODE_DETAIL_VIEW;
    const Person *selected = interaction_state ? interaction_get_selected(interaction_state) : NULL;
    const Person *previous_selected = app_state ? app_state->selected_person : NULL;
    bool triggered_detail_view      = false;

    if (key_enter_pressed && selected && expansion && detail_view && layout && layout->count > 0U)
    {
        triggered_detail_view = app_start_detail_view_for_selection(
            selected, layout, expansion, detail_view, camera, app_state, logger);
    }

    if (!triggered_detail_view && app_state && selected && selected != previous_selected &&
        expansion && detail_view && layout && layout->count > 0U)
    {
        triggered_detail_view = app_start_detail_view_for_selection(
            selected, layout, expansion, detail_view, camera, app_state, logger);
    }

    if (app_state)
    {
        if (selected && app_state->selected_person != (Person *)selected)
        {
            app_state->selected_person = (Person *)selected;
        }
        else if (!selected && !in_detail_mode && app_state->selected_person)
        {
            app_state->selected_person = NULL;
        }
    }

    if (in_detail_mode && key_c_pressed && detail_view)
    {
        detail_view_focus_next_certificate(detail_view);
    }
    else if (in_detail_mode && key_x_pressed && detail_view)
    {
        detail_view_clear_certificate_zoom(detail_view);
    }
    else if (key_backspace_pressed && expansion && expansion_is_active(expansion))
    {
        if (!expansion_is_reversing(expansion))
        {
            expansion_reverse(expansion, camera);
            if (logger)
            {
                AT_LOG(logger, AT_LOG_INFO, "Exiting detail view.");
            }
        }
    }
    /* Manual validation checklist:
     * 1. Press Ctrl+N/O/S/Shift+S to verify the status banner reports the corresponding action.
     * 2. Press Space to confirm the camera immediately snaps back to its default orbit.
     * 3. Press Escape while dialogs are open to ensure they close and the selection clears.
     */
#else
    (void)file_state;
    (void)tree;
    (void)layout;
    (void)interaction_state;
    (void)render_state;
    (void)camera;
    (void)logger;
    (void)settings;
    (void)persisted_settings;
    (void)settings_path;
    (void)auto_save;
    (void)settings_applied_revision;
    (void)app_state;
    (void)expansion;
    (void)detail_view;
#endif
}

static FamilyTree *app_create_placeholder_tree(void)
{
    FamilyTree *tree = family_tree_create("Placeholder Tree");
    if (!tree)
    {
        return NULL;
    }
    Person *person = person_create(1U);
    if (!person)
    {
        family_tree_destroy(tree);
        return NULL;
    }
    person_set_name(person, "Avery", "", "Prototype");
    person_set_birth(person, "1990-01-01", "Unknown");
    person->is_alive = true;
    if (!family_tree_add_person(tree, person))
    {
        person_destroy(person);
        family_tree_destroy(tree);
        return NULL;
    }
    return tree;
}

static FamilyTree *app_auto_save_tree_supplier(void *user_data)
{
    FamilyTree **tree_ref = (FamilyTree **)user_data;
    if (!tree_ref)
    {
        return NULL;
    }
    return *tree_ref;
}

static void app_apply_settings(const Settings *settings, RenderState *render_state,
                               CameraController *camera, PersistenceAutoSave *auto_save)
{
    if (camera)
    {
        settings_runtime_apply_camera(settings, camera);
    }
    if (render_state)
    {
        settings_runtime_apply_render(settings, &render_state->config);
    }
    if (auto_save && settings)
    {
        persistence_auto_save_set_enabled(auto_save, settings->auto_save_enabled);
        persistence_auto_save_set_interval(auto_save, settings->auto_save_interval_seconds);
    }
}

static void app_collect_camera_input(CameraControllerInput *input, bool auto_orbit_enabled,
                                     const Settings *settings, float wheel_delta)
{
    if (!input)
    {
        return;
    }

    float orbit_sensitivity        = 0.15f;
    float pan_sensitivity          = 0.5f;
    float pan_keyboard_sensitivity = 1.0f;
    float zoom_sensitivity         = 1.0f;
    settings_runtime_compute_input_sensitivity(settings, &orbit_sensitivity, &pan_sensitivity,
                                               &pan_keyboard_sensitivity, &zoom_sensitivity);
    static bool cursor_locked = false;

    camera_controller_input_clear(input);

    Vector2 mouse_delta = GetMouseDelta();
    bool orbiting       = IsMouseButtonDown(MOUSE_BUTTON_RIGHT);
    if (orbiting && !cursor_locked)
    {
        DisableCursor();
        cursor_locked = true;
    }
    else if (!orbiting && cursor_locked)
    {
        EnableCursor();
        cursor_locked = false;
    }

    if (orbiting)
    {
        input->yaw_delta -= mouse_delta.x * orbit_sensitivity;
        input->pitch_delta -= mouse_delta.y * orbit_sensitivity;
    }

    if (IsMouseButtonDown(MOUSE_BUTTON_MIDDLE))
    {
        input->pan_right -= mouse_delta.x * pan_sensitivity;
        input->pan_up += mouse_delta.y * pan_sensitivity;
    }
    else
    {
        if (IsKeyDown(KEY_A) || IsKeyDown(KEY_LEFT))
        {
            input->pan_right -= pan_keyboard_sensitivity;
        }
        if (IsKeyDown(KEY_D) || IsKeyDown(KEY_RIGHT))
        {
            input->pan_right += pan_keyboard_sensitivity;
        }
        if (IsKeyDown(KEY_W) || IsKeyDown(KEY_UP))
        {
            input->pan_up += pan_keyboard_sensitivity;
        }
        if (IsKeyDown(KEY_S) || IsKeyDown(KEY_DOWN))
        {
            input->pan_up -= pan_keyboard_sensitivity;
        }
    }

    input->zoom_delta = wheel_delta * zoom_sensitivity;

    const float threshold = 0.001f;
    bool has_manual_orbit =
        (fabsf(input->yaw_delta) > threshold) || (fabsf(input->pitch_delta) > threshold);
    bool has_manual_pan =
        (fabsf(input->pan_right) > threshold) || (fabsf(input->pan_up) > threshold);
    bool has_manual_zoom = fabsf(input->zoom_delta) > threshold;

    if (auto_orbit_enabled && !has_manual_orbit && !has_manual_pan && !has_manual_zoom)
    {
        float orbit_scale = orbit_sensitivity / 0.15f;
        if (!(orbit_scale > 0.0f))
        {
            orbit_scale = 1.0f;
        }
        input->yaw_delta -= 0.25f * orbit_scale;
    }
}

static void app_render_scene_basic(const LayoutResult *layout, const CameraController *camera,
                                   const Person *selected_person, const Person *hovered_person,
                                   const RenderConfig *config)
{
    const Camera3D *raylib_camera = camera_controller_get_camera(camera);
    if (!raylib_camera)
    {
        return;
    }

    BeginMode3D(*raylib_camera);
    if (!config || config->show_grid)
    {
        DrawGrid(24, 1.0f);
    }

    if (layout)
    {
        for (size_t index = 0U; index < layout->count; ++index)
        {
            const LayoutNode *node = &layout->nodes[index];
            const Person *person   = node->person;
            Color color            = person && person->is_alive ? (Color){0, 195, 255, 255}
                                                                : (Color){200, 120, 240, 255};
            float radius           = 0.6f;
            if (person == hovered_person && person != selected_person)
            {
                radius *= 1.08f;
            }
            if (person == selected_person)
            {
                radius *= 1.2f;
            }
            Vector3 position = {node->position[0], node->position[1], node->position[2]};
            DrawSphere(position, radius, color);
            if (person == selected_person)
            {
                DrawSphereWires(position, radius * 1.05f, 16, 16, RAYWHITE);
            }
        }
    }

    EndMode3D();
}

static int app_run(AtLogger *logger, const AppLaunchOptions *options)
{
    if (!graphics_has_raylib_support())
    {
        AT_LOG(logger, AT_LOG_ERROR, "This build lacks raylib support; cannot start renderer.");
        return 1;
    }

    GraphicsState graphics_state;
    graphics_state_init(&graphics_state);

    GraphicsConfig config = graphics_config_default();
    config.title          = "AncestryTree";
    char error_buffer[256];
    if (!graphics_window_init(&graphics_state, &config, error_buffer, sizeof(error_buffer)))
    {
        AT_LOG(logger, AT_LOG_ERROR, "Failed to initialize window: %s", error_buffer);
        return 1;
    }

    CameraControllerConfig camera_config;
    camera_controller_config_default(&camera_config);
    CameraController camera_controller;
    camera_controller_init(&camera_controller, &camera_config);

    Settings settings;
    settings_init_defaults(&settings);
    Settings persisted_settings = settings;
    const char *settings_path   = APP_SETTINGS_PATH;
    char settings_error[256];
    settings_error[0]   = '\0';
    bool settings_dirty = false;
    if (settings_try_load(&settings, settings_path, settings_error, sizeof(settings_error)))
    {
        persisted_settings = settings;
        AT_LOG(logger, AT_LOG_INFO, "Loaded settings from %s", settings_path);
    }
    else
    {
        if (settings_error[0] != '\0')
        {
            AT_LOG(logger, AT_LOG_WARN, "Settings load failed (%s); defaults in use.",
                   settings_error);
        }
        else
        {
            AT_LOG(logger, AT_LOG_WARN, "Settings file not found; defaults in use.");
        }
    }
    FamilyTree *tree = NULL;
    AppFileState file_state;
    app_file_state_clear(&file_state);
    bool tree_loaded_from_cli   = false;
    bool tree_loaded_from_asset = false;
    bool placeholder_used       = false;

    char initial_status[256];
    initial_status[0] = '\0';
    char initial_warning[256];
    initial_warning[0]   = '\0';
    bool warning_pending = false;

    char sample_tree_path[512];
    sample_tree_path[0] = '\0';
    bool sample_tree_available =
        app_try_find_asset("assets/example_tree.json", sample_tree_path, sizeof(sample_tree_path));

    AppLaunchOptions default_options;
    memset(&default_options, 0, sizeof(default_options));
    AppLaunchOptions const *effective_options = options ? options : &default_options;
    bool allow_sample_start                   = !settings.has_loaded_sample_tree;

    AppStartupDecision startup_decision;
    char startup_message[256];
    startup_message[0] = '\0';
    if (!app_bootstrap_decide_tree_source(
            effective_options, sample_tree_available ? sample_tree_path : NULL, allow_sample_start,
            &startup_decision, startup_message, sizeof(startup_message)))
    {
        AT_LOG(logger, AT_LOG_ERROR, "%s",
               (startup_message[0] != '\0') ? startup_message
                                            : "Unable to determine startup plan.");
        graphics_window_shutdown(&graphics_state);
        return 1;
    }
    if (startup_message[0] != '\0')
    {
        AT_LOG(logger, AT_LOG_INFO, "%s", startup_message);
    }

    switch (startup_decision.source)
    {
    case APP_STARTUP_SOURCE_CLI_PATH:
        AT_LOG(logger, AT_LOG_INFO, "Loading tree from %s", startup_decision.resolved_path);
        tree = persistence_tree_load(startup_decision.resolved_path, error_buffer,
                                     sizeof(error_buffer));
        if (!tree)
        {
            const char *detail = (error_buffer[0] != '\0') ? error_buffer : NULL;
            char load_hint[256];
            if (!status_message_format_load_error(startup_decision.resolved_path, detail, load_hint,
                                                  sizeof(load_hint)))
            {
                (void)snprintf(load_hint, sizeof(load_hint),
                               "Unable to load the family tree from '%s'. %s.",
                               startup_decision.resolved_path, detail ? detail : "Unknown error");
            }
            AT_LOG(logger, AT_LOG_ERROR, "%s Placeholder data will be used.", load_hint);
            if (!warning_pending)
            {
                initial_warning[0] = '\0';
            }
            app_append_message(initial_warning, sizeof(initial_warning), load_hint);
            app_append_message(initial_warning, sizeof(initial_warning),
                               "Placeholder data will be used.");
            warning_pending = true;
        }
        else
        {
            tree_loaded_from_cli = true;
            app_file_state_set(&file_state, startup_decision.resolved_path);
            (void)snprintf(initial_status, sizeof(initial_status), "Loaded tree from %s.",
                           startup_decision.resolved_path);
        }
        break;
    case APP_STARTUP_SOURCE_SAMPLE_ASSET:
        AT_LOG(logger, AT_LOG_INFO, "Loading sample tree from %s", startup_decision.resolved_path);
        tree = persistence_tree_load(startup_decision.resolved_path, error_buffer,
                                     sizeof(error_buffer));
        if (!tree)
        {
            const char *detail = (error_buffer[0] != '\0') ? error_buffer : NULL;
            char sample_hint[256];
            if (!status_message_format_load_error(startup_decision.resolved_path, detail,
                                                  sample_hint, sizeof(sample_hint)))
            {
                (void)snprintf(sample_hint, sizeof(sample_hint),
                               "Unable to load the family tree from '%s'. %s.",
                               startup_decision.resolved_path, detail ? detail : "Unknown error");
            }
            AT_LOG(logger, AT_LOG_WARN, "%s Placeholder data will be used.", sample_hint);
            if (!warning_pending)
            {
                initial_warning[0] = '\0';
            }
            app_append_message(initial_warning, sizeof(initial_warning), sample_hint);
            app_append_message(initial_warning, sizeof(initial_warning),
                               "Re-run scripts/setup_dependencies to refresh the bundled example "
                               "or open a different tree with Ctrl+O.");
            app_append_message(initial_warning, sizeof(initial_warning),
                               "Placeholder data will be used.");
            warning_pending = true;
        }
        else
        {
            tree_loaded_from_asset = true;
            if (!settings.has_loaded_sample_tree)
            {
                settings.has_loaded_sample_tree = true;
                settings_mark_dirty(&settings);
                settings_dirty = true;
            }
            app_file_state_set(&file_state, startup_decision.resolved_path);
            if (initial_status[0] == '\0')
            {
                (void)snprintf(initial_status, sizeof(initial_status), "Sample tree loaded.");
            }
        }
        break;
    case APP_STARTUP_SOURCE_PLACEHOLDER:
        break;
    default:
        AT_LOG(logger, AT_LOG_WARN,
               "Unrecognised startup source; falling back to placeholder hologram.");
        break;
    }

    /* Manual QA: exercise `--load`, `--no-sample`, and default launches to ensure the startup
     * planner selects the expected tree source and reports placeholder fallbacks via the status
     * banner. */

    if (settings_dirty)
    {
        char save_error[256];
        save_error[0] = '\0';
        if (settings_save(&settings, settings_path, save_error, sizeof(save_error)))
        {
            persisted_settings = settings;
            AT_LOG(logger, AT_LOG_INFO, "Settings saved to %s.", settings_path);
        }
        else
        {
            const char *detail = (save_error[0] != '\0') ? save_error : NULL;
            char save_hint[256];
            if (!status_message_format_save_error(settings_path, detail, save_hint,
                                                  sizeof(save_hint)))
            {
                (void)snprintf(save_hint, sizeof(save_hint), "Could not save settings to '%s'. %s.",
                               settings_path, detail ? detail : "Unknown error");
            }
            AT_LOG(logger, AT_LOG_WARN, "%s Settings changes will persist only for this session.",
                   save_hint);
            if (!warning_pending)
            {
                initial_warning[0] = '\0';
            }
            app_append_message(initial_warning, sizeof(initial_warning), save_hint);
            app_append_message(initial_warning, sizeof(initial_warning),
                               "Settings changes will persist only for this session.");
            warning_pending = true;
        }
    }

    unsigned int settings_applied_revision = settings_get_revision(&settings);

    if (!tree)
    {
        tree = app_create_placeholder_tree();
        if (!tree)
        {
            AT_LOG(logger, AT_LOG_ERROR, "Unable to create fallback tree.");
            graphics_window_shutdown(&graphics_state);
            return 1;
        }
        placeholder_used = true;
        app_file_state_clear(&file_state);
        if (initial_status[0] == '\0')
        {
            (void)snprintf(initial_status, sizeof(initial_status), "Placeholder tree initialised.");
        }
    }

    LayoutAlgorithm initial_algorithm = app_select_layout_algorithm(NULL, &settings);
    LayoutResult layout               = layout_calculate_with_algorithm(tree, initial_algorithm);
    app_focus_camera_on_layout(&camera_controller, &layout);

    RenderState render_state;
    render_state_init(&render_state);
    char render_error[256];
    bool render_ready = render_init(&render_state, NULL, render_error, sizeof(render_error));
    AT_LOG_WARN_IF(logger, !render_ready, "Render pipeline fallback: %s", render_error);
    bool render_target_warned = false;
    if (render_ready)
    {
        if (!render_resize(&render_state, graphics_state.width, graphics_state.height, render_error,
                           sizeof(render_error)))
        {
            AT_LOG(logger, AT_LOG_WARN, "Render target unavailable: %s", render_error);
            render_target_warned = true;
        }
    }

    InteractionState interaction_state;
    interaction_state_init(&interaction_state);
    interaction_state_set_pick_radius(&interaction_state, render_state.config.sphere_radius);

    AppState app_state;
    app_state_init(&app_state);
    if (!app_state_configure(&app_state, &tree, &layout, &interaction_state, &camera_controller,
                             &settings, &persisted_settings))
    {
        AT_LOG(logger, AT_LOG_ERROR, "Unable to configure application state manager.");
        layout_result_destroy(&layout);
        family_tree_destroy(tree);
        if (render_ready)
        {
            render_cleanup(&render_state);
        }
        graphics_window_shutdown(&graphics_state);
        return 1;
    }

    ExpansionState expansion_state;
    expansion_state_reset(&expansion_state);

    DetailViewSystem *detail_view = detail_view_create();
    bool detail_view_ready        = detail_view != NULL;
    if (!detail_view_ready && logger)
    {
        AT_LOG(logger, AT_LOG_WARN, "Detail view system unavailable; immersive mode disabled.");
    }

    PersistenceAutoSave auto_save;
    memset(&auto_save, 0, sizeof(auto_save));
    bool auto_save_ready = false;
    char auto_save_error[256];
    auto_save_error[0] = '\0';
    PersistenceAutoSaveConfig auto_save_config;
    auto_save_config.tree_supplier    = app_auto_save_tree_supplier;
    auto_save_config.user_data        = &tree;
    auto_save_config.path             = APP_AUTO_SAVE_PATH;
    auto_save_config.interval_seconds = settings.auto_save_interval_seconds;
    if (persistence_auto_save_init(&auto_save, &auto_save_config, auto_save_error,
                                   sizeof(auto_save_error)))
    {
        auto_save_ready = true;
        persistence_auto_save_set_enabled(&auto_save, settings.auto_save_enabled);
        persistence_auto_save_mark_dirty(&auto_save);
    }
    else
    {
        AT_LOG(logger, AT_LOG_WARN, "Auto-save unavailable (%s).", auto_save_error);
    }

    app_apply_settings(&settings, &render_state, &camera_controller,
                       auto_save_ready ? &auto_save : NULL);
    settings_applied_revision = settings_get_revision(&settings);

    UIContext ui;
    bool ui_ready = ui_init(&ui, graphics_state.width, graphics_state.height);
    AT_LOG_WARN_IF(logger, !ui_ready,
                   "UI overlay unavailable; Nuklear or raylib might be missing.");

    if (tree_loaded_from_asset || tree_loaded_from_cli)
    {
        app_state_clear_tree_dirty(&app_state);
    }
    else if (placeholder_used)
    {
        app_state_mark_tree_dirty(&app_state);
    }

    if (ui_ready)
    {
        if (initial_status[0] != '\0')
        {
            (void)ui_notify_status(&ui, initial_status);
        }
        if (warning_pending && initial_warning[0] != '\0')
        {
            (void)ui_show_error_dialog(&ui, "Startup Warning", initial_warning);
        }
        bool onboarding_pending = !settings.onboarding_completed;
        ui_onboarding_configure(&ui, onboarding_pending, onboarding_pending);
    }

    SetTargetFPS((int)config.target_fps);

    EventShortcutPayload shortcut_payload;
    shortcut_payload.ui                 = ui_ready ? &ui : NULL;
    shortcut_payload.file_state         = &file_state;
    shortcut_payload.tree               = &tree;
    shortcut_payload.layout             = &layout;
    shortcut_payload.interaction_state  = &interaction_state;
    shortcut_payload.render_state       = &render_state;
    shortcut_payload.camera             = &camera_controller;
    shortcut_payload.logger             = logger;
    shortcut_payload.settings           = &settings;
    shortcut_payload.persisted_settings = &persisted_settings;
    shortcut_payload.settings_path      = settings_path;
    shortcut_payload.auto_save          = auto_save_ready ? &auto_save : NULL;
    shortcut_payload.settings_revision  = &settings_applied_revision;
    shortcut_payload.app_state          = &app_state;
    shortcut_payload.expansion          = &expansion_state;
    shortcut_payload.detail_view        = detail_view_ready ? detail_view : NULL;

    EventQueuePayload queue_payload;
    queue_payload.ui                 = ui_ready ? &ui : NULL;
    queue_payload.file_state         = &file_state;
    queue_payload.tree               = &tree;
    queue_payload.layout             = &layout;
    queue_payload.interaction_state  = &interaction_state;
    queue_payload.render_state       = &render_state;
    queue_payload.camera             = &camera_controller;
    queue_payload.logger             = logger;
    queue_payload.settings           = &settings;
    queue_payload.persisted_settings = &persisted_settings;
    queue_payload.settings_path      = settings_path;
    queue_payload.auto_save          = auto_save_ready ? &auto_save : NULL;
    queue_payload.settings_revision  = &settings_applied_revision;
    queue_payload.app_state          = &app_state;

    EventProcessContext event_context;
    event_context.graphics_state        = &graphics_state;
    event_context.ui                    = ui_ready ? &ui : NULL;
    event_context.interaction_state     = &interaction_state;
    event_context.layout                = &layout;
    event_context.camera                = &camera_controller;
    event_context.render_state          = &render_state;
    event_context.render_ready          = render_ready;
    event_context.render_error_buffer   = render_error;
    event_context.render_error_capacity = sizeof(render_error);
    event_context.render_target_warned  = &render_target_warned;
    event_context.logger                = logger;
    event_context.shortcut_handler      = event_shortcut_handler;
    event_context.shortcut_user_data    = &shortcut_payload;
    event_context.queue_handler         = event_queue_handler;
    event_context.queue_user_data       = &queue_payload;

    bool expansion_was_active = false;

    while (!WindowShouldClose())
    {
        float delta_seconds    = GetFrameTime();
        float wheel_delta      = GetMouseWheelMove();
        bool shift_down_global = IsKeyDown(KEY_LEFT_SHIFT) || IsKeyDown(KEY_RIGHT_SHIFT);

        bool detail_view_consumes_wheel = false;
        if (detail_view_ready && detail_view && detail_view_content_ready(detail_view))
        {
            float prior_detail_phase = detail_view_get_detail_phase(detail_view);
            if (prior_detail_phase > 0.01f && detail_view_timeline_requires_scroll(detail_view) &&
                detail_view_timeline_hovered(detail_view) && shift_down_global)
            {
                detail_view_consumes_wheel = true;
            }
        }

        float camera_wheel_delta = detail_view_consumes_wheel ? 0.0f : wheel_delta;
        float ui_wheel_delta     = detail_view_consumes_wheel ? 0.0f : wheel_delta;

        event_context.render_ready = render_ready;
        event_context.ui           = ui_ready ? &ui : NULL;
        shortcut_payload.ui        = event_context.ui;
        queue_payload.ui           = event_context.ui;
        shortcut_payload.auto_save = auto_save_ready ? &auto_save : NULL;
        queue_payload.auto_save    = auto_save_ready ? &auto_save : NULL;

        event_process(&event_context, EVENT_PROCESS_PHASE_PRE_FRAME, delta_seconds);

        CameraControllerInput controller_input;
        app_collect_camera_input(&controller_input, ui_auto_orbit_enabled(&ui), &settings,
                                 camera_wheel_delta);
        camera_controller_update(&camera_controller, &controller_input, delta_seconds);
        app_state_tick(&app_state, delta_seconds);

        const ExpansionState *detail_expansion_ptr = NULL;
        float detail_view_phase                    = 0.0f;
        bool detail_view_should_render             = false;

        bool expansion_active_before = expansion_is_active(&expansion_state);
        bool expansion_reversing     = expansion_is_reversing(&expansion_state);
        if (expansion_active_before)
        {
            (void)expansion_update(&expansion_state, delta_seconds, &camera_controller);
        }
        bool expansion_active_now = expansion_is_active(&expansion_state);
        if (!expansion_active_now && expansion_was_active)
        {
            app_state.interaction_mode = APP_INTERACTION_MODE_TREE_VIEW;
        }
        expansion_was_active = expansion_active_now;

        if (expansion_active_now)
        {
            detail_expansion_ptr = &expansion_state;
        }

        if (detail_view_ready)
        {
            bool expansion_forward = expansion_active_now && !expansion_reversing;
            float target_phase     = expansion_forward ? 1.0f : 0.0f;
            detail_view_update(detail_view, delta_seconds, detail_expansion_ptr, target_phase,
                               target_phase, wheel_delta);
            detail_view_phase         = detail_view_get_detail_phase(detail_view);
            detail_view_should_render = detail_view_phase > 0.01f;
        }

        bool ui_frame_started = ui_begin_frame(&ui, delta_seconds, ui_wheel_delta);

        BeginDrawing();
        ClearBackground((Color){8, 10, 18, 255});

        const Person *selected_person = interaction_get_selected(&interaction_state);
        const Person *hovered_person  = interaction_get_hovered(&interaction_state);

        bool rendered = render_scene(&render_state, &layout, &camera_controller, selected_person,
                                     hovered_person);
        if (!rendered)
        {
            app_render_scene_basic(&layout, &camera_controller, selected_person, hovered_person,
                                   &render_state.config);
        }

        if (detail_view_should_render)
        {
#if defined(ANCESTRYTREE_HAVE_RAYLIB)
            const Camera3D *detail_camera = camera_controller_get_camera(&camera_controller);
            detail_view_render(detail_view, detail_expansion_ptr, &render_state.config,
                               detail_camera);
#else
            detail_view_render(detail_view, detail_expansion_ptr, &render_state.config, NULL);
#endif
        }

        bool settings_dirty = memcmp(&settings, &persisted_settings, sizeof(Settings)) != 0;

        if (ui_frame_started)
        {
            ui_draw_overlay(&ui, tree, &layout, &camera_controller, (float)GetFPS(),
                            selected_person, hovered_person, &render_state.config, &settings,
                            settings_dirty);
            ui_end_frame(&ui);
        }

        if (settings_get_revision(&settings) != settings_applied_revision)
        {
            app_apply_settings(&settings, &render_state, &camera_controller,
                               auto_save_ready ? &auto_save : NULL);
            settings_applied_revision = settings_get_revision(&settings);
        }
        EndDrawing();

        event_process(&event_context, EVENT_PROCESS_PHASE_POST_FRAME, delta_seconds);

        if (auto_save_ready)
        {
            if (!persistence_auto_save_tick(&auto_save, delta_seconds, auto_save_error,
                                            sizeof(auto_save_error)))
            {
                AT_LOG(logger, AT_LOG_WARN, "Auto-save tick failed: %s", auto_save_error);
                auto_save_error[0] = '\0';
            }
        }
    }

    if (auto_save_ready)
    {
        if (!persistence_auto_save_flush(&auto_save, auto_save_error, sizeof(auto_save_error)) &&
            auto_save_error[0] != '\0')
        {
            AT_LOG(logger, AT_LOG_WARN, "Auto-save flush failed on shutdown: %s", auto_save_error);
        }
        persistence_auto_save_shutdown(&auto_save);
    }

    EnableCursor();
    ui_cleanup(&ui);
    if (detail_view)
    {
        detail_view_destroy(detail_view);
    }
    render_cleanup(&render_state);
    app_state_shutdown(&app_state);
    layout_result_destroy(&layout);
    family_tree_destroy(tree);
    graphics_window_shutdown(&graphics_state);
    return 0;
}

#endif /* ANCESTRYTREE_HAVE_RAYLIB */

int main(int argc, char **argv)
{
    AtLogger logger;
    at_logger_init(&logger);
    AppLaunchOptions launch_options;
    char cli_error[256];
    const char *program_name = (argc > 0 && argv) ? argv[0] : "ancestrytree";
    if (!app_cli_parse(argc, argv ? argv : NULL, &launch_options, cli_error, sizeof(cli_error)))
    {
        if (cli_error[0] != '\0')
        {
            fprintf(stderr, "Error: %s\n\n", cli_error);
        }
        app_cli_print_usage(program_name);
        return 1;
    }
    if (launch_options.show_help)
    {
        app_cli_print_usage(program_name);
        return 0;
    }
    at_logger_set_level(&logger, launch_options.log_level);
    char log_error[256];
    log_error[0] = '\0';
    if (at_logger_open_file(&logger, APP_LOG_PATH, log_error, sizeof(log_error)))
    {
        AT_LOG(&logger, AT_LOG_INFO, "Log file attached: %s", APP_LOG_PATH);
    }
    else if (log_error[0] != '\0')
    {
        AT_LOG(&logger, AT_LOG_WARN, "Failed to open log file (%s).", log_error);
    }
    else
    {
        AT_LOG(&logger, AT_LOG_WARN, "Failed to open log file.");
    }

#if defined(ANCESTRYTREE_HAVE_RAYLIB)
    AT_LOG(&logger, AT_LOG_INFO, "AncestryTree prototype starting.");
    return app_run(&logger, &launch_options);
#else
    AT_LOG(&logger, AT_LOG_INFO, "AncestryTree prototype starting.");
    AT_LOG(&logger, AT_LOG_INFO, "Rendering systems are not yet initialized in this build.");
    printf("AncestryTree stub runtime. Replace with engine initialization.\n");
    return 0;
#endif
}
