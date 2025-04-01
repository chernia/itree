/**
 * Operations for the itree type.
 */
#include "postgres.h"
#include "fmgr.h"
#include "utils/builtins.h"
#include "itree.h"


/**
 * Get number of segments and extract them
 */
int itree_get_segments(itree *tree, uint16_t *segments) {
    int byte_pos = 0, seg_count = 0;

    memset(segments, 0, ITREE_MAX_LEVELS * sizeof(uint16_t));//just in case the caller has not initialized the array

    while (byte_pos < ITREE_MAX_LEVELS && tree->data[byte_pos] != 0) {
        bool is_2byte = false;
        if (byte_pos + 1 < ITREE_MAX_LEVELS) {
            int shift = (byte_pos + 1 < 8) ? (7 - (byte_pos + 1)) : (15 - (byte_pos + 1));
            uint8_t control = (byte_pos + 1 < 8) ? tree->control[0] : tree->control[1];
            is_2byte = !((control >> shift) & 1);
        }

        if (is_2byte && byte_pos + 1 < ITREE_MAX_LEVELS && tree->data[byte_pos + 1] != 0) {
            segments[seg_count++] = (tree->data[byte_pos] << 8) | tree->data[byte_pos + 1];
            byte_pos += 2;
        } else {
            segments[seg_count++] = tree->data[byte_pos];
            byte_pos++;
        }
    }

    return seg_count;
}

 /**
  * Check if the first itree is a descendant of the second.
  * child <@ parent
  */
 PG_FUNCTION_INFO_V1(itree_is_descendant);
 Datum itree_is_descendant(PG_FUNCTION_ARGS) {
     itree *child = PG_GETARG_ITREE(0);
     itree *parent = PG_GETARG_ITREE(1);
     uint16_t child_segs[ITREE_MAX_LEVELS] = {0};
     uint16_t parent_segs[ITREE_MAX_LEVELS] = {0};
     int child_len = itree_get_segments(child, child_segs);
     int parent_len = itree_get_segments(parent, parent_segs);
 
     // Child must be longer or equal?(like ltree) and match the start of parent's segments
     if (child_len < parent_len) {
         PG_RETURN_BOOL(false);
     }
 
     for (int i = 0; i < parent_len; i++) {
         if (child_segs[i] != parent_segs[i]) {
             PG_RETURN_BOOL(false);
         }
     }
 
     PG_RETURN_BOOL(true);
 }
 
 /**
  * Check if the first itree is an ancestor of the second.
  * parent @> child
  */
 PG_FUNCTION_INFO_V1(itree_is_ancestor);
 Datum itree_is_ancestor(PG_FUNCTION_ARGS) {
     itree *parent = PG_GETARG_ITREE(0);
     itree *child = PG_GETARG_ITREE(1);
     uint16_t parent_segs[ITREE_MAX_LEVELS] = {0};
     uint16_t child_segs[ITREE_MAX_LEVELS] = {0};
     int parent_len = itree_get_segments(parent, parent_segs);
     int child_len = itree_get_segments(child, child_segs);
 
     // Parent must be shorter or equal?(like ltree) and match the start of child's segments
     if (parent_len > child_len) {
         PG_RETURN_BOOL(false);
     }
 
     for (int i = 0; i < parent_len; i++) {
         if (parent_segs[i] != child_segs[i]) {
             PG_RETURN_BOOL(false);
         }
     }
 
     PG_RETURN_BOOL(true);
 }

/**
 * Compare two itree values: -1 (a < b), 0 (a = b), 1 (a > b)
 */
static int int_itree_cmp(itree *a, itree *b) {
    uint16_t a_segs[ITREE_MAX_LEVELS] = {0};
    uint16_t b_segs[ITREE_MAX_LEVELS] = {0};
    int a_len = itree_get_segments(a, a_segs);
    int b_len = itree_get_segments(b, b_segs);
    int i;

    for (i = 0; i < a_len && i < b_len; i++) {
        if (a_segs[i] < b_segs[i]) return -1;
        if (a_segs[i] > b_segs[i]) return 1;
    }

    // If equal up to shorter length, shorter wins
    if (a_len < b_len) return -1;
    if (a_len > b_len) return 1;
    return 0;
}

 /**
 * Compare two itree values for equality.
 * This works with the assumption that the data array is zero-padded.
 * TODO: rewrite with logical segments: itree_get_segments
 */
PG_FUNCTION_INFO_V1(itree_eq);
Datum itree_eq(PG_FUNCTION_ARGS){

    itree *a = PG_GETARG_ITREE(0);
    itree *b = PG_GETARG_ITREE(1);
    int i;

    for (i = 0; i < ITREE_MAX_LEVELS; i++) {
        if (a->data[i] != b->data[i]) {
            PG_RETURN_BOOL(false);
        }
        if(a->data[i] == 0 && b->data[i] == 0){
            break;
        }
    }
    
    PG_RETURN_BOOL(true);
}


PG_FUNCTION_INFO_V1(itree_cmp);
Datum itree_cmp(PG_FUNCTION_ARGS) {
    itree *a = PG_GETARG_ITREE(0);
    itree *b = PG_GETARG_ITREE(1);
    PG_RETURN_INT32(int_itree_cmp(a, b));
}

PG_FUNCTION_INFO_V1(itree_lt);
Datum itree_lt(PG_FUNCTION_ARGS) {
    itree *a = PG_GETARG_ITREE(0);
    itree *b = PG_GETARG_ITREE(1);
    PG_RETURN_BOOL(int_itree_cmp(a, b) < 0);
}

PG_FUNCTION_INFO_V1(itree_le);
Datum itree_le(PG_FUNCTION_ARGS) {
    itree *a = PG_GETARG_ITREE(0);
    itree *b = PG_GETARG_ITREE(1);
    PG_RETURN_BOOL(int_itree_cmp(a, b) <= 0);
}

PG_FUNCTION_INFO_V1(itree_gt);
Datum itree_gt(PG_FUNCTION_ARGS) {
    itree *a = PG_GETARG_ITREE(0);
    itree *b = PG_GETARG_ITREE(1);
    PG_RETURN_BOOL(int_itree_cmp(a, b) > 0);
}

PG_FUNCTION_INFO_V1(itree_ge);
Datum itree_ge(PG_FUNCTION_ARGS) {
    itree *a = PG_GETARG_ITREE(0);
    itree *b = PG_GETARG_ITREE(1);
    PG_RETURN_BOOL(int_itree_cmp(a, b) >= 0);
}

PG_FUNCTION_INFO_V1(ilevel);
Datum ilevel(PG_FUNCTION_ARGS) {
    itree *tree = PG_GETARG_ITREE(0);
    uint16_t segments[ITREE_MAX_LEVELS] = {0};
    int seg_count = itree_get_segments(tree, segments);
    PG_RETURN_INT32(seg_count);
}

/**
 * Concatenate two itree values.
 * This function assumes that the two itrees are valid and do not exceed the maximum levels.
 * It creates a new itree by concatenating the segments of both itrees.
 * TODO: check for overflow and max levels
 * TODO: rewrite with logical segments: itree_get_segments to avoid going through itree_out
 */
PG_FUNCTION_INFO_V1(itree_additree);
Datum itree_additree(PG_FUNCTION_ARGS) {
    itree *a = PG_GETARG_ITREE(0);
    itree *b = PG_GETARG_ITREE(1);

    // Convert itree to cstring
    char *a_cstr = DatumGetCString(DirectFunctionCall1(itree_out, PointerGetDatum(a)));
    char *b_cstr = DatumGetCString(DirectFunctionCall1(itree_out, PointerGetDatum(b)));

    // Concatenate the two strings with a dot separator
    StringInfoData buf;
    initStringInfo(&buf);
    appendStringInfoString(&buf, a_cstr); // Append the first itree as a string
    appendStringInfoChar(&buf, '.');     // Append the dot separator
    appendStringInfoString(&buf, b_cstr); // Append the second itree as a string

    // Convert the concatenated string back to itree
    itree *result = (itree *) DatumGetPointer(DirectFunctionCall1(itree_in, CStringGetDatum(buf.data)));

    // Free temporary memory
    pfree(a_cstr);
    pfree(b_cstr);
    pfree(buf.data);

    PG_FREE_IF_COPY(a, 0);
    PG_FREE_IF_COPY(b, 1);

    PG_RETURN_ITREE(result);
}

/* PG_FUNCTION_INFO_V1(itree_addint);
Datum itree_addint(PG_FUNCTION_ARGS);


PG_FUNCTION_INFO_V1(itree_intadd);
Datum itree_intadd(PG_FUNCTION_ARGS);
 */