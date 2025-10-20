#include "test_framework.h"

#include "ui_scaling.h"

DECLARE_TEST(test_ui_scaling_returns_default_for_invalid_height)
{
    float size = ui_scaling_default_font_size(0);
    ASSERT_FLOAT_NEAR(size, 18.0f, 0.01f);
}

DECLARE_TEST(test_ui_scaling_matches_reference_height)
{
    float size = ui_scaling_default_font_size(1080);
    ASSERT_FLOAT_NEAR(size, 20.0f, 0.01f);
}

DECLARE_TEST(test_ui_scaling_caps_low_resolution)
{
    float size = ui_scaling_default_font_size(600);
    ASSERT_TRUE(size >= 16.0f - 0.01f);
}

DECLARE_TEST(test_ui_scaling_caps_high_resolution)
{
    float size = ui_scaling_default_font_size(3000);
    ASSERT_TRUE(size <= 38.0f + 0.01f);
}

void register_ui_scaling_tests(TestRegistry *registry)
{
    REGISTER_TEST(registry, test_ui_scaling_returns_default_for_invalid_height);
    REGISTER_TEST(registry, test_ui_scaling_matches_reference_height);
    REGISTER_TEST(registry, test_ui_scaling_caps_low_resolution);
    REGISTER_TEST(registry, test_ui_scaling_caps_high_resolution);
}
