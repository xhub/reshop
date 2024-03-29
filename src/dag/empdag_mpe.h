#include "asprintf.h"

#include <assert.h>

#include "empdag.h"
#include "macros.h"
#include "mp_equil.h"



static inline void _mpe_namedlist_init(DagMpeArray *dat)
{
   assert(dat);
   dat->len = 0;
   dat->max = 0;

   dat->arr = NULL;
   dat->names = NULL;
   dat->arcs = NULL;
   dat->rarcs = NULL;
}

static inline int _mpe_namedlist_resize(DagMpeArray *dat, unsigned size)
{
   CALLOC_(dat->arr, Mpe*, size);
   CALLOC_(dat->names, const char*, size);
   CALLOC_(dat->arcs, UIntArray, size);
   CALLOC_(dat->rarcs, UIntArray, size);

   return OK;
}

static inline int _mpe_namedlist_reserve(DagMpeArray *dat, unsigned reserve)
{
   unsigned max_lb = reserve + dat->len;
   if (max_lb > dat->max) {
      unsigned old_max = dat->max;
      unsigned new_elts = max_lb - old_max;
      dat->max = max_lb;

      REALLOC_(dat->arr, Mpe*, max_lb);
      REALLOC_(dat->names, const char*, max_lb);
      REALLOC_(dat->arcs, UIntArray, max_lb);
      REALLOC_(dat->rarcs, UIntArray, max_lb);

      memset(&dat->arr[old_max], 0, new_elts * sizeof(unsigned));
      memset(&dat->names[old_max], 0, new_elts * sizeof(const char*));
      memset(&dat->arcs[old_max], 0, new_elts * sizeof(UIntArray));
      memset(&dat->rarcs[old_max], 0, new_elts * sizeof(UIntArray));
   }

   return OK;
}

static inline OWNERSHIP_TAKES(2) OWNERSHIP_TAKES(3)
int _mpe_namedlist_add(DagMpeArray *dat, Mpe* elt, const char *name)
{

   if (dat->len >= dat->max) {
      unsigned old_max = dat->max;
      unsigned max = dat->max = MAX(2*dat->max, dat->len+1);
      unsigned new_elts = max - old_max;

      REALLOC_(dat->arr, Mpe*, max);
      REALLOC_(dat->names, const char*, max);
      REALLOC_(dat->arcs, UIntArray, max);
      REALLOC_(dat->rarcs, UIntArray, max);

      memset(&dat->arr[old_max], 0, new_elts * sizeof(unsigned));
      memset(&dat->names[old_max], 0, new_elts * sizeof(const char*));
      memset(&dat->arcs[old_max], 0, new_elts * sizeof(UIntArray));
      memset(&dat->rarcs[old_max], 0, new_elts * sizeof(UIntArray));
   }

   dat->arr[dat->len] = elt;
   dat->names[dat->len++] = name;

   return OK;
}

static inline int _mpe_namedlist_copy(DagMpeArray * restrict dat,
                                const DagMpeArray * restrict dat_src,
                                Model *mdl)
{
   unsigned size = dat_src->len;
   if (size == 0) {
      _mpe_namedlist_init(dat);
      return OK;
   }

   dat->len = size;
   dat->max = size;

   MALLOC_(dat->arr, Mpe*, size);
   MALLOC_(dat->names, const char*, size);
   MALLOC_(dat->arcs, UIntArray, size);
   MALLOC_(dat->rarcs, UIntArray, size);

   for (unsigned i = 0; i < size; i++) {
      if (dat_src->arr[i]) {
         A_CHECK(dat->arr[i], mpe_dup(dat_src->arr[i], mdl));
         
         if (dat_src->names[i]) {
              A_CHECK(dat->names[i], strdup(dat_src->names[i]));
              
         } else {
            dat->names[i] = NULL;
         }

         S_CHECK(rhp_uint_copy(&dat->arcs[i], &dat_src->arcs[i]));
         S_CHECK(rhp_uint_copy(&dat->rarcs[i], &dat_src->rarcs[i]));
      } else {
         dat->arr[i] = NULL;
         dat->names[i] = NULL;
      }

   }

   return OK;
}

static inline void _mpe_namedlist_free(DagMpeArray *dat)
{
   unsigned len = dat->len;
   if (len > 0) {
      for (unsigned i = 0; i < len; ++i) {
         mpe_free(dat->arr[i]);
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
