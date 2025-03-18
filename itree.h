#ifndef ITREE_H
#define ITREE_H

#include "postgres.h"

typedef struct {
    uint8_t control[2];  // 16 control bits
    uint8_t data[14];    // 14 data bytes
} itree;

#define ITREE_SIZE 16
#define ITREE_MAX_LEVELS 14
/* Function prototypes */
Datum itree_in(PG_FUNCTION_ARGS);
Datum itree_out(PG_FUNCTION_ARGS);
Datum itree_eq(PG_FUNCTION_ARGS);
Datum itree_is_descendant(PG_FUNCTION_ARGS);
Datum itree_is_ancestor(PG_FUNCTION_ARGS);
Datum itree_extract_value(PG_FUNCTION_ARGS);
Datum itree_consistent(PG_FUNCTION_ARGS);
Datum itree_typmod_in(PG_FUNCTION_ARGS);
Datum itree_typmod_out(PG_FUNCTION_ARGS);

/* Datum macros */
#define DatumGetITree(X) ((itree *)DatumGetPointer(X))
#define ITreeGetDatum(X) PointerGetDatum(X)
#define PG_RETURN_ITREE(x) PG_RETURN_POINTER(x)
#define PG_GETARG_ITREE(n) DatumGetITree(PG_GETARG_DATUM(n))

#endif