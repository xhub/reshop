#ifndef RESHOP_COMPAT_H
#define RESHOP_COMPAT_H

#include "reshop_config.h"

#ifdef _WIN32
  #define DLLPRE ""
  #define DLLEXT ".dll"
#elif defined(__APPLE__)
  #define DLLPRE "lib"
  #define DLLEXT ".dylib"
#else
  #define DLLPRE "lib"
  #define DLLEXT ".so"
#endif

#define DLL_FROM_NAME(X) DLLPRE X  DLLEXT

#if defined(_WIN32) || defined(_WIN64)
  #include <string.h>
//  #define snprintf _snprintf
//  #define vsnprintf _vsnprintf
  #define strcasecmp _stricmp
  #define strncasecmp _strnicmp
  #define strdup _strdup

  #include <direct.h>
  #define mkdir(X,Y) _mkdir(X)

  #define getpid _getpid

// hardcode this has it is extremely unlikely to change ...
   #define PATH_MAX 260

#if defined(_WIN64)
    #define SSIZE_MAX _I64_MAX
#else
    #define SSIZE_MAX LONG_MAX
#endif

#else /* Not windows */

#include <strings.h>

#endif

#if defined(__GNUC__) || defined(__clang__)
#define CHECK_RESULT __attribute__ ((warn_unused_result))
#elif defined(_MSC_VER) && (_MSC_VER >= 1700)
#define CHECK_RESULT _Must_inspect_result_
#else
#define CHECK_RESULT
#endif


#ifdef _WIN32
#define GMS_CONFIG_FILE "gmscmpNT.txt"
#else
#define GMS_CONFIG_FILE "gmscmpun.txt"
#endif

#ifdef COMPAT_GLIBC
/* Gen by objdump -T /lib/x86_64-linux-gnu/libm.so.6  |
 * awk '$6 ~ /\(GLIBC/ && $2 ~ /g/ && $4 ~ /text/ {
 *   sub(/)/, "\")", $6);
 *   print "__asm__(\".symver " $7 "," $7 "@" substr($6,2)";"
 *   }'*/
__asm__(".symver memcpy,memcpy@GLIBC_2.2.5");
__asm__(".symver exp2f,exp2f@GLIBC_2.2.5");
__asm__(".symver pow10f,pow10f@GLIBC_2.2.5");
__asm__(".symver pow10l,pow10l@GLIBC_2.2.5");
__asm__(".symver pow10,pow10@GLIBC_2.2.5");
__asm__(".symver pow,pow@GLIBC_2.2.5");
__asm__(".symver log2f,log2f@GLIBC_2.2.5");
__asm__(".symver lgamma,lgamma@GLIBC_2.2.5");
__asm__(".symver powf,powf@GLIBC_2.2.5");
__asm__(".symver exp2,exp2@GLIBC_2.2.5");
__asm__(".symver lgammaf,lgammaf@GLIBC_2.2.5");
__asm__(".symver lgammal,lgammal@GLIBC_2.2.5");
__asm__(".symver expf,expf@GLIBC_2.2.5");
__asm__(".symver exp,exp@GLIBC_2.2.5");
__asm__(".symver log2,log2@GLIBC_2.2.5");
__asm__(".symver logf,logf@GLIBC_2.2.5");
__asm__(".symver log,log@GLIBC_2.2.5");
__asm__(".symver dlopen,dlopen@GLIBC_2.2.5");
__asm__(".symver dlsym,dlsym@GLIBC_2.2.5");
__asm__(".symver dlerror,dlerror@GLIBC_2.2.5");
__asm__(".symver dlclose,dlclose@GLIBC_2.2.5");
__asm__(".symver fmod,fmod@GLIBC_2.2.5");
__asm__(".symver __isoc23_strtol,strtol@GLIBC_2.2.5");
#endif

#ifndef __has_feature         // Optional of course.
  #define __has_feature(x) 0  // Compatibility with non-clang compilers.
#endif
#ifndef __has_extension
  #define __has_extension __has_feature // Compatibility with pre-3.0 compilers.
#endif

#if defined(__STDC_VERSION__) && __STDC_VERSION__ >= 202000L
#define RESHOP_STATIC_ASSERT(EXPR, STR) static_assert(EXPR, STR);
#elif defined(__GNUC__)
#if (defined(__clang__) && __has_extension(c_static_assert)) || \
    (__GNUC__ > 4 || (__GNUC__ == 4 &&  __GNUC_MINOR__ >= 6))
#define RESHOP_STATIC_ASSERT(EXPR, STR) _Static_assert(EXPR, STR);
#endif
#endif

#ifndef RESHOP_STATIC_ASSERT
#define RESHOP_STATIC_ASSERT(EXPR, STR)
#endif

/**
 * @brief Recursively remove a directory
 *
 * @param pathname the directory to remove
 *
 * @return the error code 
 */
int rmfn(const char *pathname);

#ifdef _WIN32
const char * mygetenv(const char *envvar);
void myfreeenvval(const char *envval);
#else
#include <stdlib.h>
#define mygetenv getenv
#define myfreeenvval(X) 
#endif

#ifdef __GNUC__

#define RHP_LIKELY(x) (__builtin_expect(!!(x),1))
#define RHP_UNLIKELY(x) (__builtin_expect(!!(x),0))

#define ALLOC_SIZE(IDX) __attribute__ ((alloc_size(IDX)))
#define NONNULL_ONEIDX(IDX) __attribute__ ((nonnull(IDX)))
#define NONNULL_IDX(E1, ...) __attribute__ ((nonnull(E1, __VA_ARGS__)))
#define NONNULL __attribute__ ((nonnull))
#define UNUSED __attribute__((unused))
#define __maybe_unused  __attribute__((unused))
#define FORMAT_CHK(S,A) __attribute__ ((format (printf, S, A)))
// TODO: DUPLICATE!

#ifndef NDEBUG
#define CTRMEM __attribute__ ((cleanup(ctr_memclean)))
#define RHP_HAS_CLEANUP
#else
 #define CTRMEM
#endif

#if defined(__STDC_VERSION__) && __STDC_VERSION__ >= 202000L
#define FALLTHRU [[fallthrough]];
#else
#define FALLTHRU __attribute__((fallthrough));
#endif

#if !defined(__clang__) && (__GNUC__ >= 11)
#define MALLOC_ATTR(...) __attribute__ ((malloc, malloc(__VA_ARGS__)))
#else
#define MALLOC_ATTR(...) __attribute__ ((malloc))
#endif

#define MALLOC_ATTR_SIMPLE __attribute__ ((malloc))

#if !defined(__clang__) && (__GNUC__ >= 10)
#   define ACCESS_ATTR(TYPE, ...) __attribute__ ((access (TYPE, __VA_ARGS__)))
#else
#   define ACCESS_ATTR(TYPE, ...)
#endif

#define WRITE_ONLY(...) ACCESS_ATTR(write_only, __VA_ARGS__)
#define READ_ONLY(...) ACCESS_ATTR(read_only, __VA_ARGS__)

#else /* NOT __GNUC__ */

#define RHP_LIKELY(x) (x)
#define RHP_UNLIKELY(x) (x)

#define ALLOC_SIZE(SIZE)
#define CTRMEM
#define NONNULL_ONEIDX(IDX) 
#define NONNULL_IDX(E1, ...)
#define NONNULL
#define UNUSED
#define __maybe_unused
#define FORMAT_CHK(S,A)
#define MALLOC_ATTR(...)
#define MALLOC_ATTR_SIMPLE
#define ACCESS_ATTR(TYPE, ...)
#define WRITE_ONLY(...)
#define READ_ONLY(...) 
#define FALLTHRU 

#endif

/* ---------------------------------------------------------------------
 * Help scan-build reason about the ownership of memory (unix.Malloc checker)
 * Very limited information available online.
 *
 * See https://interrupt.memfault.com/blog/arm-cortexm-with-llvm-clang#enabling-malloc-static-analysis-checkers
 * --------------------------------------------------------------------- */

#if defined(__clang__)

#define OWNERSHIP_TAKES(N) __attribute__ ((ownership_takes(malloc, N)))
#define OWNERSHIP_RETURNS __attribute__ ((ownership_returns(malloc)))

#else /* NOT clang */

#define OWNERSHIP_TAKES(N) 
#define OWNERSHIP_RETURNS 

#endif



#define rhp_idx int

#endif /* RESHOP_COMPAT_H  */
