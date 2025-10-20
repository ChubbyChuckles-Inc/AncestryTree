#include "test_framework.h"
#include "at_memory.h"
#include "person.h"
#include "tree.h"
#include "persistence.h"
#include "test_persistence_helpers.h"

#include <string.h>

/* Manual check: run scripts/run_memory_checks.(sh|ps1) to review full valgrind/Dr. Memory reports after changes. */

TEST(test_family_tree_person_lifecycle_releases_memory)
{
#if AT_MEMORY_ENABLE_TRACKING
    at_memory_reset_tracking();
#endif

    FamilyTree *tree = family_tree_create("Memory Check");
    ASSERT_NOT_NULL(tree);

    Person *parent = person_create(1U);
    ASSERT_NOT_NULL(parent);
    ASSERT_TRUE(person_set_name(parent, "Ada", "", "Lovelace"));
    ASSERT_TRUE(person_set_birth(parent, "1815-12-10", "London"));
    ASSERT_TRUE(family_tree_add_person(tree, parent));

    Person *child = person_create(2U);
    ASSERT_NOT_NULL(child);
    ASSERT_TRUE(person_set_name(child, "Byron", "", "Lovelace"));
    ASSERT_TRUE(person_set_birth(child, "1835-07-10", "London"));
    ASSERT_TRUE(person_add_child(parent, child));
    ASSERT_TRUE(person_set_parent(child, parent, PERSON_PARENT_MOTHER));
    ASSERT_TRUE(family_tree_add_person(tree, child));

    char validation_buffer[256];
    memset(validation_buffer, 0, sizeof(validation_buffer));
    ASSERT_TRUE(family_tree_validate(tree, validation_buffer, sizeof(validation_buffer)));

    family_tree_destroy(tree);

#if AT_MEMORY_ENABLE_TRACKING
    ASSERT_EQ(at_memory_outstanding_allocations(), 0U);
    ASSERT_EQ(at_memory_outstanding_bytes(), 0U);
#endif
}

TEST(test_persistence_load_destroy_releases_memory)
{
#if AT_MEMORY_ENABLE_TRACKING
    at_memory_reset_tracking();
#endif

    char error_buffer[256];
    const char *asset_path = test_resolve_asset_path("assets/example_tree.json");
    ASSERT_NOT_NULL(asset_path);

    FamilyTree *tree = persistence_tree_load(asset_path, error_buffer, sizeof(error_buffer));
    ASSERT_NOT_NULL(tree);

    family_tree_destroy(tree);

#if AT_MEMORY_ENABLE_TRACKING
    ASSERT_EQ(at_memory_outstanding_allocations(), 0U);
    ASSERT_EQ(at_memory_outstanding_bytes(), 0U);
#endif
}

void register_memory_suite_tests(TestRegistry *registry)
{
    REGISTER_TEST(registry, test_family_tree_person_lifecycle_releases_memory);
    REGISTER_TEST(registry, test_persistence_load_destroy_releases_memory);
}
