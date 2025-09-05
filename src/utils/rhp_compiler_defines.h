#ifndef RHP_COMPILER_DEFINES
#define RHP_COMPILER_DEFINES

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

/* noreturn */
#if (defined(__STDC_VERSION__) && __STDC_VERSION__ >= 202300L) || \
    (defined(__cplusplus) && __cplusplus >= 201103L)

#  define NO_RETURN [[noreturn]]

#else

#  define NO_RETURN

#endif

/* **************************************************************************
 * Main definition of compiler-specific attributes
 * ************************************************************************** */

#if defined(__GNUC__) || defined(__clang__) || defined(__INTEL_LLVM_COMPILER)

#define RHP_LIKELY(x) (__builtin_expect(!!(x),1))
#define RHP_UNLIKELY(x) (__builtin_expect(!!(x),0))

/* This indicates that the returned pointer points to memory whose size is given
 * by the argument, or product of 2 arguments. Useful for __builtin_object_size */
#define ALLOC_SIZE(IDX) __attribute__ ((alloc_size(IDX)))

/* Arguments must be non-null pointers. Useful for scan-build as well */
#define NONNULL_AT(...) __attribute__ ((nonnull(__VA_ARGS__)))
#define NONNULL __attribute__ ((nonnull))


#define CHECK_RESULT __attribute__ ((warn_unused_result))

#if defined(__MINGW64__) && !defined(__clang__)
#define FORMAT_CHK(S,A) __attribute__ ((format (gnu_printf, S, A)))
#else
#define FORMAT_CHK(S,A) __attribute__ ((format (printf, S, A)))
#endif

#define UNUSED __attribute__((unused))

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

#elif defined(__clang__)
   #define MALLOC_ATTR(...) __attribute__ ((malloc))
   #define IGNORE_DEALLOC_MISMATCH(EXPR) EXPR

#else
   #define MALLOC_ATTR(...)
   #define IGNORE_DEALLOC_MISMATCH(EXPR)
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


#elif defined(_MSC_VER) && !defined(__INTEL_COMPILER) /* intel classic compiler is quite broken */

#if (_MSC_VER >= 1700) 

#  define CHECK_RESULT _Must_inspect_result_
#else
#  define CHECK_RESULT
#endif

#define RHP_LIKELY(x) (x)
#define RHP_UNLIKELY(x) (x)

#define ALLOC_SIZE(SIZE)
#define CTRMEM
#define NONNULL_AT(...)       /* __VA_ARGS__ */
#define NONNULL
#define UNUSED
#define FORMAT_CHK(S,A)       /* S, A */

#define MALLOC_ATTR(...)          /* __VA_ARGS__ */
#define IGNORE_DEALLOC_MISMATCH(EXPR) 

#define MALLOC_ATTR_SIMPLE
#define ACCESS_ATTR(TYPE, ...)  /* __VA_ARGS__ */
#define WRITE_ONLY(...)         /* __VA_ARGS__ */
#define READ_ONLY(...)          /* __VA_ARGS__ */
#define FALLTHRU 

#define __counted_by(member)

#else /* no real support here */

#define CHECK_RESULT

#define RHP_LIKELY(x) (x)
#define RHP_UNLIKELY(x) (x)

#define ALLOC_SIZE(SIZE)
#define CTRMEM
#define NONNULL_AT(...) /* __VA_ARGS__ */
#define NONNULL
#define UNUSED
#define FORMAT_CHK(S,A)
#define MALLOC_ATTR(...) /* __VA_ARGS__ */
#define IGNORE_DEALLOC_MISMATCH(EXPR)  /* EXPR */
#define MALLOC_ATTR_SIMPLE
#define ACCESS_ATTR(TYPE, ...) /* TYPE, __VA_ARGS__ */
#define WRITE_ONLY(...) /* __VA_ARGS__ */
#define READ_ONLY(...)  /* __VA_ARGS__ */
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
#if defined(__INTEL_COMPILER) || (defined(_MSC_VER) && !defined(__clang__)) || defined(__cplusplus)
#   define VMT(...)  /* __VA_ARGS__ */

#else

#   define VMT(...)  __VA_ARGS__

#endif

#if defined(_MSC_VER) && !defined(__INTEL_LLVM_COMPILER) && !defined(__clang__) && !defined(__GNUC__)
#define RHP_COMPILER_CL
#endif

#endif
