#include "camera_controller.h"
#include "expansion.h"
#include "layout.h"
#include "persistence.h"
#include "person.h"
#include "test_framework.h"
#include "test_persistence_helpers.h"
#include "tree.h"
#include "ui.h"

#include <stdio.h>

DECLARE_TEST(test_integration_create_save_load_round_trip)
{
    FamilyTree *tree = family_tree_create("Integration Workflow");
    ASSERT_NOT_NULL(tree);

    Person *alex = person_create(100U);
    Person *blair = person_create(101U);
    Person *casey = person_create(102U);
    ASSERT_NOT_NULL(alex);
    ASSERT_NOT_NULL(blair);
    ASSERT_NOT_NULL(casey);

    ASSERT_TRUE(person_set_name(alex, "Alex", NULL, "Integration"));
    ASSERT_TRUE(person_set_birth(alex, "1970-01-01", "Mars Colony"));
    ASSERT_TRUE(person_set_death(alex, "2024-05-01", "Mars Colony"));

    ASSERT_TRUE(person_set_name(blair, "Blair", NULL, "Integration"));
    ASSERT_TRUE(person_set_birth(blair, "1972-05-16", "Mars Colony"));
    blair->is_alive = true;

    ASSERT_TRUE(person_set_name(casey, "Casey", NULL, "Integration"));
    ASSERT_TRUE(person_set_birth(casey, "1998-09-23", "Mars Colony"));
    casey->is_alive = true;

    ASSERT_TRUE(family_tree_add_person(tree, alex));
    ASSERT_TRUE(family_tree_add_person(tree, blair));
    ASSERT_TRUE(family_tree_add_person(tree, casey));

    ASSERT_TRUE(person_add_child(alex, casey));
    ASSERT_TRUE(person_add_child(blair, casey));
    ASSERT_TRUE(person_set_parent(casey, alex, PERSON_PARENT_FATHER));
    ASSERT_TRUE(person_set_parent(casey, blair, PERSON_PARENT_MOTHER));

    char path_buffer[128];
    test_temp_file_path(path_buffer, sizeof(path_buffer), "integration_output.json");
    char error_buffer[256];
    error_buffer[0] = '\0';
    char validation_error[256];
    validation_error[0] = '\0';
    if (!family_tree_validate(tree, validation_error, sizeof(validation_error)))
    {
        printf("family_tree_validate failed: %s\n", validation_error);
        ASSERT_TRUE(false);
    }
    ASSERT_TRUE(persistence_tree_save(tree, path_buffer, error_buffer, sizeof(error_buffer)));

    FamilyTree *loaded = persistence_tree_load(path_buffer, error_buffer, sizeof(error_buffer));
    ASSERT_NOT_NULL(loaded);
    ASSERT_EQ(loaded->person_count, 3U);

    Person *loaded_casey = family_tree_find_person(loaded, 102U);
    ASSERT_NOT_NULL(loaded_casey);
    ASSERT_NOT_NULL(loaded_casey->parents[PERSON_PARENT_FATHER]);
    ASSERT_NOT_NULL(loaded_casey->parents[PERSON_PARENT_MOTHER]);
    ASSERT_EQ(loaded_casey->parents[PERSON_PARENT_FATHER]->id, 100U);
    ASSERT_EQ(loaded_casey->parents[PERSON_PARENT_MOTHER]->id, 101U);

    ASSERT_EQ(remove(path_buffer), 0);

    family_tree_destroy(tree);
    family_tree_destroy(loaded);
}

DECLARE_TEST(test_integration_expansion_workflow)
{
    ExpansionState state;
    expansion_state_reset(&state);
    ASSERT_FALSE(expansion_is_active(&state));

    Person *subject = person_create(900U);
    ASSERT_NOT_NULL(subject);
    ASSERT_TRUE(person_set_name(subject, "Integration", NULL, "Expansion"));

    LayoutNode nodes[1];
    nodes[0].person = subject;
    nodes[0].position[0] = 2.0f;
    nodes[0].position[1] = 1.0f;
    nodes[0].position[2] = -3.0f;
    LayoutResult layout;
    layout.nodes = nodes;
    layout.count = 1U;

    CameraControllerConfig config;
    camera_controller_config_default(&config);
    CameraController controller;
    ASSERT_TRUE(camera_controller_init(&controller, &config));

    ASSERT_TRUE(expansion_start(&state, &layout, subject, &controller));
    ASSERT_TRUE(expansion_is_active(&state));
    ASSERT_FALSE(expansion_is_in_detail_mode(&state));
    ASSERT_FALSE(expansion_is_reversing(&state));

    float position[3];
    expansion_current_position(&state, position);
    ASSERT_FLOAT_NEAR(position[0], nodes[0].position[0], 0.001f);
    ASSERT_FLOAT_NEAR(position[1], nodes[0].position[1], 0.001f);
    ASSERT_FLOAT_NEAR(position[2], nodes[0].position[2], 0.001f);

    ASSERT_FALSE(expansion_update(&state, 0.1f, &controller));
    ASSERT_TRUE(expansion_is_active(&state));
    ASSERT_FALSE(expansion_is_in_detail_mode(&state));

    ASSERT_TRUE(expansion_update(&state, state.duration, &controller));
    ASSERT_TRUE(expansion_is_active(&state));
    ASSERT_TRUE(expansion_is_in_detail_mode(&state));
    ASSERT_FALSE(expansion_is_reversing(&state));
    ASSERT_FLOAT_NEAR(expansion_primary_scale(&state), state.radius_target, 0.001f);
    ASSERT_FLOAT_NEAR(expansion_room_light_factor(&state), 1.0f, 0.01f);

    expansion_reverse(&state, &controller);
    ASSERT_TRUE(expansion_is_active(&state));
    ASSERT_TRUE(expansion_is_reversing(&state));
    ASSERT_FALSE(expansion_is_in_detail_mode(&state));

    ASSERT_TRUE(expansion_update(&state, state.duration, &controller));
    ASSERT_FALSE(expansion_is_active(&state));

    person_destroy(subject);
}

DECLARE_TEST(test_integration_ui_interaction_workflow)
{
    UIContext ui = {0};
    UIEvent events[UI_EVENT_QUEUE_CAPACITY] = {0};

    ASSERT_TRUE(ui_event_enqueue(&ui, UI_EVENT_NEW_TREE));
    ASSERT_TRUE(ui_event_enqueue_with_u32(&ui, UI_EVENT_FOCUS_PERSON, 77U));

    size_t processed = ui_poll_events(&ui, events, 1U);
    ASSERT_EQ(processed, 1U);
    ASSERT_EQ(events[0].type, UI_EVENT_NEW_TREE);

    processed = ui_poll_events(&ui, events, UI_EVENT_QUEUE_CAPACITY);
    ASSERT_EQ(processed, 1U);
    ASSERT_EQ(events[0].type, UI_EVENT_FOCUS_PERSON);
    ASSERT_EQ(events[0].param_u32, 77U);

    ASSERT_EQ(ui_poll_events(&ui, events, UI_EVENT_QUEUE_CAPACITY), 0U);

    for (size_t index = 0U; index < UI_EVENT_QUEUE_CAPACITY; ++index)
    {
        ASSERT_TRUE(ui_event_enqueue_with_u32(&ui, UI_EVENT_FOCUS_PERSON, (uint32_t)index));
    }
    ASSERT_FALSE(ui_event_enqueue(&ui, UI_EVENT_NEW_TREE));

    processed = ui_poll_events(&ui, events, UI_EVENT_QUEUE_CAPACITY / 2U);
    ASSERT_EQ(processed, UI_EVENT_QUEUE_CAPACITY / 2U);
    for (size_t index = 0U; index < processed; ++index)
    {
        ASSERT_EQ(events[index].type, UI_EVENT_FOCUS_PERSON);
        ASSERT_EQ(events[index].param_u32, (uint32_t)index);
    }

    processed = ui_poll_events(&ui, events, UI_EVENT_QUEUE_CAPACITY);
    ASSERT_EQ(processed, UI_EVENT_QUEUE_CAPACITY - (UI_EVENT_QUEUE_CAPACITY / 2U));
    for (size_t index = 0U; index < processed; ++index)
    {
        ASSERT_EQ(events[index].type, UI_EVENT_FOCUS_PERSON);
        ASSERT_EQ(events[index].param_u32, (uint32_t)(index + (UI_EVENT_QUEUE_CAPACITY / 2U)));
    }

    ASSERT_TRUE(ui_event_enqueue(&ui, UI_EVENT_REQUEST_EXIT));
    processed = ui_poll_events(&ui, events, UI_EVENT_QUEUE_CAPACITY);
    ASSERT_EQ(processed, 1U);
    ASSERT_EQ(events[0].type, UI_EVENT_REQUEST_EXIT);
}

void register_integration_tests(TestRegistry *registry)
{
    REGISTER_TEST(registry, test_integration_create_save_load_round_trip);
    REGISTER_TEST(registry, test_integration_expansion_workflow);
    REGISTER_TEST(registry, test_integration_ui_interaction_workflow);
}
