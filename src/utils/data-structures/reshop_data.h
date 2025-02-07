#ifndef RESHOP_DATA_H
#define RESHOP_DATA_H

#include <stddef.h>

#include "compat.h"
#include "rhpidx.h"

/** @file reshop_data
 *
 *  @brief Basic data structures
 */

struct rhp_stack_idx;


/**  @brief Container for integer data */
typedef struct rhp_int_data {
   unsigned len;     /**< number of elements in the container  */
   unsigned max;     /**< maximum number of elements            */
   int *arr;         /**< elements                     */
} IntArray;

typedef struct rhp_uint_data {
   unsigned len;     /**< number of elements in the container  */
   unsigned max;     /**< maximum number of elements            */
   unsigned *arr;   /**< elements                     */
} UIntArray;

typedef struct rhp_obj_data {
   unsigned len;
   unsigned max;
   void** arr;
   int (*sort)(const void *a, const void *b);
} ObjArray;


void rhp_int_init(IntArray *dat) NONNULL;
int rhp_int_add(IntArray *dat, int v) NONNULL;
int rhp_int_addsorted(IntArray *dat, int v) NONNULL;
int rhp_int_addseq(IntArray *dat, int start, unsigned len) NONNULL;
bool rhp_int_chksorted(const IntArray *dat) NONNULL;
int rhp_int_copy(IntArray * restrict dat,
                 const IntArray * restrict dat_src) NONNULL;
void rhp_int_empty(IntArray *dat) NONNULL;
int rhp_int_extend(IntArray * restrict dat,
                   const IntArray * restrict dat_src) NONNULL;
int rhp_int_extend_sorted(IntArray * restrict dat,
                   const IntArray * restrict dat_src) NONNULL;
int rhp_int_extend_except(IntArray * restrict dat,
                          const IntArray * restrict dat_src,
                          int val) NONNULL;
int rhp_int_extend_except_sorted(IntArray * restrict dat,
                                 const IntArray * restrict dat_src,
                                 int val) NONNULL;
unsigned rhp_int_find(const IntArray *dat, int v) NONNULL;
unsigned rhp_int_findsorted(const IntArray *dat, int val) NONNULL;
void rhp_int_print(const IntArray *dat, unsigned mode) NONNULL;
int rhp_int_reserve(IntArray *dat, unsigned size) NONNULL;
int rhp_int_rmsorted(IntArray *dat, int v) NONNULL;
int rhp_int_rmsortednofail(IntArray *dat, int v) NONNULL;
int rhp_int_rm(IntArray *dat, int v) NONNULL;

static inline unsigned rhp_int_at(const IntArray *dat, unsigned i)
{
  return i < dat->len ? dat->arr[i] : IdxNotFound;
}

void rhp_obj_init(ObjArray *dat) NONNULL;
int rhp_obj_add(ObjArray *dat, void *v) NONNULL_AT(1);
void rhp_obj_empty(ObjArray *dat) NONNULL;
int rhp_obj_reserve(ObjArray *dat, unsigned size) NONNULL;


struct rhp_stack_gen* rhp_stack_gen_alloc(unsigned size);
void rhp_stack_gen_free(struct rhp_stack_gen *s);
int rhp_stack_gen_put(struct rhp_stack_gen *s, void *obj);
int rhp_stack_gen_pop(struct rhp_stack_gen *s, void **obj);
size_t rhp_stack_gen_len(struct rhp_stack_gen *s);

void rhp_uint_init(UIntArray *dat) NONNULL;
int rhp_uint_add(UIntArray *dat, unsigned v) NONNULL;
int rhp_uint_addsorted(UIntArray *dat, unsigned v) NONNULL;
int rhp_uint_addseq(UIntArray *dat, unsigned start, unsigned len) NONNULL;
int rhp_uint_addset(UIntArray *dat, const UIntArray *src) NONNULL;
int rhp_uint_adduniqsorted(UIntArray *dat, unsigned v) NONNULL;
int rhp_uint_adduniqnofail(UIntArray *dat, unsigned v) NONNULL;
unsigned rhp_uint_find(const UIntArray *dat, unsigned v) NONNULL;
unsigned rhp_uint_findsorted(const UIntArray *dat, unsigned val) NONNULL;
int rhp_uint_copy(UIntArray * restrict dat,
                 const UIntArray * restrict dat_src) NONNULL;
void rhp_uint_empty(UIntArray *dat) NONNULL;
int rhp_uint_reserve(UIntArray *dat, unsigned size) NONNULL;
int rhp_uint_rm(UIntArray *dat, unsigned v) NONNULL;
int rhp_uint_rmsorted(UIntArray *dat, unsigned v) NONNULL;
int rhp_uint_rmnofail(UIntArray *dat, unsigned v) NONNULL;
int rhp_int_set(IntArray *dat, unsigned idx, int v) NONNULL;

static inline unsigned rhp_uint_at(const UIntArray *dat, unsigned i)
{
  return i < dat->len ? dat->arr[i] : IdxNotFound;
}

#define rhp_idx_data rhp_int_data
#define rhp_idx_init rhp_int_init
#define rhp_idx_add rhp_int_add
#define rhp_idx_addsorted rhp_int_addsorted
#define rhp_idx_addseq rhp_int_addseq
#define rhp_idx_at rhp_int_at
#define rhp_idx_chksorted rhp_int_chksorted
#define rhp_idx_copy rhp_int_copy
#define rhp_idx_empty rhp_int_empty
#define rhp_idx_extend_sorted rhp_int_extend
#define rhp_idx_extend_except_sorted rhp_int_extend_except_sorted
#define rhp_idx_extend rhp_int_extend
#define rhp_idx_extend_except rhp_int_extend_except
#define rhp_idx_find rhp_int_find
#define rhp_idx_findsorted rhp_int_findsorted
#define rhp_idx_reserve rhp_int_reserve
#define rhp_idx_rmsorted rhp_int_rmsorted
#define rhp_idx_rmsortednofail rhp_int_rmsortednofail
#define rhp_idx_rm rhp_int_rm
#define rhp_idx_set rhp_int_set

#define mpidarray_data rhp_uint_data
#define mpidarray_init rhp_uint_init
#define mpidarray_add rhp_uint_add
#define mpidarray_addsorted rhp_uint_addsorted
#define mpidarray_addseq rhp_uint_addseq
#define mpidarray_at rhp_uint_at
#define mpidarray_copy rhp_uint_copy
#define mpidarray_empty rhp_uint_empty
#define mpidarray_find rhp_uint_find
#define mpidarray_findsorted rhp_uint_findsorted
#define mpidarray_reserve rhp_uint_reserve
#define mpidarray_rmsorted rhp_uint_rmsorted
#define mpidarray_rm rhp_uint_rm
#define mpidarray_set rhp_uint_set

#define daguidarray_data rhp_uint_data
#define daguidarray_init rhp_uint_init
#define daguidarray_add rhp_uint_add
#define daguidarray_addsorted rhp_uint_addsorted
#define daguidarray_adduniqsorted rhp_uint_adduniqsorted
#define daguidarray_addseq rhp_uint_addseq
#define daguidarray_at rhp_uint_at
#define daguidarray_copy rhp_uint_copy
#define daguidarray_empty rhp_uint_empty
#define daguidarray_find rhp_uint_find
#define daguidarray_findsorted rhp_uint_findsorted
#define daguidarray_reserve rhp_uint_reserve
#define daguidarray_rmsorted rhp_uint_rmsorted
#define daguidarray_rm rhp_uint_rm
#define daguidarray_rmnofail rhp_uint_rmnofail
#define daguidarray_set rhp_uint_set



typedef struct {
   int *data;
   unsigned size;
} IntScratch;

typedef struct {
   double *data;
   unsigned size;
} DblScratch;

void scratchint_empty(IntScratch *scratch) NONNULL;
void scratchint_init(IntScratch *scratch) NONNULL;
int scratchint_ensure(IntScratch *scratch, unsigned size) NONNULL;
void scratchdbl_empty(DblScratch *scratch) NONNULL;
void scratchdbl_init(DblScratch *scratch) NONNULL;
int scratchdbl_ensure(DblScratch *scratch, unsigned size) NONNULL;
#endif
