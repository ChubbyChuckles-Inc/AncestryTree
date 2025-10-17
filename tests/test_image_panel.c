#include "image_panel.h"
#include "test_framework.h"

#include <math.h>
#include <stddef.h>
#include <string.h>

#ifndef M_PI
#define M_PI 3.14159265358979323846
#endif

static void sort_floats(float *values, size_t count)
{
    for (size_t outer = 0; outer < count; ++outer)
    {
        for (size_t inner = outer + 1U; inner < count; ++inner)
        {
            if (values[inner] < values[outer])
            {
                float temp = values[outer];
                values[outer] = values[inner];
                values[inner] = temp;
            }
        }
    }
}

TEST(test_image_panel_layout_even_distribution)
{
    ImagePanelSystem *system = image_panel_create();
    ASSERT_NOT_NULL(system);

    ImagePanelLayoutParams params;
    memset(&params, 0, sizeof(params));
    params.desired_count = 6U;
    params.cone_inner_radius = 0.52f;
    params.cone_outer_radius = 0.72f;
    params.min_height = 0.28f;
    params.max_height = 0.68f;
    params.min_angle_radians = 0.7f;
    params.timeline_radius = 0.62f;
    params.timeline_height = 0.0f;
    params.timeline_clear_margin = 0.12f;

    ASSERT_TRUE(image_panel_layout_init(system, &params));
    size_t count = image_panel_count(system);
    ASSERT_EQ(count, 6U);

    float angles[IMAGE_PANEL_MAX_COUNT] = {0};
    for (size_t index = 0; index < count; ++index)
    {
        ImagePanelAnchor anchor;
        ASSERT_TRUE(image_panel_get_anchor(system, index, &anchor));
        float angle = atan2f(anchor.position[2], anchor.position[0]);
        if (angle < 0.0f)
        {
            angle += (float)(2.0 * M_PI);
        }
        angles[index] = angle;
    }
    sort_floats(angles, count);

    float min_separation = params.min_angle_radians - 0.05f;
    if (min_separation < 0.0f)
    {
        min_separation = 0.0f;
    }

    for (size_t index = 0; index < count; ++index)
    {
        float current = angles[index];
        float next = angles[(index + 1U) % count];
        float separation = (index + 1U < count) ? (next - current) : ((float)(2.0 * M_PI) - current + angles[0]);
        ASSERT_TRUE(separation >= min_separation);
    }

    image_panel_destroy(system);
}

TEST(test_image_panel_normals_face_origin)
{
    ImagePanelSystem *system = image_panel_create();
    ASSERT_NOT_NULL(system);

    ImagePanelLayoutParams params;
    memset(&params, 0, sizeof(params));
    params.desired_count = 5U;
    params.cone_inner_radius = 0.5f;
    params.cone_outer_radius = 0.7f;
    params.min_height = 0.2f;
    params.max_height = 0.6f;
    params.min_angle_radians = 0.6f;
    params.timeline_radius = 0.6f;
    params.timeline_height = 0.0f;
    params.timeline_clear_margin = 0.1f;

    ASSERT_TRUE(image_panel_layout_init(system, &params));
    ASSERT_EQ(image_panel_count(system), 5U);

    image_panel_interaction(system, 0.016f, 0U, false);

    for (size_t index = 0; index < image_panel_count(system); ++index)
    {
        ImagePanelAnchor anchor;
        ASSERT_TRUE(image_panel_get_anchor(system, index, &anchor));
        float to_origin[3] = {-anchor.position[0], -anchor.position[1], -anchor.position[2]};
        float length = sqrtf(to_origin[0] * to_origin[0] + to_origin[1] * to_origin[1] + to_origin[2] * to_origin[2]);
        ASSERT_TRUE(length > 0.0f);
        to_origin[0] /= length;
        to_origin[1] /= length;
        to_origin[2] /= length;
        float dot = anchor.normal[0] * to_origin[0] + anchor.normal[1] * to_origin[1] + anchor.normal[2] * to_origin[2];
        ASSERT_TRUE(dot > 0.8f);
    }

    image_panel_destroy(system);
}

TEST(test_image_panel_interaction_focus_tracks_selection)
{
    ImagePanelSystem *system = image_panel_create();
    ASSERT_NOT_NULL(system);

    ImagePanelLayoutParams params;
    memset(&params, 0, sizeof(params));
    params.desired_count = 4U;
    params.cone_inner_radius = 0.5f;
    params.cone_outer_radius = 0.65f;
    params.min_height = 0.25f;
    params.max_height = 0.55f;
    params.min_angle_radians = 0.7f;
    params.timeline_radius = 0.58f;
    params.timeline_height = 0.0f;
    params.timeline_clear_margin = 0.12f;

    ASSERT_TRUE(image_panel_layout_init(system, &params));

    for (int step = 0; step < 12; ++step)
    {
        image_panel_interaction(system, 0.05f, 2U, true);
    }
    ASSERT_TRUE(image_panel_is_focus_active(system, 2U));

    ImagePanelAnchor focused;
    ASSERT_TRUE(image_panel_get_anchor(system, 2U, &focused));
    float focused_radial = sqrtf(focused.position[0] * focused.position[0] + focused.position[2] * focused.position[2]);

    ImagePanelAnchor neighbour;
    ASSERT_TRUE(image_panel_get_anchor(system, 1U, &neighbour));
    float neighbour_radial = sqrtf(neighbour.position[0] * neighbour.position[0] + neighbour.position[2] * neighbour.position[2]);

    ASSERT_TRUE(focused_radial < neighbour_radial);

    image_panel_destroy(system);
}

TEST(test_image_panel_interaction_relief_from_timeline_occlusion)
{
    ImagePanelSystem *system = image_panel_create();
    ASSERT_NOT_NULL(system);

    ImagePanelLayoutParams params;
    memset(&params, 0, sizeof(params));
    params.desired_count = 3U;
    params.cone_inner_radius = 0.55f;
    params.cone_outer_radius = 0.6f;
    params.min_height = 0.1f;
    params.max_height = 0.2f;
    params.min_angle_radians = 0.8f;
    params.timeline_radius = 0.575f;
    params.timeline_height = 0.0f;
    params.timeline_clear_margin = 0.15f;

    ASSERT_TRUE(image_panel_layout_init(system, &params));

    ImagePanelAnchor before;
    ASSERT_TRUE(image_panel_get_anchor(system, 0U, &before));
    float initial_height = before.position[1];
    ASSERT_FALSE(before.sightline_clear);

    for (int step = 0; step < 10; ++step)
    {
        image_panel_interaction(system, 0.05f, 0U, false);
    }

    ImagePanelAnchor after;
    ASSERT_TRUE(image_panel_get_anchor(system, 0U, &after));
    ASSERT_TRUE(after.position[1] > initial_height + 0.05f);
    ASSERT_TRUE(after.sightline_clear);

    image_panel_destroy(system);
}

void register_image_panel_tests(TestRegistry *registry)
{
    REGISTER_TEST(registry, test_image_panel_layout_even_distribution);
    REGISTER_TEST(registry, test_image_panel_normals_face_origin);
    REGISTER_TEST(registry, test_image_panel_interaction_focus_tracks_selection);
    REGISTER_TEST(registry, test_image_panel_interaction_relief_from_timeline_occlusion);
}
