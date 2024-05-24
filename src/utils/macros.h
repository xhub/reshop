#ifndef RESHOP_MACROS_H
#define RESHOP_MACROS_H

#include "reshop_config.h"

#include <assert.h>
#include <errno.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>

#include "compat.h"
#include "status.h"

//#define ABS(a)          (((a) >= 0)  ? (a) : (-(a)))
#define MIN(a,b)        (((a) < (b)) ? (a) : (b))
#define MAX(a,b)        (((a) < (b)) ? (b) : (a))
#define MAX3(a,b,c)     MAX(a, MAX(b,c))

#define RHP_CAT(x, y) x ## y
#define RHP_CAT2(x, y) RHP_CAT(x, y)
#define RHP_CAT3(x, y, z) RHP_CAT2(RHP_CAT2(x, y), z)

#define ARRAY_SIZE(arr) (sizeof(arr)/sizeof(arr[0]))
#define MACRO_STR(...) #__VA_ARGS__
#define XMACRO_STR(x) MACRO_STR(x)

/* See https://en.wikibooks.org/wiki/C_Programming/C_Reference/nonstandard/strlcpy  */
#define STRNCPY(dst, src, n) strncpy (dst, src, n)
#define STRNCPY_FIXED(dst, src) strncpy(dst, src, sizeof (dst)-1); dst[sizeof (dst)-1] = '\0';

#define MALLOCBYTES(ptr,type,bytes) ((ptr) = (type*)malloc(bytes)); assert((ptr))

#define MALLOCBYTES_(ptr,type,bytes) MALLOCBYTES((ptr),type,bytes); \
  if (RHP_UNLIKELY(!(ptr))) { return Error_InsufficientMemory; }

#define MALLOCBYTES_NULL(ptr,type,bytes) MALLOCBYTES((ptr),type,bytes); \
  if (RHP_UNLIKELY(!(ptr))) { return NULL; }

#define MALLOCBYTES_EXIT(ptr,type,bytes) MALLOCBYTES((ptr),type,bytes); \
   if (RHP_UNLIKELY(!(ptr))) { status = Error_InsufficientMemory; goto _exit; };

#define MALLOCBYTES_EXIT_NULL(ptr,type,bytes) MALLOCBYTES((ptr),type,bytes); \
   if (RHP_UNLIKELY(!(ptr))) { goto _exit; };

#define MALLOC(ptr,type,n)  ((ptr) = (type*)malloc((n)*sizeof(type))); \
   assert(((ptr) || n == 0) && "Allocation error for object " #ptr); \
/*
#define MALLOC(ptr,type,n)  ((ptr) = (type*)calloc((n),sizeof(type))); \
   assert(((ptr) || n == 0) && "Allocation error for object " #ptr);
*/

#define MALLOC_(ptr,type,n) MALLOC(ptr,type,n) \
   if (RHP_UNLIKELY(!(ptr))) { return Error_InsufficientMemory; }

#define MALLOC_NULL(ptr,type,n)  MALLOC(ptr,type,n); \
   if (RHP_UNLIKELY(!(ptr))) { return NULL; };

#define MALLOC_EXIT(ptr,type,n)  MALLOC(ptr,type,n); \
   if (RHP_UNLIKELY(!(ptr))) { status = Error_InsufficientMemory; goto _exit; };

#define MALLOC_EXIT_NULL(ptr,type,n)  MALLOC(ptr,type,n); \
   if (RHP_UNLIKELY(!(ptr))) { goto _exit; };

#define CALLOC(ptr,type,n)  ((ptr) = (type*)calloc((n),sizeof(type))); \
   assert(((ptr) || n == 0) && "Allocation error for object " #ptr);

#define CALLOC_(ptr,type,n) CALLOC(ptr,type,n) \
    if (RHP_UNLIKELY(!(ptr))) { return Error_InsufficientMemory; }

#define CALLOC_NULL(ptr,type,n) CALLOC(ptr,type,n) \
    if (RHP_UNLIKELY(!(ptr))) { error("%s :: allocation for #ptr of type #type and size %d failed\n", \
        __func__, n); return NULL; }

#define CALLOC_EXIT(ptr,type,n)  CALLOC(ptr,type,n); \
   if (RHP_UNLIKELY(!(ptr))) { status = Error_InsufficientMemory; goto _exit; };

#define CALLOC_EXIT_NULL(ptr,type,n)  CALLOC(ptr,type,n); \
   if (RHP_UNLIKELY(!(ptr))) { goto _exit; };

ALLOC_SIZE(2) MALLOC_ATTR_SIMPLE
static inline void *myrealloc(void *ptr, size_t size)
{
#if defined(__GNUC__) && !defined(__clang__)
#pragma GCC diagnostic ignored "-Wuse-after-free"
#endif
   void* oldptr = ptr;
   void *newptr  = realloc(ptr, size); /* NOLINT(bugprone-suspicious-realloc-usage) */
   if (RHP_UNLIKELY(!newptr && errno == ENOMEM && oldptr)) { free(oldptr); }
   return newptr;
#if defined(__GNUC__) && !defined(__clang__)
#pragma GCC diagnostic pop
#endif
}

#define REALLOC(ptr,type,n) { (ptr) = (type*)myrealloc(ptr, (n)*sizeof(type)); \
   assert(((ptr) || (n) == 0) && "Allocation error for object " #ptr); }

#define REALLOC_(ptr,type,n)  REALLOC(ptr,type,n) \
   if (RHP_UNLIKELY(!(ptr)) || (n) == 0) { return Error_InsufficientMemory; }

#define REALLOC_NULL(ptr,type,n)  REALLOC(ptr,type,n) \
   if (RHP_UNLIKELY(!(ptr))) { return NULL; }

#define REALLOC_EXIT(ptr,type,n)  REALLOC(ptr,type,n) \
   if (RHP_UNLIKELY(!(ptr))) { status = Error_InsufficientMemory; goto _exit; }

#define REALLOC_BYTES(ptr,size,type) ((ptr) = (type*)myrealloc((ptr), size)); \
   if (RHP_UNLIKELY(!(ptr))) { return Error_InsufficientMemory; }


#define FREE(ptr) do {                                  \
      if (RHP_LIKELY(ptr)) { free((void*)(ptr)); (ptr) = NULL; }   \
   } while(0)


/* ----------------------------------------------------------------------
 * Compile time control over devel and debug output
 * ---------------------------------------------------------------------- */

void backtrace_(const char *expr, int status);

#if defined(RHP_OUTPUT_BACKTRACE)
#define BACKTRACE(EXPR, status) backtrace_(XMACRO_STR(EXPR), status)
#else
#define BACKTRACE(EXPR, status)
#endif

#ifdef RHP_EXTRA_DEBUG_OUTPUT
#define DPRINT(STR, ...) printout(PO_INFO, STR, __VA_ARGS__)
#define DPRINTF(X) X
#else
#define DPRINTF(X)
#define DPRINT(STR, ...)
#endif


#define S_CHECK(EXPR) { int status42 = EXPR; if (RHP_UNLIKELY(status42)) { \
  BACKTRACE((EXPR), status42); return status42; } }

#define SN_CHECK(EXPR) { int status42 = EXPR; if (RHP_UNLIKELY(status42)) { \
  BACKTRACE(EXPR, status42); return NULL; } }

#define SN_CHECK_EXIT(EXPR) { int status42 = EXPR; if (RHP_UNLIKELY(status42)) { \
  BACKTRACE(EXPR, status42); goto _exit; } }

#define S_CHECK_EXIT(EXPR) { int status42 = EXPR; if (RHP_UNLIKELY(status42)) { \
  BACKTRACE(EXPR, status42); status = status42; goto _exit; } }

#define N_CHECK(D,EXPR) { (D) = EXPR; if (RHP_UNLIKELY(!(D))) { \
  BACKTRACE((D, "=", EXPR), Error_NullPointer); return Error_NullPointer; } }

#define A_CHECK(D,EXPR) { (D) = EXPR; if (RHP_UNLIKELY(!(D))) { \
  BACKTRACE(EXPR, Error_InsufficientMemory); return Error_InsufficientMemory; } }

#define AA_CHECK(D,EXPR) { (D) = EXPR; if (RHP_UNLIKELY(!(D))) { BACKTRACE(EXPR, Error_NullPointer); \
  return NULL; } }

#define A_CHECK_EXIT(D,EXPR) { (D) = EXPR; if (RHP_UNLIKELY(!(D))) { \
  BACKTRACE(EXPR, Error_InsufficientMemory); status = Error_InsufficientMemory; goto _exit; } }

#define N_CHECK_EXIT(D,EXPR) { (D) = EXPR; if (RHP_UNLIKELY(!(D))) { \
  BACKTRACE((D, "=", EXPR), Error_NullPointer); status = Error_NullPointer; goto _exit; } }

#define AA_CHECK_EXIT(D,EXPR) { (D) = EXPR; if (RHP_UNLIKELY(!(D))) { BACKTRACE(EXPR, Error_NullPointer); goto _exit; } }

#define TO_IMPLEMENT(STR) { error("%s NOT IMPLEMENTED (yet): " STR "\n", __func__); return Error_NotImplemented; }
#define TO_IMPLEMENT_EXIT(STR) { error("%s NOT IMPLEMENTED (yet): " STR "\n", __func__); status = Error_NotImplemented; goto _exit; }


/* Glibc provides a handy strerror_r */
#if !defined(__clang_analyzer__) && defined(__GLIBC__) && (!(defined(_POSIX_C_SOURCE) && (_POSIX_C_SOURCE >= 200112L)) ||  (defined(_GNU_SOURCE)))
#define STRERROR(errcode, buf, size, msg) msg = strerror_r(errcode, buf, size)
#else
#define STRERROR(errcode, buf, size, msg) const char* tmp =  strerror(errcode); \
  strncpy(buf, tmp, size); msg = buf;
#endif


#define SYS_CALL(EXPR) { \
   int status42 = EXPR; \
   if (RHP_UNLIKELY(status42)) { \
      int errsv42 = errno; \
      error("System call '%s' failed!\n", #EXPR); \
      char *msg42, buf42[256]; \
      STRERROR(errsv42, buf42, sizeof(buf42)-1, msg42); \
      error("Error msg is: %s\n", msg42); \
   } \
}

#define IO_CALL(EXPR) { int status42 = EXPR; if (RHP_UNLIKELY(status42 < 0)) { \
  error("%s :: write error %d\n", __func__, status42); \
  return Error_SystemError; } }

#define IO_CALL_NULL(EXPR) { int status42 = EXPR; if (RHP_UNLIKELY(status42 < 0)) { \
  error("%s :: write error %d\n", __func__, status42); \
  return NULL; } }

#define IO_CALL_EXIT(EXPR) { int res42 = EXPR; if (RHP_UNLIKELY(res42 < 0)) { \
  error("%s :: write error %d\n", __func__, res42); \
  status = Error_SystemError; goto _exit;} }



#ifdef __linux__
#include <sys/prctl.h>
#define RHP_TRACE_ME() prctl(PR_SET_PTRACER, PR_SET_PTRACER_ANY, 0, 0, 0);

#else

#define RHP_TRACE_ME() 

#endif




#ifndef _UNISTD_H 

#ifndef MY_GETPID
#define MY_GETPID
extern int getpid(void);
#endif

#define GDB_STOP() \
fprintf(stderr, "PID: %d in %s", getpid(), __func__); /*NOLINT(bugprone-unused-return-value,cert-err33-c)*/ \
RHP_TRACE_ME(); __maybe_unused int c42 = getchar(); //NOLINT(bugprone-unused-return-value,cert-err33-c)

#else //_UNISTD_H

#define GDB_STOP() \
  fprintf(stderr, "PID: %d in %s", getpid(), __func__); /*NOLINT(bugprone-unused-return-value,cert-err33-c)*/ \
  RHP_TRACE_ME(); __maybe_unused int c42 = getchar(); //NOLINT(bugprone-unused-return-value,cert-err33-c)

#endif

#ifdef DO_POISON
#include "poison.h"
#endif

/* Check if string was truncated  */
#define SNPRINTF_CHECK(STR, SIZE, ...) { int res = snprintf(STR, SIZE, __VA_ARGS__); \
  if (res < 0) { error("%s :: error while calling snprintf for to fill" \
    #STR " with size" #SIZE "); return code is %d", __func__, res); return Error_SystemError; }\
  if (res >= SIZE) { error("%s :: error while calling snprintf to fill" \
    #STR " of size " #SIZE "; truncation happened! Please change the following path" \
    "to be shorter than %d\n", __func__, SIZE); error(__VA_ARGS__); return Error_SystemError; } }


/* From https://stackoverflow.com/questions/2124339/c-preprocessor-va-args-number-of-arguments/2124385#2124385 */
#define PP_NARG(...) \
         PP_NARG_(__VA_ARGS__,PP_RSEQ_N())
#define PP_NARG_(...) \
         PP_ARG_N(__VA_ARGS__)
#define PP_ARG_N( \
          _1, _2, _3, _4, _5, _6, _7, _8, _9,_10, \
         _11,_12,_13,_14,_15,_16,_17,_18,_19,_20, \
         _21,_22,_23,_24,_25,_26,_27,_28,_29,_30, \
         _31,_32,_33,_34,_35,_36,_37,_38,_39,_40, \
         _41,_42,_43,_44,_45,_46,_47,_48,_49,_50, \
         _51,_52,_53,_54,_55,_56,_57,_58,_59,_60, \
         _61,_62,_63,N,...) N
#define PP_RSEQ_N() \
         63,62,61,60,                   \
         59,58,57,56,55,54,53,52,51,50, \
         49,48,47,46,45,44,43,42,41,40, \
         39,38,37,36,35,34,33,32,31,30, \
         29,28,27,26,25,24,23,22,21,20, \
         19,18,17,16,15,14,13,12,11,10, \
         9,8,7,6,5,4,3,2,1,0

#endif /* MACROS_H */
