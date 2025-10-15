#include "persistence.h"
#include "person.h"
#include "test_framework.h"
#include "tree.h"

#include <stdio.h>
#include <stdlib.h>
#include <string.h>

static char *create_temp_path(void)
{
    static unsigned counter = 0U;
    ++counter;
    char buffer[64];
    (void)snprintf(buffer, sizeof(buffer), "tree_save_test_%u.json", counter);
    size_t length = strlen(buffer);
    char *path = malloc(length + 1U);
    if (!path)
    {
        return NULL;
    }
    memcpy(path, buffer, length + 1U);
    return path;
}

static FamilyTree *build_sample_tree(void)
{
    FamilyTree *tree = family_tree_create("Sample Tree");
    (void)family_tree_set_creation_date(tree, "2025-10-15");

    Person *root = person_create(1U);
    person_set_name(root, "Ada", "", "Lovelace");
    person_set_birth(root, "1815-12-10", "London");

    Person *child = person_create(2U);
    person_set_name(child, "Byron", NULL, "Lovelace");
    person_set_birth(child, "1836-05-12", "London");

    person_add_child(root, child);
    family_tree_add_person(tree, root);
    family_tree_add_person(tree, child);

    return tree;
}

TEST(test_persistence_writes_expected_fields)
{
    FamilyTree *tree = build_sample_tree();
    char buffer[256];
    char *path = create_temp_path();
    ASSERT_NOT_NULL(path);

    ASSERT_TRUE(persistence_tree_save(tree, path, buffer, sizeof(buffer)));

    FILE *stream = fopen(path, "rb");
    ASSERT_NOT_NULL(stream);
    fseek(stream, 0L, SEEK_END);
    long size = ftell(stream);
    rewind(stream);
    char *content = malloc((size_t)size + 1U);
    ASSERT_NOT_NULL(content);
    fread(content, 1U, (size_t)size, stream);
    content[size] = '\0';
    fclose(stream);

    ASSERT_NOT_NULL(strstr(content, "\"id\": 1"));
    ASSERT_NOT_NULL(strstr(content, "\"children\": [2]"));
    ASSERT_NOT_NULL(strstr(content, "\"root_ids\": [1]"));
    ASSERT_NOT_NULL(strstr(content, "\"birth_date\": \"1815-12-10\""));

    free(content);
    remove(path);
    free(path);
    family_tree_destroy(tree);
}

TEST(test_persistence_handles_invalid_path)
{
    FamilyTree *tree = build_sample_tree();
    char buffer[256];
    ASSERT_FALSE(persistence_tree_save(tree, "Z:/unlikely/path/tree.json", buffer, sizeof(buffer)));
    family_tree_destroy(tree);
}

void register_persistence_tests(TestRegistry *registry)
{
    REGISTER_TEST(registry, test_persistence_writes_expected_fields);
    REGISTER_TEST(registry, test_persistence_handles_invalid_path);
}
