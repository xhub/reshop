#ifndef RESHOP_COMPAT_H
#define RESHOP_COMPAT_H

#include "reshop_config.h"
#include "rhp_compiler_defines.h"


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


#define rhp_idx int

#endif /* RESHOP_COMPAT_H */
