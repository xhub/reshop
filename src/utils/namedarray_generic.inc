/* The following defines influence the behavior
 *
 * - IDX_TYPE            Type of the index (default is unisgned)
 * - RHP_LOCAL_SCOPE:    define this if this is only used locally (in a header or just one compilation unit)
 * - RHP_DEFINE_STRUCT:  if defined, then the structure is defined.
 * - RHP_ONLY_DEF:       if defined, then only the declaration of the function are done.
 *                       This is useful when including this in a header
 */

#ifndef RHP_ARRAY_PREFIX
#error "Must declare RHP_ARRAY_PREFIX"
#endif

#ifndef RHP_ARRAY_TYPE
#error "Must declare RHP_ARRAY_TYPE"
#endif

#ifndef RHP_ELT_TYPE
#error "Must declare RHP_ELT_TYPE"
#endif

#ifndef IDX_TYPE
#define IDX_TYPE unsigned
#endif

#define RHP_CONCAT(x, y) x ## _ ## y
#define _RHP_MAKE_STR(x, y) RHP_CONCAT(x,y)
#define RHP_MAKE_STR(x) _RHP_MAKE_STR(RHP_ARRAY_PREFIX,x)

#include "macros.h"

#ifdef RHP_LOCAL_SCOPE
#define FN_SCOPE static inline
#else
#define FN_SCOPE
#endif

#ifdef RHP_DEFINE_STRUCT
struct RHP_ARRAY_TYPE {
  IDX_TYPE len;
  IDX_TYPE max;
  const char **names;
  RHP_ELT_TYPE *arr;
};
#endif

#ifdef RHP_ONLY_DEF
int RHP_MAKE_STR(init)(struct RHP_ARRAY_TYPE *dat);
int RHP_MAKE_STR(resize)(struct RHP_ARRAY_TYPE *dat, IDX_TYPE size);
int RHP_MAKE_STR(stripmem)(struct RHP_ARRAY_TYPE *dat);
int RHP_MAKE_STR(add)(struct RHP_ARRAY_TYPE *dat, RHP_ELT_TYPE elt, const char *name);
int RHP_MAKE_STR(at)(struct RHP_ARRAY_TYPE *dat, IDX_TYPE i);
int RHP_MAKE_STR(set)(struct RHP_ARRAY_TYPE *dat, RHP_ELT_TYPE* elts, IDX_TYPE len);
int RHP_MAKE_STR(find)(struct RHP_ARRAY_TYPE *dat, unsigned val);
int RHP_MAKE_STR(findbyname)(struct RHP_ARRAY_TYPE *dat, const char *name);
int RHP_MAKE_STR(findbyname_nocase)(struct RHP_ARRAY_TYPE *dat, const char *name);
void RHP_MAKE_STR(free)(struct RHP_ARRAY_TYPE *dat);

#else

FN_SCOPE void RHP_MAKE_STR(init)(struct RHP_ARRAY_TYPE *dat)
{
  dat->len = 0;
  dat->max = 0;
  dat->names = NULL;
  dat->arr = NULL;
}

FN_SCOPE int RHP_MAKE_STR(resize)(struct RHP_ARRAY_TYPE *dat, IDX_TYPE size)
{
  MALLOC_(dat->arr, RHP_ELT_TYPE, size);
  MALLOC_(dat->names, const char*, size);

  return OK;
}

FN_SCOPE int RHP_MAKE_STR(stripmem)(struct RHP_ARRAY_TYPE *dat)
{
  if (dat->max > dat->len) {
    REALLOC_(dat->arr, RHP_ELT_TYPE, dat->len);
    REALLOC_(dat->names, const char*, dat->len);
  }

  return OK;
}

#ifdef RHP_ELT_SORT
FN_SCOPE OWNERSHIP_TAKES(3)
int RHP_MAKE_STR(add)(struct RHP_ARRAY_TYPE *dat, RHP_ELT_TYPE elt, const char *name)
{

   if (dat->len >= dat->max) {
      dat->max = MAX(2*dat->max, dat->len+1);
      REALLOC_(dat->arr, RHP_ELT_TYPE, dat->max);
      REALLOC_(dat->names, const char*, dat->max);
   }

   /* Fast check (try to append the value at the end */
   if (dat->len == 0 || dat->arr[dat->len-1].RHP_ELT_SORT <= elt.RHP_ELT_SORT) {
      dat->arr[dat->len++] = elt;
      return OK;
   }

   /*  TODO(xhub) we should be able to start from pos = dat->len-1 :( */
   unsigned pos = dat->len-1;
   for (unsigned i = 0, len = dat->len-1; i < len; ++i, --pos) {
      if (dat->arr[pos].RHP_ELT_SORT <= elt.RHP_ELT_SORT) {
         pos++;
         break;
      }
   }

   memmove(&dat->arr[pos+1], &dat->arr[pos], (dat->len-pos) * sizeof(RHP_ELT_TYPE));
   dat->arr[pos] = elt;
   dat->len++;

   return OK;
}
#else /* RHP_ELT_SORT */
FN_SCOPE OWNERSHIP_TAKES(3)
int RHP_MAKE_STR(add)(struct RHP_ARRAY_TYPE *dat, RHP_ELT_TYPE elt, const char *name)
{

  if (dat->len >= dat->max) {
    dat->max = MAX(2*dat->max, dat->len+1);
    REALLOC_(dat->arr, RHP_ELT_TYPE, dat->max);
    REALLOC_(dat->names, const char*, dat->max);
  }

  dat->names[dat->len] = name;
  dat->arr[dat->len++] = elt;
  return OK;
}
#endif

FN_SCOPE const RHP_ELT_TYPE RHP_MAKE_STR(at)(struct RHP_ARRAY_TYPE *dat, IDX_TYPE i)
{
   return i < dat->len ? dat->arr[i] : RHP_ELT_INVALID;
}

FN_SCOPE int RHP_MAKE_STR(set)(struct RHP_ARRAY_TYPE *dat, RHP_ELT_TYPE* elts, const char **names, IDX_TYPE len)
{
  if (dat->len != 0) {
    return Error_UnExpectedData;
  } else if (dat->max < len) {
    return Error_SizeTooSmall;
  }

  memcpy(dat->arr, elts, sizeof(RHP_ELT_TYPE) * len);
  memcpy(dat->names, names, sizeof(const char*) * len);
  dat->len = len;

  return OK;
}

#ifdef RHP_ELT_SORT
FN_SCOPE int RHP_MAKE_STR(find)(struct RHP_ARRAY_TYPE *dat, unsigned val)
{
  unsigned min = 0;
  unsigned max = dat->len - 1;

   const RHP_ELT_TYPE * restrict arr = dat->arr;
  while (max >= min) {
      unsigned i = (min + max) / 2;
      unsigned ival = arr[i].RHP_ELT_SORT;

      if (val < ival) {
        if (i == 0) {
            goto _exit;
        }
        max = i - 1;
      } else if (val > ival) {
        min = i + 1;
      } else {
        return i;
      }
    }

_exit:
  return UINT_MAX;
}
#else /* RHP_ELT_SORT */
FN_SCOPE int RHP_MAKE_STR(find)(struct RHP_ARRAY_TYPE *dat, unsigned val)
{
  return Error_NotImplemented;
}
#endif

FN_SCOPE unsigned RHP_MAKE_STR(findbyname)(const struct RHP_ARRAY_TYPE *dat, const char *name)
{
   for (unsigned i = 0, len = dat->len; i < len; ++i) {
      if (!strcmp(name, dat->names[i])) {
         return i;
      }
   }
   return UINT_MAX;
}

FN_SCOPE unsigned RHP_MAKE_STR(findbyname_nocase)(const struct RHP_ARRAY_TYPE *dat, const char *name)
{
   for (unsigned i = 0, len = dat->len; i < len; ++i) {
      if (!strcasecmp(name, dat->names[i])) {
         return i;
      }
   }
   return UINT_MAX;
}

FN_SCOPE void RHP_MAKE_STR(free)(struct RHP_ARRAY_TYPE *dat)
{
   if (dat->len > 0) {
      FREE(dat->arr);
      FREE(dat->names);
   }
}

#ifdef RHP_ELT_FREE
FN_SCOPE void RHP_MAKE_STR(freeall)(struct RHP_ARRAY_TYPE *dat)
{
   RHP_ELT_TYPE *arr = dat->arr;
   assert(arr || dat->len == 0);
   for (unsigned i = 0, len = dat->len; i < len; ++i) {
#ifdef RHP_ELT_FREE_TAKE_ADDRESS
      RHP_ELT_FREE(&arr[i]);
#else
      RHP_ELT_FREE(arr[i]);
#endif
   }
   FREE(arr);
}
#endif

#endif /* RHP_ONLY_DEF */

#undef RHP_ARRAY_PREFIX
#undef RHP_ARRAY_TYPE
#undef RHP_ELT_TYPE
#undef RHP_ELT_SORT
#undef IDX_TYPE
#undef RHP_CONCAT
#undef _RHP_MAKE_STR
#undef RHP_MAKE_STR
#undef FN_SCOPE
#undef RHP_LOCAL_SCOPE
#undef RHP_DEFINE_STRUCT
#undef RHP_ONLY_DEF
#undef RHP_ELT_INVALID
