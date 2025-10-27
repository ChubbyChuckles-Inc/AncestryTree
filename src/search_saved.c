#include "search_saved.h"

#include "at_memory.h"

#include <ctype.h>
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
