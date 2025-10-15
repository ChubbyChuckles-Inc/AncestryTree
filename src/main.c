#include "at_log.h"
#include "camera_controller.h"
#include "graphics.h"
#include "interaction.h"
#include "layout.h"
#include "path_utils.h"
#include "persistence.h"
#include "person.h"
#include "render.h"
#include "tree.h"
#include "ui.h"

#include <math.h>
#include <stdbool.h>
#include <stdio.h>
#include <string.h>

#if defined(ANCESTRYTREE_HAVE_RAYLIB)
#include <raylib.h>
#endif

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

static void app_collect_camera_input(CameraControllerInput *input, bool auto_orbit_enabled)
{
    if (!input)
    {
        return;
    }

    const float orbit_sensitivity = 0.15f;
    const float pan_sensitivity = 0.5f;
    const float pan_keyboard_sensitivity = 1.0f;

    camera_controller_input_clear(input);

    Vector2 mouse_delta = GetMouseDelta();
    if (IsMouseButtonDown(MOUSE_BUTTON_RIGHT))
    {
        input->yaw_delta -= mouse_delta.x * orbit_sensitivity;
        input->pitch_delta -= mouse_delta.y * orbit_sensitivity;
        DisableCursor();
    }
    else
    {
        EnableCursor();
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

    input->zoom_delta = GetMouseWheelMove();

    const float threshold = 0.001f;
    bool has_manual_orbit = (fabsf(input->yaw_delta) > threshold) || (fabsf(input->pitch_delta) > threshold);
    bool has_manual_pan = (fabsf(input->pan_right) > threshold) || (fabsf(input->pan_up) > threshold);
    bool has_manual_zoom = fabsf(input->zoom_delta) > threshold;

    if (auto_orbit_enabled && !has_manual_orbit && !has_manual_pan && !has_manual_zoom)
    {
        input->yaw_delta -= 0.25f;
    }
}

static void app_render_scene_basic(const LayoutResult *layout, const CameraController *camera,
                                   const Person *selected_person, const Person *hovered_person)
{
    const Camera3D *raylib_camera = camera_controller_get_camera(camera);
    if (!raylib_camera)
    {
        return;
    }

    BeginMode3D(*raylib_camera);
    DrawGrid(24, 1.0f);

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

    char tree_path[512];
    FamilyTree *tree = NULL;
    if (app_try_find_asset("assets/example_tree.json", tree_path, sizeof(tree_path)))
    {
        AT_LOG(logger, AT_LOG_INFO, "Loading sample tree from %s", tree_path);
        tree = persistence_tree_load(tree_path, error_buffer, sizeof(error_buffer));
        if (!tree)
        {
            AT_LOG(logger, AT_LOG_WARN, "Failed to load sample tree (%s). Using placeholder data.", error_buffer);
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
    }

    LayoutResult layout = layout_calculate(tree);
    float layout_center[3];
    if (layout_compute_center(&layout, layout_center))
    {
        camera_controller_focus(&camera_controller, layout_center, camera_controller.config.default_radius);
    }

    RenderState render_state;
    render_state_init(&render_state);
    char render_error[256];
    bool render_ready = render_init(&render_state, NULL, render_error, sizeof(render_error));
    AT_LOG_WARN_IF(logger, !render_ready, "Render pipeline fallback: %s", render_error);

    InteractionState interaction_state;
    interaction_state_init(&interaction_state);
    interaction_state_set_pick_radius(&interaction_state, render_state.config.sphere_radius);

    UIContext ui;
    bool ui_ready = ui_init(&ui, graphics_state.width, graphics_state.height);
    AT_LOG_WARN_IF(logger, !ui_ready, "UI overlay unavailable; Nuklear or raylib might be missing.");

    SetTargetFPS((int)config.target_fps);

    while (!WindowShouldClose())
    {
        float delta_seconds = GetFrameTime();
        if (graphics_window_handle_resize(&graphics_state))
        {
            ui_resize(&ui, graphics_state.width, graphics_state.height);
        }

        CameraControllerInput controller_input;
        app_collect_camera_input(&controller_input, ui_auto_orbit_enabled(&ui));
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
        if (IsKeyPressed(KEY_ESCAPE))
        {
            interaction_clear_selection(&interaction_state);
        }
#endif

        const Person *selected_person = interaction_get_selected(&interaction_state);
        const Person *hovered_person = interaction_get_hovered(&interaction_state);

        bool rendered = render_scene(&render_state, &layout, &camera_controller, selected_person, hovered_person);
        if (!rendered)
        {
            app_render_scene_basic(&layout, &camera_controller, selected_person, hovered_person);
        }

        if (ui_frame_started)
        {
            ui_draw_overlay(&ui, tree, &layout, &camera_controller, (float)GetFPS());
            ui_end_frame(&ui);
        }

        EndDrawing();
    }

    EnableCursor();
    ui_cleanup(&ui);
    render_cleanup(&render_state);
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
