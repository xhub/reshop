/* Copyright (C) GAMS Development and others
 * All Rights Reserved.
 * This code is published under the Eclipse Public License.
 *
 * Author: Stefan Vigerske
 */

#ifndef __GAMSLINKSCONFIG_H__
#define __GAMSLINKSCONFIG_H__

/* Define to 1 if the Cbc package is available */
#define COIN_HAS_CBC 1
#define GAMSLINKS_HAS_CBC 1

/* Define to 1 if the Ipopt package is available */
#define COIN_HAS_IPOPT 1
#define GAMSLINKS_HAS_IPOPT 1

/* Define to 1 if the Osi package is available */
#define COIN_HAS_OSI 1
#define GAMSLINKS_HAS_OSI 1

/* Define to 1 if the OsiCpx package is available */
/* #define COIN_HAS_OSICPX 1 */
/* #define GAMSLINKS_HAS_OSICPX 1 */

/* Define to 1 if the OsiGlpk package is available */
/* #undef COIN_HAS_OSIGLPK */

/* Define to 1 if the OsiGrb package is available */
/* #define COIN_HAS_OSIGRB 1 */
/* #define GAMSLINKS_HAS_OSIGRB 1 */

/* Define to 1 if the OsiMsk package is available */
/* #define COIN_HAS_OSIMSK 1 */
/* #define GAMSLINKS_HAS_OSIMSK 1 */

/* Define to 1 if the OsiSpx package is available */
/* #undef COIN_HAS_OSISPX */

/* Define to 1 if the OsiXpr package is available */
/* #define COIN_HAS_OSIXPR 1 */
/* #define GAMSLINKS_HAS_OSIXPR 1 */

/* Define to 1 if the SCIP package is available */
#define COIN_HAS_SCIP 1
#define GAMSLINKS_HAS_SCIP 1

#define COIN_HAS_CPLEX 1
#define COIN_HAS_GUROBI 1
#define COIN_HAS_MOSEK 1
#define COIN_HAS_XPRESS 1
#define GAMSLINKS_HAS_CPLEX 1
#define GAMSLINKS_HAS_GUROBI 1
#define GAMSLINKS_HAS_MOSEK 1
#define GAMSLINKS_HAS_XPRESS 1

/* Define to a macro mangling the given C identifier (in lower and upper
   case), which must not contain underscores, for linking with Fortran. */
#if defined(FNAME_LCASE_DECOR)
#define F77_FUNC(name,NAME) name ## _
#define F77_FUNC_(name,NAME) name ## _
#elif defined(FNAME_UCASE_NODECOR)
#define F77_FUNC(name,NAME) NAME
#define F77_FUNC_(name,NAME) NAME
#endif

/* Define to 1 if you have the <dlfcn.h> header file. */
#ifndef _MSC_VER
#define HAVE_DLFCN_H 1
#endif

/* Define to 1 if we have goto_set_num_threads */
/* #undef HAVE_GOTO_SETNUMTHREADS */

/* Define to 1 if you have the <inttypes.h> header file. */
#define HAVE_INTTYPES_H 1

/* Define to 1 if you have the <memory.h> header file. */
#define HAVE_MEMORY_H 1

/* Define to 1 if you have the `snprintf' function. */
#ifndef _MSC_VER
#define HAVE_SNPRINTF 1
#endif

/* Define to 1 if you have the <stdint.h> header file. */
#define HAVE_STDINT_H 1

/* Define to 1 if you have the <stdlib.h> header file. */
#define HAVE_STDLIB_H 1

/* Define to 1 if you have the <strings.h> header file. */
#ifndef _MSC_VER
#define HAVE_STRINGS_H 1
#endif

/* Define to 1 if you have the <string.h> header file. */
#define HAVE_STRING_H 1

/* Define to 1 if you have the `strtok' function. */
/* #undef HAVE_STRTOK */

/* Define to 1 if you have the `strtok_r' function. */
#ifndef _MSC_VER
#define HAVE_STRTOK_R 1
#endif

/* Define to 1 if you have the `strtok_s' function. */
#ifdef _MSC_VER
#define HAVE_STRTOK_S
#endif

/* Define to 1 if you have the <sys/stat.h> header file. */
#define HAVE_SYS_STAT_H 1

/* Define to 1 if you have the <sys/types.h> header file. */
#define HAVE_SYS_TYPES_H 1

/* Define to 1 if you have the <unistd.h> header file. */
#ifndef _MSC_VER
#define HAVE_UNISTD_H 1
#endif

/* Define to 1 if va_copy is available */
#define HAVE_VA_COPY 1

/* Define to 1 if you have the `vsnprintf' function. */
#ifndef _MSC_VER
#define HAVE_VSNPRINTF 1
#endif

/* Define to 1 if you have the `_snprintf' function. */
/* #undef HAVE__SNPRINTF */

/* Define to 1 if you have the `_vsnprintf' function. */
/* #undef HAVE__VSNPRINTF */

/* Define to 1 if you have the ANSI C header files. */
#define STDC_HEADERS 1



#ifndef HAVE_SNPRINTF
#ifdef HAVE__SNPRINTF
#define snprintf _snprintf
#else
/* some snprintf seems to be availabe on Windows, though configure couldn't detect it */
/*#error "Do not have snprintf of _snprintf." */
#endif
#endif

#ifndef HAVE_STRTOK_R
#ifdef HAVE_STRTOK_S
#define strtok_r strtok_s
#else
#ifdef HAVE_STRTOK
#define strtok_r(a,b,c) strtok(a,b)
#else
#error "Do not have strtok_r, strtok_s, or strtok."
#endif
#endif
#endif

#if defined(_WIN32)
# if ! defined(STDCALL)
#  define STDCALL   __stdcall
# endif
# if ! defined(DllExport)
#  define DllExport __declspec( dllexport )
# endif
#elif defined(__GNUC__)
# if ! defined(STDCALL)
#  define STDCALL
# endif
# if ! defined(DllExport)
#  define DllExport __attribute__((__visibility__("default")))
# endif
#else
# if ! defined(STDCALL)
#  define STDCALL
# endif
# if ! defined(DllExport)
#  define DllExport
# endif
#endif

#endif /*__GAMSLINKSCONFIG_H__*/
