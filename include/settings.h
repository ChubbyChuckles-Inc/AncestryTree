#ifndef SETTINGS_H
#define SETTINGS_H

#include <stdbool.h>
#include <stddef.h>

typedef enum SettingsGraphicsQuality
{
    SETTINGS_GRAPHICS_QUALITY_PERFORMANCE = 0,
    SETTINGS_GRAPHICS_QUALITY_QUALITY     = 1
} SettingsGraphicsQuality;

typedef enum SettingsLayoutAlgorithm
{
    SETTINGS_LAYOUT_ALGORITHM_HIERARCHICAL   = 0,
    SETTINGS_LAYOUT_ALGORITHM_FORCE_DIRECTED = 1
} SettingsLayoutAlgorithm;

typedef enum SettingsColorScheme
{
    SETTINGS_COLOR_SCHEME_CYAN_GRAPH   = 0,
    SETTINGS_COLOR_SCHEME_SOLAR_ORCHID = 1
} SettingsColorScheme;

typedef enum SettingsLanguage
{
    SETTINGS_LANGUAGE_ENGLISH = 0,
    SETTINGS_LANGUAGE_FUTURE  = 1
} SettingsLanguage;

typedef struct SettingsPanelLayout
{
    bool valid;
    float x;
    float y;
    float width;
    float height;
} SettingsPanelLayout;

typedef struct SettingsWindowPlacement
{
    bool valid;
    int x;
    int y;
    int width;
    int height;
} SettingsWindowPlacement;

typedef struct SettingsCameraState
{
    bool valid;
    float target[3];
    float yaw;
    float pitch;
    float radius;
} SettingsCameraState;

typedef struct Settings
{
    SettingsGraphicsQuality graphics_quality;
    float camera_rotation_sensitivity;
    float camera_pan_sensitivity;
    float camera_keyboard_pan_sensitivity;
    float camera_zoom_sensitivity;
    bool auto_save_enabled;
    unsigned int auto_save_interval_seconds;
    SettingsLayoutAlgorithm default_layout_algorithm;
    SettingsColorScheme color_scheme;
    float name_panel_font_size;
    float name_panel_width_scale;
    float name_panel_height_scale;
    SettingsLanguage language;
    bool high_contrast_mode;
    float ui_font_scale;
    bool screen_reader_enabled;
    bool has_loaded_sample_tree;
    bool onboarding_completed;
    char last_tree_path[512];
    SettingsWindowPlacement window_placement;
    SettingsCameraState camera_state;
    SettingsPanelLayout panel_hud;
    SettingsPanelLayout panel_about;
    SettingsPanelLayout panel_help;
    SettingsPanelLayout panel_search;
    SettingsPanelLayout panel_analytics;
    SettingsPanelLayout panel_add_person;
    SettingsPanelLayout panel_edit_person;
    SettingsPanelLayout panel_settings;
    bool panel_hud_visible;
    bool panel_about_visible;
    bool panel_help_visible;
    bool panel_search_visible;
    bool panel_analytics_visible;
    bool panel_add_person_visible;
    bool panel_settings_visible;
    unsigned int revision;
} Settings;

void settings_init_defaults(Settings *settings);
bool settings_try_load(Settings *settings, const char *path, char *error_buffer,
                       size_t error_buffer_size);
bool settings_save(const Settings *settings, const char *path, char *error_buffer,
                   size_t error_buffer_size);
void settings_mark_dirty(Settings *settings);
unsigned int settings_get_revision(const Settings *settings);

#endif /* SETTINGS_H */
