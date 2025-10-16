#ifndef SEARCH_H
#define SEARCH_H

#include "tree.h"

#include <stdbool.h>
#include <stddef.h>

#ifdef __cplusplus
extern "C"
{
#endif

    typedef struct SearchFilter
    {
        const char *name_substring;
        bool include_alive;
        bool include_deceased;
        bool use_birth_year_range;
        int birth_year_min;
        int birth_year_max;
    } SearchFilter;

    size_t search_execute(const FamilyTree *tree, const SearchFilter *filter, const Person **out_results,
                          size_t capacity);

#ifdef __cplusplus
}
#endif

#endif /* SEARCH_H */
