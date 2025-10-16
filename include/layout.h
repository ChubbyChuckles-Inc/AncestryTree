#ifndef LAYOUT_H
#define LAYOUT_H

#include "tree.h"

#include <stdbool.h>
#include <stddef.h>

typedef enum LayoutAlgorithm
{
    LAYOUT_ALGORITHM_HIERARCHICAL = 0,
    LAYOUT_ALGORITHM_FORCE_DIRECTED = 1
} LayoutAlgorithm;

typedef struct LayoutNode
{
    Person *person;
    float position[3];
} LayoutNode;

typedef struct LayoutResult
{
    LayoutNode *nodes;
    size_t count;
} LayoutResult;

void layout_result_destroy(LayoutResult *result);
bool layout_result_copy(LayoutResult *destination, const LayoutResult *source);
void layout_result_move(LayoutResult *destination, LayoutResult *source);

LayoutResult layout_calculate(const FamilyTree *tree);
LayoutResult layout_calculate_with_algorithm(const FamilyTree *tree, LayoutAlgorithm algorithm);
LayoutResult layout_calculate_force_directed(const FamilyTree *tree);

bool layout_animate(const LayoutResult *from, const LayoutResult *to, float alpha, LayoutResult *out);

#endif /* LAYOUT_H */
