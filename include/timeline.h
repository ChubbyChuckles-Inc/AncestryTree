#ifndef TIMELINE_H
#define TIMELINE_H

#include <stdbool.h>
#include <stddef.h>

typedef enum TimelineEventType
{
    TIMELINE_EVENT_BIRTH = 0,
    TIMELINE_EVENT_MARRIAGE,
    TIMELINE_EVENT_DEATH,
    TIMELINE_EVENT_CUSTOM
} TimelineEventType;

typedef struct TimelineEntry
{
    TimelineEventType type;
    char *date;
    char *description;
    char *location;
    char **media_paths;
    size_t media_count;
    size_t media_capacity;
} TimelineEntry;

void timeline_entry_init(TimelineEntry *entry, TimelineEventType type);
void timeline_entry_reset(TimelineEntry *entry);
bool timeline_entry_set_date(TimelineEntry *entry, const char *value);
bool timeline_entry_set_description(TimelineEntry *entry, const char *value);
bool timeline_entry_set_location(TimelineEntry *entry, const char *value);
bool timeline_entry_add_media(TimelineEntry *entry, const char *path);
bool timeline_entry_clone(const TimelineEntry *source, TimelineEntry *destination);
bool timeline_entry_validate(const TimelineEntry *entry, char *error_buffer, size_t error_buffer_size);

#endif /* TIMELINE_H */
