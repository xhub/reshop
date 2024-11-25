#ifndef RESHOP_COMPAT_H
#define RESHOP_COMPAT_H

#include "reshop_config.h"

/* Note: _WIN32 is depfined for all targets of interest
 *       _WIN64 is defined for amd64 and arm64
 */
#ifdef _WIN32
# define DIRSEP "\\"
#else
# define DIRSEP "/"
#endif

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

#ifdef _WIN32
#define GMS_CONFIG_FILE "gmscmpNT.txt"
#else
#define GMS_CONFIG_FILE "gmscmpun.txt"
#endif

#if defined(_WIN32)
  #include <string.h>
  #define strcasecmp _stricmp
  #define strncasecmp _strnicmp
  #define strdup _strdup

  #include <direct.h>
  #define mkdir(X,Y) _mkdir(X)

  #define getpid _getpid

// hardcode this as it is extremely unlikely to change ...
   #define PATH_MAX 260

#if defined(_WIN64)
    #define SSIZE_MAX _I64_MAX
#else
    #define SSIZE_MAX LONG_MAX
#endif

#else /* Not windows */

#include <strings.h>

#endif

/**
 * @brief Recursively remove a directory
 *
 * @param pathname the directory to remove
 *
 * @return the error code 
 */
int rmfn(const char *pathname);

/* ----------------------------------------------------------------------
 * To get environment variables
 * ---------------------------------------------------------------------- */
#ifdef _WIN32
const char * mygetenv(const char *envvar);
void myfreeenvval(const char *envval);
#else
#include <stdlib.h>
#define mygetenv getenv
#define myfreeenvval(X) 
#endif



#ifdef COMPAT_GLIBC
/* Gen by objdump -T /lib/x86_64-linux-gnu/libm.so.6  |
 * awk '$6 ~ /\(GLIBC/ && $2 ~ /g/ && $4 ~ /text/ {
 *   sub(/)/, "\")", $6);
 *   print "__asm__(\".symver " $7 "," $7 "@" substr($6,2)";"
 *   }'*/

/* libc 2.14 */
__asm__(".symver memcpy,memcpy@GLIBC_2.2.5");

/* libm 2.27 */
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

/* libc 2.34 */
__asm__(".symver dlopen,dlopen@GLIBC_2.2.5");
__asm__(".symver dlsym,dlsym@GLIBC_2.2.5");
__asm__(".symver dlerror,dlerror@GLIBC_2.2.5");
__asm__(".symver dlclose,dlclose@GLIBC_2.2.5");

/* libm 2.38 */
__asm__(".symver fmod,fmod@GLIBC_2.2.5");

/* libc 2.38 */
__asm__(".symver __isoc23_strtol,strtol@GLIBC_2.2.5");

#endif

#ifndef __has_feature         // Optional of course.
  #define __has_feature(x) 0  // Compatibility with non-clang compilers.
#endif
#ifndef __has_extension
  #define __has_extension __has_feature // Compatibility with pre-3.0 compilers.
#endif

#if (defined(__STDC_VERSION__) && __STDC_VERSION__ >= 202000L) || \
    (defined(__cplusplus) && __cplusplus >= 201103L)

#   define RESHOP_STATIC_ASSERT(EXPR, STR) static_assert(EXPR, STR);

#elif defined(__GNUC__)

#if (defined(__clang__) && __has_extension(c_static_assert)) || \
    (__GNUC__ > 4 || (__GNUC__ == 4 &&  __GNUC_MINOR__ >= 6))

#   define RESHOP_STATIC_ASSERT(EXPR, STR) _Static_assert(EXPR, STR);

#endif

#endif

#ifndef RESHOP_STATIC_ASSERT
#define RESHOP_STATIC_ASSERT(EXPR, STR) 
#endif


#if defined(__GNUC__) || defined(__clang__) || defined(__INTEL_LLVM_COMPILER)

#define RHP_LIKELY(x) (__builtin_expect(!!(x),1))
#define RHP_UNLIKELY(x) (__builtin_expect(!!(x),0))

#define ALLOC_SIZE(IDX) __attribute__ ((alloc_size(IDX)))
#define NONNULL_AT(...) __attribute__ ((nonnull(__VA_ARGS__)))
#define NONNULL __attribute__ ((nonnull))
#define CHECK_RESULT __attribute__ ((warn_unused_result))
#define FORMAT_CHK(S,A) __attribute__ ((format (printf, S, A)))
// TODO: DUPLICATE!
#define UNUSED __attribute__((unused))
#define __maybe_unused  __attribute__((unused))

#ifndef NDEBUG
#  define CTRMEM __attribute__ ((cleanup(ctr_memclean)))
#  define RHP_HAS_CLEANUP
#else
#  define CTRMEM
#endif

#if defined(__STDC_VERSION__) && __STDC_VERSION__ >= 202000L
#  define FALLTHRU [[fallthrough]];
#else
#  define FALLTHRU __attribute__((fallthrough));
#endif

#if !defined(__clang__) && (__GNUC__ >= 11)
   #define MALLOC_ATTR(...) __attribute__ ((malloc, malloc(__VA_ARGS__)))
   #define IGNORE_DEALLOC_MISMATCH(EXPR) \
       _Pragma("GCC diagnostic push"); _Pragma("GCC diagnostic ignored \"-Wmismatched-dealloc\""); \
       EXPR; \
       _Pragma("GCC diagnostic pop");

#else
   #define MALLOC_ATTR(...) __attribute__ ((malloc))
   #define IGNORE_DEALLOC_MISMATCH(EXPR) EXPR
#endif

#define MALLOC_ATTR_SIMPLE __attribute__ ((malloc))

#if !defined(__clang__) && (__GNUC__ >= 10)
#   define ACCESS_ATTR(TYPE, ...) __attribute__ ((access (TYPE, __VA_ARGS__)))
#else
#   define ACCESS_ATTR(TYPE, ...)
#endif

#define WRITE_ONLY(...) ACCESS_ATTR(write_only, __VA_ARGS__)
#define READ_ONLY(...) ACCESS_ATTR(read_only, __VA_ARGS__)


/* __counted_by is defined in stddef.h */
#ifdef __counted_by
#undef __counted_by
#endif

#if __has_attribute(__counted_by__)
# define __counted_by(member)		__attribute__((__counted_by__(member)))
#else
# define __counted_by(member)
#endif


#else /* NOT __GNUC__ */

#if defined(_MSC_VER) && (_MSC_VER >= 1700)
#  define CHECK_RESULT _Must_inspect_result_
#else
#  define CHECK_RESULT
#endif

#define RHP_LIKELY(x) (x)
#define RHP_UNLIKELY(x) (x)

#define ALLOC_SIZE(SIZE)
#define CTRMEM
#define NONNULL_AT(...)
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

#define __counted_by(member)

#endif


/* This is useful for variable only used in debug mode.
 * They are useful as they make the code cleaner, but with NDEBUG,
 * the compiler might complain about them
 */
#define DBGUSED UNUSED

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

/* ----------------------------------------------------------------------
 * For packing enums and such
 * ---------------------------------------------------------------------- */

#ifndef __GNUC__
#   define __extension__ /* */
#endif

#if (defined(__STDC_VERSION__) && __STDC_VERSION__ >= 202300L) || \
    (defined(__GNUC__) && (__GNUC__ >= 13)) || \
    (defined(__clang__) && (__clang_major__ >= 17))

#   include <stdint.h>

#   define ENUM_U8  : uint8_t
#   define ENUM_U32 : uint32_t

#else

#   define ENUM_U8   
#   define ENUM_U32  

#endif

/* ----------------------------------------------------------------------
 * Variably modified types are very useful, but older compilers are lost here
 * ---------------------------------------------------------------------- */

// Note: Newer LLVM-based intel compiler define __INTEL_LLVM_COMPILER
#if defined(__INTEL_COMPILER) || (defined(_MSC_VER) && !defined(__clang__))
#   define VMT(...)  /* __VA_ARGS__ */

#else

#   define VMT(...)  __VA_ARGS__

#endif

#define rhp_idx int

#endif /* RESHOP_COMPAT_H */
