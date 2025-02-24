#include "postgres.h"
#include "fmgr.h"
#include "access/gin.h"      // For GIN-specific types and functions
#include "access/stratnum.h" // For StrategyNumber
#include "itree.h"

PG_FUNCTION_INFO_V1(itree_extract_value);
Datum itree_extract_value(PG_FUNCTION_ARGS) {
    itree *tree = (itree *)PG_GETARG_POINTER(0);  // Cast from pointer
    int32 *nkeys = (int32 *)PG_GETARG_POINTER(1); // GIN passes nkeys as pointer
    Datum *keys = (Datum *)palloc(ITREE_MAX_LEVELS * sizeof(Datum));
    int i, levels = 0;

    for (i = 0; i < ITREE_MAX_LEVELS && tree->data[i]; i++) {
        keys[levels++] = Int32GetDatum(tree->data[i]);
    }
    *nkeys = levels;
    PG_RETURN_POINTER(keys);
}

PG_FUNCTION_INFO_V1(itree_consistent);
Datum itree_consistent(PG_FUNCTION_ARGS) {
    bool *recheck = (bool *)PG_GETARG_POINTER(5); // 6th arg is recheck
    itree *query = (itree *)PG_GETARG_POINTER(1); // 2nd arg is query
    StrategyNumber strategy = PG_GETARG_INT16(2); // 3rd arg is strategy
    itree *key = (itree *)PG_GETARG_POINTER(0);   // 1st arg is indexed value
    int query_levels = 0, key_levels = 0;

    // Count levels in query and key
    while (query_levels < ITREE_MAX_LEVELS && query->data[query_levels]) {
        query_levels++;
    }
    while (key_levels < ITREE_MAX_LEVELS && key->data[key_levels]) {
        key_levels++;
    }

    *recheck = false; // Exact match for now
    if (strategy == 1) { // <@ (is-descendant)
        if (query_levels >= key_levels) {
            PG_RETURN_BOOL(false);
        }
        PG_RETURN_BOOL(memcmp(key->data, query->data, query_levels) == 0);
    } else if (strategy == 2) { // @> (is-ancestor)
        if (key_levels >= query_levels) {
            PG_RETURN_BOOL(false);
        }
        PG_RETURN_BOOL(memcmp(key->data, query->data, key_levels) == 0);
    }
    PG_RETURN_BOOL(false);
}