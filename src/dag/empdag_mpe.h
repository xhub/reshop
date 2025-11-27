#include "asprintf.h"

#include <assert.h>

#include "empdag.h"
#include "macros.h"
#include "mp_equil.h"



static inline void dagnash_array_init(DagNashArray *dat)
{
   assert(dat);
   dat->len = 0;
   dat->max = 0;

   dat->arr = NULL;
   dat->names = NULL;
   dat->arcs = NULL;
   dat->rarcs = NULL;
}

static inline int dagnash_array_resize(DagNashArray *dat, unsigned size)
{
   CALLOC_(dat->arr, Nash*, size);
   CALLOC_(dat->names, const char*, size);
   CALLOC_(dat->arcs, UIntArray, size);
   CALLOC_(dat->rarcs, UIntArray, size);

   return OK;
}

static inline int dagnash_array_reserve(DagNashArray *dat, unsigned reserve)
{
   unsigned max_lb = reserve + dat->len;
   if (max_lb > dat->max) {
      unsigned old_max = dat->max;
      unsigned new_elts = max_lb - old_max;
      dat->max = max_lb;

      REALLOC_(dat->arr, Nash*, max_lb);
      REALLOC_(dat->names, const char*, max_lb);
      REALLOC_(dat->arcs, UIntArray, max_lb);
      REALLOC_(dat->rarcs, UIntArray, max_lb);

      memset(&dat->arr[old_max], 0, new_elts * sizeof(*dat->arr));
      memset(&dat->names[old_max], 0, new_elts * sizeof(*dat->names));
      memset(&dat->arcs[old_max], 0, new_elts * sizeof(UIntArray));
      memset(&dat->rarcs[old_max], 0, new_elts * sizeof(UIntArray));
   }

   return OK;
}

static inline OWNERSHIP_HOLDS(2) OWNERSHIP_TAKES(3)
int dagnash_array_add(DagNashArray *dat, Nash* elt, const char *name)
{

   if (dat->len >= dat->max) {
      unsigned old_max = dat->max;
      unsigned max = dat->max = MAX(2*dat->max, dat->len+1);
      unsigned new_elts = max - old_max;

      REALLOC_(dat->arr, Nash*, max);
      REALLOC_(dat->names, const char*, max);
      REALLOC_(dat->arcs, UIntArray, max);
      REALLOC_(dat->rarcs, UIntArray, max);

      memset(&dat->arr[old_max], 0, new_elts * sizeof(*dat->arr));
      memset(&dat->names[old_max], 0, new_elts * sizeof(*dat->names));
      memset(&dat->arcs[old_max], 0, new_elts * sizeof(UIntArray));
      memset(&dat->rarcs[old_max], 0, new_elts * sizeof(UIntArray));
   }

   dat->arr[dat->len] = elt;
   dat->names[dat->len++] = name;

   return OK;
}

static inline int dagnash_array_copy(DagNashArray * restrict dat_dst,
                                     const DagNashArray * restrict dat_src,
                                     Model *mdl_dst)
{
   unsigned size = dat_src->len;
   if (size == 0) {
      dagnash_array_init(dat_dst);
      return OK;
   }

   dat_dst->len = size;
   dat_dst->max = size;

   MALLOC_(dat_dst->arr, Nash*, size);
   MALLOC_(dat_dst->names, const char*, size);
   MALLOC_(dat_dst->arcs, UIntArray, size);
   MALLOC_(dat_dst->rarcs, UIntArray, size);

   for (unsigned i = 0; i < size; i++) {
      if (dat_src->arr[i]) {
         A_CHECK(dat_dst->arr[i], nash_dup(dat_src->arr[i], mdl_dst));
 
         if (dat_src->names[i]) {
            A_CHECK(dat_dst->names[i], strdup(dat_src->names[i]));
         } else {
            dat_dst->names[i] = NULL;
         }

         S_CHECK(rhp_uint_copy(&dat_dst->arcs[i], &dat_src->arcs[i]));
         S_CHECK(rhp_uint_copy(&dat_dst->rarcs[i], &dat_src->rarcs[i]));
      } else {
         dat_dst->arr[i] = NULL;
         dat_dst->names[i] = NULL;
      }

   }

   return OK;
}

static inline void dagnash_array_free(DagNashArray *dat)
{
   unsigned len = dat->len;
   if (len > 0) {
      for (unsigned i = 0; i < len; ++i) {
         nash_free(dat->arr[i]);
         FREE(dat->names[i]);
         rhp_uint_empty(&dat->arcs[i]);
         rhp_uint_empty(&dat->rarcs[i]);
      }
      FREE(dat->arr);
      FREE(dat->names);
      FREE(dat->arcs);
      FREE(dat->rarcs);
   }
}
