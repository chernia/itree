/**
 * Operations for the itree type.
 */
#include <assert.h>
#include "postgres.h"
#include "fmgr.h"
#include "utils/builtins.h"
#include "itree.h"


// Function to initialize an itree instance
itree *init_itree() {
    itree *tree_instance = (itree *) palloc(sizeof(itree));
    memset(tree_instance, 0, sizeof(itree));
    tree_instance->control[0] = 0xFF;
    tree_instance->control[1] = 0xFF;
    return tree_instance;
}

/**
 * Helper function to create an itree from a segments array.
 * Allocates memory for the itree and populates it based on the segments array.
 *
 * @param segments Array of uint16_t segments (unused segments must be 0).
 * @return Pointer to the newly created itree.
 */
itree *create_itree_from_segments(const uint16_t *segments) {
    // Allocate memory for the itree
    itree *result = init_itree();
    int byte_pos = 0;

    for (int i = 0; i < ITREE_MAX_LEVELS; i++) {
        if (segments[i] == 0) {
            break; // Stop processing when encountering an unused segment
        }

        if (segments[i] <= 255) { // 1-byte segment
            result->data[byte_pos] = (uint8_t)segments[i];
            set_control_bit(result, byte_pos, 1);
            byte_pos++;
        } else { // 2-byte segment
            result->data[byte_pos] = (uint8_t)(segments[i] >> 8);
            set_control_bit(result, byte_pos, 1);
            byte_pos++;
            result->data[byte_pos] = (uint8_t)(segments[i] & 0xFF);
            set_control_bit(result, byte_pos, 0); // continuation
            byte_pos++;
        }
    }

    return result;
}

// Function to get the control bit associated with data[data_index]
// The control bit for data[data_index] is stored at physical bit position 'data_index'
// within the 16-bit field formed by control[0] and control[1].
int get_control_bit(const itree* tree_instance, int data_index) {
    // Validate data_index to ensure it's within the expected range (0-15)
    if(data_index < 0 || data_index >= ITREE_MAX_LEVELS){
        elog(ERROR, "Data index %d out of bounds [0,%d)", data_index, ITREE_MAX_LEVELS);
    }

    // The physical bit position in the 16-bit control space (0-15) is now simply data_index
    int physical_bit_position = data_index;

    // Determine which byte of the control array (control[0] or control[1]) holds this bit
    int byte_array_index = physical_bit_position / 8; // Will be 0 for bits 0-7, 1 for bits 8-15

    // Determine the bit position within that specific byte (0-7)
    int bit_within_byte = physical_bit_position % 8;

    // Access the correct byte
    uint8_t target_byte = tree_instance->control[byte_array_index];

    // Extract and return the bit
    return (target_byte >> bit_within_byte) & 1;
}

// Function to set the control bit associated with data[data_index]
// data_index ranges from 0 to 15.
// bit_value should be 0 or 1.
void set_control_bit(itree* tree_instance, int data_index, int bit_value) {
    // Validate inputs
    if(data_index < 0 || data_index >= ITREE_MAX_LEVELS){
        elog(ERROR, "Data index %d out of bounds [0,%d)", data_index, ITREE_MAX_LEVELS);
    }
    if((bit_value != 0 && bit_value != 1)){
        elog(ERROR, "itree control bit value must be 0 or 1");
    }

    int physical_bit_position = data_index;
    int byte_array_index = physical_bit_position / 8;
    int bit_within_byte = physical_bit_position % 8;

    if (bit_value == 1) {
        // Set the bit (turn it to 1)
        tree_instance->control[byte_array_index] |= (1 << bit_within_byte);
    } else {
        // Clear the bit (turn it to 0)
        tree_instance->control[byte_array_index] &= ~(1 << bit_within_byte);
    }
}

/**
 * Get number of segments and extract them
 */
int itree_get_segments(itree *tree, uint16_t *segments) {
    int seg_count = 0;
    int byte_pos = 0;

    while (byte_pos < ITREE_MAX_LEVELS) {
        // End of itree: data byte is 0 and control bit is 1 (start of segment)
        if (tree->data[byte_pos] == 0 && get_control_bit(tree, byte_pos))
            break;

        // If this byte starts a new segment
        if (get_control_bit(tree, byte_pos)) {
            // Check if next byte is part of this segment (2-byte segment)
            if (byte_pos + 1 < ITREE_MAX_LEVELS && !get_control_bit(tree, byte_pos + 1)) {
                segments[seg_count++] = ((uint16_t)tree->data[byte_pos] << 8) | tree->data[byte_pos + 1];
                byte_pos += 2;
                continue;
            } else {   // Single byte segment
                segments[seg_count++] = tree->data[byte_pos];
            }
        }
        byte_pos++;
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
 */
PG_FUNCTION_INFO_V1(itree_eq);
Datum itree_eq(PG_FUNCTION_ARGS){
    itree *a = PG_GETARG_ITREE(0);
    itree *b = PG_GETARG_ITREE(1);

    if (int_itree_cmp(a, b) == 0) {
        PG_RETURN_BOOL(true);
    } else {
        PG_RETURN_BOOL(false);
    }
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
 */
PG_FUNCTION_INFO_V1(itree_additree);
Datum itree_additree(PG_FUNCTION_ARGS) {
    itree *a = PG_GETARG_ITREE(0);
    itree *b = PG_GETARG_ITREE(1);
    uint16_t a_segments[ITREE_MAX_LEVELS] = {0};
    uint16_t b_segments[ITREE_MAX_LEVELS] = {0};

    int a_seg_count = itree_get_segments(a, a_segments);
    int b_seg_count = itree_get_segments(b, b_segments);

    if( a_seg_count + b_seg_count > ITREE_MAX_LEVELS) {
        ereport(ERROR, (errcode(ERRCODE_INVALID_PARAMETER_VALUE),
                        errmsg("itree concatenation exceeds maximum levels")));
    }

    uint16_t all_segments[ITREE_MAX_LEVELS] = {0};

    // Copy segments from a and b to all_segments
    memcpy(all_segments, a_segments, sizeof(uint16_t) * a_seg_count);
    memcpy(all_segments + a_seg_count, b_segments, sizeof(uint16_t) * b_seg_count);
    
    itree *result = create_itree_from_segments(all_segments);

    PG_RETURN_ITREE(result);
}

/**
 * subitree ( itree, start integer, end integer ) → itree
 * 
 * Returns subpath of itree from position start to position end-1 (counting from 0).
 * subitree('1.2.3.4', 1, 2) → 2
 */
PG_FUNCTION_INFO_V1(itree_subitree);
Datum itree_subitree(PG_FUNCTION_ARGS) {
    itree *tree = PG_GETARG_ITREE(0);
    int start = PG_GETARG_INT32(1);
    int end = PG_GETARG_INT32(2);

    if (start < 0 || end < 0 || start >= ITREE_MAX_LEVELS || end >= ITREE_MAX_LEVELS) {
        ereport(ERROR, (errcode(ERRCODE_INVALID_PARAMETER_VALUE),
                        errmsg("itree subpath out of bounds")));
    }

    uint16_t segments[ITREE_MAX_LEVELS] = {0};
    int seg_count = itree_get_segments(tree, segments);

    if (start >= seg_count || end > seg_count) {
        ereport(ERROR, (errcode(ERRCODE_INVALID_PARAMETER_VALUE),
                        errmsg("itree subpath out of bounds")));
    }

    uint16_t sub_segments[ITREE_MAX_LEVELS] = {0};
    memcpy(sub_segments, segments + start, sizeof(uint16_t) * (end - start));

    itree *result = create_itree_from_segments(sub_segments);

    PG_RETURN_ITREE(result);
}

/**
 * subpath ( itree, offset integer, len integer ) → itree
 * Returns subpath of itree starting at position offset, with length len. 
 * If offset is negative, subpath starts that far from the end of the path. 
 * If len is negative, leaves that many labels off the end of the path.
 * 
 * subpath('1.2.3.4.5', 0, 2) → 1.2
 * 
 * Get the parent:
 * subpath('1.2.3.4.5', 0, -1) → 1.2.3.4 
 * 
 */
PG_FUNCTION_INFO_V1(itree_subpath);
Datum itree_subpath(PG_FUNCTION_ARGS) {
    itree *tree = PG_GETARG_ITREE(0);
    int offset = PG_GETARG_INT32(1);
    int len = PG_GETARG_INT32(2);

    uint16_t segments[ITREE_MAX_LEVELS] = {0};
    int seg_count = itree_get_segments(tree, segments);
    
    // Adjust offset if negative
    if (offset < 0) {
        offset = seg_count + offset; // Start from the end
    }

    // Adjust len if negative
    if (len < 0) {
        len = seg_count + len - offset; // Exclude segments from the end
    }

    // Validate adjusted offset and len
    if (offset < 0 || offset >= seg_count || len < 0 || offset + len > seg_count) {
        ereport(ERROR, (errcode(ERRCODE_INVALID_PARAMETER_VALUE),
                        errmsg("itree subpath out of bounds")));
    }

    uint16_t sub_segments[ITREE_MAX_LEVELS] = {0};
    memcpy(sub_segments, segments + offset, sizeof(uint16_t) * len);

    itree *result = create_itree_from_segments(sub_segments);

    PG_RETURN_ITREE(result);
}