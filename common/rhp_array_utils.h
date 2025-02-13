#ifndef RHP_ARRAY_UTILS_H
#define RHP_ARRAY_UTILS_H

/* Consume should specify how to handle error message */
#ifndef ARR_ERRLOG
#error "The consumer of this library must define ARR_ERRLOG, the loggin function "
#endif

#ifndef ARR_ERRACTION
#error "The consumer of this library must define ARR_ERRACTION, the function called in case of an error"
#endif

#include <assert.h>

#include "rhp_basic_memory.h"
#include "rhpgui_macros.h"

#ifndef RHP_MAX
#define RHP_MAX(a,b)        (((a) < (b)) ? (b) : (a))
#endif

#define arr_nbytes(a, n) (sizeof(*(a)) + ((sizeof(((a)->arr[0]))) * (n)))

#define arr_add(a, elt) { assert(a); \
   if ((a)->len >= (a)->max) { \
      (a)->max = RHP_MAX(2*((a)->max), ((a)->len+10)); \
      (a) = myrealloc((a)->arr, arr_nbytes((a), (a)->max)); \
      if (!(a)) { \
         ARR_ERRLOG("FATAL ERROR: allocation failure\n"); \
         ARR_ERRACTION; \
      } \
   } \
   (a)->arr[(a)->len++] = (elt); \
}

#define mymalloc malloc

#ifdef __cplusplus
#define CAST(a) (decltype(a))

#elif defined(__STDC_VERSION__) && (__STDC_VERSION__ >= 202300L)

#define CAST(a) (typeof(a))

#else

#define CAST(a) 

#endif

#define arr_init_size(a, n) { \
   (a) = CAST(a) mymalloc(arr_nbytes(a, n)); \
   if (!(a)) { \
      ARR_ERRLOG("FATAL ERROR: allocation failure\n"); \
      ARR_ERRACTION; \
   } \
   (a)->len = 0; (a)->max = n; \
}

#define arr_add_nochk(a, elt) { \
   assert(a->len < a->max); \
   a->arr[a->len++] = elt; \
}

#define arr_getlast(a) ( assert((a)->len > 0), &((a)->arr[(a)->len-1]) )

#define arr_free(a, fn) if (a) { \
   for (u32 i = 0, len = (a)->len; i < len; ++i) { \
      fn(&(a)->arr[i]); \
   } \
}

#define darr_free(da, fn) if (da) { \
   for (u32 i = 0, len = (da)->len; i < len; ++i) { \
      fn(&(da)->darr[i]); \
   } \
   free((da)->darr); \
}

#endif // RHP_ARRAY_UTILS_H
