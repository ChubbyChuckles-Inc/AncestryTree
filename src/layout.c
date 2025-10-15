#include "layout.h"

#include "at_memory.h"

#include <math.h>
#include <stdlib.h>
#include <string.h>

#define LAYOUT_VERTICAL_SPACING 2.5f
#define LAYOUT_HORIZONTAL_SPACING 2.0f
#define LAYOUT_DEPTH_SPACING 1.5f

static void layout_result_init(LayoutResult *result)
{
    if (!result)
    {
        return;
    }
    result->nodes = NULL;
    result->count = 0U;
}

void layout_result_destroy(LayoutResult *result)
{
    if (!result)
    {
        return;
    }
    free(result->nodes);
    result->nodes = NULL;
    result->count = 0U;
}

static size_t family_tree_count_roots(const FamilyTree *tree)
{
    if (!tree || !tree->persons)
    {
        return 0U;
    }
    size_t roots = 0U;
    for (size_t index = 0U; index < tree->person_count; ++index)
    {
        const Person *person = tree->persons[index];
        if (!person)
        {
            continue;
        }
        if (!person->parents[PERSON_PARENT_FATHER] && !person->parents[PERSON_PARENT_MOTHER])
        {
            ++roots;
        }
    }
    return roots;
}

static bool layout_allocate_nodes(LayoutResult *result, size_t count)
{
    if (!result)
    {
        return false;
    }
    result->nodes = calloc(count, sizeof(LayoutNode));
    if (!result->nodes)
    {
        return false;
    }
    result->count = count;
    return true;
}

static size_t layout_collect_generation(const FamilyTree *tree, Person **buffer, size_t capacity,
                                        const Person *const *previous_generation, size_t previous_count)
{
    size_t count = 0U;
    for (size_t index = 0U; index < previous_count; ++index)
    {
        const Person *parent = previous_generation[index];
        if (!parent)
        {
            continue;
        }
        for (size_t child_index = 0U; child_index < parent->children_count; ++child_index)
        {
            Person *child = parent->children[child_index];
            if (!child)
            {
                continue;
            }
            bool already_collected = false;
            for (size_t scan = 0U; scan < count; ++scan)
            {
                if (buffer[scan] == child)
                {
                    already_collected = true;
                    break;
                }
            }
            if (already_collected)
            {
                continue;
            }
            if (count < capacity)
            {
                buffer[count++] = child;
            }
        }
    }
    return count;
}

static void layout_assign_generation(LayoutNode *nodes, size_t start_index, Person *const *generation, size_t count,
                                     float vertical_level)
{
    if (!nodes || !generation)
    {
        return;
    }
    const float total_width = (float)(count - 1U) * LAYOUT_HORIZONTAL_SPACING;
    const float origin = -total_width / 2.0f;
    for (size_t index = 0U; index < count; ++index)
    {
        LayoutNode *node = &nodes[start_index + index];
        node->person = generation[index];
        node->position[0] = origin + (float)index * LAYOUT_HORIZONTAL_SPACING;
        node->position[1] = vertical_level;
        node->position[2] = 0.0f;
    }
}

static size_t layout_collect_all(Person **scratch, const FamilyTree *tree)
{
    if (!scratch || !tree)
    {
        return 0U;
    }
    size_t count = 0U;
    for (size_t index = 0U; index < tree->person_count; ++index)
    {
        Person *person = tree->persons[index];
        if (!person)
        {
            continue;
        }
        scratch[count++] = person;
    }
    return count;
}

LayoutResult layout_calculate(const FamilyTree *tree)
{
    LayoutResult result;
    layout_result_init(&result);
    if (!tree || tree->person_count == 0U)
    {
        return result;
    }

    if (!layout_allocate_nodes(&result, tree->person_count))
    {
        return result;
    }

    Person **scratch = calloc(tree->person_count, sizeof(Person *));
    if (!scratch)
    {
        layout_result_destroy(&result);
        return result;
    }

    size_t node_index = 0U;
    Person **current_generation = calloc(tree->person_count, sizeof(Person *));
    Person **next_generation = calloc(tree->person_count, sizeof(Person *));
    if (!current_generation || !next_generation)
    {
        free(scratch);
        free(current_generation);
        free(next_generation);
        layout_result_destroy(&result);
        return result;
    }

    size_t root_count = 0U;
    for (size_t index = 0U; index < tree->person_count; ++index)
    {
        Person *person = tree->persons[index];
        if (!person)
        {
            continue;
        }
        if (!person->parents[PERSON_PARENT_FATHER] && !person->parents[PERSON_PARENT_MOTHER])
        {
            current_generation[root_count++] = person;
        }
    }
    if (root_count == 0U)
    {
        root_count = layout_collect_all(current_generation, tree);
    }

    float level = 0.0f;
    while (root_count > 0U)
    {
        layout_assign_generation(result.nodes, node_index, current_generation, root_count, level);
        node_index += root_count;
        level -= LAYOUT_VERTICAL_SPACING;

        size_t next_count = layout_collect_generation(tree, next_generation, tree->person_count,
                                                      (const Person *const *)current_generation, root_count);
        memset(current_generation, 0, sizeof(Person *) * tree->person_count);
        memcpy(current_generation, next_generation, sizeof(Person *) * next_count);
        memset(next_generation, 0, sizeof(Person *) * tree->person_count);
        root_count = next_count;
    }

    free(scratch);
    free(current_generation);
    free(next_generation);
    return result;
}
