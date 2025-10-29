#include "settings.h"

#include <ctype.h>
#include <math.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>

static int settings_strcasecmp(const char *a, const char *b)
{
#if defined(_MSC_VER)
    return _stricmp(a, b);
#else
    return strcasecmp(a, b);
#endif
}

static void settings_panel_layout_reset(SettingsPanelLayout *layout)
{
    if (!layout)
    {
        return;
    }
    layout->valid  = false;
    layout->x      = 0.0f;
    layout->y      = 0.0f;
    layout->width  = 0.0f;
    layout->height = 0.0f;
}

static void settings_window_placement_reset(SettingsWindowPlacement *placement)
{
    if (!placement)
    {
        return;
    }
    placement->valid  = false;
    placement->x      = 0;
    placement->y      = 0;
    placement->width  = 0;
    placement->height = 0;
}

static bool settings_parse_window_bounds(const char *value, SettingsWindowPlacement *placement)
{
    if (!value || !placement)
    {
        return false;
    }
    unsigned int valid = 0U;
    int x              = 0;
    int y              = 0;
    int w              = 0;
    int h              = 0;
    int parsed         = sscanf(value, "%u,%d,%d,%d,%d", &valid, &x, &y, &w, &h);
    if (parsed != 5)
    {
        return false;
    }
    if (w <= 0 || h <= 0)
    {
        settings_window_placement_reset(placement);
        return true;
    }
    placement->valid  = valid != 0U;
    placement->x      = x;
    placement->y      = y;
    placement->width  = w;
    placement->height = h;
    if (!placement->valid)
    {
        settings_window_placement_reset(placement);
    }
    return true;
}

static void settings_camera_state_reset(SettingsCameraState *state)
{
    if (!state)
    {
        return;
    }
    state->valid     = false;
    state->target[0] = 0.0f;
    state->target[1] = 0.0f;
    state->target[2] = 0.0f;
    state->yaw       = 0.0f;
    state->pitch     = 0.0f;
    state->radius    = 0.0f;
}

static bool settings_parse_camera_state(const char *value, SettingsCameraState *state)
{
    if (!value || !state)
    {
        return false;
    }
    unsigned int valid = 0U;
    double target_x    = 0.0;
    double target_y    = 0.0;
    double target_z    = 0.0;
    double yaw         = 0.0;
    double pitch       = 0.0;
    double radius      = 0.0;
    int parsed         = sscanf(value, "%u,%lf,%lf,%lf,%lf,%lf,%lf", &valid, &target_x, &target_y,
                                &target_z, &yaw, &pitch, &radius);
    if (parsed != 7)
    {
        return false;
    }
    if (!isfinite(target_x) || !isfinite(target_y) || !isfinite(target_z) || !isfinite(yaw) ||
        !isfinite(pitch) || !isfinite(radius) || radius <= 0.0)
    {
        settings_camera_state_reset(state);
        return true;
    }
    state->valid     = valid != 0U;
    state->target[0] = (float)target_x;
    state->target[1] = (float)target_y;
    state->target[2] = (float)target_z;
    state->yaw       = (float)yaw;
    state->pitch     = (float)pitch;
    state->radius    = (float)radius;
    if (!state->valid)
    {
        settings_camera_state_reset(state);
    }
    return true;
}

static bool settings_parse_panel_layout(const char *value, SettingsPanelLayout *layout)
{
    if (!value || !layout)
    {
        return false;
    }
    unsigned int valid = 0U;
    double x           = 0.0;
    double y           = 0.0;
    double w           = 0.0;
    double h           = 0.0;
    int parsed         = sscanf(value, "%u,%lf,%lf,%lf,%lf", &valid, &x, &y, &w, &h);
    if (parsed != 5)
    {
        return false;
    }
    layout->x      = (float)x;
    layout->y      = (float)y;
    layout->width  = (float)w;
    layout->height = (float)h;
    if (!(w > 0.0) || !(h > 0.0))
    {
        layout->valid  = false;
        layout->x      = 0.0f;
        layout->y      = 0.0f;
        layout->width  = 0.0f;
        layout->height = 0.0f;
        return true;
    }
    layout->valid = valid != 0U;
    return true;
}

static void settings_set_defaults(Settings *settings)
{
    if (!settings)
    {
        return;
    }
    settings->graphics_quality                = SETTINGS_GRAPHICS_QUALITY_QUALITY;
    settings->camera_rotation_sensitivity     = 0.15f;
    settings->camera_pan_sensitivity          = 0.5f;
    settings->camera_keyboard_pan_sensitivity = 1.0f;
    settings->camera_zoom_sensitivity         = 1.0f;
    settings->auto_save_enabled               = true;
    settings->auto_save_interval_seconds      = 120U;
    settings->default_layout_algorithm        = SETTINGS_LAYOUT_ALGORITHM_HIERARCHICAL;
    settings->color_scheme                    = SETTINGS_COLOR_SCHEME_CYAN_GRAPH;
    settings->name_panel_font_size            = 26.0f;
    settings->name_panel_width_scale          = 1.0f;
    settings->name_panel_height_scale         = 1.0f;
    settings->language                        = SETTINGS_LANGUAGE_ENGLISH;
    settings->high_contrast_mode              = false;
    settings->ui_font_scale                   = 1.0f;
    settings->screen_reader_enabled           = false;
    settings->has_loaded_sample_tree          = false;
    settings->onboarding_completed            = false;
#if defined(_MSC_VER)
    (void)strncpy_s(settings->last_tree_path, sizeof(settings->last_tree_path), "", _TRUNCATE);
#else
    settings->last_tree_path[0] = '\0';
#endif
    settings_window_placement_reset(&settings->window_placement);
    settings_camera_state_reset(&settings->camera_state);
    settings_panel_layout_reset(&settings->panel_hud);
    settings_panel_layout_reset(&settings->panel_about);
    settings_panel_layout_reset(&settings->panel_help);
    settings_panel_layout_reset(&settings->panel_search);
    settings_panel_layout_reset(&settings->panel_analytics);
    settings_panel_layout_reset(&settings->panel_add_person);
    settings_panel_layout_reset(&settings->panel_edit_person);
    settings_panel_layout_reset(&settings->panel_settings);
    settings->view_show_grid           = true;
    settings->view_show_connections    = true;
    settings->view_show_overlay        = true;
    settings->view_show_name_panels    = true;
    settings->view_show_profile_images = true;
    settings->view_smooth_connections  = true;
    settings->view_particles_enabled   = true;
    settings->panel_hud_visible        = true;
    settings->panel_about_visible      = false;
    settings->panel_help_visible       = false;
    settings->panel_search_visible     = false;
    settings->panel_analytics_visible  = false;
    settings->panel_add_person_visible = false;
    settings->panel_settings_visible   = false;
}
void settings_init_defaults(Settings *settings)
{
    if (!settings)
    {
        return;
    }
    settings_set_defaults(settings);
    settings->revision = 1U;
}

static bool settings_parse_bool(const char *value, bool *out_result)
{
    if (!value || !out_result)
    {
        return false;
    }
    if (strcmp(value, "1") == 0 || settings_strcasecmp(value, "true") == 0 ||
        settings_strcasecmp(value, "yes") == 0)
    {
        *out_result = true;
        return true;
    }
    if (strcmp(value, "0") == 0 || settings_strcasecmp(value, "false") == 0 ||
        settings_strcasecmp(value, "no") == 0)
    {
        *out_result = false;
        return true;
    }
    return false;
}

static bool settings_parse_float(const char *value, float *out_result)
{
    if (!value || !out_result)
    {
        return false;
    }
    char *end    = NULL;
    float parsed = (float)strtod(value, &end);
    if (end == value || !isfinite(parsed))
    {
        return false;
    }
    *out_result = parsed;
    return true;
}

static bool settings_parse_uint(const char *value, unsigned int *out_result)
{
    if (!value || !out_result)
    {
        return false;
    }
    char *end   = NULL;
    long parsed = strtol(value, &end, 10);
    if (end == value || parsed < 0L)
    {
        return false;
    }
    *out_result = (unsigned int)parsed;
    return true;
}

static void settings_trim(char *text)
{
    if (!text)
    {
        return;
    }
    size_t length = strlen(text);
    while (length > 0U && (text[length - 1U] == '\n' || text[length - 1U] == '\r' ||
                           text[length - 1U] == ' ' || text[length - 1U] == '\t'))
    {
        text[length - 1U] = '\0';
        --length;
    }
    char *cursor = text;
    while (*cursor == ' ' || *cursor == '\t')
    {
        ++cursor;
    }
    if (cursor != text)
    {
        memmove(text, cursor, strlen(cursor) + 1U);
    }
}

bool settings_try_load(Settings *settings, const char *path, char *error_buffer,
                       size_t error_buffer_size)
{
    if (!settings || !path)
    {
        return false;
    }
#if defined(_MSC_VER)
    FILE *stream = NULL;
    if (fopen_s(&stream, path, "rb") != 0)
    {
        return false;
    }
#else
    FILE *stream = fopen(path, "rb");
    if (!stream)
    {
        return false;
    }
#endif

    settings_set_defaults(settings);

    char line[256];
    while (fgets(line, sizeof(line), stream))
    {
        settings_trim(line);
        if (line[0] == '\0' || line[0] == '#')
        {
            continue;
        }
        char *equals = strchr(line, '=');
        if (!equals)
        {
            continue;
        }
        *equals     = '\0';
        char *key   = line;
        char *value = equals + 1;
        settings_trim(key);
        settings_trim(value);
        if (settings_strcasecmp(key, "graphics_quality") == 0)
        {
            unsigned int parsed = 0U;
            if (settings_parse_uint(value, &parsed) &&
                parsed <= (unsigned int)SETTINGS_GRAPHICS_QUALITY_QUALITY)
            {
                settings->graphics_quality = (SettingsGraphicsQuality)parsed;
            }
        }
        else if (settings_strcasecmp(key, "camera_rotation_sensitivity") == 0)
        {
            float parsed = 0.0f;
            if (settings_parse_float(value, &parsed) && parsed > 0.0f)
            {
                settings->camera_rotation_sensitivity = parsed;
            }
        }
        else if (settings_strcasecmp(key, "camera_pan_sensitivity") == 0)
        {
            float parsed = 0.0f;
            if (settings_parse_float(value, &parsed) && parsed > 0.0f)
            {
                settings->camera_pan_sensitivity = parsed;
            }
        }
        else if (settings_strcasecmp(key, "camera_keyboard_pan_sensitivity") == 0)
        {
            float parsed = 0.0f;
            if (settings_parse_float(value, &parsed) && parsed > 0.0f)
            {
                settings->camera_keyboard_pan_sensitivity = parsed;
            }
        }
        else if (settings_strcasecmp(key, "camera_zoom_sensitivity") == 0)
        {
            float parsed = 0.0f;
            if (settings_parse_float(value, &parsed) && parsed > 0.0f)
            {
                settings->camera_zoom_sensitivity = parsed;
            }
        }
        else if (settings_strcasecmp(key, "auto_save_enabled") == 0)
        {
            bool parsed = false;
            if (settings_parse_bool(value, &parsed))
            {
                settings->auto_save_enabled = parsed;
            }
        }
        else if (settings_strcasecmp(key, "auto_save_interval_seconds") == 0)
        {
            unsigned int parsed = 0U;
            if (settings_parse_uint(value, &parsed) && parsed > 0U)
            {
                settings->auto_save_interval_seconds = parsed;
            }
        }
        else if (settings_strcasecmp(key, "default_layout_algorithm") == 0)
        {
            unsigned int parsed = 0U;
            if (settings_parse_uint(value, &parsed) &&
                parsed <= (unsigned int)SETTINGS_LAYOUT_ALGORITHM_FORCE_DIRECTED)
            {
                settings->default_layout_algorithm = (SettingsLayoutAlgorithm)parsed;
            }
        }
        else if (settings_strcasecmp(key, "color_scheme") == 0)
        {
            unsigned int parsed = 0U;
            if (settings_parse_uint(value, &parsed) &&
                parsed <= (unsigned int)SETTINGS_COLOR_SCHEME_SOLAR_ORCHID)
            {
                settings->color_scheme = (SettingsColorScheme)parsed;
            }
        }
        else if (settings_strcasecmp(key, "name_panel_font_size") == 0)
        {
            float parsed = 0.0f;
            if (settings_parse_float(value, &parsed) && parsed > 0.0f)
            {
                if (parsed < 16.0f)
                {
                    parsed = 16.0f;
                }
                if (parsed > 72.0f)
                {
                    parsed = 72.0f;
                }
                settings->name_panel_font_size = parsed;
            }
        }
        else if (settings_strcasecmp(key, "name_panel_width_scale") == 0)
        {
            float parsed = 0.0f;
            if (settings_parse_float(value, &parsed) && parsed > 0.0f)
            {
                settings->name_panel_width_scale = parsed;
            }
        }
        else if (settings_strcasecmp(key, "name_panel_height_scale") == 0)
        {
            float parsed = 0.0f;
            if (settings_parse_float(value, &parsed) && parsed > 0.0f)
            {
                settings->name_panel_height_scale = parsed;
            }
        }
        else if (settings_strcasecmp(key, "view_show_grid") == 0)
        {
            bool parsed = false;
            if (settings_parse_bool(value, &parsed))
            {
                settings->view_show_grid = parsed;
            }
        }
        else if (settings_strcasecmp(key, "view_show_connections") == 0)
        {
            bool parsed = false;
            if (settings_parse_bool(value, &parsed))
            {
                settings->view_show_connections = parsed;
            }
        }
        else if (settings_strcasecmp(key, "view_show_overlay") == 0)
        {
            bool parsed = false;
            if (settings_parse_bool(value, &parsed))
            {
                settings->view_show_overlay = parsed;
            }
        }
        else if (settings_strcasecmp(key, "view_show_name_panels") == 0)
        {
            bool parsed = false;
            if (settings_parse_bool(value, &parsed))
            {
                settings->view_show_name_panels = parsed;
            }
        }
        else if (settings_strcasecmp(key, "view_show_profile_images") == 0)
        {
            bool parsed = false;
            if (settings_parse_bool(value, &parsed))
            {
                settings->view_show_profile_images = parsed;
            }
        }
        else if (settings_strcasecmp(key, "view_smooth_connections") == 0)
        {
            bool parsed = false;
            if (settings_parse_bool(value, &parsed))
            {
                settings->view_smooth_connections = parsed;
            }
        }
        else if (settings_strcasecmp(key, "view_particles_enabled") == 0)
        {
            bool parsed = false;
            if (settings_parse_bool(value, &parsed))
            {
                settings->view_particles_enabled = parsed;
            }
        }
        else if (settings_strcasecmp(key, "language") == 0)
        {
            unsigned int parsed = 0U;
            if (settings_parse_uint(value, &parsed) &&
                parsed <= (unsigned int)SETTINGS_LANGUAGE_FUTURE)
            {
                settings->language = (SettingsLanguage)parsed;
            }
        }
        else if (settings_strcasecmp(key, "high_contrast_mode") == 0)
        {
            bool parsed = false;
            if (settings_parse_bool(value, &parsed))
            {
                settings->high_contrast_mode = parsed;
            }
        }
        else if (settings_strcasecmp(key, "ui_font_scale") == 0)
        {
            float parsed = 0.0f;
            if (settings_parse_float(value, &parsed) && parsed > 0.1f)
            {
                if (parsed < 0.6f)
                {
                    parsed = 0.6f;
                }
                if (parsed > 1.8f)
                {
                    parsed = 1.8f;
                }
                settings->ui_font_scale = parsed;
            }
        }
        else if (settings_strcasecmp(key, "screen_reader_enabled") == 0)
        {
            bool parsed = false;
            if (settings_parse_bool(value, &parsed))
            {
                settings->screen_reader_enabled = parsed;
            }
        }
        else if (settings_strcasecmp(key, "has_loaded_sample_tree") == 0)
        {
            bool parsed = false;
            if (settings_parse_bool(value, &parsed))
            {
                settings->has_loaded_sample_tree = parsed;
            }
        }
        else if (settings_strcasecmp(key, "onboarding_completed") == 0)
        {
            bool parsed = false;
            if (settings_parse_bool(value, &parsed))
            {
                settings->onboarding_completed = parsed;
            }
        }
        else if (settings_strcasecmp(key, "last_tree_path") == 0)
        {
#if defined(_MSC_VER)
            (void)strncpy_s(settings->last_tree_path, sizeof(settings->last_tree_path), value,
                            _TRUNCATE);
#else
            (void)snprintf(settings->last_tree_path, sizeof(settings->last_tree_path), "%s", value);
#endif
        }
        else if (settings_strcasecmp(key, "window_bounds") == 0)
        {
            (void)settings_parse_window_bounds(value, &settings->window_placement);
        }
        else if (settings_strcasecmp(key, "camera_state") == 0)
        {
            (void)settings_parse_camera_state(value, &settings->camera_state);
        }
        else if (settings_strcasecmp(key, "panel_hud") == 0)
        {
            (void)settings_parse_panel_layout(value, &settings->panel_hud);
        }
        else if (settings_strcasecmp(key, "panel_about") == 0)
        {
            (void)settings_parse_panel_layout(value, &settings->panel_about);
        }
        else if (settings_strcasecmp(key, "panel_help") == 0)
        {
            (void)settings_parse_panel_layout(value, &settings->panel_help);
        }
        else if (settings_strcasecmp(key, "panel_search") == 0)
        {
            (void)settings_parse_panel_layout(value, &settings->panel_search);
        }
        else if (settings_strcasecmp(key, "panel_analytics") == 0)
        {
            (void)settings_parse_panel_layout(value, &settings->panel_analytics);
        }
        else if (settings_strcasecmp(key, "panel_add_person") == 0)
        {
            (void)settings_parse_panel_layout(value, &settings->panel_add_person);
        }
        else if (settings_strcasecmp(key, "panel_edit_person") == 0)
        {
            (void)settings_parse_panel_layout(value, &settings->panel_edit_person);
        }
        else if (settings_strcasecmp(key, "panel_settings") == 0)
        {
            (void)settings_parse_panel_layout(value, &settings->panel_settings);
        }
        else if (settings_strcasecmp(key, "panel_hud_visible") == 0)
        {
            bool parsed = settings->panel_hud_visible;
            if (settings_parse_bool(value, &parsed))
            {
                settings->panel_hud_visible = parsed;
            }
        }
        else if (settings_strcasecmp(key, "panel_about_visible") == 0)
        {
            bool parsed = settings->panel_about_visible;
            if (settings_parse_bool(value, &parsed))
            {
                settings->panel_about_visible = parsed;
            }
        }
        else if (settings_strcasecmp(key, "panel_help_visible") == 0)
        {
            bool parsed = settings->panel_help_visible;
            if (settings_parse_bool(value, &parsed))
            {
                settings->panel_help_visible = parsed;
            }
        }
        else if (settings_strcasecmp(key, "panel_search_visible") == 0)
        {
            bool parsed = settings->panel_search_visible;
            if (settings_parse_bool(value, &parsed))
            {
                settings->panel_search_visible = parsed;
            }
        }
        else if (settings_strcasecmp(key, "panel_analytics_visible") == 0)
        {
            bool parsed = settings->panel_analytics_visible;
            if (settings_parse_bool(value, &parsed))
            {
                settings->panel_analytics_visible = parsed;
            }
        }
        else if (settings_strcasecmp(key, "panel_add_person_visible") == 0)
        {
            bool parsed = settings->panel_add_person_visible;
            if (settings_parse_bool(value, &parsed))
            {
                settings->panel_add_person_visible = parsed;
            }
        }
        else if (settings_strcasecmp(key, "panel_settings_visible") == 0)
        {
            bool parsed = settings->panel_settings_visible;
            if (settings_parse_bool(value, &parsed))
            {
                settings->panel_settings_visible = parsed;
            }
        }
    }

    fclose(stream);
    settings->revision += 1U;
    return true;
}

bool settings_save(const Settings *settings, const char *path, char *error_buffer,
                   size_t error_buffer_size)
{
    if (!settings || !path)
    {
        if (error_buffer && error_buffer_size > 0U)
        {
            (void)snprintf(error_buffer, error_buffer_size, "settings or path is NULL");
        }
        return false;
    }
#if defined(_MSC_VER)
    FILE *stream = NULL;
    if (fopen_s(&stream, path, "wb") != 0)
    {
        if (error_buffer && error_buffer_size > 0U)
        {
            (void)snprintf(error_buffer, error_buffer_size, "failed to open %s for writing", path);
        }
        return false;
    }
#else
    FILE *stream = fopen(path, "wb");
    if (!stream)
    {
        if (error_buffer && error_buffer_size > 0U)
        {
            (void)snprintf(error_buffer, error_buffer_size, "failed to open %s for writing", path);
        }
        return false;
    }
#endif

    int written = fprintf(
        stream,
        "graphics_quality=%u\n"
        "camera_rotation_sensitivity=%.4f\n"
        "camera_pan_sensitivity=%.4f\n"
        "camera_keyboard_pan_sensitivity=%.4f\n"
        "camera_zoom_sensitivity=%.4f\n"
        "auto_save_enabled=%u\n"
        "auto_save_interval_seconds=%u\n"
        "default_layout_algorithm=%u\n"
        "color_scheme=%u\n"
        "name_panel_font_size=%.2f\n"
        "name_panel_width_scale=%.3f\n"
        "name_panel_height_scale=%.3f\n"
        "view_show_grid=%u\n"
        "view_show_connections=%u\n"
        "view_show_overlay=%u\n"
        "view_show_name_panels=%u\n"
        "view_show_profile_images=%u\n"
        "view_smooth_connections=%u\n"
        "view_particles_enabled=%u\n"
        "language=%u\n"
        "high_contrast_mode=%u\n"
        "ui_font_scale=%.3f\n"
        "screen_reader_enabled=%u\n"
        "onboarding_completed=%u\n"
        "has_loaded_sample_tree=%u\n"
        "last_tree_path=%s\n"
        "window_bounds=%u,%d,%d,%d,%d\n"
        "camera_state=%u,%.3f,%.3f,%.3f,%.4f,%.4f,%.4f\n"
        "panel_hud=%u,%.2f,%.2f,%.2f,%.2f\n"
        "panel_about=%u,%.2f,%.2f,%.2f,%.2f\n"
        "panel_help=%u,%.2f,%.2f,%.2f,%.2f\n"
        "panel_search=%u,%.2f,%.2f,%.2f,%.2f\n"
        "panel_analytics=%u,%.2f,%.2f,%.2f,%.2f\n"
        "panel_add_person=%u,%.2f,%.2f,%.2f,%.2f\n"
        "panel_edit_person=%u,%.2f,%.2f,%.2f,%.2f\n"
        "panel_settings=%u,%.2f,%.2f,%.2f,%.2f\n"
        "panel_hud_visible=%u\n"
        "panel_about_visible=%u\n"
        "panel_help_visible=%u\n"
        "panel_search_visible=%u\n"
        "panel_analytics_visible=%u\n"
        "panel_add_person_visible=%u\n"
        "panel_settings_visible=%u\n",
        (unsigned int)settings->graphics_quality, settings->camera_rotation_sensitivity,
        settings->camera_pan_sensitivity, settings->camera_keyboard_pan_sensitivity,
        settings->camera_zoom_sensitivity, settings->auto_save_enabled ? 1U : 0U,
        settings->auto_save_interval_seconds, (unsigned int)settings->default_layout_algorithm,
        (unsigned int)settings->color_scheme, settings->name_panel_font_size,
        settings->name_panel_width_scale, settings->name_panel_height_scale,
        settings->view_show_grid ? 1U : 0U, settings->view_show_connections ? 1U : 0U,
        settings->view_show_overlay ? 1U : 0U, settings->view_show_name_panels ? 1U : 0U,
        settings->view_show_profile_images ? 1U : 0U, settings->view_smooth_connections ? 1U : 0U,
        settings->view_particles_enabled ? 1U : 0U, (unsigned int)settings->language,
        settings->high_contrast_mode ? 1U : 0U, settings->ui_font_scale,
        settings->screen_reader_enabled ? 1U : 0U, settings->onboarding_completed ? 1U : 0U,
        settings->has_loaded_sample_tree ? 1U : 0U, settings->last_tree_path,
        settings->window_placement.valid ? 1U : 0U, settings->window_placement.x,
        settings->window_placement.y, settings->window_placement.width,
        settings->window_placement.height, settings->camera_state.valid ? 1U : 0U,
        (double)settings->camera_state.target[0], (double)settings->camera_state.target[1],
        (double)settings->camera_state.target[2], (double)settings->camera_state.yaw,
        (double)settings->camera_state.pitch, (double)settings->camera_state.radius,
        settings->panel_hud.valid ? 1U : 0U, (double)settings->panel_hud.x,
        (double)settings->panel_hud.y, (double)settings->panel_hud.width,
        (double)settings->panel_hud.height, settings->panel_about.valid ? 1U : 0U,
        (double)settings->panel_about.x, (double)settings->panel_about.y,
        (double)settings->panel_about.width, (double)settings->panel_about.height,
        settings->panel_help.valid ? 1U : 0U, (double)settings->panel_help.x,
        (double)settings->panel_help.y, (double)settings->panel_help.width,
        (double)settings->panel_help.height, settings->panel_search.valid ? 1U : 0U,
        (double)settings->panel_search.x, (double)settings->panel_search.y,
        (double)settings->panel_search.width, (double)settings->panel_search.height,
        settings->panel_analytics.valid ? 1U : 0U, (double)settings->panel_analytics.x,
        (double)settings->panel_analytics.y, (double)settings->panel_analytics.width,
        (double)settings->panel_analytics.height, settings->panel_add_person.valid ? 1U : 0U,
        (double)settings->panel_add_person.x, (double)settings->panel_add_person.y,
        (double)settings->panel_add_person.width, (double)settings->panel_add_person.height,
        settings->panel_edit_person.valid ? 1U : 0U, (double)settings->panel_edit_person.x,
        (double)settings->panel_edit_person.y, (double)settings->panel_edit_person.width,
        (double)settings->panel_edit_person.height, settings->panel_settings.valid ? 1U : 0U,
        (double)settings->panel_settings.x, (double)settings->panel_settings.y,
        (double)settings->panel_settings.width, (double)settings->panel_settings.height,
        settings->panel_hud_visible ? 1U : 0U, settings->panel_about_visible ? 1U : 0U,
        settings->panel_help_visible ? 1U : 0U, settings->panel_search_visible ? 1U : 0U,
        settings->panel_analytics_visible ? 1U : 0U, settings->panel_add_person_visible ? 1U : 0U,
        settings->panel_settings_visible ? 1U : 0U);

    bool success = written >= 0;
    if (!success && error_buffer && error_buffer_size > 0U)
    {
        (void)snprintf(error_buffer, error_buffer_size, "failed to write settings to %s", path);
    }
    fclose(stream);
    return success;
}

void settings_mark_dirty(Settings *settings)
{
    if (!settings)
    {
        return;
    }
    settings->revision += 1U;
    if (settings->revision == 0U)
    {
        settings->revision = 1U;
    }
}

unsigned int settings_get_revision(const Settings *settings)
{
    return settings ? settings->revision : 0U;
}
