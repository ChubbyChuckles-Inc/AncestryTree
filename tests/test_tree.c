#include "tree.h"
#include "test_framework.h"

#include <stdlib.h>
#include <string.h>

TEST(test_tree_add_person_and_find)
{
    FamilyTree *tree = family_tree_create("Test Tree");
    ASSERT_NOT_NULL(tree);

    Person *person = person_create(1U);
    ASSERT_TRUE(person_set_name(person, "Tim", NULL, "Berners-Lee"));
    ASSERT_TRUE(person_set_birth(person, "1955-06-08", "London"));

    ASSERT_TRUE(family_tree_add_person(tree, person));
    Person *found = family_tree_find_person(tree, 1U);
    ASSERT_EQ(found, person);

    family_tree_destroy(tree);
}

TEST(test_tree_prevents_duplicate_ids)
{
    FamilyTree *tree = family_tree_create("Test Tree");
    Person *first = person_create(2U);
    Person *second = person_create(2U);
    ASSERT_TRUE(person_set_name(first, "Margaret", NULL, "Hamilton"));
    ASSERT_TRUE(person_set_birth(first, "1936-08-17", "Paoli"));
    ASSERT_TRUE(person_set_name(second, "Katherine", NULL, "Johnson"));
    ASSERT_TRUE(person_set_birth(second, "1918-08-26", "White Sulphur Springs"));

    ASSERT_TRUE(family_tree_add_person(tree, first));
    ASSERT_FALSE(family_tree_add_person(tree, second));

    person_destroy(second);
    family_tree_destroy(tree);
}

TEST(test_tree_remove_person)
{
    FamilyTree *tree = family_tree_create("Test Tree");
    Person *person = person_create(12U);
    ASSERT_TRUE(person_set_name(person, "Dorothy", NULL, "Vaughan"));
    ASSERT_TRUE(person_set_birth(person, "1910-09-20", "Kansas City"));

    ASSERT_TRUE(family_tree_add_person(tree, person));
    ASSERT_TRUE(family_tree_remove_person(tree, 12U));
    ASSERT_NULL(family_tree_find_person(tree, 12U));

    family_tree_destroy(tree);
}

TEST(test_tree_relationship_validation)
{
    FamilyTree *tree = family_tree_create("Test Tree");
    Person *parent = person_create(10U);
    Person *child = person_create(11U);
    ASSERT_TRUE(person_set_name(parent, "Marie", NULL, "Curie"));
    ASSERT_TRUE(person_set_birth(parent, "1867-11-07", "Warsaw"));
    ASSERT_TRUE(person_set_name(child, "Irene", NULL, "Joliot-Curie"));
    ASSERT_TRUE(person_set_birth(child, "1897-09-12", "Paris"));

    ASSERT_TRUE(family_tree_add_person(tree, parent));
    ASSERT_TRUE(family_tree_add_person(tree, child));
    ASSERT_TRUE(person_add_child(parent, child));

    char buffer[128];
    ASSERT_TRUE(family_tree_validate(tree, buffer, sizeof(buffer)));

    child->parents[0] = NULL;
    ASSERT_FALSE(family_tree_validate(tree, buffer, sizeof(buffer)));

    family_tree_destroy(tree);
}

TEST(test_tree_detects_cycles)
{
    FamilyTree *tree = family_tree_create("Cycle Tree");
    Person *a = person_create(20U);
    Person *b = person_create(21U);
    ASSERT_TRUE(person_set_name(a, "Parent", NULL, "Zero"));
    ASSERT_TRUE(person_set_birth(a, "1950-01-01", "City"));
    ASSERT_TRUE(person_set_name(b, "Child", NULL, "One"));
    ASSERT_TRUE(person_set_birth(b, "1970-01-01", "City"));

    ASSERT_TRUE(family_tree_add_person(tree, a));
    ASSERT_TRUE(family_tree_add_person(tree, b));
    ASSERT_TRUE(person_add_child(a, b));
    ASSERT_TRUE(person_add_child(b, a));

    char buffer[128];
    ASSERT_FALSE(family_tree_validate(tree, buffer, sizeof(buffer)));

    family_tree_destroy(tree);
}

TEST(test_tree_root_detection)
{
    FamilyTree *tree = family_tree_create("Roots");
    Person *root_one = person_create(30U);
    Person *root_two = person_create(31U);
    Person *child = person_create(32U);
    ASSERT_TRUE(person_set_name(root_one, "Root", NULL, "One"));
    ASSERT_TRUE(person_set_birth(root_one, "1940-03-03", "Town"));
    ASSERT_TRUE(person_set_name(root_two, "Root", NULL, "Two"));
    ASSERT_TRUE(person_set_birth(root_two, "1945-04-04", "Town"));
    ASSERT_TRUE(person_set_name(child, "Child", NULL, "Three"));
    ASSERT_TRUE(person_set_birth(child, "1970-05-05", "Town"));

    ASSERT_TRUE(family_tree_add_person(tree, root_one));
    ASSERT_TRUE(family_tree_add_person(tree, root_two));
    ASSERT_TRUE(family_tree_add_person(tree, child));
    ASSERT_TRUE(person_add_child(root_one, child));

    Person *roots[3] = {NULL, NULL, NULL};
    size_t count = family_tree_get_roots(tree, roots, 3U);
    ASSERT_EQ(count, 2U);
    bool found_one = false;
    bool found_two = false;
    for (size_t index = 0; index < count && index < 3U; ++index)
    {
        if (roots[index] == root_one)
        {
            found_one = true;
        }
        if (roots[index] == root_two)
        {
            found_two = true;
        }
    }
    ASSERT_TRUE(found_one);
    ASSERT_TRUE(found_two);

    family_tree_destroy(tree);
}

void register_tree_tests(TestRegistry *registry)
{
    REGISTER_TEST(registry, test_tree_add_person_and_find);
    REGISTER_TEST(registry, test_tree_prevents_duplicate_ids);
    REGISTER_TEST(registry, test_tree_remove_person);
    REGISTER_TEST(registry, test_tree_relationship_validation);
    REGISTER_TEST(registry, test_tree_detects_cycles);
    REGISTER_TEST(registry, test_tree_root_detection);
}
