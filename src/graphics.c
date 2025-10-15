#include "graphics.h"

#include <stdio.h>
#include <string.h>

#if defined(ANCESTRYTREE_HAVE_RAYLIB)
#include <raylib.h>
#endif

static bool graphics_set_error(char *buffer, size_t buffer_size, const char *message)
{
    if (!buffer || buffer_size == 0U)
    {
        return false;
    }
    const char *text = message ? message : "unknown graphics error";
#if defined(_MSC_VER)
    (void)strncpy_s(buffer, buffer_size, text, _TRUNCATE);
#else
    (void)snprintf(buffer, buffer_size, "%s", text);
#endif
    return false;
}

void graphics_state_init(GraphicsState *state)
{
    if (!state)
    {
        return;
    }
    memset(state, 0, sizeof(*state));
}

GraphicsConfig graphics_config_default(void)
{
    GraphicsConfig config;
    config.width = 1280;
    config.height = 720;
    config.title = "AncestryTree";
    config.icon_path = NULL;
    config.enable_msaa = true;
    config.start_fullscreen = false;
    config.target_fps = 60U;
    return config;
}

bool graphics_has_raylib_support(void)
{
#if defined(ANCESTRYTREE_HAVE_RAYLIB)
    return true;
#else
    return false;
#endif
}

bool graphics_window_init(GraphicsState *state, const GraphicsConfig *config, char *error_buffer,
                          size_t error_buffer_size)
{
    if (!state)
    {
        return graphics_set_error(error_buffer, error_buffer_size, "graphics state pointer is NULL");
    }
    if (!config)
    {
        return graphics_set_error(error_buffer, error_buffer_size, "graphics config pointer is NULL");
    }
    if (state->initialized)
    {
        return true;
    }
#if !defined(ANCESTRYTREE_HAVE_RAYLIB)
    graphics_state_init(state);
    return graphics_set_error(error_buffer, error_buffer_size,
                              "raylib support not built; set RAYLIB_HOME and rebuild");
#else
    SetConfigFlags(FLAG_WINDOW_RESIZABLE);
    if (config->enable_msaa)
    {
        SetConfigFlags(FLAG_MSAA_4X_HINT);
    }
    InitWindow(config->width, config->height, config->title ? config->title : "AncestryTree");
    if (!IsWindowReady())
    {
        CloseWindow();
        graphics_state_init(state);
        return graphics_set_error(error_buffer, error_buffer_size, "failed to initialize raylib window");
    }

    if (config->icon_path && config->icon_path[0] != '\0')
    {
        Image icon = LoadImage(config->icon_path);
        if (icon.data != NULL)
        {
            SetWindowIcon(icon);
            UnloadImage(icon);
        }
    }

    if (config->target_fps > 0U)
    {
        SetTargetFPS((int)config->target_fps);
    }
    if (config->start_fullscreen)
    {
        ToggleFullscreen();
        state->fullscreen = true;
    }
    state->initialized = true;
    state->width = GetScreenWidth();
    state->height = GetScreenHeight();
    return true;
#endif
}

void graphics_window_shutdown(GraphicsState *state)
{
    if (!state)
    {
        return;
    }
#if defined(ANCESTRYTREE_HAVE_RAYLIB)
    if (state->initialized)
    {
        CloseWindow();
    }
#endif
    graphics_state_init(state);
}

bool graphics_window_handle_resize(GraphicsState *state)
{
    if (!state || !state->initialized)
    {
        return false;
    }
#if defined(ANCESTRYTREE_HAVE_RAYLIB)
    if (IsWindowResized())
    {
        state->width = GetScreenWidth();
        state->height = GetScreenHeight();
        return true;
    }
#endif
    return false;
}

bool graphics_window_toggle_fullscreen(GraphicsState *state, char *error_buffer, size_t error_buffer_size)
{
    if (!state)
    {
        return graphics_set_error(error_buffer, error_buffer_size, "graphics state pointer is NULL");
    }
    if (!state->initialized)
    {
        return graphics_set_error(error_buffer, error_buffer_size, "window not initialized");
    }
#if !defined(ANCESTRYTREE_HAVE_RAYLIB)
    return graphics_set_error(error_buffer, error_buffer_size, "raylib support not built; fullscreen unavailable");
#else
    ToggleFullscreen();
    state->fullscreen = !state->fullscreen;
    state->width = GetScreenWidth();
    state->height = GetScreenHeight();
    return true;
#endif
}

/*
 * Manual validation checklist:
 * 1. Build with raylib enabled, launch the app, and verify the window opens at the
 *    requested resolution and title text.
 * 2. Toggle fullscreen (Alt+Enter or calling graphics_window_toggle_fullscreen) and
 *    confirm the stored dimensions update and the window exits fullscreen reliably.
 * 3. Resize the window and ensure graphics_window_handle_resize captures the new size.
 */
