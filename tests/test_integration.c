#include "persistence.h"
#include "person.h"
#include "test_framework.h"
#include "test_persistence_helpers.h"
#include "tree.h"

#include <stdio.h>

DECLARE_TEST(test_integration_create_save_load_round_trip)
{
    FamilyTree *tree = family_tree_create("Integration Workflow");
    ASSERT_NOT_NULL(tree);

    Person *alex = person_create(100U);
    Person *blair = person_create(101U);
    Person *casey = person_create(102U);
    ASSERT_NOT_NULL(alex);
    ASSERT_NOT_NULL(blair);
    ASSERT_NOT_NULL(casey);

    ASSERT_TRUE(person_set_name(alex, "Alex", NULL, "Integration"));
    ASSERT_TRUE(person_set_birth(alex, "1970-01-01", "Mars Colony"));
    ASSERT_TRUE(person_set_death(alex, "2024-05-01", "Mars Colony"));

    ASSERT_TRUE(person_set_name(blair, "Blair", NULL, "Integration"));
    ASSERT_TRUE(person_set_birth(blair, "1972-05-16", "Mars Colony"));
    blair->is_alive = true;

    ASSERT_TRUE(person_set_name(casey, "Casey", NULL, "Integration"));
    ASSERT_TRUE(person_set_birth(casey, "1998-09-23", "Mars Colony"));
    casey->is_alive = true;

    ASSERT_TRUE(family_tree_add_person(tree, alex));
    ASSERT_TRUE(family_tree_add_person(tree, blair));
    ASSERT_TRUE(family_tree_add_person(tree, casey));

    ASSERT_TRUE(person_add_child(alex, casey));
    ASSERT_TRUE(person_add_child(blair, casey));
    ASSERT_TRUE(person_set_parent(casey, alex, PERSON_PARENT_FATHER));
    ASSERT_TRUE(person_set_parent(casey, blair, PERSON_PARENT_MOTHER));

    char path_buffer[128];
    test_temp_file_path(path_buffer, sizeof(path_buffer), "integration_output.json");
    char error_buffer[256];
    error_buffer[0] = '\0';
    char validation_error[256];
    validation_error[0] = '\0';
    if (!family_tree_validate(tree, validation_error, sizeof(validation_error)))
    {
        printf("family_tree_validate failed: %s\n", validation_error);
        ASSERT_TRUE(false);
    }
    ASSERT_TRUE(persistence_tree_save(tree, path_buffer, error_buffer, sizeof(error_buffer)));

    FamilyTree *loaded = persistence_tree_load(path_buffer, error_buffer, sizeof(error_buffer));
    ASSERT_NOT_NULL(loaded);
    ASSERT_EQ(loaded->person_count, 3U);

    Person *loaded_casey = family_tree_find_person(loaded, 102U);
    ASSERT_NOT_NULL(loaded_casey);
    ASSERT_NOT_NULL(loaded_casey->parents[PERSON_PARENT_FATHER]);
    ASSERT_NOT_NULL(loaded_casey->parents[PERSON_PARENT_MOTHER]);
    ASSERT_EQ(loaded_casey->parents[PERSON_PARENT_FATHER]->id, 100U);
    ASSERT_EQ(loaded_casey->parents[PERSON_PARENT_MOTHER]->id, 101U);

    ASSERT_EQ(remove(path_buffer), 0);

    family_tree_destroy(tree);
    family_tree_destroy(loaded);
}

void register_integration_tests(TestRegistry *registry)
{
    REGISTER_TEST(registry, test_integration_create_save_load_round_trip);
}
