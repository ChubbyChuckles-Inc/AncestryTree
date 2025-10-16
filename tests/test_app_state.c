#include "app.h"
#include "camera_controller.h"
#include "interaction.h"
#include "layout.h"
#include "settings.h"
#include "test_framework.h"
#include "tree.h"

#include <stdlib.h>
#include <string.h>

typedef struct MockCommand
{
    AppCommand base;
    int *value;
    int delta;
    bool fail_execute;
    bool fail_undo;
    bool executed;
} MockCommand;

static bool mock_command_execute(AppCommand *command, AppState *state)
{
    (void)state;
    MockCommand *mock = (MockCommand *)command;
    if (mock->fail_execute)
    {
        return false;
    }
    if (!mock->value)
    {
        return false;
    }
    *mock->value += mock->delta;
    mock->executed = true;
    return true;
}

static bool mock_command_undo(AppCommand *command, AppState *state)
{
    (void)state;
    MockCommand *mock = (MockCommand *)command;
    if (mock->fail_undo)
    {
        return false;
    }
    if (!mock->value)
    {
        return false;
    }
    *mock->value -= mock->delta;
    mock->executed = false;
    return true;
}

static void mock_command_destroy(AppCommand *command)
{
    free(command);
}

static const AppCommandVTable MOCK_COMMAND_VTABLE = {
    mock_command_execute,
    mock_command_undo,
    mock_command_destroy,
};

static AppCommand *mock_command_create(int *value, int delta, bool fail_execute, bool fail_undo)
{
    MockCommand *mock = (MockCommand *)malloc(sizeof(MockCommand));
    if (!mock)
    {
        return NULL;
    }
    mock->base.vtable = &MOCK_COMMAND_VTABLE;
    mock->value = value;
    mock->delta = delta;
    mock->fail_execute = fail_execute;
    mock->fail_undo = fail_undo;
    mock->executed = false;
    return (AppCommand *)mock;
}

static void app_state_test_context_init(AppState *state, FamilyTree **tree, LayoutResult *layout,
                                        InteractionState *interaction, CameraController *camera, Settings *settings,
                                        Settings *persisted_settings)
{
    *tree = family_tree_create("Test Family");
    ASSERT_NOT_NULL(*tree);
    *layout = layout_calculate(*tree);
    interaction_state_init(interaction);
    interaction_state_set_pick_radius(interaction, 1.0f);
    memset(settings, 0, sizeof(Settings));
    settings_init_defaults(settings);
    *persisted_settings = *settings;
    CameraControllerConfig camera_config;
    camera_controller_config_default(&camera_config);
    camera_controller_init(camera, &camera_config);
    app_state_init(state);
    ASSERT_TRUE(app_state_configure(state, tree, layout, interaction, camera, settings, persisted_settings));
}

static void app_state_test_context_shutdown(AppState *state, LayoutResult *layout, FamilyTree *tree)
{
    app_state_shutdown(state);
    layout_result_destroy(layout);
    family_tree_destroy(tree);
}

DECLARE_TEST(test_app_state_push_undo_redo)
{
    AppState state;
    FamilyTree *tree = NULL;
    LayoutResult layout;
    InteractionState interaction;
    CameraController camera;
    Settings settings;
    Settings persisted_settings;

    app_state_test_context_init(&state, &tree, &layout, &interaction, &camera, &settings, &persisted_settings);

    int value = 0;
    AppCommand *command = mock_command_create(&value, 4, false, false);
    ASSERT_NOT_NULL(command);

    char error_buffer[128];
    ASSERT_TRUE(app_state_push_command(&state, command, error_buffer, sizeof(error_buffer)));
    ASSERT_EQ(value, 4);
    ASSERT_TRUE(app_state_is_tree_dirty(&state));

    ASSERT_TRUE(app_state_undo(&state, error_buffer, sizeof(error_buffer)));
    ASSERT_EQ(value, 0);
    ASSERT_FALSE(app_state_is_tree_dirty(&state));

    ASSERT_TRUE(app_state_redo(&state, error_buffer, sizeof(error_buffer)));
    ASSERT_EQ(value, 4);
    ASSERT_TRUE(app_state_is_tree_dirty(&state));

    app_state_test_context_shutdown(&state, &layout, tree);
}

DECLARE_TEST(test_app_state_push_failure_destroys_command)
{
    AppState state;
    FamilyTree *tree = NULL;
    LayoutResult layout;
    InteractionState interaction;
    CameraController camera;
    Settings settings;
    Settings persisted_settings;

    app_state_test_context_init(&state, &tree, &layout, &interaction, &camera, &settings, &persisted_settings);

    int value = 0;
    AppCommand *command = mock_command_create(&value, 7, true, false);
    ASSERT_NOT_NULL(command);

    char error_buffer[128];
    ASSERT_FALSE(app_state_push_command(&state, command, error_buffer, sizeof(error_buffer)));
    ASSERT_EQ(value, 0);
    ASSERT_FALSE(app_state_is_tree_dirty(&state));

    app_state_test_context_shutdown(&state, &layout, tree);
}

DECLARE_TEST(test_app_state_reset_history_clears_dirty_flag)
{
    AppState state;
    FamilyTree *tree = NULL;
    LayoutResult layout;
    InteractionState interaction;
    CameraController camera;
    Settings settings;
    Settings persisted_settings;

    app_state_test_context_init(&state, &tree, &layout, &interaction, &camera, &settings, &persisted_settings);

    int value = 0;
    AppCommand *command = mock_command_create(&value, 3, false, false);
    ASSERT_NOT_NULL(command);

    char error_buffer[128];
    ASSERT_TRUE(app_state_push_command(&state, command, error_buffer, sizeof(error_buffer)));
    ASSERT_TRUE(app_state_is_tree_dirty(&state));

    app_state_reset_history(&state);
    ASSERT_FALSE(app_state_is_tree_dirty(&state));
    ASSERT_TRUE(app_state_is_history_empty(&state));

    app_state_test_context_shutdown(&state, &layout, tree);
}

void register_app_state_tests(TestRegistry *registry)
{
    REGISTER_TEST(registry, test_app_state_push_undo_redo);
    REGISTER_TEST(registry, test_app_state_push_failure_destroys_command);
    REGISTER_TEST(registry, test_app_state_reset_history_clears_dirty_flag);
}
