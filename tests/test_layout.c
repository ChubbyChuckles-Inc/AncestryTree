#include "layout.h"
#include "person.h"
#include "test_framework.h"
#include "test_persistence_helpers.h"
#include "tree.h"

#include <math.h>
#include <stdlib.h>

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

static const LayoutNode *find_node_by_id(const LayoutResult *result, uint32_t id)
{
    if (!result || !result->nodes)
    {
        return NULL;
    }
    for (size_t index = 0U; index < result->count; ++index)
    {
        if (result->nodes[index].person && result->nodes[index].person->id == id)
        {
            return &result->nodes[index];
        }
    }
    return NULL;
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

    LayoutNode *root_node  = NULL;
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
    Person *sam  = person_create(101U);
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
    if (!family_tree_add_person(tree, alex))
    {
        person_destroy(alex);
        person_destroy(sam);
        family_tree_destroy(tree);
        return NULL;
    }
    if (!family_tree_add_person(tree, sam))
    {
        person_destroy(sam);
        family_tree_destroy(tree);
        return NULL;
    }

    return tree;
}

static FamilyTree *layout_create_single_person_tree(void)
{
    FamilyTree *tree = family_tree_create("Solo");
    if (!tree)
    {
        return NULL;
    }
    Person *solo = person_create(201U);
    if (!solo)
    {
        family_tree_destroy(tree);
        return NULL;
    }
    person_set_name(solo, "Solo", NULL, "Node");
    person_set_birth(solo, "2000-01-01", "Nowhere");
    if (!family_tree_add_person(tree, solo))
    {
        person_destroy(solo);
        family_tree_destroy(tree);
        return NULL;
    }
    return tree;
}

static FamilyTree *layout_create_small_family_tree(void)
{
    FamilyTree *tree = family_tree_create("Small Family");
    if (!tree)
    {
        return NULL;
    }
    Person *parent      = person_create(210U);
    Person *left_child  = person_create(211U);
    Person *right_child = person_create(212U);
    if (!parent || !left_child || !right_child)
    {
        person_destroy(parent);
        person_destroy(left_child);
        person_destroy(right_child);
        family_tree_destroy(tree);
        return NULL;
    }

    person_set_name(parent, "Parent", NULL, "Node");
    person_set_name(left_child, "Left", NULL, "Node");
    person_set_name(right_child, "Right", NULL, "Node");
    person_set_birth(parent, "1985-01-01", "Base");
    person_set_birth(left_child, "2010-01-01", "Base");
    person_set_birth(right_child, "2012-01-01", "Base");

    person_add_child(parent, left_child);
    person_add_child(parent, right_child);

    if (!family_tree_add_person(tree, parent))
    {
        person_destroy(left_child);
        person_destroy(right_child);
        family_tree_destroy(tree);
        return NULL;
    }
    if (!family_tree_add_person(tree, left_child))
    {
        person_destroy(left_child);
        person_destroy(right_child);
        family_tree_destroy(tree);
        return NULL;
    }
    if (!family_tree_add_person(tree, right_child))
    {
        person_destroy(right_child);
        family_tree_destroy(tree);
        return NULL;
    }

    return tree;
}

static FamilyTree *layout_create_generation_tree(size_t generations, size_t branching)
{
    FamilyTree *tree = family_tree_create("Generations");
    if (!tree)
    {
        return NULL;
    }
    Person *current_parent = person_create(300U);
    if (!current_parent)
    {
        family_tree_destroy(tree);
        return NULL;
    }
    person_set_name(current_parent, "Root", NULL, "Ancestor");
    person_set_birth(current_parent, "1900-01-01", "Origin");
    if (!family_tree_add_person(tree, current_parent))
    {
        person_destroy(current_parent);
        family_tree_destroy(tree);
        return NULL;
    }

    uint32_t next_id             = 301U;
    Person **previous_generation = (Person **)calloc(branching, sizeof(Person *));
    if (!previous_generation)
    {
        family_tree_destroy(tree);
        return NULL;
    }
    previous_generation[0] = current_parent;
    size_t previous_count  = 1U;

    for (size_t generation = 1U; generation < generations; ++generation)
    {
        size_t new_count         = previous_count * branching;
        Person **next_generation = (Person **)calloc(new_count, sizeof(Person *));
        if (!next_generation)
        {
            free(previous_generation);
            family_tree_destroy(tree);
            return NULL;
        }
        size_t cursor = 0U;
        for (size_t parent_index = 0U; parent_index < previous_count; ++parent_index)
        {
            Person *parent = previous_generation[parent_index];
            if (!parent)
            {
                continue;
            }
            for (size_t child_index = 0U; child_index < branching; ++child_index)
            {
                Person *child = person_create(next_id++);
                if (!child)
                {
                    free(next_generation);
                    free(previous_generation);
                    family_tree_destroy(tree);
                    return NULL;
                }
                person_set_name(child, "Child", NULL, "Generation");
                person_set_birth(child, "1950-01-01", "Somewhere");
                person_add_child(parent, child);
                if (!family_tree_add_person(tree, child))
                {
                    person_destroy(child);
                    free(next_generation);
                    free(previous_generation);
                    family_tree_destroy(tree);
                    return NULL;
                }
                next_generation[cursor++] = child;
            }
        }
        free(previous_generation);
        previous_generation = next_generation;
        previous_count      = cursor;
    }

    free(previous_generation);
    return tree;
}

static FamilyTree *layout_create_complex_tree(void)
{
    FamilyTree *tree = family_tree_create("Complex");
    if (!tree)
    {
        return NULL;
    }
    Person *parent_a  = person_create(400U);
    Person *parent_b  = person_create(401U);
    Person *parent_c  = person_create(402U);
    Person *child_one = person_create(403U);
    Person *child_two = person_create(404U);
    if (!parent_a || !parent_b || !parent_c || !child_one || !child_two)
    {
        person_destroy(parent_a);
        person_destroy(parent_b);
        person_destroy(parent_c);
        person_destroy(child_one);
        person_destroy(child_two);
        family_tree_destroy(tree);
        return NULL;
    }

    person_set_name(parent_a, "Alex", NULL, "Alpha");
    person_set_name(parent_b, "Blair", NULL, "Alpha");
    person_set_name(parent_c, "Casey", NULL, "Beta");
    person_set_name(child_one, "Drew", NULL, "Alpha");
    person_set_name(child_two, "Evan", NULL, "Alpha");
    person_set_birth(parent_a, "1970-01-01", "Colony");
    person_set_birth(parent_b, "1972-02-02", "Colony");
    person_set_birth(parent_c, "1975-03-03", "Colony");
    person_set_birth(child_one, "1996-01-01", "Colony");
    person_set_birth(child_two, "1998-01-01", "Colony");

    person_add_spouse(parent_a, parent_b);
    person_add_spouse(parent_b, parent_c);
    person_add_child(parent_a, child_one);
    person_add_child(parent_b, child_one);
    person_add_child(parent_b, child_two);
    person_add_child(parent_c, child_two);

    if (!family_tree_add_person(tree, parent_a))
    {
        person_destroy(parent_b);
        person_destroy(parent_c);
        person_destroy(child_one);
        person_destroy(child_two);
        family_tree_destroy(tree);
        return NULL;
    }
    if (!family_tree_add_person(tree, parent_b))
    {
        person_destroy(parent_b);
        person_destroy(parent_c);
        person_destroy(child_one);
        person_destroy(child_two);
        family_tree_destroy(tree);
        return NULL;
    }
    if (!family_tree_add_person(tree, parent_c))
    {
        person_destroy(parent_c);
        person_destroy(child_one);
        person_destroy(child_two);
        family_tree_destroy(tree);
        return NULL;
    }
    if (!family_tree_add_person(tree, child_one))
    {
        person_destroy(child_one);
        person_destroy(child_two);
        family_tree_destroy(tree);
        return NULL;
    }
    if (!family_tree_add_person(tree, child_two))
    {
        person_destroy(child_two);
        family_tree_destroy(tree);
        return NULL;
    }

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

TEST(test_layout_small_family_balanced_around_parent)
{
    FamilyTree *tree = layout_create_small_family_tree();
    ASSERT_NOT_NULL(tree);

    LayoutResult result = layout_calculate(tree);
    ASSERT_EQ(result.count, tree->person_count);

    const LayoutNode *parent_node = NULL;
    const LayoutNode *left_node   = NULL;
    const LayoutNode *right_node  = NULL;
    for (size_t index = 0U; index < result.count; ++index)
    {
        const Person *person = result.nodes[index].person;
        if (!person)
        {
            continue;
        }
        if (person->id == 210U)
        {
            parent_node = &result.nodes[index];
        }
        else if (person->id == 211U)
        {
            left_node = &result.nodes[index];
        }
        else if (person->id == 212U)
        {
            right_node = &result.nodes[index];
        }
    }

    ASSERT_NOT_NULL(parent_node);
    ASSERT_NOT_NULL(left_node);
    ASSERT_NOT_NULL(right_node);
    ASSERT_TRUE(left_node->position[1] < parent_node->position[1]);
    ASSERT_TRUE(right_node->position[1] < parent_node->position[1]);

    float average_horizontal = 0.5f * (left_node->position[0] + right_node->position[0]);
    ASSERT_FLOAT_NEAR(average_horizontal, parent_node->position[0], 0.001f);
    ASSERT_TRUE(fabsf(left_node->position[0] - right_node->position[0]) >= 1.0f);

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
    const LayoutNode *sam_node  = NULL;
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
    ASSERT_FLOAT_NEAR(fabsf(alex_node->position[0] - sam_node->position[0]), expected_spacing,
                      0.001f);

    layout_result_destroy(&result);
    family_tree_destroy(tree);
}

TEST(test_layout_single_person_stays_at_origin)
{
    FamilyTree *tree = layout_create_single_person_tree();
    ASSERT_NOT_NULL(tree);

    LayoutResult result = layout_calculate(tree);
    ASSERT_EQ(result.count, 1U);
    ASSERT_FLOAT_NEAR(result.nodes[0].position[0], 0.0f, 0.0001f);
    ASSERT_FLOAT_NEAR(result.nodes[0].position[1], 0.0f, 0.0001f);
    ASSERT_FLOAT_NEAR(result.nodes[0].position[2], 0.0f, 0.0001f);

    layout_result_destroy(&result);
    family_tree_destroy(tree);
}

TEST(test_layout_multiple_generations_stack_levels)
{
    FamilyTree *tree = layout_create_generation_tree(3U, 2U);
    ASSERT_NOT_NULL(tree);

    LayoutResult result = layout_calculate(tree);
    ASSERT_EQ(result.count, tree->person_count);

    float highest_level = -INFINITY;
    float lowest_level  = INFINITY;
    for (size_t index = 0U; index < result.count; ++index)
    {
        float level = result.nodes[index].position[1];
        if (level > highest_level)
        {
            highest_level = level;
        }
        if (level < lowest_level)
        {
            lowest_level = level;
        }
    }
    ASSERT_TRUE(isfinite(highest_level));
    ASSERT_TRUE(isfinite(lowest_level));
    ASSERT_TRUE(highest_level > lowest_level);

    layout_result_destroy(&result);
    family_tree_destroy(tree);
}

TEST(test_layout_large_family_has_unique_horizontal_spacing)
{
    FamilyTree *tree = layout_create_generation_tree(2U, 5U);
    ASSERT_NOT_NULL(tree);

    LayoutResult result = layout_calculate(tree);
    ASSERT_EQ(result.count, tree->person_count);

    for (size_t parent_index = 0U; parent_index < result.count; ++parent_index)
    {
        const LayoutNode *node_a = &result.nodes[parent_index];
        for (size_t child_index = parent_index + 1U; child_index < result.count; ++child_index)
        {
            const LayoutNode *node_b = &result.nodes[child_index];
            if (fabsf(node_a->position[1] - node_b->position[1]) < 0.0001f)
            {
                ASSERT_TRUE(fabsf(node_a->position[0] - node_b->position[0]) >= 1.0f);
            }
        }
    }

    layout_result_destroy(&result);
    family_tree_destroy(tree);
}

TEST(test_layout_complex_relationships_remain_finite)
{
    FamilyTree *tree = layout_create_complex_tree();
    ASSERT_NOT_NULL(tree);

    LayoutResult result = layout_calculate(tree);
    ASSERT_EQ(result.count, tree->person_count);

    for (size_t index = 0U; index < result.count; ++index)
    {
        ASSERT_TRUE(position_is_finite(result.nodes[index].position));
    }

    layout_result_destroy(&result);
    family_tree_destroy(tree);
}

TEST(test_layout_force_directed_preserves_levels)
{
    FamilyTree *tree = layout_create_small_family_tree();
    ASSERT_NOT_NULL(tree);

    LayoutResult hierarchical = layout_calculate(tree);
    LayoutResult force        = layout_calculate_force_directed(tree);
    ASSERT_EQ(force.count, hierarchical.count);

    bool any_moved = false;
    for (size_t index = 0U; index < force.count; ++index)
    {
        const LayoutNode *force_node = &force.nodes[index];
        ASSERT_NOT_NULL(force_node->person);
        ASSERT_TRUE(position_is_finite(force_node->position));
        const LayoutNode *baseline = find_node_by_id(&hierarchical, force_node->person->id);
        ASSERT_NOT_NULL(baseline);
        ASSERT_TRUE(fabsf(force_node->position[1] - baseline->position[1]) < 0.05f);
        if (fabsf(force_node->position[0] - baseline->position[0]) > 0.05f ||
            fabsf(force_node->position[2] - baseline->position[2]) > 0.05f)
        {
            any_moved = true;
        }
    }
    ASSERT_TRUE(any_moved);

    layout_result_destroy(&force);
    layout_result_destroy(&hierarchical);
    family_tree_destroy(tree);
}

TEST(test_layout_animate_interpolates_between_layouts)
{
    FamilyTree *tree = test_build_sample_tree();
    ASSERT_NOT_NULL(tree);

    LayoutResult start   = layout_calculate(tree);
    LayoutResult target  = layout_calculate_force_directed(tree);
    LayoutResult blended = {0};

    ASSERT_TRUE(layout_animate(&start, &target, 0.5f, &blended));
    ASSERT_EQ(blended.count, target.count);

    const LayoutNode *start_root  = find_node_by_id(&start, 1U);
    const LayoutNode *target_root = find_node_by_id(&target, 1U);
    const LayoutNode *blend_root  = find_node_by_id(&blended, 1U);
    ASSERT_NOT_NULL(start_root);
    ASSERT_NOT_NULL(target_root);
    ASSERT_NOT_NULL(blend_root);

    float expected_x = (start_root->position[0] + target_root->position[0]) * 0.5f;
    float expected_z = (start_root->position[2] + target_root->position[2]) * 0.5f;
    ASSERT_TRUE(fabsf(blend_root->position[0] - expected_x) < 0.05f);
    ASSERT_TRUE(fabsf(blend_root->position[2] - expected_z) < 0.05f);
    ASSERT_TRUE(fabsf(blend_root->position[1] - target_root->position[1]) < 0.05f);

    layout_result_destroy(&blended);
    layout_result_destroy(&target);
    layout_result_destroy(&start);
    family_tree_destroy(tree);
}

TEST(test_layout_cache_invalidation_on_structure_change)
{
    LayoutCache cache;
    layout_cache_init(&cache);

    FamilyTree *tree = layout_create_small_family_tree();
    ASSERT_NOT_NULL(tree);

    LayoutResult initial_layout;
    ASSERT_TRUE(
        layout_cache_calculate(&cache, tree, LAYOUT_ALGORITHM_HIERARCHICAL, &initial_layout));
    ASSERT_EQ(initial_layout.count, tree->person_count);

    size_t original_count = initial_layout.count;
    layout_result_destroy(&initial_layout);

    Person *parent = tree->persons[0];
    ASSERT_NOT_NULL(parent);

    Person *new_child = person_create(999U);
    ASSERT_NOT_NULL(new_child);
    person_set_name(new_child, "New", NULL, "Child");
    person_set_birth(new_child, "2015-05-05", "Base");
    ASSERT_TRUE(person_add_child(parent, new_child));
    ASSERT_TRUE(family_tree_add_person(tree, new_child));

    LayoutResult updated_layout;
    ASSERT_TRUE(
        layout_cache_calculate(&cache, tree, LAYOUT_ALGORITHM_HIERARCHICAL, &updated_layout));
    ASSERT_EQ(updated_layout.count, tree->person_count);
    ASSERT_EQ(updated_layout.count, original_count + 1U);

    layout_result_destroy(&updated_layout);
    layout_cache_reset(&cache);
    family_tree_destroy(tree);
}

TEST(test_layout_cache_handles_multiple_algorithms)
{
    LayoutCache cache;
    layout_cache_init(&cache);

    FamilyTree *tree = test_build_sample_tree();
    ASSERT_NOT_NULL(tree);

    LayoutResult hierarchical_layout;
    ASSERT_TRUE(
        layout_cache_calculate(&cache, tree, LAYOUT_ALGORITHM_HIERARCHICAL, &hierarchical_layout));
    ASSERT_EQ(hierarchical_layout.count, tree->person_count);

    LayoutResult force_layout;
    ASSERT_TRUE(
        layout_cache_calculate(&cache, tree, LAYOUT_ALGORITHM_FORCE_DIRECTED, &force_layout));
    ASSERT_EQ(force_layout.count, tree->person_count);

    bool hierarchical_cached = false;
    bool force_cached        = false;
    for (size_t index = 0U; index < cache.count && index < LAYOUT_CACHE_MAX_ENTRIES; ++index)
    {
        if (!cache.entries[index].valid)
        {
            continue;
        }
        if (cache.entries[index].algorithm == LAYOUT_ALGORITHM_HIERARCHICAL)
        {
            hierarchical_cached = true;
        }
        else if (cache.entries[index].algorithm == LAYOUT_ALGORITHM_FORCE_DIRECTED)
        {
            force_cached = true;
        }
    }

    ASSERT_TRUE(hierarchical_cached);
    ASSERT_TRUE(force_cached);

    layout_result_destroy(&hierarchical_layout);
    layout_result_destroy(&force_layout);
    layout_cache_reset(&cache);
    family_tree_destroy(tree);
}

TEST(test_layout_incremental_matches_full_recalculation)
{
    LayoutCache cache;
    layout_cache_init(&cache);

    FamilyTree *tree = layout_create_small_family_tree();
    ASSERT_NOT_NULL(tree);

    LayoutResult baseline;
    ASSERT_TRUE(layout_cache_calculate(&cache, tree, LAYOUT_ALGORITHM_HIERARCHICAL, &baseline));
    layout_result_destroy(&baseline);

    Person *parent = tree->persons[0];
    ASSERT_NOT_NULL(parent);

    Person *new_child = person_create(9137U);
    ASSERT_NOT_NULL(new_child);
    person_set_name(new_child, "Incremental", NULL, "Child");
    person_set_birth(new_child, "2018-04-12", "Nowhere");
    ASSERT_TRUE(person_add_child(parent, new_child));
    ASSERT_TRUE(family_tree_add_person(tree, new_child));

    const Person *changes[] = {new_child, parent};
    LayoutResult incremental;
    ASSERT_TRUE(layout_cache_incremental(&cache, tree, LAYOUT_ALGORITHM_HIERARCHICAL, changes,
                                         sizeof(changes) / sizeof(changes[0]), &incremental));

    LayoutResult full = layout_calculate(tree);
    ASSERT_EQ(incremental.count, full.count);

    for (size_t index = 0U; index < tree->person_count; ++index)
    {
        Person *person = tree->persons[index];
        ASSERT_NOT_NULL(person);
        const LayoutNode *incremental_node = find_node_by_id(&incremental, person->id);
        const LayoutNode *full_node        = find_node_by_id(&full, person->id);
        ASSERT_NOT_NULL(incremental_node);
        ASSERT_NOT_NULL(full_node);
        ASSERT_TRUE(fabsf(incremental_node->position[0] - full_node->position[0]) < 0.0001f);
        ASSERT_TRUE(fabsf(incremental_node->position[1] - full_node->position[1]) < 0.0001f);
        ASSERT_TRUE(fabsf(incremental_node->position[2] - full_node->position[2]) < 0.0001f);
    }

    layout_result_destroy(&full);
    layout_result_destroy(&incremental);
    layout_cache_reset(&cache);
    family_tree_destroy(tree);
}

void register_layout_tests(TestRegistry *registry)
{
    REGISTER_TEST(registry, test_layout_assigns_positions_for_all_persons);
    REGISTER_TEST(registry, test_layout_descendants_positioned_on_lower_levels);
    REGISTER_TEST(registry, test_layout_root_centered);
    REGISTER_TEST(registry, test_layout_small_family_balanced_around_parent);
    REGISTER_TEST(registry, test_layout_spouses_positioned_side_by_side);
    REGISTER_TEST(registry, test_layout_single_person_stays_at_origin);
    REGISTER_TEST(registry, test_layout_multiple_generations_stack_levels);
    REGISTER_TEST(registry, test_layout_large_family_has_unique_horizontal_spacing);
    REGISTER_TEST(registry, test_layout_complex_relationships_remain_finite);
    REGISTER_TEST(registry, test_layout_force_directed_preserves_levels);
    REGISTER_TEST(registry, test_layout_animate_interpolates_between_layouts);
    REGISTER_TEST(registry, test_layout_cache_invalidation_on_structure_change);
    REGISTER_TEST(registry, test_layout_cache_handles_multiple_algorithms);
    REGISTER_TEST(registry, test_layout_incremental_matches_full_recalculation);
}
