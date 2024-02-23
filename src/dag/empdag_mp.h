#include "asprintf.h"

#include <assert.h>

#include "empdag.h"
#include "macros.h"
#include "mathprgm.h"

#define RHP_LOCAL_SCOPE 1
#define RHP_ARRAY_PREFIX _edgeVFs
#define RHP_ARRAY_TYPE VFedges
#define RHP_ELT_TYPE struct rhp_empdag_arcVF
#define RHP_ELT_SORT child_id
#define RHP_ELT_INVALID ((struct rhp_empdag_arcVF) {.child_id = UINT_MAX, .type = ArcVFUnset});
#include "array_generic.inc"

static int _edgeVFs_copy(VarcArray * restrict dat,
                         const VarcArray * restrict dat_src)
{
   /* ---------------------------------------------------------------------
    * The semantics of this function is to overwrite, without reading,
    * all the data in the destination struct. We could have a safecopy function
    * that checks whether the destination struct has some data. However, the latter
    * could not be used on uninitialized data
    * --------------------------------------------------------------------- */

   unsigned len = dat_src->len;
   dat->len = dat->max = len;

   if (len == 0) {
      dat->arr = NULL;
      return OK;
   }

   MALLOC_(dat->arr, Varc, len);
   memcpy(dat->arr, dat_src->arr, len*sizeof(Varc));

   return OK;
}


static inline void _mp_namedarray_init(struct mp_namedarray *dat)
{
   assert(dat);
   dat->len = 0;
   dat->max = 0;

   dat->arr = NULL;
   dat->names = NULL;
   dat->Carcs = NULL;
   dat->Varcs = NULL;
   dat->rarcs = NULL;
}

static inline int _mp_namedarray_resize(struct mp_namedarray *dat, unsigned size)
{
   CALLOC_(dat->arr, MathPrgm*, size);
   CALLOC_(dat->names, const char*, size);
   CALLOC_(dat->Carcs, UIntArray, size);
   CALLOC_(dat->Varcs, VarcArray, size);
   CALLOC_(dat->rarcs, UIntArray, size);

   return OK;
}

static inline int _mp_namedarray_reserve(struct mp_namedarray *dat, unsigned reserve)
{
   unsigned max_lb = reserve + dat->len;
   if (max_lb > dat->max) {
      unsigned old_max = dat->max;
      unsigned new_elts = max_lb - old_max;
      dat->max = max_lb;

      REALLOC_(dat->arr, MathPrgm*, max_lb);
      REALLOC_(dat->names, const char*, max_lb);
      REALLOC_(dat->Carcs, UIntArray, max_lb);
      REALLOC_(dat->Varcs, VarcArray, max_lb);
      REALLOC_(dat->rarcs, UIntArray, max_lb);

      memset(&dat->arr[old_max], 0, new_elts * sizeof(unsigned));
      memset(&dat->names[old_max], 0, new_elts * sizeof(const char*));
      memset(&dat->Carcs[old_max], 0, new_elts * sizeof(UIntArray));
      memset(&dat->Varcs[old_max], 0, new_elts * sizeof(VarcArray));
      memset(&dat->rarcs[old_max], 0, new_elts * sizeof(UIntArray));
   }

   return OK;
}

static inline OWNERSHIP_TAKES(2) OWNERSHIP_TAKES(3)
int _mp_namedarray_add(struct mp_namedarray *dat, MathPrgm* elt, const char *name)
{

   if (dat->len >= dat->max) {
      unsigned old_max = dat->max;
      unsigned max = dat->max = MAX(2*dat->max, dat->len+1);
      unsigned new_elts = max - old_max;

      REALLOC_(dat->arr, MathPrgm*, max);
      REALLOC_(dat->names, const char*, max);
      REALLOC_(dat->Carcs, UIntArray, max);
      REALLOC_(dat->Varcs, VarcArray, max);
      REALLOC_(dat->rarcs, UIntArray, max);

      memset(&dat->arr[old_max], 0, new_elts * sizeof(unsigned));
      memset(&dat->names[old_max], 0, new_elts * sizeof(const char*));
      memset(&dat->Carcs[old_max], 0, new_elts * sizeof(UIntArray));
      memset(&dat->Varcs[old_max], 0, new_elts * sizeof(VarcArray));
      memset(&dat->rarcs[old_max], 0, new_elts * sizeof(UIntArray));
   }

   dat->arr[dat->len] = elt;
   dat->names[dat->len++] = name;

   return OK;
}

static inline int _mp_namedarray_copy(struct mp_namedarray * restrict dat,
                                const struct mp_namedarray * restrict dat_src,
                                Model *mdl)
{
   unsigned size = dat_src->len;
   if (size == 0) {
      _mp_namedarray_init(dat);
      return OK;
   }

   dat->len = size;
   dat->max = size;

   MALLOC_(dat->arr, MathPrgm*, size);
   MALLOC_(dat->names, const char*, size);
   MALLOC_(dat->Carcs, UIntArray, size);
   MALLOC_(dat->Varcs, VarcArray, size);
   MALLOC_(dat->rarcs, UIntArray, size);

   for (unsigned i = 0; i < size; i++) {
      if (dat_src->arr[i]) {
         A_CHECK(dat->arr[i], mp_dup(dat_src->arr[i], mdl));
         
         if (dat_src->names[i]) {
              A_CHECK(dat->names[i], strdup(dat_src->names[i]));
              
         } else {
            dat->names[i] = NULL;
         }

         S_CHECK(rhp_uint_copy(&dat->Carcs[i], &dat_src->Carcs[i]));
         S_CHECK(_edgeVFs_copy(&dat->Varcs[i], &dat_src->Varcs[i]));
         S_CHECK(rhp_uint_copy(&dat->rarcs[i], &dat_src->rarcs[i]));

      } else {
         dat->arr[i] = NULL;
         dat->names[i] = NULL;
      }

   }

   return OK;
}

static inline void _mp_namedarray_free(struct mp_namedarray *dat)
{
   unsigned len = dat->len;
   if (len > 0) {
      for (unsigned i = 0; i < len; ++i) {
         mp_free(dat->arr[i]);
         FREE(dat->names[i]);
         rhp_uint_empty(&dat->Carcs[i]);
         rhp_uint_empty(&dat->rarcs[i]);
         _edgeVFs_free(&dat->Varcs[i]);
      }

      FREE(dat->arr);
      FREE(dat->names);
      FREE(dat->Carcs);
      FREE(dat->Varcs);
      FREE(dat->rarcs);
   }
}
