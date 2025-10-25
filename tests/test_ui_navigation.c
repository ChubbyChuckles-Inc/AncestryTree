#include "test_framework.h"
#include "ui_navigation.h"

#include <string.h>

TEST(test_ui_navigation_tab_forward_cycle)
{
    UINavigationState state;
    ui_navigation_init(&state);

    state.active           = true;
    state.current          = 1;
    state.count_last_frame = 3U;

    UINavigationInput input;
    memset(&input, 0, sizeof(input));
    input.tab_pressed    = true;
    input.tab_with_shift = false;

    ui_navigation_begin_frame(&state, &input);
    (void)ui_navigation_register_item(&state);
    (void)ui_navigation_register_item(&state);
    (void)ui_navigation_register_item(&state);
    ui_navigation_end_frame(&state);

    ASSERT_EQ(state.current, 2);
    ASSERT_TRUE(ui_navigation_is_focused(&state, 2));
}

TEST(test_ui_navigation_tab_reverse_wraps)
{
    UINavigationState state;
    ui_navigation_init(&state);

    state.active           = false;
    state.count_last_frame = 4U;
    state.current          = -1;

    UINavigationInput input;
    memset(&input, 0, sizeof(input));
    input.tab_pressed    = true;
    input.tab_with_shift = true;

    ui_navigation_begin_frame(&state, &input);
    (void)ui_navigation_register_item(&state);
    (void)ui_navigation_register_item(&state);
    (void)ui_navigation_register_item(&state);
    (void)ui_navigation_register_item(&state);
    ui_navigation_end_frame(&state);

    ASSERT_EQ(state.current, 3);
    ASSERT_TRUE(ui_navigation_is_focused(&state, 3));
}

TEST(test_ui_navigation_activation_consumed_once)
{
    UINavigationState state;
    ui_navigation_init(&state);

    state.active           = true;
    state.current          = 0;
    state.count_last_frame = 1U;

    UINavigationInput input;
    memset(&input, 0, sizeof(input));
    input.activate_pressed = true;

    ui_navigation_begin_frame(&state, &input);
    (void)ui_navigation_register_item(&state);
    ui_navigation_end_frame(&state);

    ASSERT_TRUE(ui_navigation_consume_activation(&state));
    ASSERT_FALSE(ui_navigation_consume_activation(&state));
}

TEST(test_ui_navigation_deactivates_without_items)
{
    UINavigationState state;
    ui_navigation_init(&state);

    state.active           = true;
    state.current          = 0;
    state.count_last_frame = 1U;

    UINavigationInput input;
    memset(&input, 0, sizeof(input));

    ui_navigation_begin_frame(&state, &input);
    ui_navigation_end_frame(&state);

    ASSERT_FALSE(state.active);
    ASSERT_EQ(state.current, -1);
}

void register_ui_navigation_tests(TestRegistry *registry)
{
    RUN_TEST(registry, test_ui_navigation_tab_forward_cycle);
    RUN_TEST(registry, test_ui_navigation_tab_reverse_wraps);
    RUN_TEST(registry, test_ui_navigation_activation_consumed_once);
    RUN_TEST(registry, test_ui_navigation_deactivates_without_items);
}
