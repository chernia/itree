/** 
 * ---------------------------------------------------------------------------------------------------------------------------------
 * ITREE is a fixed 16 byte hierarchical tree structure that can store up to 14 levels of integers.
 * It is inspired by the amazing LTREE to serve as a compact ID for hierarchical data structures.
 * 
 * Each segment holds a 1 or 2 byte integer value from 1 to a maximum value of 65535.
 * 
 * 1. Data Structure:
 * The fist 2 bytes are control bytes that indicate whether the segment is 1 or 2 bytes long.
 * The first 3 bits of the first control byte are reserved for future use.
 * Therefore bits 3 to 15 are set to 1 for each segment that starts a segment and 0 if it is appended to the previous segment.
 * As value 0 is not allowed, we use it as a sentinel value to indicate the end of the tree on a last segment with 1 control bit.
 * 
 * 2.Operations
 * 3.Use Cases
 * ---------------------------------------------------------------------------------------------------------------------------------
 */
#include "postgres.h"
#include "fmgr.h"
#include "utils/array.h"
#include "utils/typcache.h"
#include "utils/memutils.h"
#include "catalog/pg_type_d.h" 
#include "itree.h"

PG_MODULE_MAGIC;

PG_FUNCTION_INFO_V1(itree_in);
Datum itree_in(PG_FUNCTION_ARGS) {
    char *input = PG_GETARG_CSTRING(0);

    // Handle the special case where the input is "NULL"
    if (input == NULL || strcmp(input, "NULL") == 0 ||!input || !*input) {
        ereport(ERROR, (errcode(ERRCODE_INVALID_TEXT_REPRESENTATION),
                        errmsg("invalid input syntax for itree: \"%s\"", input)));
    }

    itree *result = (itree *)palloc(ITREE_SIZE);
    int levels = 0, byte_pos = 0;
    char *ptr = input;

    result->control[0] = 0xFF;
    result->control[1] = 0xFF;
    memset(result->data, 0, sizeof(result->data));

    while (levels < ITREE_MAX_LEVELS && byte_pos < ITREE_MAX_LEVELS) {
        long val = strtol(ptr, &ptr, 10);//get the value and move the pointer
        if (val <= 0 || val > 65535) {
            ereport(ERROR, (errcode(ERRCODE_INVALID_TEXT_REPRESENTATION),
                            errmsg("itree segment must be in range 1..65535 (got %ld)", val)));
        }
        elog(DEBUG1, "itree segment %ld", val);

         // Set the control bit for this segment
        set_control_bit(result, byte_pos, 1);

        if (val <= 255) {
            result->data[byte_pos++] = (uint8_t)val;
        } else  {
            // 2-byte segment: store high then low byte
            result->data[byte_pos] = (uint8_t)(val >> 8);
            // Set control bit for the second byte to 0 (continuation)
            set_control_bit(result, byte_pos + 1, 0);
            result->data[byte_pos + 1] = (uint8_t)(val & 0xFF);
            byte_pos += 2;
        }
        levels++;

        // Advance ptr to next segment separator, if any
        if (*ptr == '.') ptr++;
        else break;
    }
              
    if (levels > ITREE_MAX_LEVELS) {
        ereport(ERROR, (errcode(ERRCODE_INVALID_TEXT_REPRESENTATION),
                        errmsg("itree exceeds max levels (%d)", ITREE_MAX_LEVELS)));
    }

    PG_RETURN_ITREE(result);
}

/** Convert an itree Datum to cstring */
PG_FUNCTION_INFO_V1(itree_out);
Datum itree_out(PG_FUNCTION_ARGS) {
     // Check for NULL input
    if (PG_ARGISNULL(0)) {
        PG_RETURN_CSTRING(pstrdup("NULL"));
    }

    itree *tree = PG_GETARG_ITREE(0);
    char buffer[ITREE_MAX_LEVELS * 6];
    int len = 0;
    uint16_t segments[ITREE_MAX_LEVELS];
    int seg_count = itree_get_segments(tree, segments);

    for (int i = 0; i < seg_count; i++) {
        if (i > 0) buffer[len++] = '.';
        len += sprintf(buffer + len, "%u", segments[i]);
    }

    char *result = palloc(len + 1);

    memcpy(result, buffer, len);
    result[len] = '\0';
    PG_RETURN_CSTRING(result);
}

PG_FUNCTION_INFO_V1(itree_typmod_in);
Datum itree_typmod_in(PG_FUNCTION_ARGS) {
    ArrayType *ta = PG_GETARG_ARRAYTYPE_P(0);
    Datum *elems;
    int nitems;
    int32 typmod = ITREE_MAX_LEVELS;

    /* Ensure the input is a one-dimensional array */
    if (ARR_NDIM(ta) != 1)
        ereport(ERROR, (errcode(ERRCODE_INVALID_PARAMETER_VALUE),
                        errmsg("invalid typmod array format")));

    /* Extract elements (as cstring[], not int4[]) */
    deconstruct_array(ta, CSTRINGOID, -2, false, 'c', &elems, NULL, &nitems);

    /* We expect exactly one element */
    if (nitems != 1)
        ereport(ERROR, (errcode(ERRCODE_INVALID_PARAMETER_VALUE),
                        errmsg("typmod must have exactly one integer value")));

    /* Convert cstring to int32 */
    typmod = atoi(DatumGetCString(elems[0]));

    /* Reject values > ITREE_MAX_LEVELS */
    if (typmod < 0 || typmod > ITREE_MAX_LEVELS)
        ereport(ERROR, (errcode(ERRCODE_INVALID_PARAMETER_VALUE),
                        errmsg("invalid typmod value %d", typmod)));

    PG_RETURN_INT32(typmod);
}


PG_FUNCTION_INFO_V1(itree_typmod_out);
Datum itree_typmod_out(PG_FUNCTION_ARGS) {
    int32 typmod = PG_GETARG_INT32(0);

    if (typmod >= 0)
        PG_RETURN_CSTRING(psprintf("(%d)", typmod));
    else
        PG_RETURN_CSTRING("");
}
