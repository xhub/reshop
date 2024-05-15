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

#define GEV_MAIN
#include "gevmcc.h"

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
static gevErrorCallback_t ErrorCallBack = NULL;
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

void gevInitMutexes(void)
{
  int rc;
  if (0==MutexIsInitialized) {
    rc = GC_mutex_init (&libMutex);     if(0!=rc) gevErrorHandling("Problem initializing libMutex");
    rc = GC_mutex_init (&objMutex);     if(0!=rc) gevErrorHandling("Problem initializing objMutex");
    rc = GC_mutex_init (&exceptMutex);  if(0!=rc) gevErrorHandling("Problem initializing exceptMutex");
    MutexIsInitialized = 1;
  }
}

void gevFiniMutexes(void)
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
void gevInitMutexes(void) {}
void gevFiniMutexes(void) {}
#endif

#if !defined(GAMS_UNUSED)
#define GAMS_UNUSED(x) (void)x;
#endif

typedef void (GEV_CALLCONV *gevXCreate_t) (gevHandle_t *pgev);
static GEV_FUNCPTR(gevXCreate);

typedef void (GEV_CALLCONV *gevXCreateD_t) (gevHandle_t *pgev, const char *dirName);
static GEV_FUNCPTR(gevXCreateD);
typedef void (GEV_CALLCONV *gevXFree_t)   (gevHandle_t *pgev);
static GEV_FUNCPTR(gevXFree);
typedef int (GEV_CALLCONV *gevXAPIVersion_t) (int api, char *msg, int *cl);
static GEV_FUNCPTR(gevXAPIVersion);
typedef int (GEV_CALLCONV *gevXCheck_t) (const char *ep, int nargs, int s[], char *msg);
static GEV_FUNCPTR(gevXCheck);
#define printNoReturn(f,nargs) { \
  char d_msgBuf[256]; \
  strcpy(d_msgBuf,#f " could not be loaded: "); \
  gevXCheck(#f,nargs,d_s,d_msgBuf+strlen(d_msgBuf)); \
  gevErrorHandling(d_msgBuf); \
}
#define printAndReturn(f,nargs,rtype) { \
  char d_msgBuf[256]; \
  strcpy(d_msgBuf,#f " could not be loaded: "); \
  gevXCheck(#f,nargs,d_s,d_msgBuf+strlen(d_msgBuf)); \
  gevErrorHandling(d_msgBuf); \
  return (rtype) 0; \
}


/** Register callback for log and status streams
 * @param pgev gev object handle
 * @param lsw Pointer to callback for log and status streams
 * @param logenabled Flag to enable log or not
 * @param usrmem User memory
 */
void  GEV_CALLCONV d_gevRegisterWriteCallback (gevHandle_t pgev, Tgevlswrite_t lsw, int logenabled, void *usrmem)
{
  int d_s[]={0,59,15,1};
  GAMS_UNUSED(pgev)
  GAMS_UNUSED(lsw)
  GAMS_UNUSED(logenabled)
  GAMS_UNUSED(usrmem)
  printNoReturn(gevRegisterWriteCallback,3)
}

/** Complete initialization of environment
 * @param pgev gev object handle
 * @param palg Pointer to ALGX structure (GAMS Internal)
 * @param ivec Array of integer options
 * @param rvec Array of real/double options
 * @param svec Array of string options
 */
void  GEV_CALLCONV d_gevCompleteEnvironment (gevHandle_t pgev, void *palg, void *ivec, void *rvec, void *svec)
{
  int d_s[]={0,1,1,1,1};
  GAMS_UNUSED(pgev)
  GAMS_UNUSED(palg)
  GAMS_UNUSED(ivec)
  GAMS_UNUSED(rvec)
  GAMS_UNUSED(svec)
  printNoReturn(gevCompleteEnvironment,4)
}

/** Initialization in legacy mode (from control file)
 * @param pgev gev object handle
 * @param cntrfn Name of control file
 */
int  GEV_CALLCONV d_gevInitEnvironmentLegacy (gevHandle_t pgev, const char *cntrfn)
{
  int d_s[]={3,11};
  GAMS_UNUSED(pgev)
  GAMS_UNUSED(cntrfn)
  printAndReturn(gevInitEnvironmentLegacy,1,int )
}

/** Switch log and status streams to another file or callback
 * @param pgev gev object handle
 * @param lo logoption (0..3)
 * @param logfn Log file name
 * @param logappend Flag whether to append to log stream or not
 * @param statfn Status file name
 * @param statappend Flag whether to append to status stream or not
 * @param lsw Pointer to callback for log and status streams
 * @param usrmem User memory
 * @param lshandle Log and status handle for later restoring
 */
int  GEV_CALLCONV d_gevSwitchLogStat (gevHandle_t pgev, int lo, const char *logfn, int logappend, const char *statfn, int statappend, Tgevlswrite_t lsw, void *usrmem, void **lshandle)
{
  int d_s[]={15,3,11,15,11,15,59,1,2};
  GAMS_UNUSED(pgev)
  GAMS_UNUSED(lo)
  GAMS_UNUSED(logfn)
  GAMS_UNUSED(logappend)
  GAMS_UNUSED(statfn)
  GAMS_UNUSED(statappend)
  GAMS_UNUSED(lsw)
  GAMS_UNUSED(usrmem)
  GAMS_UNUSED(lshandle)
  printAndReturn(gevSwitchLogStat,8,int )
}

/** Switch log and status streams to another file or callback
 * @param pgev gev object handle
 * @param lo logoption (0..3)
 * @param logfn Log file name
 * @param logappend Flag whether to append to log stream or not
 * @param statfn Status file name
 * @param statappend Flag whether to append to status stream or not
 * @param lsw Pointer to callback for log and status streams
 * @param usrmem User memory
 * @param lshandle Log and status handle for later restoring
 * @param doStack Select stacking mode, where a new callback is stacked over the current settings instead of undoing them
 */
int  GEV_CALLCONV d_gevSwitchLogStatEx (gevHandle_t pgev, int lo, const char *logfn, int logappend, const char *statfn, int statappend, Tgevlswrite_t lsw, void *usrmem, void **lshandle, int doStack)
{
  int d_s[]={15,3,11,15,11,15,59,1,2,15};
  GAMS_UNUSED(pgev)
  GAMS_UNUSED(lo)
  GAMS_UNUSED(logfn)
  GAMS_UNUSED(logappend)
  GAMS_UNUSED(statfn)
  GAMS_UNUSED(statappend)
  GAMS_UNUSED(lsw)
  GAMS_UNUSED(usrmem)
  GAMS_UNUSED(lshandle)
  GAMS_UNUSED(doStack)
  printAndReturn(gevSwitchLogStatEx,9,int )
}

/** Returns handle to last log and status stream stored by gevSwitchLogStat (Workaround for problem with vptr in Python)
 * @param pgev gev object handle
 */
void * GEV_CALLCONV d_gevGetLShandle (gevHandle_t pgev)
{
  int d_s[]={1};
  GAMS_UNUSED(pgev)
  printAndReturn(gevGetLShandle,0,void *)
}

/** Restore log status stream settings
 * @param pgev gev object handle
 * @param lshandle Log and status handle for later restoring
 */
int  GEV_CALLCONV d_gevRestoreLogStat (gevHandle_t pgev, void **lshandle)
{
  int d_s[]={15,2};
  GAMS_UNUSED(pgev)
  GAMS_UNUSED(lshandle)
  printAndReturn(gevRestoreLogStat,1,int )
}

/** Restore log status stream settings but never append to former log
 * @param pgev gev object handle
 * @param lshandle Log and status handle for later restoring
 */
int  GEV_CALLCONV d_gevRestoreLogStatRewrite (gevHandle_t pgev, void **lshandle)
{
  int d_s[]={15,2};
  GAMS_UNUSED(pgev)
  GAMS_UNUSED(lshandle)
  printAndReturn(gevRestoreLogStatRewrite,1,int )
}

/** Send string to log stream
 * @param pgev gev object handle
 * @param s String
 */
void  GEV_CALLCONV d_gevLog (gevHandle_t pgev, const char *s)
{
  int d_s[]={0,11};
  GAMS_UNUSED(pgev)
  GAMS_UNUSED(s)
  printNoReturn(gevLog,1)
}

/** Send PChar to log stream, no newline added
 * @param pgev gev object handle
 * @param p Pointer to array of characters
 */
void  GEV_CALLCONV d_gevLogPChar (gevHandle_t pgev, const char *p)
{
  int d_s[]={0,9};
  GAMS_UNUSED(pgev)
  GAMS_UNUSED(p)
  printNoReturn(gevLogPChar,1)
}

/** Send string to status stream
 * @param pgev gev object handle
 * @param s String
 */
void  GEV_CALLCONV d_gevStat (gevHandle_t pgev, const char *s)
{
  int d_s[]={0,11};
  GAMS_UNUSED(pgev)
  GAMS_UNUSED(s)
  printNoReturn(gevStat,1)
}

/** Send string to status and copy to listing file
 * @param pgev gev object handle
 * @param s String
 */
void  GEV_CALLCONV d_gevStatC (gevHandle_t pgev, const char *s)
{
  int d_s[]={0,11};
  GAMS_UNUSED(pgev)
  GAMS_UNUSED(s)
  printNoReturn(gevStatC,1)
}

/** Send PChar to status stream, no newline added
 * @param pgev gev object handle
 * @param p Pointer to array of characters
 */
void  GEV_CALLCONV d_gevStatPChar (gevHandle_t pgev, const char *p)
{
  int d_s[]={0,9};
  GAMS_UNUSED(pgev)
  GAMS_UNUSED(p)
  printNoReturn(gevStatPChar,1)
}

/** GAMS internal status stream operation {=0}
 * @param pgev gev object handle
 * @param s String
 */
void  GEV_CALLCONV d_gevStatAudit (gevHandle_t pgev, const char *s)
{
  int d_s[]={0,11};
  GAMS_UNUSED(pgev)
  GAMS_UNUSED(s)
  printNoReturn(gevStatAudit,1)
}

/** GAMS internal status stream operation {=1}
 * @param pgev gev object handle
 */
void  GEV_CALLCONV d_gevStatCon (gevHandle_t pgev)
{
  int d_s[]={0};
  GAMS_UNUSED(pgev)
  printNoReturn(gevStatCon,0)
}

/** GAMS internal status stream operation {=2}
 * @param pgev gev object handle
 */
void  GEV_CALLCONV d_gevStatCoff (gevHandle_t pgev)
{
  int d_s[]={0};
  GAMS_UNUSED(pgev)
  printNoReturn(gevStatCoff,0)
}

/** GAMS internal status stream operation {=3}
 * @param pgev gev object handle
 */
void  GEV_CALLCONV d_gevStatEOF (gevHandle_t pgev)
{
  int d_s[]={0};
  GAMS_UNUSED(pgev)
  printNoReturn(gevStatEOF,0)
}

/** GAMS internal status stream operation {=4}
 * @param pgev gev object handle
 */
void  GEV_CALLCONV d_gevStatSysout (gevHandle_t pgev)
{
  int d_s[]={0};
  GAMS_UNUSED(pgev)
  printNoReturn(gevStatSysout,0)
}

/** GAMS internal status stream operation {=5}
 * @param pgev gev object handle
 * @param mi Index or constraint
 * @param s String
 */
void  GEV_CALLCONV d_gevStatAddE (gevHandle_t pgev, int mi, const char *s)
{
  int d_s[]={0,3,11};
  GAMS_UNUSED(pgev)
  GAMS_UNUSED(mi)
  GAMS_UNUSED(s)
  printNoReturn(gevStatAddE,2)
}

/** GAMS internal status stream operation {=6}
 * @param pgev gev object handle
 * @param mj Index or variable
 * @param s String
 */
void  GEV_CALLCONV d_gevStatAddV (gevHandle_t pgev, int mj, const char *s)
{
  int d_s[]={0,3,11};
  GAMS_UNUSED(pgev)
  GAMS_UNUSED(mj)
  GAMS_UNUSED(s)
  printNoReturn(gevStatAddV,2)
}

/** GAMS internal status stream operation {=7}
 * @param pgev gev object handle
 * @param mi Index or constraint
 * @param mj Index or variable
 * @param s String
 */
void  GEV_CALLCONV d_gevStatAddJ (gevHandle_t pgev, int mi, int mj, const char *s)
{
  int d_s[]={0,3,3,11};
  GAMS_UNUSED(pgev)
  GAMS_UNUSED(mi)
  GAMS_UNUSED(mj)
  GAMS_UNUSED(s)
  printNoReturn(gevStatAddJ,3)
}

/** GAMS internal status stream operation {=8}
 * @param pgev gev object handle
 */
void  GEV_CALLCONV d_gevStatEject (gevHandle_t pgev)
{
  int d_s[]={0};
  GAMS_UNUSED(pgev)
  printNoReturn(gevStatEject,0)
}

/** GAMS internal status stream operation {=9}
 * @param pgev gev object handle
 * @param c Character
 */
void  GEV_CALLCONV d_gevStatEdit (gevHandle_t pgev, const char c)
{
  int d_s[]={0,18};
  GAMS_UNUSED(pgev)
  GAMS_UNUSED(c)
  printNoReturn(gevStatEdit,1)
}

/** GAMS internal status stream operation {=E}
 * @param pgev gev object handle
 * @param s String
 * @param mi Index or constraint
 * @param s2 String
 */
void  GEV_CALLCONV d_gevStatE (gevHandle_t pgev, const char *s, int mi, const char *s2)
{
  int d_s[]={0,11,3,11};
  GAMS_UNUSED(pgev)
  GAMS_UNUSED(s)
  GAMS_UNUSED(mi)
  GAMS_UNUSED(s2)
  printNoReturn(gevStatE,3)
}

/** GAMS internal status stream operation {=V}
 * @param pgev gev object handle
 * @param s String
 * @param mj Index or variable
 * @param s2 String
 */
void  GEV_CALLCONV d_gevStatV (gevHandle_t pgev, const char *s, int mj, const char *s2)
{
  int d_s[]={0,11,3,11};
  GAMS_UNUSED(pgev)
  GAMS_UNUSED(s)
  GAMS_UNUSED(mj)
  GAMS_UNUSED(s2)
  printNoReturn(gevStatV,3)
}

/** GAMS internal status stream operation {=T}
 * @param pgev gev object handle
 */
void  GEV_CALLCONV d_gevStatT (gevHandle_t pgev)
{
  int d_s[]={0};
  GAMS_UNUSED(pgev)
  printNoReturn(gevStatT,0)
}

/** GAMS internal status stream operation {=A}
 * @param pgev gev object handle
 * @param s String
 */
void  GEV_CALLCONV d_gevStatA (gevHandle_t pgev, const char *s)
{
  int d_s[]={0,11};
  GAMS_UNUSED(pgev)
  GAMS_UNUSED(s)
  printNoReturn(gevStatA,1)
}

/** GAMS internal status stream operation {=B}
 * @param pgev gev object handle
 * @param s String
 */
void  GEV_CALLCONV d_gevStatB (gevHandle_t pgev, const char *s)
{
  int d_s[]={0,11};
  GAMS_UNUSED(pgev)
  GAMS_UNUSED(s)
  printNoReturn(gevStatB,1)
}

/** Send string to log and status streams and copy to listing file
 * @param pgev gev object handle
 * @param s String
 */
void  GEV_CALLCONV d_gevLogStat (gevHandle_t pgev, const char *s)
{
  int d_s[]={0,11};
  GAMS_UNUSED(pgev)
  GAMS_UNUSED(s)
  printNoReturn(gevLogStat,1)
}

/** Send string to log and status streams
 * @param pgev gev object handle
 * @param s String
 */
void  GEV_CALLCONV d_gevLogStatNoC (gevHandle_t pgev, const char *s)
{
  int d_s[]={0,11};
  GAMS_UNUSED(pgev)
  GAMS_UNUSED(s)
  printNoReturn(gevLogStatNoC,1)
}

/** Send string to log and status streams, no newline added
 * @param pgev gev object handle
 * @param p Pointer to array of characters
 */
void  GEV_CALLCONV d_gevLogStatPChar (gevHandle_t pgev, const char *p)
{
  int d_s[]={0,9};
  GAMS_UNUSED(pgev)
  GAMS_UNUSED(p)
  printNoReturn(gevLogStatPChar,1)
}

/** Flush status streams (does not work with callback)
 * @param pgev gev object handle
 */
void  GEV_CALLCONV d_gevLogStatFlush (gevHandle_t pgev)
{
  int d_s[]={0};
  GAMS_UNUSED(pgev)
  printNoReturn(gevLogStatFlush,0)
}

/** Get anchor line for log (points to file and is clickable in GAMS IDE)
 * @param pgev gev object handle
 * @param s String
 */
char * GEV_CALLCONV d_gevGetAnchor (gevHandle_t pgev, const char *s, char *buf)
{
  int d_s[]={12,11};
  GAMS_UNUSED(pgev)
  GAMS_UNUSED(s)
  GAMS_UNUSED(buf)
  printAndReturn(gevGetAnchor,1,char *)
}

/** Put a line to log that points to the current lst line"
 * @param pgev gev object handle
 * @param s String
 */
void  GEV_CALLCONV d_gevLSTAnchor (gevHandle_t pgev, const char *s)
{
  int d_s[]={0,11};
  GAMS_UNUSED(pgev)
  GAMS_UNUSED(s)
  printNoReturn(gevLSTAnchor,1)
}

/** Append status file to current status file
 * @param pgev gev object handle
 * @param statfn Status file name
 * @param msg Message
 */
int  GEV_CALLCONV d_gevStatAppend (gevHandle_t pgev, const char *statfn, char *msg)
{
  int d_s[]={3,11,12};
  GAMS_UNUSED(pgev)
  GAMS_UNUSED(statfn)
  GAMS_UNUSED(msg)
  printAndReturn(gevStatAppend,2,int )
}

/** Print MIP report to log and lst
 * @param pgev gev object handle
 * @param gmoptr Pointer to GAMS modeling object
 * @param fixobj 
 * @param fixiter 
 * @param agap 
 * @param rgap 
 */
void  GEV_CALLCONV d_gevMIPReport (gevHandle_t pgev, void *gmoptr, double fixobj, int fixiter, double agap, double rgap)
{
  int d_s[]={0,1,13,3,13,13};
  GAMS_UNUSED(pgev)
  GAMS_UNUSED(gmoptr)
  GAMS_UNUSED(fixobj)
  GAMS_UNUSED(fixiter)
  GAMS_UNUSED(agap)
  GAMS_UNUSED(rgap)
  printNoReturn(gevMIPReport,5)
}

/** Name of solver executable
 * @param pgev gev object handle
 * @param solvername Name of solver
 * @param exename Name of solver executable
 */
int  GEV_CALLCONV d_gevGetSlvExeInfo (gevHandle_t pgev, const char *solvername, char *exename)
{
  int d_s[]={3,11,12};
  GAMS_UNUSED(pgev)
  GAMS_UNUSED(solvername)
  GAMS_UNUSED(exename)
  printAndReturn(gevGetSlvExeInfo,2,int )
}

/** Solver library name, prefix, and API version
 * @param pgev gev object handle
 * @param solvername Name of solver
 * @param libname Name of solver library
 * @param prefix Prefix of solver
 * @param ifversion Version of solver interface
 */
int  GEV_CALLCONV d_gevGetSlvLibInfo (gevHandle_t pgev, const char *solvername, char *libname, char *prefix, int *ifversion)
{
  int d_s[]={3,11,12,12,4};
  GAMS_UNUSED(pgev)
  GAMS_UNUSED(solvername)
  GAMS_UNUSED(libname)
  GAMS_UNUSED(prefix)
  GAMS_UNUSED(ifversion)
  printAndReturn(gevGetSlvLibInfo,4,int )
}

/** Check if solver is capable to handle model type given
 * @param pgev gev object handle
 * @param modeltype Modeltype
 * @param solvername Name of solver
 * @param capable Flag whether solver is capable or not
 */
int  GEV_CALLCONV d_gevCapabilityCheck (gevHandle_t pgev, int modeltype, const char *solvername, int *capable)
{
  int d_s[]={3,3,11,20};
  GAMS_UNUSED(pgev)
  GAMS_UNUSED(modeltype)
  GAMS_UNUSED(solvername)
  GAMS_UNUSED(capable)
  printAndReturn(gevCapabilityCheck,3,int )
}

/** Provide information if solver is hidden
 * @param pgev gev object handle
 * @param solvername Name of solver
 * @param hidden 
 * @param defaultok 
 */
int  GEV_CALLCONV d_gevSolverVisibility (gevHandle_t pgev, const char *solvername, int *hidden, int *defaultok)
{
  int d_s[]={3,11,20,20};
  GAMS_UNUSED(pgev)
  GAMS_UNUSED(solvername)
  GAMS_UNUSED(hidden)
  GAMS_UNUSED(defaultok)
  printAndReturn(gevSolverVisibility,3,int )
}

/** Number of solvers in the system
 * @param pgev gev object handle
 */
int  GEV_CALLCONV d_gevNumSolvers (gevHandle_t pgev)
{
  int d_s[]={3};
  GAMS_UNUSED(pgev)
  printAndReturn(gevNumSolvers,0,int )
}

/** Name of the solver chosen for modeltype (if non is chosen, it is the default)
 * @param pgev gev object handle
 * @param modeltype Modeltype
 */
char * GEV_CALLCONV d_gevGetSolver (gevHandle_t pgev, int modeltype, char *buf)
{
  int d_s[]={12,3};
  GAMS_UNUSED(pgev)
  GAMS_UNUSED(modeltype)
  GAMS_UNUSED(buf)
  printAndReturn(gevGetSolver,1,char *)
}

/** Name of the select solver
 * @param pgev gev object handle
 * @param gmoptr Pointer to GAMS modeling object
 */
char * GEV_CALLCONV d_gevGetCurrentSolver (gevHandle_t pgev, void *gmoptr, char *buf)
{
  int d_s[]={12,1};
  GAMS_UNUSED(pgev)
  GAMS_UNUSED(gmoptr)
  GAMS_UNUSED(buf)
  printAndReturn(gevGetCurrentSolver,1,char *)
}

/** Name of the default solver for modeltype
 * @param pgev gev object handle
 * @param modeltype Modeltype
 */
char * GEV_CALLCONV d_gevGetSolverDefault (gevHandle_t pgev, int modeltype, char *buf)
{
  int d_s[]={12,3};
  GAMS_UNUSED(pgev)
  GAMS_UNUSED(modeltype)
  GAMS_UNUSED(buf)
  printAndReturn(gevGetSolverDefault,1,char *)
}

/** Internal ID of solver, 0 for failure
 * @param pgev gev object handle
 * @param solvername Name of solver
 */
int  GEV_CALLCONV d_gevSolver2Id (gevHandle_t pgev, const char *solvername)
{
  int d_s[]={3,11};
  GAMS_UNUSED(pgev)
  GAMS_UNUSED(solvername)
  printAndReturn(gevSolver2Id,1,int )
}

/** Solver name
 * @param pgev gev object handle
 * @param solverid Internal ID of solver
 */
char * GEV_CALLCONV d_gevId2Solver (gevHandle_t pgev, int solverid, char *buf)
{
  int d_s[]={12,3};
  GAMS_UNUSED(pgev)
  GAMS_UNUSED(solverid)
  GAMS_UNUSED(buf)
  printAndReturn(gevId2Solver,1,char *)
}

/** Creates grid directory for next gevCallSolver call and returns name (if called with gevSolveLinkAsyncGrid or gevSolveLinkAsyncSimulate)
 * @param pgev gev object handle
 */
char * GEV_CALLCONV d_gevCallSolverNextGridDir (gevHandle_t pgev, char *buf)
{
  int d_s[]={12};
  GAMS_UNUSED(pgev)
  GAMS_UNUSED(buf)
  printAndReturn(gevCallSolverNextGridDir,0,char *)
}

/** Call GAMS solver on GMO model or control file
 * @param pgev gev object handle
 * @param gmoptr Pointer to GAMS modeling object
 * @param cntrfn Name of control file
 * @param solvername Name of solver
 * @param solvelink Solvelink option for solver called through gevCallSolver (see enumerated constants)
 * @param Logging Log option for solver called through gevCallSolver (see enumerated constants)
 * @param logfn Log file name
 * @param statfn Status file name
 * @param reslim Resource limit
 * @param iterlim Iteration limit
 * @param domlim Domain violation limit
 * @param optcr Optimality criterion for relative gap
 * @param optca Optimality criterion for absolute gap
 * @param jobhandle Handle to solver job in case of solvelink=gevSolveLinkAsyncGrid
 * @param msg Message
 */
int  GEV_CALLCONV d_gevCallSolver (gevHandle_t pgev, void *gmoptr, const char *cntrfn, const char *solvername, int solvelink, int Logging, const char *logfn, const char *statfn, double reslim, int iterlim, int domlim, double optcr, double optca, void **jobhandle, char *msg)
{
  int d_s[]={3,1,11,11,3,3,11,11,13,3,3,13,13,2,12};
  GAMS_UNUSED(pgev)
  GAMS_UNUSED(gmoptr)
  GAMS_UNUSED(cntrfn)
  GAMS_UNUSED(solvername)
  GAMS_UNUSED(solvelink)
  GAMS_UNUSED(Logging)
  GAMS_UNUSED(logfn)
  GAMS_UNUSED(statfn)
  GAMS_UNUSED(reslim)
  GAMS_UNUSED(iterlim)
  GAMS_UNUSED(domlim)
  GAMS_UNUSED(optcr)
  GAMS_UNUSED(optca)
  GAMS_UNUSED(jobhandle)
  GAMS_UNUSED(msg)
  printAndReturn(gevCallSolver,14,int )
}

/** Check status of solver job if called with gevSolveLinkAsyncGrid (0 job is done, 1 unknown job handle, 2 job is running)
 * @param pgev gev object handle
 * @param jobhandle Handle to solver job in case of solvelink=gevSolveLinkAsyncGrid
 */
int  GEV_CALLCONV d_gevCallSolverHandleStatus (gevHandle_t pgev, void *jobhandle)
{
  int d_s[]={3,1};
  GAMS_UNUSED(pgev)
  GAMS_UNUSED(jobhandle)
  printAndReturn(gevCallSolverHandleStatus,1,int )
}

/** Delete instance of solver job if called with gevSolveLinkAsyncGrid (0 deleted, 1 unknown job handle, 2 deletion failed)
 * @param pgev gev object handle
 * @param jobhandle Handle to solver job in case of solvelink=gevSolveLinkAsyncGrid
 */
int  GEV_CALLCONV d_gevCallSolverHandleDelete (gevHandle_t pgev, void **jobhandle)
{
  int d_s[]={3,2};
  GAMS_UNUSED(pgev)
  GAMS_UNUSED(jobhandle)
  printAndReturn(gevCallSolverHandleDelete,1,int )
}

/** Collect solution from solver job if called with gevSolveLinkAsyncGrid (0 loaded, 1 unknown job handle, 2 job is running, 3 other error), delete instance
 * @param pgev gev object handle
 * @param jobhandle Handle to solver job in case of solvelink=gevSolveLinkAsyncGrid
 * @param gmoptr Pointer to GAMS modeling object
 */
int  GEV_CALLCONV d_gevCallSolverHandleCollect (gevHandle_t pgev, void **jobhandle, void *gmoptr)
{
  int d_s[]={3,2,1};
  GAMS_UNUSED(pgev)
  GAMS_UNUSED(jobhandle)
  GAMS_UNUSED(gmoptr)
  printAndReturn(gevCallSolverHandleCollect,2,int )
}

/** Get integer valued option (see enumerated constants)
 * @param pgev gev object handle
 * @param optname Name of option (see enumerated constants)
 */
int  GEV_CALLCONV d_gevGetIntOpt (gevHandle_t pgev, const char *optname)
{
  int d_s[]={3,11};
  GAMS_UNUSED(pgev)
  GAMS_UNUSED(optname)
  printAndReturn(gevGetIntOpt,1,int )
}

/** Get double valued option (see enumerated constants)
 * @param pgev gev object handle
 * @param optname Name of option (see enumerated constants)
 */
double  GEV_CALLCONV d_gevGetDblOpt (gevHandle_t pgev, const char *optname)
{
  int d_s[]={13,11};
  GAMS_UNUSED(pgev)
  GAMS_UNUSED(optname)
  printAndReturn(gevGetDblOpt,1,double )
}

/** Get string valued option (see enumerated constants)
 * @param pgev gev object handle
 * @param optname Name of option (see enumerated constants)
 */
char * GEV_CALLCONV d_gevGetStrOpt (gevHandle_t pgev, const char *optname, char *buf)
{
  int d_s[]={12,11};
  GAMS_UNUSED(pgev)
  GAMS_UNUSED(optname)
  GAMS_UNUSED(buf)
  printAndReturn(gevGetStrOpt,1,char *)
}

/** Set integer valued option (see enumerated constants)
 * @param pgev gev object handle
 * @param optname Name of option (see enumerated constants)
 * @param ival Integer value
 */
void  GEV_CALLCONV d_gevSetIntOpt (gevHandle_t pgev, const char *optname, int ival)
{
  int d_s[]={0,11,3};
  GAMS_UNUSED(pgev)
  GAMS_UNUSED(optname)
  GAMS_UNUSED(ival)
  printNoReturn(gevSetIntOpt,2)
}

/** Set double valued option (see enumerated constants)
 * @param pgev gev object handle
 * @param optname Name of option (see enumerated constants)
 * @param rval Real/Double value
 */
void  GEV_CALLCONV d_gevSetDblOpt (gevHandle_t pgev, const char *optname, double rval)
{
  int d_s[]={0,11,13};
  GAMS_UNUSED(pgev)
  GAMS_UNUSED(optname)
  GAMS_UNUSED(rval)
  printNoReturn(gevSetDblOpt,2)
}

/** Set string valued option (see enumerated constants)
 * @param pgev gev object handle
 * @param optname Name of option (see enumerated constants)
 * @param sval String value
 */
void  GEV_CALLCONV d_gevSetStrOpt (gevHandle_t pgev, const char *optname, const char *sval)
{
  int d_s[]={0,11,11};
  GAMS_UNUSED(pgev)
  GAMS_UNUSED(optname)
  GAMS_UNUSED(sval)
  printNoReturn(gevSetStrOpt,2)
}

/** Copy environment options to passed in option object
 * @param pgev gev object handle
 * @param optptr Pointer to option object
 */
void  GEV_CALLCONV d_gevSynchronizeOpt (gevHandle_t pgev, void *optptr)
{
  int d_s[]={0,1};
  GAMS_UNUSED(pgev)
  GAMS_UNUSED(optptr)
  printNoReturn(gevSynchronizeOpt,1)
}

/** GAMS Julian time
 * @param pgev gev object handle
 */
double  GEV_CALLCONV d_gevTimeJNow (gevHandle_t pgev)
{
  int d_s[]={13};
  GAMS_UNUSED(pgev)
  printAndReturn(gevTimeJNow,0,double )
}

/** Time difference in seconds since creation or last call to gevTimeDiff
 * @param pgev gev object handle
 */
double  GEV_CALLCONV d_gevTimeDiff (gevHandle_t pgev)
{
  int d_s[]={13};
  GAMS_UNUSED(pgev)
  printAndReturn(gevTimeDiff,0,double )
}

/** Time difference in seconds since creation of object
 * @param pgev gev object handle
 */
double  GEV_CALLCONV d_gevTimeDiffStart (gevHandle_t pgev)
{
  int d_s[]={13};
  GAMS_UNUSED(pgev)
  printAndReturn(gevTimeDiffStart,0,double )
}

/** Reset timer (overwrites time stamp from creation)
 * @param pgev gev object handle
 */
void  GEV_CALLCONV d_gevTimeSetStart (gevHandle_t pgev)
{
  int d_s[]={0};
  GAMS_UNUSED(pgev)
  printNoReturn(gevTimeSetStart,0)
}

/** Uninstalls an already registered interrupt handler
 * @param pgev gev object handle
 */
void  GEV_CALLCONV d_gevTerminateUninstall (gevHandle_t pgev)
{
  int d_s[]={0};
  GAMS_UNUSED(pgev)
  printNoReturn(gevTerminateUninstall,0)
}

/** Installs an already registered interrupt handler
 * @param pgev gev object handle
 */
void  GEV_CALLCONV d_gevTerminateInstall (gevHandle_t pgev)
{
  int d_s[]={0};
  GAMS_UNUSED(pgev)
  printNoReturn(gevTerminateInstall,0)
}

/** Register a pointer to some memory that will indicate an interrupt and the pointer to a interrupt handler and installs it
 * @param pgev gev object handle
 * @param intr Pointer to some memory indicating an interrupt
 * @param ehdler Pointer to interrupt handler
 */
void  GEV_CALLCONV d_gevTerminateSet (gevHandle_t pgev, void *intr, void *ehdler)
{
  int d_s[]={0,1,1};
  GAMS_UNUSED(pgev)
  GAMS_UNUSED(intr)
  GAMS_UNUSED(ehdler)
  printNoReturn(gevTerminateSet,2)
}

/** Check if one should interrupt
 * @param pgev gev object handle
 */
int  GEV_CALLCONV d_gevTerminateGet (gevHandle_t pgev)
{
  int d_s[]={15};
  GAMS_UNUSED(pgev)
  printAndReturn(gevTerminateGet,0,int )
}

/** Resets the interrupt counter
 * @param pgev gev object handle
 */
void  GEV_CALLCONV d_gevTerminateClear (gevHandle_t pgev)
{
  int d_s[]={0};
  GAMS_UNUSED(pgev)
  printNoReturn(gevTerminateClear,0)
}

/** Increases the interrupt counter
 * @param pgev gev object handle
 */
void  GEV_CALLCONV d_gevTerminateRaise (gevHandle_t pgev)
{
  int d_s[]={0};
  GAMS_UNUSED(pgev)
  printNoReturn(gevTerminateRaise,0)
}

/** Get installed termination handler
 * @param pgev gev object handle
 * @param intr Pointer to some memory indicating an interrupt
 * @param ehdler Pointer to interrupt handler
 */
void  GEV_CALLCONV d_gevTerminateGetHandler (gevHandle_t pgev, void **intr, void **ehdler)
{
  int d_s[]={0,2,2};
  GAMS_UNUSED(pgev)
  GAMS_UNUSED(intr)
  GAMS_UNUSED(ehdler)
  printNoReturn(gevTerminateGetHandler,2)
}

/** Get scratch file name plus scratch extension including path of scratch directory
 * @param pgev gev object handle
 * @param s String
 */
char * GEV_CALLCONV d_gevGetScratchName (gevHandle_t pgev, const char *s, char *buf)
{
  int d_s[]={12,11};
  GAMS_UNUSED(pgev)
  GAMS_UNUSED(s)
  GAMS_UNUSED(buf)
  printAndReturn(gevGetScratchName,1,char *)
}

/** Creates model instance file
 * @param pgev gev object handle
 * @param mifn Model instance file name
 * @param gmoptr Pointer to GAMS modeling object
 * @param nlcodelen Length of nonlinear code
 */
int  GEV_CALLCONV d_gevWriteModelInstance (gevHandle_t pgev, const char *mifn, void *gmoptr, int *nlcodelen)
{
  int d_s[]={3,11,1,21};
  GAMS_UNUSED(pgev)
  GAMS_UNUSED(mifn)
  GAMS_UNUSED(gmoptr)
  GAMS_UNUSED(nlcodelen)
  printAndReturn(gevWriteModelInstance,3,int )
}

/** Duplicates a scratch directory and points to read only files in source scratch directory
 * @param pgev gev object handle
 * @param scrdir Scratch directory
 * @param logfn Log file name
 * @param cntrfn Name of control file
 */
int  GEV_CALLCONV d_gevDuplicateScratchDir (gevHandle_t pgev, const char *scrdir, const char *logfn, char *cntrfn)
{
  int d_s[]={3,11,11,12};
  GAMS_UNUSED(pgev)
  GAMS_UNUSED(scrdir)
  GAMS_UNUSED(logfn)
  GAMS_UNUSED(cntrfn)
  printAndReturn(gevDuplicateScratchDir,3,int )
}

/** Legacy Jacobian Evaluation: Initialize row wise Jacobian structure
 * @param pgev gev object handle
 * @param evalptr Pointer to structure for legacy Jacobian evaluation
 * @param gmoptr Pointer to GAMS modeling object
 */
int  GEV_CALLCONV d_gevInitJacLegacy (gevHandle_t pgev, void **evalptr, void *gmoptr)
{
  int d_s[]={3,2,1};
  GAMS_UNUSED(pgev)
  GAMS_UNUSED(evalptr)
  GAMS_UNUSED(gmoptr)
  printAndReturn(gevInitJacLegacy,2,int )
}

/** Legacy Jacobian Evaluation: Set column and row permutation GAMS to solver
 * @param pgev gev object handle
 * @param evalptr Pointer to structure for legacy Jacobian evaluation
 * @param n Number of variables
 * @param cgms2slv GAMS to solver permutation of columns
 * @param m Number of constraints
 * @param rgms2slv GAMS to solver permutation of rows
 */
void  GEV_CALLCONV d_gevSetColRowPermLegacy (gevHandle_t pgev, void *evalptr, int n, int cgms2slv[], int m, int rgms2slv[])
{
  int d_s[]={0,1,3,8,3,8};
  GAMS_UNUSED(pgev)
  GAMS_UNUSED(evalptr)
  GAMS_UNUSED(n)
  GAMS_UNUSED(cgms2slv)
  GAMS_UNUSED(m)
  GAMS_UNUSED(rgms2slv)
  printNoReturn(gevSetColRowPermLegacy,5)
}

/** Legacy Jacobian Evaluation: Set Jacobian permutation GAMS to solver
 * @param pgev gev object handle
 * @param evalptr Pointer to structure for legacy Jacobian evaluation
 * @param njacs Number of Jacobian elements in jacs and jgms2slv arrays
 * @param jacs Array of original indices of Jacobian elements (1-based), length njacs
 * @param jgms2slv GAMS to solver permutation of Jacobian elements, length njacs
 */
void  GEV_CALLCONV d_gevSetJacPermLegacy (gevHandle_t pgev, void *evalptr, int njacs, int jacs[], int jgms2slv[])
{
  int d_s[]={0,1,3,8,8};
  GAMS_UNUSED(pgev)
  GAMS_UNUSED(evalptr)
  GAMS_UNUSED(njacs)
  GAMS_UNUSED(jacs)
  GAMS_UNUSED(jgms2slv)
  printNoReturn(gevSetJacPermLegacy,4)
}

/** Legacy Jacobian Evaluation: Set new point and do point copy magic
 * @param pgev gev object handle
 * @param evalptr Pointer to structure for legacy Jacobian evaluation
 * @param x Input values for variables
 */
int  GEV_CALLCONV d_gevEvalNewPointLegacy (gevHandle_t pgev, void *evalptr, double x[])
{
  int d_s[]={3,1,6};
  GAMS_UNUSED(pgev)
  GAMS_UNUSED(evalptr)
  GAMS_UNUSED(x)
  printAndReturn(gevEvalNewPointLegacy,2,int )
}

/** Legacy Jacobian Evaluation: Evaluate row and store in Jacobian structure
 * @param pgev gev object handle
 * @param evalptr Pointer to structure for legacy Jacobian evaluation
 * @param si Solve index for row i
 * @param x Input values for variables
 * @param f Function value
 * @param jac Array to store the gradients
 * @param domviol Domain violations
 * @param njacsupd Number of Jacobian elements updated
 */
int  GEV_CALLCONV d_gevEvalJacLegacy (gevHandle_t pgev, void *evalptr, int si, double x[], double *f, double jac[], int *domviol, int *njacsupd)
{
  int d_s[]={3,1,3,6,22,6,21,21};
  GAMS_UNUSED(pgev)
  GAMS_UNUSED(evalptr)
  GAMS_UNUSED(si)
  GAMS_UNUSED(x)
  GAMS_UNUSED(f)
  GAMS_UNUSED(jac)
  GAMS_UNUSED(domviol)
  GAMS_UNUSED(njacsupd)
  printAndReturn(gevEvalJacLegacy,7,int )
}

/** Legacy Jacobian Evaluation: Evaluate set of rows and store in Jacobian structure
 * @param pgev gev object handle
 * @param evalptr Pointer to structure for legacy Jacobian evaluation
 * @param cnt count
 * @param rowidx Vector of row indicies
 * @param x Input values for variables
 * @param fvec Vector of function values
 * @param jac Array to store the gradients
 * @param domviol Domain violations
 * @param njacsupd Number of Jacobian elements updated
 */
int  GEV_CALLCONV d_gevEvalJacLegacyX (gevHandle_t pgev, void *evalptr, int cnt, int rowidx[], double x[], double fvec[], double jac[], int *domviol, int *njacsupd)
{
  int d_s[]={3,1,3,8,6,6,6,21,21};
  GAMS_UNUSED(pgev)
  GAMS_UNUSED(evalptr)
  GAMS_UNUSED(cnt)
  GAMS_UNUSED(rowidx)
  GAMS_UNUSED(x)
  GAMS_UNUSED(fvec)
  GAMS_UNUSED(jac)
  GAMS_UNUSED(domviol)
  GAMS_UNUSED(njacsupd)
  printAndReturn(gevEvalJacLegacyX,8,int )
}

/** Legacy Jacobian Evaluation: Provide next nonlinear row, start with M
 * @param pgev gev object handle
 * @param evalptr Pointer to structure for legacy Jacobian evaluation
 * @param si Solve index for row i
 */
int  GEV_CALLCONV d_gevNextNLLegacy (gevHandle_t pgev, void *evalptr, int si)
{
  int d_s[]={3,1,3};
  GAMS_UNUSED(pgev)
  GAMS_UNUSED(evalptr)
  GAMS_UNUSED(si)
  printAndReturn(gevNextNLLegacy,2,int )
}

/** Legacy Jacobian Evaluation: Provide permuted row index
 * @param pgev gev object handle
 * @param evalptr Pointer to structure for legacy Jacobian evaluation
 * @param si Solve index for row i
 */
int  GEV_CALLCONV d_gevRowGms2SlvLegacy (gevHandle_t pgev, void *evalptr, int si)
{
  int d_s[]={3,1,3};
  GAMS_UNUSED(pgev)
  GAMS_UNUSED(evalptr)
  GAMS_UNUSED(si)
  printAndReturn(gevRowGms2SlvLegacy,2,int )
}

/** Legacy Jacobian Evaluation: Free row wise Jacobian structure
 * @param pgev gev object handle
 * @param evalptr Pointer to structure for legacy Jacobian evaluation
 */
void  GEV_CALLCONV d_gevFreeJacLegacy (gevHandle_t pgev, void **evalptr)
{
  int d_s[]={0,2};
  GAMS_UNUSED(pgev)
  GAMS_UNUSED(evalptr)
  printNoReturn(gevFreeJacLegacy,1)
}

/** Pass pointer to ALGX structure
 * @param pgev gev object handle
 */
void * GEV_CALLCONV d_gevGetALGX (gevHandle_t pgev)
{
  int d_s[]={1};
  GAMS_UNUSED(pgev)
  printAndReturn(gevGetALGX,0,void *)
}

/** Prevent log and status file to be opened
 * @param pgev gev object handle
 */
void GEV_CALLCONV d_gevSkipIOLegacySet (gevHandle_t pgev,const int x)
{
  int d_s[]={0,15};
  GAMS_UNUSED(pgev)
  GAMS_UNUSED(x)
  printNoReturn(gevSkipIOLegacySet,1)
}

/** Number of threads (1..n); if option gevThreadsRaw = 0, this function gives the number of available processors
 * @param pgev gev object handle
 */
int  GEV_CALLCONV d_gevThreads (gevHandle_t pgev)
{
  int d_s[]={3};
  GAMS_UNUSED(pgev)
  printAndReturn(gevThreads,0,int )
}

/** Number of solves
 * @param pgev gev object handle
 */
double  GEV_CALLCONV d_gevNSolves (gevHandle_t pgev)
{
  int d_s[]={13};
  GAMS_UNUSED(pgev)
  printAndReturn(gevNSolves,0,double )
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

  LOADIT(gevXCreate, "gevXCreate");
  LOADIT(gevXCreateD, "CgevXCreateD");

  LOADIT(gevXFree, "gevXFree");
  LOADIT(gevXCheck, "CgevXCheck");
  LOADIT(gevXAPIVersion, "CgevXAPIVersion");

  if (!gevXAPIVersion(8,errBuf,&cl))
    return 1;

#define CheckAndLoad(f,nargs,prefix) \
  if (!gevXCheck(#f,nargs,s,errBuf)) \
    f = &d_##f; \
  else { \
    LOADIT(f,prefix #f); \
  }
  {int s[]={0,59,15,1}; CheckAndLoad(gevRegisterWriteCallback,3,""); }
  {int s[]={0,1,1,1,1}; CheckAndLoad(gevCompleteEnvironment,4,""); }
  {int s[]={3,11}; CheckAndLoad(gevInitEnvironmentLegacy,1,"C"); }
  {int s[]={15,3,11,15,11,15,59,1,2}; CheckAndLoad(gevSwitchLogStat,8,"C"); }
  {int s[]={15,3,11,15,11,15,59,1,2,15}; CheckAndLoad(gevSwitchLogStatEx,9,"C"); }
  {int s[]={1}; CheckAndLoad(gevGetLShandle,0,""); }
  {int s[]={15,2}; CheckAndLoad(gevRestoreLogStat,1,""); }
  {int s[]={15,2}; CheckAndLoad(gevRestoreLogStatRewrite,1,""); }
  {int s[]={0,11}; CheckAndLoad(gevLog,1,"C"); }
  {int s[]={0,9}; CheckAndLoad(gevLogPChar,1,""); }
  {int s[]={0,11}; CheckAndLoad(gevStat,1,"C"); }
  {int s[]={0,11}; CheckAndLoad(gevStatC,1,"C"); }
  {int s[]={0,9}; CheckAndLoad(gevStatPChar,1,""); }
  {int s[]={0,11}; CheckAndLoad(gevStatAudit,1,"C"); }
  {int s[]={0}; CheckAndLoad(gevStatCon,0,""); }
  {int s[]={0}; CheckAndLoad(gevStatCoff,0,""); }
  {int s[]={0}; CheckAndLoad(gevStatEOF,0,""); }
  {int s[]={0}; CheckAndLoad(gevStatSysout,0,""); }
  {int s[]={0,3,11}; CheckAndLoad(gevStatAddE,2,"C"); }
  {int s[]={0,3,11}; CheckAndLoad(gevStatAddV,2,"C"); }
  {int s[]={0,3,3,11}; CheckAndLoad(gevStatAddJ,3,"C"); }
  {int s[]={0}; CheckAndLoad(gevStatEject,0,""); }
  {int s[]={0,18}; CheckAndLoad(gevStatEdit,1,""); }
  {int s[]={0,11,3,11}; CheckAndLoad(gevStatE,3,"C"); }
  {int s[]={0,11,3,11}; CheckAndLoad(gevStatV,3,"C"); }
  {int s[]={0}; CheckAndLoad(gevStatT,0,""); }
  {int s[]={0,11}; CheckAndLoad(gevStatA,1,"C"); }
  {int s[]={0,11}; CheckAndLoad(gevStatB,1,"C"); }
  {int s[]={0,11}; CheckAndLoad(gevLogStat,1,"C"); }
  {int s[]={0,11}; CheckAndLoad(gevLogStatNoC,1,"C"); }
  {int s[]={0,9}; CheckAndLoad(gevLogStatPChar,1,""); }
  {int s[]={0}; CheckAndLoad(gevLogStatFlush,0,""); }
  {int s[]={12,11}; CheckAndLoad(gevGetAnchor,1,"C"); }
  {int s[]={0,11}; CheckAndLoad(gevLSTAnchor,1,"C"); }
  {int s[]={3,11,12}; CheckAndLoad(gevStatAppend,2,"C"); }
  {int s[]={0,1,13,3,13,13}; CheckAndLoad(gevMIPReport,5,""); }
  {int s[]={3,11,12}; CheckAndLoad(gevGetSlvExeInfo,2,"C"); }
  {int s[]={3,11,12,12,4}; CheckAndLoad(gevGetSlvLibInfo,4,"C"); }
  {int s[]={3,3,11,20}; CheckAndLoad(gevCapabilityCheck,3,"C"); }
  {int s[]={3,11,20,20}; CheckAndLoad(gevSolverVisibility,3,"C"); }
  {int s[]={3}; CheckAndLoad(gevNumSolvers,0,""); }
  {int s[]={12,3}; CheckAndLoad(gevGetSolver,1,"C"); }
  {int s[]={12,1}; CheckAndLoad(gevGetCurrentSolver,1,"C"); }
  {int s[]={12,3}; CheckAndLoad(gevGetSolverDefault,1,"C"); }
  {int s[]={3,11}; CheckAndLoad(gevSolver2Id,1,"C"); }
  {int s[]={12,3}; CheckAndLoad(gevId2Solver,1,"C"); }
  {int s[]={12}; CheckAndLoad(gevCallSolverNextGridDir,0,"C"); }
  {int s[]={3,1,11,11,3,3,11,11,13,3,3,13,13,2,12}; CheckAndLoad(gevCallSolver,14,"C"); }
  {int s[]={3,1}; CheckAndLoad(gevCallSolverHandleStatus,1,""); }
  {int s[]={3,2}; CheckAndLoad(gevCallSolverHandleDelete,1,""); }
  {int s[]={3,2,1}; CheckAndLoad(gevCallSolverHandleCollect,2,""); }
  {int s[]={3,11}; CheckAndLoad(gevGetIntOpt,1,"C"); }
  {int s[]={13,11}; CheckAndLoad(gevGetDblOpt,1,"C"); }
  {int s[]={12,11}; CheckAndLoad(gevGetStrOpt,1,"C"); }
  {int s[]={0,11,3}; CheckAndLoad(gevSetIntOpt,2,"C"); }
  {int s[]={0,11,13}; CheckAndLoad(gevSetDblOpt,2,"C"); }
  {int s[]={0,11,11}; CheckAndLoad(gevSetStrOpt,2,"C"); }
  {int s[]={0,1}; CheckAndLoad(gevSynchronizeOpt,1,""); }
  {int s[]={13}; CheckAndLoad(gevTimeJNow,0,""); }
  {int s[]={13}; CheckAndLoad(gevTimeDiff,0,""); }
  {int s[]={13}; CheckAndLoad(gevTimeDiffStart,0,""); }
  {int s[]={0}; CheckAndLoad(gevTimeSetStart,0,""); }
  {int s[]={0}; CheckAndLoad(gevTerminateUninstall,0,""); }
  {int s[]={0}; CheckAndLoad(gevTerminateInstall,0,""); }
  {int s[]={0,1,1}; CheckAndLoad(gevTerminateSet,2,""); }
  {int s[]={15}; CheckAndLoad(gevTerminateGet,0,""); }
  {int s[]={0}; CheckAndLoad(gevTerminateClear,0,""); }
  {int s[]={0}; CheckAndLoad(gevTerminateRaise,0,""); }
  {int s[]={0,2,2}; CheckAndLoad(gevTerminateGetHandler,2,""); }
  {int s[]={12,11}; CheckAndLoad(gevGetScratchName,1,"C"); }
  {int s[]={3,11,1,21}; CheckAndLoad(gevWriteModelInstance,3,"C"); }
  {int s[]={3,11,11,12}; CheckAndLoad(gevDuplicateScratchDir,3,"C"); }
  {int s[]={3,2,1}; CheckAndLoad(gevInitJacLegacy,2,""); }
  {int s[]={0,1,3,8,3,8}; CheckAndLoad(gevSetColRowPermLegacy,5,""); }
  {int s[]={0,1,3,8,8}; CheckAndLoad(gevSetJacPermLegacy,4,""); }
  {int s[]={3,1,6}; CheckAndLoad(gevEvalNewPointLegacy,2,""); }
  {int s[]={3,1,3,6,22,6,21,21}; CheckAndLoad(gevEvalJacLegacy,7,""); }
  {int s[]={3,1,3,8,6,6,6,21,21}; CheckAndLoad(gevEvalJacLegacyX,8,""); }
  {int s[]={3,1,3}; CheckAndLoad(gevNextNLLegacy,2,""); }
  {int s[]={3,1,3}; CheckAndLoad(gevRowGms2SlvLegacy,2,""); }
  {int s[]={0,2}; CheckAndLoad(gevFreeJacLegacy,1,""); }
  {int s[]={1}; CheckAndLoad(gevGetALGX,0,""); }
  {int s[]={0,15}; CheckAndLoad(gevSkipIOLegacySet,1,""); }
  {int s[]={3}; CheckAndLoad(gevThreads,0,""); }
  {int s[]={13}; CheckAndLoad(gevNSolves,0,""); }

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

int gevGetReady (char *msgBuf, int msgBufSize)
{
  int rc;
  lock(libMutex);
  rc = libloader(NULL, NULL, msgBuf, msgBufSize);
  unlock(libMutex);
  return rc;
} /* gevGetReady */

int gevGetReadyD (const char *dirName, char *msgBuf, int msgBufSize)
{
  int rc;
  lock(libMutex);
  rc = libloader(dirName, NULL, msgBuf, msgBufSize);
  unlock(libMutex);
  return rc;
} /* gevGetReadyD */

int gevGetReadyL (const char *libName, char *msgBuf, int msgBufSize)
{
  char dirName[1024],fName[1024];
  int rc;
  extractFileDirFileName (libName, dirName, fName);
  lock(libMutex);
  rc = libloader(dirName, fName, msgBuf, msgBufSize);
  unlock(libMutex);
  return rc;
} /* gevGetReadyL */

int gevCreate (gevHandle_t *pgev, char *msgBuf, int msgBufSize)
{
  int gevIsReady;

  gevIsReady = gevGetReady (msgBuf, msgBufSize);
  if (! gevIsReady) {
    return 0;
  }
  assert(gevXCreate);
  gevXCreate(pgev);
  if(pgev == NULL)
  { strcpy(msgBuf,"Error while creating object"); return 0; }
  lock(objMutex);
  objectCount++;
  unlock(objMutex);
  return 1;                     /* return true on successful library load */
} /* gevCreate */

int gevCreateD (gevHandle_t *pgev, const char *dirName,
                char *msgBuf, int msgBufSize)
{
  int gevIsReady;

  gevIsReady = gevGetReadyD (dirName, msgBuf, msgBufSize);
  if (! gevIsReady) {
    return 0;
  }
  assert(gevXCreate);
  gevXCreate(pgev);
  if(pgev == NULL)
  { strcpy(msgBuf,"Error while creating object"); return 0; }
  lock(objMutex);
  objectCount++;
  unlock(objMutex);
  return 1;                     /* return true on successful library load */
} /* gevCreateD */

int gevCreateDD (gevHandle_t *pgev, const char *dirName,
                char *msgBuf, int msgBufSize)
{
  int gevIsReady;
  gevIsReady = gevGetReadyD (dirName, msgBuf, msgBufSize);
  if (! gevIsReady) {
    return 0;
  }
  assert(gevXCreateD);
  gevXCreateD(pgev, dirName);
  if(pgev == NULL)
  { strcpy(msgBuf,"Error while creating object"); return 0; }
  lock(objMutex);
  objectCount++;
  unlock(objMutex);
  return 1;                     /* return true on successful library load */
} /* gevCreateD */


int gevCreateL (gevHandle_t *pgev, const char *libName,
                char *msgBuf, int msgBufSize)
{
  int gevIsReady;

  gevIsReady = gevGetReadyL (libName, msgBuf, msgBufSize);
  if (! gevIsReady) {
    return 0;
  }
  assert(gevXCreate);
  gevXCreate(pgev);
  if(pgev == NULL)
  { strcpy(msgBuf,"Error while creating object"); return 0; }
  lock(objMutex);
  objectCount++;
  unlock(objMutex);
  return 1;                     /* return true on successful library load */
} /* gevCreateL */

int gevFree   (gevHandle_t *pgev)
{
  assert(gevXFree);
  gevXFree(pgev); pgev = NULL;
  lock(objMutex);
  objectCount--;
  unlock(objMutex);
  return 1;
} /* gevFree */

int gevLibraryLoaded(void)
{
  int rc;
  lock(libMutex);
  rc = isLoaded;
  unlock(libMutex);
  return rc;
} /* gevLibraryLoaded */

int gevLibraryUnload(void)
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
} /* gevLibraryUnload */

int  gevCorrectLibraryVersion(char *msgBuf, int msgBufLen)
{
  int cl;
  char localBuf[256];

  if (msgBuf && msgBufLen) msgBuf[0] = '\0';

  if (! isLoaded) {
    strncpy(msgBuf, "Library needs to be initialized first", msgBufLen);
    return 0;
  }

  if (NULL == gevXAPIVersion) {
    strncpy(msgBuf, "Function gevXAPIVersion not found", msgBufLen);
    return 0;
  }

  gevXAPIVersion(GEVAPIVERSION,localBuf,&cl);
  strncpy(msgBuf, localBuf, msgBufLen);

  if (1 == cl)
    return 1;
  else
    return 0;
}

int gevGetScreenIndicator(void)
{
  return ScreenIndicator;
}

void gevSetScreenIndicator(int scrind)
{
  ScreenIndicator = scrind ? 1 : 0;
}

int gevGetExceptionIndicator(void)
{
   return ExceptionIndicator;
}

void gevSetExceptionIndicator(int excind)
{
  ExceptionIndicator = excind ? 1 : 0;
}

int gevGetExitIndicator(void)
{
  return ExitIndicator;
}

void gevSetExitIndicator(int extind)
{
  ExitIndicator = extind ? 1 : 0;
}

gevErrorCallback_t gevGetErrorCallback(void)
{
  return ErrorCallBack;
}

void gevSetErrorCallback(gevErrorCallback_t func)
{
  lock(exceptMutex);
  ErrorCallBack = func;
  unlock(exceptMutex);
}

int gevGetAPIErrorCount(void)
{
  return APIErrorCount;
}

void gevSetAPIErrorCount(int ecnt)
{
  APIErrorCount = ecnt;
}

void gevErrorHandling(const char *msg)
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

