#include "test_framework.h"
#include "ui_theme.h"

#include <string.h>

TEST(ui_animated_panel_reset_defaults_visible_false)
{
    UIAnimatedPanel panel;
    memset(&panel, 0, sizeof(panel));
    ui_animated_panel_reset(&panel, false);
    ASSERT_FLOAT_NEAR(panel.progress, 0.0f, 0.0001f);
#if defined(ANCESTRYTREE_HAVE_RAYLIB) && defined(ANCESTRYTREE_HAVE_NUKLEAR)
    ASSERT_FLOAT_NEAR(panel.show_speed, 6.0f, 0.0001f);
    ASSERT_FLOAT_NEAR(panel.hide_speed, 5.0f, 0.0001f);
    ASSERT_FLOAT_NEAR(panel.min_alpha, 0.12f, 0.0001f);
#else
    ASSERT_FLOAT_NEAR(panel.show_speed, 4.0f, 0.0001f);
    ASSERT_FLOAT_NEAR(panel.hide_speed, 3.0f, 0.0001f);
    ASSERT_FLOAT_NEAR(panel.min_alpha, 0.1f, 0.0001f);
#endif
}

TEST(ui_animated_panel_reset_defaults_visible_true)
{
    UIAnimatedPanel panel;
    memset(&panel, 0, sizeof(panel));
    ui_animated_panel_reset(&panel, true);
    ASSERT_FLOAT_NEAR(panel.progress, 1.0f, 0.0001f);
}

TEST(ui_animated_panel_update_clamps_and_transitions)
{
    UIAnimatedPanel panel;
    ui_animated_panel_reset(&panel, false);
    ui_animated_panel_update(&panel, true, 1.0f);
    ASSERT_TRUE(panel.progress <= 1.0f);
    ASSERT_TRUE(panel.progress > 0.0f);

    ui_animated_panel_update(&panel, false, 10.0f);
    ASSERT_FLOAT_NEAR(panel.progress, 0.0f, 0.0001f);
}

TEST(ui_animated_panel_visibility_and_alpha)
{
    UIAnimatedPanel panel;
    ui_animated_panel_reset(&panel, false);
    ASSERT_FALSE(ui_animated_panel_visible(&panel));
    ASSERT_FLOAT_NEAR(ui_animated_panel_alpha(&panel), 0.0f, 0.0001f);

    ui_animated_panel_update(&panel, true, 0.05f);
    ASSERT_TRUE(ui_animated_panel_visible(&panel));
    float alpha = ui_animated_panel_alpha(&panel);
#if defined(ANCESTRYTREE_HAVE_RAYLIB) && defined(ANCESTRYTREE_HAVE_NUKLEAR)
    ASSERT_TRUE(alpha >= 0.12f);
#else
    ASSERT_TRUE(alpha >= 0.1f);
#endif
    ASSERT_TRUE(alpha <= 1.0f);
}

void register_ui_theme_tests(TestRegistry *registry)
{
    RUN_TEST(registry, ui_animated_panel_reset_defaults_visible_false);
    RUN_TEST(registry, ui_animated_panel_reset_defaults_visible_true);
    RUN_TEST(registry, ui_animated_panel_update_clamps_and_transitions);
    RUN_TEST(registry, ui_animated_panel_visibility_and_alpha);
}
