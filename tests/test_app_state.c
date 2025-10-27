#include "app.h"
#include "at_memory.h"
#include "camera_controller.h"
#include "interaction.h"
#include "layout.h"
#include "persistence.h"
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

static void mock_command_destroy(AppCommand *command) { AT_FREE(command); }

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
    mock->base.vtable  = &MOCK_COMMAND_VTABLE;
    mock->value        = value;
    mock->delta        = delta;
    mock->fail_execute = fail_execute;
    mock->fail_undo    = fail_undo;
    mock->executed     = false;
    return (AppCommand *)mock;
}

static void app_state_test_context_init(AppState *state, FamilyTree **tree, LayoutResult *layout,
                                        InteractionState *interaction, CameraController *camera,
                                        Settings *settings, Settings *persisted_settings)
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
    ASSERT_TRUE(app_state_configure(state, tree, layout, interaction, camera, settings,
                                    persisted_settings));
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

    app_state_test_context_init(&state, &tree, &layout, &interaction, &camera, &settings,
                                &persisted_settings);

    int value           = 0;
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

    app_state_test_context_init(&state, &tree, &layout, &interaction, &camera, &settings,
                                &persisted_settings);

    int value           = 0;
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

    app_state_test_context_init(&state, &tree, &layout, &interaction, &camera, &settings,
                                &persisted_settings);

    int value           = 0;
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

    app_state_test_context_init(&state, &tree, &layout, &interaction, &camera, &settings,
                                &persisted_settings);

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

    app_state_test_context_init(&state, &tree, &layout, &interaction, &camera, &settings,
                                &persisted_settings);

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

    app_state_test_context_init(&state, &tree, &layout, &interaction, &camera, &settings,
                                &persisted_settings);

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

    app_state_test_context_init(&state, &tree, &layout, &interaction, &camera, &settings,
                                &persisted_settings);

    char error_buffer[128];

    Person *father =
        app_state_test_create_person(3101U, "Orion", "Prime", "1960-02-02", "Luna Base");
    ASSERT_NOT_NULL(father);
    ASSERT_TRUE(app_state_add_person(&state, father, error_buffer, sizeof(error_buffer)));

    Person *mother =
        app_state_test_create_person(3102U, "Lyra", "Prime", "1965-03-03", "Luna Base");
    ASSERT_NOT_NULL(mother);
    ASSERT_TRUE(app_state_add_person(&state, mother, error_buffer, sizeof(error_buffer)));

    Person *primary_spouse =
        app_state_test_create_person(3103U, "Vega", "Prime", "1985-04-04", "Mars City");
    ASSERT_NOT_NULL(primary_spouse);
    ASSERT_TRUE(app_state_add_person(&state, primary_spouse, error_buffer, sizeof(error_buffer)));

    Person *secondary_spouse =
        app_state_test_create_person(3104U, "Altair", "Prime", "1986-05-05", "Mars City");
    ASSERT_NOT_NULL(secondary_spouse);
    ASSERT_TRUE(app_state_add_person(&state, secondary_spouse, error_buffer, sizeof(error_buffer)));

    Person *person =
        app_state_test_create_person(3001U, "Iris", "Vector", "2000-09-09", "Mars City");
    ASSERT_NOT_NULL(person);
    ASSERT_TRUE(app_state_add_person(&state, person, error_buffer, sizeof(error_buffer)));

    ASSERT_TRUE(person_set_parent(person, father, PERSON_PARENT_FATHER));
    ASSERT_TRUE(person_add_child(father, person));
    ASSERT_TRUE(person_add_spouse(person, primary_spouse));

    AppPersonEditData edit_data = {
        .first          = "Iris",
        .middle         = "Quantum",
        .last           = "Vector",
        .birth_date     = "2000-09-09",
        .birth_location = "Mars City",
        .death_date     = "2080-01-01",
        .death_location = "Europa",
        .clear_death    = false,
        .relationships =
            {
                .apply_father  = true,
                .father_id     = 0U,
                .apply_mother  = true,
                .mother_id     = mother->id,
                .apply_spouses = true,
                .spouse_count  = 2U,
            },
    };
    edit_data.relationships.spouse_ids[0] = primary_spouse->id;
    edit_data.relationships.spouse_ids[1] = secondary_spouse->id;

    AppCommand *command = app_command_create_edit_person(3001U, &edit_data);
    ASSERT_NOT_NULL(command);

    ASSERT_TRUE(app_state_push_command(&state, command, error_buffer, sizeof(error_buffer)));
    Person *edited = family_tree_find_person(tree, 3001U);
    ASSERT_NOT_NULL(edited);
    ASSERT_STREQ(edited->name.middle, "Quantum");
    ASSERT_FALSE(edited->is_alive);
    ASSERT_STREQ(edited->dates.death_location, "Europa");
    ASSERT_NULL(edited->parents[PERSON_PARENT_FATHER]);
    ASSERT_EQ(father->children_count, 0U);
    ASSERT_EQ(edited->parents[PERSON_PARENT_MOTHER], mother);
    ASSERT_EQ(mother->children_count, 1U);
    ASSERT_EQ(edited->spouses_count, 2U);
    ASSERT_EQ(primary_spouse->spouses_count, 1U);
    ASSERT_EQ(primary_spouse->spouses[0].partner, edited);
    ASSERT_EQ(secondary_spouse->spouses_count, 1U);
    ASSERT_EQ(secondary_spouse->spouses[0].partner, edited);

    ASSERT_TRUE(app_state_undo(&state, error_buffer, sizeof(error_buffer)));
    Person *reverted = family_tree_find_person(tree, 3001U);
    ASSERT_NOT_NULL(reverted);
    ASSERT_TRUE(reverted->name.middle == NULL);
    ASSERT_TRUE(reverted->is_alive);
    ASSERT_NULL(reverted->dates.death_location);
    ASSERT_EQ(reverted->parents[PERSON_PARENT_FATHER], father);
    ASSERT_EQ(father->children_count, 1U);
    ASSERT_NULL(reverted->parents[PERSON_PARENT_MOTHER]);
    ASSERT_EQ(mother->children_count, 0U);
    ASSERT_EQ(reverted->spouses_count, 1U);
    ASSERT_EQ(reverted->spouses[0].partner, primary_spouse);
    ASSERT_EQ(primary_spouse->spouses_count, 1U);
    ASSERT_EQ(secondary_spouse->spouses_count, 0U);

    ASSERT_TRUE(app_state_redo(&state, error_buffer, sizeof(error_buffer)));
    Person *reapplied = family_tree_find_person(tree, 3001U);
    ASSERT_NOT_NULL(reapplied);
    ASSERT_STREQ(reapplied->name.middle, "Quantum");
    ASSERT_FALSE(reapplied->is_alive);
    ASSERT_NULL(reapplied->parents[PERSON_PARENT_FATHER]);
    ASSERT_EQ(father->children_count, 0U);
    ASSERT_EQ(reapplied->parents[PERSON_PARENT_MOTHER], mother);
    ASSERT_EQ(mother->children_count, 1U);
    ASSERT_EQ(reapplied->spouses_count, 2U);
    ASSERT_EQ(secondary_spouse->spouses_count, 1U);

    app_state_test_context_shutdown(&state, &layout, tree);
}

DECLARE_TEST(test_app_state_create_person_with_relationships)
{
    AppState state;
    FamilyTree *tree = NULL;
    LayoutResult layout;
    InteractionState interaction;
    CameraController camera;
    Settings settings;
    Settings persisted_settings;

    app_state_test_context_init(&state, &tree, &layout, &interaction, &camera, &settings,
                                &persisted_settings);

    char error_buffer[128];
    error_buffer[0] = '\0';

    Person *father =
        app_state_test_create_person(101U, "Orion", "Prime", "1960-02-02", "Luna Base");
    ASSERT_NOT_NULL(father);
    ASSERT_TRUE(app_state_add_person(&state, father, error_buffer, sizeof(error_buffer)));

    Person *mother = app_state_test_create_person(102U, "Lyra", "Prime", "1965-03-03", "Luna Base");
    ASSERT_NOT_NULL(mother);
    ASSERT_TRUE(app_state_add_person(&state, mother, error_buffer, sizeof(error_buffer)));

    Person *spouse = app_state_test_create_person(103U, "Vega", "Prime", "1985-04-04", "Mars City");
    ASSERT_NOT_NULL(spouse);
    ASSERT_TRUE(app_state_add_person(&state, spouse, error_buffer, sizeof(error_buffer)));

    AppPersonCreateData create_data;
    memset(&create_data, 0, sizeof(create_data));
    create_data.first                           = "Nova";
    create_data.middle                          = "Starlight";
    create_data.last                            = "Prime";
    create_data.birth_date                      = "2090-01-01";
    create_data.birth_location                  = "Orbital Habitat";
    create_data.is_alive                        = true;
    create_data.profile_image_path              = "profiles/nova.png";
    create_data.certificate_paths[0]            = "certificates/nova_birth.png";
    create_data.certificate_count               = 1U;
    create_data.timeline_entries[0].type        = TIMELINE_EVENT_BIRTH;
    create_data.timeline_entries[0].date        = "2090-01-01";
    create_data.timeline_entries[0].description = "Birth aboard orbital habitat.";
    create_data.timeline_entries[0].location    = "Orbital Habitat";
    create_data.timeline_count                  = 1U;
    create_data.father_id                       = father->id;
    create_data.mother_id                       = mother->id;
    create_data.spouse_id                       = spouse->id;

    uint32_t new_person_id = 0U;
    ASSERT_TRUE(app_state_create_person(&state, &create_data, &new_person_id, error_buffer,
                                        sizeof(error_buffer)));
    ASSERT_TRUE(new_person_id != 0U);

    Person *created = family_tree_find_person(tree, new_person_id);
    ASSERT_NOT_NULL(created);
    ASSERT_STREQ(created->name.middle, "Starlight");
    ASSERT_TRUE(created->is_alive);
    ASSERT_EQ(created->parents[PERSON_PARENT_FATHER], father);
    ASSERT_EQ(created->parents[PERSON_PARENT_MOTHER], mother);
    ASSERT_EQ(father->children_count, 1U);
    ASSERT_EQ(mother->children_count, 1U);
    ASSERT_EQ(spouse->spouses_count, 1U);
    ASSERT_EQ(spouse->spouses[0].partner, created);
    ASSERT_STREQ(created->profile_image_path, "profiles/nova.png");
    ASSERT_EQ(created->certificate_count, 1U);
    ASSERT_STREQ(created->certificate_paths[0], "certificates/nova_birth.png");
    ASSERT_EQ(created->timeline_count, 1U);
    ASSERT_STREQ(created->timeline_entries[0].description, "Birth aboard orbital habitat.");
    ASSERT_TRUE(app_state_is_tree_dirty(&state));

    ASSERT_TRUE(app_state_undo(&state, error_buffer, sizeof(error_buffer)));
    ASSERT_NULL(family_tree_find_person(tree, new_person_id));
    ASSERT_EQ(father->children_count, 0U);
    ASSERT_EQ(mother->children_count, 0U);
    ASSERT_EQ(spouse->spouses_count, 0U);

    ASSERT_TRUE(app_state_redo(&state, error_buffer, sizeof(error_buffer)));
    Person *reapplied = family_tree_find_person(tree, new_person_id);
    ASSERT_NOT_NULL(reapplied);
    ASSERT_EQ(father->children_count, 1U);
    ASSERT_EQ(mother->children_count, 1U);
    ASSERT_EQ(spouse->spouses_count, 1U);

    app_state_test_context_shutdown(&state, &layout, tree);
}

DECLARE_TEST(test_app_state_create_grandchild_through_child_spouses)
{
    AppState state;
    FamilyTree *tree = NULL;
    LayoutResult layout;
    InteractionState interaction;
    CameraController camera;
    Settings settings;
    Settings persisted_settings;

    app_state_test_context_init(&state, &tree, &layout, &interaction, &camera, &settings,
                                &persisted_settings);

    char error_buffer[128];
    error_buffer[0] = '\0';

    AppPersonCreateData ancestor;
    memset(&ancestor, 0, sizeof(ancestor));
    ancestor.first          = "Heinrich";
    ancestor.last           = "Schmidt";
    ancestor.birth_date     = "1930-05-05";
    ancestor.is_alive       = false;
    ancestor.death_date     = "1999-03-03";
    uint32_t grandfather_id = 0U;
    ASSERT_TRUE(app_state_create_person(&state, &ancestor, &grandfather_id, error_buffer,
                                        sizeof(error_buffer)));

    memset(&ancestor, 0, sizeof(ancestor));
    ancestor.first          = "Margarete";
    ancestor.last           = "Schmidt";
    ancestor.birth_date     = "1932-07-07";
    ancestor.is_alive       = true;
    ancestor.spouse_id      = grandfather_id;
    uint32_t grandmother_id = 0U;
    ASSERT_TRUE(app_state_create_person(&state, &ancestor, &grandmother_id, error_buffer,
                                        sizeof(error_buffer)));

    AppPersonCreateData child;
    memset(&child, 0, sizeof(child));
    child.first          = "Anneliese";
    child.last           = "Schmidt";
    child.birth_date     = "1960-01-15";
    child.is_alive       = true;
    child.father_id      = grandfather_id;
    child.mother_id      = grandmother_id;
    uint32_t daughter_id = 0U;
    ASSERT_TRUE(
        app_state_create_person(&state, &child, &daughter_id, error_buffer, sizeof(error_buffer)));

    AppPersonCreateData spouse;
    memset(&spouse, 0, sizeof(spouse));
    spouse.first       = "Karl";
    spouse.last        = "Meyer";
    spouse.birth_date  = "1958-11-11";
    spouse.is_alive    = true;
    spouse.spouse_id   = daughter_id;
    uint32_t spouse_id = 0U;
    ASSERT_TRUE(
        app_state_create_person(&state, &spouse, &spouse_id, error_buffer, sizeof(error_buffer)));

    AppPersonCreateData grandchild;
    memset(&grandchild, 0, sizeof(grandchild));
    grandchild.first       = "Jonas";
    grandchild.last        = "Meyer";
    grandchild.birth_date  = "1986-04-04";
    grandchild.is_alive    = true;
    grandchild.father_id   = spouse_id;
    grandchild.mother_id   = daughter_id;
    uint32_t grandchild_id = 0U;
    ASSERT_TRUE(app_state_create_person(&state, &grandchild, &grandchild_id, error_buffer,
                                        sizeof(error_buffer)));

    Person *grandchild_person = family_tree_find_person(tree, grandchild_id);
    ASSERT_NOT_NULL(grandchild_person);
    ASSERT_EQ(grandchild_person->parents[PERSON_PARENT_FATHER]->id, spouse_id);
    ASSERT_EQ(grandchild_person->parents[PERSON_PARENT_MOTHER]->id, daughter_id);

    Person *daughter = family_tree_find_person(tree, daughter_id);
    ASSERT_NOT_NULL(daughter);
    ASSERT_EQ(daughter->children_count, 1U);
    ASSERT_EQ(daughter->children[0]->id, grandchild_id);

    Person *spouse_person = family_tree_find_person(tree, spouse_id);
    ASSERT_NOT_NULL(spouse_person);
    ASSERT_EQ(spouse_person->children_count, 1U);
    ASSERT_EQ(spouse_person->children[0]->id, grandchild_id);

    app_state_test_context_shutdown(&state, &layout, tree);
}

DECLARE_TEST(test_app_state_create_grandchild_in_loaded_tree)
{
    char error_buffer[256];
    error_buffer[0] = '\0';
    FamilyTree *loaded_tree =
        persistence_tree_load("assets/rickert_tree.json", error_buffer, sizeof(error_buffer));
    ASSERT_NOT_NULL(loaded_tree);

    LayoutResult layout = layout_calculate(loaded_tree);
    InteractionState interaction;
    interaction_state_init(&interaction);
    interaction_state_set_pick_radius(&interaction, 1.0f);
    CameraControllerConfig camera_config;
    camera_controller_config_default(&camera_config);
    CameraController camera;
    camera_controller_init(&camera, &camera_config);

    Settings settings;
    settings_init_defaults(&settings);
    Settings persisted_settings = settings;

    AppState state;
    app_state_init(&state);
    FamilyTree *tree_handle = loaded_tree;
    ASSERT_TRUE(app_state_configure(&state, &tree_handle, &layout, &interaction, &camera, &settings,
                                    &persisted_settings));

    const Person *father = family_tree_find_person(tree_handle, 3U);
    const Person *mother = family_tree_find_person(tree_handle, 4U);
    ASSERT_NOT_NULL(father);
    ASSERT_NOT_NULL(mother);

    AppPersonCreateData grandchild;
    memset(&grandchild, 0, sizeof(grandchild));
    grandchild.first      = "Lisa";
    grandchild.last       = "Meyer";
    grandchild.birth_date = "1990-08-12";
    grandchild.is_alive   = true;
    grandchild.father_id  = father->id;
    grandchild.mother_id  = mother->id;

    uint32_t grandchild_id = 0U;
    ASSERT_TRUE(app_state_create_person(&state, &grandchild, &grandchild_id, error_buffer,
                                        sizeof(error_buffer)));
    ASSERT_TRUE(grandchild_id != 0U);

    const Person *created = family_tree_find_person(tree_handle, grandchild_id);
    ASSERT_NOT_NULL(created);
    ASSERT_EQ(created->parents[PERSON_PARENT_FATHER], father);
    ASSERT_EQ(created->parents[PERSON_PARENT_MOTHER], mother);

    app_state_shutdown(&state);
    layout_result_destroy(&layout);
    family_tree_destroy(tree_handle);
}

DECLARE_TEST(test_app_state_create_person_validation)
{
    AppState state;
    FamilyTree *tree = NULL;
    LayoutResult layout;
    InteractionState interaction;
    CameraController camera;
    Settings settings;
    Settings persisted_settings;

    app_state_test_context_init(&state, &tree, &layout, &interaction, &camera, &settings,
                                &persisted_settings);

    char error_buffer[128];
    error_buffer[0] = '\0';

    AppPersonCreateData invalid_name;
    memset(&invalid_name, 0, sizeof(invalid_name));
    invalid_name.first      = "";
    invalid_name.last       = "Tester";
    invalid_name.birth_date = "2100-01-01";
    invalid_name.is_alive   = true;
    ASSERT_FALSE(
        app_state_create_person(&state, &invalid_name, NULL, error_buffer, sizeof(error_buffer)));
    ASSERT_TRUE(error_buffer[0] != '\0');

    AppPersonCreateData invalid_deceased;
    memset(&invalid_deceased, 0, sizeof(invalid_deceased));
    invalid_deceased.first      = "Alpha";
    invalid_deceased.last       = "Tester";
    invalid_deceased.birth_date = "2100-01-01";
    invalid_deceased.is_alive   = false;
    invalid_deceased.death_date = "";
    error_buffer[0]             = '\0';
    ASSERT_FALSE(app_state_create_person(&state, &invalid_deceased, NULL, error_buffer,
                                         sizeof(error_buffer)));
    ASSERT_TRUE(error_buffer[0] != '\0');

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
    REGISTER_TEST(registry, test_app_state_create_person_with_relationships);
    REGISTER_TEST(registry, test_app_state_create_grandchild_through_child_spouses);
    REGISTER_TEST(registry, test_app_state_create_grandchild_in_loaded_tree);
    REGISTER_TEST(registry, test_app_state_create_person_validation);
}
