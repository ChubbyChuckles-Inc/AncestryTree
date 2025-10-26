#include "status_messages.h"

#include <stdio.h>

static const char *status_message_detail(const char *detail)
{
    if (detail && detail[0] != '\0')
    {
        return detail;
    }
    return "An unspecified issue occurred";
}

static const char *status_message_path(const char *path)
{
    if (path && path[0] != '\0')
    {
        return path;
    }
    return "(unspecified path)";
}

bool status_message_format_save_error(const char *path, const char *detail, char *buffer,
                                      size_t capacity)
{
    if (!buffer || capacity == 0U)
    {
        return false;
    }
    int written = snprintf(buffer, capacity,
                           "Could not save the family tree to '%s'. %s. "
                           "Try choosing a different folder, verify you have write access, and "
                           "confirm there is enough free disk space.",
                           status_message_path(path), status_message_detail(detail));
    if (written < 0)
    {
        buffer[0] = '\0';
        return false;
    }
    if ((size_t)written >= capacity)
    {
        buffer[capacity - 1U] = '\0';
    }
    return true;
}

bool status_message_format_load_error(const char *path, const char *detail, char *buffer,
                                      size_t capacity)
{
    if (!buffer || capacity == 0U)
    {
        return false;
    }
    int written = snprintf(buffer, capacity,
                           "Unable to load the family tree from '%s'. %s. "
                           "Check that the file exists, is not locked by another application, "
                           "and contains valid AncestryTree JSON data.",
                           status_message_path(path), status_message_detail(detail));
    if (written < 0)
    {
        buffer[0] = '\0';
        return false;
    }
    if ((size_t)written >= capacity)
    {
        buffer[capacity - 1U] = '\0';
    }
    return true;
}
