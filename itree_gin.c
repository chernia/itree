#include "postgres.h"
#include "fmgr.h"
#include "access/gin.h"      // For GIN-specific types and functions
#include "access/stratnum.h" // For StrategyNumber
#include "itree.h"

#define GinOverlapStrategy		1
#define GinContainsStrategy		2
#define GinContainedStrategy	3
#define GinEqualStrategy		4

/**
 * FUNCTION 2 itree_extract_value(itree, internal, internal),
 * Datum *extractValue(Datum itemValue, int32 *nkeys, bool **nullFlags)
 * Returns a palloc'd array of keys given an item to be indexed. The number of returned keys must be stored into *nkeys. 
 * If any of the keys can be null, also palloc an array of *nkeys bool fields, store its address at *nullFlags, 
 * and set these null flags as needed. *nullFlags can be left NULL (its initial value) if all keys are non-null. 
 * The return value can be NULL if the item contains no keys.
 */
PG_FUNCTION_INFO_V1(itree_extract_value);
Datum itree_extract_value(PG_FUNCTION_ARGS) {
    itree *tree = PG_GETARG_ITREE(0);
    int32 *nkeys = (int32 *)PG_GETARG_POINTER(1);
    Datum *keys = NULL;
    uint16_t segments[ITREE_MAX_LEVELS] = {0};
    int seg_count = itree_get_segments(tree, segments);
    int i, byte_pos;

    elog(LOG, "itree_extract_value: nargs = %d", fcinfo->nargs);

    // Number of subpaths = number of segments (all prefixes)
    *nkeys = seg_count;
    if (seg_count == 0) {
        PG_RETURN_POINTER(NULL);
    }

    // Allocate keys array
    keys = (Datum *)palloc(seg_count * sizeof(Datum));

    // Rebuild each prefix as an itree
    byte_pos = 0;
    for (i = 0; i < seg_count; i++) {
        itree *subpath = (itree *)palloc0(ITREE_SIZE);
        int j;

        // Copy control and data up to this segment
        subpath->control[0] = tree->control[0];
        subpath->control[1] = tree->control[1];
        for (j = 0; j < byte_pos; j++) {
            subpath->data[j] = tree->data[j];
        }

        // Add current segment
        if (segments[i] <= 255) {
            subpath->data[byte_pos++] = (uint8_t)segments[i];
        } else {
            subpath->data[byte_pos++] = (uint8_t)(segments[i] >> 8);
            subpath->data[byte_pos] = (uint8_t)(segments[i] & 0xFF);
            if (byte_pos < 8) {
                subpath->control[0] &= ~(1 << (7 - byte_pos));
            } else {
                subpath->control[1] &= ~(1 << (15 - byte_pos));
            }
            byte_pos++;
        }

        keys[i] = PointerGetDatum(subpath);
    }

    PG_RETURN_POINTER(keys);
}

/**
 * bool consistent(bool check[], StrategyNumber n, Datum query, int32 nkeys, Pointer extra_data[], bool *recheck, Datum queryKeys[], bool nullFlags[])
 * FUNCTION 1 itree_consistent(internal, itree, smallint, int, int, internal),
 */
PG_FUNCTION_INFO_V1(itree_consistent);
Datum itree_consistent(PG_FUNCTION_ARGS) {
    bool *check = (bool *)PG_GETARG_POINTER(0);
    StrategyNumber strategy = PG_GETARG_UINT16(1);
    itree *query = PG_GETARG_ITREE(2);
    int32 nkeys = PG_GETARG_INT32(3);
    Pointer *extra_data = (Pointer *)PG_GETARG_POINTER(4);
    bool *recheck = (bool *)PG_GETARG_POINTER(5);
    Datum *queryKeys = (Datum *)PG_GETARG_POINTER(6);
    bool *nullFlags = (bool *)PG_GETARG_POINTER(7);
    bool result = false;

    elog(LOG, "itree_consistent: nargs = %d, strategy = %d, nkeys = %d", fcinfo->nargs, strategy, nkeys);
    if (fcinfo->nargs != 8) {
        elog(ERROR, "itree_consistent: expected 8 args, got %d", fcinfo->nargs);
    }
    if (recheck == NULL) {
        elog(ERROR, "itree_consistent: null recheck pointer");
    }

    *recheck = true;
    for (int i = 0; i < nkeys; i++) {
        if (nullFlags[i]) continue;
        itree *key = DatumGetITree(queryKeys[i]);
        uint16_t query_segs[ITREE_MAX_LEVELS] = {0};
        uint16_t key_segs[ITREE_MAX_LEVELS] = {0};
        int query_len = itree_get_segments(query, query_segs);
        int key_len = itree_get_segments(key, key_segs);

        switch (strategy) {
            case 1:  // <@
                if (check[i] && key_len >= query_len) {
                    int j;
                    for (j = 0; j < query_len; j++) {
                        if (key_segs[j] != query_segs[j]) {
                            check[i] = false;
                            break;
                        }
                    }
                    if (key_len == query_len && check[i]) *recheck = false;
                }
                break;
            case 2:  // @>
                if (check[i] && key_len <= query_len) {
                    int j;
                    for (j = 0; j < key_len; j++) {
                        if (key_segs[j] != query_segs[j]) {
                            check[i] = false;
                            break;
                        }
                    }
                    if (key_len == query_len && check[i]) *recheck = false;
                }
                break;
            default:
                elog(ERROR, "itree_consistent: unknown strategy %d", strategy);
        }
    }

    
    for (int i = 0; i < nkeys; i++) {
        if (check[i]) {
            result = true;
            break;
        }
    }
    PG_RETURN_BOOL(result);
}

/**
 * int comparePartial(Datum partial_key, Datum key, StrategyNumber n, Pointer extra_data)
 */
PG_FUNCTION_INFO_V1(itree_compare_partial);
Datum itree_compare_partial(PG_FUNCTION_ARGS) {
    itree *partial = PG_GETARG_ITREE(0);  // Query partial key
    itree *key = PG_GETARG_ITREE(1);      // Indexed key
    StrategyNumber strategy = PG_GETARG_UINT16(2);
    uint16_t partial_segs[ITREE_MAX_LEVELS] = {0};
    uint16_t key_segs[ITREE_MAX_LEVELS] = {0};
    int partial_len = itree_get_segments(partial, partial_segs);
    int key_len = itree_get_segments(key, key_segs);

    elog(LOG, "itree_compare_partial: nargs = %d", fcinfo->nargs);

    switch (strategy) {
        case 1:  // <@ (descendant)
            // If partial is a prefix of key, key could be a descendant
            if (partial_len > key_len) {
                PG_RETURN_INT32(1);  // Partial too long, no match
            }
            for (int i = 0; i < partial_len; i++) {
                if (partial_segs[i] < key_segs[i]) return -1;
                if (partial_segs[i] > key_segs[i]) return 1;
            }
            PG_RETURN_INT32(0);  // Possible match

        case 2:  // @> (ancestor)
            // If key is a prefix of partial, key could be an ancestor
            if (key_len > partial_len) {
                PG_RETURN_INT32(1);  // Key too long, no match
            }
            for (int i = 0; i < key_len; i++) {
                if (partial_segs[i] < key_segs[i]) return -1;
                if (partial_segs[i] > key_segs[i]) return 1;
            }
            PG_RETURN_INT32(0);  // Possible match

        default:
            elog(ERROR, "unknown strategy number: %d", strategy);
            PG_RETURN_INT32(0);
    }
}

/**
 * Datum *extractQuery(Datum query, int32 *nkeys, StrategyNumber n, bool **pmatch, Pointer **extra_data, bool **nullFlags, int32 *searchMode)
 */
PG_FUNCTION_INFO_V1(itree_extract_query);
Datum itree_extract_query(PG_FUNCTION_ARGS) {
    itree *query = PG_GETARG_ITREE(0);
    int32 *nkeys = (int32 *)PG_GETARG_POINTER(1);
    StrategyNumber strategy = PG_GETARG_UINT16(2);
    bool **pmatch = (bool **)PG_GETARG_POINTER(3);
    Pointer **extra_data = (Pointer **)PG_GETARG_POINTER(4);
    bool **nullFlags = (bool **)PG_GETARG_POINTER(5);
    int32 *searchMode = (int32 *)PG_GETARG_POINTER(6);
    Datum *keys = NULL;
    uint16_t segments[ITREE_MAX_LEVELS] = {0};
    int seg_count = itree_get_segments(query, segments);
    int i, byte_pos;

    elog(LOG, "itree_extract_query: nargs = %d", fcinfo->nargs);

    switch (strategy) {
        case 1:  // <@ (descendant of query)
            // Extract all prefixes of query (like extract_value)
            *nkeys = seg_count;
            if (seg_count == 0) {
                *searchMode = GIN_SEARCH_MODE_DEFAULT;
                PG_RETURN_POINTER(NULL);
            }
            keys = (Datum *)palloc(seg_count * sizeof(Datum));
            byte_pos = 0;
            for (i = 0; i < seg_count; i++) {
                itree *subpath = (itree *)palloc0(ITREE_SIZE);
                int j;
                subpath->control[0] = query->control[0];
                subpath->control[1] = query->control[1];
                for (j = 0; j < byte_pos; j++) {
                    subpath->data[j] = query->data[j];
                }
                if (segments[i] <= 255) {
                    subpath->data[byte_pos++] = (uint8_t)segments[i];
                } else {
                    subpath->data[byte_pos++] = (uint8_t)(segments[i] >> 8);
                    subpath->data[byte_pos] = (uint8_t)(segments[i] & 0xFF);
                    if (byte_pos < 8) {
                        subpath->control[0] &= ~(1 << (7 - byte_pos));
                    } else {
                        subpath->control[1] &= ~(1 << (15 - byte_pos));
                    }
                    byte_pos++;
                }
                keys[i] = PointerGetDatum(subpath);
            }
            *searchMode = GIN_SEARCH_MODE_DEFAULT;
            break;

        case 2:  // @> (ancestor of query)
            // Only need the full query path
            *nkeys = 1;
            keys = (Datum *)palloc(sizeof(Datum));
            keys[0] = PointerGetDatum(query);  // Use query as-is
            *searchMode = GIN_SEARCH_MODE_DEFAULT;
            break;

        default:
            elog(ERROR, "unknown strategy number: %d", strategy);
            *nkeys = 0;
            *searchMode = GIN_SEARCH_MODE_DEFAULT;
            PG_RETURN_POINTER(NULL);
    }

    *pmatch = NULL;      // No partial matching needed
    *extra_data = NULL;  // No extra data
    *nullFlags = NULL;   // No null flags
    PG_RETURN_POINTER(keys);
}

/**
 * int compare(Datum a, Datum b)
 */
PG_FUNCTION_INFO_V1(itree_compare);
Datum itree_compare(PG_FUNCTION_ARGS) {
    itree *a = PG_GETARG_ITREE(0);
    itree *b = PG_GETARG_ITREE(1);
    uint16_t a_segs[ITREE_MAX_LEVELS] = {0};
    uint16_t b_segs[ITREE_MAX_LEVELS] = {0};
    int a_len = itree_get_segments(a, a_segs);
    int b_len = itree_get_segments(b, b_segs);
    int i;

    elog(LOG, "itree_compare: nargs = %d", fcinfo->nargs);

    for (i = 0; i < a_len && i < b_len; i++) {
        if (a_segs[i] < b_segs[i]) PG_RETURN_INT32(-1);
        if (a_segs[i] > b_segs[i]) PG_RETURN_INT32(1);
    }

    if (a_len < b_len) PG_RETURN_INT32(-1);
    if (a_len > b_len) PG_RETURN_INT32(1);
    PG_RETURN_INT32(0);
}

//tri_consistent

/**
 * void options(local_relopts *relopts)

    Defines a set of user-visible parameters that control operator class behavior.

    The options function is passed a pointer to a local_relopts struct, which needs to be filled with a set of operator class specific options. The options can be accessed from other support functions using the PG_HAS_OPCLASS_OPTIONS() and PG_GET_OPCLASS_OPTIONS() macros.

    Since both key extraction of indexed values and representation of the key in GIN are flexible, they may depend on user-specified parameters.

 */