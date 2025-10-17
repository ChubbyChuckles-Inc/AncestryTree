#include "expansion.h"
#include "camera_controller.h"
#include "layout.h"
#include "person.h"
#include "test_framework.h"

#include <string.h>

static void prepare_layout(Person *person, LayoutResult *layout, LayoutNode *storage)
{
    storage[0].person = person;
    storage[0].position[0] = 1.5f;
    storage[0].position[1] = 0.5f;
    storage[0].position[2] = -0.75f;
    layout->nodes = storage;
    layout->count = 1U;
}

TEST(test_expansion_cycle_reaches_detail_mode_and_resets)
{
    ExpansionState state;
    expansion_state_reset(&state);

    Person *person = person_create(101U);
    ASSERT_NOT_NULL(person);
    ASSERT_TRUE(person_set_name(person, "Ada", NULL, "Lovelace"));
    ASSERT_TRUE(person_set_birth(person, "1815-12-10", "London"));

    LayoutNode node_storage[1];
    LayoutResult layout;
    memset(&layout, 0, sizeof(layout));
    prepare_layout(person, &layout, node_storage);

    CameraControllerConfig config;
    camera_controller_config_default(&config);
    CameraController camera;
    ASSERT_TRUE(camera_controller_init(&camera, &config));

    ASSERT_TRUE(expansion_start(&state, &layout, person, &camera));
    ASSERT_TRUE(expansion_is_active(&state));
    float baseline_radius = expansion_primary_scale(&state);

    const float delta = 0.05f;
    for (int step = 0; step < 40; ++step)
    {
        expansion_update(&state, delta, &camera);
    }

    ASSERT_TRUE(expansion_is_in_detail_mode(&state));
    ASSERT_TRUE(expansion_primary_scale(&state) > baseline_radius);
    ASSERT_TRUE(expansion_room_light_factor(&state) > 0.5f);
    ASSERT_TRUE(expansion_tree_light_factor(&state) < 0.8f);

    expansion_reverse(&state, &camera);
    ASSERT_TRUE(expansion_is_reversing(&state));

    bool reset_observed = false;
    for (int step = 0; step < 40; ++step)
    {
        if (expansion_update(&state, delta, &camera))
        {
            reset_observed = true;
            break;
        }
    }

    ASSERT_TRUE(reset_observed);
    ASSERT_FALSE(expansion_is_active(&state));

    person_destroy(person);
}

void register_expansion_tests(TestRegistry *registry)
{
    REGISTER_TEST(registry, test_expansion_cycle_reaches_detail_mode_and_resets);
}
