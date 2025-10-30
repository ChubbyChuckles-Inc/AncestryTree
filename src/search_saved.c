#include "search_saved.h"

#include "at_memory.h"

#include <ctype.h>
#include <errno.h>
#include <stdio.h>
#include <string.h>

#define SEARCH_SAVED_INITIAL_CAPACITY 4U

static void search_saved_set_error(char *buffer, size_t buffer_size, const char *message)
{
    if (!buffer || buffer_size == 0U)
    {
        return;
    }
#if defined(_MSC_VER)
    (void)strncpy_s(buffer, buffer_size, message ? message : "", _TRUNCATE);
#else
    if (message)
    {
        (void)snprintf(buffer, buffer_size, "%s", message);
    }
    else
    {
        buffer[0] = '\0';
    }
#endif
}

static int search_saved_casefold_char(int value) { return tolower((unsigned char)value); }

static int search_saved_casecmp(const char *lhs, const char *rhs)
{
    if (!lhs || !rhs)
    {
        return (lhs == rhs) ? 0 : (lhs ? 1 : -1);
    }
    size_t index = 0U;
    while (lhs[index] != '\0' && rhs[index] != '\0')
    {
        int diff = search_saved_casefold_char(lhs[index]) - search_saved_casefold_char(rhs[index]);
        if (diff != 0)
        {
            return diff;
        }
        index++;
    }
    return search_saved_casefold_char(lhs[index]) - search_saved_casefold_char(rhs[index]);
}

static void search_saved_trim(char *text)
{
    if (!text)
    {
        return;
    }
    size_t length = strlen(text);
    while (length > 0U)
    {
        char c = text[length - 1U];
        if (c == '\n' || c == '\r' || c == '\t' || c == ' ')
        {
            text[length - 1U] = '\0';
            --length;
        }
        else
        {
            break;
        }
    }
    const char *cursor = text;
    while (*cursor == ' ' || *cursor == '\t')
    {
        ++cursor;
    }
    if (cursor != text)
    {
        memmove(text, cursor, strlen(cursor) + 1U);
    }
}

static bool search_saved_contains_forbidden(const char *value)
{
    if (!value)
    {
        return false;
    }
    for (const char *cursor = value; *cursor != '\0'; ++cursor)
    {
        if (*cursor == '|' || *cursor == '\n' || *cursor == '\r')
        {
            return true;
        }
    }
    return false;
}

static const char *search_saved_mode_label(SearchQueryMode mode)
{
    switch (mode)
    {
    case SEARCH_QUERY_MODE_SUBSTRING:
        return "substring";
    case SEARCH_QUERY_MODE_BOOLEAN:
        return "boolean";
    case SEARCH_QUERY_MODE_REGEX:
        return "regex";
    default:
        return "substring";
    }
}

static bool search_saved_parse_mode(const char *text, SearchQueryMode *out_mode)
{
    if (!text || !out_mode)
    {
        return false;
    }

    char lowered[32];
    size_t index = 0U;
    for (; text[index] != '\0' && index + 1U < sizeof(lowered); ++index)
    {
        lowered[index] = (char)tolower((unsigned char)text[index]);
    }
    lowered[index] = '\0';

    if (strcmp(lowered, "substring") == 0 || strcmp(lowered, "sub") == 0 ||
        strcmp(lowered, "0") == 0)
    {
        *out_mode = SEARCH_QUERY_MODE_SUBSTRING;
        return true;
    }
    if (strcmp(lowered, "boolean") == 0 || strcmp(lowered, "bool") == 0 ||
        strcmp(lowered, "1") == 0)
    {
        *out_mode = SEARCH_QUERY_MODE_BOOLEAN;
        return true;
    }
    if (strcmp(lowered, "regex") == 0 || strcmp(lowered, "regexp") == 0 ||
        strcmp(lowered, "regular") == 0 || strcmp(lowered, "2") == 0)
    {
        *out_mode = SEARCH_QUERY_MODE_REGEX;
        return true;
    }
    return false;
}

void search_saved_list_init(SearchSavedQueryList *list)
{
    if (!list)
    {
        return;
    }
    list->entries  = NULL;
    list->count    = 0U;
    list->capacity = 0U;
}

void search_saved_list_reset(SearchSavedQueryList *list)
{
    if (!list)
    {
        return;
    }
    AT_FREE(list->entries);
    list->entries  = NULL;
    list->count    = 0U;
    list->capacity = 0U;
}

static bool search_saved_expand(SearchSavedQueryList *list)
{
    if (!list)
    {
        return false;
    }
    size_t new_capacity =
        (list->capacity == 0U) ? SEARCH_SAVED_INITIAL_CAPACITY : list->capacity * 2U;
    SearchSavedQuery *resized = (SearchSavedQuery *)at_secure_realloc(list->entries, new_capacity,
                                                                      sizeof(SearchSavedQuery));
    if (!resized)
    {
        return false;
    }
    list->entries  = resized;
    list->capacity = new_capacity;
    return true;
}

bool search_saved_list_add(SearchSavedQueryList *list, const char *name, SearchQueryMode mode,
                           const char *expression, char *error_buffer, size_t error_buffer_size)
{
    if (!list)
    {
        search_saved_set_error(error_buffer, error_buffer_size, "No collection provided.");
        return false;
    }
    if (!name || name[0] == '\0')
    {
        search_saved_set_error(error_buffer, error_buffer_size,
                               "Provide a name for the saved query.");
        return false;
    }
    if (!expression || expression[0] == '\0')
    {
        search_saved_set_error(error_buffer, error_buffer_size,
                               "Provide an expression before saving.");
        return false;
    }
    if (mode < SEARCH_QUERY_MODE_SUBSTRING || mode > SEARCH_QUERY_MODE_REGEX)
    {
        search_saved_set_error(error_buffer, error_buffer_size, "Unsupported query mode.");
        return false;
    }

    size_t trimmed_name_length = strnlen(name, SEARCH_SAVED_QUERY_NAME_MAX);
    if (trimmed_name_length >= SEARCH_SAVED_QUERY_NAME_MAX)
    {
        search_saved_set_error(error_buffer, error_buffer_size, "Saved query name is too long.");
        return false;
    }

    size_t trimmed_expression_length = strnlen(expression, SEARCH_QUERY_MAX_EXPRESSION_LENGTH);
    if (trimmed_expression_length >= SEARCH_QUERY_MAX_EXPRESSION_LENGTH)
    {
        search_saved_set_error(error_buffer, error_buffer_size,
                               "Expression exceeds supported length.");
        return false;
    }

    if (search_saved_contains_forbidden(name))
    {
        search_saved_set_error(error_buffer, error_buffer_size,
                               "Saved query name contains invalid characters.");
        return false;
    }
    if (search_saved_contains_forbidden(expression))
    {
        search_saved_set_error(error_buffer, error_buffer_size,
                               "Saved query expression contains invalid characters.");
        return false;
    }

    size_t existing_index = 0U;
    if (search_saved_list_find_by_name(list, name, &existing_index))
    {
        search_saved_set_error(error_buffer, error_buffer_size,
                               "A saved query with that name already exists.");
        return false;
    }

    if (list->count == list->capacity)
    {
        if (!search_saved_expand(list))
        {
            search_saved_set_error(error_buffer, error_buffer_size,
                                   "Unable to grow saved query list.");
            return false;
        }
    }

    SearchSavedQuery *entry = &list->entries[list->count];
    memset(entry, 0, sizeof(*entry));
#if defined(_MSC_VER)
    (void)strncpy_s(entry->name, sizeof(entry->name), name, _TRUNCATE);
    (void)strncpy_s(entry->expression, sizeof(entry->expression), expression, _TRUNCATE);
#else
    (void)snprintf(entry->name, sizeof(entry->name), "%s", name);
    (void)snprintf(entry->expression, sizeof(entry->expression), "%s", expression);
#endif
    entry->mode = mode;
    list->count++;
    search_saved_set_error(error_buffer, error_buffer_size, NULL);
    return true;
}

bool search_saved_list_remove(SearchSavedQueryList *list, size_t index)
{
    if (!list || index >= list->count)
    {
        return false;
    }
    if (index + 1U < list->count)
    {
        memmove(&list->entries[index], &list->entries[index + 1U],
                (list->count - index - 1U) * sizeof(SearchSavedQuery));
    }
    list->count--;
    if (list->count == 0U)
    {
        search_saved_list_reset(list);
    }
    return true;
}

const SearchSavedQuery *search_saved_list_get(const SearchSavedQueryList *list, size_t index)
{
    if (!list || index >= list->count)
    {
        return NULL;
    }
    return &list->entries[index];
}

size_t search_saved_list_count(const SearchSavedQueryList *list)
{
    if (!list)
    {
        return 0U;
    }
    return list->count;
}

bool search_saved_list_find_by_name(const SearchSavedQueryList *list, const char *name,
                                    size_t *out_index)
{
    if (!list || !name)
    {
        return false;
    }
    for (size_t index = 0U; index < list->count; ++index)
    {
        if (search_saved_casecmp(list->entries[index].name, name) == 0)
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

bool search_saved_list_load(SearchSavedQueryList *list, const char *path, char *error_buffer,
                            size_t error_buffer_size)
{
    if (!list || !path || path[0] == '\0')
    {
        search_saved_set_error(error_buffer, error_buffer_size, "Invalid parameters.");
        return false;
    }

    search_saved_list_reset(list);

#if defined(_MSC_VER)
    errno        = 0;
    FILE *stream = NULL;
    if (fopen_s(&stream, path, "rb") != 0)
    {
        if (errno == ENOENT)
        {
            search_saved_set_error(error_buffer, error_buffer_size, NULL);
            return true;
        }
        search_saved_set_error(error_buffer, error_buffer_size, "Unable to open saved query file.");
        return false;
    }
#else
    FILE *stream = fopen(path, "rb");
    if (!stream)
    {
        if (errno == ENOENT)
        {
            search_saved_set_error(error_buffer, error_buffer_size, NULL);
            return true;
        }
        search_saved_set_error(error_buffer, error_buffer_size, "Unable to open saved query file.");
        return false;
    }
#endif

    char line[512];
    unsigned int line_number = 0U;
    bool success             = true;

    while (fgets(line, sizeof(line), stream))
    {
        line_number += 1U;
        search_saved_trim(line);
        if (line[0] == '\0' || line[0] == '#')
        {
            continue;
        }

        char *mode_token = strtok(line, "|");
        char *name_token = strtok(NULL, "|");
        char *expr_token = strtok(NULL, "");
        if (!mode_token || !name_token || !expr_token)
        {
            char message[128];
            (void)snprintf(message, sizeof(message), "Malformed entry on line %u.", line_number);
            search_saved_set_error(error_buffer, error_buffer_size, message);
            success = false;
            break;
        }

        search_saved_trim(mode_token);
        search_saved_trim(name_token);
        search_saved_trim(expr_token);

        SearchQueryMode mode = SEARCH_QUERY_MODE_SUBSTRING;
        if (!search_saved_parse_mode(mode_token, &mode))
        {
            char message[128];
            (void)snprintf(message, sizeof(message), "Unknown mode '%s' on line %u.", mode_token,
                           line_number);
            search_saved_set_error(error_buffer, error_buffer_size, message);
            success = false;
            break;
        }

        char add_error[128];
        if (!search_saved_list_add(list, name_token, mode, expr_token, add_error,
                                   sizeof(add_error)))
        {
            char message[192];
            if (add_error[0] != '\0')
            {
                (void)snprintf(message, sizeof(message), "Line %u: %s", line_number, add_error);
            }
            else
            {
                (void)snprintf(message, sizeof(message), "Failed to import line %u.", line_number);
            }
            search_saved_set_error(error_buffer, error_buffer_size, message);
            success = false;
            break;
        }
    }

    fclose(stream);
    if (success)
    {
        search_saved_set_error(error_buffer, error_buffer_size, NULL);
    }
    return success;
}

bool search_saved_list_save(const SearchSavedQueryList *list, const char *path, char *error_buffer,
                            size_t error_buffer_size)
{
    if (!list || !path || path[0] == '\0')
    {
        search_saved_set_error(error_buffer, error_buffer_size, "Invalid parameters.");
        return false;
    }

#if defined(_MSC_VER)
    errno        = 0;
    FILE *stream = NULL;
    if (fopen_s(&stream, path, "wb") != 0)
    {
        search_saved_set_error(error_buffer, error_buffer_size, "Unable to write saved queries.");
        return false;
    }
#else
    FILE *stream = fopen(path, "wb");
    if (!stream)
    {
        search_saved_set_error(error_buffer, error_buffer_size, "Unable to write saved queries.");
        return false;
    }
#endif

    for (size_t index = 0U; index < list->count; ++index)
    {
        const SearchSavedQuery *entry = &list->entries[index];
        const char *mode_label        = search_saved_mode_label(entry->mode);
        if (fprintf(stream, "%s|%s|%s\n", mode_label, entry->name, entry->expression) < 0)
        {
            fclose(stream);
            search_saved_set_error(error_buffer, error_buffer_size,
                                   "Failed to serialize saved queries.");
            return false;
        }
    }

    fclose(stream);
    search_saved_set_error(error_buffer, error_buffer_size, NULL);
    return true;
}
