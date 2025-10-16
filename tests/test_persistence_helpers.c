#include "test_persistence_helpers.h"

#include "person.h"

#include <stdio.h>
#include <stdlib.h>
#include <string.h>

#define TEMP_PATH_PREFIX "tree_save_test"

FamilyTree *test_build_sample_tree(void)
{
    FamilyTree *tree = family_tree_create("Sample Tree");
    if (!tree)
    {
        return NULL;
    }
    (void)family_tree_set_creation_date(tree, "2025-10-15");

    Person *root = person_create(1U);
    if (!root)
    {
        family_tree_destroy(tree);
        return NULL;
    }
    person_set_name(root, "Ada", "", "Lovelace");
    person_set_birth(root, "1815-12-10", "London");

    Person *child = person_create(2U);
    if (!child)
    {
        family_tree_destroy(tree);
        return NULL;
    }
    person_set_name(child, "Byron", NULL, "Lovelace");
    person_set_birth(child, "1836-05-12", "London");

    person_add_child(root, child);
    family_tree_add_person(tree, root);
    family_tree_add_person(tree, child);

    return tree;
}

void test_temp_file_path(char *buffer, size_t buffer_size, const char *suffix)
{
    static unsigned counter = 0U;
    if (!buffer || buffer_size == 0U)
    {
        return;
    }
    ++counter;
    (void)snprintf(buffer, buffer_size, "%s_%u_%s", TEMP_PATH_PREFIX, counter, suffix ? suffix : "tmp");
}

static bool test_try_open(const char *path)
{
    if (!path)
    {
        return false;
    }
#if defined(_MSC_VER)
    FILE *stream = NULL;
    if (fopen_s(&stream, path, "rb") != 0)
    {
        return false;
    }
#else
    FILE *stream = fopen(path, "rb");
    if (!stream)
    {
        return false;
    }
#endif
    fclose(stream);
    return true;
}

bool test_file_exists(const char *path)
{
    return test_try_open(path);
}

bool test_delete_file(const char *path)
{
    if (!path)
    {
        return false;
    }
    return remove(path) == 0;
}

bool test_write_text_file(const char *path, const char *content)
{
    if (!path || !content)
    {
        return false;
    }
#if defined(_MSC_VER)
    FILE *stream = NULL;
    if (fopen_s(&stream, path, "wb") != 0)
    {
        return false;
    }
#else
    FILE *stream = fopen(path, "wb");
    if (!stream)
    {
        return false;
    }
#endif
    size_t length = strlen(content);
    size_t written = fwrite(content, 1U, length, stream);
    fclose(stream);
    return written == length;
}

const char *test_resolve_asset_path(const char *relative_path)
{
    static char resolved[260];
    const char *prefixes[] = {"", "../", "../../"};
    for (size_t index = 0U; index < sizeof(prefixes) / sizeof(prefixes[0]); ++index)
    {
        const char *prefix = prefixes[index];
        const char *candidate = relative_path;
        if (prefix[0] != '\0')
        {
            (void)snprintf(resolved, sizeof(resolved), "%s%s", prefix, relative_path);
            candidate = resolved;
        }
        if (test_try_open(candidate))
        {
            return candidate;
        }
    }
    return NULL;
}
