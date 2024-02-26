#ifndef RESHOP_SNAN_H
#define RESHOP_SNAN_H

#ifndef _MATH_H
#include "reshop_config.h"
#include <math.h>
#endif

/*  compatibility for issignaling: only with glibc for now */
#if !defined(issignaling) 
//|| defined(COMPAT_GLIBC)

#define issignaling(x) false
#else /*  !defined(issignaling)*/


#endif /* !defined(issignaling) */

const char *nansstr(const char *func, unsigned char code);

//#define MKSNAN(code) __builtin_nans(nansstr(__func__, code))

#if defined __has_builtin
#  if __has_builtin (__builtin_nans)
#     define SNAN_FROM_BUILTIN
#     define MKSNAN(code) (__builtin_nans(""))
#     define SNAN_UNINIT (__builtin_nans("01"))
#     define SNAN_UNDEF  (__builtin_nans("02"))
#     define SNAN_NA     (__builtin_nans("03"))
#     ifndef SNAN
#       define SNAN (__builtin_nans(""))
#     endif
#  endif
#endif

/*  TODO(xhub) investigate this */
#if !defined(SNAN_FROM_BUILTIN)
#include <stdint.h>
#define MKSNAN(code)  (0x7FF4000000000000ULL + (uint64_t)(code))
#define SNAN          0x7FF4000000000000ULL
#define SNAN_UNINIT   0x7FF4000000000001ULL
#define SNAN_UNDEF    0x7FF4000000000002ULL
#define SNAN_NA       0x7FF4000000000003ULL
#endif

#endif /* RESHOP_SNAN_H */
