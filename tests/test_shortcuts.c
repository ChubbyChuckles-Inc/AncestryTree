#include "shortcuts.h"
#include "test_framework.h"

static ShortcutResult shortcuts_eval(const ShortcutState *state)
{
    ShortcutResult result;
    shortcuts_evaluate(state, &result);
    return result;
}

TEST(test_shortcuts_ctrl_n_produces_new_event)
{
    ShortcutState state = {0};
    state.ctrl_down = true;
    state.key_new_pressed = true;
    ShortcutResult result = shortcuts_eval(&state);
    ASSERT_EQ(result.event, UI_EVENT_NEW_TREE);
}

TEST(test_shortcuts_ctrl_o_produces_open_event)
{
    ShortcutState state = {0};
    state.ctrl_down = true;
    state.key_open_pressed = true;
    ShortcutResult result = shortcuts_eval(&state);
    ASSERT_EQ(result.event, UI_EVENT_OPEN_TREE);
}

TEST(test_shortcuts_ctrl_s_prioritises_save_as_when_shift_held)
{
    ShortcutState state = {0};
    state.ctrl_down = true;
    state.shift_down = true;
    state.key_save_pressed = true;
    ShortcutResult result = shortcuts_eval(&state);
    ASSERT_EQ(result.event, UI_EVENT_SAVE_TREE_AS);

    state.shift_down = false;
    result = shortcuts_eval(&state);
    ASSERT_EQ(result.event, UI_EVENT_SAVE_TREE);
}

TEST(test_shortcuts_space_triggers_camera_reset)
{
    ShortcutState state = {0};
    state.key_space_pressed = true;
    ShortcutResult result = shortcuts_eval(&state);
    ASSERT_EQ(result.event, UI_EVENT_RESET_CAMERA);
}

TEST(test_shortcuts_escape_triggers_escape_event)
{
    ShortcutState state = {0};
    state.key_escape_pressed = true;
    ShortcutResult result = shortcuts_eval(&state);
    ASSERT_EQ(result.event, UI_EVENT_ESCAPE);
}

TEST(test_shortcuts_ctrl_z_and_ctrl_y_map_to_undo_redo)
{
    ShortcutState state = {0};
    state.ctrl_down = true;
    state.key_undo_pressed = true;
    ShortcutResult result = shortcuts_eval(&state);
    ASSERT_EQ(result.event, UI_EVENT_UNDO);

    state.key_undo_pressed = false;
    state.key_redo_pressed = true;
    result = shortcuts_eval(&state);
    ASSERT_EQ(result.event, UI_EVENT_REDO);
}

TEST(test_shortcuts_no_modifier_returns_none)
{
    ShortcutState state = {0};
    ShortcutResult result = shortcuts_eval(&state);
    ASSERT_EQ(result.event, UI_EVENT_NONE);

    state.shift_down = true;
    state.key_save_pressed = true;
    result = shortcuts_eval(&state);
    ASSERT_EQ(result.event, UI_EVENT_NONE);
}

void register_shortcuts_tests(TestRegistry *registry)
{
    REGISTER_TEST(registry, test_shortcuts_ctrl_n_produces_new_event);
    REGISTER_TEST(registry, test_shortcuts_ctrl_o_produces_open_event);
    REGISTER_TEST(registry, test_shortcuts_ctrl_s_prioritises_save_as_when_shift_held);
    REGISTER_TEST(registry, test_shortcuts_space_triggers_camera_reset);
    REGISTER_TEST(registry, test_shortcuts_escape_triggers_escape_event);
    REGISTER_TEST(registry, test_shortcuts_ctrl_z_and_ctrl_y_map_to_undo_redo);
    REGISTER_TEST(registry, test_shortcuts_no_modifier_returns_none);
}
