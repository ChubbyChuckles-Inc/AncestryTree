#include "layout.h"
#include "person.h"
#include "test_framework.h"
#include "test_persistence_helpers.h"
#include "tree.h"

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

static FamilyTree *layout_create_spouse_tree(void)
{
    FamilyTree *tree = family_tree_create("Spouse Tree");
    if (!tree)
    {
        return NULL;
    }

    Person *alex = person_create(100U);
    Person *sam = person_create(101U);
    if (!alex || !sam)
    {
        person_destroy(alex);
        person_destroy(sam);
        family_tree_destroy(tree);
        return NULL;
    }

    person_set_name(alex, "Alex", NULL, "Root");
    person_set_name(sam, "Sam", NULL, "Root");
    person_set_birth(alex, "1980-01-01", "Earth");
    person_set_birth(sam, "1981-06-01", "Earth");

    person_add_spouse(alex, sam);
    family_tree_add_person(tree, alex);
    family_tree_add_person(tree, sam);

    return tree;
}

TEST(test_layout_root_centered)
{
    FamilyTree *tree = test_build_sample_tree();
    ASSERT_NOT_NULL(tree);

    LayoutResult result = layout_calculate(tree);
    ASSERT_EQ(result.count, tree->person_count);

    bool found_root = false;
    for (size_t index = 0U; index < result.count; ++index)
    {
        if (result.nodes[index].person && result.nodes[index].person->id == 1U)
        {
            ASSERT_FLOAT_NEAR(result.nodes[index].position[0], 0.0f, 0.001f);
            found_root = true;
        }
    }
    ASSERT_TRUE(found_root);

    layout_result_destroy(&result);
    family_tree_destroy(tree);
}

TEST(test_layout_spouses_positioned_side_by_side)
{
    FamilyTree *tree = layout_create_spouse_tree();
    ASSERT_NOT_NULL(tree);

    LayoutResult result = layout_calculate(tree);
    ASSERT_EQ(result.count, tree->person_count);

    const LayoutNode *alex_node = NULL;
    const LayoutNode *sam_node = NULL;
    for (size_t index = 0U; index < result.count; ++index)
    {
        if (!result.nodes[index].person)
        {
            continue;
        }
        if (result.nodes[index].person->id == 100U)
        {
            alex_node = &result.nodes[index];
        }
        else if (result.nodes[index].person->id == 101U)
        {
            sam_node = &result.nodes[index];
        }
    }

    ASSERT_NOT_NULL(alex_node);
    ASSERT_NOT_NULL(sam_node);
    ASSERT_FLOAT_NEAR(alex_node->position[1], sam_node->position[1], 0.0001f);
    const float expected_spacing = 2.0f;
    ASSERT_FLOAT_NEAR(fabsf(alex_node->position[0] - sam_node->position[0]), expected_spacing, 0.001f);

    layout_result_destroy(&result);
    family_tree_destroy(tree);
}

void register_layout_tests(TestRegistry *registry)
{
    REGISTER_TEST(registry, test_layout_assigns_positions_for_all_persons);
    REGISTER_TEST(registry, test_layout_descendants_positioned_on_lower_levels);
    REGISTER_TEST(registry, test_layout_root_centered);
    REGISTER_TEST(registry, test_layout_spouses_positioned_side_by_side);
}
