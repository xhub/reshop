/* The following defines influence the behavior
 *
 * - IDX_TYPE            Type of the index (default is unisgned)
 * - RHP_LOCAL_SCOPE:    define this if this is only used locally (in a header or just one compilation unit)
 * - RHP_DEFINE_STRUCT:  if defined, then the structure is defined.
 */


#ifndef RHP_LIST_PREFIX
#error "Must declare RHP_LIST_PREFIX"
#endif

#ifndef RHP_LIST_TYPE
#error "Must declare RHP_LIST_TYPE"
#endif

#ifndef RHP_ELT_TYPE
#error "Must declare RHP_ELT_TYPE"
#endif

#ifndef RHP_ELT_DUP
#error "Must declare RHP_ELT_DUP"
#endif

#ifndef RHP_ELT_FREE
#error "Must declare RHP_ELT_FREE"
#endif

#include <assert.h>
#include "macros.h"


#ifndef RHP_LIST_IDENT
#define RHP_LIST_IDENT list
#endif

#ifndef IDX_TYPE
#define IDX_TYPE unsigned
#endif

#define RHP_CONCAT(x, y) x ## _ ## y
#define _RHP_MAKE_STR(x, y) RHP_CONCAT(x,y)
#define RHP_MAKE_STR(x) _RHP_MAKE_STR(RHP_LIST_PREFIX,x)

#ifdef RHP_DEFINE_STRUCT
struct RHP_LIST_TYPE {
   IDX_TYPE len;
   IDX_TYPE max;
   RHP_ELT_TYPE *RHP_LIST_IDENT;
   const char **names;
};
#endif

#ifdef RHP_LOCAL_SCOPE
#define FN_SCOPE static inline
#else
#define FN_SCOPE
#endif

FN_SCOPE void RHP_MAKE_STR(init)(struct RHP_LIST_TYPE *dat)
{
   assert(dat);
   dat->len = 0;
   dat->max = 0;
   dat->RHP_LIST_IDENT = NULL;
   dat->names = NULL;
   dat->Carcs = NULL;
   dat->rarcs = NULL;
}

FN_SCOPE int RHP_MAKE_STR(resize)(struct RHP_LIST_TYPE *dat, IDX_TYPE size)
{
   CALLOC_(dat->RHP_LIST_IDENT, RHP_ELT_TYPE, size);
   CALLOC_(dat->names, const char*, size);
   CALLOC_(dat->Carcs, UIntArray, size);
   CALLOC_(dat->rarcs, UIntArray, size);

   return OK;
}

FN_SCOPE int RHP_MAKE_STR(reserve)(struct RHP_LIST_TYPE *dat, IDX_TYPE reserve)
{
   unsigned max_lb = reserve + dat->len;
   if (max_lb > dat->max) {
      unsigned old_max = dat->max;
      unsigned new_elts = max_lb - old_max;
      dat->max = max_lb;
      REALLOC_(dat->RHP_LIST_IDENT, RHP_ELT_TYPE, max_lb);
      REALLOC_(dat->names, const char*, max_lb);
      REALLOC_(dat->Carcs, UIntArray, max_lb);
      REALLOC_(dat->rarcs, UIntArray, max_lb);
      memset(&dat->RHP_LIST_IDENT[old_max], 0, new_elts * sizeof(RHP_ELT_TYPE));
      memset(&dat->names[old_max], 0, new_elts * sizeof(const char*));
      memset(&dat->Carcs[old_max], 0, new_elts * sizeof(UIntArray));
      memset(&dat->rarcs[old_max], 0, new_elts * sizeof(UIntArray));
   }

   return OK;
}

FN_SCOPE OWNERSHIP_HOLDS(2) OWNERSHIP_TAKES(3)
int RHP_MAKE_STR(add)(struct RHP_LIST_TYPE *dat, RHP_ELT_TYPE elt, const char *name)
{

   if (dat->len >= dat->max) {
      unsigned old_max = dat->max;
      unsigned max = dat->max = MAX(2*dat->max, dat->len+1);
      unsigned new_elts = max - old_max;
      REALLOC_(dat->RHP_LIST_IDENT, RHP_ELT_TYPE, max);
      REALLOC_(dat->names, const char*, max);
      REALLOC_(dat->Carcs, UIntArray, max);
      REALLOC_(dat->rarcs, UIntArray, max);
      memset(&dat->RHP_LIST_IDENT[old_max], 0, new_elts * sizeof(RHP_ELT_TYPE));
      memset(&dat->names[old_max], 0, new_elts * sizeof(const char*));
      memset(&dat->Carcs[old_max], 0, new_elts * sizeof(UIntArray));
      memset(&dat->rarcs[old_max], 0, new_elts * sizeof(UIntArray));
   }

   dat->RHP_LIST_IDENT[dat->len] = elt;
   dat->names[dat->len++] = name;

   return OK;
}

FN_SCOPE int RHP_MAKE_STR(copy)(struct RHP_LIST_TYPE * restrict dat,
                                const struct RHP_LIST_TYPE * restrict dat_src,
                                Model *mdl)
{
   unsigned size = dat_src->len;
   if (size == 0) {
      RHP_MAKE_STR(init)(dat);
      return OK;
   }

   dat->len = size;
   dat->max = size;

   MALLOC_(dat->RHP_LIST_IDENT, RHP_ELT_TYPE, size);
   MALLOC_(dat->names, const char*, size);
   MALLOC_(dat->Carcs, UIntArray, size);
   MALLOC_(dat->rarcs, UIntArray, size);

   for (unsigned i = 0; i < size; i++) {
      if (dat_src->RHP_LIST_IDENT[i]) {
         A_CHECK(dat->RHP_LIST_IDENT[i], RHP_ELT_DUP(dat_src->RHP_LIST_IDENT[i], mdl));
         
         if (dat_src->names[i]) {
              A_CHECK(dat->names[i], strdup(dat_src->names[i]));
              
         } else {
            dat->names[i] = NULL;
         }

         S_CHECK(rhp_uint_copy(&dat->Carcs[i], &dat_src->Carcs[i]));
         S_CHECK(rhp_uint_copy(&dat->rarcs[i], &dat_src->rarcs[i]));
      } else {
         dat->RHP_LIST_IDENT[i] = NULL;
         dat->names[i] = NULL;
      }

   }

   return OK;
}

FN_SCOPE void RHP_MAKE_STR(free)(struct RHP_LIST_TYPE *dat)
{
   unsigned len = dat->len;
   if (len > 0) {
      for (unsigned i = 0; i < len; ++i) {
         RHP_ELT_FREE(dat->RHP_LIST_IDENT[i]);
         FREE(dat->names[i]);
         rhp_uint_empty(&dat->Carcs[i]);
         rhp_uint_empty(&dat->rarcs[i]);
      }
      FREE(dat->RHP_LIST_IDENT);
      FREE(dat->names);
      FREE(dat->Carcs);
      FREE(dat->rarcs);
   }
}

#undef RHP_LIST_PREFIX
#undef RHP_LIST_TYPE
#undef RHP_ELT_TYPE
#undef RHP_ELT_FREE
#undef RHP_ELT_DUP
#undef RHP_LIST_IDENT
#undef IDX_TYPE
#undef RHP_CONCAT
#undef _RHP_MAKE_STR
#undef RHP_MAKE_STR
#undef FN_SCOPE
#undef RHP_DEFINE_STRUCT
#undef RHP_LOCAL_SCOPE
