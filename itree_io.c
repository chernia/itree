#include "postgres.h"
#include "fmgr.h"
#include "utils/array.h"
#include "utils/typcache.h"
#include "utils/memutils.h"
#include "itree.h"

PG_MODULE_MAGIC;
PG_FUNCTION_INFO_V1(itree_in);
Datum itree_in(PG_FUNCTION_ARGS) {
    char *input = PG_GETARG_CSTRING(0);
    int32 max_levels = PG_GETARG_INT32(1);
    itree *result = (itree *)palloc(ITREE_SIZE);
    int levels = 0, byte_pos = 0;
    char *ptr;

    if (max_levels < 1 || max_levels > ITREE_MAX_LEVELS) {
        elog(LOG, "itree_in: Invalid max_levels %d, using default 14", max_levels);
        max_levels = 14;
    }

    if (!input || !*input) {
        ereport(ERROR, (errcode(ERRCODE_INVALID_TEXT_REPRESENTATION),
                        errmsg("itree input cannot be empty or null")));
    }

    // Initialize all control bits to 1 (assume all bytes start segments)
    result->control[0] = 0xFF;
    result->control[1] = 0xFF;
    memset(result->data, 0, sizeof(result->data));  // Clear data only

    ptr = input;

    while (levels < max_levels && byte_pos < ITREE_MAX_LEVELS) {
        long val = atol(ptr);
        if (val < 0) {
            ereport(ERROR, (errcode(ERRCODE_INVALID_TEXT_REPRESENTATION),
                            errmsg("itree segment must be non-negative")));
        }

        if (val <= 255) {
            result->data[byte_pos++] = (uint8_t)val;
            levels++;
        } else if (val <= 65535 && byte_pos + 1 < ITREE_MAX_LEVELS) {
            result->data[byte_pos++] = (uint8_t)(val >> 8);
            result->data[byte_pos] = (uint8_t)(val & 0xFF);
            // Clear control bit for continuation byte
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

PG_FUNCTION_INFO_V1(itree_out);
Datum itree_out(PG_FUNCTION_ARGS) {
    itree *tree = PG_GETARG_ITREE(0);
    char buffer[ITREE_MAX_LEVELS * 6];
    char *result;
    int i, len = 0, bit_pos = 0;

    for (i = 0; bit_pos < ITREE_MAX_LEVELS; i++) {
        bool is_2byte = false;
        uint8_t control = (i < 8) ? tree->control[0] : tree->control[1];
        int shift = (i < 8) ? (7 - i) : (15 - i);
        bool is_start = (control >> shift) & 1;

        if (!is_start && i > 0) {
            bit_pos++;  // Continuation byte, skip
            continue;
        }

        // Check next bit only if it’s within bounds and a continuation
        if (bit_pos + 1 < ITREE_MAX_LEVELS) {
            int next_i = i + 1;
            int next_shift = (next_i < 8) ? (7 - next_i) : (15 - next_i);
            uint8_t next_control = (next_i < 8) ? tree->control[0] : tree->control[1];
            if (next_shift >= 0 && !((next_control >> next_shift) & 1)) {
                is_2byte = true;  // Next byte is a continuation
            }
        }

        if (len > 0) buffer[len++] = '.';

        if (is_2byte && bit_pos + 1 < ITREE_MAX_LEVELS) {
            uint16_t val = (tree->data[bit_pos] << 8) | tree->data[bit_pos + 1];
            len += sprintf(buffer + len, "%u", val);
            bit_pos += 2;
        } else {
            len += sprintf(buffer + len, "%u", tree->data[bit_pos]);
            bit_pos++;
        }

        // Stop if no more segments (next control bit unset)
        if (bit_pos < ITREE_MAX_LEVELS) {
            int next_i = i + 1;
            int next_shift = (next_i < 8) ? (7 - next_i) : (15 - next_i);
            if (next_shift >= 0) {
                uint8_t next_control = (next_i < 8) ? tree->control[0] : tree->control[1];
                if (!((next_control >> next_shift) & 1)) {
                    break;  // No more segments
                }
            }
        }
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