#include "relationship.h"

#include "at_memory.h"
#include "person.h"
#include "tree.h"

#include <stddef.h>
#include <stdio.h>
#include <string.h>

typedef struct RelationshipQueue
{
    size_t head;
    size_t tail;
    size_t capacity;
    size_t *items;
} RelationshipQueue;

static size_t relationship_index_of(const FamilyTree *tree, const Person *person)
{
    if (!tree || !person)
    {
        return SIZE_MAX;
    }
    for (size_t index = 0U; index < tree->person_count; ++index)
    {
        if (tree->persons[index] == person)
        {
            return index;
        }
    }
    return SIZE_MAX;
}

static bool relationship_queue_init(RelationshipQueue *queue, size_t capacity)
{
    if (!queue || capacity == 0U)
    {
        return false;
    }
    queue->items = (size_t *)AT_CALLOC(capacity, sizeof(size_t));
    if (!queue->items)
    {
        return false;
    }
    queue->capacity = capacity;
    queue->head     = 0U;
    queue->tail     = 0U;
    return true;
}

static void relationship_queue_shutdown(RelationshipQueue *queue)
{
    if (!queue)
    {
        return;
    }
    if (queue->items)
    {
        AT_FREE(queue->items);
        queue->items = NULL;
    }
    queue->capacity = 0U;
    queue->head     = 0U;
    queue->tail     = 0U;
}

static bool relationship_queue_push(RelationshipQueue *queue, size_t value)
{
    if (!queue || !queue->items)
    {
        return false;
    }
    if (queue->tail >= queue->capacity)
    {
        return false;
    }
    queue->items[queue->tail++] = value;
    return true;
}

static bool relationship_queue_pop(RelationshipQueue *queue, size_t *out_value)
{
    if (!queue || !queue->items || queue->head >= queue->tail)
    {
        return false;
    }
    if (out_value)
    {
        *out_value = queue->items[queue->head];
    }
    queue->head += 1U;
    return true;
}

void relationship_result_reset(RelationshipResult *result)
{
    if (!result)
    {
        return;
    }
    memset(result->chain, 0, sizeof(result->chain));
    memset(result->link_types, 0, sizeof(result->link_types));
    result->step_count = 0U;
}

static void relationship_cleanup_buffers(bool *visited, size_t *previous,
                                         RelationshipLinkType *previous_link)
{
    if (visited)
    {
        AT_FREE(visited);
    }
    if (previous)
    {
        AT_FREE(previous);
    }
    if (previous_link)
    {
        AT_FREE(previous_link);
    }
}

static size_t relationship_find_index_by_id(const FamilyTree *tree, uint32_t person_id)
{
    if (!tree)
    {
        return SIZE_MAX;
    }
    for (size_t index = 0U; index < tree->person_count; ++index)
    {
        Person *person = tree->persons[index];
        if (person && person->id == person_id)
        {
            return index;
        }
    }
    return SIZE_MAX;
}

static void relationship_copy_result(const FamilyTree *tree, const size_t *ordered_indices,
                                     size_t ordered_count,
                                     const RelationshipLinkType *previous_link,
                                     RelationshipResult *result)
{
    if (!tree || !ordered_indices || ordered_count == 0U || !result)
    {
        return;
    }

    relationship_result_reset(result);
    for (size_t index = 0U; index < ordered_count; ++index)
    {
        size_t person_index = ordered_indices[index];
        Person *person = (person_index < tree->person_count) ? tree->persons[person_index] : NULL;
        result->chain[index] = person ? person->id : 0U;
        if (index > 0U)
        {
            size_t node_index = ordered_indices[index];
            result->link_types[index - 1U] =
                previous_link ? previous_link[node_index] : RELATIONSHIP_LINK_PARENT;
        }
    }
    result->step_count = (ordered_count > 0U) ? (ordered_count - 1U) : 0U;
}

bool relationship_calculate(const FamilyTree *tree, uint32_t source_id, uint32_t target_id,
                            RelationshipResult *result)
{
    if (!tree || tree->person_count == 0U)
    {
        return false;
    }
    size_t person_count = tree->person_count;
    if (person_count > SIZE_MAX / sizeof(bool))
    {
        return false;
    }

    const size_t source_index = relationship_find_index_by_id(tree, source_id);
    const size_t target_index = relationship_find_index_by_id(tree, target_id);
    if (source_index == SIZE_MAX || target_index == SIZE_MAX)
    {
        return false;
    }
    if (source_index == target_index)
    {
        if (result)
        {
            relationship_result_reset(result);
            result->chain[0]   = source_id;
            result->step_count = 0U;
        }
        return true;
    }

    bool *visited    = (bool *)AT_CALLOC(person_count, sizeof(bool));
    size_t *previous = (size_t *)AT_CALLOC(person_count, sizeof(size_t));
    RelationshipLinkType *previous_link =
        (RelationshipLinkType *)AT_CALLOC(person_count, sizeof(RelationshipLinkType));

    if (!visited || !previous || !previous_link)
    {
        relationship_cleanup_buffers(visited, previous, previous_link);
        return false;
    }

    for (size_t index = 0U; index < person_count; ++index)
    {
        previous[index] = SIZE_MAX;
    }

    RelationshipQueue queue;
    if (!relationship_queue_init(&queue, person_count))
    {
        relationship_cleanup_buffers(visited, previous, previous_link);
        return false;
    }

    visited[source_index]       = true;
    previous[source_index]      = SIZE_MAX;
    previous_link[source_index] = RELATIONSHIP_LINK_PARENT;
    if (!relationship_queue_push(&queue, source_index))
    {
        relationship_queue_shutdown(&queue);
        relationship_cleanup_buffers(visited, previous, previous_link);
        return false;
    }

    bool found = false;
    while (queue.head < queue.tail)
    {
        size_t current_index = 0U;
        (void)relationship_queue_pop(&queue, &current_index);
        if (current_index == target_index)
        {
            found = true;
            break;
        }

        Person *current_person = tree->persons[current_index];
        if (!current_person)
        {
            continue;
        }

        Person *parent_candidates[2] = {current_person->parents[0], current_person->parents[1]};
        for (size_t parent_slot = 0U; parent_slot < 2U; ++parent_slot)
        {
            Person *parent      = parent_candidates[parent_slot];
            size_t parent_index = relationship_index_of(tree, parent);
            if (parent_index == SIZE_MAX || visited[parent_index])
            {
                continue;
            }
            visited[parent_index]       = true;
            previous[parent_index]      = current_index;
            previous_link[parent_index] = RELATIONSHIP_LINK_PARENT;
            if (!relationship_queue_push(&queue, parent_index))
            {
                relationship_queue_shutdown(&queue);
                relationship_cleanup_buffers(visited, previous, previous_link);
                return false;
            }
        }

        for (size_t child_index = 0U; child_index < current_person->children_count; ++child_index)
        {
            Person *child        = current_person->children[child_index];
            size_t child_tree_ix = relationship_index_of(tree, child);
            if (child_tree_ix == SIZE_MAX || visited[child_tree_ix])
            {
                continue;
            }
            visited[child_tree_ix]       = true;
            previous[child_tree_ix]      = current_index;
            previous_link[child_tree_ix] = RELATIONSHIP_LINK_CHILD;
            if (!relationship_queue_push(&queue, child_tree_ix))
            {
                relationship_queue_shutdown(&queue);
                relationship_cleanup_buffers(visited, previous, previous_link);
                return false;
            }
        }

        for (size_t spouse_index = 0U; spouse_index < current_person->spouses_count; ++spouse_index)
        {
            Person *partner        = current_person->spouses[spouse_index].partner;
            size_t partner_tree_ix = relationship_index_of(tree, partner);
            if (partner_tree_ix == SIZE_MAX || visited[partner_tree_ix])
            {
                continue;
            }
            visited[partner_tree_ix]       = true;
            previous[partner_tree_ix]      = current_index;
            previous_link[partner_tree_ix] = RELATIONSHIP_LINK_SPOUSE;
            if (!relationship_queue_push(&queue, partner_tree_ix))
            {
                relationship_queue_shutdown(&queue);
                relationship_cleanup_buffers(visited, previous, previous_link);
                return false;
            }
        }
    }

    if (!found && !visited[target_index])
    {
        relationship_queue_shutdown(&queue);
        relationship_cleanup_buffers(visited, previous, previous_link);
        return false;
    }

    size_t *ordered_indices = queue.items;
    size_t ordered_count    = 0U;
    size_t walk_index       = target_index;
    while (walk_index != SIZE_MAX)
    {
        if (ordered_count >= queue.capacity)
        {
            relationship_queue_shutdown(&queue);
            relationship_cleanup_buffers(visited, previous, previous_link);
            return false;
        }
        ordered_indices[ordered_count++] = walk_index;
        if (walk_index == source_index)
        {
            break;
        }
        walk_index = previous[walk_index];
    }

    if (ordered_indices[ordered_count - 1U] != source_index)
    {
        relationship_queue_shutdown(&queue);
        relationship_cleanup_buffers(visited, previous, previous_link);
        return false;
    }

    for (size_t left = 0U, right = ordered_count - 1U; left < right; ++left, --right)
    {
        size_t temp            = ordered_indices[left];
        ordered_indices[left]  = ordered_indices[right];
        ordered_indices[right] = temp;
    }

    if (ordered_count > RELATIONSHIP_MAX_CHAIN_LENGTH)
    {
        relationship_queue_shutdown(&queue);
        relationship_cleanup_buffers(visited, previous, previous_link);
        return false;
    }

    relationship_copy_result(tree, ordered_indices, ordered_count, previous_link, result);

    relationship_queue_shutdown(&queue);
    relationship_cleanup_buffers(visited, previous, previous_link);
    return true;
}

static bool relationship_compose_display_name(const FamilyTree *tree, uint32_t person_id,
                                              char *buffer, size_t capacity)
{
    if (!buffer || capacity == 0U)
    {
        return false;
    }
    buffer[0] = '\0';
    if (!tree)
    {
        return false;
    }
    Person *person = family_tree_find_person(tree, person_id);
    if (person && person_format_display_name(person, buffer, capacity))
    {
        return true;
    }
    int written = snprintf(buffer, capacity, "Person %u", person_id);
    if (written < 0 || (size_t)written >= capacity)
    {
        if (capacity > 0U)
        {
            buffer[capacity - 1U] = '\0';
        }
        return false;
    }
    return true;
}

static const char *relationship_link_label(RelationshipLinkType type)
{
    switch (type)
    {
    case RELATIONSHIP_LINK_PARENT:
        return "parent";
    case RELATIONSHIP_LINK_CHILD:
        return "child";
    case RELATIONSHIP_LINK_SPOUSE:
        return "spouse";
    default:
        return "unknown";
    }
}

static bool relationship_all_steps_match(const RelationshipResult *result,
                                         RelationshipLinkType type)
{
    if (!result)
    {
        return false;
    }
    for (size_t index = 0U; index < result->step_count; ++index)
    {
        if (result->link_types[index] != type)
        {
            return false;
        }
    }
    return result->step_count > 0U;
}

static bool relationship_format_generation_label(size_t steps, bool source_is_ancestor,
                                                 char *buffer, size_t capacity)
{
    if (!buffer || capacity == 0U || steps < 2U)
    {
        return false;
    }

    const char *base = source_is_ancestor ? "grandparent" : "grandchild";
    size_t greats    = (steps >= 2U) ? (steps - 2U) : 0U;

    buffer[0]           = '\0';
    size_t written      = 0U;
    const char prefix[] = "great-";
    for (size_t index = 0U; index < greats; ++index)
    {
        int result = snprintf(buffer + written, capacity - written, "%s", prefix);
        if (result < 0 || (size_t)result >= capacity - written)
        {
            buffer[capacity - 1U] = '\0';
            return false;
        }
        written += (size_t)result;
    }
    int result = snprintf(buffer + written, capacity - written, "%s", base);
    if (result < 0 || (size_t)result >= capacity - written)
    {
        buffer[capacity - 1U] = '\0';
        return false;
    }
    return true;
}

static bool relationship_try_special_cases(const FamilyTree *tree, const RelationshipResult *result,
                                           char *buffer, size_t capacity)
{
    if (!tree || !result || !buffer || capacity == 0U)
    {
        return false;
    }
    buffer[0] = '\0';

    if (result->step_count == 0U)
    {
        char name[96];
        if (!relationship_compose_display_name(tree, result->chain[0], name, sizeof(name)))
        {
            return false;
        }
        int written = snprintf(buffer, capacity, "%s is the same individual.", name);
        if (written < 0 || (size_t)written >= capacity)
        {
            buffer[capacity - 1U] = '\0';
            return false;
        }
        return true;
    }

    char source_name[96];
    char target_name[96];
    relationship_compose_display_name(tree, result->chain[0], source_name, sizeof(source_name));
    relationship_compose_display_name(tree, result->chain[result->step_count], target_name,
                                      sizeof(target_name));

    if (result->step_count == 1U)
    {
        switch (result->link_types[0])
        {
        case RELATIONSHIP_LINK_CHILD:
        {
            int written =
                snprintf(buffer, capacity, "%s is a parent of %s.", source_name, target_name);
            return written >= 0 && (size_t)written < capacity;
        }
        case RELATIONSHIP_LINK_PARENT:
        {
            int written =
                snprintf(buffer, capacity, "%s is a child of %s.", source_name, target_name);
            return written >= 0 && (size_t)written < capacity;
        }
        case RELATIONSHIP_LINK_SPOUSE:
        {
            int written =
                snprintf(buffer, capacity, "%s is married to %s.", source_name, target_name);
            return written >= 0 && (size_t)written < capacity;
        }
        default:
            break;
        }
    }

    if (result->step_count == 2U && result->link_types[0] == RELATIONSHIP_LINK_PARENT &&
        result->link_types[1] == RELATIONSHIP_LINK_CHILD)
    {
        char connector[96];
        relationship_compose_display_name(tree, result->chain[1], connector, sizeof(connector));
        int written = snprintf(buffer, capacity, "%s and %s are siblings through %s.", source_name,
                               target_name, connector);
        return written >= 0 && (size_t)written < capacity;
    }

    if (result->step_count == 2U && result->link_types[0] == RELATIONSHIP_LINK_CHILD &&
        result->link_types[1] == RELATIONSHIP_LINK_PARENT)
    {
        char child_name[96];
        relationship_compose_display_name(tree, result->chain[1], child_name, sizeof(child_name));
        int written = snprintf(buffer, capacity, "%s and %s co-parent %s.", source_name,
                               target_name, child_name);
        return written >= 0 && (size_t)written < capacity;
    }

    if (relationship_all_steps_match(result, RELATIONSHIP_LINK_CHILD))
    {
        char label[128];
        if (!relationship_format_generation_label(result->step_count, true, label, sizeof(label)))
        {
            return false;
        }
        int written =
            snprintf(buffer, capacity, "%s is the %s of %s.", source_name, label, target_name);
        return written >= 0 && (size_t)written < capacity;
    }

    if (relationship_all_steps_match(result, RELATIONSHIP_LINK_PARENT))
    {
        char label[128];
        if (!relationship_format_generation_label(result->step_count, false, label, sizeof(label)))
        {
            return false;
        }
        int written =
            snprintf(buffer, capacity, "%s is the %s of %s.", source_name, label, target_name);
        return written >= 0 && (size_t)written < capacity;
    }

    return false;
}

bool relationship_describe(const FamilyTree *tree, const RelationshipResult *result, char *buffer,
                           size_t capacity)
{
    if (!tree || !result || !buffer || capacity == 0U)
    {
        return false;
    }

    if (result->step_count == 0U)
    {
        return relationship_try_special_cases(tree, result, buffer, capacity);
    }

    if (relationship_try_special_cases(tree, result, buffer, capacity))
    {
        return true;
    }

    char source_name[96];
    char target_name[96];
    relationship_compose_display_name(tree, result->chain[0], source_name, sizeof(source_name));
    relationship_compose_display_name(tree, result->chain[result->step_count], target_name,
                                      sizeof(target_name));

    char path[RELATIONSHIP_MAX_CHAIN_LENGTH * 16U];
    size_t written = 0U;
    for (size_t index = 0U; index < result->step_count; ++index)
    {
        const char *label = relationship_link_label(result->link_types[index]);
        int produced      = snprintf(path + written, sizeof(path) - written, "%s%s",
                                (index == 0U) ? "" : " -> ", label);
        if (produced < 0 || (size_t)produced >= sizeof(path) - written)
        {
            break;
        }
        written += (size_t)produced;
    }
    if (written == 0U)
    {
        (void)snprintf(path, sizeof(path), "family connections");
    }

    int description =
        snprintf(buffer, capacity, "%s reaches %s via %s.", source_name, target_name, path);
    if (description < 0 || (size_t)description >= capacity)
    {
        buffer[capacity - 1U] = '\0';
        return false;
    }
    return true;
}
