#include "tree_statistics.h"

#include "at_date.h"
#include "at_memory.h"
#include "at_string.h"

#include <ctype.h>
#include <stdlib.h>
#include <string.h>

typedef struct TreeLocationAccumulator
{
    char *location;
    size_t count;
} TreeLocationAccumulator;

typedef struct TreeStatisticsQueueEntry
{
    Person *person;
    size_t depth;
} TreeStatisticsQueueEntry;

static void tree_statistics_free_locations(TreeLocationCount *locations, size_t count)
{
    if (!locations)
    {
        return;
    }
    for (size_t index = 0U; index < count; ++index)
    {
        AT_FREE(locations[index].location);
    }
    AT_FREE(locations);
}

void tree_statistics_init(TreeStatistics *stats)
{
    if (!stats)
    {
        return;
    }
    memset(stats, 0, sizeof(TreeStatistics));
}

void tree_statistics_reset(TreeStatistics *stats)
{
    if (!stats)
    {
        return;
    }
    tree_statistics_free_locations(stats->birth_locations, stats->birth_location_count);
    tree_statistics_free_locations(stats->death_locations, stats->death_location_count);
    stats->birth_locations        = NULL;
    stats->birth_location_count   = 0U;
    stats->death_locations        = NULL;
    stats->death_location_count   = 0U;
    stats->person_count           = 0U;
    stats->living_count           = 0U;
    stats->deceased_count         = 0U;
    stats->generation_count       = 0U;
    stats->lifespan_sample_count  = 0U;
    stats->average_lifespan_years = 0.0;
}

static bool tree_statistics_strings_equal_case_insensitive(const char *lhs, const char *rhs)
{
    if (lhs == rhs)
    {
        return true;
    }
    if (!lhs || !rhs)
    {
        return false;
    }
    while (*lhs && *rhs)
    {
        unsigned char left  = (unsigned char)(*lhs);
        unsigned char right = (unsigned char)(*rhs);
        int left_lower      = tolower(left);
        int right_lower     = tolower(right);
        if (left_lower != right_lower)
        {
            return false;
        }
        lhs++;
        rhs++;
    }
    return (*lhs == '\0' && *rhs == '\0');
}

static char *tree_statistics_normalize_location(const char *location)
{
    if (!location)
    {
        return NULL;
    }
    size_t length = at_string_length_safe(location);
    if (length == 0U)
    {
        return NULL;
    }

    size_t start = 0U;
    while (start < length && isspace((unsigned char)location[start]))
    {
        start++;
    }
    size_t end = length;
    while (end > start && isspace((unsigned char)location[end - 1U]))
    {
        end--;
    }
    if (end <= start)
    {
        return NULL;
    }

    size_t trimmed_length = end - start;
    char *copy            = at_string_dup_range(location + start, trimmed_length);
    if (!copy)
    {
        return NULL;
    }
    if (tree_statistics_strings_equal_case_insensitive(copy, "Unknown"))
    {
        AT_FREE(copy);
        return NULL;
    }
    return copy;
}

static bool tree_statistics_accumulate_location(TreeLocationAccumulator **entries, size_t *count,
                                                size_t *capacity, const char *location)
{
    if (!entries || !count || !capacity)
    {
        return false;
    }
    char *normalized = tree_statistics_normalize_location(location);
    if (!normalized)
    {
        return true;
    }
    TreeLocationAccumulator *array = *entries;
    for (size_t index = 0U; index < *count; ++index)
    {
        if (tree_statistics_strings_equal_case_insensitive(array[index].location, normalized))
        {
            array[index].count += 1U;
            AT_FREE(normalized);
            return true;
        }
    }
    if (*count >= *capacity)
    {
        size_t new_capacity              = (*capacity == 0U) ? 4U : (*capacity * 2U);
        TreeLocationAccumulator *resized = (TreeLocationAccumulator *)at_secure_realloc(
            array, new_capacity, sizeof(TreeLocationAccumulator));
        if (!resized)
        {
            AT_FREE(normalized);
            return false;
        }
        for (size_t index = *capacity; index < new_capacity; ++index)
        {
            resized[index].location = NULL;
            resized[index].count    = 0U;
        }
        array     = resized;
        *entries  = resized;
        *capacity = new_capacity;
    }
    array[*count].location = normalized;
    array[*count].count    = 1U;
    *count += 1U;
    return true;
}

static void tree_statistics_release_accumulators(TreeLocationAccumulator *entries, size_t count)
{
    if (!entries)
    {
        return;
    }
    for (size_t index = 0U; index < count; ++index)
    {
        AT_FREE(entries[index].location);
    }
    AT_FREE(entries);
}

static int tree_statistics_location_compare(const void *lhs, const void *rhs)
{
    const TreeLocationAccumulator *left  = (const TreeLocationAccumulator *)lhs;
    const TreeLocationAccumulator *right = (const TreeLocationAccumulator *)rhs;
    if (left->count < right->count)
    {
        return 1;
    }
    if (left->count > right->count)
    {
        return -1;
    }
    if (!left->location && !right->location)
    {
        return 0;
    }
    if (!left->location)
    {
        return 1;
    }
    if (!right->location)
    {
        return -1;
    }
    return strcmp(left->location, right->location);
}

static bool tree_statistics_emit_locations(TreeLocationCount **out_locations, size_t *out_count,
                                           TreeLocationAccumulator *accum, size_t accum_count,
                                           size_t max_entries)
{
    if (!out_locations || !out_count)
    {
        return false;
    }
    *out_locations = NULL;
    *out_count     = 0U;
    if (accum_count == 0U)
    {
        return true;
    }

    qsort(accum, accum_count, sizeof(TreeLocationAccumulator), tree_statistics_location_compare);

    size_t copy_count =
        (max_entries == 0U || max_entries > accum_count) ? accum_count : max_entries;
    if (copy_count == 0U)
    {
        return true;
    }

    TreeLocationCount *locations =
        (TreeLocationCount *)AT_CALLOC(copy_count, sizeof(TreeLocationCount));
    if (!locations)
    {
        return false;
    }

    for (size_t index = 0U; index < copy_count; ++index)
    {
        char *dup = at_string_dup(accum[index].location);
        if (!dup)
        {
            for (size_t cleanup_index = 0U; cleanup_index < index; ++cleanup_index)
            {
                AT_FREE(locations[cleanup_index].location);
            }
            AT_FREE(locations);
            return false;
        }
        locations[index].location = dup;
        locations[index].count    = accum[index].count;
    }

    *out_locations = locations;
    *out_count     = copy_count;
    return true;
}

static bool tree_statistics_person_seen(Person **visited, size_t visited_count, Person *person)
{
    if (!person || !visited)
    {
        return false;
    }
    for (size_t index = 0U; index < visited_count; ++index)
    {
        if (visited[index] == person)
        {
            return true;
        }
    }
    return false;
}

static bool tree_statistics_enqueue(TreeStatisticsQueueEntry *queue, size_t capacity, size_t *back,
                                    Person **visited, size_t *visited_count, Person *person,
                                    size_t depth)
{
    if (!queue || !back || !visited || !visited_count || !person)
    {
        return true;
    }
    if (tree_statistics_person_seen(visited, *visited_count, person))
    {
        return true;
    }
    if (*back >= capacity || *visited_count >= capacity)
    {
        return false;
    }
    queue[*back].person = person;
    queue[*back].depth  = depth;
    *back += 1U;
    visited[*visited_count] = person;
    *visited_count += 1U;
    return true;
}

static bool tree_statistics_compute_generation_count(const FamilyTree *tree, size_t *out_count)
{
    if (!out_count)
    {
        return false;
    }
    *out_count = 0U;
    if (!tree || tree->person_count == 0U)
    {
        return true;
    }

    size_t capacity = tree->person_count;
    TreeStatisticsQueueEntry *queue =
        (TreeStatisticsQueueEntry *)AT_CALLOC(capacity, sizeof(TreeStatisticsQueueEntry));
    Person **visited = (Person **)AT_CALLOC(capacity, sizeof(Person *));
    Person **roots   = (Person **)AT_CALLOC(capacity, sizeof(Person *));
    if (!queue || !visited || !roots)
    {
        AT_FREE(queue);
        AT_FREE(visited);
        AT_FREE(roots);
        return false;
    }

    size_t back          = 0U;
    size_t front         = 0U;
    size_t visited_count = 0U;

    size_t root_count = family_tree_get_roots(tree, roots, capacity);
    if (root_count == 0U)
    {
        for (size_t index = 0U; index < tree->person_count; ++index)
        {
            Person *candidate = tree->persons[index];
            if (!tree_statistics_enqueue(queue, capacity, &back, visited, &visited_count, candidate,
                                         1U))
            {
                AT_FREE(queue);
                AT_FREE(visited);
                AT_FREE(roots);
                return false;
            }
        }
    }
    else
    {
        for (size_t index = 0U; index < root_count; ++index)
        {
            Person *root = roots[index];
            if (!tree_statistics_enqueue(queue, capacity, &back, visited, &visited_count, root, 1U))
            {
                AT_FREE(queue);
                AT_FREE(visited);
                AT_FREE(roots);
                return false;
            }
        }
    }

    size_t max_depth    = 0U;
    size_t scan_index   = 0U;
    bool enqueue_failed = false;

    while ((front < back) || (scan_index < tree->person_count))
    {
        if (front >= back)
        {
            Person *fallback = tree->persons[scan_index++];
            if (!fallback)
            {
                continue;
            }
            if (tree_statistics_person_seen(visited, visited_count, fallback))
            {
                continue;
            }
            if (!tree_statistics_enqueue(queue, capacity, &back, visited, &visited_count, fallback,
                                         1U))
            {
                enqueue_failed = true;
                break;
            }
            continue;
        }

        TreeStatisticsQueueEntry entry = queue[front++];
        if (entry.depth > max_depth)
        {
            max_depth = entry.depth;
        }
        Person *person = entry.person;
        if (!person)
        {
            continue;
        }
        for (size_t child_index = 0U; child_index < person->children_count; ++child_index)
        {
            Person *child = person->children[child_index];
            if (!tree_statistics_enqueue(queue, capacity, &back, visited, &visited_count, child,
                                         entry.depth + 1U))
            {
                enqueue_failed = true;
                break;
            }
        }
        if (enqueue_failed)
        {
            break;
        }
    }

    if (enqueue_failed)
    {
        AT_FREE(queue);
        AT_FREE(visited);
        AT_FREE(roots);
        return false;
    }

    *out_count = max_depth;

    AT_FREE(queue);
    AT_FREE(visited);
    AT_FREE(roots);
    return true;
}

static bool tree_statistics_parse_component(const char *text, size_t count, int *out_value)
{
    int value = 0;
    for (size_t index = 0U; index < count; ++index)
    {
        unsigned char ch = (unsigned char)text[index];
        if (!isdigit(ch))
        {
            return false;
        }
        value = (value * 10) + (int)(ch - '0');
    }
    if (out_value)
    {
        *out_value = value;
    }
    return true;
}

static bool tree_statistics_parse_iso8601(const char *value, int *out_year, int *out_month,
                                          int *out_day)
{
    if (!value)
    {
        return false;
    }
    size_t length = strlen(value);
    if (length != 10U || value[4] != '-' || value[7] != '-')
    {
        return false;
    }
    int year  = 0;
    int month = 0;
    int day   = 0;
    if (!tree_statistics_parse_component(value, 4U, &year) ||
        !tree_statistics_parse_component(value + 5, 2U, &month) ||
        !tree_statistics_parse_component(value + 8, 2U, &day))
    {
        return false;
    }
    if (out_year)
    {
        *out_year = year;
    }
    if (out_month)
    {
        *out_month = month;
    }
    if (out_day)
    {
        *out_day = day;
    }
    return true;
}

static bool tree_statistics_parse_date(const char *value, int *out_year, int *out_month,
                                       int *out_day)
{
    if (!value)
    {
        return false;
    }
    size_t length = strlen(value);
    if (length == 4U)
    {
        int year = 0;
        if (!tree_statistics_parse_component(value, 4U, &year))
        {
            return false;
        }
        if (out_year)
        {
            *out_year = year;
        }
        if (out_month)
        {
            *out_month = 7;
        }
        if (out_day)
        {
            *out_day = 1;
        }
        return true;
    }
    if (!at_date_is_valid_iso8601(value))
    {
        return false;
    }
    return tree_statistics_parse_iso8601(value, out_year, out_month, out_day);
}

static bool tree_statistics_compute_lifespan_years(const Person *person, double *out_years)
{
    if (!person || !out_years)
    {
        return false;
    }
    const char *birth_date = person->dates.birth_date;
    const char *death_date = person->dates.death_date;
    if (!birth_date || !death_date)
    {
        return false;
    }
    int birth_year  = 0;
    int birth_month = 0;
    int birth_day   = 0;
    int death_year  = 0;
    int death_month = 0;
    int death_day   = 0;
    if (!tree_statistics_parse_date(birth_date, &birth_year, &birth_month, &birth_day) ||
        !tree_statistics_parse_date(death_date, &death_year, &death_month, &death_day))
    {
        return false;
    }
    double years = (double)(death_year - birth_year);
    years += (double)(death_month - birth_month) / 12.0;
    years += (double)(death_day - birth_day) / 365.0;
    if (!(years >= 0.0))
    {
        return false;
    }
    *out_years = years;
    return true;
}

bool tree_statistics_calculate(TreeStatistics *stats, const FamilyTree *tree,
                               size_t max_location_entries)
{
    if (!stats)
    {
        return false;
    }
    tree_statistics_reset(stats);
    if (!tree)
    {
        return true;
    }
    stats->person_count = tree->person_count;
    if (tree->person_count == 0U)
    {
        return true;
    }

    TreeLocationAccumulator *birth_accum = NULL;
    TreeLocationAccumulator *death_accum = NULL;
    size_t birth_count                   = 0U;
    size_t death_count                   = 0U;
    size_t birth_capacity                = 0U;
    size_t death_capacity                = 0U;

    double lifespan_total = 0.0;

    for (size_t index = 0U; index < tree->person_count; ++index)
    {
        Person *person = tree->persons[index];
        if (!person)
        {
            continue;
        }
        if (person->is_alive)
        {
            stats->living_count += 1U;
        }
        else
        {
            stats->deceased_count += 1U;
        }

        double lifespan_years = 0.0;
        if (tree_statistics_compute_lifespan_years(person, &lifespan_years))
        {
            stats->lifespan_sample_count += 1U;
            lifespan_total += lifespan_years;
        }

        if (!tree_statistics_accumulate_location(&birth_accum, &birth_count, &birth_capacity,
                                                 person->dates.birth_location))
        {
            tree_statistics_release_accumulators(birth_accum, birth_count);
            tree_statistics_release_accumulators(death_accum, death_count);
            tree_statistics_reset(stats);
            return false;
        }
        if (!tree_statistics_accumulate_location(&death_accum, &death_count, &death_capacity,
                                                 person->dates.death_location))
        {
            tree_statistics_release_accumulators(birth_accum, birth_count);
            tree_statistics_release_accumulators(death_accum, death_count);
            tree_statistics_reset(stats);
            return false;
        }
    }

    if (stats->lifespan_sample_count > 0U)
    {
        stats->average_lifespan_years = lifespan_total / (double)stats->lifespan_sample_count;
    }

    size_t generation_count = 0U;
    if (!tree_statistics_compute_generation_count(tree, &generation_count))
    {
        tree_statistics_release_accumulators(birth_accum, birth_count);
        tree_statistics_release_accumulators(death_accum, death_count);
        tree_statistics_reset(stats);
        return false;
    }
    stats->generation_count = generation_count;

    if (!tree_statistics_emit_locations(&stats->birth_locations, &stats->birth_location_count,
                                        birth_accum, birth_count, max_location_entries))
    {
        tree_statistics_release_accumulators(birth_accum, birth_count);
        tree_statistics_release_accumulators(death_accum, death_count);
        tree_statistics_reset(stats);
        return false;
    }
    if (!tree_statistics_emit_locations(&stats->death_locations, &stats->death_location_count,
                                        death_accum, death_count, max_location_entries))
    {
        tree_statistics_release_accumulators(birth_accum, birth_count);
        tree_statistics_release_accumulators(death_accum, death_count);
        tree_statistics_reset(stats);
        return false;
    }

    tree_statistics_release_accumulators(birth_accum, birth_count);
    tree_statistics_release_accumulators(death_accum, death_count);
    return true;
}
