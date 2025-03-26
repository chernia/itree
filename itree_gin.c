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
 * FUNCTION 2 itree_extract_value(itree, internal, internal)
 * 
 * Datum *extractValue(Datum itemValue, int32 *nkeys, bool **nullFlags)
 * Returns a palloc'd array of keys given an item to be indexed. The number of returned keys must be stored into *nkeys. 
 * No NULL keys possible, so *nullFlags is left as NULL.
 */
PG_FUNCTION_INFO_V1(itree_extract_value);
Datum itree_extract_value(PG_FUNCTION_ARGS) {
    itree *tree = PG_GETARG_ITREE(0);
    int32 *nkeys = (int32 *)PG_GETARG_POINTER(1);
    Datum *keys = NULL;
    uint16_t segments[ITREE_MAX_LEVELS] = {0};
    int seg_count = itree_get_segments(tree, segments);
    int i, byte_pos;

    // Number of subpaths = number of segments
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
 * Datum *extractQuery(Datum query, int32 *nkeys, StrategyNumber n, bool **pmatch, 
 *                      Pointer **extra_data, bool **nullFlags, int32 *searchMode)
 * Returns a palloc'd array of keys given a value to be queried; that is, 
 * query is the value on the right-hand side of an indexable operator 
 * whose left-hand side is the indexed column. 
 * n is the strategy number of the operator within the operator class.
 * The number of returned keys must be stored into *nkeys.
 * No NULL keys possible, so *nullFlags is left as NULL.
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
 * bool consistent(bool check[], StrategyNumber n, Datum query, int32 nkeys, Pointer extra_data[], bool *recheck, Datum queryKeys[], bool nullFlags[])
 * FUNCTION 1 itree_consistent(internal, itree, smallint, int, int, internal),
 */
PG_FUNCTION_INFO_V1(itree_consistent);
Datum itree_consistent(PG_FUNCTION_ARGS) {
    bool *check = (bool *)PG_GETARG_POINTER(0);
    StrategyNumber strategy = PG_GETARG_UINT16(1);
    itree *query = PG_GETARG_ITREE(2);
    int32 nkeys = PG_GETARG_INT32(3);
    bool *recheck = (bool *)PG_GETARG_POINTER(5);
    Datum *queryKeys = (Datum *)PG_GETARG_POINTER(6);
    bool *nullFlags = (bool *)PG_GETARG_POINTER(7);
    bool result = false;

    
    /*
    On success, *recheck should be set to true if the heap tuple needs to be rechecked against the query operator, 
    or false if the index test is exact. That is:
        1. a false return value guarantees that the heap tuple does not match the query; 
        2. a true return value with *recheck set to false guarantees that the heap tuple does match the query; 
        3. and a true return value with *recheck set to true means that the heap tuple might match the query, 
        so it needs to be fetched and rechecked by evaluating the query operator directly against the originally indexed item.    
    */
    *recheck = true;

    for (int i = 0; i < nkeys; i++) {
        if (nullFlags[i]) continue; //we should not have null flags
        itree *key = DatumGetITree(queryKeys[i]);
        uint16_t query_segs[ITREE_MAX_LEVELS] = {0};
        uint16_t key_segs[ITREE_MAX_LEVELS] = {0};
        int query_len = itree_get_segments(query, query_segs);
        int key_len = itree_get_segments(key, key_segs);

        switch (strategy) {
            case 1:  // key <@ query (descendant)
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
            case 2:  // key @> query (ancestor)
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

/******************************************************* 
 * OPTIONAL SUPPORT FUNCTIONS
*******************************************************/