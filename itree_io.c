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
    // int32 typmod = PG_GETARG_INT32(1); this is TypeId, not typmod
    // int32 typmod = PG_GETARG_INT32(2); //always -1
    itree *result = (itree *)palloc(ITREE_SIZE);
    int levels = 0, byte_pos = 0;
    char *ptr;
    int32 max_levels = ITREE_MAX_LEVELS;

/*  int32 max_levels = (typmod >= 0) ? typmod : ITREE_MAX_LEVELS;
    elog(LOG, "itree_in input = %s, typmod = %d", input, typmod);
 
    if (max_levels < 1 || max_levels > ITREE_MAX_LEVELS) {
        elog(LOG, "itree_in: Invalid max_levels %d, using default 14", max_levels);
        max_levels = ITREE_MAX_LEVELS;
    }
 */
    if (!input || !*input) {
        ereport(ERROR, (errcode(ERRCODE_INVALID_TEXT_REPRESENTATION),
                        errmsg("itree input cannot be empty or null")));
    }

    result->control[0] = 0xFF;
    result->control[1] = 0xFF;
    memset(result->data, 0, sizeof(result->data));

    ptr = input;

    while (levels < max_levels && byte_pos < ITREE_MAX_LEVELS) {
        long val = atol(ptr);
        if (val <= 0) {
            ereport(ERROR, (errcode(ERRCODE_INVALID_TEXT_REPRESENTATION),
                            errmsg("itree segment must be positive (got %ld)", val)));
        }

        if (val <= 255) {
            result->data[byte_pos++] = (uint8_t)val;
            levels++;
        } else if (val <= 65535 && byte_pos + 1 < sizeof(result->data)) {
            result->data[byte_pos++] = (uint8_t)(val >> 8);
            result->data[byte_pos] = (uint8_t)(val & 0xFF);
            if (byte_pos < 8) {
                result->control[0] &= ~(1 << (7 - byte_pos));
            } else {
                result->control[1] &= ~(1 << (15 - byte_pos));
            }
            byte_pos++;
            levels++;
        } else {
            ereport(ERROR, (errcode(ERRCODE_INVALID_TEXT_REPRESENTATION),
                            errmsg("itree segment %ld too large or exceeds space", val)));
        }

        ptr = strchr(ptr, '.');
        if (!ptr) break;
        ptr++;
        if (!*ptr) break;
    }

    if (levels > max_levels) {
        ereport(ERROR, (errcode(ERRCODE_INVALID_TEXT_REPRESENTATION),
                        errmsg("itree exceeds max levels (%d)", max_levels)));
    }

    PG_RETURN_ITREE(result);
}

/** Convert an itree Datum to cstring */
PG_FUNCTION_INFO_V1(itree_out);
Datum itree_out(PG_FUNCTION_ARGS) {
    itree *tree = PG_GETARG_ITREE(0);
    char buffer[ITREE_MAX_LEVELS * 6];
    char *result;
    int len = 0, bit_pos = 0;

    while (bit_pos < ITREE_MAX_LEVELS && tree->data[bit_pos] != 0) {
        bool is_2byte = false;
        if (bit_pos + 1 < ITREE_MAX_LEVELS) {
            int shift = (bit_pos + 1 < 8) ? (7 - (bit_pos + 1)) : (15 - (bit_pos + 1));
            uint8_t control = (bit_pos + 1 < 8) ? tree->control[0] : tree->control[1];
            is_2byte = !((control >> shift) & 1);
        }

        if (len > 0) buffer[len++] = '.';

        if (is_2byte && bit_pos + 1 < ITREE_MAX_LEVELS && tree->data[bit_pos + 1] != 0) {
            uint16_t val = (tree->data[bit_pos] << 8) | tree->data[bit_pos + 1];
            len += sprintf(buffer + len, "%u", val);
            bit_pos += 2;
        } else {
            len += sprintf(buffer + len, "%u", tree->data[bit_pos]);
            bit_pos++;
        }
    }

    result = palloc(len + 1);
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

/**
 * Compare two itree values for equality.
 */
PG_FUNCTION_INFO_V1(itree_eq);
Datum itree_eq(PG_FUNCTION_ARGS){
    itree *a = (itree *)PG_GETARG_POINTER(0);
    itree *b = (itree *)PG_GETARG_POINTER(1);
    int i;

    for (i = 0; i < ITREE_SIZE; i++) {
        if (a->data[i] != b->data[i]) {
            PG_RETURN_BOOL(false);
        }
    }
    PG_RETURN_BOOL(true);
}


