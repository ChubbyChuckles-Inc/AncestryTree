#include "settings_runtime.h"

#include "settings.h"
#include "test_framework.h"

TEST(test_settings_runtime_camera_scaling_respects_settings)
{
    CameraControllerConfig config;
    camera_controller_config_default(&config);
    CameraController controller;
    ASSERT_TRUE(camera_controller_init(&controller, &config));
    Settings settings;
    settings_init_defaults(&settings);

    ASSERT_TRUE(settings_runtime_apply_camera(&settings, &controller));
    ASSERT_FLOAT_NEAR(controller.config.rotation_speed, 1.5f, 0.0001f);
    ASSERT_FLOAT_NEAR(controller.config.pan_speed, 10.0f, 0.0001f);
    ASSERT_FLOAT_NEAR(controller.config.zoom_speed, 15.0f, 0.0001f);

    settings.camera_rotation_sensitivity = 0.30f;
    settings.camera_pan_sensitivity      = 0.75f;
    settings.camera_zoom_sensitivity     = 1.50f;
    ASSERT_TRUE(settings_runtime_apply_camera(&settings, &controller));
    ASSERT_FLOAT_NEAR(controller.config.rotation_speed, 3.0f, 0.0001f);
    ASSERT_FLOAT_NEAR(controller.config.pan_speed, 15.0f, 0.0001f);
    ASSERT_FLOAT_NEAR(controller.config.zoom_speed, 22.5f, 0.0001f);
}

TEST(test_settings_runtime_render_quality_and_colors)
{
    Settings settings;
    settings_init_defaults(&settings);
    RenderConfig config = render_config_default();

    ASSERT_TRUE(settings_runtime_apply_render(&settings, &config));
    ASSERT_TRUE(config.connection_antialiasing);
    ASSERT_TRUE(config.show_profile_images);
    ASSERT_EQ((unsigned int)config.alive_color.r, 0U);
    ASSERT_EQ((unsigned int)config.connection_color_parent_child.r, 64U);

    settings.graphics_quality = SETTINGS_GRAPHICS_QUALITY_PERFORMANCE;
    settings.color_scheme     = SETTINGS_COLOR_SCHEME_SOLAR_ORCHID;
    ASSERT_TRUE(settings_runtime_apply_render(&settings, &config));
    ASSERT_FALSE(config.connection_antialiasing);
    ASSERT_FALSE(config.show_profile_images);
    ASSERT_EQ((unsigned int)config.alive_color.r, 255U);
    ASSERT_EQ((unsigned int)config.connection_color_parent_child.r, 255U);
}

TEST(test_settings_runtime_high_contrast_overrides_environment)
{
    Settings settings;
    settings_init_defaults(&settings);
    settings.graphics_quality   = SETTINGS_GRAPHICS_QUALITY_PERFORMANCE;
    settings.high_contrast_mode = true;

    RenderConfig config = render_config_default();
    ASSERT_TRUE(settings_runtime_apply_render(&settings, &config));
    ASSERT_TRUE(config.connection_antialiasing);
    ASSERT_FALSE(config.show_background_gradient);
    ASSERT_FALSE(config.enable_fog);
    ASSERT_EQ((unsigned int)config.alive_color.r, 255U);
    ASSERT_EQ((unsigned int)config.deceased_color.g, 204U);
}

TEST(test_settings_runtime_input_sensitivity_clamped)
{
    float orbit        = 0.0f;
    float pan_mouse    = 0.0f;
    float pan_keyboard = 0.0f;
    float zoom         = 0.0f;
    settings_runtime_compute_input_sensitivity(NULL, &orbit, &pan_mouse, &pan_keyboard, &zoom);
    ASSERT_FLOAT_NEAR(orbit, 0.15f, 0.0001f);
    ASSERT_FLOAT_NEAR(pan_mouse, 0.5f, 0.0001f);
    ASSERT_FLOAT_NEAR(pan_keyboard, 1.0f, 0.0001f);
    ASSERT_FLOAT_NEAR(zoom, 1.0f, 0.0001f);

    Settings settings;
    settings_init_defaults(&settings);
    settings.camera_rotation_sensitivity     = 100.0f;
    settings.camera_pan_sensitivity          = 100.0f;
    settings.camera_keyboard_pan_sensitivity = 100.0f;
    settings.camera_zoom_sensitivity         = 0.0f;
    settings_runtime_compute_input_sensitivity(&settings, &orbit, &pan_mouse, &pan_keyboard, &zoom);
    ASSERT_FLOAT_NEAR(orbit, 5.0f, 0.0001f);
    ASSERT_FLOAT_NEAR(pan_mouse, 10.0f, 0.0001f);
    ASSERT_FLOAT_NEAR(pan_keyboard, 10.0f, 0.0001f);
    ASSERT_FLOAT_NEAR(zoom, 1.0f, 0.0001f);
}

void register_settings_runtime_tests(TestRegistry *registry)
{
    REGISTER_TEST(registry, test_settings_runtime_camera_scaling_respects_settings);
    REGISTER_TEST(registry, test_settings_runtime_render_quality_and_colors);
    REGISTER_TEST(registry, test_settings_runtime_high_contrast_overrides_environment);
    REGISTER_TEST(registry, test_settings_runtime_input_sensitivity_clamped);
}
