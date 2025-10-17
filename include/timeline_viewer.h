#ifndef TIMELINE_VIEWER_H
#define TIMELINE_VIEWER_H

#include <stddef.h>

#include "detail_view.h"

typedef struct TimelineViewEventVisual
{
    float normalized_time;
    float eased_height;
} TimelineViewEventVisual;

typedef struct TimelineViewState
{
    TimelineViewEventVisual events[DETAIL_VIEW_MAX_TIMELINE_EVENTS];
    size_t event_count;
    int hover_index;
    float hover_blend;
    float scroll_offset;
} TimelineViewState;

void timeline_reset(TimelineViewState *state);
void timeline_init(TimelineViewState *state, const struct DetailViewContent *content);
void timeline_update(TimelineViewState *state, float delta_seconds, float activation);
void timeline_render(const TimelineViewState *state, float activation, int screen_width, int screen_height,
                     const struct DetailViewContent *content);
int timeline_event_hover(TimelineViewState *state, int cursor_x, int cursor_y, int screen_width, int screen_height,
                         const struct DetailViewContent *content);

#endif /* TIMELINE_VIEWER_H */
