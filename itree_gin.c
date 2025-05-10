#include "postgres.h"
#include "fmgr.h"
#include "access/gin.h"      // For GIN-specific types and functions
#include "access/stratnum.h" // For StrategyNumber
#include "itree.h"


/**
 * FUNCTION 2 itree_extract_value(itree, internal, internal)
 * 
 * Datum *extractValue(Datum itemValue, int32 *nkeys, bool **nullFlags)
     * Returns a palloc'd array of keys given an item to be indexed. 
 * The number of returned keys must be stored into *nkeys. 
 * If any of the keys can be null, also palloc an array of *nkeys bool fields, 
 * store its address at *nullFlags, and set these null flags as needed. 
 * *nullFlags can be left NULL (its initial value) if all keys are non-null. 
 * The return value can be NULL if the item contains no keys.
 * 
 */
PG_FUNCTION_INFO_V1(itree_extract_value);
Datum itree_extract_value(PG_FUNCTION_ARGS) {
    itree *tree = PG_GETARG_ITREE(0);
    int32 *nkeys = (int32 *)PG_GETARG_POINTER(1);
    bool **nullFlags = (bool **)PG_GETARG_POINTER(2);

    // Handle NULL value
    if (tree == NULL) {
        *nkeys = 0;
        *nullFlags = NULL;
        PG_RETURN_POINTER(NULL);
    }

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
 * FUNCTION 3 Datum *extractQuery(Datum query, int32 *nkeys, StrategyNumber n, bool **pmatch, 
 *                      Pointer **extra_data, bool **nullFlags, int32 *searchMode)
 * Returns a palloc'd array of keys given a value to be queried; that is, 
 * query is the value on the right-hand side of an indexable operator 
 * whose left-hand side is the indexed column. 
 * n is the strategy number of the operator within the operator class.
 * The number of returned keys must be stored into *nkeys.
 * 
 * If any of the keys can be null, also palloc an array of *nkeys bool fields, 
 * store its address at *nullFlags, and set these null flags as needed. 
 * *nullFlags can be left NULL (its initial value) if all keys are non-null. 
 * The return value can be NULL if the query contains no keys.
 *
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

    // Handle NULL query
    if (query == NULL) {
        *nkeys = 0;
        *pmatch = NULL;
        *extra_data = NULL;
        *nullFlags = NULL;
        *searchMode = GIN_SEARCH_MODE_DEFAULT;
        PG_RETURN_POINTER(NULL);
    }

    Datum *keys = NULL;
    uint16_t segments[ITREE_MAX_LEVELS] = {0};
    int seg_count = itree_get_segments(query, segments);
    int i, byte_pos;


    switch (strategy) {
        case 1:  // <@ value is a descendant of query or equal to it
            // value should contain the full query path
            *nkeys = 1;
            keys = (Datum *)palloc(sizeof(Datum));
            keys[0] = PointerGetDatum(query);  // Use query as-is
            *searchMode = GIN_SEARCH_MODE_DEFAULT;
            break;
        case 2:  // @> (ancestor of query)
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
 * FUNCTION 4 : bool consistent(bool check[], StrategyNumber n, Datum query, int32 nkeys, Pointer extra_data[], bool *recheck, Datum queryKeys[], bool nullFlags[])
 * Returns true if an indexed item satisfies the query operator with strategy number n (or might satisfy it, if the recheck indication is returned). 
 * This function does not have direct access to the indexed item's value, since GIN does not store items explicitly. 
 * Rather, what is available is knowledge about which key values extracted from the query appear in a given indexed item.
 * The check array has length nkeys, which is the same as the number of keys previously returned by extractQuery for this query datum. 
 * Each element of the check array is true if the indexed item contains the corresponding query key, i.e., 
 * if (check[i] == true) the i-th key of the extractQuery result array is present in the indexed item. 
 * The original query datum is passed in case the consistent method needs to consult it, and so are the queryKeys[]
 * - no nullFlags possible, so nullFlags[] is left as NULL. 
 * 
 */
PG_FUNCTION_INFO_V1(itree_consistent);
Datum itree_consistent(PG_FUNCTION_ARGS) {
    bool *check = (bool *)PG_GETARG_POINTER(0);//array is already populated by the GIN index and indicates which query keys match the indexed item.
    //StrategyNumber strategy = PG_GETARG_UINT16(1);
    itree *query = PG_GETARG_ITREE(2);
    int32 nkeys = PG_GETARG_INT32(3);
    bool *recheck = (bool *)PG_GETARG_POINTER(5);
    //Datum *queryKeys = (Datum *)PG_GETARG_POINTER(6);
    // bool *nullFlags = (bool *)PG_GETARG_POINTER(7);
    
    /*
    On success, *recheck should be set to true if the heap tuple needs to be rechecked against the query operator, 
    or false if the index test is exact. That is:
        1. a false return value guarantees that the heap tuple does not match the query; 
        2. a true return value with *recheck set to false guarantees that the heap tuple does match the query; 
        3. and a true return value with *recheck set to true means that the heap tuple might match the query, 
        so it needs to be fetched and rechecked by evaluating the query operator directly against the originally indexed item.    
    */ 
    // Handle NULL query
    if (query == NULL) {
        PG_RETURN_NULL();
    }

    *recheck = false; // Default to no recheck needed for @> and <@ any key matched should mean a match
    for (int i = 0; i < nkeys; i++) {
        if (check[i]) {
            PG_RETURN_BOOL(true);
        }
    }

    PG_RETURN_BOOL(false);
}

/**
 * GinTernaryValue triConsistent(GinTernaryValue check[], StrategyNumber n, Datum query, int32 nkeys, Pointer extra_data[], Datum queryKeys[], bool nullFlags[])
 * 
 * triConsistent is similar to consistent, but instead of Booleans in the check vector, there are three possible values for each key: GIN_TRUE, GIN_FALSE and GIN_MAYBE. 
 * GIN_FALSE and GIN_TRUE have the same meaning as regular Boolean values, while GIN_MAYBE means that the presence of that key is not known. 
 * When GIN_MAYBE values are present, the function should only return GIN_TRUE if the item certainly matches whether or not the index item contains the corresponding query keys. 
 * Likewise, the function must return GIN_FALSE only if the item certainly does not match, whether or not it contains the GIN_MAYBE keys. 
 * If the result depends on the GIN_MAYBE entries, i.e., the match cannot be confirmed or refuted based on the known query keys, the function must return GIN_MAYBE.
 * When there are no GIN_MAYBE values in the check vector, a GIN_MAYBE return value is the equivalent of setting the recheck flag in the Boolean consistent function.
 */


/******************************************************* 
 * OPTIONAL SUPPORT FUNCTIONS
*******************************************************/

