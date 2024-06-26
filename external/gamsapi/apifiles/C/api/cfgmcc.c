/* C code 
 *
 * GAMS - Loading mechanism for GAMS Expert-Level APIs
 *
 * Copyright (c) 2016-2024 GAMS Software GmbH <support@gams.com>
 * Copyright (c) 2016-2024 GAMS Development Corp. <support@gams.com>
 *
 * Permission is hereby granted, free of charge, to any person obtaining a copy
 * of this software and associated documentation files (the "Software"), to deal
 * in the Software without restriction, including without limitation the rights
 * to use, copy, modify, merge, publish, distribute, sublicense, and/or sell
 * copies of the Software, and to permit persons to whom the Software is
 * furnished to do so, subject to the following conditions:
 *
 * The above copyright notice and this permission notice shall be included in all
 * copies or substantial portions of the Software.
 *
 * THE SOFTWARE IS PROVIDED "AS IS", WITHOUT WARRANTY OF ANY KIND, EXPRESS OR
 * IMPLIED, INCLUDING BUT NOT LIMITED TO THE WARRANTIES OF MERCHANTABILITY,
 * FITNESS FOR A PARTICULAR PURPOSE AND NONINFRINGEMENT. IN NO EVENT SHALL THE
 * AUTHORS OR COPYRIGHT HOLDERS BE LIABLE FOR ANY CLAIM, DAMAGES OR OTHER
 * LIABILITY, WHETHER IN AN ACTION OF CONTRACT, TORT OR OTHERWISE, ARISING FROM,
 * OUT OF OR IN CONNECTION WITH THE SOFTWARE OR THE USE OR OTHER DEALINGS IN THE
 * SOFTWARE.
 */



#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <assert.h>
#include <ctype.h>
#include <errno.h>

#define CFG_MAIN
#include "cfgmcc.h"

#if defined(_WIN32)
# include <windows.h>
  static char winErr[] = "Windows error";
  typedef HINSTANCE soHandle_t;
#else
# include <unistd.h>
# include <dlfcn.h>
# include <sys/utsname.h>
  typedef void *soHandle_t;
#endif

static soHandle_t h;
static int isLoaded = 0;
static int objectCount = 0;
static int ScreenIndicator = 1;
static int ExceptionIndicator = 0;
static int ExitIndicator = 1;
static cfgErrorCallback_t ErrorCallBack = NULL;
static int APIErrorCount = 0;

#if !defined(GC_NO_MUTEX)
#ifndef _GCMUTEX_
#define _GCMUTEX_

#include <stdlib.h>
#include <string.h>

#ifdef _WIN32
#include <windows.h>
#else
#include <pthread.h>
#endif

#ifdef _WIN32
typedef CRITICAL_SECTION GC_mutex_t;
#else
typedef pthread_mutex_t  GC_mutex_t;
#endif

#ifdef __GNUC__
__attribute__((unused))
#endif
static int GC_mutex_init(GC_mutex_t* mx)
{
#ifdef _WIN32
   InitializeCriticalSection(mx);
   return 0;
#else
   return pthread_mutex_init(mx, NULL);
#endif
}

#ifdef __GNUC__
__attribute__((unused))
#endif
static void GC_mutex_delete(GC_mutex_t* mx)
{
#ifdef _WIN32
   DeleteCriticalSection(mx);
#else
   (void) pthread_mutex_destroy(mx);
#endif
   memset(mx, 0, sizeof(*mx));
}

#ifdef __GNUC__
__attribute__((unused))
#endif
static int GC_mutex_lock(GC_mutex_t* mx)
{
#ifdef _WIN32
   EnterCriticalSection(mx);
   return 0;
#else
   return pthread_mutex_lock(mx);
#endif
}

#ifdef __GNUC__
__attribute__((unused))
#endif
static int GC_mutex_unlock(GC_mutex_t* mx)
{
#ifdef _WIN32
   LeaveCriticalSection(mx);
   return 0;
#else
   return pthread_mutex_unlock(mx);
#endif
}

#endif /* _GCMUTEX_ */
static GC_mutex_t libMutex;
static GC_mutex_t objMutex;
static GC_mutex_t exceptMutex;

static int MutexIsInitialized = 0;

void cfgInitMutexes(void)
{
  int rc;
  if (0==MutexIsInitialized) {
    rc = GC_mutex_init (&libMutex);     if(0!=rc) cfgErrorHandling("Problem initializing libMutex");
    rc = GC_mutex_init (&objMutex);     if(0!=rc) cfgErrorHandling("Problem initializing objMutex");
    rc = GC_mutex_init (&exceptMutex);  if(0!=rc) cfgErrorHandling("Problem initializing exceptMutex");
    MutexIsInitialized = 1;
  }
}

void cfgFiniMutexes(void)
{
  if (1==MutexIsInitialized) {
    GC_mutex_delete (&libMutex);
    GC_mutex_delete (&objMutex);
    GC_mutex_delete (&exceptMutex);
    MutexIsInitialized = 0;
  }
}
#  define lock(MUTEX)   if(MutexIsInitialized) GC_mutex_lock (&MUTEX);
#  define unlock(MUTEX) if(MutexIsInitialized) GC_mutex_unlock (&MUTEX);
#else
#  define lock(MUTEX)   ;
#  define unlock(MUTEX) ;
void cfgInitMutexes(void) {}
void cfgFiniMutexes(void) {}
#endif

#if !defined(GAMS_UNUSED)
#define GAMS_UNUSED(x) (void)x;
#endif

typedef void (CFG_CALLCONV *cfgXCreate_t) (cfgHandle_t *pcfg);
static CFG_FUNCPTR(cfgXCreate);
typedef void (CFG_CALLCONV *cfgXFree_t)   (cfgHandle_t *pcfg);
static CFG_FUNCPTR(cfgXFree);
typedef int (CFG_CALLCONV *cfgXAPIVersion_t) (int api, char *msg, int *cl);
static CFG_FUNCPTR(cfgXAPIVersion);
typedef int (CFG_CALLCONV *cfgXCheck_t) (const char *ep, int nargs, int s[], char *msg);
static CFG_FUNCPTR(cfgXCheck);
#define printNoReturn(f,nargs) { \
  char d_msgBuf[256]; \
  strcpy(d_msgBuf,#f " could not be loaded: "); \
  cfgXCheck(#f,nargs,d_s,d_msgBuf+strlen(d_msgBuf)); \
  cfgErrorHandling(d_msgBuf); \
}
#define printAndReturn(f,nargs,rtype) { \
  char d_msgBuf[256]; \
  strcpy(d_msgBuf,#f " could not be loaded: "); \
  cfgXCheck(#f,nargs,d_s,d_msgBuf+strlen(d_msgBuf)); \
  cfgErrorHandling(d_msgBuf); \
  return (rtype) 0; \
}


/** Read GAMS configuration file
 * @param pcfg cfg object handle
 * @param filename Configuration file name
 */
int  CFG_CALLCONV d_cfgReadConfig (cfgHandle_t pcfg, const char *filename)
{
  int d_s[]={3,11};
  GAMS_UNUSED(pcfg)
  GAMS_UNUSED(filename)
  printAndReturn(cfgReadConfig,1,int )
}

/** Read GAMS configuration file plus gamsconfig.yaml
 * @param pcfg cfg object handle
 * @param filename Configuration file name
 * @param sysDir GAMS System Directory
 */
int  CFG_CALLCONV d_cfgReadConfigGUC (cfgHandle_t pcfg, const char *filename, const char *sysDir)
{
  int d_s[]={3,11,11};
  GAMS_UNUSED(pcfg)
  GAMS_UNUSED(filename)
  GAMS_UNUSED(sysDir)
  printAndReturn(cfgReadConfigGUC,2,int )
}

/** Number of solvers
 * @param pcfg cfg object handle
 */
int  CFG_CALLCONV d_cfgNumAlgs (cfgHandle_t pcfg)
{
  int d_s[]={3};
  GAMS_UNUSED(pcfg)
  printAndReturn(cfgNumAlgs,0,int )
}

/** Number of default solver for model type proc
 * @param pcfg cfg object handle
 * @param proc Model type number
 */
int  CFG_CALLCONV d_cfgDefaultAlg (cfgHandle_t pcfg, int proc)
{
  int d_s[]={3,3};
  GAMS_UNUSED(pcfg)
  GAMS_UNUSED(proc)
  printAndReturn(cfgDefaultAlg,1,int )
}

/** Name of solver
 * @param pcfg cfg object handle
 * @param alg Solver number
 */
char * CFG_CALLCONV d_cfgAlgName (cfgHandle_t pcfg, int alg, char *buf)
{
  int d_s[]={12,3};
  GAMS_UNUSED(pcfg)
  GAMS_UNUSED(alg)
  GAMS_UNUSED(buf)
  printAndReturn(cfgAlgName,1,char *)
}

/** Code of solver
 * @param pcfg cfg object handle
 * @param alg Solver number
 */
char * CFG_CALLCONV d_cfgAlgCode (cfgHandle_t pcfg, int alg, char *buf)
{
  int d_s[]={12,3};
  GAMS_UNUSED(pcfg)
  GAMS_UNUSED(alg)
  GAMS_UNUSED(buf)
  printAndReturn(cfgAlgCode,1,char *)
}

/** Returns true, if alg should be hidden
 * @param pcfg cfg object handle
 * @param alg Solver number
 */
int  CFG_CALLCONV d_cfgAlgHidden (cfgHandle_t pcfg, int alg)
{
  int d_s[]={15,3};
  GAMS_UNUSED(pcfg)
  GAMS_UNUSED(alg)
  printAndReturn(cfgAlgHidden,1,int )
}

/** Solver can modify problem
 * @param pcfg cfg object handle
 * @param alg Solver number
 */
int  CFG_CALLCONV d_cfgAlgAllowsModifyProblem (cfgHandle_t pcfg, int alg)
{
  int d_s[]={15,3};
  GAMS_UNUSED(pcfg)
  GAMS_UNUSED(alg)
  printAndReturn(cfgAlgAllowsModifyProblem,1,int )
}

/** Get link library info for solver
 * @param pcfg cfg object handle
 * @param alg Solver number
 * @param name 
 * @param prefix 
 */
int  CFG_CALLCONV d_cfgAlgLibInfo (cfgHandle_t pcfg, int alg, char *name, char *prefix)
{
  int d_s[]={3,3,12,12};
  GAMS_UNUSED(pcfg)
  GAMS_UNUSED(alg)
  GAMS_UNUSED(name)
  GAMS_UNUSED(prefix)
  printAndReturn(cfgAlgLibInfo,3,int )
}

/** Get thread safety indicator for solver
 * @param pcfg cfg object handle
 * @param alg Solver number
 */
int  CFG_CALLCONV d_cfgAlgThreadSafeIndic (cfgHandle_t pcfg, int alg)
{
  int d_s[]={3,3};
  GAMS_UNUSED(pcfg)
  GAMS_UNUSED(alg)
  printAndReturn(cfgAlgThreadSafeIndic,1,int )
}

/** Number of solver
 * @param pcfg cfg object handle
 * @param id Solver name
 */
int  CFG_CALLCONV d_cfgAlgNumber (cfgHandle_t pcfg, const char *id)
{
  int d_s[]={3,11};
  GAMS_UNUSED(pcfg)
  GAMS_UNUSED(id)
  printAndReturn(cfgAlgNumber,1,int )
}

/** Solver Modeltype capability matrix
 * @param pcfg cfg object handle
 * @param alg Solver number
 * @param proc Model type number
 */
int  CFG_CALLCONV d_cfgAlgCapability (cfgHandle_t pcfg, int alg, int proc)
{
  int d_s[]={15,3,3};
  GAMS_UNUSED(pcfg)
  GAMS_UNUSED(alg)
  GAMS_UNUSED(proc)
  printAndReturn(cfgAlgCapability,2,int )
}

/** Create solver link object
 * @param pcfg cfg object handle
 * @param alg Solver number
 * @param psl 
 * @param sysDir GAMS System Directory
 * @param msg 
 */
int  CFG_CALLCONV d_cfgAlgCreate (cfgHandle_t pcfg, int alg, void **psl, const char *sysDir, char *msg)
{
  int d_s[]={15,3,2,11,12};
  GAMS_UNUSED(pcfg)
  GAMS_UNUSED(alg)
  GAMS_UNUSED(psl)
  GAMS_UNUSED(sysDir)
  GAMS_UNUSED(msg)
  printAndReturn(cfgAlgCreate,4,int )
}

/** Call solver readyapi
 * @param pcfg cfg object handle
 * @param alg Solver number
 * @param psl 
 * @param gmo 
 */
int  CFG_CALLCONV d_cfgAlgReadyAPI (cfgHandle_t pcfg, int alg, void *psl, void *gmo)
{
  int d_s[]={3,3,1,1};
  GAMS_UNUSED(pcfg)
  GAMS_UNUSED(alg)
  GAMS_UNUSED(psl)
  GAMS_UNUSED(gmo)
  printAndReturn(cfgAlgReadyAPI,3,int )
}

/** Call solver modifyproblem
 * @param pcfg cfg object handle
 * @param alg Solver number
 * @param psl 
 */
int  CFG_CALLCONV d_cfgAlgModifyProblem (cfgHandle_t pcfg, int alg, void *psl)
{
  int d_s[]={3,3,1};
  GAMS_UNUSED(pcfg)
  GAMS_UNUSED(alg)
  GAMS_UNUSED(psl)
  printAndReturn(cfgAlgModifyProblem,2,int )
}

/** Call solver modifyproblem
 * @param pcfg cfg object handle
 * @param alg Solver number
 * @param psl 
 * @param gmo 
 */
int  CFG_CALLCONV d_cfgAlgCallSolver (cfgHandle_t pcfg, int alg, void *psl, void *gmo)
{
  int d_s[]={3,3,1,1};
  GAMS_UNUSED(pcfg)
  GAMS_UNUSED(alg)
  GAMS_UNUSED(psl)
  GAMS_UNUSED(gmo)
  printAndReturn(cfgAlgCallSolver,3,int )
}

/** Call solver modifyproblem
 * @param pcfg cfg object handle
 * @param alg Solver number
 * @param vpsl 
 */
void  CFG_CALLCONV d_cfgAlgFree (cfgHandle_t pcfg, int alg, void **vpsl)
{
  int d_s[]={0,3,2};
  GAMS_UNUSED(pcfg)
  GAMS_UNUSED(alg)
  GAMS_UNUSED(vpsl)
  printNoReturn(cfgAlgFree,2)
}

/** Gives name of definition file for given solver (Returns true on success, false in case of problem)
 * @param pcfg cfg object handle
 * @param id Solver name
 * @param defFileName Name of definition file
 */
int  CFG_CALLCONV d_cfgDefFileName (cfgHandle_t pcfg, const char *id, char *defFileName)
{
  int d_s[]={15,11,12};
  GAMS_UNUSED(pcfg)
  GAMS_UNUSED(id)
  GAMS_UNUSED(defFileName)
  printAndReturn(cfgDefFileName,2,int )
}

/** Modeltype name
 * @param pcfg cfg object handle
 * @param proc Model type number
 */
char * CFG_CALLCONV d_cfgModelTypeName (cfgHandle_t pcfg, int proc, char *buf)
{
  int d_s[]={12,3};
  GAMS_UNUSED(pcfg)
  GAMS_UNUSED(proc)
  GAMS_UNUSED(buf)
  printAndReturn(cfgModelTypeName,1,char *)
}

/** Modeltype number
 * @param pcfg cfg object handle
 * @param id Solver name
 */
int  CFG_CALLCONV d_cfgModelTypeNumber (cfgHandle_t pcfg, const char *id)
{
  int d_s[]={3,11};
  GAMS_UNUSED(pcfg)
  GAMS_UNUSED(id)
  printAndReturn(cfgModelTypeNumber,1,int )
}

/** Number of pending messages
 * @param pcfg cfg object handle
 */
int  CFG_CALLCONV d_cfgNumMsg (cfgHandle_t pcfg)
{
  int d_s[]={3};
  GAMS_UNUSED(pcfg)
  printAndReturn(cfgNumMsg,0,int )
}

/** Pending messages
 * @param pcfg cfg object handle
 */
char * CFG_CALLCONV d_cfgGetMsg (cfgHandle_t pcfg, char *buf)
{
  int d_s[]={12};
  GAMS_UNUSED(pcfg)
  GAMS_UNUSED(buf)
  printAndReturn(cfgGetMsg,0,char *)
}


/** return dirName on success, NULL on failure */
static char *
extractFileDirFileName (const char *fileName, char *dirName, char *fName)
{
  int fileNameLen, shave=0;
  const char *end, *s;
  char *t;

  if (NULL == fileName || NULL == dirName || fName == NULL) {
    return NULL;
  }
  fileNameLen = (int) strlen(fileName);

#if defined(_WIN32)
  /* get the last delimiter */
  for (end = fileName + fileNameLen - 1;
       end >= fileName && '\\' != *end && ':' != *end;  end--);
  /* shave off the trailing delimiter if:
   *  it isn't the first char,
   *  it is a backslash, and
   *  it is not preceded by a delimiter
   */
  if (end > fileName && '\\' == *end
   && (! ('\\' == *(end-1) || ':' == *(end-1)))
     ) {
    end--; shave=1;
  }
#else
  /* non-Windows: implicitly, this is the Unix version */
  /* get the last delimiter */
  for (end = fileName + fileNameLen - 1;
       end >= fileName && '/' != *end;  end--);

  if (end > fileName && '/' == *end) {
    end--; shave=1;
  }
#endif  /* if defined(_WIN32) */

  for (s = fileName, t = dirName;  s <= end;  s++, t++)
    *t = *s;
  *t = '\0';

  if (shave) s++;
  for (t = fName;  s <= fileName + fileNameLen - 1;  s++, t++)
    *t = *s;
  *t = '\0';

  return dirName;
} /* extractFileDirFileName */

static soHandle_t
loadLib (const char *libName, char **errMsg)
{
  soHandle_t h;

#if defined(_WIN32)
#if defined(UNICODE) || defined (_UNICODE)
  h = LoadLibraryA(libName);
#else
  h = LoadLibrary(libName);
#endif
  if (NULL == h) {
    *errMsg = winErr;
  }
  else {
    *errMsg = NULL;
  }
#else
  (void) dlerror();
  h = dlopen (libName, RTLD_NOW);
  if (NULL == h) {
    *errMsg = dlerror();
  }
  else {
    *errMsg = NULL;
  }
#endif

  return h;
} /* loadLib */

static int
unLoadLib (soHandle_t hh)
{
  int rc;

#if defined(_WIN32)
  rc = FreeLibrary (hh);
  return ! rc;
#else
  rc = dlclose (hh);
#endif
  return rc;
} /* unLoadLib */

static void *
loadSym (soHandle_t h, const char *sym, char **errMsg)
{
  void *s;
  const char *from;
  char *to;
  const char *tripSym;
  char lcbuf[257];
  char ucbuf[257];
  size_t symLen;
  int trip;

  /* search in this order:
   *  1. lower
   *  2. original
   *  3. upper
   */

  symLen = 0;
  for (trip = 1;  trip <= 3;  trip++) {
    switch (trip) {
    case 1:                             /* lower */
      for (from = sym, to = lcbuf;  *from;  from++, to++) {
        *to = tolower(*from);
      }
      symLen = from - sym;
      lcbuf[symLen] = '\0';
      tripSym = lcbuf;
      break;
    case 2:                             /* original */
      tripSym = sym;
      break;
    case 3:                             /* upper */
      for (from = sym, to = ucbuf;  *from;  from++, to++) {
        *to = toupper(*from);
      }
      ucbuf[symLen] = '\0';
      tripSym = ucbuf;
      break;
    default:
      tripSym = sym;
    } /* end switch */
#if defined(_WIN32)
#  if defined(HAVE_INTPTR_T)
    s = (void *)(intptr_t)GetProcAddress (h, tripSym);
#  else
    s = (void *)GetProcAddress (h, tripSym);
#  endif
    *errMsg = NULL;
    if (NULL != s) {
      return s;
    }
#else
    (void) dlerror();
    s = dlsym (h, tripSym);
    *errMsg = dlerror();
    if (NULL == *errMsg) {
      return s;
    }
#endif
  } /* end loop over symbol name variations */

  return NULL;
} /* loadSym */

/* TNAME = type name, ENAME = exported name */
#if defined(HAVE_INTPTR_T)
#  define LOADIT(TNAME,ENAME) symName = ENAME; TNAME = (TNAME##_t) (intptr_t) loadSym (h, symName, &errMsg); if (NULL == TNAME) goto symMissing
#  define LOADIT_ERR_OK(TNAME,ENAME) symName = ENAME; TNAME = (TNAME##_t) (intptr_t) loadSym (h, symName, &errMsg)
#else
#  define LOADIT(TNAME,ENAME) symName = ENAME; TNAME = (TNAME##_t) loadSym (h, symName, &errMsg); if (NULL == TNAME) goto symMissing
#  define LOADIT_ERR_OK(TNAME,ENAME) symName = ENAME; TNAME = (TNAME##_t) loadSym (h, symName, &errMsg)
#endif

#if ! defined(GMS_DLL_BASENAME)
# define GMS_DLL_BASENAME "joatdclib"
#endif
#if defined(_WIN32)
# if ! defined(GMS_DLL_PREFIX)
#  define GMS_DLL_PREFIX ""
# endif
# if ! defined(GMS_DLL_EXTENSION)
#  define GMS_DLL_EXTENSION ".dll"
# endif
# if ! defined(GMS_DLL_SUFFIX)
#  if defined(_WIN64)
#   define GMS_DLL_SUFFIX "64"
#  else
#   define GMS_DLL_SUFFIX ""
#  endif
# endif

#else  /* start non-Windows */

# if ! defined(GMS_DLL_PREFIX)
#  define GMS_DLL_PREFIX "lib"
# endif
# if ! defined(GMS_DLL_EXTENSION)
#  if defined(__APPLE__)
#   define GMS_DLL_EXTENSION ".dylib"
#  else
#   define GMS_DLL_EXTENSION ".so"
#  endif
# endif
# if ! defined(GMS_DLL_SUFFIX)
#  if defined(__WORDSIZE)
#   if 64 == __WORDSIZE
#    define GMS_DLL_SUFFIX "64"
#   else
#    define GMS_DLL_SUFFIX ""
#   endif
#  elif defined(__SIZEOF_POINTER__)
#   if 4 == __SIZEOF_POINTER__
#    define GMS_DLL_SUFFIX ""
#   elif 8 == __SIZEOF_POINTER__
#    define GMS_DLL_SUFFIX "64"
#   endif
#  endif
# endif /* ! defined(GMS_DLL_SUFFIX) */
#endif

/** XLibraryLoad: return 0 on success, ~0 on failure */
static int
XLibraryLoad (const char *dllName, char *errBuf, int errBufSize)
{
  char *errMsg;
  const char *symName;
  int rc, cl;

  if (isLoaded)
    return 0;
  h = loadLib (dllName, &errMsg);
  if (NULL == h) {
    if (NULL != errBuf) {
      int elen;
      char* ebuf;
      elen = errBufSize-1;  ebuf = errBuf;
      rc = sprintf (ebuf, "%.*s", elen, "Could not load shared library ");
      elen -= rc;  ebuf+= rc;
      rc = sprintf (ebuf, "%.*s", elen, dllName);
      elen -= rc;  ebuf+= rc;
      rc = sprintf (ebuf, "%.*s", elen, ": ");
      elen -= rc;  ebuf+= rc;
      rc = sprintf (ebuf, "%.*s", elen, errMsg);
      /* elen -= rc;  ebuf+= rc; */
      errBuf[errBufSize-1] = '\0';
    }
    return 1;
  }
  else {
     /* printf ("Loaded shared library %s successfully\n", dllName); */
    if (errBuf && errBufSize)
      errBuf[0] = '\0';
  }

  LOADIT(cfgXCreate, "cfgXCreate");
  LOADIT(cfgXFree, "cfgXFree");
  LOADIT(cfgXCheck, "CcfgXCheck");
  LOADIT(cfgXAPIVersion, "CcfgXAPIVersion");

  if (!cfgXAPIVersion(4,errBuf,&cl))
    return 1;

#define CheckAndLoad(f,nargs,prefix) \
  if (!cfgXCheck(#f,nargs,s,errBuf)) \
    f = &d_##f; \
  else { \
    LOADIT(f,prefix #f); \
  }
  {int s[]={3,11}; CheckAndLoad(cfgReadConfig,1,"C"); }
  {int s[]={3,11,11}; CheckAndLoad(cfgReadConfigGUC,2,"C"); }
  {int s[]={3}; CheckAndLoad(cfgNumAlgs,0,""); }
  {int s[]={3,3}; CheckAndLoad(cfgDefaultAlg,1,""); }
  {int s[]={12,3}; CheckAndLoad(cfgAlgName,1,"C"); }
  {int s[]={12,3}; CheckAndLoad(cfgAlgCode,1,"C"); }
  {int s[]={15,3}; CheckAndLoad(cfgAlgHidden,1,""); }
  {int s[]={15,3}; CheckAndLoad(cfgAlgAllowsModifyProblem,1,""); }
  {int s[]={3,3,12,12}; CheckAndLoad(cfgAlgLibInfo,3,"C"); }
  {int s[]={3,3}; CheckAndLoad(cfgAlgThreadSafeIndic,1,""); }
  {int s[]={3,11}; CheckAndLoad(cfgAlgNumber,1,"C"); }
  {int s[]={15,3,3}; CheckAndLoad(cfgAlgCapability,2,""); }
  {int s[]={15,3,2,11,12}; CheckAndLoad(cfgAlgCreate,4,"C"); }
  {int s[]={3,3,1,1}; CheckAndLoad(cfgAlgReadyAPI,3,""); }
  {int s[]={3,3,1}; CheckAndLoad(cfgAlgModifyProblem,2,""); }
  {int s[]={3,3,1,1}; CheckAndLoad(cfgAlgCallSolver,3,""); }
  {int s[]={0,3,2}; CheckAndLoad(cfgAlgFree,2,""); }
  {int s[]={15,11,12}; CheckAndLoad(cfgDefFileName,2,"C"); }
  {int s[]={12,3}; CheckAndLoad(cfgModelTypeName,1,"C"); }
  {int s[]={3,11}; CheckAndLoad(cfgModelTypeNumber,1,"C"); }
  {int s[]={3}; CheckAndLoad(cfgNumMsg,0,""); }
  {int s[]={12}; CheckAndLoad(cfgGetMsg,0,"C"); }

 return 0;

 symMissing:
  if (errBuf && errBufSize>0) {
    int elen;
    char* ebuf;
    elen = errBufSize;  ebuf = errBuf;
    rc = sprintf (ebuf, "%.*s", elen, "Could not load symbol '");
    elen -= rc;  ebuf+= rc;
    rc = sprintf (ebuf, "%.*s", elen, symName);
    elen -= rc;  ebuf+= rc;
    rc = sprintf (ebuf, "%.*s", elen, "': ");
    elen -= rc;  ebuf+= rc;
    rc = sprintf (ebuf, "%.*s", elen, errMsg);
    /* elen -= rc;  ebuf+= rc; */
    errBuf[errBufSize-1] = '\0';
    /* printf ("%s\n", errBuf); */
    return 2;
  }

 return 0;

} /* XLibraryLoad */

static int
libloader(const char *dllPath, const char *dllName, char *msgBuf, int msgBufSize)
{

  char dllNameBuf[512];
  int myrc = 0;

#if ! defined(GMS_DLL_PREFIX)
# error "GMS_DLL_PREFIX expected but not defined"
#endif
#if ! defined(GMS_DLL_BASENAME)
# error "GMS_DLL_BASENAME expected but not defined"
#endif
#if ! defined(GMS_DLL_EXTENSION)
# error "GMS_DLL_EXTENSION expected but not defined"
#endif
#if ! defined(GMS_DLL_SUFFIX)
# error "GMS_DLL_SUFFIX expected but not defined"
#endif

  if (NULL != msgBuf) msgBuf[0] = '\0';

  if (! isLoaded) {
    if (NULL != dllPath && '\0' != *dllPath) {
      strncpy(dllNameBuf, dllPath, sizeof(dllNameBuf)-1);
      dllNameBuf[sizeof(dllNameBuf)-1] = '\0';
#if defined(_WIN32)
      if ('\\' != dllNameBuf[strlen(dllNameBuf)])
        strcat(dllNameBuf,"\\");
#else
      if ('/' != dllNameBuf[strlen(dllNameBuf)])
        strcat(dllNameBuf,"/");
#endif
    }
    else {
      dllNameBuf[0] = '\0';
    }
    if (NULL != dllName && '\0' != *dllName) {
      strncat(dllNameBuf, dllName, sizeof(dllNameBuf)-strlen(dllNameBuf)-1);
    }
    else {
      strncat(dllNameBuf, GMS_DLL_PREFIX GMS_DLL_BASENAME, sizeof(dllNameBuf)-strlen(dllNameBuf)-1);
      strncat(dllNameBuf, GMS_DLL_SUFFIX                 , sizeof(dllNameBuf)-strlen(dllNameBuf)-1);
      strncat(dllNameBuf, GMS_DLL_EXTENSION              , sizeof(dllNameBuf)-strlen(dllNameBuf)-1);
    }
    isLoaded = ! XLibraryLoad (dllNameBuf, msgBuf, msgBufSize);
    if (isLoaded) {
    }
    else {                              /* library load failed */
      myrc |= 1;
    }
  }
  return (myrc & 1) == 0;
}

int cfgGetReady (char *msgBuf, int msgBufSize)
{
  int rc;
  lock(libMutex);
  rc = libloader(NULL, NULL, msgBuf, msgBufSize);
  unlock(libMutex);
  return rc;
} /* cfgGetReady */

int cfgGetReadyD (const char *dirName, char *msgBuf, int msgBufSize)
{
  int rc;
  lock(libMutex);
  rc = libloader(dirName, NULL, msgBuf, msgBufSize);
  unlock(libMutex);
  return rc;
} /* cfgGetReadyD */

int cfgGetReadyL (const char *libName, char *msgBuf, int msgBufSize)
{
  char dirName[1024],fName[1024];
  int rc;
  extractFileDirFileName (libName, dirName, fName);
  lock(libMutex);
  rc = libloader(dirName, fName, msgBuf, msgBufSize);
  unlock(libMutex);
  return rc;
} /* cfgGetReadyL */

int cfgCreate (cfgHandle_t *pcfg, char *msgBuf, int msgBufSize)
{
  int cfgIsReady;

  cfgIsReady = cfgGetReady (msgBuf, msgBufSize);
  if (! cfgIsReady) {
    return 0;
  }
  assert(cfgXCreate);
  cfgXCreate(pcfg);
  if(pcfg == NULL)
  { strcpy(msgBuf,"Error while creating object"); return 0; }
  lock(objMutex);
  objectCount++;
  unlock(objMutex);
  return 1;                     /* return true on successful library load */
} /* cfgCreate */

int cfgCreateD (cfgHandle_t *pcfg, const char *dirName,
                char *msgBuf, int msgBufSize)
{
  int cfgIsReady;

  cfgIsReady = cfgGetReadyD (dirName, msgBuf, msgBufSize);
  if (! cfgIsReady) {
    return 0;
  }
  assert(cfgXCreate);
  cfgXCreate(pcfg);
  if(pcfg == NULL)
  { strcpy(msgBuf,"Error while creating object"); return 0; }
  lock(objMutex);
  objectCount++;
  unlock(objMutex);
  return 1;                     /* return true on successful library load */
} /* cfgCreateD */

int cfgCreateL (cfgHandle_t *pcfg, const char *libName,
                char *msgBuf, int msgBufSize)
{
  int cfgIsReady;

  cfgIsReady = cfgGetReadyL (libName, msgBuf, msgBufSize);
  if (! cfgIsReady) {
    return 0;
  }
  assert(cfgXCreate);
  cfgXCreate(pcfg);
  if(pcfg == NULL)
  { strcpy(msgBuf,"Error while creating object"); return 0; }
  lock(objMutex);
  objectCount++;
  unlock(objMutex);
  return 1;                     /* return true on successful library load */
} /* cfgCreateL */

int cfgFree   (cfgHandle_t *pcfg)
{
  assert(cfgXFree);
  cfgXFree(pcfg); pcfg = NULL;
  lock(objMutex);
  objectCount--;
  unlock(objMutex);
  return 1;
} /* cfgFree */

int cfgLibraryLoaded(void)
{
  int rc;
  lock(libMutex);
  rc = isLoaded;
  unlock(libMutex);
  return rc;
} /* cfgLibraryLoaded */

int cfgLibraryUnload(void)
{
  lock(objMutex);
  if (objectCount > 0)
  {
    unlock(objMutex);
    return 0;
  }
  unlock(objMutex);
  lock(libMutex);
  if (isLoaded)
  {
    isLoaded = 0;
    (void) unLoadLib(h);
  }
  unlock(libMutex);
  return 1;
} /* cfgLibraryUnload */

int  cfgCorrectLibraryVersion(char *msgBuf, int msgBufLen)
{
  int cl;
  char localBuf[256];

  if (msgBuf && msgBufLen) msgBuf[0] = '\0';

  if (! isLoaded) {
    strncpy(msgBuf, "Library needs to be initialized first", msgBufLen);
    return 0;
  }

  if (NULL == cfgXAPIVersion) {
    strncpy(msgBuf, "Function cfgXAPIVersion not found", msgBufLen);
    return 0;
  }

  cfgXAPIVersion(CFGAPIVERSION,localBuf,&cl);
  strncpy(msgBuf, localBuf, msgBufLen);

  if (1 == cl)
    return 1;
  else
    return 0;
}

int cfgGetScreenIndicator(void)
{
  return ScreenIndicator;
}

void cfgSetScreenIndicator(int scrind)
{
  ScreenIndicator = scrind ? 1 : 0;
}

int cfgGetExceptionIndicator(void)
{
   return ExceptionIndicator;
}

void cfgSetExceptionIndicator(int excind)
{
  ExceptionIndicator = excind ? 1 : 0;
}

int cfgGetExitIndicator(void)
{
  return ExitIndicator;
}

void cfgSetExitIndicator(int extind)
{
  ExitIndicator = extind ? 1 : 0;
}

cfgErrorCallback_t cfgGetErrorCallback(void)
{
  return ErrorCallBack;
}

void cfgSetErrorCallback(cfgErrorCallback_t func)
{
  lock(exceptMutex);
  ErrorCallBack = func;
  unlock(exceptMutex);
}

int cfgGetAPIErrorCount(void)
{
  return APIErrorCount;
}

void cfgSetAPIErrorCount(int ecnt)
{
  APIErrorCount = ecnt;
}

void cfgErrorHandling(const char *msg)
{
  APIErrorCount++;
  if (ScreenIndicator) { printf("%s\n", msg); fflush(stdout); }
  lock(exceptMutex);
  if (ErrorCallBack)
    if (ErrorCallBack(APIErrorCount, msg)) { unlock(exceptMutex); exit(123); }
  unlock(exceptMutex);
  assert(!ExceptionIndicator);
  if (ExitIndicator) exit(123);
}

