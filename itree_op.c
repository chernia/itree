#include "postgres.h"
#include "fmgr.h"
#include "itree.h"

PG_FUNCTION_INFO_V1(itree_is_descendant);
Datum itree_is_descendant(PG_FUNCTION_ARGS) {
    itree *child = (itree *)PG_GETARG_POINTER(0);  // Cast from generic pointer
    itree *parent = (itree *)PG_GETARG_POINTER(1); // Cast from generic pointer
    int parent_levels = 0;

    while (parent_levels < ITREE_MAX_LEVELS && parent->data[parent_levels]) {
        parent_levels++;
    }
    if (parent_levels == ITREE_MAX_LEVELS) {
        PG_RETURN_BOOL(false); // Full parent can’t be ancestor
    }
    PG_RETURN_BOOL(memcmp(child->data, parent->data, parent_levels) == 0);
}

PG_FUNCTION_INFO_V1(itree_is_ancestor);
Datum itree_is_ancestor(PG_FUNCTION_ARGS) {
    itree *parent = (itree *)PG_GETARG_POINTER(0); // Cast from generic pointer
    itree *child = (itree *)PG_GETARG_POINTER(1);  // Cast from generic pointer
    int parent_levels = 0;

    while (parent_levels < ITREE_MAX_LEVELS && parent->data[parent_levels]) {
        parent_levels++;
    }
    if (parent_levels == ITREE_MAX_LEVELS) {
        PG_RETURN_BOOL(false);
    }
    PG_RETURN_BOOL(memcmp(child->data, parent->data, parent_levels) == 0);
}