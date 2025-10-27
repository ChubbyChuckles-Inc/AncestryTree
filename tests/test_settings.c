#include "settings.h"
#include "test_framework.h"
#include "test_persistence_helpers.h"

#include <stdio.h>

static Settings settings_make_defaults(void)
{
    Settings settings;
    settings_init_defaults(&settings);
    return settings;
}

TEST(test_settings_defaults_are_expected)
{
    Settings settings = settings_make_defaults();
    ASSERT_EQ(settings.graphics_quality, SETTINGS_GRAPHICS_QUALITY_QUALITY);
    ASSERT_TRUE(settings.auto_save_enabled);
    ASSERT_EQ(settings.auto_save_interval_seconds, 120U);
    ASSERT_EQ(settings.default_layout_algorithm, SETTINGS_LAYOUT_ALGORITHM_HIERARCHICAL);
    ASSERT_EQ(settings.color_scheme, SETTINGS_COLOR_SCHEME_CYAN_GRAPH);
    ASSERT_EQ(settings.language, SETTINGS_LANGUAGE_ENGLISH);
    ASSERT_FALSE(settings.high_contrast_mode);
    ASSERT_FLOAT_NEAR(settings.ui_font_scale, 1.0f, 0.0001f);
    ASSERT_FALSE(settings.screen_reader_enabled);
    ASSERT_FALSE(settings.has_loaded_sample_tree);
}

TEST(test_settings_mark_dirty_increments_revision)
{
    Settings settings             = settings_make_defaults();
    unsigned int initial_revision = settings_get_revision(&settings);
    settings_mark_dirty(&settings);
    ASSERT_TRUE(settings_get_revision(&settings) > initial_revision);
}

TEST(test_settings_save_and_load_round_trip)
{
    Settings settings                        = settings_make_defaults();
    settings.graphics_quality                = SETTINGS_GRAPHICS_QUALITY_PERFORMANCE;
    settings.camera_rotation_sensitivity     = 0.25f;
    settings.camera_pan_sensitivity          = 0.8f;
    settings.camera_keyboard_pan_sensitivity = 1.5f;
    settings.camera_zoom_sensitivity         = 1.2f;
    settings.auto_save_enabled               = false;
    settings.auto_save_interval_seconds      = 45U;
    settings.default_layout_algorithm        = SETTINGS_LAYOUT_ALGORITHM_FORCE_DIRECTED;
    settings.color_scheme                    = SETTINGS_COLOR_SCHEME_SOLAR_ORCHID;
    settings.language                        = SETTINGS_LANGUAGE_FUTURE;
    settings.high_contrast_mode              = true;
    settings.ui_font_scale                   = 1.35f;
    settings.screen_reader_enabled           = true;
    settings.has_loaded_sample_tree          = true;
    settings_mark_dirty(&settings);

    char path[128];
    test_temp_file_path(path, sizeof(path), "settings.cfg");
    test_delete_file(path);

    char error[128];
    ASSERT_TRUE(settings_save(&settings, path, error, sizeof(error)));

    Settings loaded;
    settings_init_defaults(&loaded);
    ASSERT_TRUE(settings_try_load(&loaded, path, error, sizeof(error)));
    ASSERT_EQ(loaded.graphics_quality, SETTINGS_GRAPHICS_QUALITY_PERFORMANCE);
    ASSERT_FLOAT_NEAR(loaded.camera_rotation_sensitivity, 0.25f, 0.0001f);
    ASSERT_FLOAT_NEAR(loaded.camera_pan_sensitivity, 0.8f, 0.0001f);
    ASSERT_FLOAT_NEAR(loaded.camera_keyboard_pan_sensitivity, 1.5f, 0.0001f);
    ASSERT_FALSE(loaded.auto_save_enabled);
    ASSERT_EQ(loaded.auto_save_interval_seconds, 45U);
    ASSERT_EQ(loaded.default_layout_algorithm, SETTINGS_LAYOUT_ALGORITHM_FORCE_DIRECTED);
    ASSERT_EQ(loaded.color_scheme, SETTINGS_COLOR_SCHEME_SOLAR_ORCHID);
    ASSERT_EQ(loaded.language, SETTINGS_LANGUAGE_FUTURE);
    ASSERT_TRUE(loaded.high_contrast_mode);
    ASSERT_FLOAT_NEAR(loaded.ui_font_scale, 1.35f, 0.0001f);
    ASSERT_TRUE(loaded.screen_reader_enabled);
    ASSERT_TRUE(loaded.has_loaded_sample_tree);

    test_delete_file(path);
}

void register_settings_tests(TestRegistry *registry)
{
    REGISTER_TEST(registry, test_settings_defaults_are_expected);
    REGISTER_TEST(registry, test_settings_mark_dirty_increments_revision);
    REGISTER_TEST(registry, test_settings_save_and_load_round_trip);
}
