#ifndef LAYOUT_H
#define LAYOUT_H

#include "tree.h"

#include <stddef.h>

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
LayoutResult layout_calculate(const FamilyTree *tree);

#endif /* LAYOUT_H */
