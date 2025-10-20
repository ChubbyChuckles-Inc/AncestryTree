#ifndef RENDER_INTERNAL_H
#define RENDER_INTERNAL_H

#include "layout.h"

#include <stdbool.h>
#include <stddef.h>

struct Person;

typedef struct RenderBatcherGrouping
{
    const LayoutNode **alive_nodes;
    size_t alive_count;
    const LayoutNode **deceased_nodes;
    size_t deceased_count;
    const LayoutNode *selected_node;
    const LayoutNode *hovered_node;
} RenderBatcherGrouping;

void render_batcher_grouping_reset(RenderBatcherGrouping *grouping);

bool render_batcher_plan(const LayoutResult *layout, const struct Person *selected_person,
                         const struct Person *hovered_person, RenderBatcherGrouping *grouping,
                         const LayoutNode **alive_storage, size_t alive_capacity,
                         const LayoutNode **deceased_storage, size_t deceased_capacity,
                         const unsigned char *visibility_mask, size_t visibility_count);

#endif /* RENDER_INTERNAL_H */
