#include "postgres.h"
#include "fmgr.h"
#include "utils/array.h"
#include "utils/typcache.h"
#include "itree.h"

PG_MODULE_MAGIC;
PG_FUNCTION_INFO_V1(itree_in);
Datum itree_in(PG_FUNCTION_ARGS) {
    char *input = PG_GETARG_CSTRING(0);
    int32 max_levels = PG_GETARG_INT32(1);
    itree *result;  // Pointer to allocated struct
    int levels = 0;
    char *ptr;

    if (max_levels < 1 || max_levels > ITREE_MAX_LEVELS) {
        max_levels = 15;
    }

    if (!input || !*input) {
        ereport(ERROR, (errcode(ERRCODE_INVALID_TEXT_REPRESENTATION),
                        errmsg("itree input cannot be empty or null")));
    }

    result = (itree *)palloc(ITREE_SIZE);
    memset(result->data, 0, ITREE_SIZE);  // Zero-initialize

    ptr = input;
    while (levels < ITREE_MAX_LEVELS) {
        long val = atol(ptr);
        if (val < 0 || val > 255) {
            ereport(ERROR, (errcode(ERRCODE_INVALID_TEXT_REPRESENTATION),
                            errmsg("itree segment must be 0–255")));
        }
        result->data[levels++] = (uint8_t)val;

        ptr = strchr(ptr, '.');
        if (!ptr) break;
        ptr++;
        if (!*ptr) break;
    }

    if (levels > max_levels) {
        ereport(ERROR, (errcode(ERRCODE_INVALID_TEXT_REPRESENTATION),
                        errmsg("itree exceeds max levels (%d)", max_levels)));
    }

    PG_RETURN_ITREE(result);  // Return pointer
}

PG_FUNCTION_INFO_V1(itree_out);
Datum itree_out(PG_FUNCTION_ARGS) {
    //itree *tree = (itree *)PG_GETARG_POINTER(0);
    itree *tree = PG_GETARG_ITREE(0);
    char buffer[ITREE_MAX_LEVELS * 4];
    char *result;
    int i, len = 0;

    for (i = 0; i < ITREE_MAX_LEVELS && tree->data[i]; i++) {
        if (i > 0) buffer[len++] = '.';
        len += sprintf(buffer + len, "%d", tree->data[i]);
    }
    result = palloc(len + 1);
    memcpy(result, buffer, len);
    result[len] = '\0';
    PG_RETURN_CSTRING(result);
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



PG_FUNCTION_INFO_V1(itree_typmod_in);
Datum itree_typmod_in(PG_FUNCTION_ARGS) {
    ArrayType *ta = PG_GETARG_ARRAYTYPE_P(0);
    int32 typmod = 15;

    if (ARR_DIMS(ta)[0] > 0) {
        char *typmod_str = (char *)ARR_DATA_PTR(ta);
        typmod = atoi(typmod_str);
        if (typmod < 1 || typmod > ITREE_MAX_LEVELS) {
            ereport(ERROR, (errcode(ERRCODE_INVALID_PARAMETER_VALUE),
                            errmsg("max levels must be 1–16")));
        }
    }
    PG_RETURN_INT32(typmod);
}

PG_FUNCTION_INFO_V1(itree_typmod_out);
Datum itree_typmod_out(PG_FUNCTION_ARGS) {
    int32 typmod = PG_GETARG_INT32(0);
    char *result;

    if (typmod < 0) { // -1 means no typmod
        result = pstrdup("");
    } else {
        char buf[16];
        snprintf(buf, sizeof(buf), "%d", typmod);
        result = pstrdup(buf);
    }
    PG_RETURN_CSTRING(result);
}