#ifndef TEST_PERSISTENCE_HELPERS_H
#define TEST_PERSISTENCE_HELPERS_H

#include "tree.h"

#include <stdbool.h>
#include <stddef.h>

FamilyTree *test_build_sample_tree(void);
void test_temp_file_path(char *buffer, size_t buffer_size, const char *suffix);
const char *test_resolve_asset_path(const char *relative_path);
bool test_file_exists(const char *path);
bool test_delete_file(const char *path);

#endif /* TEST_PERSISTENCE_HELPERS_H */
