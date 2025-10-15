#include "persistence.h"
#include "person.h"
#include "test_framework.h"
#include "tree.h"
#include "test_persistence_helpers.h"

#include <stdio.h>
#include <stdlib.h>
#include <string.h>

#define TEMP_PATH_BUFFER_SIZE 128

TEST(test_persistence_writes_expected_fields)
{
    FamilyTree *tree = test_build_sample_tree();
    char buffer[256];
    char path[TEMP_PATH_BUFFER_SIZE];
    test_temp_file_path(path, sizeof(path), "save.json");

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
    ASSERT_NOT_NULL(strstr(content, "\"version\": \"1.0\""));

    free(content);
    remove(path);
    family_tree_destroy(tree);
}

TEST(test_persistence_handles_invalid_path)
{
    FamilyTree *tree = test_build_sample_tree();
    char buffer[256];
    ASSERT_FALSE(persistence_tree_save(tree, "Z:/unlikely/path/tree.json", buffer, sizeof(buffer)));
    family_tree_destroy(tree);
}

TEST(test_persistence_roundtrip_load_save)
{
    char buffer[256];
    const char *sample_path = test_resolve_asset_path("assets/example_tree.json");
    ASSERT_NOT_NULL(sample_path);
    FamilyTree *tree = persistence_tree_load(sample_path, buffer, sizeof(buffer));
    ASSERT_NOT_NULL(tree);

    char temp_path[TEMP_PATH_BUFFER_SIZE];
    test_temp_file_path(temp_path, sizeof(temp_path), "roundtrip.json");
    ASSERT_TRUE(persistence_tree_save(tree, temp_path, buffer, sizeof(buffer)));

    FamilyTree *loaded = persistence_tree_load(temp_path, buffer, sizeof(buffer));
    ASSERT_NOT_NULL(loaded);

    ASSERT_EQ(loaded->person_count, tree->person_count);
    Person *first_original = family_tree_find_person(tree, 1U);
    Person *first_loaded = family_tree_find_person(loaded, 1U);
    ASSERT_NOT_NULL(first_original);
    ASSERT_NOT_NULL(first_loaded);
    ASSERT_STREQ(first_original->name.first, first_loaded->name.first);
    ASSERT_STREQ(first_original->name.last, first_loaded->name.last);

    remove(temp_path);
    family_tree_destroy(loaded);
    family_tree_destroy(tree);
}

void register_persistence_tests(TestRegistry *registry)
{
    REGISTER_TEST(registry, test_persistence_writes_expected_fields);
    REGISTER_TEST(registry, test_persistence_handles_invalid_path);
    REGISTER_TEST(registry, test_persistence_roundtrip_load_save);
}
