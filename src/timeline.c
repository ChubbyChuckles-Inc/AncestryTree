#include "timeline.h"

#include "at_date.h"
#include "at_memory.h"
#include "at_string.h"

#include <stdio.h>
#include <stdlib.h>
#include <string.h>

static bool timeline_replace_string(char **target, const char *value)
{
    if (!target)
    {
        return false;
    }
    if (!value)
    {
        AT_FREE(*target);
        *target = NULL;
        return true;
    }
    char *copy = at_string_dup(value);
    if (!copy)
    {
        return false;
    }
    AT_FREE(*target);
    *target = copy;
    return true;
}

static bool timeline_ensure_media_capacity(TimelineEntry *entry)
{
    if (!entry)
    {
        return false;
    }
    if (entry->media_count < entry->media_capacity)
    {
        return true;
    }
    size_t new_capacity = (entry->media_capacity == 0U) ? 4U : entry->media_capacity * 2U;
    char **new_paths = at_secure_realloc(entry->media_paths, new_capacity, sizeof(char *));
    if (!new_paths)
    {
        return false;
    }
    entry->media_paths = new_paths;
    entry->media_capacity = new_capacity;
    return true;
}

void timeline_entry_init(TimelineEntry *entry, TimelineEventType type)
{
    if (!entry)
    {
        return;
    }
    entry->type = type;
    entry->date = NULL;
    entry->description = NULL;
    entry->location = NULL;
    entry->media_paths = NULL;
    entry->media_count = 0U;
    entry->media_capacity = 0U;
}

void timeline_entry_reset(TimelineEntry *entry)
{
    if (!entry)
    {
        return;
    }
    AT_FREE(entry->date);
    AT_FREE(entry->description);
    AT_FREE(entry->location);
    for (size_t index = 0; index < entry->media_count; ++index)
    {
        AT_FREE(entry->media_paths[index]);
    }
    AT_FREE(entry->media_paths);
    timeline_entry_init(entry, entry->type);
}

bool timeline_entry_set_date(TimelineEntry *entry, const char *value)
{
    if (value && !at_date_is_valid_year_or_date(value))
    {
        return false;
    }
    return timeline_replace_string(entry ? &entry->date : NULL, value);
}

bool timeline_entry_set_description(TimelineEntry *entry, const char *value)
{
    return timeline_replace_string(entry ? &entry->description : NULL, value);
}

bool timeline_entry_set_location(TimelineEntry *entry, const char *value)
{
    return timeline_replace_string(entry ? &entry->location : NULL, value);
}

bool timeline_entry_add_media(TimelineEntry *entry, const char *path)
{
    if (!entry || !path || path[0] == '\0')
    {
        return false;
    }
    if (!timeline_ensure_media_capacity(entry))
    {
        return false;
    }
    char *copy = at_string_dup(path);
    if (!copy)
    {
        return false;
    }
    entry->media_paths[entry->media_count++] = copy;
    return true;
}

bool timeline_entry_clone(const TimelineEntry *source, TimelineEntry *destination)
{
    if (!source || !destination)
    {
        return false;
    }
    timeline_entry_init(destination, source->type);
    if (!timeline_entry_set_date(destination, source->date))
    {
        timeline_entry_reset(destination);
        return false;
    }
    if (!timeline_entry_set_description(destination, source->description))
    {
        timeline_entry_reset(destination);
        return false;
    }
    if (!timeline_entry_set_location(destination, source->location))
    {
        timeline_entry_reset(destination);
        return false;
    }
    for (size_t index = 0; index < source->media_count; ++index)
    {
        if (!timeline_entry_add_media(destination, source->media_paths[index]))
        {
            timeline_entry_reset(destination);
            return false;
        }
    }
    destination->type = source->type;
    return true;
}

bool timeline_entry_validate(const TimelineEntry *entry, char *error_buffer, size_t error_buffer_size)
{
    if (!entry)
    {
        if (error_buffer && error_buffer_size > 0U)
        {
            (void)snprintf(error_buffer, error_buffer_size, "Timeline entry is NULL");
        }
        return false;
    }
    if (!entry->date || entry->date[0] == '\0')
    {
        if (error_buffer && error_buffer_size > 0U)
        {
            (void)snprintf(error_buffer, error_buffer_size, "Timeline entry missing date");
        }
        return false;
    }
    if (!at_date_is_valid_year_or_date(entry->date))
    {
        if (error_buffer && error_buffer_size > 0U)
        {
            (void)snprintf(error_buffer, error_buffer_size, "Timeline entry has invalid date");
        }
        return false;
    }
    if (!entry->description || entry->description[0] == '\0')
    {
        if (error_buffer && error_buffer_size > 0U)
        {
            (void)snprintf(error_buffer, error_buffer_size, "Timeline entry missing description");
        }
        return false;
    }
    for (size_t index = 0; index < entry->media_count; ++index)
    {
        if (!entry->media_paths[index] || entry->media_paths[index][0] == '\0')
        {
            if (error_buffer && error_buffer_size > 0U)
            {
                (void)snprintf(error_buffer, error_buffer_size, "Timeline media path invalid at index %zu", index);
            }
            return false;
        }
    }
    return true;
}
