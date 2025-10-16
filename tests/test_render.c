#include "render.h"

#include "render_internal.h"

#include "layout.h"
#include "person.h"
#include "test_framework.h"

#include <math.h>
#include <stdlib.h>

static void test_free_layout(LayoutResult *layout)
{
    if (!layout)
    {
        return;
    }
    free(layout->nodes);
    layout->nodes = NULL;
    layout->count = 0U;
}

TEST(test_render_config_default_is_valid)
{
    RenderConfig config = render_config_default();
    ASSERT_TRUE(render_config_validate(&config));
    ASSERT_TRUE(config.sphere_radius > 0.0f);
}

TEST(test_render_find_person_position_returns_expected_coordinates)
{
    Person *person = person_create(1U);
    ASSERT_NOT_NULL(person);

    LayoutResult layout;
    layout.count = 1U;
    layout.nodes = calloc(1U, sizeof(LayoutNode));
    ASSERT_NOT_NULL(layout.nodes);
    layout.nodes[0].person = person;
    layout.nodes[0].position[0] = 1.0f;
    layout.nodes[0].position[1] = -2.0f;
    layout.nodes[0].position[2] = 3.5f;

    float position[3] = {0.0f, 0.0f, 0.0f};
    ASSERT_TRUE(render_find_person_position(&layout, person, position));
    ASSERT_FLOAT_NEAR(position[0], 1.0f, 0.0001f);
    ASSERT_FLOAT_NEAR(position[1], -2.0f, 0.0001f);
    ASSERT_FLOAT_NEAR(position[2], 3.5f, 0.0001f);

    float missing[3] = {0.0f, 0.0f, 0.0f};
    Person *other = person_create(2U);
    ASSERT_NOT_NULL(other);
    ASSERT_FALSE(render_find_person_position(&layout, other, missing));

    person_destroy(other);
    test_free_layout(&layout);
    person_destroy(person);
}

TEST(test_render_collect_parent_child_segments_collects_all_children)
{
    Person *parent = person_create(10U);
    Person *child_a = person_create(11U);
    Person *child_b = person_create(12U);
    ASSERT_TRUE(parent != NULL && child_a != NULL && child_b != NULL);
    ASSERT_TRUE(person_add_child(parent, child_a));
    ASSERT_TRUE(person_add_child(parent, child_b));

    LayoutResult layout;
    layout.count = 3U;
    layout.nodes = calloc(layout.count, sizeof(LayoutNode));
    ASSERT_NOT_NULL(layout.nodes);
    layout.nodes[0].person = parent;
    layout.nodes[0].position[0] = 0.0f;
    layout.nodes[0].position[1] = 0.0f;
    layout.nodes[0].position[2] = 0.0f;
    layout.nodes[1].person = child_a;
    layout.nodes[1].position[0] = 1.0f;
    layout.nodes[1].position[1] = -1.0f;
    layout.nodes[1].position[2] = 0.0f;
    layout.nodes[2].person = child_b;
    layout.nodes[2].position[0] = -1.0f;
    layout.nodes[2].position[1] = -1.0f;
    layout.nodes[2].position[2] = 0.0f;

    RenderConnectionSegment segments[4];
    size_t count = render_collect_parent_child_segments(&layout, segments, 4U);
    ASSERT_EQ(count, 2U);

    bool seen_a = false;
    bool seen_b = false;
    for (size_t index = 0; index < count; ++index)
    {
        const RenderConnectionSegment *segment = &segments[index];
        ASSERT_FLOAT_NEAR(segment->start[1], 0.0f, 0.0001f);
        if (fabsf(segment->end[0] - 1.0f) < 0.001f)
        {
            seen_a = true;
        }
        if (fabsf(segment->end[0] + 1.0f) < 0.001f)
        {
            seen_b = true;
        }
    }
    ASSERT_TRUE(seen_a);
    ASSERT_TRUE(seen_b);

    test_free_layout(&layout);
    person_destroy(child_b);
    person_destroy(child_a);
    person_destroy(parent);
}

TEST(test_render_collect_spouse_segments_ignores_duplicates)
{
    Person *one = person_create(21U);
    Person *two = person_create(22U);
    ASSERT_TRUE(person_add_spouse(one, two));

    LayoutResult layout;
    layout.count = 2U;
    layout.nodes = calloc(layout.count, sizeof(LayoutNode));
    ASSERT_NOT_NULL(layout.nodes);
    layout.nodes[0].person = one;
    layout.nodes[0].position[0] = 0.0f;
    layout.nodes[0].position[1] = 0.0f;
    layout.nodes[0].position[2] = 0.0f;
    layout.nodes[1].person = two;
    layout.nodes[1].position[0] = 2.0f;
    layout.nodes[1].position[1] = 0.0f;
    layout.nodes[1].position[2] = 0.0f;

    RenderConnectionSegment segments[4];
    size_t count = render_collect_spouse_segments(&layout, segments, 4U);
    ASSERT_EQ(count, 1U);
    ASSERT_FLOAT_NEAR(segments[0].start[0], 0.0f, 0.0001f);
    ASSERT_FLOAT_NEAR(segments[0].end[0], 2.0f, 0.0001f);

    test_free_layout(&layout);
    person_destroy(two);
    person_destroy(one);
}

TEST(test_render_batcher_plan_groups_alive_and_deceased)
{
    Person *alive_a = person_create(31U);
    Person *alive_b = person_create(32U);
    Person *deceased = person_create(33U);
    ASSERT_TRUE(alive_a && alive_b && deceased);
    alive_a->is_alive = true;
    alive_b->is_alive = true;
    deceased->is_alive = false;

    LayoutResult layout;
    layout.count = 3U;
    layout.nodes = calloc(layout.count, sizeof(LayoutNode));
    ASSERT_NOT_NULL(layout.nodes);
    layout.nodes[0].person = alive_a;
    layout.nodes[1].person = alive_b;
    layout.nodes[2].person = deceased;

    const LayoutNode *alive_nodes[3] = {0};
    const LayoutNode *deceased_nodes[3] = {0};
    RenderBatcherGrouping grouping;
    render_batcher_grouping_reset(&grouping);
    ASSERT_TRUE(render_batcher_plan(&layout, NULL, NULL, &grouping, alive_nodes, 3U, deceased_nodes, 3U));
    ASSERT_EQ(grouping.alive_count, 2U);
    ASSERT_EQ(grouping.deceased_count, 1U);
    ASSERT_TRUE(grouping.alive_nodes[0]->person->is_alive);
    ASSERT_TRUE(grouping.alive_nodes[1]->person->is_alive);
    ASSERT_FALSE(grouping.deceased_nodes[0]->person->is_alive);

    free(layout.nodes);
    person_destroy(deceased);
    person_destroy(alive_b);
    person_destroy(alive_a);
}

TEST(test_render_batcher_plan_handles_selected_and_hovered)
{
    Person *alive = person_create(40U);
    Person *hover = person_create(41U);
    Person *deceased = person_create(42U);
    ASSERT_TRUE(alive && hover && deceased);
    alive->is_alive = true;
    hover->is_alive = true;
    deceased->is_alive = false;

    LayoutResult layout;
    layout.count = 3U;
    layout.nodes = calloc(layout.count, sizeof(LayoutNode));
    ASSERT_NOT_NULL(layout.nodes);
    layout.nodes[0].person = alive;
    layout.nodes[1].person = hover;
    layout.nodes[2].person = deceased;

    const LayoutNode *alive_nodes[3] = {0};
    const LayoutNode *deceased_nodes[3] = {0};
    RenderBatcherGrouping grouping;
    render_batcher_grouping_reset(&grouping);
    ASSERT_TRUE(render_batcher_plan(&layout, alive, hover, &grouping, alive_nodes, 3U, deceased_nodes, 3U));
    ASSERT_EQ(grouping.alive_count, 0U);
    ASSERT_EQ(grouping.deceased_count, 1U);
    ASSERT_NOT_NULL(grouping.selected_node);
    ASSERT_EQ(grouping.selected_node->person, alive);
    ASSERT_NOT_NULL(grouping.hovered_node);
    ASSERT_EQ(grouping.hovered_node->person, hover);
    ASSERT_EQ(grouping.deceased_nodes[0]->person, deceased);

    free(layout.nodes);
    person_destroy(deceased);
    person_destroy(hover);
    person_destroy(alive);
}

TEST(test_render_batcher_plan_handles_hover_equal_selected)
{
    Person *alive = person_create(50U);
    ASSERT_NOT_NULL(alive);
    alive->is_alive = true;

    LayoutResult layout;
    layout.count = 1U;
    layout.nodes = calloc(layout.count, sizeof(LayoutNode));
    ASSERT_NOT_NULL(layout.nodes);
    layout.nodes[0].person = alive;

    const LayoutNode *alive_nodes[1] = {0};
    const LayoutNode *deceased_nodes[1] = {0};
    RenderBatcherGrouping grouping;
    render_batcher_grouping_reset(&grouping);
    ASSERT_TRUE(render_batcher_plan(&layout, alive, alive, &grouping, alive_nodes, 1U, deceased_nodes, 1U));
    ASSERT_EQ(grouping.alive_count, 0U);
    ASSERT_NULL(grouping.hovered_node);
    ASSERT_NOT_NULL(grouping.selected_node);

    free(layout.nodes);
    person_destroy(alive);
}

TEST(test_render_resize_updates_dimensions_when_raylib_missing)
{
    RenderState state;
    render_state_init(&state);
    RenderConfig config = render_config_default();
    char error_buffer[64];
    ASSERT_TRUE(render_init(&state, &config, error_buffer, sizeof(error_buffer)));
    bool resize_ok = render_resize(&state, 640, 360, error_buffer, sizeof(error_buffer));
    ASSERT_EQ(state.render_width, 640);
    ASSERT_EQ(state.render_height, 360);
#if defined(ANCESTRYTREE_HAVE_RAYLIB)
    if (!resize_ok)
    {
        ASSERT_FALSE(render_has_render_target(&state));
    }
#else
    ASSERT_TRUE(resize_ok);
    ASSERT_FALSE(render_has_render_target(&state));
#endif
    render_cleanup(&state);
}

TEST(test_render_resize_rejects_zero_dimension)
{
    RenderState state;
    render_state_init(&state);
    RenderConfig config = render_config_default();
    char error_buffer[64];
    ASSERT_TRUE(render_init(&state, &config, error_buffer, sizeof(error_buffer)));
    ASSERT_FALSE(render_resize(&state, 0, 480, error_buffer, sizeof(error_buffer)));
    ASSERT_FALSE(render_has_render_target(&state));
    render_cleanup(&state);
}

void register_render_tests(TestRegistry *registry)
{
    REGISTER_TEST(registry, test_render_config_default_is_valid);
    REGISTER_TEST(registry, test_render_find_person_position_returns_expected_coordinates);
    REGISTER_TEST(registry, test_render_collect_parent_child_segments_collects_all_children);
    REGISTER_TEST(registry, test_render_collect_spouse_segments_ignores_duplicates);
    REGISTER_TEST(registry, test_render_batcher_plan_groups_alive_and_deceased);
    REGISTER_TEST(registry, test_render_batcher_plan_handles_selected_and_hovered);
    REGISTER_TEST(registry, test_render_batcher_plan_handles_hover_equal_selected);
    REGISTER_TEST(registry, test_render_resize_updates_dimensions_when_raylib_missing);
    REGISTER_TEST(registry, test_render_resize_rejects_zero_dimension);
}
