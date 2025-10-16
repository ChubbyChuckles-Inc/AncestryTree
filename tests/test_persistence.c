#include "persistence.h"
#include "person.h"
#include "test_framework.h"
#include "at_memory.h"
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

#if defined(_MSC_VER)
    FILE *stream = NULL;
    ASSERT_EQ(fopen_s(&stream, path, "rb"), 0);
#else
    FILE *stream = fopen(path, "rb");
#endif
    ASSERT_NOT_NULL(stream);
    fseek(stream, 0L, SEEK_END);
    long size = ftell(stream);
    rewind(stream);
    char *content = (char *)AT_MALLOC((size_t)size + 1U);
    ASSERT_NOT_NULL(content);
    fread(content, 1U, (size_t)size, stream);
    content[size] = '\0';
    fclose(stream);

    ASSERT_NOT_NULL(strstr(content, "\"id\": 1"));
    ASSERT_NOT_NULL(strstr(content, "\"children\": [2]"));
    ASSERT_NOT_NULL(strstr(content, "\"root_ids\": [1]"));
    ASSERT_NOT_NULL(strstr(content, "\"birth_date\": \"1815-12-10\""));
    ASSERT_NOT_NULL(strstr(content, "\"version\": \"1.0\""));

    AT_FREE(content);
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

TEST(test_persistence_load_corrupted_file_reports_error)
{
    char buffer[256];
    char path[TEMP_PATH_BUFFER_SIZE];
    test_temp_file_path(path, sizeof(path), "corrupt.json");
    ASSERT_TRUE(test_write_text_file(path, "{this is not valid json"));

    FamilyTree *tree = persistence_tree_load(path, buffer, sizeof(buffer));
    ASSERT_NULL(tree);
    ASSERT_TRUE(strlen(buffer) > 0U);

    test_delete_file(path);
}

TEST(test_persistence_load_handles_missing_asset_paths)
{
    static const char *json_content =
        "{\n"
        "  \"metadata\": {\n"
        "    \"version\": \"1.0\",\n"
        "    \"name\": \"Missing Asset Tree\",\n"
        "    \"creation_date\": \"2025-10-16\",\n"
        "    \"root_ids\": [1]\n"
        "  },\n"
        "  \"persons\": [\n"
        "    {\n"
        "      \"id\": 1,\n"
        "      \"name\": {\n"
        "        \"first\": \"Test\",\n"
        "        \"middle\": \"\",\n"
        "        \"last\": \"Person\"\n"
        "      },\n"
        "      \"dates\": {\n"
        "        \"birth_date\": \"2000-01-01\",\n"
        "        \"birth_location\": \"Nowhere\",\n"
        "        \"death_date\": null,\n"
        "        \"death_location\": null\n"
        "      },\n"
        "      \"is_alive\": true,\n"
        "      \"parents\": [null, null],\n"
        "      \"children\": [],\n"
        "      \"spouses\": [],\n"
        "      \"certificates\": [\"assets/missing/cert.png\"],\n"
        "      \"profile_image\": \"assets/missing/profile.png\",\n"
        "      \"timeline\": [],\n"
        "      \"metadata\": {}\n"
        "    }\n"
        "  ]\n"
        "}\n";

    char path[TEMP_PATH_BUFFER_SIZE];
    test_temp_file_path(path, sizeof(path), "missing_assets.json");
    ASSERT_TRUE(test_write_text_file(path, json_content));

    char buffer[256];
    FamilyTree *tree = persistence_tree_load(path, buffer, sizeof(buffer));
    ASSERT_NOT_NULL(tree);
    Person *person = family_tree_find_person(tree, 1U);
    ASSERT_NOT_NULL(person);
    ASSERT_EQ(person->certificate_count, 1U);
    ASSERT_STREQ(person->certificate_paths[0], "assets/missing/cert.png");
    ASSERT_NOT_NULL(person->profile_image_path);
    ASSERT_STREQ(person->profile_image_path, "assets/missing/profile.png");

    family_tree_destroy(tree);
    test_delete_file(path);
}

TEST(test_persistence_load_parses_escaped_characters)
{
    static const char *json_content =
        "{\n"
        "  \"metadata\": {\n"
        "    \"version\": \"1.0\",\n"
        "    \"name\": \"Escaped Timeline Tree\",\n"
        "    \"creation_date\": \"2025-10-16\",\n"
        "    \"root_ids\": [1]\n"
        "  },\n"
        "  \"persons\": [\n"
        "    {\n"
        "      \"id\": 1,\n"
        "      \"name\": {\n"
        "        \"first\": \"Edge\",\n"
        "        \"middle\": \"\",\n"
        "        \"last\": \"Case\"\n"
        "      },\n"
        "      \"dates\": {\n"
        "        \"birth_date\": \"1990-05-05\",\n"
        "        \"birth_location\": \"Somewhere\",\n"
        "        \"death_date\": null,\n"
        "        \"death_location\": null\n"
        "      },\n"
        "      \"is_alive\": true,\n"
        "      \"parents\": [null, null],\n"
        "      \"children\": [],\n"
        "      \"spouses\": [],\n"
        "      \"certificates\": [],\n"
        "      \"profile_image\": \"\",\n"
        "      \"timeline\": [\n"
        "        {\n"
        "          \"type\": \"custom\",\n"
        "          \"date\": \"2020\",\n"
        "          \"description\": \"Line one\\n\\\"Quoted\\\" detail\",\n"
        "          \"location\": \"Virtual\",\n"
        "          \"media\": []\n"
        "        }\n"
        "      ],\n"
        "      \"metadata\": {}\n"
        "    }\n"
        "  ]\n"
        "}\n";

    char path[TEMP_PATH_BUFFER_SIZE];
    test_temp_file_path(path, sizeof(path), "escaped.json");
    ASSERT_TRUE(test_write_text_file(path, json_content));

    char buffer[256];
    FamilyTree *tree = persistence_tree_load(path, buffer, sizeof(buffer));
    ASSERT_NOT_NULL(tree);
    Person *person = family_tree_find_person(tree, 1U);
    ASSERT_NOT_NULL(person);
    ASSERT_EQ(person->timeline_count, 1U);
    ASSERT_NOT_NULL(person->timeline_entries);
    const TimelineEntry *entry = &person->timeline_entries[0];
    ASSERT_STREQ(entry->description, "Line one\n\"Quoted\" detail");
    ASSERT_STREQ(entry->location, "Virtual");

    family_tree_destroy(tree);
    test_delete_file(path);
}

void register_persistence_tests(TestRegistry *registry)
{
    REGISTER_TEST(registry, test_persistence_writes_expected_fields);
    REGISTER_TEST(registry, test_persistence_handles_invalid_path);
    REGISTER_TEST(registry, test_persistence_roundtrip_load_save);
    REGISTER_TEST(registry, test_persistence_load_corrupted_file_reports_error);
    REGISTER_TEST(registry, test_persistence_load_handles_missing_asset_paths);
    REGISTER_TEST(registry, test_persistence_load_parses_escaped_characters);
}
