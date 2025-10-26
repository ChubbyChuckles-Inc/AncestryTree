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
    settings->language                        = SETTINGS_LANGUAGE_ENGLISH;
    settings->high_contrast_mode              = false;
    settings->ui_font_scale                   = 1.0f;
    settings->screen_reader_enabled           = false;
    settings->onboarding_completed            = false;
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
        else if (settings_strcasecmp(key, "onboarding_completed") == 0)
        {
            bool parsed = false;
            if (settings_parse_bool(value, &parsed))
            {
                settings->onboarding_completed = parsed;
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
        "language=%u\n"
        "high_contrast_mode=%u\n"
        "ui_font_scale=%.3f\n"
        "screen_reader_enabled=%u\n"
        "onboarding_completed=%u\n",
        (unsigned int)settings->graphics_quality, settings->camera_rotation_sensitivity,
        settings->camera_pan_sensitivity, settings->camera_keyboard_pan_sensitivity,
        settings->camera_zoom_sensitivity, settings->auto_save_enabled ? 1U : 0U,
        settings->auto_save_interval_seconds, (unsigned int)settings->default_layout_algorithm,
        (unsigned int)settings->color_scheme, (unsigned int)settings->language,
        settings->high_contrast_mode ? 1U : 0U, settings->ui_font_scale,
        settings->screen_reader_enabled ? 1U : 0U, settings->onboarding_completed ? 1U : 0U);

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
