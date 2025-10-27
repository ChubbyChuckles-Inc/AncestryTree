#ifndef LAYOUT_H
#define LAYOUT_H

#include "tree.h"

#include <stdbool.h>
#include <stddef.h>
#include <stdint.h>

typedef enum LayoutAlgorithm
{
    LAYOUT_ALGORITHM_HIERARCHICAL   = 0,
    LAYOUT_ALGORITHM_FORCE_DIRECTED = 1,
    LAYOUT_ALGORITHM_COUNT
} LayoutAlgorithm;

typedef struct LayoutNode
{
    Person *person;
    float position[3];
} LayoutNode;

typedef struct LayoutResult
{
    LayoutNode *nodes;
    size_t count;
} LayoutResult;

#define LAYOUT_CACHE_MAX_ENTRIES 4U

typedef struct LayoutCacheEntry
{
    LayoutAlgorithm algorithm;
    uint64_t signature;
    LayoutResult layout;
    bool valid;
} LayoutCacheEntry;

typedef struct LayoutCache
{
    LayoutCacheEntry entries[LAYOUT_CACHE_MAX_ENTRIES];
    size_t count;
    size_t next_index;
    bool last_valid[LAYOUT_ALGORITHM_COUNT];
    size_t last_index[LAYOUT_ALGORITHM_COUNT];
} LayoutCache;

void layout_result_destroy(LayoutResult *result);
bool layout_result_copy(LayoutResult *destination, const LayoutResult *source);
void layout_result_move(LayoutResult *destination, LayoutResult *source);

LayoutResult layout_calculate(const FamilyTree *tree);
LayoutResult layout_calculate_with_algorithm(const FamilyTree *tree, LayoutAlgorithm algorithm);
LayoutResult layout_calculate_force_directed(const FamilyTree *tree);

bool layout_animate(const LayoutResult *from, const LayoutResult *to, float alpha,
                    LayoutResult *out);

void layout_cache_init(LayoutCache *cache);
void layout_cache_reset(LayoutCache *cache);
void layout_cache_invalidate(LayoutCache *cache);
bool layout_cache_calculate(LayoutCache *cache, const FamilyTree *tree, LayoutAlgorithm algorithm,
                            LayoutResult *out);
bool layout_cache_incremental(LayoutCache *cache, const FamilyTree *tree, LayoutAlgorithm algorithm,
                              const Person *const *changed, size_t changed_count,
                              LayoutResult *out);

#endif /* LAYOUT_H */
