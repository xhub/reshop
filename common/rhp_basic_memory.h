#ifndef RHP_BASIC_MEMORY_H
#define RHP_BASIC_MEMORY_H

#include <errno.h>
#include <stddef.h>
#include <stdlib.h>

#include "rhp_compiler_defines.h"

ALLOC_SIZE(2) MALLOC_ATTR_SIMPLE
static inline void* myrealloc(void *ptr, size_t size)
{
#if defined(__GNUC__) && !defined(__clang__) && (__GNUC__ > 11)
#pragma GCC diagnostic push
#pragma GCC diagnostic ignored "-Wuse-after-free"
#pragma GCC diagnostic ignored "-Wfree-nonheap-object"
#endif
   void* oldptr = ptr;
   void *newptr  = realloc(ptr, size); /* NOLINT(bugprone-suspicious-realloc-usage) */
   if (RHP_UNLIKELY(!newptr && errno == ENOMEM && oldptr)) { free(oldptr); }
   return newptr;
#if defined(__GNUC__) && !defined(__clang__) && (__GNUC__ > 11)
#pragma GCC diagnostic pop
#endif
}

#endif // RHP_BASIC_MEMORY_H
