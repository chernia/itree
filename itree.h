#ifndef ITREE_H
#define ITREE_H

#include "postgres.h"

typedef struct {
    uint8_t data[16];  // 128 bits, 8-bit segments
} itree;

#define ITREE_SIZE 16
#define ITREE_MAX_LEVELS 16

/* Function prototypes */
Datum itree_in(PG_FUNCTION_ARGS);
Datum itree_out(PG_FUNCTION_ARGS);
Datum itree_is_descendant(PG_FUNCTION_ARGS);
Datum itree_is_ancestor(PG_FUNCTION_ARGS);
Datum itree_extract_value(PG_FUNCTION_ARGS);
Datum itree_consistent(PG_FUNCTION_ARGS);
Datum itree_typmod_in(PG_FUNCTION_ARGS);
Datum itree_typmod_out(PG_FUNCTION_ARGS);

#endif