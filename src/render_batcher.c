#include "render_internal.h"

#include "person.h"

#include <stdbool.h>
#include <stddef.h>

static void render_batcher_grouping_internal_reset(RenderBatcherGrouping *grouping)
{
    if (!grouping)
    {
        return;
    }
    grouping->alive_nodes    = NULL;
    grouping->alive_count    = 0U;
    grouping->deceased_nodes = NULL;
    grouping->deceased_count = 0U;
    grouping->selected_node  = NULL;
    grouping->hovered_node   = NULL;
}

void render_batcher_grouping_reset(RenderBatcherGrouping *grouping)
{
    render_batcher_grouping_internal_reset(grouping);
}

static bool render_batcher_validate_inputs(const LayoutResult *layout,
                                           const LayoutNode **alive_storage, size_t alive_capacity,
                                           const LayoutNode **deceased_storage,
                                           size_t deceased_capacity)
{
    if (!layout || !alive_storage || !deceased_storage)
    {
        return false;
    }
    if (alive_capacity < layout->count || deceased_capacity < layout->count)
    {
        return false;
    }
    return true;
}

bool render_batcher_plan(const LayoutResult *layout, const Person *selected_person,
                         const Person *hovered_person, RenderBatcherGrouping *grouping,
                         const LayoutNode **alive_storage, size_t alive_capacity,
                         const LayoutNode **deceased_storage, size_t deceased_capacity,
                         const unsigned char *visibility_mask, size_t visibility_count)
{
    if (!grouping)
    {
        return false;
    }
    render_batcher_grouping_internal_reset(grouping);
    if (!render_batcher_validate_inputs(layout, alive_storage, alive_capacity, deceased_storage,
                                        deceased_capacity))
    {
        return false;
    }

    grouping->alive_nodes    = alive_storage;
    grouping->deceased_nodes = deceased_storage;
    grouping->alive_count    = 0U;
    grouping->deceased_count = 0U;

    if (!layout || layout->count == 0U)
    {
        return true;
    }

    for (size_t index = 0; index < layout->count; ++index)
    {
        const LayoutNode *node = &layout->nodes[index];
        const Person *person   = node ? node->person : NULL;
        if (!node || !person)
        {
            continue;
        }
        if (visibility_mask && index < visibility_count && visibility_mask[index] == 0U)
        {
            continue;
        }
        bool is_selected = (selected_person && person == selected_person);
        bool is_hovered  = (hovered_person && person == hovered_person);
        if (is_selected)
        {
            grouping->selected_node = node;
            continue;
        }
        if (is_hovered)
        {
            grouping->hovered_node = node;
            continue;
        }
        if (person->is_alive)
        {
            grouping->alive_nodes[grouping->alive_count++] = node;
        }
        else
        {
            grouping->deceased_nodes[grouping->deceased_count++] = node;
        }
    }

    if (grouping->hovered_node && grouping->selected_node == grouping->hovered_node)
    {
        grouping->hovered_node = NULL;
    }

    return true;
}
