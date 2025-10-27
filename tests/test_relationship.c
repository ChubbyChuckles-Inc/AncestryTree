#include "relationship.h"
#include "test_framework.h"

#include "person.h"
#include "tree.h"

#include <string.h>

static FamilyTree *relationship_build_test_tree(void)
{
    FamilyTree *tree = family_tree_create("Relationship Test");
    if (!tree)
    {
        return NULL;
    }

    Person *alice  = person_create(1U);
    Person *bob    = person_create(2U);
    Person *carol  = person_create(3U);
    Person *clara  = person_create(4U);
    Person *dave   = person_create(5U);
    Person *eva    = person_create(6U);
    Person *george = person_create(7U);
    Person *finn   = person_create(8U);

    Person *created[]          = {alice, bob, carol, clara, dave, eva, george, finn};
    const size_t created_count = sizeof(created) / sizeof(created[0]);

    for (size_t index = 0U; index < created_count; ++index)
    {
        if (!created[index])
        {
            for (size_t cleanup = 0U; cleanup < created_count; ++cleanup)
            {
                if (created[cleanup])
                {
                    person_destroy(created[cleanup]);
                }
            }
            family_tree_destroy(tree);
            return NULL;
        }
    }

    if (!person_set_name(alice, "Alice", NULL, "Hart") ||
        !person_set_name(bob, "Bob", NULL, "Hart") ||
        !person_set_name(carol, "Carol", NULL, "Hart") ||
        !person_set_name(clara, "Clara", NULL, "Hart") ||
        !person_set_name(dave, "Dave", NULL, "Nguyen") ||
        !person_set_name(eva, "Eva", NULL, "Nguyen") ||
        !person_set_name(george, "George", NULL, "Solo") ||
        !person_set_name(finn, "Finn", NULL, "Nguyen"))
    {
        for (size_t cleanup = 0U; cleanup < created_count; ++cleanup)
        {
            person_destroy(created[cleanup]);
        }
        family_tree_destroy(tree);
        return NULL;
    }

    for (size_t index = 0U; index < created_count; ++index)
    {
        if (!family_tree_add_person(tree, created[index]))
        {
            for (size_t cleanup = index; cleanup < created_count; ++cleanup)
            {
                if (created[cleanup])
                {
                    person_destroy(created[cleanup]);
                }
            }
            family_tree_destroy(tree);
            return NULL;
        }
    }

    if (!person_add_spouse(alice, bob) || !person_add_child(alice, carol) ||
        !person_add_child(bob, carol) || !person_add_child(alice, clara) ||
        !person_add_child(bob, clara) || !person_add_spouse(carol, dave) ||
        !person_add_child(carol, eva) || !person_add_child(dave, eva) ||
        !person_add_child(eva, finn))
    {
        family_tree_destroy(tree);
        return NULL;
    }

    return tree;
}

TEST(test_relationship_parent_child_path)
{
    FamilyTree *tree = relationship_build_test_tree();
    ASSERT_NOT_NULL(tree);

    RelationshipResult result;
    relationship_result_reset(&result);

    ASSERT_TRUE(relationship_calculate(tree, 1U, 3U, &result));
    ASSERT_EQ(result.step_count, 1U);
    ASSERT_EQ(result.link_types[0], RELATIONSHIP_LINK_CHILD);

    char description[160];
    ASSERT_TRUE(relationship_describe(tree, &result, description, sizeof(description)));
    ASSERT_NOT_NULL(strstr(description, "parent"));

    family_tree_destroy(tree);
}

TEST(test_relationship_siblings_description)
{
    FamilyTree *tree = relationship_build_test_tree();
    ASSERT_NOT_NULL(tree);

    RelationshipResult result;
    relationship_result_reset(&result);

    ASSERT_TRUE(relationship_calculate(tree, 3U, 4U, &result));
    ASSERT_EQ(result.step_count, 2U);
    ASSERT_EQ(result.link_types[0], RELATIONSHIP_LINK_PARENT);
    ASSERT_EQ(result.link_types[1], RELATIONSHIP_LINK_CHILD);

    char description[160];
    ASSERT_TRUE(relationship_describe(tree, &result, description, sizeof(description)));
    ASSERT_NOT_NULL(strstr(description, "siblings"));

    family_tree_destroy(tree);
}

TEST(test_relationship_grandparent_detection)
{
    FamilyTree *tree = relationship_build_test_tree();
    ASSERT_NOT_NULL(tree);

    RelationshipResult result;
    relationship_result_reset(&result);

    ASSERT_TRUE(relationship_calculate(tree, 1U, 6U, &result));
    ASSERT_EQ(result.step_count, 2U);
    ASSERT_EQ(result.link_types[0], RELATIONSHIP_LINK_CHILD);
    ASSERT_EQ(result.link_types[1], RELATIONSHIP_LINK_CHILD);

    char description[160];
    ASSERT_TRUE(relationship_describe(tree, &result, description, sizeof(description)));
    ASSERT_NOT_NULL(strstr(description, "grandparent"));

    family_tree_destroy(tree);
}

TEST(test_relationship_great_grandparent_detection)
{
    FamilyTree *tree = relationship_build_test_tree();
    ASSERT_NOT_NULL(tree);

    RelationshipResult result;
    relationship_result_reset(&result);

    ASSERT_TRUE(relationship_calculate(tree, 1U, 8U, &result));
    ASSERT_EQ(result.step_count, 3U);
    ASSERT_EQ(result.link_types[0], RELATIONSHIP_LINK_CHILD);
    ASSERT_EQ(result.link_types[1], RELATIONSHIP_LINK_CHILD);
    ASSERT_EQ(result.link_types[2], RELATIONSHIP_LINK_CHILD);

    char description[160];
    ASSERT_TRUE(relationship_describe(tree, &result, description, sizeof(description)));
    ASSERT_NOT_NULL(strstr(description, "great-grandparent"));

    family_tree_destroy(tree);
}

TEST(test_relationship_same_person)
{
    FamilyTree *tree = relationship_build_test_tree();
    ASSERT_NOT_NULL(tree);

    RelationshipResult result;
    relationship_result_reset(&result);

    ASSERT_TRUE(relationship_calculate(tree, 2U, 2U, &result));
    ASSERT_EQ(result.step_count, 0U);

    char description[120];
    ASSERT_TRUE(relationship_describe(tree, &result, description, sizeof(description)));
    ASSERT_NOT_NULL(strstr(description, "same"));

    family_tree_destroy(tree);
}

TEST(test_relationship_no_path_returns_false)
{
    FamilyTree *tree = relationship_build_test_tree();
    ASSERT_NOT_NULL(tree);

    RelationshipResult result;
    relationship_result_reset(&result);

    ASSERT_FALSE(relationship_calculate(tree, 7U, 5U, &result));

    family_tree_destroy(tree);
}

void register_relationship_tests(TestRegistry *registry)
{
    REGISTER_TEST(registry, test_relationship_parent_child_path);
    REGISTER_TEST(registry, test_relationship_siblings_description);
    REGISTER_TEST(registry, test_relationship_grandparent_detection);
    REGISTER_TEST(registry, test_relationship_great_grandparent_detection);
    REGISTER_TEST(registry, test_relationship_same_person);
    REGISTER_TEST(registry, test_relationship_no_path_returns_false);
}
