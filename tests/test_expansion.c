#include "expansion.h"

#include "camera_controller.h"
#include "layout.h"
#include "person.h"
#include "test_framework.h"

#include <math.h>

TEST(test_expansion_start_validates_input)
{
    ExpansionState state;
    expansion_state_reset(&state);

    LayoutResult empty_layout;
    empty_layout.nodes = NULL;
    empty_layout.count = 0U;

    Person *person = person_create(1U);
    ASSERT_NOT_NULL(person);

    ASSERT_FALSE(expansion_start(&state, &empty_layout, person, NULL));

    person_destroy(person);
}

TEST(test_expansion_start_initializes_state)
{
    ExpansionState state;
    expansion_state_reset(&state);

    Person *person = person_create(2U);
    ASSERT_NOT_NULL(person);

    LayoutNode node;
    node.person = person;
    node.position[0] = 2.0f;
    node.position[1] = 3.0f;
    node.position[2] = -1.0f;

    LayoutResult layout;
    layout.nodes = &node;
    layout.count = 1U;

    ASSERT_TRUE(expansion_start(&state, &layout, person, NULL));
    ASSERT_TRUE(expansion_is_active(&state));
    ASSERT_FALSE(expansion_is_in_detail_mode(&state));

    float current_position[3];
    expansion_current_position(&state, current_position);
    ASSERT_FLOAT_NEAR(current_position[0], node.position[0], 0.001f);
    ASSERT_FLOAT_NEAR(current_position[1], node.position[1], 0.001f);
    ASSERT_FLOAT_NEAR(current_position[2], node.position[2], 0.001f);

    ASSERT_FLOAT_NEAR(expansion_primary_scale(&state), 1.0f, 0.001f);
    ASSERT_FLOAT_NEAR(expansion_inactive_scale(&state), 1.0f, 0.001f);
    ASSERT_FLOAT_NEAR(expansion_inactive_opacity(&state), 1.0f, 0.001f);

    person_destroy(person);
}

TEST(test_expansion_update_and_reverse)
{
    ExpansionState state;
    expansion_state_reset(&state);

    Person *person = person_create(3U);
    ASSERT_NOT_NULL(person);

    LayoutNode node;
    node.person = person;
    node.position[0] = -4.0f;
    node.position[1] = 1.5f;
    node.position[2] = 5.0f;

    LayoutResult layout;
    layout.nodes = &node;
    layout.count = 1U;

    CameraControllerConfig config;
    camera_controller_config_default(&config);
    CameraController camera;
    ASSERT_TRUE(camera_controller_init(&camera, &config));

    ASSERT_TRUE(expansion_start(&state, &layout, person, &camera));

    float step = state.duration * 0.6f;
    ASSERT_FALSE(expansion_update(&state, step, &camera));
    ASSERT_FALSE(expansion_is_in_detail_mode(&state));

    ASSERT_TRUE(expansion_update(&state, step, &camera));
    ASSERT_TRUE(expansion_is_in_detail_mode(&state));

    expansion_reverse(&state, &camera);
    ASSERT_TRUE(expansion_is_active(&state));
    ASSERT_FALSE(expansion_is_in_detail_mode(&state));

    /* Advance past the reverse animation. */
    float remaining = state.duration;
    while (expansion_is_active(&state) && remaining > 0.0f)
    {
        float slice = fminf(remaining, state.duration * 0.5f);
        (void)expansion_update(&state, slice, &camera);
        remaining -= slice;
    }
    if (expansion_is_active(&state))
    {
        (void)expansion_update(&state, state.duration + 0.01f, &camera);
    }
    ASSERT_FALSE(expansion_is_active(&state));
    ASSERT_FALSE(expansion_is_in_detail_mode(&state));

    person_destroy(person);
}

void register_expansion_tests(TestRegistry *registry)
{
    REGISTER_TEST(registry, test_expansion_start_validates_input);
    REGISTER_TEST(registry, test_expansion_start_initializes_state);
    REGISTER_TEST(registry, test_expansion_update_and_reverse);
}
