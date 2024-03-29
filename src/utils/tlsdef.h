#ifndef TLSDEF_H
#define TLSDEF_H

/** @file tlsdef.h
 *
 *  @brief definition of thread local variable
 */

#if !defined(__GNUC__)
#   if !defined(_WIN32)
#      error "Need constructor/destructor support, like .(init|fini)_array (ELF) or __mod_(init|term)_func (Mach-O)"
#   endif

#   define DESTRUCTOR_ATTR 
#   define CONSTRUCTOR_ATTR
#   define DESTRUCTOR_ATTR_PRIO(N)
#   define CONSTRUCTOR_ATTR_PRIO(N)

#elif defined(__APPLE__) && !defined(__clang__)

/* TODO: delete this as soon as GCC is no longer required on Darwin */
#   define NO_CONSTRUCTOR_PRIO

#   define DESTRUCTOR_ATTR __attribute__ ((destructor))
#   define CONSTRUCTOR_ATTR __attribute__ ((constructor))
#   define DESTRUCTOR_ATTR_PRIO(N) __attribute__ ((destructor))
#   define CONSTRUCTOR_ATTR_PRIO(N) __attribute__ ((constructor))

#else

#   define DESTRUCTOR_ATTR __attribute__ ((destructor))
#   define CONSTRUCTOR_ATTR __attribute__ ((constructor))
#   define DESTRUCTOR_ATTR_PRIO(N) __attribute__ ((destructor(N)))
#   define CONSTRUCTOR_ATTR_PRIO(N) __attribute__ ((constructor(N)))
#endif

/* crashes now. Investigate __declspec(thread) and LoadLibrary  */
#if defined(_WIN32) && !defined(__INTEL_LLVM_COMPILER)
#define tlsvar
#endif

#ifndef tlsvar

#if !defined(__cplusplus) && !defined(BUILD_AS_CPP)

#   ifdef SWIG

#      define tlsvar

    // GCC on Darwin does not support threads.h but does not define __STDC_NO_THREADS__
#   elif __STDC_VERSION__ >= 201112L && !defined(_WIN32) && !(defined __APPLE__) && (!defined(__STDC_NO_THREADS__) || __STDC_NO_THREADS__ == 0)
#      include <threads.h>
#      define tlsvar thread_local

#   elif defined(__GNUC__) || (defined(__ICC) && defined(__linux)) || defined(__INTEL_LLVM_COMPILER)
#      define tlsvar __thread

#   elif defined(_WIN32) && (defined(__ICC) || defined(__clang__))
#      define tlsvar __declspec(thread)

#   elif defined(__APPLE__)  // Apple seems to support this
#      define tlsvar thread_local

#   elif defined(RESHOP_ALLOW_GLOBAL)
#      define tlsvar

#   else
#      error "Don't know how to create a thread-local variable"
#   endif

#else /* Compile as C++ */

#   ifdef SWIG
#      define tlsvar

#   elif __cplusplus >= 201103L
#      define tlsvar thread_local
#   elif defined(__GNUC__) || (defined(__ICC) && defined(__linux))
#      define tlsvar __thread
#   elif defined(_MSC_VER) || (defined(__ICC) && defined(_WIN32))
#      define tlsvar __declspec(thread)
#   elif defined(RESHOP_ALLOW_GLOBAL)
#      define tlsvar
#   else
#      error "Don't know how to create a thread-local variable"
#   endif

#endif

#endif /*  ifndef(tlsvar) */

#endif

