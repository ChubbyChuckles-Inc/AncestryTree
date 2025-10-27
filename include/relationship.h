#ifndef RELATIONSHIP_H
#define RELATIONSHIP_H

#include "tree.h"

#include <stdbool.h>
#include <stddef.h>
#include <stdint.h>

#ifdef __cplusplus
extern "C"
{
#endif

    typedef enum RelationshipLinkType
    {
        RELATIONSHIP_LINK_PARENT = 0,
        RELATIONSHIP_LINK_CHILD,
        RELATIONSHIP_LINK_SPOUSE
    } RelationshipLinkType;

#define RELATIONSHIP_MAX_CHAIN_LENGTH 64U

    typedef struct RelationshipResult
    {
        uint32_t chain[RELATIONSHIP_MAX_CHAIN_LENGTH];
        RelationshipLinkType link_types[RELATIONSHIP_MAX_CHAIN_LENGTH - 1U];
        size_t step_count;
    } RelationshipResult;

    void relationship_result_reset(RelationshipResult *result);

    bool relationship_calculate(const FamilyTree *tree, uint32_t source_id, uint32_t target_id,
                                RelationshipResult *result);

    bool relationship_describe(const FamilyTree *tree, const RelationshipResult *result,
                               char *buffer, size_t capacity);

#ifdef __cplusplus
}
#endif

#endif /* RELATIONSHIP_H */
