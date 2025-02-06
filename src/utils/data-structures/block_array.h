#ifndef RHP_BLOCK_ARRAY
#define RHP_BLOCK_ARRAY

#include "compat.h"
#include "rhp_fwd.h"

/** @file block_array.h */

typedef struct {
   double *arr;
   unsigned size;
} DblArrayElt;

/** Collection of double arrays */
typedef struct {
   unsigned size;
   unsigned len;
   unsigned max;
   DblArrayElt blocks[] __counted_by(max);
} DblArrayBlock;

DblArrayBlock* dblarrs_allocA(M_ArenaLink *arena, unsigned max);
DblArrayBlock* dblarrs_new_(M_ArenaLink *arena, unsigned len, ...);

/* FIXME: remove as soon as possible */
#ifdef __INTEL_COMPILER
#define dblarrs_new(arena, ...) dblarrs_new_(arena, VA_NARG_TYPED(uintptr_t, __VA_ARGS__), __VA_ARGS__)
#else
#define dblarrs_new(arena, ...) dblarrs_new_(arena, PP_NARG(__VA_ARGS__), __VA_ARGS__)
#endif


int dblarrs_store(DblArrayBlock* dblarr, double *arr);
double dblarrs_fget(DblArrayBlock* dblarr, unsigned idx);

#endif
