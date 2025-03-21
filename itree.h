#ifndef ITREE_H
#define ITREE_H

#include "postgres.h"

typedef struct {
    uint8_t control[2];  // All 1s, simplifies logic
    uint8_t data[14];    // 14 data bytes, 0 marks end
} itree;

#define ITREE_SIZE 16
#define ITREE_MAX_LEVELS 14  // Max 14 1-byte or 7 2-byte segments

#define DatumGetITree(X) ((itree *)DatumGetPointer(X))
#define ITreeGetDatum(X) PointerGetDatum(X)
#define PG_RETURN_ITREE(x) PG_RETURN_POINTER(x)
#define PG_GETARG_ITREE(n) DatumGetITree(PG_GETARG_DATUM(n))

// Helpers
int itree_get_segments(itree *tree, uint16_t *segments);

#endif