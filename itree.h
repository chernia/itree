#ifndef ITREE_H
#define ITREE_H

#include "postgres.h"
#include "fmgr.h"



#define ITREE_MAX_LEVELS 16  // Max 16 1-byte segments
#define ITREE_SIZE ITREE_MAX_LEVELS + 2 // 2 bytes for control
#define ITREE_MAX_SEGMENT_LENGTH 2  // Max 2 bytes for segment length

typedef struct {
    uint8_t control[2];  //all control bits used for segments
    uint8_t data[ITREE_MAX_LEVELS]; // 16 bytes for segments
} itree;


#define DatumGetITree(X) ((itree *)DatumGetPointer(X))
#define ITreeGetDatum(X) PointerGetDatum(X)
#define PG_RETURN_ITREE(x) PG_RETURN_POINTER(x)
#define PG_GETARG_ITREE(n) DatumGetITree(PG_GETARG_DATUM(n))




//in out functions
PGDLLEXPORT Datum itree_in(PG_FUNCTION_ARGS);
PGDLLEXPORT Datum itree_out(PG_FUNCTION_ARGS);
PGDLLEXPORT Datum itree_typmod_in(PG_FUNCTION_ARGS);
PGDLLEXPORT Datum itree_typmod_out(PG_FUNCTION_ARGS);
//comparison functions
PGDLLEXPORT Datum itree_eq(PG_FUNCTION_ARGS);
PGDLLEXPORT Datum itree_cmp(PG_FUNCTION_ARGS);
PGDLLEXPORT Datum itree_lt(PG_FUNCTION_ARGS);
PGDLLEXPORT Datum itree_le(PG_FUNCTION_ARGS);
PGDLLEXPORT Datum itree_ge(PG_FUNCTION_ARGS);
PGDLLEXPORT Datum itree_gt(PG_FUNCTION_ARGS);
PGDLLEXPORT Datum itree_ne(PG_FUNCTION_ARGS);
PGDLLEXPORT Datum itree_is_descendant(PG_FUNCTION_ARGS);
 PGDLLEXPORT Datum itree_is_ancestor(PG_FUNCTION_ARGS);
PGDLLEXPORT Datum itree_ilevel(PG_FUNCTION_ARGS);
/* Concatenation functions */
PGDLLEXPORT Datum itree_additree(PG_FUNCTION_ARGS);
PGDLLEXPORT Datum itree_addint(PG_FUNCTION_ARGS);
PGDLLEXPORT Datum itree_intadd(PG_FUNCTION_ARGS);

//helper functions
void set_control_bit(itree* tree_instance, int data_index, int bit_value);
int get_control_bit(const itree* tree_instance, int data_index);
int itree_get_segments(itree *tree, uint16_t *segments);


#endif