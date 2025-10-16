#include "search.h"

#include "person.h"

#include <ctype.h>
#include <stddef.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>

static void lowercase_copy(char *destination, size_t capacity, const char *source)
{
    if (!destination || capacity == 0U)
    {
        return;
    }
    if (!source)
    {
        destination[0] = '\0';
        return;
    }
    size_t index = 0U;
    while (index + 1U < capacity && source[index] != '\0')
    {
        destination[index] = (char)tolower((unsigned char)source[index]);
        index++;
    }
    destination[index] = '\0';
}

static bool extract_year(const char *value, int *out_year)
{
    if (!value)
    {
        return false;
    }
    int digits = 0;
    int year = 0;
    for (size_t index = 0U; value[index] != '\0'; ++index)
    {
        unsigned char ch = (unsigned char)value[index];
        if (isdigit(ch))
        {
            year = year * 10 + (ch - '0');
            digits++;
            if (digits == 4)
            {
                if (out_year)
                {
                    *out_year = year;
                }
                return true;
            }
        }
        else if (digits > 0)
        {
            break;
        }
    }
    return false;
}

static int string_casefold_compare(const char *lhs, const char *rhs)
{
    size_t index = 0U;
    while (lhs[index] != '\0' && rhs[index] != '\0')
    {
        int diff = tolower((unsigned char)lhs[index]) - tolower((unsigned char)rhs[index]);
        if (diff != 0)
        {
            return diff;
        }
        index++;
    }
    return tolower((unsigned char)lhs[index]) - tolower((unsigned char)rhs[index]);
}

static bool filter_allows_alive(const SearchFilter *filter)
{
    if (!filter)
    {
        return true;
    }
    if (filter->include_alive || filter->include_deceased)
    {
        return filter->include_alive;
    }
    return true;
}

static bool filter_allows_deceased(const SearchFilter *filter)
{
    if (!filter)
    {
        return true;
    }
    if (filter->include_alive || filter->include_deceased)
    {
        return filter->include_deceased;
    }
    return true;
}

static bool search_matches_person(const Person *person, const SearchFilter *filter)
{
    if (!person)
    {
        return false;
    }
    if (!filter)
    {
        return true;
    }

    bool is_alive = person->is_alive;
    if (is_alive && !filter_allows_alive(filter))
    {
        return false;
    }
    if (!is_alive && !filter_allows_deceased(filter))
    {
        return false;
    }

    if (filter->name_substring && filter->name_substring[0] != '\0')
    {
        char name_buffer[160];
        if (!person_format_display_name(person, name_buffer, sizeof(name_buffer)))
        {
            (void)snprintf(name_buffer, sizeof(name_buffer), "Person %u", person->id);
        }
        char haystack[160];
        char needle[96];
        lowercase_copy(haystack, sizeof(haystack), name_buffer);
        lowercase_copy(needle, sizeof(needle), filter->name_substring);
        if (strstr(haystack, needle) == NULL)
        {
            return false;
        }
    }

    if (filter->use_birth_year_range)
    {
        int birth_year = 0;
        if (!extract_year(person->dates.birth_date, &birth_year))
        {
            return false;
        }
        int min_year = filter->birth_year_min;
        int max_year = filter->birth_year_max;
        if (min_year > max_year)
        {
            int swap = min_year;
            min_year = max_year;
            max_year = swap;
        }
        if (birth_year < min_year || birth_year > max_year)
        {
            return false;
        }
    }

    return true;
}

static int search_compare_persons(const void *lhs, const void *rhs)
{
    const Person *const *left = (const Person *const *)lhs;
    const Person *const *right = (const Person *const *)rhs;
    const Person *left_person = *left;
    const Person *right_person = *right;
    char left_name[160];
    char right_name[160];
    if (!person_format_display_name(left_person, left_name, sizeof(left_name)))
    {
        (void)snprintf(left_name, sizeof(left_name), "Person %u", left_person->id);
    }
    if (!person_format_display_name(right_person, right_name, sizeof(right_name)))
    {
        (void)snprintf(right_name, sizeof(right_name), "Person %u", right_person->id);
    }
    int diff = string_casefold_compare(left_name, right_name);
    if (diff != 0)
    {
        return diff;
    }
    if (left_person->id < right_person->id)
    {
        return -1;
    }
    if (left_person->id > right_person->id)
    {
        return 1;
    }
    return 0;
}

size_t search_execute(const FamilyTree *tree, const SearchFilter *filter, const Person **out_results,
                      size_t capacity)
{
    if (!tree || !out_results || capacity == 0U)
    {
        return 0U;
    }

    SearchFilter resolved_filter;
    if (!filter)
    {
        resolved_filter.name_substring = NULL;
        resolved_filter.include_alive = true;
        resolved_filter.include_deceased = true;
        resolved_filter.use_birth_year_range = false;
        resolved_filter.birth_year_min = 0;
        resolved_filter.birth_year_max = 0;
        filter = &resolved_filter;
    }

    size_t stored = 0U;
    for (size_t index = 0U; index < tree->person_count; ++index)
    {
        const Person *person = tree->persons[index];
        if (!search_matches_person(person, filter))
        {
            continue;
        }
        if (stored < capacity)
        {
            out_results[stored] = person;
        }
        stored++;
        if (stored == capacity)
        {
            /* Continue iterating to honour sort stability even when capacity reached. */
        }
    }

    size_t count_to_sort = (stored < capacity) ? stored : capacity;
    if (count_to_sort > 1U)
    {
        qsort(out_results, count_to_sort, sizeof(out_results[0]), search_compare_persons);
    }
    return count_to_sort;
}
