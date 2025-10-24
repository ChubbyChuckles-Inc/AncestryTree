#include "settings_runtime.h"

#include <math.h>
#include <string.h>

#define DEFAULT_ORBIT_SENSITIVITY 0.15f
#define DEFAULT_PAN_SENSITIVITY 0.5f
#define DEFAULT_KEYBOARD_PAN_SENSITIVITY 1.0f
#define DEFAULT_ZOOM_SENSITIVITY 1.0f

#define BASE_ROTATION_SPEED 1.5f
#define BASE_PAN_SPEED 10.0f
#define BASE_ZOOM_SPEED 15.0f

static float clamp_positive(float value, float minimum, float maximum, float fallback)
{
    if (!(value > 0.0f))
    {
        value = fallback;
    }
    if (value < minimum)
    {
        value = minimum;
    }
    if (value > maximum)
    {
        value = maximum;
    }
    return value;
}

static void apply_color_scheme(RenderConfig *config, SettingsColorScheme scheme)
{
    if (!config)
    {
        return;
    }
    switch (scheme)
    {
    case SETTINGS_COLOR_SCHEME_SOLAR_ORCHID:
        config->alive_color                   = (RenderColor){255, 178, 102, 255};
        config->deceased_color                = (RenderColor){190, 120, 255, 255};
        config->selected_outline_color        = (RenderColor){255, 255, 255, 255};
        config->connection_color_parent_child = (RenderColor){255, 170, 90, 210};
        config->connection_color_spouse       = (RenderColor){220, 150, 255, 210};
        break;
    case SETTINGS_COLOR_SCHEME_CYAN_GRAPH:
    default:
        config->alive_color                   = (RenderColor){0, 195, 255, 255};
        config->deceased_color                = (RenderColor){200, 120, 240, 255};
        config->selected_outline_color        = (RenderColor){255, 255, 255, 255};
        config->connection_color_parent_child = (RenderColor){64, 200, 255, 200};
        config->connection_color_spouse       = (RenderColor){255, 180, 120, 200};
        break;
    }
}

static void apply_default_environment(RenderConfig *config)
{
    if (!config)
    {
        return;
    }
    config->show_background_gradient   = true;
    config->background_gradient_top    = (RenderColor){10, 30, 64, 255};
    config->background_gradient_bottom = (RenderColor){2, 8, 20, 255};
    config->enable_fog                 = true;
    if (!(config->fog_start_distance >= 0.0f))
    {
        config->fog_start_distance = 18.0f;
    }
    if (!(config->fog_end_distance > config->fog_start_distance))
    {
        config->fog_end_distance = 78.0f;
    }
    config->fog_color           = (RenderColor){8, 20, 42, 255};
    config->enable_rim_lighting = true;
    if (!(config->rim_intensity > 0.0f))
    {
        config->rim_intensity = 1.1f;
    }
    if (!(config->rim_power > 0.0f))
    {
        config->rim_power = 2.6f;
    }
    config->rim_color = (RenderColor){100, 240, 255, 255};
}

static void apply_high_contrast_environment(RenderConfig *config)
{
    if (!config)
    {
        return;
    }
    config->alive_color                   = (RenderColor){255, 255, 255, 255};
    config->deceased_color                = (RenderColor){255, 204, 0, 255};
    config->selected_outline_color        = (RenderColor){0, 0, 0, 255};
    config->connection_color_parent_child = (RenderColor){255, 255, 0, 255};
    config->connection_color_spouse       = (RenderColor){0, 255, 255, 255};
    config->show_background_gradient      = false;
    config->background_gradient_top       = (RenderColor){0, 0, 0, 255};
    config->background_gradient_bottom    = (RenderColor){0, 0, 0, 255};
    config->enable_fog                    = false;
    config->fog_start_distance            = 0.0f;
    config->fog_end_distance              = 0.0f;
    config->fog_color                     = (RenderColor){0, 0, 0, 255};
    config->enable_rim_lighting           = true;
    config->rim_intensity                 = fmaxf(1.2f, config->rim_intensity);
    config->rim_power                     = fmaxf(2.4f, config->rim_power);
    config->rim_color                     = (RenderColor){255, 255, 255, 255};
    config->connection_antialiasing       = true;
}

void settings_runtime_compute_input_sensitivity(const Settings *settings, float *orbit_sensitivity,
                                                float *pan_mouse_sensitivity,
                                                float *pan_keyboard_sensitivity,
                                                float *zoom_sensitivity)
{
    float orbit        = DEFAULT_ORBIT_SENSITIVITY;
    float pan_mouse    = DEFAULT_PAN_SENSITIVITY;
    float pan_keyboard = DEFAULT_KEYBOARD_PAN_SENSITIVITY;
    float zoom         = DEFAULT_ZOOM_SENSITIVITY;

    if (settings)
    {
        orbit        = settings->camera_rotation_sensitivity;
        pan_mouse    = settings->camera_pan_sensitivity;
        pan_keyboard = settings->camera_keyboard_pan_sensitivity;
        zoom         = settings->camera_zoom_sensitivity;
    }

    orbit        = clamp_positive(orbit, 0.05f, 5.0f, DEFAULT_ORBIT_SENSITIVITY);
    pan_mouse    = clamp_positive(pan_mouse, 0.05f, 10.0f, DEFAULT_PAN_SENSITIVITY);
    pan_keyboard = clamp_positive(pan_keyboard, 0.05f, 10.0f, DEFAULT_KEYBOARD_PAN_SENSITIVITY);
    zoom         = clamp_positive(zoom, 0.05f, 5.0f, DEFAULT_ZOOM_SENSITIVITY);

    if (orbit_sensitivity)
    {
        *orbit_sensitivity = orbit;
    }
    if (pan_mouse_sensitivity)
    {
        *pan_mouse_sensitivity = pan_mouse;
    }
    if (pan_keyboard_sensitivity)
    {
        *pan_keyboard_sensitivity = pan_keyboard;
    }
    if (zoom_sensitivity)
    {
        *zoom_sensitivity = zoom;
    }
}

bool settings_runtime_apply_camera(const Settings *settings, CameraController *controller)
{
    if (!controller)
    {
        return false;
    }

    float orbit        = DEFAULT_ORBIT_SENSITIVITY;
    float pan_mouse    = DEFAULT_PAN_SENSITIVITY;
    float pan_keyboard = DEFAULT_KEYBOARD_PAN_SENSITIVITY;
    float zoom         = DEFAULT_ZOOM_SENSITIVITY;
    settings_runtime_compute_input_sensitivity(settings, &orbit, &pan_mouse, &pan_keyboard, &zoom);

    float rotation_factor = orbit / DEFAULT_ORBIT_SENSITIVITY;
    if (!(rotation_factor > 0.0f))
    {
        rotation_factor = 1.0f;
    }
    float pan_factor = pan_mouse / DEFAULT_PAN_SENSITIVITY;
    if (!(pan_factor > 0.0f))
    {
        pan_factor = 1.0f;
    }
    float zoom_factor = zoom / DEFAULT_ZOOM_SENSITIVITY;
    if (!(zoom_factor > 0.0f))
    {
        zoom_factor = 1.0f;
    }

    controller->config.rotation_speed = BASE_ROTATION_SPEED * rotation_factor;
    controller->config.pan_speed      = BASE_PAN_SPEED * pan_factor;
    controller->config.zoom_speed     = BASE_ZOOM_SPEED * zoom_factor;

    (void)pan_keyboard;
    return true;
}

bool settings_runtime_apply_render(const Settings *settings, RenderConfig *config)
{
    if (!config)
    {
        return false;
    }

    apply_default_environment(config);

    if (!settings)
    {
        apply_color_scheme(config, SETTINGS_COLOR_SCHEME_CYAN_GRAPH);
        config->connection_antialiasing = true;
        config->glow_intensity          = fmaxf(0.3f, config->glow_intensity);
        config->connection_radius       = fmaxf(0.03f, config->connection_radius);
        config->show_profile_images     = true;
        return true;
    }

    if (settings->graphics_quality == SETTINGS_GRAPHICS_QUALITY_PERFORMANCE)
    {
        config->connection_antialiasing = false;
        config->glow_intensity          = fmaxf(0.3f, 0.55f);
        config->connection_radius       = fmaxf(0.02f, 0.035f);
        config->show_profile_images     = false;
    }
    else
    {
        config->connection_antialiasing = true;
        config->glow_intensity          = fmaxf(0.3f, 0.85f);
        config->connection_radius       = fmaxf(0.03f, 0.05f);
        config->show_profile_images     = true;
    }

    apply_color_scheme(config, settings->color_scheme);

    if (settings->high_contrast_mode)
    {
        apply_high_contrast_environment(config);
    }

    return true;
}
