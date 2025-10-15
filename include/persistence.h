#ifndef PERSISTENCE_H
#define PERSISTENCE_H

#include "tree.h"

#include <stdbool.h>
#include <stddef.h>

#ifdef __cplusplus
extern "C"
{
#endif

    bool persistence_tree_save(const FamilyTree *tree, const char *path, char *error_buffer, size_t error_buffer_size);

#ifdef __cplusplus
}
#endif

#endif /* PERSISTENCE_H */
