#include "graphics.h"
#include "test_framework.h"

#include <string.h>

TEST(test_graphics_window_init_handles_backend_availability)
{
    GraphicsState state;
    graphics_state_init(&state);
    GraphicsConfig config = graphics_config_default();
    char error[256];

    bool has_backend = graphics_has_raylib_support();
    bool result = graphics_window_init(&state, &config, error, sizeof(error));
    if (has_backend)
    {
        ASSERT_TRUE(result);
        ASSERT_TRUE(state.initialized);
        ASSERT_TRUE(state.width > 0);
        ASSERT_TRUE(state.height > 0);
        graphics_window_shutdown(&state);
    }
    else
    {
        ASSERT_FALSE(result);
        ASSERT_FALSE(state.initialized);
        ASSERT_NOT_NULL(strstr(error, "raylib"));
    }
}

TEST(test_graphics_fullscreen_requires_initialized_window)
{
    GraphicsState state;
    graphics_state_init(&state);
    char error[128];
    ASSERT_FALSE(graphics_window_toggle_fullscreen(&state, error, sizeof(error)));
    ASSERT_NOT_NULL(strstr(error, "window not initialized"));
}

void register_graphics_tests(TestRegistry *registry)
{
    REGISTER_TEST(registry, test_graphics_window_init_handles_backend_availability);
    REGISTER_TEST(registry, test_graphics_fullscreen_requires_initialized_window);
}
