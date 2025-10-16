#include "app.h"
#include "camera_controller.h"
#include "interaction.h"
#include "layout.h"
#include "settings.h"
#include "test_framework.h"
#include "at_memory.h"
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
    AT_FREE(command);
}

static const AppCommandVTable MOCK_COMMAND_VTABLE = {
    mock_command_execute,
    mock_command_undo,
    mock_command_destroy,
};

static AppCommand *mock_command_create(int *value, int delta, bool fail_execute, bool fail_undo)
{
    MockCommand *mock = (MockCommand *)AT_MALLOC(sizeof(MockCommand));
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

static Person *app_state_test_create_person(uint32_t id, const char *first, const char *last,
                                            const char *birth_date, const char *birth_location)
{
    Person *person = person_create(id);
    if (!person)
    {
        return NULL;
    }
    if (!person_set_name(person, first, NULL, last))
    {
        person_destroy(person);
        return NULL;
    }
    if (!person_set_birth(person, birth_date, birth_location))
    {
        person_destroy(person);
        return NULL;
    }
    return person;
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

DECLARE_TEST(test_app_state_layout_algorithm_switch_triggers_transition)
{
    AppState state;
    FamilyTree *tree = NULL;
    LayoutResult layout;
    InteractionState interaction;
    CameraController camera;
    Settings settings;
    Settings persisted_settings;

    app_state_test_context_init(&state, &tree, &layout, &interaction, &camera, &settings, &persisted_settings);

    settings.default_layout_algorithm = SETTINGS_LAYOUT_ALGORITHM_FORCE_DIRECTED;
    app_state_tick(&state, 0.0f);
    ASSERT_TRUE(state.layout_transition_active);

    /* Advance animation to completion. */
    for (int step = 0; step < 12; ++step)
    {
        app_state_tick(&state, 0.1f);
    }

    ASSERT_FALSE(state.layout_transition_active);
    ASSERT_EQ(state.active_layout_algorithm, LAYOUT_ALGORITHM_FORCE_DIRECTED);
    ASSERT_EQ(layout.count, tree->person_count);

    app_state_test_context_shutdown(&state, &layout, tree);
}

DECLARE_TEST(test_app_command_add_person_roundtrip)
{
    AppState state;
    FamilyTree *tree = NULL;
    LayoutResult layout;
    InteractionState interaction;
    CameraController camera;
    Settings settings;
    Settings persisted_settings;

    app_state_test_context_init(&state, &tree, &layout, &interaction, &camera, &settings, &persisted_settings);

    Person *person = app_state_test_create_person(1001U, "Ada", "Hologram", "1985-06-01", NULL);
    ASSERT_NOT_NULL(person);

    AppCommand *command = app_command_create_add_person(person);
    ASSERT_NOT_NULL(command);

    char error_buffer[128];
    ASSERT_TRUE(app_state_push_command(&state, command, error_buffer, sizeof(error_buffer)));
    ASSERT_NOT_NULL(family_tree_find_person(tree, 1001U));

    ASSERT_TRUE(app_state_undo(&state, error_buffer, sizeof(error_buffer)));
    ASSERT_NULL(family_tree_find_person(tree, 1001U));

    ASSERT_TRUE(app_state_redo(&state, error_buffer, sizeof(error_buffer)));
    ASSERT_NOT_NULL(family_tree_find_person(tree, 1001U));

    app_state_test_context_shutdown(&state, &layout, tree);
}

DECLARE_TEST(test_app_command_delete_person_roundtrip)
{
    AppState state;
    FamilyTree *tree = NULL;
    LayoutResult layout;
    InteractionState interaction;
    CameraController camera;
    Settings settings;
    Settings persisted_settings;

    app_state_test_context_init(&state, &tree, &layout, &interaction, &camera, &settings, &persisted_settings);

    char error_buffer[128];

    Person *parent = app_state_test_create_person(2001U, "Nova", "Prime", "1970-01-01", NULL);
    ASSERT_NOT_NULL(parent);
    ASSERT_TRUE(app_state_add_person(&state, parent, error_buffer, sizeof(error_buffer)));

    Person *child = app_state_test_create_person(2002U, "Echo", "Prime", "1995-04-12", NULL);
    ASSERT_NOT_NULL(child);
    ASSERT_TRUE(person_add_child(parent, child));
    ASSERT_TRUE(app_state_add_person(&state, child, error_buffer, sizeof(error_buffer)));

    ASSERT_EQ(parent->children_count, 1U);
    ASSERT_EQ(parent->children[0], child);
    ASSERT_TRUE(child->parents[0] == parent || child->parents[1] == parent);

    AppCommand *command = app_command_create_delete_person(2002U);
    ASSERT_NOT_NULL(command);

    ASSERT_TRUE(app_state_push_command(&state, command, error_buffer, sizeof(error_buffer)));
    ASSERT_NULL(family_tree_find_person(tree, 2002U));
    ASSERT_EQ(parent->children_count, 0U);

    ASSERT_TRUE(app_state_undo(&state, error_buffer, sizeof(error_buffer)));
    Person *restored_child = family_tree_find_person(tree, 2002U);
    ASSERT_NOT_NULL(restored_child);
    ASSERT_EQ(parent->children_count, 1U);
    ASSERT_EQ(parent->children[0], restored_child);
    ASSERT_TRUE(restored_child->parents[0] == parent || restored_child->parents[1] == parent);

    ASSERT_TRUE(app_state_redo(&state, error_buffer, sizeof(error_buffer)));
    ASSERT_NULL(family_tree_find_person(tree, 2002U));
    ASSERT_EQ(parent->children_count, 0U);

    app_state_test_context_shutdown(&state, &layout, tree);
}

DECLARE_TEST(test_app_command_edit_person_roundtrip)
{
    AppState state;
    FamilyTree *tree = NULL;
    LayoutResult layout;
    InteractionState interaction;
    CameraController camera;
    Settings settings;
    Settings persisted_settings;

    app_state_test_context_init(&state, &tree, &layout, &interaction, &camera, &settings, &persisted_settings);

    char error_buffer[128];

    Person *person = app_state_test_create_person(3001U, "Iris", "Vector", "2000-09-09", "Mars City");
    ASSERT_NOT_NULL(person);
    ASSERT_TRUE(app_state_add_person(&state, person, error_buffer, sizeof(error_buffer)));

    AppPersonEditData edit_data = {
        "Iris",
        "Quantum",
        "Vector",
        "2000-09-09",
        "Mars City",
        "2080-01-01",
        "Europa",
        false,
    };

    AppCommand *command = app_command_create_edit_person(3001U, &edit_data);
    ASSERT_NOT_NULL(command);

    ASSERT_TRUE(app_state_push_command(&state, command, error_buffer, sizeof(error_buffer)));
    Person *edited = family_tree_find_person(tree, 3001U);
    ASSERT_NOT_NULL(edited);
    ASSERT_STREQ(edited->name.middle, "Quantum");
    ASSERT_FALSE(edited->is_alive);
    ASSERT_STREQ(edited->dates.death_location, "Europa");

    ASSERT_TRUE(app_state_undo(&state, error_buffer, sizeof(error_buffer)));
    Person *reverted = family_tree_find_person(tree, 3001U);
    ASSERT_NOT_NULL(reverted);
    ASSERT_TRUE(reverted->name.middle == NULL);
    ASSERT_TRUE(reverted->is_alive);
    ASSERT_NULL(reverted->dates.death_location);

    ASSERT_TRUE(app_state_redo(&state, error_buffer, sizeof(error_buffer)));
    Person *reapplied = family_tree_find_person(tree, 3001U);
    ASSERT_NOT_NULL(reapplied);
    ASSERT_STREQ(reapplied->name.middle, "Quantum");
    ASSERT_FALSE(reapplied->is_alive);

    app_state_test_context_shutdown(&state, &layout, tree);
}

void register_app_state_tests(TestRegistry *registry)
{
    REGISTER_TEST(registry, test_app_state_push_undo_redo);
    REGISTER_TEST(registry, test_app_state_push_failure_destroys_command);
    REGISTER_TEST(registry, test_app_state_reset_history_clears_dirty_flag);
    REGISTER_TEST(registry, test_app_command_add_person_roundtrip);
    REGISTER_TEST(registry, test_app_command_delete_person_roundtrip);
    REGISTER_TEST(registry, test_app_command_edit_person_roundtrip);
}
