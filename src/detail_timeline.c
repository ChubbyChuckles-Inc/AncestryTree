#include "detail_timeline.h"

#include "at_memory.h"
#include "person.h"
#include "timeline.h"

#include <ctype.h>
#include <stdlib.h>
#include <string.h>

#define DETAIL_TIMELINE_DEFAULT_SPACING 1.0f

static bool detail_timeline_ensure_capacity(DetailTimeline *timeline, size_t capacity)
{
    if (!timeline)
    {
        return false;
    }
    if (timeline->capacity >= capacity)
    {
        return true;
    }
    size_t new_capacity = (timeline->capacity == 0U) ? capacity : timeline->capacity;
    while (new_capacity < capacity)
    {
        new_capacity = (new_capacity == 0U) ? 4U : new_capacity * 2U;
    }
    DetailTimelineEvent *events =
        at_secure_realloc(timeline->events, new_capacity, sizeof(DetailTimelineEvent));
    if (!events)
    {
        return false;
    }
    timeline->events = events;
    timeline->capacity = new_capacity;
    return true;
}

static bool detail_timeline_parse_year(const char *text, int *out_year)
{
    if (!text || !out_year)
    {
        return false;
    }
    const char *ptr = text;
    while (*ptr != '\0' && !isdigit((unsigned char)*ptr) && *ptr != '-' && *ptr != '+')
    {
        ptr++;
    }
    if (*ptr == '\0')
    {
        return false;
    }
    char buffer[8];
    size_t length = 0U;
    if (*ptr == '+' || *ptr == '-')
    {
        buffer[length++] = *ptr++;
    }
    while (isdigit((unsigned char)*ptr) && length + 1U < sizeof(buffer))
    {
        buffer[length++] = *ptr++;
    }
    buffer[length] = '\0';
    if (length < 4U)
    {
        return false;
    }
    *out_year = atoi(buffer);
    return true;
}

bool detail_timeline_init(DetailTimeline *timeline)
{
    if (!timeline)
    {
        return false;
    }
    memset(timeline, 0, sizeof(*timeline));
    timeline->min_year = 0;
    timeline->max_year = 0;
    timeline->has_year_data = false;
    return true;
}

void detail_timeline_reset(DetailTimeline *timeline)
{
    if (!timeline)
    {
        return;
    }
    timeline->count = 0U;
    timeline->min_year = 0;
    timeline->max_year = 0;
    timeline->has_year_data = false;
}

void detail_timeline_shutdown(DetailTimeline *timeline)
{
    if (!timeline)
    {
        return;
    }
    detail_timeline_reset(timeline);
    AT_FREE(timeline->events);
    timeline->events = NULL;
    timeline->capacity = 0U;
}

static void detail_timeline_assign_uniform_positions(DetailTimeline *timeline)
{
    if (!timeline || timeline->count == 0U)
    {
        return;
    }
    float step = (timeline->count == 1U) ? 0.0f : DETAIL_TIMELINE_DEFAULT_SPACING / (float)(timeline->count - 1U);
    for (size_t index = 0U; index < timeline->count; ++index)
    {
        timeline->events[index].position = step * (float)index;
    }
}

static void detail_timeline_normalize_positions(DetailTimeline *timeline)
{
    if (!timeline || timeline->count == 0U)
    {
        return;
    }
    float min_pos = timeline->events[0].position;
    float max_pos = timeline->events[0].position;
    for (size_t index = 1U; index < timeline->count; ++index)
    {
        if (timeline->events[index].position < min_pos)
        {
            min_pos = timeline->events[index].position;
        }
        if (timeline->events[index].position > max_pos)
        {
            max_pos = timeline->events[index].position;
        }
    }
    float range = max_pos - min_pos;
    if (range <= 0.0f)
    {
        for (size_t index = 0U; index < timeline->count; ++index)
        {
            timeline->events[index].position = 0.0f;
        }
        return;
    }
    for (size_t index = 0U; index < timeline->count; ++index)
    {
        timeline->events[index].position = (timeline->events[index].position - min_pos) / range;
    }
}

bool detail_timeline_build(DetailTimeline *timeline, const Person *person)
{
    if (!timeline)
    {
        return false;
    }
    detail_timeline_reset(timeline);
    if (!person || person->timeline_count == 0U)
    {
        return true;
    }
    if (!detail_timeline_ensure_capacity(timeline, person->timeline_count))
    {
        return false;
    }

    timeline->min_year = 0;
    timeline->max_year = 0;
    timeline->has_year_data = false;

    for (size_t index = 0U; index < person->timeline_count; ++index)
    {
        const TimelineEntry *entry = &person->timeline_entries[index];
        DetailTimelineEvent *event = &timeline->events[index];
        event->entry = entry;
        event->year = 0;
        event->position = (float)index;
        int parsed_year = 0;
        if (entry->date && detail_timeline_parse_year(entry->date, &parsed_year))
        {
            event->year = parsed_year;
            event->position = (float)parsed_year;
            if (!timeline->has_year_data)
            {
                timeline->min_year = parsed_year;
                timeline->max_year = parsed_year;
                timeline->has_year_data = true;
            }
            else
            {
                if (parsed_year < timeline->min_year)
                {
                    timeline->min_year = parsed_year;
                }
                if (parsed_year > timeline->max_year)
                {
                    timeline->max_year = parsed_year;
                }
            }
        }
    }

    timeline->count = person->timeline_count;
    if (!timeline->has_year_data)
    {
        detail_timeline_assign_uniform_positions(timeline);
        return true;
    }

    if (timeline->min_year == timeline->max_year)
    {
        for (size_t index = 0U; index < timeline->count; ++index)
        {
            timeline->events[index].position = 0.5f;
        }
        return true;
    }

    detail_timeline_normalize_positions(timeline);
    return true;
}

const DetailTimelineEvent *detail_timeline_get_event(const DetailTimeline *timeline, size_t index)
{
    if (!timeline || index >= timeline->count)
    {
        return NULL;
    }
    return &timeline->events[index];
}

bool detail_timeline_hit_test(const DetailTimeline *timeline, float axis_start, float axis_end, float cursor_x,
                              float tolerance, size_t *out_index)
{
    if (!timeline || timeline->count == 0U || axis_start >= axis_end)
    {
        return false;
    }
    float width = axis_end - axis_start;
    for (size_t index = 0U; index < timeline->count; ++index)
    {
        const DetailTimelineEvent *event = &timeline->events[index];
        float marker_x = axis_start + width * event->position;
        if (cursor_x >= marker_x - tolerance && cursor_x <= marker_x + tolerance)
        {
            if (out_index)
            {
                *out_index = index;
            }
            return true;
        }
    }
    return false;
}
