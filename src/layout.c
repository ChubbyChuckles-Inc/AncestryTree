#include "layout.h"

#include "at_memory.h"

#include <math.h>
#include <stdbool.h>
#include <stdint.h>
#include <stdlib.h>
#include <string.h>

#define LAYOUT_VERTICAL_SPACING 2.5f
#define LAYOUT_HORIZONTAL_SPACING 2.0f
#define LAYOUT_DEPTH_SPACING 1.5f

#define LAYOUT_FORCE_ITERATIONS 160U
#define LAYOUT_FORCE_TIMESTEP 0.016f
#define LAYOUT_FORCE_REPULSION 7.5f
#define LAYOUT_FORCE_SPRING 0.08f
#define LAYOUT_FORCE_DAMPING 0.82f
#define LAYOUT_FORCE_TARGET_DISTANCE 2.5f
#define LAYOUT_FORCE_LAYER_STRENGTH 0.18f

typedef struct LayoutGeneration
{
    Person **members;
    size_t count;
    float level;
} LayoutGeneration;

typedef struct LayoutLookupEntry
{
    const Person *person;
    const LayoutNode *node;
} LayoutLookupEntry;

static size_t layout_collect_all(Person **scratch, const FamilyTree *tree);
static bool layout_build_generations(const FamilyTree *tree, LayoutGeneration **out_generations,
                                     size_t *out_count);
static void layout_generations_free(LayoutGeneration *generations, size_t count);
static bool layout_list_contains(const Person *const *list, size_t count, const Person *person);
static bool layout_impact_add(const Person **buffer, size_t *count, size_t capacity,
                              const Person *person);
static const LayoutNode *layout_lookup_find(const LayoutLookupEntry *entries, size_t count,
                                            const Person *person);
static int layout_lookup_compare(const void *lhs, const void *rhs);
static void layout_assign_generation(LayoutNode *nodes, size_t start_index, Person *const *members,
                                     size_t count, float level);

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

bool layout_result_copy(LayoutResult *destination, const LayoutResult *source)
{
    if (!destination || !source)
    {
        return false;
    }

    layout_result_destroy(destination);
    if (source->count == 0U || !source->nodes)
    {
        destination->nodes = NULL;
        destination->count = 0U;
        return true;
    }

    destination->nodes = (LayoutNode *)calloc(source->count, sizeof(LayoutNode));
    if (!destination->nodes)
    {
        destination->count = 0U;
        return false;
    }
    memcpy(destination->nodes, source->nodes, source->count * sizeof(LayoutNode));
    destination->count = source->count;
    return true;
}

void layout_result_move(LayoutResult *destination, LayoutResult *source)
{
    if (!destination || !source)
    {
        return;
    }
    layout_result_destroy(destination);
    destination->nodes = source->nodes;
    destination->count = source->count;
    source->nodes      = NULL;
    source->count      = 0U;
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

void layout_cache_init(LayoutCache *cache)
{
    if (!cache)
    {
        return;
    }
    cache->count      = 0U;
    cache->next_index = 0U;
    for (size_t algo = 0U; algo < LAYOUT_ALGORITHM_COUNT; ++algo)
    {
        cache->last_valid[algo] = false;
        cache->last_index[algo] = 0U;
    }
    for (size_t index = 0U; index < LAYOUT_CACHE_MAX_ENTRIES; ++index)
    {
        cache->entries[index].algorithm    = LAYOUT_ALGORITHM_HIERARCHICAL;
        cache->entries[index].signature    = 0U;
        cache->entries[index].layout.nodes = NULL;
        cache->entries[index].layout.count = 0U;
        cache->entries[index].valid        = false;
    }
}

void layout_cache_reset(LayoutCache *cache)
{
    if (!cache)
    {
        return;
    }
    for (size_t index = 0U; index < LAYOUT_CACHE_MAX_ENTRIES; ++index)
    {
        if (cache->entries[index].valid)
        {
            layout_result_destroy(&cache->entries[index].layout);
        }
        cache->entries[index].layout.nodes = NULL;
        cache->entries[index].layout.count = 0U;
        cache->entries[index].signature    = 0U;
        cache->entries[index].algorithm    = LAYOUT_ALGORITHM_HIERARCHICAL;
        cache->entries[index].valid        = false;
    }
    cache->count      = 0U;
    cache->next_index = 0U;
    for (size_t algo = 0U; algo < LAYOUT_ALGORITHM_COUNT; ++algo)
    {
        cache->last_valid[algo] = false;
        cache->last_index[algo] = 0U;
    }
}

void layout_cache_invalidate(LayoutCache *cache) { layout_cache_reset(cache); }

static uint64_t layout_hash_mix(uint64_t hash, uint64_t value)
{
    const uint64_t prime = 1099511628211ULL;
    hash ^= value;
    hash *= prime;
    return hash;
}

static uint64_t layout_person_signature(uint64_t hash, const Person *person)
{
    if (!person)
    {
        return layout_hash_mix(hash, 0ULL);
    }

    hash = layout_hash_mix(hash, (uint64_t)person->id);
    hash = layout_hash_mix(hash, (uint64_t)(person->is_alive ? 1U : 0U));

    for (size_t parent_index = 0U; parent_index < 2U; ++parent_index)
    {
        const Person *parent = person->parents[parent_index];
        uint64_t parent_id   = parent ? (uint64_t)parent->id : 0ULL;
        hash                 = layout_hash_mix(hash, parent_id);
    }

    hash = layout_hash_mix(hash, (uint64_t)person->children_count);
    for (size_t child_index = 0U; child_index < person->children_count; ++child_index)
    {
        const Person *child = person->children[child_index];
        uint64_t child_id   = child ? (uint64_t)child->id : 0ULL;
        hash                = layout_hash_mix(hash, child_id);
    }

    hash = layout_hash_mix(hash, (uint64_t)person->spouses_count);
    for (size_t spouse_index = 0U; spouse_index < person->spouses_count; ++spouse_index)
    {
        const Person *partner = person->spouses[spouse_index].partner;
        uint64_t partner_id   = partner ? (uint64_t)partner->id : 0ULL;
        hash                  = layout_hash_mix(hash, partner_id);
    }

    return hash;
}

static uint64_t layout_tree_signature(const FamilyTree *tree)
{
    const uint64_t offset = 1469598103934665603ULL;
    if (!tree)
    {
        return offset;
    }

    uint64_t hash = layout_hash_mix(offset, (uint64_t)tree->person_count);
    for (size_t index = 0U; index < tree->person_count; ++index)
    {
        hash = layout_person_signature(hash, tree->persons[index]);
    }
    return hash;
}

static LayoutCacheEntry *layout_cache_find(LayoutCache *cache, uint64_t signature,
                                           LayoutAlgorithm algorithm)
{
    if (!cache)
    {
        return NULL;
    }
    for (size_t index = 0U; index < cache->count && index < LAYOUT_CACHE_MAX_ENTRIES; ++index)
    {
        LayoutCacheEntry *entry = &cache->entries[index];
        if (!entry->valid)
        {
            continue;
        }
        if (entry->signature == signature && entry->algorithm == algorithm)
        {
            return entry;
        }
    }
    return NULL;
}

static LayoutCacheEntry *layout_cache_store(LayoutCache *cache, LayoutAlgorithm algorithm,
                                            uint64_t signature, LayoutResult *layout)
{
    if (!cache || !layout)
    {
        return NULL;
    }

    LayoutCacheEntry *entry = NULL;
    for (size_t index = 0U; index < cache->count; ++index)
    {
        LayoutCacheEntry *candidate = &cache->entries[index];
        if (!candidate->valid || candidate->algorithm == algorithm)
        {
            entry = candidate;
            break;
        }
    }

    if (!entry)
    {
        if (cache->count < LAYOUT_CACHE_MAX_ENTRIES)
        {
            entry = &cache->entries[cache->count++];
        }
        else
        {
            size_t slot       = cache->next_index % LAYOUT_CACHE_MAX_ENTRIES;
            cache->next_index = (cache->next_index + 1U) % LAYOUT_CACHE_MAX_ENTRIES;
            entry             = &cache->entries[slot];
        }
    }

    layout_result_destroy(&entry->layout);
    layout_result_move(&entry->layout, layout);
    entry->algorithm   = algorithm;
    entry->signature   = signature;
    entry->valid       = true;
    size_t entry_index = (size_t)(entry - cache->entries);
    if (algorithm >= 0 && algorithm < LAYOUT_ALGORITHM_COUNT)
    {
        cache->last_valid[algorithm] = true;
        cache->last_index[algorithm] = entry_index;
    }
    return entry;
}

static LayoutCacheEntry *layout_cache_latest(LayoutCache *cache, LayoutAlgorithm algorithm)
{
    if (!cache || algorithm < 0 || algorithm >= LAYOUT_ALGORITHM_COUNT)
    {
        return NULL;
    }
    if (!cache->last_valid[algorithm])
    {
        return NULL;
    }
    size_t index = cache->last_index[algorithm];
    if (index >= LAYOUT_CACHE_MAX_ENTRIES)
    {
        return NULL;
    }
    LayoutCacheEntry *entry = &cache->entries[index];
    if (!entry->valid || entry->algorithm != algorithm)
    {
        return NULL;
    }
    return entry;
}

bool layout_cache_calculate(LayoutCache *cache, const FamilyTree *tree, LayoutAlgorithm algorithm,
                            LayoutResult *out)
{
    if (!out)
    {
        return false;
    }
    out->nodes = NULL;
    out->count = 0U;

    if (!tree || tree->person_count == 0U)
    {
        return true;
    }

    uint64_t signature = layout_tree_signature(tree);

    LayoutCacheEntry *entry = layout_cache_find(cache, signature, algorithm);
    if (entry)
    {
        return layout_result_copy(out, &entry->layout);
    }

    LayoutResult computed = layout_calculate_with_algorithm(tree, algorithm);
    if (!computed.nodes && computed.count > 0U)
    {
        layout_result_destroy(&computed);
        return false;
    }

    if (!cache)
    {
        out->nodes = computed.nodes;
        out->count = computed.count;
        return true;
    }

    LayoutCacheEntry *stored = layout_cache_store(cache, algorithm, signature, &computed);
    if (!stored)
    {
        layout_result_destroy(&computed);
        return false;
    }

    if (!layout_result_copy(out, &stored->layout))
    {
        return false;
    }

    return true;
}

bool layout_cache_incremental(LayoutCache *cache, const FamilyTree *tree, LayoutAlgorithm algorithm,
                              const Person *const *changed, size_t changed_count, LayoutResult *out)
{
    if (!out)
    {
        return false;
    }
    out->nodes = NULL;
    out->count = 0U;

    if (!tree || tree->person_count == 0U || !changed || changed_count == 0U)
    {
        return layout_cache_calculate(cache, tree, algorithm, out);
    }
    if (algorithm != LAYOUT_ALGORITHM_HIERARCHICAL)
    {
        return layout_cache_calculate(cache, tree, algorithm, out);
    }

    LayoutCacheEntry *previous = layout_cache_latest(cache, algorithm);
    if (!previous || !previous->valid || previous->layout.count == 0U)
    {
        return layout_cache_calculate(cache, tree, algorithm, out);
    }

    size_t impact_capacity = tree->person_count * 2U + 8U;
    const Person **impact  = (const Person **)calloc(impact_capacity, sizeof(const Person *));
    if (!impact)
    {
        return layout_cache_calculate(cache, tree, algorithm, out);
    }
    size_t impact_count = 0U;
    bool success        = true;

    for (size_t change_index = 0U; change_index < changed_count && success; ++change_index)
    {
        const Person *person = changed[change_index];
        if (!layout_impact_add(impact, &impact_count, impact_capacity, person))
        {
            success = false;
            break;
        }
        if (!person)
        {
            continue;
        }
        for (size_t parent_slot = 0U; parent_slot < 2U && success; ++parent_slot)
        {
            Person *parent = person->parents[parent_slot];
            if (!layout_impact_add(impact, &impact_count, impact_capacity, parent))
            {
                success = false;
                break;
            }
            if (parent)
            {
                for (size_t child_index = 0U; child_index < parent->children_count && success;
                     ++child_index)
                {
                    Person *sibling = parent->children[child_index];
                    if (!layout_impact_add(impact, &impact_count, impact_capacity, sibling))
                    {
                        success = false;
                    }
                }
            }
        }
        for (size_t child_index = 0U; child_index < person->children_count && success;
             ++child_index)
        {
            Person *child = person->children[child_index];
            if (!layout_impact_add(impact, &impact_count, impact_capacity, child))
            {
                success = false;
            }
        }
        for (size_t spouse_index = 0U; spouse_index < person->spouses_count && success;
             ++spouse_index)
        {
            Person *partner = person->spouses[spouse_index].partner;
            if (!layout_impact_add(impact, &impact_count, impact_capacity, partner))
            {
                success = false;
            }
            if (partner)
            {
                for (size_t partner_child = 0U; partner_child < partner->children_count && success;
                     ++partner_child)
                {
                    Person *shared_child = partner->children[partner_child];
                    if (!layout_impact_add(impact, &impact_count, impact_capacity, shared_child))
                    {
                        success = false;
                    }
                }
            }
        }
    }

    if (!success)
    {
        free(impact);
        return layout_cache_calculate(cache, tree, algorithm, out);
    }

    LayoutGeneration *generations = NULL;
    size_t generation_count       = 0U;
    if (!layout_build_generations(tree, &generations, &generation_count))
    {
        free(impact);
        return layout_cache_calculate(cache, tree, algorithm, out);
    }

    LayoutLookupEntry *lookup =
        (LayoutLookupEntry *)calloc(previous->layout.count, sizeof(LayoutLookupEntry));
    if (!lookup)
    {
        layout_generations_free(generations, generation_count);
        free(impact);
        return layout_cache_calculate(cache, tree, algorithm, out);
    }
    for (size_t index = 0U; index < previous->layout.count; ++index)
    {
        lookup[index].person = previous->layout.nodes[index].person;
        lookup[index].node   = &previous->layout.nodes[index];
    }
    if (previous->layout.count > 1U)
    {
        qsort(lookup, previous->layout.count, sizeof(LayoutLookupEntry), layout_lookup_compare);
    }

    LayoutResult incremental;
    layout_result_init(&incremental);
    if (!layout_allocate_nodes(&incremental, tree->person_count))
    {
        free(lookup);
        layout_generations_free(generations, generation_count);
        free(impact);
        return layout_cache_calculate(cache, tree, algorithm, out);
    }

    size_t node_index = 0U;
    for (size_t gen_index = 0U; gen_index < generation_count; ++gen_index)
    {
        LayoutGeneration *generation = &generations[gen_index];
        if (generation->count == 0U)
        {
            continue;
        }

        bool generation_changed = false;
        for (size_t member_index = 0U; member_index < generation->count; ++member_index)
        {
            if (layout_list_contains(impact, impact_count, generation->members[member_index]))
            {
                generation_changed = true;
                break;
            }
        }

        bool reuse_positions = !generation_changed;
        if (reuse_positions)
        {
            for (size_t member_index = 0U; member_index < generation->count; ++member_index)
            {
                const LayoutNode *previous_node = layout_lookup_find(
                    lookup, previous->layout.count, generation->members[member_index]);
                if (!previous_node)
                {
                    reuse_positions = false;
                    break;
                }
            }
        }

        if (!reuse_positions)
        {
            LayoutNode *temp = (LayoutNode *)calloc(generation->count, sizeof(LayoutNode));
            if (!temp)
            {
                layout_result_destroy(&incremental);
                free(lookup);
                layout_generations_free(generations, generation_count);
                free(impact);
                return layout_cache_calculate(cache, tree, algorithm, out);
            }
            layout_assign_generation(temp, 0U, generation->members, generation->count,
                                     generation->level);
            for (size_t member_index = 0U; member_index < generation->count; ++member_index)
            {
                incremental.nodes[node_index + member_index] = temp[member_index];
            }
            free(temp);
        }
        else
        {
            for (size_t member_index = 0U; member_index < generation->count; ++member_index)
            {
                const LayoutNode *previous_node = layout_lookup_find(
                    lookup, previous->layout.count, generation->members[member_index]);
                LayoutNode *destination = &incremental.nodes[node_index + member_index];
                destination->person     = generation->members[member_index];
                if (previous_node)
                {
                    destination->position[0] = previous_node->position[0];
                    destination->position[1] = previous_node->position[1];
                    destination->position[2] = previous_node->position[2];
                }
                else
                {
                    destination->position[0] = 0.0f;
                    destination->position[1] = generation->level;
                    destination->position[2] = 0.0f;
                }
            }
        }

        node_index += generation->count;
    }
    incremental.count = node_index;

    free(lookup);
    layout_generations_free(generations, generation_count);
    free(impact);

    uint64_t signature = layout_tree_signature(tree);
    if (!cache)
    {
        layout_result_destroy(out);
        out->nodes = incremental.nodes;
        out->count = incremental.count;
        return true;
    }

    LayoutCacheEntry *stored = layout_cache_store(cache, algorithm, signature, &incremental);
    if (!stored)
    {
        layout_result_destroy(&incremental);
        return false;
    }

    if (!layout_result_copy(out, &stored->layout))
    {
        return false;
    }

    return true;
}

static size_t layout_collect_generation(Person **buffer, size_t capacity,
                                        const Person *const *previous_generation,
                                        size_t previous_count)
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

static int layout_find_person_index(Person *const *generation, size_t count, const Person *person)
{
    if (!generation || !person)
    {
        return -1;
    }
    for (size_t index = 0U; index < count; ++index)
    {
        if (generation[index] == person)
        {
            return (int)index;
        }
    }
    return -1;
}

static const Person *layout_find_generation_spouse(Person *const *generation, size_t count,
                                                   const Person *person, const bool *assigned)
{
    if (!generation || !person)
    {
        return NULL;
    }
    for (size_t spouse_index = 0U; spouse_index < person->spouses_count; ++spouse_index)
    {
        const Person *candidate = person->spouses[spouse_index].partner;
        if (!candidate)
        {
            continue;
        }
        int index = layout_find_person_index(generation, count, candidate);
        if (index >= 0)
        {
            if (!assigned || !assigned[index])
            {
                return candidate;
            }
        }
    }
    return NULL;
}

static void layout_generations_free(LayoutGeneration *generations, size_t count)
{
    if (!generations)
    {
        return;
    }
    for (size_t index = 0U; index < count; ++index)
    {
        free(generations[index].members);
        generations[index].members = NULL;
        generations[index].count   = 0U;
        generations[index].level   = 0.0f;
    }
    free(generations);
}

static bool layout_build_generations(const FamilyTree *tree, LayoutGeneration **out_generations,
                                     size_t *out_count)
{
    if (!out_generations || !out_count)
    {
        return false;
    }
    *out_generations = NULL;
    *out_count       = 0U;
    if (!tree || tree->person_count == 0U)
    {
        return true;
    }

    LayoutGeneration *generations =
        (LayoutGeneration *)calloc(tree->person_count, sizeof(LayoutGeneration));
    if (!generations)
    {
        return false;
    }

    Person **current_generation = (Person **)calloc(tree->person_count, sizeof(Person *));
    Person **next_generation    = (Person **)calloc(tree->person_count, sizeof(Person *));
    if (!current_generation || !next_generation)
    {
        free(current_generation);
        free(next_generation);
        free(generations);
        return false;
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

    size_t generation_index = 0U;
    float level             = 0.0f;
    while (root_count > 0U)
    {
        LayoutGeneration *generation = &generations[generation_index];
        generation->count            = root_count;
        generation->level            = level;
        if (root_count > 0U)
        {
            generation->members = (Person **)calloc(root_count, sizeof(Person *));
            if (!generation->members)
            {
                layout_generations_free(generations, generation_index);
                free(current_generation);
                free(next_generation);
                return false;
            }
            memcpy(generation->members, current_generation, sizeof(Person *) * root_count);
        }
        generation_index += 1U;
        level -= LAYOUT_VERTICAL_SPACING;

        size_t next_count =
            layout_collect_generation(next_generation, tree->person_count,
                                      (const Person *const *)current_generation, root_count);
        memset(current_generation, 0, sizeof(Person *) * tree->person_count);
        memcpy(current_generation, next_generation, sizeof(Person *) * next_count);
        memset(next_generation, 0, sizeof(Person *) * tree->person_count);
        root_count = next_count;
    }

    free(current_generation);
    free(next_generation);
    *out_generations = generations;
    *out_count       = generation_index;
    return true;
}

static bool layout_list_contains(const Person *const *list, size_t count, const Person *person)
{
    if (!list || !person)
    {
        return false;
    }
    for (size_t index = 0U; index < count; ++index)
    {
        if (list[index] == person)
        {
            return true;
        }
    }
    return false;
}

static bool layout_impact_add(const Person **buffer, size_t *count, size_t capacity,
                              const Person *person)
{
    if (!buffer || !count || !person)
    {
        return true;
    }
    if (layout_list_contains(buffer, *count, person))
    {
        return true;
    }
    if (*count >= capacity)
    {
        return false;
    }
    buffer[*count] = person;
    *count += 1U;
    return true;
}

static int layout_lookup_compare(const void *lhs, const void *rhs)
{
    const LayoutLookupEntry *a = (const LayoutLookupEntry *)lhs;
    const LayoutLookupEntry *b = (const LayoutLookupEntry *)rhs;
    if (a->person < b->person)
    {
        return -1;
    }
    if (a->person > b->person)
    {
        return 1;
    }
    return 0;
}

static const LayoutNode *layout_lookup_find(const LayoutLookupEntry *entries, size_t count,
                                            const Person *person)
{
    if (!entries || count == 0U || !person)
    {
        return NULL;
    }
    LayoutLookupEntry key;
    key.person                      = person;
    key.node                        = NULL;
    const LayoutLookupEntry *result = (const LayoutLookupEntry *)bsearch(
        &key, entries, count, sizeof(LayoutLookupEntry), layout_lookup_compare);
    return result ? result->node : NULL;
}

static const LayoutNode *layout_find_node(const LayoutResult *layout, const Person *person)
{
    if (!layout || !layout->nodes || !person)
    {
        return NULL;
    }
    for (size_t index = 0U; index < layout->count; ++index)
    {
        if (layout->nodes[index].person == person)
        {
            return &layout->nodes[index];
        }
    }
    return NULL;
}

static int layout_find_node_index(const LayoutResult *layout, const Person *person)
{
    if (!layout || !layout->nodes || !person)
    {
        return -1;
    }
    for (size_t index = 0U; index < layout->count; ++index)
    {
        if (layout->nodes[index].person == person)
        {
            return (int)index;
        }
    }
    return -1;
}

static bool layout_nodes_contains(LayoutNode *nodes, size_t count, const Person *person)
{
    if (!nodes || !person)
    {
        return false;
    }
    for (size_t index = 0U; index < count; ++index)
    {
        if (nodes[index].person == person)
        {
            return true;
        }
    }
    return false;
}

typedef struct LayoutEdge
{
    size_t start;
    size_t end;
} LayoutEdge;

static bool layout_force_edge_exists(const LayoutEdge *edges, size_t count, size_t start,
                                     size_t end)
{
    for (size_t index = 0U; index < count; ++index)
    {
        if ((edges[index].start == start && edges[index].end == end) ||
            (edges[index].start == end && edges[index].end == start))
        {
            return true;
        }
    }
    return false;
}

static bool layout_force_add_edge(LayoutEdge **edges, size_t *count, size_t *capacity, size_t start,
                                  size_t end)
{
    if (!edges || !count || !capacity)
    {
        return false;
    }
    if (start == end)
    {
        return true;
    }
    if (layout_force_edge_exists(*edges, *count, start, end))
    {
        return true;
    }
    if (*count >= *capacity)
    {
        size_t new_capacity = (*capacity == 0U) ? 8U : (*capacity * 2U);
        LayoutEdge *resized = (LayoutEdge *)realloc(*edges, new_capacity * sizeof(LayoutEdge));
        if (!resized)
        {
            return false;
        }
        *edges    = resized;
        *capacity = new_capacity;
    }
    (*edges)[*count].start = start;
    (*edges)[*count].end   = end;
    *count += 1U;
    return true;
}

static void layout_assign_generation(LayoutNode *nodes, size_t start_index,
                                     Person *const *generation, size_t count, float vertical_level)
{
    if (!nodes || !generation)
    {
        return;
    }
    if (count == 0U)
    {
        return;
    }

    Person **ordered = (Person **)calloc(count, sizeof(Person *));
    bool *assigned   = (bool *)calloc(count, sizeof(bool));
    if (!ordered || !assigned)
    {
        free(ordered);
        free(assigned);
        const float total_width_fallback =
            (float)(count > 0U ? count - 1U : 0U) * LAYOUT_HORIZONTAL_SPACING;
        const float origin_fallback = -total_width_fallback / 2.0f;
        for (size_t index = 0U; index < count; ++index)
        {
            LayoutNode *node  = &nodes[start_index + index];
            node->person      = generation[index];
            node->position[0] = origin_fallback + (float)index * LAYOUT_HORIZONTAL_SPACING;
            node->position[1] = vertical_level;
            node->position[2] = 0.0f;
        }
        return;
    }

    size_t ordered_count = 0U;
    for (size_t index = 0U; index < count; ++index)
    {
        if (assigned[index])
        {
            continue;
        }
        Person *person           = generation[index];
        ordered[ordered_count++] = person;
        assigned[index]          = true;

        const Person *spouse = layout_find_generation_spouse(generation, count, person, assigned);
        if (spouse)
        {
            int spouse_index = layout_find_person_index(generation, count, spouse);
            if (spouse_index >= 0 && !assigned[spouse_index])
            {
                ordered[ordered_count++] = generation[spouse_index];
                assigned[spouse_index]   = true;
            }
        }
    }

    if (ordered_count < count)
    {
        for (size_t index = 0U; index < count && ordered_count < count; ++index)
        {
            Person *person     = generation[index];
            bool already_added = false;
            for (size_t scan = 0U; scan < ordered_count; ++scan)
            {
                if (ordered[scan] == person)
                {
                    already_added = true;
                    break;
                }
            }
            if (!already_added)
            {
                ordered[ordered_count++] = person;
            }
        }
    }

    const float total_width =
        (float)(ordered_count > 0U ? ordered_count - 1U : 0U) * LAYOUT_HORIZONTAL_SPACING;
    const float origin = -total_width / 2.0f;
    for (size_t index = 0U; index < ordered_count; ++index)
    {
        LayoutNode *node  = &nodes[start_index + index];
        node->person      = ordered[index];
        node->position[0] = origin + (float)index * LAYOUT_HORIZONTAL_SPACING;
        node->position[1] = vertical_level;
        node->position[2] = 0.0f;
    }

    free(ordered);
    free(assigned);
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

static LayoutResult layout_calculate_hierarchical_internal(const FamilyTree *tree)
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

    size_t node_index           = 0U;
    Person **current_generation = calloc(tree->person_count, sizeof(Person *));
    Person **next_generation    = calloc(tree->person_count, sizeof(Person *));
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

        size_t next_count =
            layout_collect_generation(next_generation, tree->person_count,
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

LayoutResult layout_calculate(const FamilyTree *tree)
{
    return layout_calculate_hierarchical_internal(tree);
}

static bool layout_force_prepare_edges(LayoutResult *layout, LayoutEdge **edges, size_t *edge_count,
                                       size_t *edge_capacity)
{
    if (!layout || !edges || !edge_count || !edge_capacity)
    {
        return false;
    }
    *edge_count    = 0U;
    *edge_capacity = layout->count * 4U;
    if (*edge_capacity < 8U)
    {
        *edge_capacity = 8U;
    }
    *edges = (LayoutEdge *)calloc(*edge_capacity, sizeof(LayoutEdge));
    if (!*edges)
    {
        *edge_capacity = 0U;
        return false;
    }

    bool success = true;
    for (size_t index = 0U; index < layout->count && success; ++index)
    {
        Person *person = layout->nodes[index].person;
        if (!person)
        {
            continue;
        }
        for (size_t child_index = 0U; child_index < person->children_count && success;
             ++child_index)
        {
            Person *child          = person->children[child_index];
            int child_layout_index = layout_find_node_index(layout, child);
            if (child_layout_index >= 0)
            {
                success = layout_force_add_edge(edges, edge_count, edge_capacity, index,
                                                (size_t)child_layout_index);
            }
        }
        for (size_t spouse_index = 0U; spouse_index < person->spouses_count && success;
             ++spouse_index)
        {
            const Person *partner    = person->spouses[spouse_index].partner;
            int partner_layout_index = layout_find_node_index(layout, partner);
            if (partner_layout_index >= 0)
            {
                success = layout_force_add_edge(edges, edge_count, edge_capacity, index,
                                                (size_t)partner_layout_index);
            }
        }
    }
    if (!success)
    {
        free(*edges);
        *edges         = NULL;
        *edge_count    = 0U;
        *edge_capacity = 0U;
        return false;
    }
    return true;
}

LayoutResult layout_calculate_force_directed(const FamilyTree *tree)
{
    LayoutResult result = layout_calculate_hierarchical_internal(tree);
    if (!tree || tree->person_count == 0U || result.count <= 1U)
    {
        return result;
    }

    size_t count         = result.count;
    float (*velocity)[3] = (float (*)[3])calloc(count, sizeof(float[3]));
    float (*forces)[3]   = (float (*)[3])calloc(count, sizeof(float[3]));
    float *layer_targets = (float *)calloc(count, sizeof(float));
    if (!velocity || !forces || !layer_targets)
    {
        free(velocity);
        free(forces);
        free(layer_targets);
        return result;
    }
    for (size_t index = 0U; index < count; ++index)
    {
        layer_targets[index] = result.nodes[index].position[1];
    }

    LayoutEdge *edges    = NULL;
    size_t edge_count    = 0U;
    size_t edge_capacity = 0U;
    if (!layout_force_prepare_edges(&result, &edges, &edge_count, &edge_capacity))
    {
        edge_count = 0U;
        edges      = NULL;
    }

    const float epsilon = 0.0001f;
    for (unsigned int iteration = 0U; iteration < LAYOUT_FORCE_ITERATIONS; ++iteration)
    {
        for (size_t index = 0U; index < count; ++index)
        {
            forces[index][0] = 0.0f;
            forces[index][1] = 0.0f;
            forces[index][2] = 0.0f;
        }

        for (size_t i = 0U; i < count; ++i)
        {
            for (size_t j = i + 1U; j < count; ++j)
            {
                float dx           = result.nodes[i].position[0] - result.nodes[j].position[0];
                float dy           = result.nodes[i].position[1] - result.nodes[j].position[1];
                float dz           = result.nodes[i].position[2] - result.nodes[j].position[2];
                float distance_sq  = dx * dx + dy * dy + dz * dz + epsilon;
                float distance     = sqrtf(distance_sq);
                float inv_distance = (distance > epsilon) ? (1.0f / distance) : 0.0f;
                float magnitude    = LAYOUT_FORCE_REPULSION / distance_sq;
                float fx           = dx * inv_distance * magnitude;
                float fy           = dy * inv_distance * magnitude;
                float fz           = dz * inv_distance * magnitude;
                forces[i][0] += fx;
                forces[i][1] += fy;
                forces[i][2] += fz;
                forces[j][0] -= fx;
                forces[j][1] -= fy;
                forces[j][2] -= fz;
            }
        }

        for (size_t edge_index = 0U; edge_index < edge_count; ++edge_index)
        {
            size_t start = edges[edge_index].start;
            size_t end   = edges[edge_index].end;
            if (start >= count || end >= count)
            {
                continue;
            }
            float dx           = result.nodes[end].position[0] - result.nodes[start].position[0];
            float dy           = result.nodes[end].position[1] - result.nodes[start].position[1];
            float dz           = result.nodes[end].position[2] - result.nodes[start].position[2];
            float distance_sq  = dx * dx + dy * dy + dz * dz + epsilon;
            float distance     = sqrtf(distance_sq);
            float inv_distance = (distance > epsilon) ? (1.0f / distance) : 0.0f;
            float displacement = distance - LAYOUT_FORCE_TARGET_DISTANCE;
            float magnitude    = LAYOUT_FORCE_SPRING * displacement;
            float fx           = dx * inv_distance * magnitude;
            float fy           = dy * inv_distance * magnitude;
            float fz           = dz * inv_distance * magnitude;
            forces[start][0] += fx;
            forces[start][1] += fy;
            forces[start][2] += fz;
            forces[end][0] -= fx;
            forces[end][1] -= fy;
            forces[end][2] -= fz;
        }

        for (size_t index = 0U; index < count; ++index)
        {
            float vertical_offset = result.nodes[index].position[1] - layer_targets[index];
            forces[index][1] -= vertical_offset * LAYOUT_FORCE_LAYER_STRENGTH;

            velocity[index][0] = (velocity[index][0] + forces[index][0] * LAYOUT_FORCE_TIMESTEP) *
                                 LAYOUT_FORCE_DAMPING;
            velocity[index][1] = (velocity[index][1] + forces[index][1] * LAYOUT_FORCE_TIMESTEP) *
                                 LAYOUT_FORCE_DAMPING;
            velocity[index][2] = (velocity[index][2] + forces[index][2] * LAYOUT_FORCE_TIMESTEP) *
                                 LAYOUT_FORCE_DAMPING;

            result.nodes[index].position[0] += velocity[index][0] * LAYOUT_FORCE_TIMESTEP;
            result.nodes[index].position[1] += velocity[index][1] * LAYOUT_FORCE_TIMESTEP;
            result.nodes[index].position[2] += velocity[index][2] * LAYOUT_FORCE_TIMESTEP;
        }
    }

    float center_x = 0.0f;
    float center_z = 0.0f;
    for (size_t index = 0U; index < count; ++index)
    {
        center_x += result.nodes[index].position[0];
        center_z += result.nodes[index].position[2];
    }
    center_x /= (count > 0U) ? (float)count : 1.0f;
    center_z /= (count > 0U) ? (float)count : 1.0f;

    float max_radius = 0.0f;
    for (size_t index = 0U; index < count; ++index)
    {
        result.nodes[index].position[0] -= center_x;
        result.nodes[index].position[2] -= center_z;
        result.nodes[index].position[1] = layer_targets[index];
        float radius_sq = result.nodes[index].position[0] * result.nodes[index].position[0] +
                          result.nodes[index].position[2] * result.nodes[index].position[2];
        if (radius_sq > max_radius)
        {
            max_radius = radius_sq;
        }
    }

    const float max_distance = 30.0f;
    if (max_radius > max_distance * max_distance && max_radius > 0.0f)
    {
        float scale = max_distance / sqrtf(max_radius);
        for (size_t index = 0U; index < count; ++index)
        {
            result.nodes[index].position[0] *= scale;
            result.nodes[index].position[2] *= scale;
        }
    }

    free(edges);
    free(velocity);
    free(forces);
    free(layer_targets);
    return result;
}

LayoutResult layout_calculate_with_algorithm(const FamilyTree *tree, LayoutAlgorithm algorithm)
{
    if (algorithm == LAYOUT_ALGORITHM_FORCE_DIRECTED)
    {
        return layout_calculate_force_directed(tree);
    }
    return layout_calculate_hierarchical_internal(tree);
}

bool layout_animate(const LayoutResult *from, const LayoutResult *to, float alpha,
                    LayoutResult *out)
{
    if (!out)
    {
        return false;
    }
    if (!from && !to)
    {
        layout_result_destroy(out);
        return true;
    }

    if (alpha < 0.0f)
    {
        alpha = 0.0f;
    }
    else if (alpha > 1.0f)
    {
        alpha = 1.0f;
    }

    size_t from_count = (from && from->nodes) ? from->count : 0U;
    size_t to_count   = (to && to->nodes) ? to->count : 0U;
    size_t capacity   = from_count + to_count;
    if (capacity == 0U)
    {
        layout_result_destroy(out);
        return true;
    }

    LayoutNode *nodes = (LayoutNode *)calloc(capacity, sizeof(LayoutNode));
    if (!nodes)
    {
        return false;
    }

    size_t count = 0U;
    if (to && to->nodes)
    {
        for (size_t index = 0U; index < to->count; ++index)
        {
            const LayoutNode *target_node = &to->nodes[index];
            const LayoutNode *start_node  = layout_find_node(from, target_node->person);
            float start_position[3]       = {target_node->position[0], target_node->position[1],
                                             target_node->position[2]};
            if (start_node)
            {
                start_position[0] = start_node->position[0];
                start_position[1] = start_node->position[1];
                start_position[2] = start_node->position[2];
            }
            nodes[count].person = target_node->person;
            for (size_t axis = 0U; axis < 3U; ++axis)
            {
                float target_coord          = target_node->position[axis];
                float start_coord           = start_position[axis];
                nodes[count].position[axis] = start_coord + (target_coord - start_coord) * alpha;
            }
            count += 1U;
        }
    }

    if (from && from->nodes)
    {
        for (size_t index = 0U; index < from->count; ++index)
        {
            const LayoutNode *start_node = &from->nodes[index];
            if (!layout_nodes_contains(nodes, count, start_node->person))
            {
                nodes[count] = *start_node;
                count += 1U;
            }
        }
    }

    layout_result_destroy(out);
    out->nodes = nodes;
    out->count = count;
    return true;
}
