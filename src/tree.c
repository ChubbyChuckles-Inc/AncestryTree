#include "tree.h"

#include "at_memory.h"
#include "at_string.h"

#include <stdio.h>
#include <stdlib.h>
#include <string.h>

static bool ensure_person_capacity(FamilyTree *tree)
{
    if (!tree)
    {
        return false;
    }
    if (tree->person_count < tree->person_capacity)
    {
        return true;
    }
    size_t new_capacity = (tree->person_capacity == 0U) ? 8U : tree->person_capacity * 2U;
    Person **persons = at_secure_realloc(tree->persons, new_capacity, sizeof(Person *));
    if (!persons)
    {
        return false;
    }
    tree->persons = persons;
    tree->person_capacity = new_capacity;
    return true;
}

FamilyTree *family_tree_create(const char *name)
{
    FamilyTree *tree = calloc(1U, sizeof(FamilyTree));
    if (!tree)
    {
        return NULL;
    }
    if (name)
    {
        tree->name = at_string_dup(name);
        if (!tree->name)
        {
            family_tree_destroy(tree);
            return NULL;
        }
    }
    return tree;
}

void family_tree_destroy(FamilyTree *tree)
{
    if (!tree)
    {
        return;
    }
    for (size_t index = 0; index < tree->person_count; ++index)
    {
        person_destroy(tree->persons[index]);
    }
    free(tree->persons);
    free(tree->name);
    free(tree->creation_date);
    free(tree);
}

bool family_tree_set_creation_date(FamilyTree *tree, const char *creation_date_iso8601)
{
    if (!tree)
    {
        return false;
    }
    if (!creation_date_iso8601)
    {
        free(tree->creation_date);
        tree->creation_date = NULL;
        return true;
    }
    char *copy = at_string_dup(creation_date_iso8601);
    if (!copy)
    {
        return false;
    }
    free(tree->creation_date);
    tree->creation_date = copy;
    return true;
}

static bool family_tree_contains_person(const FamilyTree *tree, const Person *person)
{
    for (size_t index = 0; index < tree->person_count; ++index)
    {
        if (tree->persons[index] == person)
        {
            return true;
        }
    }
    return false;
}

static int family_tree_index_of(const FamilyTree *tree, const Person *person)
{
    for (size_t index = 0; index < tree->person_count; ++index)
    {
        if (tree->persons[index] == person)
        {
            return (int)index;
        }
    }
    return -1;
}

static bool family_tree_detect_cycle_from(const FamilyTree *tree, size_t index, unsigned char *states,
                                          size_t *cycle_index)
{
    if (states[index] == 1U)
    {
        if (cycle_index)
        {
            *cycle_index = index;
        }
        return true;
    }
    if (states[index] == 2U)
    {
        return false;
    }

    states[index] = 1U;
    const Person *person = tree->persons[index];
    for (size_t child_idx = 0; child_idx < person->children_count; ++child_idx)
    {
        const Person *child = person->children[child_idx];
        int child_index = family_tree_index_of(tree, child);
        if (child_index < 0)
        {
            continue;
        }
        if (family_tree_detect_cycle_from(tree, (size_t)child_index, states, cycle_index))
        {
            return true;
        }
    }

    states[index] = 2U;
    return false;
}

bool family_tree_add_person(FamilyTree *tree, Person *person)
{
    if (!tree || !person)
    {
        return false;
    }
    if (person->id == 0U)
    {
        return false;
    }
    for (size_t index = 0; index < tree->person_count; ++index)
    {
        if (tree->persons[index]->id == person->id)
        {
            return false;
        }
    }
    if (!ensure_person_capacity(tree))
    {
        return false;
    }
    tree->persons[tree->person_count++] = person;
    return true;
}

Person *family_tree_find_person(const FamilyTree *tree, uint32_t id)
{
    if (!tree || id == 0U)
    {
        return NULL;
    }
    for (size_t index = 0; index < tree->person_count; ++index)
    {
        if (tree->persons[index]->id == id)
        {
            return tree->persons[index];
        }
    }
    return NULL;
}

bool family_tree_remove_person(FamilyTree *tree, uint32_t id)
{
    if (!tree || id == 0U)
    {
        return false;
    }
    for (size_t index = 0; index < tree->person_count; ++index)
    {
        if (tree->persons[index]->id == id)
        {
            person_destroy(tree->persons[index]);
            for (size_t shift = index + 1; shift < tree->person_count; ++shift)
            {
                tree->persons[shift - 1] = tree->persons[shift];
            }
            tree->person_count--;
            return true;
        }
    }
    return false;
}

size_t family_tree_get_roots(const FamilyTree *tree, Person **out_roots, size_t capacity)
{
    if (!tree)
    {
        return 0U;
    }
    size_t count = 0U;
    for (size_t index = 0; index < tree->person_count; ++index)
    {
        Person *candidate = tree->persons[index];
        bool has_parent = false;
        for (size_t parent_index = 0; parent_index < 2U; ++parent_index)
        {
            Person *parent = candidate->parents[parent_index];
            if (parent && family_tree_contains_person(tree, parent))
            {
                has_parent = true;
                break;
            }
        }
        if (!has_parent)
        {
            if (out_roots && count < capacity)
            {
                out_roots[count] = candidate;
            }
            count++;
        }
    }
    return count;
}

static bool family_tree_validate_relationships(const FamilyTree *tree, const Person *person,
                                               char *error_buffer, size_t error_buffer_size)
{
    for (size_t index = 0; index < person->children_count; ++index)
    {
        const Person *child = person->children[index];
        if (!family_tree_contains_person(tree, child))
        {
            if (error_buffer && error_buffer_size > 0U)
            {
                (void)snprintf(error_buffer, error_buffer_size, "Person %u references child outside tree",
                               person->id);
            }
            return false;
        }
        bool parent_link_found = false;
        for (size_t parent_index = 0; parent_index < 2U; ++parent_index)
        {
            if (child->parents[parent_index] == person)
            {
                parent_link_found = true;
                break;
            }
        }
        if (!parent_link_found)
        {
            if (error_buffer && error_buffer_size > 0U)
            {
                (void)snprintf(error_buffer, error_buffer_size,
                               "Child %u missing parent back-reference to %u", child->id, person->id);
            }
            return false;
        }
    }
    for (size_t index = 0; index < person->spouses_count; ++index)
    {
        const Person *spouse = person->spouses[index].partner;
        if (!spouse)
        {
            if (error_buffer && error_buffer_size > 0U)
            {
                (void)snprintf(error_buffer, error_buffer_size, "Person %u has spouse entry without partner",
                               person->id);
            }
            return false;
        }
        if (!family_tree_contains_person(tree, spouse))
        {
            if (error_buffer && error_buffer_size > 0U)
            {
                (void)snprintf(error_buffer, error_buffer_size, "Person %u references spouse outside tree",
                               person->id);
            }
            return false;
        }
        bool reciprocal = false;
        for (size_t spouse_index = 0; spouse_index < spouse->spouses_count; ++spouse_index)
        {
            if (spouse->spouses[spouse_index].partner == person)
            {
                reciprocal = true;
                break;
            }
        }
        if (!reciprocal)
        {
            if (error_buffer && error_buffer_size > 0U)
            {
                (void)snprintf(error_buffer, error_buffer_size,
                               "Spouse relationship not reciprocal between %u and %u", person->id, spouse->id);
            }
            return false;
        }
    }
    for (size_t index = 0; index < 2U; ++index)
    {
        const Person *parent = person->parents[index];
        if (parent && !family_tree_contains_person(tree, parent))
        {
            if (error_buffer && error_buffer_size > 0U)
            {
                (void)snprintf(error_buffer, error_buffer_size, "Person %u has parent outside tree", person->id);
            }
            return false;
        }
    }
    return true;
}

bool family_tree_validate(const FamilyTree *tree, char *error_buffer, size_t error_buffer_size)
{
    if (!tree)
    {
        if (error_buffer && error_buffer_size > 0U)
        {
            (void)snprintf(error_buffer, error_buffer_size, "Family tree pointer is NULL");
        }
        return false;
    }
    for (size_t index = 0; index < tree->person_count; ++index)
    {
        Person *person = tree->persons[index];
        if (!person_validate(person, error_buffer, error_buffer_size))
        {
            return false;
        }
        if (!family_tree_validate_relationships(tree, person, error_buffer, error_buffer_size))
        {
            return false;
        }
    }
    if (tree->person_count > 0U)
    {
        unsigned char *states = calloc(tree->person_count, sizeof(unsigned char));
        if (!states)
        {
            if (error_buffer && error_buffer_size > 0U)
            {
                (void)snprintf(error_buffer, error_buffer_size, "Insufficient memory for tree validation");
            }
            return false;
        }
        size_t cycle_index = 0U;
        for (size_t index = 0; index < tree->person_count; ++index)
        {
            if (family_tree_detect_cycle_from(tree, index, states, &cycle_index))
            {
                if (error_buffer && error_buffer_size > 0U)
                {
                    (void)snprintf(error_buffer, error_buffer_size, "Cycle detected involving person %u",
                                   tree->persons[cycle_index]->id);
                }
                free(states);
                return false;
            }
        }
        free(states);
    }
    return true;
}
