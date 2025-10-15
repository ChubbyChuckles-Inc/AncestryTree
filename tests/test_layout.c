#include "layout.h"
#include "test_framework.h"
#include "test_persistence_helpers.h"

#include <math.h>

static bool position_is_finite(const float position[3])
{
    for (size_t index = 0U; index < 3U; ++index)
    {
        if (!isfinite(position[index]))
        {
            return false;
        }
    }
    return true;
}

TEST(test_layout_assigns_positions_for_all_persons)
{
    FamilyTree *tree = test_build_sample_tree();
    ASSERT_NOT_NULL(tree);

    LayoutResult result = layout_calculate(tree);
    ASSERT_EQ(result.count, tree->person_count);
    for (size_t index = 0U; index < result.count; ++index)
    {
        ASSERT_NOT_NULL(result.nodes[index].person);
        ASSERT_TRUE(position_is_finite(result.nodes[index].position));
    }

    layout_result_destroy(&result);
    family_tree_destroy(tree);
}

TEST(test_layout_descendants_positioned_on_lower_levels)
{
    FamilyTree *tree = test_build_sample_tree();
    ASSERT_NOT_NULL(tree);

    LayoutResult result = layout_calculate(tree);
    ASSERT_EQ(result.count, tree->person_count);

    LayoutNode *root_node = NULL;
    LayoutNode *child_node = NULL;
    for (size_t index = 0U; index < result.count; ++index)
    {
        if (result.nodes[index].person->id == 1U)
        {
            root_node = &result.nodes[index];
        }
        else if (result.nodes[index].person->id == 2U)
        {
            child_node = &result.nodes[index];
        }
    }

    ASSERT_NOT_NULL(root_node);
    ASSERT_NOT_NULL(child_node);
    ASSERT_TRUE(child_node->position[1] < root_node->position[1]);

    layout_result_destroy(&result);
    family_tree_destroy(tree);
}

void register_layout_tests(TestRegistry *registry)
{
    REGISTER_TEST(registry, test_layout_assigns_positions_for_all_persons);
    REGISTER_TEST(registry, test_layout_descendants_positioned_on_lower_levels);
}
