#ifndef SEARCH_SAVED_H
#define SEARCH_SAVED_H

#include "search.h"

#include <stdbool.h>
#include <stddef.h>

#ifdef __cplusplus
extern "C"
{
#endif

    typedef struct SearchSavedQuery
    {
        SearchQueryMode mode;
        char name[SEARCH_SAVED_QUERY_NAME_MAX];
        char expression[SEARCH_QUERY_MAX_EXPRESSION_LENGTH];
    } SearchSavedQuery;

    typedef struct SearchSavedQueryList
    {
        SearchSavedQuery *entries;
        size_t count;
        size_t capacity;
    } SearchSavedQueryList;

    void search_saved_list_init(SearchSavedQueryList *list);
    void search_saved_list_reset(SearchSavedQueryList *list);

    bool search_saved_list_add(SearchSavedQueryList *list, const char *name, SearchQueryMode mode,
                               const char *expression, char *error_buffer,
                               size_t error_buffer_size);

    bool search_saved_list_remove(SearchSavedQueryList *list, size_t index);

    const SearchSavedQuery *search_saved_list_get(const SearchSavedQueryList *list, size_t index);

    size_t search_saved_list_count(const SearchSavedQueryList *list);

    bool search_saved_list_find_by_name(const SearchSavedQueryList *list, const char *name,
                                        size_t *out_index);

#ifdef __cplusplus
}
#endif

#endif /* SEARCH_SAVED_H */
