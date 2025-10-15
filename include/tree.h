#ifndef TREE_H
#define TREE_H

#include "person.h"

#include <stdbool.h>
#include <stddef.h>
#include <stdint.h>

typedef struct FamilyTree
{
    char *name;
    char *creation_date;
    Person **persons;
    size_t person_count;
    size_t person_capacity;
} FamilyTree;

FamilyTree *family_tree_create(const char *name);
void family_tree_destroy(FamilyTree *tree);

bool family_tree_set_creation_date(FamilyTree *tree, const char *creation_date_iso8601);
bool family_tree_add_person(FamilyTree *tree, Person *person);
Person *family_tree_find_person(const FamilyTree *tree, uint32_t id);
bool family_tree_remove_person(FamilyTree *tree, uint32_t id);
bool family_tree_validate(const FamilyTree *tree, char *error_buffer, size_t error_buffer_size);

#endif /* TREE_H */
