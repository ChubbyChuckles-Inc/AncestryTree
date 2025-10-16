#ifndef DETAIL_TIMELINE_H
#define DETAIL_TIMELINE_H

#include <stdbool.h>
#include <stddef.h>

struct Person;
struct TimelineEntry;

typedef struct DetailTimelineEvent
{
    const struct TimelineEntry *entry;
    int year;
    float position;
} DetailTimelineEvent;

typedef struct DetailTimeline
{
    DetailTimelineEvent *events;
    size_t count;
    size_t capacity;
    int min_year;
    int max_year;
    bool has_year_data;
} DetailTimeline;

#ifdef __cplusplus
extern "C"
{
#endif

    bool detail_timeline_init(DetailTimeline *timeline);
    void detail_timeline_reset(DetailTimeline *timeline);
    void detail_timeline_shutdown(DetailTimeline *timeline);
    bool detail_timeline_build(DetailTimeline *timeline, const struct Person *person);
    const DetailTimelineEvent *detail_timeline_get_event(const DetailTimeline *timeline, size_t index);
    bool detail_timeline_hit_test(const DetailTimeline *timeline, float axis_start, float axis_end, float cursor_x,
                                  float tolerance, size_t *out_index);

#ifdef __cplusplus
}
#endif

#endif /* DETAIL_TIMELINE_H */
