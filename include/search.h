#ifndef SEARCH_H
#define SEARCH_H

#include "tree.h"

#include <stdbool.h>
#include <stddef.h>

#define SEARCH_QUERY_MAX_EXPRESSION_LENGTH 192U
#define SEARCH_QUERY_MAX_TERM_LENGTH 64U
#define SEARCH_SAVED_QUERY_NAME_MAX 48U

#ifdef __cplusplus
extern "C"
{
#endif

    typedef enum SearchQueryMode
    {
        SEARCH_QUERY_MODE_SUBSTRING = 0,
        SEARCH_QUERY_MODE_BOOLEAN,
        SEARCH_QUERY_MODE_REGEX
    } SearchQueryMode;

    typedef struct SearchFilter
    {
        const char *name_substring;
        bool include_alive;
        bool include_deceased;
        bool use_birth_year_range;
        int birth_year_min;
        int birth_year_max;
        SearchQueryMode query_mode;
        const char *query_expression;
    } SearchFilter;

    size_t search_execute(const FamilyTree *tree, const SearchFilter *filter,
                          const Person **out_results, size_t capacity);

#ifdef __cplusplus
}
#endif

#endif /* SEARCH_H */
