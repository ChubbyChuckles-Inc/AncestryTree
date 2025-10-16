#include "app.h"
#include "at_log.h"
#include "camera_controller.h"
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
#include "tree.h"
#include "ui.h"

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

#if defined(ANCESTRYTREE_HAVE_RAYLIB)

static bool app_try_find_asset(const char *relative_path, char *resolved_path, size_t capacity)
{
    if (!relative_path || !resolved_path || capacity == 0U)
    {
        return false;
    }

    const unsigned int level_candidates[] = {0U, 1U, 2U, 3U};
    const char *application_dir = GetApplicationDirectory();
    for (size_t index = 0U; index < sizeof(level_candidates) / sizeof(level_candidates[0]); ++index)
    {
        if (path_join_relative(application_dir, level_candidates[index], relative_path, resolved_path, capacity) &&
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
static void app_apply_settings(const Settings *settings, RenderState *render_state, CameraController *camera,
                               PersistenceAutoSave *auto_save);

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
    (void)ui_notify_status(ui, message);
    if (logger)
    {
        AT_LOG(logger, AT_LOG_ERROR, "%s", message);
    }
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

static bool app_handle_save(FamilyTree *tree, AppFileState *files, char *error_buffer, size_t error_buffer_size)
{
    if (!tree || !files)
    {
        return false;
    }
    const char *target_path = files->current_path[0] != '\0' ? files->current_path : APP_DEFAULT_SAVE_PATH;
    if (!app_save_tree_to_path(tree, target_path, error_buffer, error_buffer_size))
    {
        return false;
    }
    app_file_state_set(files, target_path);
    return true;
}

static bool app_handle_save_as(FamilyTree *tree, AppFileState *files, char *error_buffer, size_t error_buffer_size,
                               char *out_path, size_t out_path_capacity)
{
    if (!tree || !files || !out_path || out_path_capacity == 0U)
    {
        return false;
    }
    if (!app_generate_timestamped_path(out_path, out_path_capacity))
    {
        return false;
    }
    if (!app_save_tree_to_path(tree, out_path, error_buffer, error_buffer_size))
    {
        return false;
    }
    app_file_state_set(files, out_path);
    return true;
}

static bool app_swap_tree(FamilyTree **tree, LayoutResult *layout, FamilyTree *replacement)
{
    if (!tree || !layout || !replacement)
    {
        return false;
    }
    layout_result_destroy(layout);
    if (*tree)
    {
        family_tree_destroy(*tree);
    }
    *tree = replacement;
    *layout = layout_calculate(*tree);
    return true;
}

static void app_on_tree_changed(LayoutResult *layout, InteractionState *interaction_state, RenderState *render_state,
                                CameraController *camera, PersistenceAutoSave *auto_save)
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

static void app_process_ui_event(const UIEvent *event, UIContext *ui, AppFileState *file_state, FamilyTree **tree,
                                 LayoutResult *layout, InteractionState *interaction_state,
                                 RenderState *render_state, CameraController *camera, AtLogger *logger,
                                 Settings *settings, Settings *persisted_settings, const char *settings_path,
                                 PersistenceAutoSave *auto_save, unsigned int *settings_applied_revision,
                                 AppState *app_state)
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
        if (!app_swap_tree(tree, layout, replacement))
        {
            family_tree_destroy(replacement);
            app_report_error(ui, logger, "Unable to swap in new tree data.");
            return;
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
        char resolved_path[512];
        if (!app_try_find_asset("assets/example_tree.json", resolved_path, sizeof(resolved_path)))
        {
            app_report_error(ui, logger, "Sample tree not found; ensure assets/example_tree.json exists.");
            return;
        }
        FamilyTree *loaded = persistence_tree_load(resolved_path, error_buffer, sizeof(error_buffer));
        if (!loaded)
        {
            char message[256];
            (void)snprintf(message, sizeof(message), "Load failed: %s", error_buffer);
            app_report_error(ui, logger, message);
            return;
        }
        if (!app_swap_tree(tree, layout, loaded))
        {
            family_tree_destroy(loaded);
            app_report_error(ui, logger, "Unable to replace current tree with loaded data.");
            return;
        }
        app_file_state_set(file_state, resolved_path);
        app_on_tree_changed(layout, interaction_state, render_state, camera, auto_save);
        if (app_state)
        {
            app_state_reset_history(app_state);
            app_state_clear_tree_dirty(app_state);
        }
        app_report_status(ui, logger, "Sample tree loaded.");
    }
    break;
    case UI_EVENT_SAVE_TREE:
        if (!*tree)
        {
            app_report_error(ui, logger, "No tree available to save.");
            return;
        }
        if (!app_handle_save(*tree, file_state, error_buffer, sizeof(error_buffer)))
        {
            char message[256];
            (void)snprintf(message, sizeof(message), "Save failed: %s", error_buffer);
            app_report_error(ui, logger, message);
            return;
        }
        else
        {
            if (app_state)
            {
                app_state_clear_tree_dirty(app_state);
            }
            char message[256];
            (void)snprintf(message, sizeof(message), "Saved tree to %s", file_state->current_path);
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
            if (!app_handle_save_as(*tree, file_state, error_buffer, sizeof(error_buffer), destination,
                                    sizeof(destination)))
            {
                char message[256];
                (void)snprintf(message, sizeof(message), "Save As failed: %s", error_buffer);
                app_report_error(ui, logger, message);
                return;
            }
            if (app_state)
            {
                app_state_clear_tree_dirty(app_state);
            }
            char message[256];
            (void)snprintf(message, sizeof(message), "Saved tree to %s", destination);
            app_report_status(ui, logger, message);
        }
        break;
    case UI_EVENT_SAVE_SETTINGS:
    {
        if (!settings || !settings_path)
        {
            app_report_error(ui, logger, "Settings context unavailable; unable to save configuration.");
            break;
        }
        char settings_error[256];
        settings_error[0] = '\0';
        if (!settings_save(settings, settings_path, settings_error, sizeof(settings_error)))
        {
            char message[256];
            if (settings_error[0] != '\0')
            {
                (void)snprintf(message, sizeof(message), "Failed to save settings: %s", settings_error);
            }
            else
            {
                (void)snprintf(message, sizeof(message), "Failed to save settings to %s", settings_path);
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
            app_report_error(ui, logger, "Settings context unavailable; unable to reload configuration.");
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
                (void)snprintf(message, sizeof(message), "Failed to reload settings: %s", settings_error);
            }
            else
            {
                (void)snprintf(message, sizeof(message), "Failed to reload settings from %s", settings_path);
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
            (void)snprintf(message, sizeof(message), "Focus failed: person %u not found.", target_id);
            app_report_error(ui, logger, message);
            break;
        }
        if (interaction_state)
        {
            interaction_select_person(interaction_state, target);
        }
        bool has_position = false;
        float focus_position[3] = {0.0f, 0.0f, 0.0f};
        for (size_t index = 0U; index < layout->count; ++index)
        {
            if (layout->nodes[index].person == target)
            {
                focus_position[0] = layout->nodes[index].position[0];
                focus_position[1] = layout->nodes[index].position[1];
                focus_position[2] = layout->nodes[index].position[2];
                has_position = true;
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

static void app_handle_pending_ui_events(UIContext *ui, AppFileState *file_state, FamilyTree **tree,
                                         LayoutResult *layout, InteractionState *interaction_state,
                                         RenderState *render_state, CameraController *camera, AtLogger *logger,
                                         Settings *settings, Settings *persisted_settings, const char *settings_path,
                                         PersistenceAutoSave *auto_save, unsigned int *settings_applied_revision,
                                         AppState *app_state)
{
    if (!ui)
    {
        return;
    }
    UIEvent events[UI_EVENT_QUEUE_CAPACITY];
    size_t count = ui_poll_events(ui, events, UI_EVENT_QUEUE_CAPACITY);
    for (size_t index = 0U; index < count; ++index)
    {
        app_process_ui_event(&events[index], ui, file_state, tree, layout, interaction_state, render_state, camera,
                             logger, settings, persisted_settings, settings_path, auto_save, settings_applied_revision,
                             app_state);
    }
}

static void app_handle_shortcut_input(UIContext *ui, AppFileState *file_state, FamilyTree **tree,
                                      LayoutResult *layout, InteractionState *interaction_state,
                                      RenderState *render_state, CameraController *camera, AtLogger *logger,
                                      Settings *settings, Settings *persisted_settings, const char *settings_path,
                                      PersistenceAutoSave *auto_save, unsigned int *settings_applied_revision,
                                      AppState *app_state)
{
    if (!ui)
    {
        return;
    }
#if defined(ANCESTRYTREE_HAVE_RAYLIB)
    ShortcutState state;
    state.ctrl_down = IsKeyDown(KEY_LEFT_CONTROL) || IsKeyDown(KEY_RIGHT_CONTROL);
    state.shift_down = IsKeyDown(KEY_LEFT_SHIFT) || IsKeyDown(KEY_RIGHT_SHIFT);
    state.key_new_pressed = IsKeyPressed(KEY_N);
    state.key_open_pressed = IsKeyPressed(KEY_O);
    state.key_save_pressed = IsKeyPressed(KEY_S);
    state.key_undo_pressed = IsKeyPressed(KEY_Z);
    state.key_redo_pressed = IsKeyPressed(KEY_Y);
    state.key_space_pressed = IsKeyPressed(KEY_SPACE);
    state.key_escape_pressed = IsKeyPressed(KEY_ESCAPE);

    ShortcutResult result;
    shortcuts_evaluate(&state, &result);
    if (result.event != UI_EVENT_NONE)
    {
        if (!ui_event_enqueue(ui, result.event))
        {
            UIEvent fallback_event;
            fallback_event.type = result.event;
            fallback_event.param_u32 = 0U;
            app_process_ui_event(&fallback_event, ui, file_state, tree, layout, interaction_state, render_state,
                                 camera, logger, settings, persisted_settings, settings_path, auto_save,
                                 settings_applied_revision, app_state);
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

static void app_apply_settings(const Settings *settings, RenderState *render_state, CameraController *camera,
                               PersistenceAutoSave *auto_save)
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

static void app_collect_camera_input(CameraControllerInput *input, bool auto_orbit_enabled, const Settings *settings)
{
    if (!input)
    {
        return;
    }

    float orbit_sensitivity = 0.15f;
    float pan_sensitivity = 0.5f;
    float pan_keyboard_sensitivity = 1.0f;
    float zoom_sensitivity = 1.0f;
    settings_runtime_compute_input_sensitivity(settings, &orbit_sensitivity, &pan_sensitivity,
                                               &pan_keyboard_sensitivity, &zoom_sensitivity);
    static bool cursor_locked = false;

    camera_controller_input_clear(input);

    Vector2 mouse_delta = GetMouseDelta();
    bool orbiting = IsMouseButtonDown(MOUSE_BUTTON_RIGHT);
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

    input->zoom_delta = GetMouseWheelMove() * zoom_sensitivity;

    const float threshold = 0.001f;
    bool has_manual_orbit = (fabsf(input->yaw_delta) > threshold) || (fabsf(input->pitch_delta) > threshold);
    bool has_manual_pan = (fabsf(input->pan_right) > threshold) || (fabsf(input->pan_up) > threshold);
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
            const Person *person = node->person;
            Color color = person && person->is_alive ? (Color){0, 195, 255, 255} : (Color){200, 120, 240, 255};
            float radius = 0.6f;
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

static int app_run(AtLogger *logger)
{
    if (!graphics_has_raylib_support())
    {
        AT_LOG(logger, AT_LOG_ERROR, "This build lacks raylib support; cannot start renderer.");
        return 1;
    }

    GraphicsState graphics_state;
    graphics_state_init(&graphics_state);

    GraphicsConfig config = graphics_config_default();
    config.title = "AncestryTree";
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
    const char *settings_path = APP_SETTINGS_PATH;
    char settings_error[256];
    settings_error[0] = '\0';
    if (settings_try_load(&settings, settings_path, settings_error, sizeof(settings_error)))
    {
        persisted_settings = settings;
        AT_LOG(logger, AT_LOG_INFO, "Loaded settings from %s", settings_path);
    }
    else
    {
        if (settings_error[0] != '\0')
        {
            AT_LOG(logger, AT_LOG_WARN, "Settings load failed (%s); defaults in use.", settings_error);
        }
        else
        {
            AT_LOG(logger, AT_LOG_WARN, "Settings file not found; defaults in use.");
        }
    }
    unsigned int settings_applied_revision = settings_get_revision(&settings);

    char tree_path[512];
    FamilyTree *tree = NULL;
    AppFileState file_state;
    app_file_state_clear(&file_state);
    bool tree_loaded_from_asset = false;
    bool placeholder_used = false;
    if (app_try_find_asset("assets/example_tree.json", tree_path, sizeof(tree_path)))
    {
        AT_LOG(logger, AT_LOG_INFO, "Loading sample tree from %s", tree_path);
        tree = persistence_tree_load(tree_path, error_buffer, sizeof(error_buffer));
        if (!tree)
        {
            AT_LOG(logger, AT_LOG_WARN, "Failed to load sample tree (%s). Using placeholder data.", error_buffer);
        }
        else
        {
            tree_loaded_from_asset = true;
            app_file_state_set(&file_state, tree_path);
        }
    }
    else
    {
        AT_LOG(logger, AT_LOG_WARN, "Sample tree asset not found; using placeholder data.");
    }

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
    }

    LayoutResult layout = layout_calculate(tree);
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
    if (!app_state_configure(&app_state, &tree, &layout, &interaction_state, &camera_controller, &settings,
                             &persisted_settings))
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

    PersistenceAutoSave auto_save;
    memset(&auto_save, 0, sizeof(auto_save));
    bool auto_save_ready = false;
    char auto_save_error[256];
    auto_save_error[0] = '\0';
    PersistenceAutoSaveConfig auto_save_config;
    auto_save_config.tree_supplier = app_auto_save_tree_supplier;
    auto_save_config.user_data = &tree;
    auto_save_config.path = APP_AUTO_SAVE_PATH;
    auto_save_config.interval_seconds = settings.auto_save_interval_seconds;
    if (persistence_auto_save_init(&auto_save, &auto_save_config, auto_save_error, sizeof(auto_save_error)))
    {
        auto_save_ready = true;
        persistence_auto_save_set_enabled(&auto_save, settings.auto_save_enabled);
        persistence_auto_save_mark_dirty(&auto_save);
    }
    else
    {
        AT_LOG(logger, AT_LOG_WARN, "Auto-save unavailable (%s).", auto_save_error);
    }

    app_apply_settings(&settings, &render_state, &camera_controller, auto_save_ready ? &auto_save : NULL);
    settings_applied_revision = settings_get_revision(&settings);

    UIContext ui;
    bool ui_ready = ui_init(&ui, graphics_state.width, graphics_state.height);
    AT_LOG_WARN_IF(logger, !ui_ready, "UI overlay unavailable; Nuklear or raylib might be missing.");

    if (tree_loaded_from_asset)
    {
        app_report_status(&ui, logger, "Sample tree loaded.");
        app_state_clear_tree_dirty(&app_state);
    }
    else if (placeholder_used)
    {
        app_report_status(&ui, logger, "Placeholder tree initialised.");
        app_state_mark_tree_dirty(&app_state);
    }

    SetTargetFPS((int)config.target_fps);

    while (!WindowShouldClose())
    {
        float delta_seconds = GetFrameTime();
        if (graphics_window_handle_resize(&graphics_state))
        {
            ui_resize(&ui, graphics_state.width, graphics_state.height);
            if (render_ready)
            {
                if (!render_resize(&render_state, graphics_state.width, graphics_state.height, render_error,
                                   sizeof(render_error)))
                {
                    if (!render_target_warned)
                    {
                        AT_LOG(logger, AT_LOG_WARN, "Render target resize failed: %s", render_error);
                        render_target_warned = true;
                    }
                }
                else if (render_target_warned)
                {
                    AT_LOG(logger, AT_LOG_INFO, "Render target restored after resize.");
                    render_target_warned = false;
                }
            }
        }

        CameraControllerInput controller_input;
        app_collect_camera_input(&controller_input, ui_auto_orbit_enabled(&ui), &settings);
        camera_controller_update(&camera_controller, &controller_input, delta_seconds);

        bool ui_frame_started = ui_begin_frame(&ui, delta_seconds);

        BeginDrawing();
        ClearBackground((Color){8, 10, 18, 255});

#if defined(ANCESTRYTREE_HAVE_RAYLIB)
        Vector2 mouse = GetMousePosition();
        interaction_update_hover(&interaction_state, &layout, &camera_controller, mouse.x, mouse.y);
        if (IsMouseButtonPressed(MOUSE_BUTTON_LEFT))
        {
            bool keep_selection = IsKeyDown(KEY_LEFT_SHIFT) || IsKeyDown(KEY_RIGHT_SHIFT);
            interaction_select_at_cursor(&interaction_state, &layout, &camera_controller, mouse.x, mouse.y,
                                         !keep_selection);
        }
#endif

        const Person *selected_person = interaction_get_selected(&interaction_state);
        const Person *hovered_person = interaction_get_hovered(&interaction_state);

        bool rendered = render_scene(&render_state, &layout, &camera_controller, selected_person, hovered_person);
        if (!rendered)
        {
            app_render_scene_basic(&layout, &camera_controller, selected_person, hovered_person,
                                   &render_state.config);
        }

        bool settings_dirty = memcmp(&settings, &persisted_settings, sizeof(Settings)) != 0;

        if (ui_frame_started)
        {
            ui_draw_overlay(&ui, tree, &layout, &camera_controller, (float)GetFPS(), selected_person, hovered_person,
                            &render_state.config, &settings, settings_dirty);
            ui_end_frame(&ui);
        }

        if (settings_get_revision(&settings) != settings_applied_revision)
        {
            app_apply_settings(&settings, &render_state, &camera_controller, auto_save_ready ? &auto_save : NULL);
            settings_applied_revision = settings_get_revision(&settings);
        }

        app_handle_shortcut_input(&ui, &file_state, &tree, &layout, &interaction_state, &render_state,
                                  &camera_controller, logger, &settings, &persisted_settings, settings_path,
                                  &auto_save, &settings_applied_revision, &app_state);
        EndDrawing();

        app_handle_pending_ui_events(&ui, &file_state, &tree, &layout, &interaction_state, &render_state,
                                     &camera_controller, logger, &settings, &persisted_settings, settings_path,
                                     &auto_save, &settings_applied_revision, &app_state);

        if (auto_save_ready)
        {
            if (!persistence_auto_save_tick(&auto_save, delta_seconds, auto_save_error, sizeof(auto_save_error)))
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
    render_cleanup(&render_state);
    app_state_shutdown(&app_state);
    layout_result_destroy(&layout);
    family_tree_destroy(tree);
    graphics_window_shutdown(&graphics_state);
    return 0;
}

#endif /* ANCESTRYTREE_HAVE_RAYLIB */

int main(void)
{
    AtLogger logger;
    at_logger_init(&logger);
    at_logger_set_level(&logger, AT_LOG_INFO);

#if defined(ANCESTRYTREE_HAVE_RAYLIB)
    AT_LOG(&logger, AT_LOG_INFO, "AncestryTree prototype starting.");
    return app_run(&logger);
#else
    AT_LOG(&logger, AT_LOG_INFO, "AncestryTree prototype starting.");
    AT_LOG(&logger, AT_LOG_INFO, "Rendering systems are not yet initialized in this build.");
    printf("AncestryTree stub runtime. Replace with engine initialization.\n");
    return 0;
#endif
}
