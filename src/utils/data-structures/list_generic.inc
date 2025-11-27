/* The following defines influence the behavior
 *
 * - IDX_TYPE            Type of the index (default is unisgned)
 * - RHP_LOCAL_SCOPE:    define this if this is only used locally (in a header or just one compilation unit)
 * - RHP_DEFINE_STRUCT:  if defined, then the structure is defined.
 * - RHP_ONLY_DEF:       if defined, then only the declaration of the function are done.
 *                       This is useful when including this in a header
 */

#ifndef RHP_LIST_PREFIX
#error "Must declare RHP_LIST_PREFIX"
#endif

#ifndef RHP_LIST_TYPE
#error "Must declare RHP_LIST_TYPE"
#endif

#ifndef RHP_ELT_TYPE
#error "Must declare RHP_ELT_TYPE, the type of the container elements"
#endif

#ifndef RHP_ELT_INVALID
#error "Must declare RHP_ELT_INVALID, the invalid value for an element"
#endif

#ifndef IDX_TYPE
#define IDX_TYPE unsigned
#endif

#include <stddef.h>
#include <string.h>

#include "macros.h"

#define RHP_CONCAT(x, y) x ## _ ## y
#define _RHP_MAKE_STR(x, y) RHP_CONCAT(x,y)
#define RHP_MAKE_STR(x) _RHP_MAKE_STR(RHP_LIST_PREFIX,x)

#ifdef RHP_LOCAL_SCOPE
#define FN_SCOPE static inline
#else
#define FN_SCOPE
#endif

#ifdef RHP_DEFINE_STRUCT
typedef struct RHP_LIST_TYPE {
  IDX_TYPE len;
  IDX_TYPE max;
  RHP_ELT_TYPE *list;
} RHP_LIST_TYPE;
#endif

#ifdef RHP_ONLY_DEF
int RHP_MAKE_STR(init)(struct RHP_LIST_TYPE *dat);
int RHP_MAKE_STR(resize)(struct RHP_LIST_TYPE *dat, IDX_TYPE size);
int RHP_MAKE_STR(stripmem)(struct RHP_LIST_TYPE *dat);
int RHP_MAKE_STR(add)(struct RHP_LIST_TYPE *dat, RHP_ELT_TYPE elt);
int RHP_MAKE_STR(at)(struct RHP_LIST_TYPE *dat, IDX_TYPE i);
int RHP_MAKE_STR(set)(struct RHP_LIST_TYPE *dat, RHP_ELT_TYPE* elts, IDX_TYPE len);
int RHP_MAKE_STR(find)(struct RHP_LIST_TYPE *dat, unsigned val);
void RHP_MAKE_STR(free)(struct RHP_LIST_TYPE *dat);

#else

FN_SCOPE void RHP_MAKE_STR(init)(struct RHP_LIST_TYPE *dat)
{
  dat->len = 0;
  dat->max = 0;
  dat->list = NULL;
}

FN_SCOPE int RHP_MAKE_STR(resize)(struct RHP_LIST_TYPE *dat, IDX_TYPE size)
{
   MALLOC_(dat->list, RHP_ELT_TYPE, size);
   dat->len = 0;
   dat->max = size;

  return OK;
}

FN_SCOPE int RHP_MAKE_STR(stripmem)(struct RHP_LIST_TYPE *dat)
{
  if (dat->max > dat->len) {
    REALLOC_(dat->list, RHP_ELT_TYPE, dat->len);
  }

  return OK;
}

#ifdef RHP_LIST_TAKES_OWNERSHIP
OWNERSHIP_HOLDS(2)
#endif
FN_SCOPE int RHP_MAKE_STR(add)(struct RHP_LIST_TYPE *dat, RHP_ELT_TYPE elt)
{

  if (dat->len >= dat->max) {
    dat->max = MAX(2*dat->max, dat->len+1);
    REALLOC_(dat->list, RHP_ELT_TYPE, dat->max);
  }

  dat->list[dat->len++] = elt;
  return OK;
}

FN_SCOPE RHP_ELT_TYPE RHP_MAKE_STR(at)(struct RHP_LIST_TYPE *dat, IDX_TYPE i)
{
   return i < dat->len ? dat->list[i] : RHP_ELT_INVALID;
}

FN_SCOPE int RHP_MAKE_STR(set)(struct RHP_LIST_TYPE *dat, RHP_ELT_TYPE* elts, IDX_TYPE len)
{
  if (dat->len != 0) {
    return Error_UnExpectedData;
  } else if (dat->max < len) {
    return Error_SizeTooSmall;
  }

  memcpy(dat->list, elts, sizeof(RHP_ELT_TYPE) * len);
  dat->len = len;

  return OK;
}

FN_SCOPE void RHP_MAKE_STR(free)(struct RHP_LIST_TYPE *dat)
{
   if (dat->max > 0) {
      FREE(dat->list);
   }
}

#ifdef RHP_ELT_FREE
FN_SCOPE void RHP_MAKE_STR(freeall)(struct RHP_LIST_TYPE *dat)
{
   RHP_ELT_TYPE *list = dat->list;
   assert(list || dat->len == 0);
   for (unsigned i = 0, len = dat->len; i < len; ++i) {
#ifdef RHP_ELT_FREE_TAKE_ADDRESS
      RHP_ELT_FREE(&list[i]);
#else
      RHP_ELT_FREE(list[i]);
#endif
   }
   FREE(list);
}
#endif

#ifdef RHP_ELT_SORT
FN_SCOPE int RHP_MAKE_STR(find)(struct RHP_LIST_TYPE *dat, unsigned val)
{
  unsigned min = 0;
  unsigned max = dat->len - 1;

   const RHP_ELT_TYPE * restrict arr = dat->list;
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
#endif /* RHP_ELT_SORT */

#endif /* RHP_ONLY_DEF */

#undef RHP_LIST_PREFIX
#undef RHP_LIST_TYPE
#undef RHP_ELT_TYPE
#undef RHP_ELT_INVALID
#undef RHP_ELT_FREE
#undef RHP_ELT_FREE_TAKE_ADDRESS
#undef RHP_ELT_SORT
#undef IDX_TYPE
#undef RHP_CONCAT
#undef _RHP_MAKE_STR
#undef RHP_MAKE_STR
#undef FN_SCOPE
#undef RHP_LOCAL_SCOPE
#undef RHP_DEFINE_STRUCT
#undef RHP_ONLY_DEF
#undef RHP_LIST_TAKES_OWNERSHIP
