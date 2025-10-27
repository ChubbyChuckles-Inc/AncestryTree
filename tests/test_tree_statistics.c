#include "test_framework.h"
#include "tree_statistics.h"

TEST(test_tree_statistics_handles_null_tree)
{
    TreeStatistics stats;
    tree_statistics_init(&stats);

    ASSERT_TRUE(tree_statistics_calculate(&stats, NULL, 5U));
    ASSERT_EQ(stats.person_count, 0U);
    ASSERT_EQ(stats.living_count, 0U);
    ASSERT_EQ(stats.deceased_count, 0U);
    ASSERT_EQ(stats.generation_count, 0U);
    ASSERT_EQ(stats.lifespan_sample_count, 0U);
    ASSERT_FLOAT_NEAR((float)stats.average_lifespan_years, 0.0f, 0.0001f);
    ASSERT_EQ(stats.birth_location_count, 0U);
    ASSERT_EQ(stats.death_location_count, 0U);

    tree_statistics_reset(&stats);
}

TEST(test_tree_statistics_basic_metrics)
{
    TreeStatistics stats;
    tree_statistics_init(&stats);

    FamilyTree *tree = family_tree_create("Analytics Test");
    ASSERT_NOT_NULL(tree);

    Person *root = person_create(1U);
    ASSERT_TRUE(person_set_name(root, "Ada", NULL, "Lovelace"));
    ASSERT_TRUE(person_set_birth(root, "1900-01-01", " Paris "));
    ASSERT_TRUE(person_set_death(root, "1980-01-01", "London"));
    ASSERT_TRUE(family_tree_add_person(tree, root));

    Person *child = person_create(2U);
    ASSERT_TRUE(person_set_name(child, "Alan", NULL, "Turing"));
    ASSERT_TRUE(person_set_birth(child, "1930-05-01", "paris"));
    ASSERT_TRUE(person_set_death(child, "2000-05-01", " london"));
    ASSERT_TRUE(family_tree_add_person(tree, child));

    Person *grandchild = person_create(3U);
    ASSERT_TRUE(person_set_name(grandchild, "Grace", NULL, "Hopper"));
    ASSERT_TRUE(person_set_birth(grandchild, "1965-07-01", "New York"));
    ASSERT_TRUE(family_tree_add_person(tree, grandchild));

    ASSERT_TRUE(person_add_child(root, child));
    ASSERT_TRUE(person_add_child(child, grandchild));

    ASSERT_TRUE(tree_statistics_calculate(&stats, tree, 2U));
    ASSERT_EQ(stats.person_count, 3U);
    ASSERT_EQ(stats.living_count, 1U);
    ASSERT_EQ(stats.deceased_count, 2U);
    ASSERT_EQ(stats.generation_count, 3U);
    ASSERT_EQ(stats.lifespan_sample_count, 2U);

    double diff = stats.average_lifespan_years - 75.0;
    if (diff < 0.0)
    {
        diff = -diff;
    }
    ASSERT_TRUE(diff < 0.01);

    ASSERT_EQ(stats.birth_location_count, 2U);
    ASSERT_STREQ(stats.birth_locations[0].location, "Paris");
    ASSERT_EQ(stats.birth_locations[0].count, 2U);
    ASSERT_STREQ(stats.birth_locations[1].location, "New York");
    ASSERT_EQ(stats.birth_locations[1].count, 1U);

    ASSERT_EQ(stats.death_location_count, 1U);
    ASSERT_STREQ(stats.death_locations[0].location, "London");
    ASSERT_EQ(stats.death_locations[0].count, 2U);

    tree_statistics_reset(&stats);
    family_tree_destroy(tree);
}

void register_tree_statistics_tests(TestRegistry *registry)
{
    REGISTER_TEST(registry, test_tree_statistics_handles_null_tree);
    REGISTER_TEST(registry, test_tree_statistics_basic_metrics);
}
