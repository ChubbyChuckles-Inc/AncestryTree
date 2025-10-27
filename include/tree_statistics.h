#ifndef TREE_STATISTICS_H
#define TREE_STATISTICS_H

#include "tree.h"

#include <stdbool.h>
#include <stddef.h>

typedef struct TreeLocationCount
{
    char *location;
    size_t count;
} TreeLocationCount;

typedef struct TreeStatistics
{
    size_t person_count;
    size_t living_count;
    size_t deceased_count;
    size_t generation_count;
    size_t lifespan_sample_count;
    double average_lifespan_years;
    TreeLocationCount *birth_locations;
    size_t birth_location_count;
    TreeLocationCount *death_locations;
    size_t death_location_count;
} TreeStatistics;

void tree_statistics_init(TreeStatistics *stats);
void tree_statistics_reset(TreeStatistics *stats);
bool tree_statistics_calculate(TreeStatistics *stats, const FamilyTree *tree,
                               size_t max_location_entries);

#endif /* TREE_STATISTICS_H */
