#ifndef GRAPHICS_H
#define GRAPHICS_H

#include <stdbool.h>
#include <stddef.h>

typedef struct GraphicsConfig
{
    int width;
    int height;
    const char *title;
    const char *icon_path;
    bool enable_msaa;
    bool start_fullscreen;
    unsigned int target_fps;
} GraphicsConfig;

typedef struct GraphicsState
{
    bool initialized;
    bool fullscreen;
    int width;
    int height;
} GraphicsState;

void graphics_state_init(GraphicsState *state);
GraphicsConfig graphics_config_default(void);
bool graphics_has_raylib_support(void);

bool graphics_window_init(GraphicsState *state, const GraphicsConfig *config, char *error_buffer,
                          size_t error_buffer_size);
void graphics_window_shutdown(GraphicsState *state);
bool graphics_window_handle_resize(GraphicsState *state);
bool graphics_window_toggle_fullscreen(GraphicsState *state, char *error_buffer, size_t error_buffer_size);

#endif /* GRAPHICS_H */
