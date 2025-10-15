#include "interaction.h"

#include "camera_controller.h"
#include "layout.h"
#include "person.h"
#include "test_framework.h"

#include <math.h>
#include <stdlib.h>

TEST(test_interaction_state_initializes_defaults)
{
    InteractionState state;
    interaction_state_init(&state);
    ASSERT_TRUE(fabsf(state.sphere_pick_radius - 0.6f) < 0.001f);
    ASSERT_NULL(interaction_get_hovered(&state));
    ASSERT_NULL(interaction_get_selected(&state));
}

TEST(test_interaction_ray_sphere_intersection_detects_hits)
{
    float origin[3] = {0.0f, 0.0f, 0.0f};
    float direction[3] = {0.0f, 0.0f, -1.0f};
    float sphere_center[3] = {0.0f, 0.0f, -5.0f};
    float distance = 0.0f;
    ASSERT_TRUE(interaction_ray_sphere_intersection(origin, direction, sphere_center, 1.0f, &distance));
    ASSERT_FLOAT_NEAR(distance, 4.0f, 0.01f);

    float miss_center[3] = {5.0f, 0.0f, -5.0f};
    ASSERT_FALSE(interaction_ray_sphere_intersection(origin, direction, miss_center, 1.0f, NULL));
}

TEST(test_interaction_select_requires_raylib_runtime)
{
    InteractionState state;
    interaction_state_init(&state);
    LayoutResult layout;
    layout.nodes = NULL;
    layout.count = 0U;
    CameraController camera;
    camera.initialized = false;
    ASSERT_FALSE(interaction_select_at_cursor(&state, &layout, &camera, 0.0f, 0.0f, true));
}

void register_interaction_tests(TestRegistry *registry)
{
    REGISTER_TEST(registry, test_interaction_state_initializes_defaults);
    REGISTER_TEST(registry, test_interaction_ray_sphere_intersection_detects_hits);
    REGISTER_TEST(registry, test_interaction_select_requires_raylib_runtime);
}
