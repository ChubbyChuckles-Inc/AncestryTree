#include "at_string.h"

#include "at_memory.h"

#include <stdbool.h>
#include <stddef.h>
#include <string.h>

char *at_string_dup(const char *input)
{
    if (!input)
    {
        return NULL;
    }
    size_t length = strlen(input);
    char *result = at_secure_realloc(NULL, length + 1U, sizeof(char));
    if (!result)
    {
        return NULL;
    }
    memcpy(result, input, length + 1U);
    return result;
}

char *at_string_dup_range(const char *input, size_t length)
{
    if (!input)
    {
        return NULL;
    }
    char *result = at_secure_realloc(NULL, length + 1U, sizeof(char));
    if (!result)
    {
        return NULL;
    }
    memcpy(result, input, length);
    result[length] = '\0';
    return result;
}

bool at_string_copy(char *destination, size_t destination_size, const char *source)
{
    if (!destination || destination_size == 0U)
    {
        return false;
    }
    if (!source)
    {
        destination[0] = '\0';
        return true;
    }
    size_t source_length = strlen(source);
    if (source_length >= destination_size)
    {
        return false;
    }
    memcpy(destination, source, source_length + 1U);
    return true;
}

bool at_string_equals(const char *lhs, const char *rhs)
{
    if (lhs == rhs)
    {
        return true;
    }
    if (!lhs || !rhs)
    {
        return false;
    }
    return strcmp(lhs, rhs) == 0;
}

size_t at_string_length_safe(const char *value)
{
    if (!value)
    {
        return 0U;
    }
    return strlen(value);
}
