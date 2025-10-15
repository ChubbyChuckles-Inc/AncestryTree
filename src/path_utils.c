#include "path_utils.h"

#include <stddef.h>
#include <string.h>

static bool path_is_separator(char character)
{
    return character == '/' || character == '\\';
}

static void path_trim_trailing_separators(char *path)
{
    if (!path)
    {
        return;
    }
    size_t length = strlen(path);
    while (length > 0U && path_is_separator(path[length - 1U]))
    {
        path[--length] = '\0';
    }
}

static bool path_remove_last_component(char *path)
{
    if (!path)
    {
        return false;
    }
    size_t length = strlen(path);
    if (length == 0U)
    {
        return false;
    }

    size_t index = length;
    while (index > 0U && path_is_separator(path[index - 1U]))
    {
        --index;
    }
    if (index == 0U)
    {
        path[0] = '\0';
        return true;
    }
    while (index > 0U && !path_is_separator(path[index - 1U]))
    {
        --index;
    }
    if (index == 0U)
    {
        path[0] = '\0';
        return true;
    }
    while (index > 0U && path_is_separator(path[index - 1U]))
    {
        --index;
    }
    path[index] = '\0';
    return true;
}

static bool path_copy_and_normalise(char *destination, size_t capacity, const char *source)
{
    if (!destination || capacity == 0U)
    {
        return false;
    }
    if (!source)
    {
        destination[0] = '\0';
        return true;
    }
    size_t length = strlen(source);
    if (length >= capacity)
    {
        return false;
    }
    memcpy(destination, source, length);
    destination[length] = '\0';
    return true;
}

static char platform_separator(void)
{
#if defined(_WIN32)
    return '\\';
#else
    return '/';
#endif
}

bool path_join_relative(const char *base_dir, unsigned int levels_up, const char *relative_path, char *out,
                        size_t capacity)
{
    if (!out || capacity == 0U)
    {
        return false;
    }

    out[0] = '\0';
    if (!path_copy_and_normalise(out, capacity, base_dir))
    {
        return false;
    }

    for (unsigned int level = 0U; level < levels_up; ++level)
    {
        if (!path_remove_last_component(out))
        {
            return false;
        }
    }

    path_trim_trailing_separators(out);

    size_t length = strlen(out);
    char separator = platform_separator();

    if (relative_path && relative_path[0] != '\0')
    {
        if (length > 0U)
        {
            if (length + 1U >= capacity)
            {
                return false;
            }
            out[length++] = separator;
            out[length] = '\0';
        }

        for (size_t index = 0U; relative_path[index] != '\0'; ++index)
        {
            char character = relative_path[index];
            if (path_is_separator(character))
            {
                character = separator;
            }
            if (length + 1U >= capacity)
            {
                return false;
            }
            out[length++] = character;
            out[length] = '\0';
        }
    }

    return true;
}
