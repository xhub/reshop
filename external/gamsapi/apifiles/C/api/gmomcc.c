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

#define GMO_MAIN
#include "gmomcc.h"

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
static gmoErrorCallback_t ErrorCallBack = NULL;
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

void gmoInitMutexes(void)
{
  int rc;
  if (0==MutexIsInitialized) {
    rc = GC_mutex_init (&libMutex);     if(0!=rc) gmoErrorHandling("Problem initializing libMutex");
    rc = GC_mutex_init (&objMutex);     if(0!=rc) gmoErrorHandling("Problem initializing objMutex");
    rc = GC_mutex_init (&exceptMutex);  if(0!=rc) gmoErrorHandling("Problem initializing exceptMutex");
    MutexIsInitialized = 1;
  }
}

void gmoFiniMutexes(void)
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
void gmoInitMutexes(void) {}
void gmoFiniMutexes(void) {}
#endif

#if !defined(GAMS_UNUSED)
#define GAMS_UNUSED(x) (void)x;
#endif

typedef void (GMO_CALLCONV *gmoXCreate_t) (gmoHandle_t *pgmo);
static GMO_FUNCPTR(gmoXCreate);

typedef void (GMO_CALLCONV *gmoXCreateD_t) (gmoHandle_t *pgmo, const char *dirName);
static GMO_FUNCPTR(gmoXCreateD);
typedef void (GMO_CALLCONV *gmoXFree_t)   (gmoHandle_t *pgmo);
static GMO_FUNCPTR(gmoXFree);
typedef int (GMO_CALLCONV *gmoXAPIVersion_t) (int api, char *msg, int *cl);
static GMO_FUNCPTR(gmoXAPIVersion);
typedef int (GMO_CALLCONV *gmoXCheck_t) (const char *ep, int nargs, int s[], char *msg);
static GMO_FUNCPTR(gmoXCheck);
#define printNoReturn(f,nargs) { \
  char d_msgBuf[256]; \
  strcpy(d_msgBuf,#f " could not be loaded: "); \
  gmoXCheck(#f,nargs,d_s,d_msgBuf+strlen(d_msgBuf)); \
  gmoErrorHandling(d_msgBuf); \
}
#define printAndReturn(f,nargs,rtype) { \
  char d_msgBuf[256]; \
  strcpy(d_msgBuf,#f " could not be loaded: "); \
  gmoXCheck(#f,nargs,d_s,d_msgBuf+strlen(d_msgBuf)); \
  gmoErrorHandling(d_msgBuf); \
  return (rtype) 0; \
}


/** Initialize GMO data
 * @param pgmo gmo object handle
 * @param rows Number of rows
 * @param cols Number of columns
 * @param codelen length of NL code
 */
int  GMO_CALLCONV d_gmoInitData (gmoHandle_t pgmo, int rows, int cols, int codelen)
{
  int d_s[]={3,3,3,3};
  GAMS_UNUSED(pgmo)
  GAMS_UNUSED(rows)
  GAMS_UNUSED(cols)
  GAMS_UNUSED(codelen)
  printAndReturn(gmoInitData,3,int )
}

/** Add a row
 * @param pgmo gmo object handle
 * @param etyp Type of equation (see enumerated constants)
 * @param ematch Index of matching variable of equation
 * @param eslack Slack of equation
 * @param escale Scale of equation
 * @param erhs RHS of equation
 * @param emarg Marginal of equation
 * @param ebas Basis flag of equation (0=basic)
 * @param enz Number of nonzeros in row
 * @param colidx Column index/indices of Jacobian(s)
 * @param jacval Value(s) of Jacobian(s)
 * @param nlflag NL flag(s) of Jacobian(s) (0 : linear,!=0 : nonlinear)
 */
int  GMO_CALLCONV d_gmoAddRow (gmoHandle_t pgmo, int etyp, int ematch, double eslack, double escale, double erhs, double emarg, int ebas, int enz, const int colidx[], const double jacval[], const int nlflag[])
{
  int d_s[]={3,3,3,13,13,13,13,3,3,7,5,7};
  GAMS_UNUSED(pgmo)
  GAMS_UNUSED(etyp)
  GAMS_UNUSED(ematch)
  GAMS_UNUSED(eslack)
  GAMS_UNUSED(escale)
  GAMS_UNUSED(erhs)
  GAMS_UNUSED(emarg)
  GAMS_UNUSED(ebas)
  GAMS_UNUSED(enz)
  GAMS_UNUSED(colidx)
  GAMS_UNUSED(jacval)
  GAMS_UNUSED(nlflag)
  printAndReturn(gmoAddRow,11,int )
}

/** Add a column
 * @param pgmo gmo object handle
 * @param vtyp Type of variable (see enumerated constants)
 * @param vlo Lower bound of variable
 * @param vl Level of variable
 * @param vup Upper bound of variable
 * @param vmarg Marginal of variable
 * @param vbas Basis flag of variable (0=basic)
 * @param vsos SOS set variable belongs to
 * @param vprior riority value of variable
 * @param vscale Scale of variable
 * @param vnz Number of nonzeros in column
 * @param rowidx Row index/indices of Jacobian
 * @param jacval Value(s) of Jacobian(s)
 * @param nlflag NL flag(s) of Jacobian(s) (0 : linear,!=0 : nonlinear)
 */
int  GMO_CALLCONV d_gmoAddCol (gmoHandle_t pgmo, int vtyp, double vlo, double vl, double vup, double vmarg, int vbas, int vsos, double vprior, double vscale, int vnz, const int rowidx[], const double jacval[], const int nlflag[])
{
  int d_s[]={3,3,13,13,13,13,3,3,13,13,3,7,5,7};
  GAMS_UNUSED(pgmo)
  GAMS_UNUSED(vtyp)
  GAMS_UNUSED(vlo)
  GAMS_UNUSED(vl)
  GAMS_UNUSED(vup)
  GAMS_UNUSED(vmarg)
  GAMS_UNUSED(vbas)
  GAMS_UNUSED(vsos)
  GAMS_UNUSED(vprior)
  GAMS_UNUSED(vscale)
  GAMS_UNUSED(vnz)
  GAMS_UNUSED(rowidx)
  GAMS_UNUSED(jacval)
  GAMS_UNUSED(nlflag)
  printAndReturn(gmoAddCol,13,int )
}

/** Complete GMO data instance
 * @param pgmo gmo object handle
 * @param msg Message
 */
int  GMO_CALLCONV d_gmoCompleteData (gmoHandle_t pgmo, char *msg)
{
  int d_s[]={3,12};
  GAMS_UNUSED(pgmo)
  GAMS_UNUSED(msg)
  printAndReturn(gmoCompleteData,1,int )
}

/** Complete matching information for MCP
 * @param pgmo gmo object handle
 * @param msg Message
 */
int  GMO_CALLCONV d_gmoFillMatches (gmoHandle_t pgmo, char *msg)
{
  int d_s[]={3,12};
  GAMS_UNUSED(pgmo)
  GAMS_UNUSED(msg)
  printAndReturn(gmoFillMatches,1,int )
}

/** Read instance from scratch files - Legacy Mode - without gmoFillMatches
 * @param pgmo gmo object handle
 * @param msg Message
 */
int  GMO_CALLCONV d_gmoLoadDataLegacy (gmoHandle_t pgmo, char *msg)
{
  int d_s[]={3,12};
  GAMS_UNUSED(pgmo)
  GAMS_UNUSED(msg)
  printAndReturn(gmoLoadDataLegacy,1,int )
}

/** Read instance from scratch files - Legacy Mode
 * @param pgmo gmo object handle
 * @param fillMatches controls gmoFillMatches call during the load
 * @param msg Message
 */
int  GMO_CALLCONV d_gmoLoadDataLegacyEx (gmoHandle_t pgmo, int fillMatches, char *msg)
{
  int d_s[]={3,15,12};
  GAMS_UNUSED(pgmo)
  GAMS_UNUSED(fillMatches)
  GAMS_UNUSED(msg)
  printAndReturn(gmoLoadDataLegacyEx,2,int )
}

/** Register GAMS environment
 * @param pgmo gmo object handle
 * @param gevptr 
 * @param msg Message
 */
int  GMO_CALLCONV d_gmoRegisterEnvironment (gmoHandle_t pgmo, void *gevptr, char *msg)
{
  int d_s[]={3,1,12};
  GAMS_UNUSED(pgmo)
  GAMS_UNUSED(gevptr)
  GAMS_UNUSED(msg)
  printAndReturn(gmoRegisterEnvironment,2,int )
}

/** Get GAMS environment object pointer
 * @param pgmo gmo object handle
 */
void * GMO_CALLCONV d_gmoEnvironment (gmoHandle_t pgmo)
{
  int d_s[]={1};
  GAMS_UNUSED(pgmo)
  printAndReturn(gmoEnvironment,0,void *)
}

/** Store current view in view object
 * @param pgmo gmo object handle
 */
void * GMO_CALLCONV d_gmoViewStore (gmoHandle_t pgmo)
{
  int d_s[]={1};
  GAMS_UNUSED(pgmo)
  printAndReturn(gmoViewStore,0,void *)
}

/** Restore view
 * @param pgmo gmo object handle
 * @param viewptr Pointer to structure storing the view of a model
 */
void  GMO_CALLCONV d_gmoViewRestore (gmoHandle_t pgmo, void **viewptr)
{
  int d_s[]={0,2};
  GAMS_UNUSED(pgmo)
  GAMS_UNUSED(viewptr)
  printNoReturn(gmoViewRestore,1)
}

/** Dump current view to stdout
 * @param pgmo gmo object handle
 */
void  GMO_CALLCONV d_gmoViewDump (gmoHandle_t pgmo)
{
  int d_s[]={0};
  GAMS_UNUSED(pgmo)
  printNoReturn(gmoViewDump,0)
}

/** Get equation index in solver space
 * @param pgmo gmo object handle
 * @param mi Index of row in original/GAMS space
 */
int  GMO_CALLCONV d_gmoGetiSolver (gmoHandle_t pgmo, int mi)
{
  int d_s[]={3,3};
  GAMS_UNUSED(pgmo)
  GAMS_UNUSED(mi)
  printAndReturn(gmoGetiSolver,1,int )
}

/** Get variable index in solver space
 * @param pgmo gmo object handle
 * @param mj Index of column original/GAMS client space
 */
int  GMO_CALLCONV d_gmoGetjSolver (gmoHandle_t pgmo, int mj)
{
  int d_s[]={3,3};
  GAMS_UNUSED(pgmo)
  GAMS_UNUSED(mj)
  printAndReturn(gmoGetjSolver,1,int )
}

/** Get equation index in solver space (without error message; negative if it fails)
 * @param pgmo gmo object handle
 * @param mi Index of row in original/GAMS space
 */
int  GMO_CALLCONV d_gmoGetiSolverQuiet (gmoHandle_t pgmo, int mi)
{
  int d_s[]={3,3};
  GAMS_UNUSED(pgmo)
  GAMS_UNUSED(mi)
  printAndReturn(gmoGetiSolverQuiet,1,int )
}

/** Get variable index in solver space (without error message; negative if it fails)
 * @param pgmo gmo object handle
 * @param mj Index of column original/GAMS client space
 */
int  GMO_CALLCONV d_gmoGetjSolverQuiet (gmoHandle_t pgmo, int mj)
{
  int d_s[]={3,3};
  GAMS_UNUSED(pgmo)
  GAMS_UNUSED(mj)
  printAndReturn(gmoGetjSolverQuiet,1,int )
}

/** Get equation index in model (original) space
 * @param pgmo gmo object handle
 * @param si Index of row in client space
 */
int  GMO_CALLCONV d_gmoGetiModel (gmoHandle_t pgmo, int si)
{
  int d_s[]={3,3};
  GAMS_UNUSED(pgmo)
  GAMS_UNUSED(si)
  printAndReturn(gmoGetiModel,1,int )
}

/** Get variable index in model (original) space
 * @param pgmo gmo object handle
 * @param sj Index of column in client space
 */
int  GMO_CALLCONV d_gmoGetjModel (gmoHandle_t pgmo, int sj)
{
  int d_s[]={3,3};
  GAMS_UNUSED(pgmo)
  GAMS_UNUSED(sj)
  printAndReturn(gmoGetjModel,1,int )
}

/** Set Permutation vectors for equations (model view)
 * @param pgmo gmo object handle
 * @param permut Permutation vector (original/GAMS to client)
 */
int  GMO_CALLCONV d_gmoSetEquPermutation (gmoHandle_t pgmo, int permut[])
{
  int d_s[]={3,8};
  GAMS_UNUSED(pgmo)
  GAMS_UNUSED(permut)
  printAndReturn(gmoSetEquPermutation,1,int )
}

/** Set Permutation vectors for equations (solver view)
 * @param pgmo gmo object handle
 * @param rvpermut Reverse permutation vector (client to original/GAMS)
 * @param len Length of array
 */
int  GMO_CALLCONV d_gmoSetRvEquPermutation (gmoHandle_t pgmo, int rvpermut[], int len)
{
  int d_s[]={3,8,3};
  GAMS_UNUSED(pgmo)
  GAMS_UNUSED(rvpermut)
  GAMS_UNUSED(len)
  printAndReturn(gmoSetRvEquPermutation,2,int )
}

/** Set Permutation vectors for variables (model view)
 * @param pgmo gmo object handle
 * @param permut Permutation vector (original/GAMS to client)
 */
int  GMO_CALLCONV d_gmoSetVarPermutation (gmoHandle_t pgmo, int permut[])
{
  int d_s[]={3,8};
  GAMS_UNUSED(pgmo)
  GAMS_UNUSED(permut)
  printAndReturn(gmoSetVarPermutation,1,int )
}

/** Set Permutation vectors for variables (solver view)
 * @param pgmo gmo object handle
 * @param rvpermut Reverse permutation vector (client to original/GAMS)
 * @param len Length of array
 */
int  GMO_CALLCONV d_gmoSetRvVarPermutation (gmoHandle_t pgmo, int rvpermut[], int len)
{
  int d_s[]={3,8,3};
  GAMS_UNUSED(pgmo)
  GAMS_UNUSED(rvpermut)
  GAMS_UNUSED(len)
  printAndReturn(gmoSetRvVarPermutation,2,int )
}

/** Set Permutation to skip =n= rows
 * @param pgmo gmo object handle
 */
int  GMO_CALLCONV d_gmoSetNRowPerm (gmoHandle_t pgmo)
{
  int d_s[]={3};
  GAMS_UNUSED(pgmo)
  printAndReturn(gmoSetNRowPerm,0,int )
}

/** Get variable type count
 * @param pgmo gmo object handle
 * @param vtyp Type of variable (see enumerated constants)
 */
int  GMO_CALLCONV d_gmoGetVarTypeCnt (gmoHandle_t pgmo, int vtyp)
{
  int d_s[]={3,3};
  GAMS_UNUSED(pgmo)
  GAMS_UNUSED(vtyp)
  printAndReturn(gmoGetVarTypeCnt,1,int )
}

/** Get equation type count
 * @param pgmo gmo object handle
 * @param etyp Type of equation (see enumerated constants)
 */
int  GMO_CALLCONV d_gmoGetEquTypeCnt (gmoHandle_t pgmo, int etyp)
{
  int d_s[]={3,3};
  GAMS_UNUSED(pgmo)
  GAMS_UNUSED(etyp)
  printAndReturn(gmoGetEquTypeCnt,1,int )
}

/** Get obj counts
 * @param pgmo gmo object handle
 * @param nz Number of nonzeros
 * @param qnz Number of quadratic nonzeros in Jacobian matrix
 * @param nlnz Number of nonlinear nonzeros
 */
int  GMO_CALLCONV d_gmoGetObjStat (gmoHandle_t pgmo, int *nz, int *qnz, int *nlnz)
{
  int d_s[]={3,4,4,4};
  GAMS_UNUSED(pgmo)
  GAMS_UNUSED(nz)
  GAMS_UNUSED(qnz)
  GAMS_UNUSED(nlnz)
  printAndReturn(gmoGetObjStat,3,int )
}

/** Get row counts
 * @param pgmo gmo object handle
 * @param si Index of row in client space
 * @param nz Number of nonzeros
 * @param qnz Number of quadratic nonzeros in Jacobian matrix
 * @param nlnz Number of nonlinear nonzeros
 */
int  GMO_CALLCONV d_gmoGetRowStat (gmoHandle_t pgmo, int si, int *nz, int *qnz, int *nlnz)
{
  int d_s[]={3,3,4,4,4};
  GAMS_UNUSED(pgmo)
  GAMS_UNUSED(si)
  GAMS_UNUSED(nz)
  GAMS_UNUSED(qnz)
  GAMS_UNUSED(nlnz)
  printAndReturn(gmoGetRowStat,4,int )
}

/** Get Jacobian row NZ counts: total and by GMOORDER_XX
 * @param pgmo gmo object handle
 * @param si Index of row in client space
 * @param nz Number of nonzeros
 * @param lnz 
 * @param qnz Number of quadratic nonzeros in Jacobian matrix
 * @param nlnz Number of nonlinear nonzeros
 */
int  GMO_CALLCONV d_gmoGetRowStatEx (gmoHandle_t pgmo, int si, int *nz, int *lnz, int *qnz, int *nlnz)
{
  int d_s[]={3,3,4,4,4,4};
  GAMS_UNUSED(pgmo)
  GAMS_UNUSED(si)
  GAMS_UNUSED(nz)
  GAMS_UNUSED(lnz)
  GAMS_UNUSED(qnz)
  GAMS_UNUSED(nlnz)
  printAndReturn(gmoGetRowStatEx,5,int )
}

/** Get column counts objnz = -1 if linear +1 if non-linear 0 otherwise
 * @param pgmo gmo object handle
 * @param sj Index of column in client space
 * @param nz Number of nonzeros
 * @param qnz Number of quadratic nonzeros in Jacobian matrix
 * @param nlnz Number of nonlinear nonzeros
 * @param objnz Nonzeros in objective
 */
int  GMO_CALLCONV d_gmoGetColStat (gmoHandle_t pgmo, int sj, int *nz, int *qnz, int *nlnz, int *objnz)
{
  int d_s[]={3,3,4,4,4,4};
  GAMS_UNUSED(pgmo)
  GAMS_UNUSED(sj)
  GAMS_UNUSED(nz)
  GAMS_UNUSED(qnz)
  GAMS_UNUSED(nlnz)
  GAMS_UNUSED(objnz)
  printAndReturn(gmoGetColStat,5,int )
}

/** Number of NZ in Q matrix of row si (-1 if Q information not used or overflow)
 * @param pgmo gmo object handle
 * @param si Index of row in client space
 */
int  GMO_CALLCONV d_gmoGetRowQNZOne (gmoHandle_t pgmo, int si)
{
  int d_s[]={3,3};
  GAMS_UNUSED(pgmo)
  GAMS_UNUSED(si)
  printAndReturn(gmoGetRowQNZOne,1,int )
}

/** Number of NZ in Q matrix of row si (-1 if Q information not used)
 * @param pgmo gmo object handle
 * @param si Index of row in client space
 */
INT64  GMO_CALLCONV d_gmoGetRowQNZOne64 (gmoHandle_t pgmo, int si)
{
  int d_s[]={23,3};
  GAMS_UNUSED(pgmo)
  GAMS_UNUSED(si)
  printAndReturn(gmoGetRowQNZOne64,1,INT64 )
}

/** Number of NZ on diagonal of Q matrix of row si (-1 if Q information not used)
 * @param pgmo gmo object handle
 * @param si Index of row in client space
 */
int  GMO_CALLCONV d_gmoGetRowQDiagNZOne (gmoHandle_t pgmo, int si)
{
  int d_s[]={3,3};
  GAMS_UNUSED(pgmo)
  GAMS_UNUSED(si)
  printAndReturn(gmoGetRowQDiagNZOne,1,int )
}

/** Number of NZ in c vector of row si (-1 if Q information not used)
 * @param pgmo gmo object handle
 * @param si Index of row in client space
 */
int  GMO_CALLCONV d_gmoGetRowCVecNZOne (gmoHandle_t pgmo, int si)
{
  int d_s[]={3,3};
  GAMS_UNUSED(pgmo)
  GAMS_UNUSED(si)
  printAndReturn(gmoGetRowCVecNZOne,1,int )
}

/** Get SOS count information
 * @param pgmo gmo object handle
 * @param numsos1 Number of SOS1 sets
 * @param numsos2 Number of SOS2 sets
 * @param nzsos Number of variables in SOS1/2 sets
 */
void  GMO_CALLCONV d_gmoGetSosCounts (gmoHandle_t pgmo, int *numsos1, int *numsos2, int *nzsos)
{
  int d_s[]={0,4,4,4};
  GAMS_UNUSED(pgmo)
  GAMS_UNUSED(numsos1)
  GAMS_UNUSED(numsos2)
  GAMS_UNUSED(nzsos)
  printNoReturn(gmoGetSosCounts,3)
}

/** Get external function information
 * @param pgmo gmo object handle
 * @param rows Number of rows
 * @param cols Number of columns
 * @param nz Number of nonzeros
 * @param orgcolind 
 */
void  GMO_CALLCONV d_gmoGetXLibCounts (gmoHandle_t pgmo, int *rows, int *cols, int *nz, int orgcolind[])
{
  int d_s[]={0,4,4,4,8};
  GAMS_UNUSED(pgmo)
  GAMS_UNUSED(rows)
  GAMS_UNUSED(cols)
  GAMS_UNUSED(nz)
  GAMS_UNUSED(orgcolind)
  printNoReturn(gmoGetXLibCounts,4)
}

/** Get model type in case of scenario solve generated models
 * @param pgmo gmo object handle
 * @param checkv a vector with column indicators to be treated as constant
 * @param actModelType active model type in case of scenario dict type emp model
 */
int  GMO_CALLCONV d_gmoGetActiveModelType (gmoHandle_t pgmo, int checkv[], int *actModelType)
{
  int d_s[]={3,8,4};
  GAMS_UNUSED(pgmo)
  GAMS_UNUSED(checkv)
  GAMS_UNUSED(actModelType)
  printAndReturn(gmoGetActiveModelType,2,int )
}

/** Get constraint matrix in row order with row start only and NL indicator
 * @param pgmo gmo object handle
 * @param rowstart Index of Jacobian row starts with
 * @param colidx Column index/indices of Jacobian(s)
 * @param jacval Value(s) of Jacobian(s)
 * @param nlflag NL flag(s) of Jacobian(s) (0 : linear,!=0 : nonlinear)
 */
int  GMO_CALLCONV d_gmoGetMatrixRow (gmoHandle_t pgmo, int rowstart[], int colidx[], double jacval[], int nlflag[])
{
  int d_s[]={3,8,8,6,8};
  GAMS_UNUSED(pgmo)
  GAMS_UNUSED(rowstart)
  GAMS_UNUSED(colidx)
  GAMS_UNUSED(jacval)
  GAMS_UNUSED(nlflag)
  printAndReturn(gmoGetMatrixRow,4,int )
}

/** Get constraint matrix in column order with columns start only and NL indicator
 * @param pgmo gmo object handle
 * @param colstart Index of Jacobian column starts with
 * @param rowidx Row index/indices of Jacobian
 * @param jacval Value(s) of Jacobian(s)
 * @param nlflag NL flag(s) of Jacobian(s) (0 : linear,!=0 : nonlinear)
 */
int  GMO_CALLCONV d_gmoGetMatrixCol (gmoHandle_t pgmo, int colstart[], int rowidx[], double jacval[], int nlflag[])
{
  int d_s[]={3,8,8,6,8};
  GAMS_UNUSED(pgmo)
  GAMS_UNUSED(colstart)
  GAMS_UNUSED(rowidx)
  GAMS_UNUSED(jacval)
  GAMS_UNUSED(nlflag)
  printAndReturn(gmoGetMatrixCol,4,int )
}

/** Get constraint matrix in column order with column start and end (colstart length is n+1)
 * @param pgmo gmo object handle
 * @param colstart Index of Jacobian column starts with
 * @param collength Number of Jacobians in column
 * @param rowidx Row index/indices of Jacobian
 * @param jacval Value(s) of Jacobian(s)
 */
int  GMO_CALLCONV d_gmoGetMatrixCplex (gmoHandle_t pgmo, int colstart[], int collength[], int rowidx[], double jacval[])
{
  int d_s[]={3,8,8,8,6};
  GAMS_UNUSED(pgmo)
  GAMS_UNUSED(colstart)
  GAMS_UNUSED(collength)
  GAMS_UNUSED(rowidx)
  GAMS_UNUSED(jacval)
  printAndReturn(gmoGetMatrixCplex,4,int )
}

/** Get name of objective
 * @param pgmo gmo object handle
 */
char * GMO_CALLCONV d_gmoGetObjName (gmoHandle_t pgmo, char *buf)
{
  int d_s[]={12};
  GAMS_UNUSED(pgmo)
  GAMS_UNUSED(buf)
  printAndReturn(gmoGetObjName,0,char *)
}

/** Get name of objective with user specified suffix
 * @param pgmo gmo object handle
 * @param suffix Suffix appended to name, could be .l, .m etc.
 */
char * GMO_CALLCONV d_gmoGetObjNameCustom (gmoHandle_t pgmo, const char *suffix, char *buf)
{
  int d_s[]={12,11};
  GAMS_UNUSED(pgmo)
  GAMS_UNUSED(suffix)
  GAMS_UNUSED(buf)
  printAndReturn(gmoGetObjNameCustom,1,char *)
}

/** Get objective function vector (dense)
 * @param pgmo gmo object handle
 * @param jacval Value(s) of Jacobian(s)
 * @param nlflag NL flag(s) of Jacobian(s) (0 : linear,!=0 : nonlinear)
 */
int  GMO_CALLCONV d_gmoGetObjVector (gmoHandle_t pgmo, double jacval[], int nlflag[])
{
  int d_s[]={3,6,8};
  GAMS_UNUSED(pgmo)
  GAMS_UNUSED(jacval)
  GAMS_UNUSED(nlflag)
  printAndReturn(gmoGetObjVector,2,int )
}

/** Get Jacobians information of objective function (sparse)
 * @param pgmo gmo object handle
 * @param colidx Column index/indices of Jacobian(s)
 * @param jacval Value(s) of Jacobian(s)
 * @param nlflag NL flag(s) of Jacobian(s) (0 : linear,!=0 : nonlinear)
 * @param nz Number of nonzeros
 * @param nlnz Number of nonlinear nonzeros
 */
int  GMO_CALLCONV d_gmoGetObjSparse (gmoHandle_t pgmo, int colidx[], double jacval[], int nlflag[], int *nz, int *nlnz)
{
  int d_s[]={3,8,6,8,4,4};
  GAMS_UNUSED(pgmo)
  GAMS_UNUSED(colidx)
  GAMS_UNUSED(jacval)
  GAMS_UNUSED(nlflag)
  GAMS_UNUSED(nz)
  GAMS_UNUSED(nlnz)
  printAndReturn(gmoGetObjSparse,5,int )
}

/** Get information for gradient of objective function (sparse)
 * @param pgmo gmo object handle
 * @param colidx Column index/indices of Jacobian(s)
 * @param gradval 
 * @param nlflag NL flag(s) of Jacobian(s) (0 : linear,!=0 : nonlinear)
 * @param nz Number of nonzeros
 * @param qnz Number of quadratic nonzeros in Jacobian matrix
 * @param nlnz Number of nonlinear nonzeros
 */
int  GMO_CALLCONV d_gmoGetObjSparseEx (gmoHandle_t pgmo, int colidx[], double gradval[], int nlflag[], int *nz, int *qnz, int *nlnz)
{
  int d_s[]={3,8,6,8,4,4,4};
  GAMS_UNUSED(pgmo)
  GAMS_UNUSED(colidx)
  GAMS_UNUSED(gradval)
  GAMS_UNUSED(nlflag)
  GAMS_UNUSED(nz)
  GAMS_UNUSED(qnz)
  GAMS_UNUSED(nlnz)
  printAndReturn(gmoGetObjSparseEx,6,int )
}

/** Get lower triangle of Q matrix of objective
 * @param pgmo gmo object handle
 * @param varidx1 First variable indices
 * @param varidx2 Second variable indices
 * @param coefs Coefficients
 */
int  GMO_CALLCONV d_gmoGetObjQMat (gmoHandle_t pgmo, int varidx1[], int varidx2[], double coefs[])
{
  int d_s[]={3,8,8,6};
  GAMS_UNUSED(pgmo)
  GAMS_UNUSED(varidx1)
  GAMS_UNUSED(varidx2)
  GAMS_UNUSED(coefs)
  printAndReturn(gmoGetObjQMat,3,int )
}

/** deprecated synonym for gmoGetObjQMat
 * @param pgmo gmo object handle
 * @param varidx1 First variable indices
 * @param varidx2 Second variable indices
 * @param coefs Coefficients
 */
int  GMO_CALLCONV d_gmoGetObjQ (gmoHandle_t pgmo, int varidx1[], int varidx2[], double coefs[])
{
  int d_s[]={3,8,8,6};
  GAMS_UNUSED(pgmo)
  GAMS_UNUSED(varidx1)
  GAMS_UNUSED(varidx2)
  GAMS_UNUSED(coefs)
  printAndReturn(gmoGetObjQ,3,int )
}

/** Get c vector of quadratic objective
 * @param pgmo gmo object handle
 * @param varidx 
 * @param coefs Coefficients
 */
int  GMO_CALLCONV d_gmoGetObjCVec (gmoHandle_t pgmo, int varidx[], double coefs[])
{
  int d_s[]={3,8,6};
  GAMS_UNUSED(pgmo)
  GAMS_UNUSED(varidx)
  GAMS_UNUSED(coefs)
  printAndReturn(gmoGetObjCVec,2,int )
}

/** Get objective activity level
 * @param pgmo gmo object handle
 */
double  GMO_CALLCONV d_gmoGetObjL (gmoHandle_t pgmo)
{
  int d_s[]={13};
  GAMS_UNUSED(pgmo)
  printAndReturn(gmoGetObjL,0,double )
}

/** Get equation activity levels
 * @param pgmo gmo object handle
 * @param e Level values of equations
 */
int  GMO_CALLCONV d_gmoGetEquL (gmoHandle_t pgmo, double e[])
{
  int d_s[]={3,6};
  GAMS_UNUSED(pgmo)
  GAMS_UNUSED(e)
  printAndReturn(gmoGetEquL,1,int )
}

/** Get individual equation activity levels
 * @param pgmo gmo object handle
 * @param si Index of row in client space
 */
double  GMO_CALLCONV d_gmoGetEquLOne (gmoHandle_t pgmo, int si)
{
  int d_s[]={13,3};
  GAMS_UNUSED(pgmo)
  GAMS_UNUSED(si)
  printAndReturn(gmoGetEquLOne,1,double )
}

/** Set equation activity levels
 * @param pgmo gmo object handle
 * @param el Level of equation
 */
int  GMO_CALLCONV d_gmoSetEquL (gmoHandle_t pgmo, const double el[])
{
  int d_s[]={3,5};
  GAMS_UNUSED(pgmo)
  GAMS_UNUSED(el)
  printAndReturn(gmoSetEquL,1,int )
}

/** Set individual equation activity levels
 * @param pgmo gmo object handle
 * @param si Index of row in client space
 * @param el Level of equation
 */
void  GMO_CALLCONV d_gmoSetEquLOne (gmoHandle_t pgmo, int si, double el)
{
  int d_s[]={0,3,13};
  GAMS_UNUSED(pgmo)
  GAMS_UNUSED(si)
  GAMS_UNUSED(el)
  printNoReturn(gmoSetEquLOne,2)
}

/** Get equation marginals
 * @param pgmo gmo object handle
 * @param pi Marginal values of equations
 */
int  GMO_CALLCONV d_gmoGetEquM (gmoHandle_t pgmo, double pi[])
{
  int d_s[]={3,6};
  GAMS_UNUSED(pgmo)
  GAMS_UNUSED(pi)
  printAndReturn(gmoGetEquM,1,int )
}

/** Get individual equation marginal
 * @param pgmo gmo object handle
 * @param si Index of row in client space
 */
double  GMO_CALLCONV d_gmoGetEquMOne (gmoHandle_t pgmo, int si)
{
  int d_s[]={13,3};
  GAMS_UNUSED(pgmo)
  GAMS_UNUSED(si)
  printAndReturn(gmoGetEquMOne,1,double )
}

/** Set equation marginals (pass NULL to set to NA)
 * @param pgmo gmo object handle
 * @param emarg Marginal of equation
 */
int  GMO_CALLCONV d_gmoSetEquM (gmoHandle_t pgmo, const double emarg[])
{
  int d_s[]={3,5};
  GAMS_UNUSED(pgmo)
  GAMS_UNUSED(emarg)
  printAndReturn(gmoSetEquM,1,int )
}

/** Get individual equation name
 * @param pgmo gmo object handle
 * @param si Index of row in client space
 */
char * GMO_CALLCONV d_gmoGetEquNameOne (gmoHandle_t pgmo, int si, char *buf)
{
  int d_s[]={12,3};
  GAMS_UNUSED(pgmo)
  GAMS_UNUSED(si)
  GAMS_UNUSED(buf)
  printAndReturn(gmoGetEquNameOne,1,char *)
}

/** Get individual equation name with quotes and user specified suffix
 * @param pgmo gmo object handle
 * @param si Index of row in client space
 * @param suffix Suffix appended to name, could be .l, .m etc.
 */
char * GMO_CALLCONV d_gmoGetEquNameCustomOne (gmoHandle_t pgmo, int si, const char *suffix, char *buf)
{
  int d_s[]={12,3,11};
  GAMS_UNUSED(pgmo)
  GAMS_UNUSED(si)
  GAMS_UNUSED(suffix)
  GAMS_UNUSED(buf)
  printAndReturn(gmoGetEquNameCustomOne,2,char *)
}

/** Get right hand sides
 * @param pgmo gmo object handle
 * @param mdblvec Array of doubles, len=number of rows in user view
 */
int  GMO_CALLCONV d_gmoGetRhs (gmoHandle_t pgmo, double mdblvec[])
{
  int d_s[]={3,6};
  GAMS_UNUSED(pgmo)
  GAMS_UNUSED(mdblvec)
  printAndReturn(gmoGetRhs,1,int )
}

/** Get individual equation right hand side
 * @param pgmo gmo object handle
 * @param si Index of row in client space
 */
double  GMO_CALLCONV d_gmoGetRhsOne (gmoHandle_t pgmo, int si)
{
  int d_s[]={13,3};
  GAMS_UNUSED(pgmo)
  GAMS_UNUSED(si)
  printAndReturn(gmoGetRhsOne,1,double )
}

/** Get individual equation RHS - independent of useQ'
 * @param pgmo gmo object handle
 * @param si Index of row in client space
 */
double  GMO_CALLCONV d_gmoGetRhsOneEx (gmoHandle_t pgmo, int si)
{
  int d_s[]={13,3};
  GAMS_UNUSED(pgmo)
  GAMS_UNUSED(si)
  printAndReturn(gmoGetRhsOneEx,1,double )
}

/** Set alternative RHS
 * @param pgmo gmo object handle
 * @param mdblvec Array of doubles, len=number of rows in user view
 */
int  GMO_CALLCONV d_gmoSetAltRHS (gmoHandle_t pgmo, const double mdblvec[])
{
  int d_s[]={3,5};
  GAMS_UNUSED(pgmo)
  GAMS_UNUSED(mdblvec)
  printAndReturn(gmoSetAltRHS,1,int )
}

/** Set individual alternative RHS
 * @param pgmo gmo object handle
 * @param si Index of row in client space
 * @param erhs RHS of equation
 */
void  GMO_CALLCONV d_gmoSetAltRHSOne (gmoHandle_t pgmo, int si, double erhs)
{
  int d_s[]={0,3,13};
  GAMS_UNUSED(pgmo)
  GAMS_UNUSED(si)
  GAMS_UNUSED(erhs)
  printNoReturn(gmoSetAltRHSOne,2)
}

/** Get equation slacks
 * @param pgmo gmo object handle
 * @param mdblvec Array of doubles, len=number of rows in user view
 */
int  GMO_CALLCONV d_gmoGetEquSlack (gmoHandle_t pgmo, double mdblvec[])
{
  int d_s[]={3,6};
  GAMS_UNUSED(pgmo)
  GAMS_UNUSED(mdblvec)
  printAndReturn(gmoGetEquSlack,1,int )
}

/** Get individual equation slack
 * @param pgmo gmo object handle
 * @param si Index of row in client space
 */
double  GMO_CALLCONV d_gmoGetEquSlackOne (gmoHandle_t pgmo, int si)
{
  int d_s[]={13,3};
  GAMS_UNUSED(pgmo)
  GAMS_UNUSED(si)
  printAndReturn(gmoGetEquSlackOne,1,double )
}

/** Set equation slacks
 * @param pgmo gmo object handle
 * @param mdblvec Array of doubles, len=number of rows in user view
 */
int  GMO_CALLCONV d_gmoSetEquSlack (gmoHandle_t pgmo, const double mdblvec[])
{
  int d_s[]={3,5};
  GAMS_UNUSED(pgmo)
  GAMS_UNUSED(mdblvec)
  printAndReturn(gmoSetEquSlack,1,int )
}

/** Get equation type
 * @param pgmo gmo object handle
 * @param mintvec Array of integers, len=number of rows in user view
 */
int  GMO_CALLCONV d_gmoGetEquType (gmoHandle_t pgmo, int mintvec[])
{
  int d_s[]={3,8};
  GAMS_UNUSED(pgmo)
  GAMS_UNUSED(mintvec)
  printAndReturn(gmoGetEquType,1,int )
}

/** Get individual equation type
 * @param pgmo gmo object handle
 * @param si Index of row in client space
 */
int  GMO_CALLCONV d_gmoGetEquTypeOne (gmoHandle_t pgmo, int si)
{
  int d_s[]={3,3};
  GAMS_UNUSED(pgmo)
  GAMS_UNUSED(si)
  printAndReturn(gmoGetEquTypeOne,1,int )
}

/** Get equation basis status
 * @param pgmo gmo object handle
 * @param mintvec Array of integers, len=number of rows in user view
 */
void  GMO_CALLCONV d_gmoGetEquStat (gmoHandle_t pgmo, int mintvec[])
{
  int d_s[]={0,8};
  GAMS_UNUSED(pgmo)
  GAMS_UNUSED(mintvec)
  printNoReturn(gmoGetEquStat,1)
}

/** Get individual basis equation status
 * @param pgmo gmo object handle
 * @param si Index of row in client space
 */
int  GMO_CALLCONV d_gmoGetEquStatOne (gmoHandle_t pgmo, int si)
{
  int d_s[]={3,3};
  GAMS_UNUSED(pgmo)
  GAMS_UNUSED(si)
  printAndReturn(gmoGetEquStatOne,1,int )
}

/** Set equation basis status
 * @param pgmo gmo object handle
 * @param mintvec Array of integers, len=number of rows in user view
 */
void  GMO_CALLCONV d_gmoSetEquStat (gmoHandle_t pgmo, const int mintvec[])
{
  int d_s[]={0,7};
  GAMS_UNUSED(pgmo)
  GAMS_UNUSED(mintvec)
  printNoReturn(gmoSetEquStat,1)
}

/** Get equation status
 * @param pgmo gmo object handle
 * @param mintvec Array of integers, len=number of rows in user view
 */
void  GMO_CALLCONV d_gmoGetEquCStat (gmoHandle_t pgmo, int mintvec[])
{
  int d_s[]={0,8};
  GAMS_UNUSED(pgmo)
  GAMS_UNUSED(mintvec)
  printNoReturn(gmoGetEquCStat,1)
}

/** Get individual equation status
 * @param pgmo gmo object handle
 * @param si Index of row in client space
 */
int  GMO_CALLCONV d_gmoGetEquCStatOne (gmoHandle_t pgmo, int si)
{
  int d_s[]={3,3};
  GAMS_UNUSED(pgmo)
  GAMS_UNUSED(si)
  printAndReturn(gmoGetEquCStatOne,1,int )
}

/** Set equation status
 * @param pgmo gmo object handle
 * @param mintvec Array of integers, len=number of rows in user view
 */
void  GMO_CALLCONV d_gmoSetEquCStat (gmoHandle_t pgmo, const int mintvec[])
{
  int d_s[]={0,7};
  GAMS_UNUSED(pgmo)
  GAMS_UNUSED(mintvec)
  printNoReturn(gmoSetEquCStat,1)
}

/** Get equation match
 * @param pgmo gmo object handle
 * @param mintvec Array of integers, len=number of rows in user view
 */
int  GMO_CALLCONV d_gmoGetEquMatch (gmoHandle_t pgmo, int mintvec[])
{
  int d_s[]={3,8};
  GAMS_UNUSED(pgmo)
  GAMS_UNUSED(mintvec)
  printAndReturn(gmoGetEquMatch,1,int )
}

/** Get individual equation match
 * @param pgmo gmo object handle
 * @param si Index of row in client space
 */
int  GMO_CALLCONV d_gmoGetEquMatchOne (gmoHandle_t pgmo, int si)
{
  int d_s[]={3,3};
  GAMS_UNUSED(pgmo)
  GAMS_UNUSED(si)
  printAndReturn(gmoGetEquMatchOne,1,int )
}

/** Get equation scale
 * @param pgmo gmo object handle
 * @param mdblvec Array of doubles, len=number of rows in user view
 */
int  GMO_CALLCONV d_gmoGetEquScale (gmoHandle_t pgmo, double mdblvec[])
{
  int d_s[]={3,6};
  GAMS_UNUSED(pgmo)
  GAMS_UNUSED(mdblvec)
  printAndReturn(gmoGetEquScale,1,int )
}

/** Get individual equation scale
 * @param pgmo gmo object handle
 * @param si Index of row in client space
 */
double  GMO_CALLCONV d_gmoGetEquScaleOne (gmoHandle_t pgmo, int si)
{
  int d_s[]={13,3};
  GAMS_UNUSED(pgmo)
  GAMS_UNUSED(si)
  printAndReturn(gmoGetEquScaleOne,1,double )
}

/** Get equation stage
 * @param pgmo gmo object handle
 * @param mdblvec Array of doubles, len=number of rows in user view
 */
int  GMO_CALLCONV d_gmoGetEquStage (gmoHandle_t pgmo, double mdblvec[])
{
  int d_s[]={3,6};
  GAMS_UNUSED(pgmo)
  GAMS_UNUSED(mdblvec)
  printAndReturn(gmoGetEquStage,1,int )
}

/** Get individual equation stage
 * @param pgmo gmo object handle
 * @param si Index of row in client space
 */
double  GMO_CALLCONV d_gmoGetEquStageOne (gmoHandle_t pgmo, int si)
{
  int d_s[]={13,3};
  GAMS_UNUSED(pgmo)
  GAMS_UNUSED(si)
  printAndReturn(gmoGetEquStageOne,1,double )
}

/** Returns 0 on error, 1 linear, 2 quadratic, 3 nonlinear'
 * @param pgmo gmo object handle
 * @param si Index of row in client space
 */
int  GMO_CALLCONV d_gmoGetEquOrderOne (gmoHandle_t pgmo, int si)
{
  int d_s[]={3,3};
  GAMS_UNUSED(pgmo)
  GAMS_UNUSED(si)
  printAndReturn(gmoGetEquOrderOne,1,int )
}

/** Get Jacobians information of row (sparse)
 * @param pgmo gmo object handle
 * @param si Index of row in client space
 * @param colidx Column index/indices of Jacobian(s)
 * @param jacval Value(s) of Jacobian(s)
 * @param nlflag NL flag(s) of Jacobian(s) (0 : linear,!=0 : nonlinear)
 * @param nz Number of nonzeros
 * @param nlnz Number of nonlinear nonzeros
 */
int  GMO_CALLCONV d_gmoGetRowSparse (gmoHandle_t pgmo, int si, int colidx[], double jacval[], int nlflag[], int *nz, int *nlnz)
{
  int d_s[]={3,3,8,6,8,4,4};
  GAMS_UNUSED(pgmo)
  GAMS_UNUSED(si)
  GAMS_UNUSED(colidx)
  GAMS_UNUSED(jacval)
  GAMS_UNUSED(nlflag)
  GAMS_UNUSED(nz)
  GAMS_UNUSED(nlnz)
  printAndReturn(gmoGetRowSparse,6,int )
}

/** Get info for one row of Jacobian (sparse)
 * @param pgmo gmo object handle
 * @param si Index of row in client space
 * @param colidx Column index/indices of Jacobian(s)
 * @param jacval Value(s) of Jacobian(s)
 * @param nlflag NL flag(s) of Jacobian(s) (0 : linear,!=0 : nonlinear)
 * @param nz Number of nonzeros
 * @param qnz Number of quadratic nonzeros in Jacobian matrix
 * @param nlnz Number of nonlinear nonzeros
 */
int  GMO_CALLCONV d_gmoGetRowSparseEx (gmoHandle_t pgmo, int si, int colidx[], double jacval[], int nlflag[], int *nz, int *qnz, int *nlnz)
{
  int d_s[]={3,3,8,6,8,4,4,4};
  GAMS_UNUSED(pgmo)
  GAMS_UNUSED(si)
  GAMS_UNUSED(colidx)
  GAMS_UNUSED(jacval)
  GAMS_UNUSED(nlflag)
  GAMS_UNUSED(nz)
  GAMS_UNUSED(qnz)
  GAMS_UNUSED(nlnz)
  printAndReturn(gmoGetRowSparseEx,7,int )
}

/** Get Jacobian information of row one by one
 * @param pgmo gmo object handle
 * @param si Index of row in client space
 * @param jacptr Pointer to next Jacobian
 * @param jacval Value(s) of Jacobian(s)
 * @param colidx Column index/indices of Jacobian(s)
 * @param nlflag NL flag(s) of Jacobian(s) (0 : linear,!=0 : nonlinear)
 */
void  GMO_CALLCONV d_gmoGetRowJacInfoOne (gmoHandle_t pgmo, int si, void **jacptr, double *jacval, int *colidx, int *nlflag)
{
  int d_s[]={0,3,2,14,4,4};
  GAMS_UNUSED(pgmo)
  GAMS_UNUSED(si)
  GAMS_UNUSED(jacptr)
  GAMS_UNUSED(jacval)
  GAMS_UNUSED(colidx)
  GAMS_UNUSED(nlflag)
  printNoReturn(gmoGetRowJacInfoOne,5)
}

/** Get lower triangle of Q matrix of row si
 * @param pgmo gmo object handle
 * @param si Index of row in client space
 * @param varidx1 First variable indices
 * @param varidx2 Second variable indices
 * @param coefs Coefficients
 */
int  GMO_CALLCONV d_gmoGetRowQMat (gmoHandle_t pgmo, int si, int varidx1[], int varidx2[], double coefs[])
{
  int d_s[]={3,3,8,8,6};
  GAMS_UNUSED(pgmo)
  GAMS_UNUSED(si)
  GAMS_UNUSED(varidx1)
  GAMS_UNUSED(varidx2)
  GAMS_UNUSED(coefs)
  printAndReturn(gmoGetRowQMat,4,int )
}

/** deprecated synonym for gmoGetRowQMat
 * @param pgmo gmo object handle
 * @param si Index of row in client space
 * @param varidx1 First variable indices
 * @param varidx2 Second variable indices
 * @param coefs Coefficients
 */
int  GMO_CALLCONV d_gmoGetRowQ (gmoHandle_t pgmo, int si, int varidx1[], int varidx2[], double coefs[])
{
  int d_s[]={3,3,8,8,6};
  GAMS_UNUSED(pgmo)
  GAMS_UNUSED(si)
  GAMS_UNUSED(varidx1)
  GAMS_UNUSED(varidx2)
  GAMS_UNUSED(coefs)
  printAndReturn(gmoGetRowQ,4,int )
}

/** Get c vector of the quadratic form for row si
 * @param pgmo gmo object handle
 * @param si Index of row in client space
 * @param varidx 
 * @param coefs Coefficients
 */
int  GMO_CALLCONV d_gmoGetRowCVec (gmoHandle_t pgmo, int si, int varidx[], double coefs[])
{
  int d_s[]={3,3,8,6};
  GAMS_UNUSED(pgmo)
  GAMS_UNUSED(si)
  GAMS_UNUSED(varidx)
  GAMS_UNUSED(coefs)
  printAndReturn(gmoGetRowCVec,3,int )
}

/** Get the constant of the quadratic form for row si
 * @param pgmo gmo object handle
 * @param si Index of row in client space
 */
double  GMO_CALLCONV d_gmoGetRowQConst (gmoHandle_t pgmo, int si)
{
  int d_s[]={13,3};
  GAMS_UNUSED(pgmo)
  GAMS_UNUSED(si)
  printAndReturn(gmoGetRowQConst,1,double )
}

/** Get equation integer values for dot optio
 * @param pgmo gmo object handle
 * @param optptr Option object pointer
 * @param dotopt Dot option name
 * @param optvals Option values
 */
int  GMO_CALLCONV d_gmoGetEquIntDotOpt (gmoHandle_t pgmo, void *optptr, const char *dotopt, int optvals[])
{
  int d_s[]={3,1,11,8};
  GAMS_UNUSED(pgmo)
  GAMS_UNUSED(optptr)
  GAMS_UNUSED(dotopt)
  GAMS_UNUSED(optvals)
  printAndReturn(gmoGetEquIntDotOpt,3,int )
}

/** Get equation double values for dot optio
 * @param pgmo gmo object handle
 * @param optptr Option object pointer
 * @param dotopt Dot option name
 * @param optvals Option values
 */
int  GMO_CALLCONV d_gmoGetEquDblDotOpt (gmoHandle_t pgmo, void *optptr, const char *dotopt, double optvals[])
{
  int d_s[]={3,1,11,6};
  GAMS_UNUSED(pgmo)
  GAMS_UNUSED(optptr)
  GAMS_UNUSED(dotopt)
  GAMS_UNUSED(optvals)
  printAndReturn(gmoGetEquDblDotOpt,3,int )
}

/** Get variable level values
 * @param pgmo gmo object handle
 * @param x Level values of variables
 */
int  GMO_CALLCONV d_gmoGetVarL (gmoHandle_t pgmo, double x[])
{
  int d_s[]={3,6};
  GAMS_UNUSED(pgmo)
  GAMS_UNUSED(x)
  printAndReturn(gmoGetVarL,1,int )
}

/** Get individual variable level
 * @param pgmo gmo object handle
 * @param sj Index of column in client space
 */
double  GMO_CALLCONV d_gmoGetVarLOne (gmoHandle_t pgmo, int sj)
{
  int d_s[]={13,3};
  GAMS_UNUSED(pgmo)
  GAMS_UNUSED(sj)
  printAndReturn(gmoGetVarLOne,1,double )
}

/** Set variable level values
 * @param pgmo gmo object handle
 * @param x Level values of variables
 */
int  GMO_CALLCONV d_gmoSetVarL (gmoHandle_t pgmo, const double x[])
{
  int d_s[]={3,5};
  GAMS_UNUSED(pgmo)
  GAMS_UNUSED(x)
  printAndReturn(gmoSetVarL,1,int )
}

/** Set individual variable level
 * @param pgmo gmo object handle
 * @param sj Index of column in client space
 * @param vl Level of variable
 */
void  GMO_CALLCONV d_gmoSetVarLOne (gmoHandle_t pgmo, int sj, double vl)
{
  int d_s[]={0,3,13};
  GAMS_UNUSED(pgmo)
  GAMS_UNUSED(sj)
  GAMS_UNUSED(vl)
  printNoReturn(gmoSetVarLOne,2)
}

/** Get variable marginals
 * @param pgmo gmo object handle
 * @param dj Marginal values of variables
 */
int  GMO_CALLCONV d_gmoGetVarM (gmoHandle_t pgmo, double dj[])
{
  int d_s[]={3,6};
  GAMS_UNUSED(pgmo)
  GAMS_UNUSED(dj)
  printAndReturn(gmoGetVarM,1,int )
}

/** Get individual variable marginal
 * @param pgmo gmo object handle
 * @param sj Index of column in client space
 */
double  GMO_CALLCONV d_gmoGetVarMOne (gmoHandle_t pgmo, int sj)
{
  int d_s[]={13,3};
  GAMS_UNUSED(pgmo)
  GAMS_UNUSED(sj)
  printAndReturn(gmoGetVarMOne,1,double )
}

/** Set variable marginals (pass null to set to NA)'
 * @param pgmo gmo object handle
 * @param dj Marginal values of variables
 */
int  GMO_CALLCONV d_gmoSetVarM (gmoHandle_t pgmo, const double dj[])
{
  int d_s[]={3,5};
  GAMS_UNUSED(pgmo)
  GAMS_UNUSED(dj)
  printAndReturn(gmoSetVarM,1,int )
}

/** Set individual variable marginal
 * @param pgmo gmo object handle
 * @param sj Index of column in client space
 * @param vmarg Marginal of variable
 */
void  GMO_CALLCONV d_gmoSetVarMOne (gmoHandle_t pgmo, int sj, double vmarg)
{
  int d_s[]={0,3,13};
  GAMS_UNUSED(pgmo)
  GAMS_UNUSED(sj)
  GAMS_UNUSED(vmarg)
  printNoReturn(gmoSetVarMOne,2)
}

/** Get individual column name
 * @param pgmo gmo object handle
 * @param sj Index of column in client space
 */
char * GMO_CALLCONV d_gmoGetVarNameOne (gmoHandle_t pgmo, int sj, char *buf)
{
  int d_s[]={12,3};
  GAMS_UNUSED(pgmo)
  GAMS_UNUSED(sj)
  GAMS_UNUSED(buf)
  printAndReturn(gmoGetVarNameOne,1,char *)
}

/** Get individual column name with quotes and user specified suffix
 * @param pgmo gmo object handle
 * @param sj Index of column in client space
 * @param suffix Suffix appended to name, could be .l, .m etc.
 */
char * GMO_CALLCONV d_gmoGetVarNameCustomOne (gmoHandle_t pgmo, int sj, const char *suffix, char *buf)
{
  int d_s[]={12,3,11};
  GAMS_UNUSED(pgmo)
  GAMS_UNUSED(sj)
  GAMS_UNUSED(suffix)
  GAMS_UNUSED(buf)
  printAndReturn(gmoGetVarNameCustomOne,2,char *)
}

/** Get variable lower bounds
 * @param pgmo gmo object handle
 * @param lovec Lower bound values of variables
 */
int  GMO_CALLCONV d_gmoGetVarLower (gmoHandle_t pgmo, double lovec[])
{
  int d_s[]={3,6};
  GAMS_UNUSED(pgmo)
  GAMS_UNUSED(lovec)
  printAndReturn(gmoGetVarLower,1,int )
}

/** Get individual variable lower bound
 * @param pgmo gmo object handle
 * @param sj Index of column in client space
 */
double  GMO_CALLCONV d_gmoGetVarLowerOne (gmoHandle_t pgmo, int sj)
{
  int d_s[]={13,3};
  GAMS_UNUSED(pgmo)
  GAMS_UNUSED(sj)
  printAndReturn(gmoGetVarLowerOne,1,double )
}

/** Get variable upper bounds
 * @param pgmo gmo object handle
 * @param upvec Upper bound values of variables
 */
int  GMO_CALLCONV d_gmoGetVarUpper (gmoHandle_t pgmo, double upvec[])
{
  int d_s[]={3,6};
  GAMS_UNUSED(pgmo)
  GAMS_UNUSED(upvec)
  printAndReturn(gmoGetVarUpper,1,int )
}

/** Get individual variable upper bound
 * @param pgmo gmo object handle
 * @param sj Index of column in client space
 */
double  GMO_CALLCONV d_gmoGetVarUpperOne (gmoHandle_t pgmo, int sj)
{
  int d_s[]={13,3};
  GAMS_UNUSED(pgmo)
  GAMS_UNUSED(sj)
  printAndReturn(gmoGetVarUpperOne,1,double )
}

/** Set alternative variable lower and upper bounds
 * @param pgmo gmo object handle
 * @param lovec Lower bound values of variables
 * @param upvec Upper bound values of variables
 */
int  GMO_CALLCONV d_gmoSetAltVarBounds (gmoHandle_t pgmo, const double lovec[], const double upvec[])
{
  int d_s[]={3,5,5};
  GAMS_UNUSED(pgmo)
  GAMS_UNUSED(lovec)
  GAMS_UNUSED(upvec)
  printAndReturn(gmoSetAltVarBounds,2,int )
}

/** Set individual alternative variable lower bound
 * @param pgmo gmo object handle
 * @param sj Index of column in client space
 * @param vlo Lower bound of variable
 */
void  GMO_CALLCONV d_gmoSetAltVarLowerOne (gmoHandle_t pgmo, int sj, double vlo)
{
  int d_s[]={0,3,13};
  GAMS_UNUSED(pgmo)
  GAMS_UNUSED(sj)
  GAMS_UNUSED(vlo)
  printNoReturn(gmoSetAltVarLowerOne,2)
}

/** Set individual alternative variable upper bound
 * @param pgmo gmo object handle
 * @param sj Index of column in client space
 * @param vup Upper bound of variable
 */
void  GMO_CALLCONV d_gmoSetAltVarUpperOne (gmoHandle_t pgmo, int sj, double vup)
{
  int d_s[]={0,3,13};
  GAMS_UNUSED(pgmo)
  GAMS_UNUSED(sj)
  GAMS_UNUSED(vup)
  printNoReturn(gmoSetAltVarUpperOne,2)
}

/** Get variable type
 * @param pgmo gmo object handle
 * @param nintvec Array of integers, len=number of columns in user view
 */
int  GMO_CALLCONV d_gmoGetVarType (gmoHandle_t pgmo, int nintvec[])
{
  int d_s[]={3,8};
  GAMS_UNUSED(pgmo)
  GAMS_UNUSED(nintvec)
  printAndReturn(gmoGetVarType,1,int )
}

/** Get individual variable type
 * @param pgmo gmo object handle
 * @param sj Index of column in client space
 */
int  GMO_CALLCONV d_gmoGetVarTypeOne (gmoHandle_t pgmo, int sj)
{
  int d_s[]={3,3};
  GAMS_UNUSED(pgmo)
  GAMS_UNUSED(sj)
  printAndReturn(gmoGetVarTypeOne,1,int )
}

/** Set alternative variable type
 * @param pgmo gmo object handle
 * @param nintvec Array of integers, len=number of columns in user view
 */
int  GMO_CALLCONV d_gmoSetAltVarType (gmoHandle_t pgmo, const int nintvec[])
{
  int d_s[]={3,7};
  GAMS_UNUSED(pgmo)
  GAMS_UNUSED(nintvec)
  printAndReturn(gmoSetAltVarType,1,int )
}

/** Set individual alternative variable type
 * @param pgmo gmo object handle
 * @param sj Index of column in client space
 * @param vtyp Type of variable (see enumerated constants)
 */
void  GMO_CALLCONV d_gmoSetAltVarTypeOne (gmoHandle_t pgmo, int sj, int vtyp)
{
  int d_s[]={0,3,3};
  GAMS_UNUSED(pgmo)
  GAMS_UNUSED(sj)
  GAMS_UNUSED(vtyp)
  printNoReturn(gmoSetAltVarTypeOne,2)
}

/** Get variable basis status
 * @param pgmo gmo object handle
 * @param nintvec Array of integers, len=number of columns in user view
 */
void  GMO_CALLCONV d_gmoGetVarStat (gmoHandle_t pgmo, int nintvec[])
{
  int d_s[]={0,8};
  GAMS_UNUSED(pgmo)
  GAMS_UNUSED(nintvec)
  printNoReturn(gmoGetVarStat,1)
}

/** Get individual variable basis status
 * @param pgmo gmo object handle
 * @param sj Index of column in client space
 */
int  GMO_CALLCONV d_gmoGetVarStatOne (gmoHandle_t pgmo, int sj)
{
  int d_s[]={3,3};
  GAMS_UNUSED(pgmo)
  GAMS_UNUSED(sj)
  printAndReturn(gmoGetVarStatOne,1,int )
}

/** Set variable basis status
 * @param pgmo gmo object handle
 * @param nintvec Array of integers, len=number of columns in user view
 */
void  GMO_CALLCONV d_gmoSetVarStat (gmoHandle_t pgmo, const int nintvec[])
{
  int d_s[]={0,7};
  GAMS_UNUSED(pgmo)
  GAMS_UNUSED(nintvec)
  printNoReturn(gmoSetVarStat,1)
}

/** Set individual variable basis status
 * @param pgmo gmo object handle
 * @param sj Index of column in client space
 * @param vstat Basis status of variable (see enumerated constants)
 */
void  GMO_CALLCONV d_gmoSetVarStatOne (gmoHandle_t pgmo, int sj, int vstat)
{
  int d_s[]={0,3,3};
  GAMS_UNUSED(pgmo)
  GAMS_UNUSED(sj)
  GAMS_UNUSED(vstat)
  printNoReturn(gmoSetVarStatOne,2)
}

/** Get variable status
 * @param pgmo gmo object handle
 * @param nintvec Array of integers, len=number of columns in user view
 */
void  GMO_CALLCONV d_gmoGetVarCStat (gmoHandle_t pgmo, int nintvec[])
{
  int d_s[]={0,8};
  GAMS_UNUSED(pgmo)
  GAMS_UNUSED(nintvec)
  printNoReturn(gmoGetVarCStat,1)
}

/** Get individual variable status
 * @param pgmo gmo object handle
 * @param sj Index of column in client space
 */
int  GMO_CALLCONV d_gmoGetVarCStatOne (gmoHandle_t pgmo, int sj)
{
  int d_s[]={3,3};
  GAMS_UNUSED(pgmo)
  GAMS_UNUSED(sj)
  printAndReturn(gmoGetVarCStatOne,1,int )
}

/** Set variable status
 * @param pgmo gmo object handle
 * @param nintvec Array of integers, len=number of columns in user view
 */
void  GMO_CALLCONV d_gmoSetVarCStat (gmoHandle_t pgmo, const int nintvec[])
{
  int d_s[]={0,7};
  GAMS_UNUSED(pgmo)
  GAMS_UNUSED(nintvec)
  printNoReturn(gmoSetVarCStat,1)
}

/** Get variable match
 * @param pgmo gmo object handle
 * @param nintvec Array of integers, len=number of columns in user view
 */
int  GMO_CALLCONV d_gmoGetVarMatch (gmoHandle_t pgmo, int nintvec[])
{
  int d_s[]={3,8};
  GAMS_UNUSED(pgmo)
  GAMS_UNUSED(nintvec)
  printAndReturn(gmoGetVarMatch,1,int )
}

/** Get individual variable match
 * @param pgmo gmo object handle
 * @param sj Index of column in client space
 */
int  GMO_CALLCONV d_gmoGetVarMatchOne (gmoHandle_t pgmo, int sj)
{
  int d_s[]={3,3};
  GAMS_UNUSED(pgmo)
  GAMS_UNUSED(sj)
  printAndReturn(gmoGetVarMatchOne,1,int )
}

/** Get variable branching priority
 * @param pgmo gmo object handle
 * @param ndblvec Array of doubles, len=number of columns in user view
 */
int  GMO_CALLCONV d_gmoGetVarPrior (gmoHandle_t pgmo, double ndblvec[])
{
  int d_s[]={3,6};
  GAMS_UNUSED(pgmo)
  GAMS_UNUSED(ndblvec)
  printAndReturn(gmoGetVarPrior,1,int )
}

/** Get individual variable branching priority
 * @param pgmo gmo object handle
 * @param sj Index of column in client space
 */
double  GMO_CALLCONV d_gmoGetVarPriorOne (gmoHandle_t pgmo, int sj)
{
  int d_s[]={13,3};
  GAMS_UNUSED(pgmo)
  GAMS_UNUSED(sj)
  printAndReturn(gmoGetVarPriorOne,1,double )
}

/** Get variable scale
 * @param pgmo gmo object handle
 * @param ndblvec Array of doubles, len=number of columns in user view
 */
int  GMO_CALLCONV d_gmoGetVarScale (gmoHandle_t pgmo, double ndblvec[])
{
  int d_s[]={3,6};
  GAMS_UNUSED(pgmo)
  GAMS_UNUSED(ndblvec)
  printAndReturn(gmoGetVarScale,1,int )
}

/** Get individual variable scale
 * @param pgmo gmo object handle
 * @param sj Index of column in client space
 */
double  GMO_CALLCONV d_gmoGetVarScaleOne (gmoHandle_t pgmo, int sj)
{
  int d_s[]={13,3};
  GAMS_UNUSED(pgmo)
  GAMS_UNUSED(sj)
  printAndReturn(gmoGetVarScaleOne,1,double )
}

/** Get variable stage
 * @param pgmo gmo object handle
 * @param ndblvec Array of doubles, len=number of columns in user view
 */
int  GMO_CALLCONV d_gmoGetVarStage (gmoHandle_t pgmo, double ndblvec[])
{
  int d_s[]={3,6};
  GAMS_UNUSED(pgmo)
  GAMS_UNUSED(ndblvec)
  printAndReturn(gmoGetVarStage,1,int )
}

/** Get individual variable stage
 * @param pgmo gmo object handle
 * @param sj Index of column in client space
 */
double  GMO_CALLCONV d_gmoGetVarStageOne (gmoHandle_t pgmo, int sj)
{
  int d_s[]={13,3};
  GAMS_UNUSED(pgmo)
  GAMS_UNUSED(sj)
  printAndReturn(gmoGetVarStageOne,1,double )
}

/** Get SOS constraints
 * @param pgmo gmo object handle
 * @param sostype SOS type 1 or 2
 * @param sosbeg Variable index start of SOS set
 * @param sosind Variable indices
 * @param soswt Weight in SOS set
 */
int  GMO_CALLCONV d_gmoGetSosConstraints (gmoHandle_t pgmo, int sostype[], int sosbeg[], int sosind[], double soswt[])
{
  int d_s[]={3,8,8,8,6};
  GAMS_UNUSED(pgmo)
  GAMS_UNUSED(sostype)
  GAMS_UNUSED(sosbeg)
  GAMS_UNUSED(sosind)
  GAMS_UNUSED(soswt)
  printAndReturn(gmoGetSosConstraints,4,int )
}

/** Get SOS set for individual variable
 * @param pgmo gmo object handle
 * @param sj Index of column in client space
 */
int  GMO_CALLCONV d_gmoGetVarSosSetOne (gmoHandle_t pgmo, int sj)
{
  int d_s[]={3,3};
  GAMS_UNUSED(pgmo)
  GAMS_UNUSED(sj)
  printAndReturn(gmoGetVarSosSetOne,1,int )
}

/** Get Jacobians information of column (sparse)
 * @param pgmo gmo object handle
 * @param sj Index of column in client space
 * @param rowidx Row index/indices of Jacobian
 * @param jacval Value(s) of Jacobian(s)
 * @param nlflag NL flag(s) of Jacobian(s) (0 : linear,!=0 : nonlinear)
 * @param nz Number of nonzeros
 * @param nlnz Number of nonlinear nonzeros
 */
int  GMO_CALLCONV d_gmoGetColSparse (gmoHandle_t pgmo, int sj, int rowidx[], double jacval[], int nlflag[], int *nz, int *nlnz)
{
  int d_s[]={3,3,8,6,8,4,4};
  GAMS_UNUSED(pgmo)
  GAMS_UNUSED(sj)
  GAMS_UNUSED(rowidx)
  GAMS_UNUSED(jacval)
  GAMS_UNUSED(nlflag)
  GAMS_UNUSED(nz)
  GAMS_UNUSED(nlnz)
  printAndReturn(gmoGetColSparse,6,int )
}

/** Get Jacobian information of column one by one
 * @param pgmo gmo object handle
 * @param sj Index of column in client space
 * @param jacptr Pointer to next Jacobian
 * @param jacval Value(s) of Jacobian(s)
 * @param rowidx Row index/indices of Jacobian
 * @param nlflag NL flag(s) of Jacobian(s) (0 : linear,!=0 : nonlinear)
 */
void  GMO_CALLCONV d_gmoGetColJacInfoOne (gmoHandle_t pgmo, int sj, void **jacptr, double *jacval, int *rowidx, int *nlflag)
{
  int d_s[]={0,3,2,14,4,4};
  GAMS_UNUSED(pgmo)
  GAMS_UNUSED(sj)
  GAMS_UNUSED(jacptr)
  GAMS_UNUSED(jacval)
  GAMS_UNUSED(rowidx)
  GAMS_UNUSED(nlflag)
  printNoReturn(gmoGetColJacInfoOne,5)
}

/** Get variable integer values for dot option
 * @param pgmo gmo object handle
 * @param optptr Option object pointer
 * @param dotopt Dot option name
 * @param optvals Option values
 */
int  GMO_CALLCONV d_gmoGetVarIntDotOpt (gmoHandle_t pgmo, void *optptr, const char *dotopt, int optvals[])
{
  int d_s[]={3,1,11,8};
  GAMS_UNUSED(pgmo)
  GAMS_UNUSED(optptr)
  GAMS_UNUSED(dotopt)
  GAMS_UNUSED(optvals)
  printAndReturn(gmoGetVarIntDotOpt,3,int )
}

/** Get variable double values for dot option
 * @param pgmo gmo object handle
 * @param optptr Option object pointer
 * @param dotopt Dot option name
 * @param optvals Option values
 */
int  GMO_CALLCONV d_gmoGetVarDblDotOpt (gmoHandle_t pgmo, void *optptr, const char *dotopt, double optvals[])
{
  int d_s[]={3,1,11,6};
  GAMS_UNUSED(pgmo)
  GAMS_UNUSED(optptr)
  GAMS_UNUSED(dotopt)
  GAMS_UNUSED(optvals)
  printAndReturn(gmoGetVarDblDotOpt,3,int )
}

/** Control writing messages for evaluation errors, default=true
 * @param pgmo gmo object handle
 * @param domsg Flag whether to write messages
 */
void  GMO_CALLCONV d_gmoEvalErrorMsg (gmoHandle_t pgmo, int domsg)
{
  int d_s[]={0,15};
  GAMS_UNUSED(pgmo)
  GAMS_UNUSED(domsg)
  printNoReturn(gmoEvalErrorMsg,1)
}

/** Control writing messages for evaluation errors, default=true
 * @param pgmo gmo object handle
 * @param domsg Flag whether to write messages
 * @param tidx Index of thread
 */
void  GMO_CALLCONV d_gmoEvalErrorMsg_MT (gmoHandle_t pgmo, int domsg, int tidx)
{
  int d_s[]={0,15,3};
  GAMS_UNUSED(pgmo)
  GAMS_UNUSED(domsg)
  GAMS_UNUSED(tidx)
  printNoReturn(gmoEvalErrorMsg_MT,2)
}

/** Set mask to ignore errors >= evalErrorMaskLevel when incrementing numerr
 * @param pgmo gmo object handle
 * @param MaskLevel Ignore evaluation errors less that this value
 */
void  GMO_CALLCONV d_gmoEvalErrorMaskLevel (gmoHandle_t pgmo, int MaskLevel)
{
  int d_s[]={0,3};
  GAMS_UNUSED(pgmo)
  GAMS_UNUSED(MaskLevel)
  printNoReturn(gmoEvalErrorMaskLevel,1)
}

/** Set mask to ignore errors >= evalErrorMaskLevel when incrementing numerr
 * @param pgmo gmo object handle
 * @param MaskLevel Ignore evaluation errors less that this value
 * @param tidx Index of thread
 */
void  GMO_CALLCONV d_gmoEvalErrorMaskLevel_MT (gmoHandle_t pgmo, int MaskLevel, int tidx)
{
  int d_s[]={0,3,3};
  GAMS_UNUSED(pgmo)
  GAMS_UNUSED(MaskLevel)
  GAMS_UNUSED(tidx)
  printNoReturn(gmoEvalErrorMaskLevel_MT,2)
}

/** New point for the next evaluation call
 * @param pgmo gmo object handle
 * @param x Level values of variables
 */
int  GMO_CALLCONV d_gmoEvalNewPoint (gmoHandle_t pgmo, const double x[])
{
  int d_s[]={3,5};
  GAMS_UNUSED(pgmo)
  GAMS_UNUSED(x)
  printAndReturn(gmoEvalNewPoint,1,int )
}

/** Set external function manager object
 * @param pgmo gmo object handle
 * @param extfunmgr 
 */
void  GMO_CALLCONV d_gmoSetExtFuncs (gmoHandle_t pgmo, void *extfunmgr)
{
  int d_s[]={0,1};
  GAMS_UNUSED(pgmo)
  GAMS_UNUSED(extfunmgr)
  printNoReturn(gmoSetExtFuncs,1)
}

/** Get QMaker stats
 * @param pgmo gmo object handle
 * @param algName the name of the QMaker algorithm used
 * @param algTime the wall-clock time in seconds used by QMaker
 * @param winnerCount3Pass count of rows where new 3-pass alg was the winner
 * @param winnerCountDblFwd count of rows where old double-forward alg was the winner
 */
int  GMO_CALLCONV d_gmoGetQMakerStats (gmoHandle_t pgmo, char *algName, double *algTime, INT64 *winnerCount3Pass, INT64 *winnerCountDblFwd)
{
  int d_s[]={3,12,14,25,25};
  GAMS_UNUSED(pgmo)
  GAMS_UNUSED(algName)
  GAMS_UNUSED(algTime)
  GAMS_UNUSED(winnerCount3Pass)
  GAMS_UNUSED(winnerCountDblFwd)
  printAndReturn(gmoGetQMakerStats,4,int )
}

/** Evaluate the constraint si (excluding RHS)
 * @param pgmo gmo object handle
 * @param si Index of row in client space
 * @param x Level values of variables
 * @param f Function value
 * @param numerr Number of errors evaluating the nonlinear function
 */
int  GMO_CALLCONV d_gmoEvalFunc (gmoHandle_t pgmo, int si, const double x[], double *f, int *numerr)
{
  int d_s[]={3,3,5,14,4};
  GAMS_UNUSED(pgmo)
  GAMS_UNUSED(si)
  GAMS_UNUSED(x)
  GAMS_UNUSED(f)
  GAMS_UNUSED(numerr)
  printAndReturn(gmoEvalFunc,4,int )
}

/** Evaluate the constraint si (excluding RHS)
 * @param pgmo gmo object handle
 * @param si Index of row in client space
 * @param x Level values of variables
 * @param f Function value
 * @param numerr Number of errors evaluating the nonlinear function
 * @param tidx Index of thread
 */
int  GMO_CALLCONV d_gmoEvalFunc_MT (gmoHandle_t pgmo, int si, const double x[], double *f, int *numerr, int tidx)
{
  int d_s[]={3,3,5,14,4,3};
  GAMS_UNUSED(pgmo)
  GAMS_UNUSED(si)
  GAMS_UNUSED(x)
  GAMS_UNUSED(f)
  GAMS_UNUSED(numerr)
  GAMS_UNUSED(tidx)
  printAndReturn(gmoEvalFunc_MT,5,int )
}

/** Evaluate the constraint si using the GMO internal variable levels (excluding RHS)
 * @param pgmo gmo object handle
 * @param si Index of row in client space
 * @param f Function value
 * @param numerr Number of errors evaluating the nonlinear function
 */
int  GMO_CALLCONV d_gmoEvalFuncInt (gmoHandle_t pgmo, int si, double *f, int *numerr)
{
  int d_s[]={3,3,14,4};
  GAMS_UNUSED(pgmo)
  GAMS_UNUSED(si)
  GAMS_UNUSED(f)
  GAMS_UNUSED(numerr)
  printAndReturn(gmoEvalFuncInt,3,int )
}

/** Evaluate the constraint si using the GMO internal variable levels (excluding RHS)
 * @param pgmo gmo object handle
 * @param si Index of row in client space
 * @param f Function value
 * @param numerr Number of errors evaluating the nonlinear function
 * @param tidx Index of thread
 */
int  GMO_CALLCONV d_gmoEvalFuncInt_MT (gmoHandle_t pgmo, int si, double *f, int *numerr, int tidx)
{
  int d_s[]={3,3,14,4,3};
  GAMS_UNUSED(pgmo)
  GAMS_UNUSED(si)
  GAMS_UNUSED(f)
  GAMS_UNUSED(numerr)
  GAMS_UNUSED(tidx)
  printAndReturn(gmoEvalFuncInt_MT,4,int )
}

/** Evaluate the nonlinear function component of constraint si
 * @param pgmo gmo object handle
 * @param si Index of row in client space
 * @param x Level values of variables
 * @param fnl Part of the function value depending on the nonlinear variables
 * @param numerr Number of errors evaluating the nonlinear function
 */
int  GMO_CALLCONV d_gmoEvalFuncNL (gmoHandle_t pgmo, int si, const double x[], double *fnl, int *numerr)
{
  int d_s[]={3,3,5,14,4};
  GAMS_UNUSED(pgmo)
  GAMS_UNUSED(si)
  GAMS_UNUSED(x)
  GAMS_UNUSED(fnl)
  GAMS_UNUSED(numerr)
  printAndReturn(gmoEvalFuncNL,4,int )
}

/** Evaluate the nonlinear function component of constraint si
 * @param pgmo gmo object handle
 * @param si Index of row in client space
 * @param x Level values of variables
 * @param fnl Part of the function value depending on the nonlinear variables
 * @param numerr Number of errors evaluating the nonlinear function
 * @param tidx Index of thread
 */
int  GMO_CALLCONV d_gmoEvalFuncNL_MT (gmoHandle_t pgmo, int si, const double x[], double *fnl, int *numerr, int tidx)
{
  int d_s[]={3,3,5,14,4,3};
  GAMS_UNUSED(pgmo)
  GAMS_UNUSED(si)
  GAMS_UNUSED(x)
  GAMS_UNUSED(fnl)
  GAMS_UNUSED(numerr)
  GAMS_UNUSED(tidx)
  printAndReturn(gmoEvalFuncNL_MT,5,int )
}

/** Evaluate objective function component
 * @param pgmo gmo object handle
 * @param x Level values of variables
 * @param f Function value
 * @param numerr Number of errors evaluating the nonlinear function
 */
int  GMO_CALLCONV d_gmoEvalFuncObj (gmoHandle_t pgmo, const double x[], double *f, int *numerr)
{
  int d_s[]={3,5,14,4};
  GAMS_UNUSED(pgmo)
  GAMS_UNUSED(x)
  GAMS_UNUSED(f)
  GAMS_UNUSED(numerr)
  printAndReturn(gmoEvalFuncObj,3,int )
}

/** Evaluate nonlinear objective function component
 * @param pgmo gmo object handle
 * @param x Level values of variables
 * @param fnl Part of the function value depending on the nonlinear variables
 * @param numerr Number of errors evaluating the nonlinear function
 */
int  GMO_CALLCONV d_gmoEvalFuncNLObj (gmoHandle_t pgmo, const double x[], double *fnl, int *numerr)
{
  int d_s[]={3,5,14,4};
  GAMS_UNUSED(pgmo)
  GAMS_UNUSED(x)
  GAMS_UNUSED(fnl)
  GAMS_UNUSED(numerr)
  printAndReturn(gmoEvalFuncNLObj,3,int )
}

/** Evaluate the function value of constraint si on the giving interval
 * @param pgmo gmo object handle
 * @param si Index of row in client space
 * @param xmin Minimum input level values of variables
 * @param xmax Maximum input level values of variables
 * @param fmin Minimum function value
 * @param fmax Maximum function value
 * @param numerr Number of errors evaluating the nonlinear function
 */
int  GMO_CALLCONV d_gmoEvalFuncInterval (gmoHandle_t pgmo, int si, const double xmin[], const double xmax[], double *fmin, double *fmax, int *numerr)
{
  int d_s[]={3,3,5,5,14,14,4};
  GAMS_UNUSED(pgmo)
  GAMS_UNUSED(si)
  GAMS_UNUSED(xmin)
  GAMS_UNUSED(xmax)
  GAMS_UNUSED(fmin)
  GAMS_UNUSED(fmax)
  GAMS_UNUSED(numerr)
  printAndReturn(gmoEvalFuncInterval,6,int )
}

/** Evaluate the function value of constraint si on the giving interval
 * @param pgmo gmo object handle
 * @param si Index of row in client space
 * @param xmin Minimum input level values of variables
 * @param xmax Maximum input level values of variables
 * @param fmin Minimum function value
 * @param fmax Maximum function value
 * @param numerr Number of errors evaluating the nonlinear function
 * @param tidx Index of thread
 */
int  GMO_CALLCONV d_gmoEvalFuncInterval_MT (gmoHandle_t pgmo, int si, const double xmin[], const double xmax[], double *fmin, double *fmax, int *numerr, int tidx)
{
  int d_s[]={3,3,5,5,14,14,4,3};
  GAMS_UNUSED(pgmo)
  GAMS_UNUSED(si)
  GAMS_UNUSED(xmin)
  GAMS_UNUSED(xmax)
  GAMS_UNUSED(fmin)
  GAMS_UNUSED(fmax)
  GAMS_UNUSED(numerr)
  GAMS_UNUSED(tidx)
  printAndReturn(gmoEvalFuncInterval_MT,7,int )
}

/** Update the nonlinear gradients of constraint si and evaluate function value
 * @param pgmo gmo object handle
 * @param si Index of row in client space
 * @param x Level values of variables
 * @param f Function value
 * @param g Gradient values
 * @param gx Inner product of the gradient with the input variables
 * @param numerr Number of errors evaluating the nonlinear function
 */
int  GMO_CALLCONV d_gmoEvalGrad (gmoHandle_t pgmo, int si, const double x[], double *f, double g[], double *gx, int *numerr)
{
  int d_s[]={3,3,5,14,6,14,4};
  GAMS_UNUSED(pgmo)
  GAMS_UNUSED(si)
  GAMS_UNUSED(x)
  GAMS_UNUSED(f)
  GAMS_UNUSED(g)
  GAMS_UNUSED(gx)
  GAMS_UNUSED(numerr)
  printAndReturn(gmoEvalGrad,6,int )
}

/** Update the nonlinear gradients of constraint si and evaluate function value
 * @param pgmo gmo object handle
 * @param si Index of row in client space
 * @param x Level values of variables
 * @param f Function value
 * @param g Gradient values
 * @param gx Inner product of the gradient with the input variables
 * @param numerr Number of errors evaluating the nonlinear function
 * @param tidx Index of thread
 */
int  GMO_CALLCONV d_gmoEvalGrad_MT (gmoHandle_t pgmo, int si, const double x[], double *f, double g[], double *gx, int *numerr, int tidx)
{
  int d_s[]={3,3,5,14,6,14,4,3};
  GAMS_UNUSED(pgmo)
  GAMS_UNUSED(si)
  GAMS_UNUSED(x)
  GAMS_UNUSED(f)
  GAMS_UNUSED(g)
  GAMS_UNUSED(gx)
  GAMS_UNUSED(numerr)
  GAMS_UNUSED(tidx)
  printAndReturn(gmoEvalGrad_MT,7,int )
}

/** Update the nonlinear gradients of constraint si and evaluate nonlinear function and gradient value
 * @param pgmo gmo object handle
 * @param si Index of row in client space
 * @param x Level values of variables
 * @param fnl Part of the function value depending on the nonlinear variables
 * @param g Gradient values
 * @param gxnl Inner product of the gradient with the input variables, nonlinear variables only
 * @param numerr Number of errors evaluating the nonlinear function
 */
int  GMO_CALLCONV d_gmoEvalGradNL (gmoHandle_t pgmo, int si, const double x[], double *fnl, double g[], double *gxnl, int *numerr)
{
  int d_s[]={3,3,5,14,6,14,4};
  GAMS_UNUSED(pgmo)
  GAMS_UNUSED(si)
  GAMS_UNUSED(x)
  GAMS_UNUSED(fnl)
  GAMS_UNUSED(g)
  GAMS_UNUSED(gxnl)
  GAMS_UNUSED(numerr)
  printAndReturn(gmoEvalGradNL,6,int )
}

/** Update the nonlinear gradients of constraint si and evaluate nonlinear function and gradient value
 * @param pgmo gmo object handle
 * @param si Index of row in client space
 * @param x Level values of variables
 * @param fnl Part of the function value depending on the nonlinear variables
 * @param g Gradient values
 * @param gxnl Inner product of the gradient with the input variables, nonlinear variables only
 * @param numerr Number of errors evaluating the nonlinear function
 * @param tidx Index of thread
 */
int  GMO_CALLCONV d_gmoEvalGradNL_MT (gmoHandle_t pgmo, int si, const double x[], double *fnl, double g[], double *gxnl, int *numerr, int tidx)
{
  int d_s[]={3,3,5,14,6,14,4,3};
  GAMS_UNUSED(pgmo)
  GAMS_UNUSED(si)
  GAMS_UNUSED(x)
  GAMS_UNUSED(fnl)
  GAMS_UNUSED(g)
  GAMS_UNUSED(gxnl)
  GAMS_UNUSED(numerr)
  GAMS_UNUSED(tidx)
  printAndReturn(gmoEvalGradNL_MT,7,int )
}

/** Update the gradients of the objective function and evaluate function and gradient value
 * @param pgmo gmo object handle
 * @param x Level values of variables
 * @param f Function value
 * @param g Gradient values
 * @param gx Inner product of the gradient with the input variables
 * @param numerr Number of errors evaluating the nonlinear function
 */
int  GMO_CALLCONV d_gmoEvalGradObj (gmoHandle_t pgmo, const double x[], double *f, double g[], double *gx, int *numerr)
{
  int d_s[]={3,5,14,6,14,4};
  GAMS_UNUSED(pgmo)
  GAMS_UNUSED(x)
  GAMS_UNUSED(f)
  GAMS_UNUSED(g)
  GAMS_UNUSED(gx)
  GAMS_UNUSED(numerr)
  printAndReturn(gmoEvalGradObj,5,int )
}

/** Update the nonlinear gradients of the objective function and evaluate function and gradient value
 * @param pgmo gmo object handle
 * @param x Level values of variables
 * @param fnl Part of the function value depending on the nonlinear variables
 * @param g Gradient values
 * @param gxnl Inner product of the gradient with the input variables, nonlinear variables only
 * @param numerr Number of errors evaluating the nonlinear function
 */
int  GMO_CALLCONV d_gmoEvalGradNLObj (gmoHandle_t pgmo, const double x[], double *fnl, double g[], double *gxnl, int *numerr)
{
  int d_s[]={3,5,14,6,14,4};
  GAMS_UNUSED(pgmo)
  GAMS_UNUSED(x)
  GAMS_UNUSED(fnl)
  GAMS_UNUSED(g)
  GAMS_UNUSED(gxnl)
  GAMS_UNUSED(numerr)
  printAndReturn(gmoEvalGradNLObj,5,int )
}

/** Evaluate the function and gradient value of constraint si on the giving interval
 * @param pgmo gmo object handle
 * @param si Index of row in client space
 * @param xmin Minimum input level values of variables
 * @param xmax Maximum input level values of variables
 * @param fmin Minimum function value
 * @param fmax Maximum function value
 * @param gmin Minimum gradient values
 * @param gmax Maximum gradient values
 * @param numerr Number of errors evaluating the nonlinear function
 */
int  GMO_CALLCONV d_gmoEvalGradInterval (gmoHandle_t pgmo, int si, const double xmin[], const double xmax[], double *fmin, double *fmax, double gmin[], double gmax[], int *numerr)
{
  int d_s[]={3,3,5,5,14,14,6,6,4};
  GAMS_UNUSED(pgmo)
  GAMS_UNUSED(si)
  GAMS_UNUSED(xmin)
  GAMS_UNUSED(xmax)
  GAMS_UNUSED(fmin)
  GAMS_UNUSED(fmax)
  GAMS_UNUSED(gmin)
  GAMS_UNUSED(gmax)
  GAMS_UNUSED(numerr)
  printAndReturn(gmoEvalGradInterval,8,int )
}

/** Evaluate the function and gradient value of constraint si on the giving interval
 * @param pgmo gmo object handle
 * @param si Index of row in client space
 * @param xmin Minimum input level values of variables
 * @param xmax Maximum input level values of variables
 * @param fmin Minimum function value
 * @param fmax Maximum function value
 * @param gmin Minimum gradient values
 * @param gmax Maximum gradient values
 * @param numerr Number of errors evaluating the nonlinear function
 * @param tidx Index of thread
 */
int  GMO_CALLCONV d_gmoEvalGradInterval_MT (gmoHandle_t pgmo, int si, const double xmin[], const double xmax[], double *fmin, double *fmax, double gmin[], double gmax[], int *numerr, int tidx)
{
  int d_s[]={3,3,5,5,14,14,6,6,4,3};
  GAMS_UNUSED(pgmo)
  GAMS_UNUSED(si)
  GAMS_UNUSED(xmin)
  GAMS_UNUSED(xmax)
  GAMS_UNUSED(fmin)
  GAMS_UNUSED(fmax)
  GAMS_UNUSED(gmin)
  GAMS_UNUSED(gmax)
  GAMS_UNUSED(numerr)
  GAMS_UNUSED(tidx)
  printAndReturn(gmoEvalGradInterval_MT,9,int )
}

/** Evaluate all nonlinear gradients and return change vector plus optional update of Jacobians
 * @param pgmo gmo object handle
 * @param rhsdelta Taylor expansion constants
 * @param dojacupd Flag whether to update Jacobians
 * @param numerr Number of errors evaluating the nonlinear function
 */
int  GMO_CALLCONV d_gmoEvalGradNLUpdate (gmoHandle_t pgmo, double rhsdelta[], int dojacupd, int *numerr)
{
  int d_s[]={3,6,15,4};
  GAMS_UNUSED(pgmo)
  GAMS_UNUSED(rhsdelta)
  GAMS_UNUSED(dojacupd)
  GAMS_UNUSED(numerr)
  printAndReturn(gmoEvalGradNLUpdate,3,int )
}

/** Retrieve the updated Jacobian elements
 * @param pgmo gmo object handle
 * @param rowidx Row index/indices of Jacobian
 * @param colidx Column index/indices of Jacobian(s)
 * @param jacval Value(s) of Jacobian(s)
 * @param len Length of array
 */
int  GMO_CALLCONV d_gmoGetJacUpdate (gmoHandle_t pgmo, int rowidx[], int colidx[], double jacval[], int *len)
{
  int d_s[]={3,8,8,6,21};
  GAMS_UNUSED(pgmo)
  GAMS_UNUSED(rowidx)
  GAMS_UNUSED(colidx)
  GAMS_UNUSED(jacval)
  GAMS_UNUSED(len)
  printAndReturn(gmoGetJacUpdate,4,int )
}

/** Initialize Hessians
 * @param pgmo gmo object handle
 * @param maxJacMult Multiplier to define memory limit for Hessian (0=no limit)
 * @param do2dir Flag whether 2nd derivatives are wanted/available
 * @param doHess Flag whether Hessians are wanted/available
 */
int  GMO_CALLCONV d_gmoHessLoad (gmoHandle_t pgmo, double maxJacMult, int *do2dir, int *doHess)
{
  int d_s[]={3,13,21,21};
  GAMS_UNUSED(pgmo)
  GAMS_UNUSED(maxJacMult)
  GAMS_UNUSED(do2dir)
  GAMS_UNUSED(doHess)
  printAndReturn(gmoHessLoad,3,int )
}

/** Unload Hessians
 * @param pgmo gmo object handle
 */
int  GMO_CALLCONV d_gmoHessUnload (gmoHandle_t pgmo)
{
  int d_s[]={3};
  GAMS_UNUSED(pgmo)
  printAndReturn(gmoHessUnload,0,int )
}

/** Hessian dimension of row
 * @param pgmo gmo object handle
 * @param si Index of row in client space
 */
int  GMO_CALLCONV d_gmoHessDim (gmoHandle_t pgmo, int si)
{
  int d_s[]={3,3};
  GAMS_UNUSED(pgmo)
  GAMS_UNUSED(si)
  printAndReturn(gmoHessDim,1,int )
}

/** Hessian nonzeros of row
 * @param pgmo gmo object handle
 * @param si Index of row in client space
 */
int  GMO_CALLCONV d_gmoHessNz (gmoHandle_t pgmo, int si)
{
  int d_s[]={3,3};
  GAMS_UNUSED(pgmo)
  GAMS_UNUSED(si)
  printAndReturn(gmoHessNz,1,int )
}

/** Hessian nonzeros of row
 * @param pgmo gmo object handle
 * @param si Index of row in client space
 */
INT64  GMO_CALLCONV d_gmoHessNz64 (gmoHandle_t pgmo, int si)
{
  int d_s[]={23,3};
  GAMS_UNUSED(pgmo)
  GAMS_UNUSED(si)
  printAndReturn(gmoHessNz64,1,INT64 )
}

/** Get Hessian Structure
 * @param pgmo gmo object handle
 * @param si Index of row in client space
 * @param hridx Hessian row indices
 * @param hcidx Hessian column indices
 * @param hessdim Dimension of Hessian
 * @param hessnz Number of nonzeros in Hessian
 */
int  GMO_CALLCONV d_gmoHessStruct (gmoHandle_t pgmo, int si, int hridx[], int hcidx[], int *hessdim, int *hessnz)
{
  int d_s[]={3,3,8,8,4,4};
  GAMS_UNUSED(pgmo)
  GAMS_UNUSED(si)
  GAMS_UNUSED(hridx)
  GAMS_UNUSED(hcidx)
  GAMS_UNUSED(hessdim)
  GAMS_UNUSED(hessnz)
  printAndReturn(gmoHessStruct,5,int )
}

/** Get Hessian Structure
 * @param pgmo gmo object handle
 * @param si Index of row in client space
 * @param hridx Hessian row indices
 * @param hcidx Hessian column indices
 * @param hessdim Dimension of Hessian
 * @param hessnz Number of nonzeros in Hessian
 */
int  GMO_CALLCONV d_gmoHessStruct64 (gmoHandle_t pgmo, int si, int hridx[], int hcidx[], int *hessdim, INT64 *hessnz)
{
  int d_s[]={3,3,8,8,4,25};
  GAMS_UNUSED(pgmo)
  GAMS_UNUSED(si)
  GAMS_UNUSED(hridx)
  GAMS_UNUSED(hcidx)
  GAMS_UNUSED(hessdim)
  GAMS_UNUSED(hessnz)
  printAndReturn(gmoHessStruct64,5,int )
}

/** Get Hessian Value
 * @param pgmo gmo object handle
 * @param si Index of row in client space
 * @param hridx Hessian row indices
 * @param hcidx Hessian column indices
 * @param hessdim Dimension of Hessian
 * @param hessnz Number of nonzeros in Hessian
 * @param x Level values of variables
 * @param hessval 
 * @param numerr Number of errors evaluating the nonlinear function
 */
int  GMO_CALLCONV d_gmoHessValue (gmoHandle_t pgmo, int si, int hridx[], int hcidx[], int *hessdim, int *hessnz, const double x[], double hessval[], int *numerr)
{
  int d_s[]={3,3,8,8,4,4,5,6,4};
  GAMS_UNUSED(pgmo)
  GAMS_UNUSED(si)
  GAMS_UNUSED(hridx)
  GAMS_UNUSED(hcidx)
  GAMS_UNUSED(hessdim)
  GAMS_UNUSED(hessnz)
  GAMS_UNUSED(x)
  GAMS_UNUSED(hessval)
  GAMS_UNUSED(numerr)
  printAndReturn(gmoHessValue,8,int )
}

/** Get Hessian Value
 * @param pgmo gmo object handle
 * @param si Index of row in client space
 * @param hridx Hessian row indices
 * @param hcidx Hessian column indices
 * @param hessdim Dimension of Hessian
 * @param hessnz Number of nonzeros in Hessian
 * @param x Level values of variables
 * @param hessval 
 * @param numerr Number of errors evaluating the nonlinear function
 */
int  GMO_CALLCONV d_gmoHessValue64 (gmoHandle_t pgmo, int si, int hridx[], int hcidx[], int *hessdim, INT64 *hessnz, const double x[], double hessval[], int *numerr)
{
  int d_s[]={3,3,8,8,4,25,5,6,4};
  GAMS_UNUSED(pgmo)
  GAMS_UNUSED(si)
  GAMS_UNUSED(hridx)
  GAMS_UNUSED(hcidx)
  GAMS_UNUSED(hessdim)
  GAMS_UNUSED(hessnz)
  GAMS_UNUSED(x)
  GAMS_UNUSED(hessval)
  GAMS_UNUSED(numerr)
  printAndReturn(gmoHessValue64,8,int )
}

/** Get Hessian-vector product
 * @param pgmo gmo object handle
 * @param si Index of row in client space
 * @param x Level values of variables
 * @param dx Direction in x-space for directional derivative of Lagrangian (W*dx)
 * @param Wdx Directional derivative of the Lagrangian in direction dx (W*dx)
 * @param numerr Number of errors evaluating the nonlinear function
 */
int  GMO_CALLCONV d_gmoHessVec (gmoHandle_t pgmo, int si, const double x[], const double dx[], double Wdx[], int *numerr)
{
  int d_s[]={3,3,5,5,6,4};
  GAMS_UNUSED(pgmo)
  GAMS_UNUSED(si)
  GAMS_UNUSED(x)
  GAMS_UNUSED(dx)
  GAMS_UNUSED(Wdx)
  GAMS_UNUSED(numerr)
  printAndReturn(gmoHessVec,5,int )
}

/** Get Hessian of the Lagrangian Value structure
 * @param pgmo gmo object handle
 * @param WRindex Row indices for the upper triangle of the symmetric matrix W (the Hessian of the Lagrangian), ordered by rows and within rows by columns
 * @param WCindex Col indices for the upper triangle of the symmetric matrix W (the Hessian of the Lagrangian), ordered by rows and within rows by columns
 */
int  GMO_CALLCONV d_gmoHessLagStruct (gmoHandle_t pgmo, int WRindex[], int WCindex[])
{
  int d_s[]={3,8,8};
  GAMS_UNUSED(pgmo)
  GAMS_UNUSED(WRindex)
  GAMS_UNUSED(WCindex)
  printAndReturn(gmoHessLagStruct,2,int )
}

/** Get Hessian of the Lagrangian Value
 * @param pgmo gmo object handle
 * @param x Level values of variables
 * @param pi Marginal values of equations
 * @param w Values for the structural nonzeros of the upper triangle of the symmetric matrix W (the Hessian of the Lagrangian), ordered by rows and within rows by columns
 * @param objweight Weight for objective in Hessian of the Lagrangian (=1 for GAMS convention)
 * @param conweight Weight for constraints in Hessian of the Lagrangian (=1 for GAMS convention)
 * @param numerr Number of errors evaluating the nonlinear function
 */
int  GMO_CALLCONV d_gmoHessLagValue (gmoHandle_t pgmo, const double x[], const double pi[], double w[], double objweight, double conweight, int *numerr)
{
  int d_s[]={3,5,5,6,13,13,4};
  GAMS_UNUSED(pgmo)
  GAMS_UNUSED(x)
  GAMS_UNUSED(pi)
  GAMS_UNUSED(w)
  GAMS_UNUSED(objweight)
  GAMS_UNUSED(conweight)
  GAMS_UNUSED(numerr)
  printAndReturn(gmoHessLagValue,6,int )
}

/** Get Hessian of the Lagrangian-vector product
 * @param pgmo gmo object handle
 * @param x Level values of variables
 * @param pi Marginal values of equations
 * @param dx Direction in x-space for directional derivative of Lagrangian (W*dx)
 * @param Wdx Directional derivative of the Lagrangian in direction dx (W*dx)
 * @param objweight Weight for objective in Hessian of the Lagrangian (=1 for GAMS convention)
 * @param conweight Weight for constraints in Hessian of the Lagrangian (=1 for GAMS convention)
 * @param numerr Number of errors evaluating the nonlinear function
 */
int  GMO_CALLCONV d_gmoHessLagVec (gmoHandle_t pgmo, const double x[], const double pi[], const double dx[], double Wdx[], double objweight, double conweight, int *numerr)
{
  int d_s[]={3,5,5,5,6,13,13,4};
  GAMS_UNUSED(pgmo)
  GAMS_UNUSED(x)
  GAMS_UNUSED(pi)
  GAMS_UNUSED(dx)
  GAMS_UNUSED(Wdx)
  GAMS_UNUSED(objweight)
  GAMS_UNUSED(conweight)
  GAMS_UNUSED(numerr)
  printAndReturn(gmoHessLagVec,7,int )
}

/** Load EMP information
 * @param pgmo gmo object handle
 * @param empinfofname Name of EMP information file, if empty assume the default name and location
 */
int  GMO_CALLCONV d_gmoLoadEMPInfo (gmoHandle_t pgmo, const char *empinfofname)
{
  int d_s[]={3,11};
  GAMS_UNUSED(pgmo)
  GAMS_UNUSED(empinfofname)
  printAndReturn(gmoLoadEMPInfo,1,int )
}

/** Get VI mapping for all rows (-1 if not a VI function)
 * @param pgmo gmo object handle
 * @param mintvec Array of integers, len=number of rows in user view
 */
int  GMO_CALLCONV d_gmoGetEquVI (gmoHandle_t pgmo, int mintvec[])
{
  int d_s[]={3,8};
  GAMS_UNUSED(pgmo)
  GAMS_UNUSED(mintvec)
  printAndReturn(gmoGetEquVI,1,int )
}

/** Get VI mapping for individual row (-1 if not a VI function)
 * @param pgmo gmo object handle
 * @param si Index of row in client space
 */
int  GMO_CALLCONV d_gmoGetEquVIOne (gmoHandle_t pgmo, int si)
{
  int d_s[]={3,3};
  GAMS_UNUSED(pgmo)
  GAMS_UNUSED(si)
  printAndReturn(gmoGetEquVIOne,1,int )
}

/** Get VI mapping for all cols (-1 if not a VI variable)
 * @param pgmo gmo object handle
 * @param nintvec Array of integers, len=number of columns in user view
 */
int  GMO_CALLCONV d_gmoGetVarVI (gmoHandle_t pgmo, int nintvec[])
{
  int d_s[]={3,8};
  GAMS_UNUSED(pgmo)
  GAMS_UNUSED(nintvec)
  printAndReturn(gmoGetVarVI,1,int )
}

/** Get VI mapping for individual cols (-1 if not a VI variable)
 * @param pgmo gmo object handle
 * @param sj Index of column in client space
 */
int  GMO_CALLCONV d_gmoGetVarVIOne (gmoHandle_t pgmo, int sj)
{
  int d_s[]={3,3};
  GAMS_UNUSED(pgmo)
  GAMS_UNUSED(sj)
  printAndReturn(gmoGetVarVIOne,1,int )
}

/** Get Agent Type of all agent (see gmoNumAgents)
 * @param pgmo gmo object handle
 * @param agentvec Array of agent types of length gmoNumAgents
 */
int  GMO_CALLCONV d_gmoGetAgentType (gmoHandle_t pgmo, int agentvec[])
{
  int d_s[]={3,8};
  GAMS_UNUSED(pgmo)
  GAMS_UNUSED(agentvec)
  printAndReturn(gmoGetAgentType,1,int )
}

/** Get Agent Type of agent
 * @param pgmo gmo object handle
 * @param aidx Index of agent
 */
int  GMO_CALLCONV d_gmoGetAgentTypeOne (gmoHandle_t pgmo, int aidx)
{
  int d_s[]={3,3};
  GAMS_UNUSED(pgmo)
  GAMS_UNUSED(aidx)
  printAndReturn(gmoGetAgentTypeOne,1,int )
}

/** Get equation and variable mapping to agents
 * @param pgmo gmo object handle
 * @param nintvec Array of integers, len=number of columns in user view
 * @param mintvec Array of integers, len=number of rows in user view
 */
int  GMO_CALLCONV d_gmoGetBiLevelInfo (gmoHandle_t pgmo, int nintvec[], int mintvec[])
{
  int d_s[]={3,8,8};
  GAMS_UNUSED(pgmo)
  GAMS_UNUSED(nintvec)
  GAMS_UNUSED(mintvec)
  printAndReturn(gmoGetBiLevelInfo,2,int )
}

/** Dump EMPInfo GDX File
 * @param pgmo gmo object handle
 * @param gdxfname Name of GDX file
 */
int  GMO_CALLCONV d_gmoDumpEMPInfoToGDX (gmoHandle_t pgmo, const char *gdxfname)
{
  int d_s[]={3,11};
  GAMS_UNUSED(pgmo)
  GAMS_UNUSED(gdxfname)
  printAndReturn(gmoDumpEMPInfoToGDX,1,int )
}

/** Get value of solution head or tail record, except for modelstat and solvestat (see enumerated constants)
 * @param pgmo gmo object handle
 * @param htrec Solution head or tail record, (see enumerated constants)
 */
double  GMO_CALLCONV d_gmoGetHeadnTail (gmoHandle_t pgmo, int htrec)
{
  int d_s[]={13,3};
  GAMS_UNUSED(pgmo)
  GAMS_UNUSED(htrec)
  printAndReturn(gmoGetHeadnTail,1,double )
}

/** Set value of solution head or tail record, except for modelstat and solvestat (see enumerated constants)
 * @param pgmo gmo object handle
 * @param htrec Solution head or tail record, (see enumerated constants)
 * @param dval Double value
 */
void  GMO_CALLCONV d_gmoSetHeadnTail (gmoHandle_t pgmo, int htrec, double dval)
{
  int d_s[]={0,3,13};
  GAMS_UNUSED(pgmo)
  GAMS_UNUSED(htrec)
  GAMS_UNUSED(dval)
  printNoReturn(gmoSetHeadnTail,2)
}

/** Set solution values for variable levels
 * @param pgmo gmo object handle
 * @param x Level values of variables
 */
int  GMO_CALLCONV d_gmoSetSolutionPrimal (gmoHandle_t pgmo, const double x[])
{
  int d_s[]={3,5};
  GAMS_UNUSED(pgmo)
  GAMS_UNUSED(x)
  printAndReturn(gmoSetSolutionPrimal,1,int )
}

/** Set solution values for variable levels and equation marginals
 * @param pgmo gmo object handle
 * @param x Level values of variables
 * @param pi Marginal values of equations
 */
int  GMO_CALLCONV d_gmoSetSolution2 (gmoHandle_t pgmo, const double x[], const double pi[])
{
  int d_s[]={3,5,5};
  GAMS_UNUSED(pgmo)
  GAMS_UNUSED(x)
  GAMS_UNUSED(pi)
  printAndReturn(gmoSetSolution2,2,int )
}

/** Set solution values for variable and equation levels as well as marginals
 * @param pgmo gmo object handle
 * @param x Level values of variables
 * @param dj Marginal values of variables
 * @param pi Marginal values of equations
 * @param e Level values of equations
 */
int  GMO_CALLCONV d_gmoSetSolution (gmoHandle_t pgmo, const double x[], const double dj[], const double pi[], const double e[])
{
  int d_s[]={3,5,5,5,5};
  GAMS_UNUSED(pgmo)
  GAMS_UNUSED(x)
  GAMS_UNUSED(dj)
  GAMS_UNUSED(pi)
  GAMS_UNUSED(e)
  printAndReturn(gmoSetSolution,4,int )
}

/** Set solution values for variable and equation levels, marginals and statuses
 * @param pgmo gmo object handle
 * @param x Level values of variables
 * @param dj Marginal values of variables
 * @param pi Marginal values of equations
 * @param e Level values of equations
 * @param xb Basis statuses of variables (see enumerated constants)
 * @param xs Statuses of variables (see enumerated constants)
 * @param yb Basis statuses of equations (see enumerated constants)
 * @param ys Statuses of equation (see enumerated constants)
 */
int  GMO_CALLCONV d_gmoSetSolution8 (gmoHandle_t pgmo, const double x[], const double dj[], const double pi[], const double e[], int xb[], int xs[], int yb[], int ys[])
{
  int d_s[]={3,5,5,5,5,8,8,8,8};
  GAMS_UNUSED(pgmo)
  GAMS_UNUSED(x)
  GAMS_UNUSED(dj)
  GAMS_UNUSED(pi)
  GAMS_UNUSED(e)
  GAMS_UNUSED(xb)
  GAMS_UNUSED(xs)
  GAMS_UNUSED(yb)
  GAMS_UNUSED(ys)
  printAndReturn(gmoSetSolution8,8,int )
}

/** Construct and set solution based on available inputs
 * @param pgmo gmo object handle
 * @param modelstathint Model status used as a hint
 * @param x Level values of variables
 * @param pi Marginal values of equations
 * @param xb Basis statuses of variables (see enumerated constants)
 * @param yb Basis statuses of equations (see enumerated constants)
 * @param infTol Infeasibility tolerance
 * @param optTol Optimality tolerance
 */
int  GMO_CALLCONV d_gmoSetSolutionFixer (gmoHandle_t pgmo, int modelstathint, const double x[], const double pi[], const int xb[], const int yb[], double infTol, double optTol)
{
  int d_s[]={3,3,5,5,7,7,13,13};
  GAMS_UNUSED(pgmo)
  GAMS_UNUSED(modelstathint)
  GAMS_UNUSED(x)
  GAMS_UNUSED(pi)
  GAMS_UNUSED(xb)
  GAMS_UNUSED(yb)
  GAMS_UNUSED(infTol)
  GAMS_UNUSED(optTol)
  printAndReturn(gmoSetSolutionFixer,7,int )
}

/** Get variable solution values (level, marginals and statuses)
 * @param pgmo gmo object handle
 * @param sj Index of column in client space
 * @param vl Level of variable
 * @param vmarg Marginal of variable
 * @param vstat Basis status of variable (see enumerated constants)
 * @param vcstat Status of variable (see enumerated constants)
 */
int  GMO_CALLCONV d_gmoGetSolutionVarRec (gmoHandle_t pgmo, int sj, double *vl, double *vmarg, int *vstat, int *vcstat)
{
  int d_s[]={3,3,14,14,4,4};
  GAMS_UNUSED(pgmo)
  GAMS_UNUSED(sj)
  GAMS_UNUSED(vl)
  GAMS_UNUSED(vmarg)
  GAMS_UNUSED(vstat)
  GAMS_UNUSED(vcstat)
  printAndReturn(gmoGetSolutionVarRec,5,int )
}

/** Set variable solution values (level, marginals and statuses)
 * @param pgmo gmo object handle
 * @param sj Index of column in client space
 * @param vl Level of variable
 * @param vmarg Marginal of variable
 * @param vstat Basis status of variable (see enumerated constants)
 * @param vcstat Status of variable (see enumerated constants)
 */
int  GMO_CALLCONV d_gmoSetSolutionVarRec (gmoHandle_t pgmo, int sj, double vl, double vmarg, int vstat, int vcstat)
{
  int d_s[]={3,3,13,13,3,3};
  GAMS_UNUSED(pgmo)
  GAMS_UNUSED(sj)
  GAMS_UNUSED(vl)
  GAMS_UNUSED(vmarg)
  GAMS_UNUSED(vstat)
  GAMS_UNUSED(vcstat)
  printAndReturn(gmoSetSolutionVarRec,5,int )
}

/** Get equation solution values (level, marginals and statuses)
 * @param pgmo gmo object handle
 * @param si Index of row in client space
 * @param el Level of equation
 * @param emarg Marginal of equation
 * @param estat Basis status of equation (see enumerated constants)
 * @param ecstat Status of equation (see enumerated constants)
 */
int  GMO_CALLCONV d_gmoGetSolutionEquRec (gmoHandle_t pgmo, int si, double *el, double *emarg, int *estat, int *ecstat)
{
  int d_s[]={3,3,14,14,4,4};
  GAMS_UNUSED(pgmo)
  GAMS_UNUSED(si)
  GAMS_UNUSED(el)
  GAMS_UNUSED(emarg)
  GAMS_UNUSED(estat)
  GAMS_UNUSED(ecstat)
  printAndReturn(gmoGetSolutionEquRec,5,int )
}

/** Set equation solution values (level, marginals and statuses)
 * @param pgmo gmo object handle
 * @param si Index of row in client space
 * @param el Level of equation
 * @param emarg Marginal of equation
 * @param estat Basis status of equation (see enumerated constants)
 * @param ecstat Status of equation (see enumerated constants)
 */
int  GMO_CALLCONV d_gmoSetSolutionEquRec (gmoHandle_t pgmo, int si, double el, double emarg, int estat, int ecstat)
{
  int d_s[]={3,3,13,13,3,3};
  GAMS_UNUSED(pgmo)
  GAMS_UNUSED(si)
  GAMS_UNUSED(el)
  GAMS_UNUSED(emarg)
  GAMS_UNUSED(estat)
  GAMS_UNUSED(ecstat)
  printAndReturn(gmoSetSolutionEquRec,5,int )
}

/** Set solution values sfor variable and equation statuses
 * @param pgmo gmo object handle
 * @param xb Basis statuses of variables (see enumerated constants)
 * @param xs Statuses of variables (see enumerated constants)
 * @param yb Basis statuses of equations (see enumerated constants)
 * @param ys Statuses of equation (see enumerated constants)
 */
int  GMO_CALLCONV d_gmoSetSolutionStatus (gmoHandle_t pgmo, int xb[], int xs[], int yb[], int ys[])
{
  int d_s[]={3,8,8,8,8};
  GAMS_UNUSED(pgmo)
  GAMS_UNUSED(xb)
  GAMS_UNUSED(xs)
  GAMS_UNUSED(yb)
  GAMS_UNUSED(ys)
  printAndReturn(gmoSetSolutionStatus,4,int )
}

/** Complete objective row/col for models with objective function
 * @param pgmo gmo object handle
 * @param locobjval Objective value
 */
void  GMO_CALLCONV d_gmoCompleteObjective (gmoHandle_t pgmo, double locobjval)
{
  int d_s[]={0,13};
  GAMS_UNUSED(pgmo)
  GAMS_UNUSED(locobjval)
  printNoReturn(gmoCompleteObjective,1)
}

/** Complete solution (e.g. for cols/rows not in view)
 * @param pgmo gmo object handle
 */
int  GMO_CALLCONV d_gmoCompleteSolution (gmoHandle_t pgmo)
{
  int d_s[]={3};
  GAMS_UNUSED(pgmo)
  printAndReturn(gmoCompleteSolution,0,int )
}

/** Compute absolute gap w.r.t. objective value and objective estimate in head or tail records
 * @param pgmo gmo object handle
 */
double  GMO_CALLCONV d_gmoGetAbsoluteGap (gmoHandle_t pgmo)
{
  int d_s[]={13};
  GAMS_UNUSED(pgmo)
  printAndReturn(gmoGetAbsoluteGap,0,double )
}

/** Compute relative gap w.r.t. objective value and objective estimate in head or tail records
 * @param pgmo gmo object handle
 */
double  GMO_CALLCONV d_gmoGetRelativeGap (gmoHandle_t pgmo)
{
  int d_s[]={13};
  GAMS_UNUSED(pgmo)
  printAndReturn(gmoGetRelativeGap,0,double )
}

/** Load solution from legacy solution file
 * @param pgmo gmo object handle
 */
int  GMO_CALLCONV d_gmoLoadSolutionLegacy (gmoHandle_t pgmo)
{
  int d_s[]={3};
  GAMS_UNUSED(pgmo)
  printAndReturn(gmoLoadSolutionLegacy,0,int )
}

/** Unload solution to legacy solution file
 * @param pgmo gmo object handle
 */
int  GMO_CALLCONV d_gmoUnloadSolutionLegacy (gmoHandle_t pgmo)
{
  int d_s[]={3};
  GAMS_UNUSED(pgmo)
  printAndReturn(gmoUnloadSolutionLegacy,0,int )
}

/** Load solution to GDX solution file (optional: rows, cols and-or header and tail info)
 * @param pgmo gmo object handle
 * @param gdxfname Name of GDX file
 * @param dorows Flag whether to read/write row information from/to solution file
 * @param docols Flag whether to read/write column information from/to solution file
 * @param doht Flag whether to read/write head and tail information from/to solution file
 */
int  GMO_CALLCONV d_gmoLoadSolutionGDX (gmoHandle_t pgmo, const char *gdxfname, int dorows, int docols, int doht)
{
  int d_s[]={3,11,15,15,15};
  GAMS_UNUSED(pgmo)
  GAMS_UNUSED(gdxfname)
  GAMS_UNUSED(dorows)
  GAMS_UNUSED(docols)
  GAMS_UNUSED(doht)
  printAndReturn(gmoLoadSolutionGDX,4,int )
}

/** Unload solution to GDX solution file (optional: rows, cols and-or header and tail info)
 * @param pgmo gmo object handle
 * @param gdxfname Name of GDX file
 * @param dorows Flag whether to read/write row information from/to solution file
 * @param docols Flag whether to read/write column information from/to solution file
 * @param doht Flag whether to read/write head and tail information from/to solution file
 */
int  GMO_CALLCONV d_gmoUnloadSolutionGDX (gmoHandle_t pgmo, const char *gdxfname, int dorows, int docols, int doht)
{
  int d_s[]={3,11,15,15,15};
  GAMS_UNUSED(pgmo)
  GAMS_UNUSED(gdxfname)
  GAMS_UNUSED(dorows)
  GAMS_UNUSED(docols)
  GAMS_UNUSED(doht)
  printAndReturn(gmoUnloadSolutionGDX,4,int )
}

/** Initialize writing of multiple solutions (e.g. scenarios) to a GDX file
 * @param pgmo gmo object handle
 * @param gdxfname Name of GDX file
 * @param scengdx Pointer to GDX solution file containing multiple solutions, e.g. scenarios
 * @param dictid GDX symbol number of dict
 */
int  GMO_CALLCONV d_gmoPrepareAllSolToGDX (gmoHandle_t pgmo, const char *gdxfname, void *scengdx, int dictid)
{
  int d_s[]={3,11,1,3};
  GAMS_UNUSED(pgmo)
  GAMS_UNUSED(gdxfname)
  GAMS_UNUSED(scengdx)
  GAMS_UNUSED(dictid)
  printAndReturn(gmoPrepareAllSolToGDX,3,int )
}

/** Add a solution (e.g. scenario) to the GDX file'
 * @param pgmo gmo object handle
 * @param scenuel Scenario labels
 */
int  GMO_CALLCONV d_gmoAddSolutionToGDX (gmoHandle_t pgmo, const char *scenuel[])
{
  int d_s[]={3,55};
  GAMS_UNUSED(pgmo)
  GAMS_UNUSED(scenuel)
  printAndReturn(gmoAddSolutionToGDX,1,int )
}

/** Finalize writing of multiple solutions (e.g. scenarios) to a GDX file
 * @param pgmo gmo object handle
 * @param msg Message
 */
int  GMO_CALLCONV d_gmoWriteSolDone (gmoHandle_t pgmo, char *msg)
{
  int d_s[]={3,12};
  GAMS_UNUSED(pgmo)
  GAMS_UNUSED(msg)
  printAndReturn(gmoWriteSolDone,1,int )
}

/** heck scenario UEL against dictionary uels and report number of varaible symbols
 * @param pgmo gmo object handle
 * @param prefix 
 * @param numsym Number of symbols
 */
int  GMO_CALLCONV d_gmoCheckSolPoolUEL (gmoHandle_t pgmo, const char *prefix, int *numsym)
{
  int d_s[]={3,11,21};
  GAMS_UNUSED(pgmo)
  GAMS_UNUSED(prefix)
  GAMS_UNUSED(numsym)
  printAndReturn(gmoCheckSolPoolUEL,2,int )
}

/** Prepare merged solution pool GDX file
 * @param pgmo gmo object handle
 * @param gdxfname Name of GDX file
 * @param numsol Number of solutions in solution pool
 * @param prefix 
 */
void * GMO_CALLCONV d_gmoPrepareSolPoolMerge (gmoHandle_t pgmo, const char *gdxfname, int numsol, const char *prefix)
{
  int d_s[]={1,11,3,11};
  GAMS_UNUSED(pgmo)
  GAMS_UNUSED(gdxfname)
  GAMS_UNUSED(numsol)
  GAMS_UNUSED(prefix)
  printAndReturn(gmoPrepareSolPoolMerge,3,void *)
}

/** Write solution to merged solution pool GDX file
 * @param pgmo gmo object handle
 * @param handle 
 */
int  GMO_CALLCONV d_gmoPrepareSolPoolNextSym (gmoHandle_t pgmo, void *handle)
{
  int d_s[]={3,1};
  GAMS_UNUSED(pgmo)
  GAMS_UNUSED(handle)
  printAndReturn(gmoPrepareSolPoolNextSym,1,int )
}

/** Write solution to merged solution pool GDX file
 * @param pgmo gmo object handle
 * @param handle 
 * @param numsol Number of solutions in solution pool
 */
int  GMO_CALLCONV d_gmoUnloadSolPoolSolution (gmoHandle_t pgmo, void *handle, int numsol)
{
  int d_s[]={3,1,3};
  GAMS_UNUSED(pgmo)
  GAMS_UNUSED(handle)
  GAMS_UNUSED(numsol)
  printAndReturn(gmoUnloadSolPoolSolution,2,int )
}

/** Finalize merged solution pool GDX file
 * @param pgmo gmo object handle
 * @param handle 
 */
int  GMO_CALLCONV d_gmoFinalizeSolPoolMerge (gmoHandle_t pgmo, void *handle)
{
  int d_s[]={3,1};
  GAMS_UNUSED(pgmo)
  GAMS_UNUSED(handle)
  printAndReturn(gmoFinalizeSolPoolMerge,1,int )
}

/** String for variable type
 * @param pgmo gmo object handle
 * @param sj Index of column in client space
 * @param s String
 */
int  GMO_CALLCONV d_gmoGetVarTypeTxt (gmoHandle_t pgmo, int sj, char *s)
{
  int d_s[]={3,3,12};
  GAMS_UNUSED(pgmo)
  GAMS_UNUSED(sj)
  GAMS_UNUSED(s)
  printAndReturn(gmoGetVarTypeTxt,2,int )
}

/** String for equation type
 * @param pgmo gmo object handle
 * @param si Index of row in client space
 * @param s String
 */
int  GMO_CALLCONV d_gmoGetEquTypeTxt (gmoHandle_t pgmo, int si, char *s)
{
  int d_s[]={3,3,12};
  GAMS_UNUSED(pgmo)
  GAMS_UNUSED(si)
  GAMS_UNUSED(s)
  printAndReturn(gmoGetEquTypeTxt,2,int )
}

/** String for solvestatus
 * @param pgmo gmo object handle
 * @param solvestat Solver status
 * @param s String
 */
int  GMO_CALLCONV d_gmoGetSolveStatusTxt (gmoHandle_t pgmo, int solvestat, char *s)
{
  int d_s[]={3,3,12};
  GAMS_UNUSED(pgmo)
  GAMS_UNUSED(solvestat)
  GAMS_UNUSED(s)
  printAndReturn(gmoGetSolveStatusTxt,2,int )
}

/** String for modelstatus
 * @param pgmo gmo object handle
 * @param modelstat Model status
 * @param s String
 */
int  GMO_CALLCONV d_gmoGetModelStatusTxt (gmoHandle_t pgmo, int modelstat, char *s)
{
  int d_s[]={3,3,12};
  GAMS_UNUSED(pgmo)
  GAMS_UNUSED(modelstat)
  GAMS_UNUSED(s)
  printAndReturn(gmoGetModelStatusTxt,2,int )
}

/** String for modeltype
 * @param pgmo gmo object handle
 * @param modeltype 
 * @param s String
 */
int  GMO_CALLCONV d_gmoGetModelTypeTxt (gmoHandle_t pgmo, int modeltype, char *s)
{
  int d_s[]={3,3,12};
  GAMS_UNUSED(pgmo)
  GAMS_UNUSED(modeltype)
  GAMS_UNUSED(s)
  printAndReturn(gmoGetModelTypeTxt,2,int )
}

/** String for solution head or tail record
 * @param pgmo gmo object handle
 * @param htrec Solution head or tail record, (see enumerated constants)
 * @param s String
 */
int  GMO_CALLCONV d_gmoGetHeadNTailTxt (gmoHandle_t pgmo, int htrec, char *s)
{
  int d_s[]={3,3,12};
  GAMS_UNUSED(pgmo)
  GAMS_UNUSED(htrec)
  GAMS_UNUSED(s)
  printAndReturn(gmoGetHeadNTailTxt,2,int )
}

/** Get current memory consumption of GMO in MB
 * @param pgmo gmo object handle
 */
double  GMO_CALLCONV d_gmoMemUsed (gmoHandle_t pgmo)
{
  int d_s[]={13};
  GAMS_UNUSED(pgmo)
  printAndReturn(gmoMemUsed,0,double )
}

/** Get peak memory consumption of GMO in MB
 * @param pgmo gmo object handle
 */
double  GMO_CALLCONV d_gmoPeakMemUsed (gmoHandle_t pgmo)
{
  int d_s[]={13};
  GAMS_UNUSED(pgmo)
  printAndReturn(gmoPeakMemUsed,0,double )
}

/** Set NL Object and constant pool
 * @param pgmo gmo object handle
 * @param nlobject Object of nonlinear instructions
 * @param nlpool Constant pool object for constants in nonlinear instruction
 */
int  GMO_CALLCONV d_gmoSetNLObject (gmoHandle_t pgmo, void *nlobject, void *nlpool)
{
  int d_s[]={3,1,1};
  GAMS_UNUSED(pgmo)
  GAMS_UNUSED(nlobject)
  GAMS_UNUSED(nlpool)
  printAndReturn(gmoSetNLObject,2,int )
}

/** Dump QMaker GDX File
 * @param pgmo gmo object handle
 * @param gdxfname Name of GDX file
 */
int  GMO_CALLCONV d_gmoDumpQMakerGDX (gmoHandle_t pgmo, const char *gdxfname)
{
  int d_s[]={3,11};
  GAMS_UNUSED(pgmo)
  GAMS_UNUSED(gdxfname)
  printAndReturn(gmoDumpQMakerGDX,1,int )
}

/** Get variable equation mapping list
 * @param pgmo gmo object handle
 * @param maptype Type of variable equation mapping
 * @param optptr Option object pointer
 * @param strict 
 * @param nmappings 
 * @param rowindex 
 * @param colindex 
 * @param mapval 
 */
int  GMO_CALLCONV d_gmoGetVarEquMap (gmoHandle_t pgmo, int maptype, void *optptr, int strict, int *nmappings, int rowindex[], int colindex[], int mapval[])
{
  int d_s[]={3,3,1,3,21,8,8,8};
  GAMS_UNUSED(pgmo)
  GAMS_UNUSED(maptype)
  GAMS_UNUSED(optptr)
  GAMS_UNUSED(strict)
  GAMS_UNUSED(nmappings)
  GAMS_UNUSED(rowindex)
  GAMS_UNUSED(colindex)
  GAMS_UNUSED(mapval)
  printAndReturn(gmoGetVarEquMap,7,int )
}

/** Get indicator constraint list
 * @param pgmo gmo object handle
 * @param optptr Option object pointer
 * @param indicstrict 1: Make the indicator reading strict. 0: accept duplicates, unmatched vars and equs, etc
 * @param numindic Number of indicator constraints
 * @param rowindic map with row indicies
 * @param colindic map with column indicies
 * @param indiconval 0 or 1 value for binary variable to activate the constraint
 */
int  GMO_CALLCONV d_gmoGetIndicatorMap (gmoHandle_t pgmo, void *optptr, int indicstrict, int *numindic, int rowindic[], int colindic[], int indiconval[])
{
  int d_s[]={3,1,3,21,8,8,8};
  GAMS_UNUSED(pgmo)
  GAMS_UNUSED(optptr)
  GAMS_UNUSED(indicstrict)
  GAMS_UNUSED(numindic)
  GAMS_UNUSED(rowindic)
  GAMS_UNUSED(colindic)
  GAMS_UNUSED(indiconval)
  printAndReturn(gmoGetIndicatorMap,6,int )
}

/** mature = 0 ... 100 = crude/not secure evaluations (non-GAMS evaluators)
 * @param pgmo gmo object handle
 */
int  GMO_CALLCONV d_gmoCrudeness (gmoHandle_t pgmo)
{
  int d_s[]={3};
  GAMS_UNUSED(pgmo)
  printAndReturn(gmoCrudeness,0,int )
}

/** Temporary function to get row function only code, do not use it
 * @param pgmo gmo object handle
 * @param si Index of row in client space
 * @param len Length of array
 * @param opcode Nonlinear code operation
 * @param field Nonlinear code field
 */
int  GMO_CALLCONV d_gmoDirtyGetRowFNLInstr (gmoHandle_t pgmo, int si, int *len, int opcode[], int field[])
{
  int d_s[]={3,3,4,8,8};
  GAMS_UNUSED(pgmo)
  GAMS_UNUSED(si)
  GAMS_UNUSED(len)
  GAMS_UNUSED(opcode)
  GAMS_UNUSED(field)
  printAndReturn(gmoDirtyGetRowFNLInstr,4,int )
}

/** Temporary function to get row function only code, do not use it
 * @param pgmo gmo object handle
 * @param len Length of array
 * @param opcode Nonlinear code operation
 * @param field Nonlinear code field
 */
int  GMO_CALLCONV d_gmoDirtyGetObjFNLInstr (gmoHandle_t pgmo, int *len, int opcode[], int field[])
{
  int d_s[]={3,4,8,8};
  GAMS_UNUSED(pgmo)
  GAMS_UNUSED(len)
  GAMS_UNUSED(opcode)
  GAMS_UNUSED(field)
  printAndReturn(gmoDirtyGetObjFNLInstr,3,int )
}

/** Temporary function to set row function only code, do not use it
 * @param pgmo gmo object handle
 * @param si Index of row in client space
 * @param len Length of array
 * @param opcode Nonlinear code operation
 * @param field Nonlinear code field
 * @param nlpool Constant pool object for constants in nonlinear instruction
 * @param nlpoolvec Constant pool array for constants in nonlinear instruction
 * @param len2 Length of second array
 */
int  GMO_CALLCONV d_gmoDirtySetRowFNLInstr (gmoHandle_t pgmo, int si, int len, const int opcode[], const int field[], void *nlpool, double nlpoolvec[], int len2)
{
  int d_s[]={3,3,3,7,7,1,6,3};
  GAMS_UNUSED(pgmo)
  GAMS_UNUSED(si)
  GAMS_UNUSED(len)
  GAMS_UNUSED(opcode)
  GAMS_UNUSED(field)
  GAMS_UNUSED(nlpool)
  GAMS_UNUSED(nlpoolvec)
  GAMS_UNUSED(len2)
  printAndReturn(gmoDirtySetRowFNLInstr,7,int )
}

/** Get file name stub of extrinsic function library
 * @param pgmo gmo object handle
 * @param libidx Library index
 */
char * GMO_CALLCONV d_gmoGetExtrLibName (gmoHandle_t pgmo, int libidx, char *buf)
{
  int d_s[]={12,3};
  GAMS_UNUSED(pgmo)
  GAMS_UNUSED(libidx)
  GAMS_UNUSED(buf)
  printAndReturn(gmoGetExtrLibName,1,char *)
}

/** Get data object pointer of extrinsic function library
 * @param pgmo gmo object handle
 * @param libidx Library index
 */
void * GMO_CALLCONV d_gmoGetExtrLibObjPtr (gmoHandle_t pgmo, int libidx)
{
  int d_s[]={1,3};
  GAMS_UNUSED(pgmo)
  GAMS_UNUSED(libidx)
  printAndReturn(gmoGetExtrLibObjPtr,1,void *)
}

/** Get name of extrinsic function
 * @param pgmo gmo object handle
 * @param libidx Library index
 * @param funcidx Function index
 */
char * GMO_CALLCONV d_gmoGetExtrLibFuncName (gmoHandle_t pgmo, int libidx, int funcidx, char *buf)
{
  int d_s[]={12,3,3};
  GAMS_UNUSED(pgmo)
  GAMS_UNUSED(libidx)
  GAMS_UNUSED(funcidx)
  GAMS_UNUSED(buf)
  printAndReturn(gmoGetExtrLibFuncName,2,char *)
}

/** Load a function from an extrinsic function library
 * @param pgmo gmo object handle
 * @param libidx Library index
 * @param name 
 * @param msg Message
 */
void * GMO_CALLCONV d_gmoLoadExtrLibEntry (gmoHandle_t pgmo, int libidx, const char *name, char *msg)
{
  int d_s[]={1,3,11,12};
  GAMS_UNUSED(pgmo)
  GAMS_UNUSED(libidx)
  GAMS_UNUSED(name)
  GAMS_UNUSED(msg)
  printAndReturn(gmoLoadExtrLibEntry,3,void *)
}

/** Load GAMS dictionary object and obtain pointer to it
 * @param pgmo gmo object handle
 */
void * GMO_CALLCONV d_gmoDict (gmoHandle_t pgmo)
{
  int d_s[]={1};
  GAMS_UNUSED(pgmo)
  printAndReturn(gmoDict,0,void *)
}

/** Load GAMS dictionary object and obtain pointer to it
 * @param pgmo gmo object handle
 */
void GMO_CALLCONV d_gmoDictSet (gmoHandle_t pgmo,const void *x)
{
  int d_s[]={0,1};
  GAMS_UNUSED(pgmo)
  GAMS_UNUSED(x)
  printNoReturn(gmoDictSet,1)
}

/** Name of model
 * @param pgmo gmo object handle
 */
char * GMO_CALLCONV d_gmoNameModel (gmoHandle_t pgmo, char *buf)
{
  int d_s[]={12};
  GAMS_UNUSED(pgmo)
  GAMS_UNUSED(buf)
  printAndReturn(gmoNameModel,0,char *)
}

/** Name of model
 * @param pgmo gmo object handle
 */
void GMO_CALLCONV d_gmoNameModelSet (gmoHandle_t pgmo,const char *x)
{
  int d_s[]={0,12};
  GAMS_UNUSED(pgmo)
  GAMS_UNUSED(x)
  printNoReturn(gmoNameModelSet,1)
}

/** Sequence number of model (0..n)
 * @param pgmo gmo object handle
 */
int  GMO_CALLCONV d_gmoModelSeqNr (gmoHandle_t pgmo)
{
  int d_s[]={3};
  GAMS_UNUSED(pgmo)
  printAndReturn(gmoModelSeqNr,0,int )
}

/** Sequence number of model (0..n)
 * @param pgmo gmo object handle
 */
void GMO_CALLCONV d_gmoModelSeqNrSet (gmoHandle_t pgmo,const int x)
{
  int d_s[]={0,3};
  GAMS_UNUSED(pgmo)
  GAMS_UNUSED(x)
  printNoReturn(gmoModelSeqNrSet,1)
}

/** Type of Model
 * @param pgmo gmo object handle
 */
int  GMO_CALLCONV d_gmoModelType (gmoHandle_t pgmo)
{
  int d_s[]={3};
  GAMS_UNUSED(pgmo)
  printAndReturn(gmoModelType,0,int )
}

/** Type of Model
 * @param pgmo gmo object handle
 */
void GMO_CALLCONV d_gmoModelTypeSet (gmoHandle_t pgmo,const int x)
{
  int d_s[]={0,3};
  GAMS_UNUSED(pgmo)
  GAMS_UNUSED(x)
  printNoReturn(gmoModelTypeSet,1)
}

/** Type of Model
 * @param pgmo gmo object handle
 */
int  GMO_CALLCONV d_gmoNLModelType (gmoHandle_t pgmo)
{
  int d_s[]={15};
  GAMS_UNUSED(pgmo)
  printAndReturn(gmoNLModelType,0,int )
}

/** Direction of optimization, see enumerated constants
 * @param pgmo gmo object handle
 */
int  GMO_CALLCONV d_gmoSense (gmoHandle_t pgmo)
{
  int d_s[]={3};
  GAMS_UNUSED(pgmo)
  printAndReturn(gmoSense,0,int )
}

/** Direction of optimization, see enumerated constants
 * @param pgmo gmo object handle
 */
void GMO_CALLCONV d_gmoSenseSet (gmoHandle_t pgmo,const int x)
{
  int d_s[]={0,3};
  GAMS_UNUSED(pgmo)
  GAMS_UNUSED(x)
  printNoReturn(gmoSenseSet,1)
}

/** Is this a QP or not
 * @param pgmo gmo object handle
 */
int  GMO_CALLCONV d_gmoIsQP (gmoHandle_t pgmo)
{
  int d_s[]={15};
  GAMS_UNUSED(pgmo)
  printAndReturn(gmoIsQP,0,int )
}

/** Number of option file
 * @param pgmo gmo object handle
 */
int  GMO_CALLCONV d_gmoOptFile (gmoHandle_t pgmo)
{
  int d_s[]={3};
  GAMS_UNUSED(pgmo)
  printAndReturn(gmoOptFile,0,int )
}

/** Number of option file
 * @param pgmo gmo object handle
 */
void GMO_CALLCONV d_gmoOptFileSet (gmoHandle_t pgmo,const int x)
{
  int d_s[]={0,3};
  GAMS_UNUSED(pgmo)
  GAMS_UNUSED(x)
  printNoReturn(gmoOptFileSet,1)
}

/** Dictionary flag
 * @param pgmo gmo object handle
 */
int  GMO_CALLCONV d_gmoDictionary (gmoHandle_t pgmo)
{
  int d_s[]={3};
  GAMS_UNUSED(pgmo)
  printAndReturn(gmoDictionary,0,int )
}

/** Dictionary flag
 * @param pgmo gmo object handle
 */
void GMO_CALLCONV d_gmoDictionarySet (gmoHandle_t pgmo,const int x)
{
  int d_s[]={0,3};
  GAMS_UNUSED(pgmo)
  GAMS_UNUSED(x)
  printNoReturn(gmoDictionarySet,1)
}

/** Scaling flag
 * @param pgmo gmo object handle
 */
int  GMO_CALLCONV d_gmoScaleOpt (gmoHandle_t pgmo)
{
  int d_s[]={3};
  GAMS_UNUSED(pgmo)
  printAndReturn(gmoScaleOpt,0,int )
}

/** Scaling flag
 * @param pgmo gmo object handle
 */
void GMO_CALLCONV d_gmoScaleOptSet (gmoHandle_t pgmo,const int x)
{
  int d_s[]={0,3};
  GAMS_UNUSED(pgmo)
  GAMS_UNUSED(x)
  printNoReturn(gmoScaleOptSet,1)
}

/** Priority Flag
 * @param pgmo gmo object handle
 */
int  GMO_CALLCONV d_gmoPriorOpt (gmoHandle_t pgmo)
{
  int d_s[]={3};
  GAMS_UNUSED(pgmo)
  printAndReturn(gmoPriorOpt,0,int )
}

/** Priority Flag
 * @param pgmo gmo object handle
 */
void GMO_CALLCONV d_gmoPriorOptSet (gmoHandle_t pgmo,const int x)
{
  int d_s[]={0,3};
  GAMS_UNUSED(pgmo)
  GAMS_UNUSED(x)
  printNoReturn(gmoPriorOptSet,1)
}

/** Do we have basis
 * @param pgmo gmo object handle
 */
int  GMO_CALLCONV d_gmoHaveBasis (gmoHandle_t pgmo)
{
  int d_s[]={3};
  GAMS_UNUSED(pgmo)
  printAndReturn(gmoHaveBasis,0,int )
}

/** Do we have basis
 * @param pgmo gmo object handle
 */
void GMO_CALLCONV d_gmoHaveBasisSet (gmoHandle_t pgmo,const int x)
{
  int d_s[]={0,3};
  GAMS_UNUSED(pgmo)
  GAMS_UNUSED(x)
  printNoReturn(gmoHaveBasisSet,1)
}

/** Model status, see enumerated constants
 * @param pgmo gmo object handle
 */
int  GMO_CALLCONV d_gmoModelStat (gmoHandle_t pgmo)
{
  int d_s[]={3};
  GAMS_UNUSED(pgmo)
  printAndReturn(gmoModelStat,0,int )
}

/** Model status, see enumerated constants
 * @param pgmo gmo object handle
 */
void GMO_CALLCONV d_gmoModelStatSet (gmoHandle_t pgmo,const int x)
{
  int d_s[]={0,3};
  GAMS_UNUSED(pgmo)
  GAMS_UNUSED(x)
  printNoReturn(gmoModelStatSet,1)
}

/** Solver status, see enumerated constants
 * @param pgmo gmo object handle
 */
int  GMO_CALLCONV d_gmoSolveStat (gmoHandle_t pgmo)
{
  int d_s[]={3};
  GAMS_UNUSED(pgmo)
  printAndReturn(gmoSolveStat,0,int )
}

/** Solver status, see enumerated constants
 * @param pgmo gmo object handle
 */
void GMO_CALLCONV d_gmoSolveStatSet (gmoHandle_t pgmo,const int x)
{
  int d_s[]={0,3};
  GAMS_UNUSED(pgmo)
  GAMS_UNUSED(x)
  printNoReturn(gmoSolveStatSet,1)
}

/** Is this an MPSGE model
 * @param pgmo gmo object handle
 */
int  GMO_CALLCONV d_gmoIsMPSGE (gmoHandle_t pgmo)
{
  int d_s[]={15};
  GAMS_UNUSED(pgmo)
  printAndReturn(gmoIsMPSGE,0,int )
}

/** Is this an MPSGE model
 * @param pgmo gmo object handle
 */
void GMO_CALLCONV d_gmoIsMPSGESet (gmoHandle_t pgmo,const int x)
{
  int d_s[]={0,15};
  GAMS_UNUSED(pgmo)
  GAMS_UNUSED(x)
  printNoReturn(gmoIsMPSGESet,1)
}

/** Style of objective, see enumerated constants
 * @param pgmo gmo object handle
 */
int  GMO_CALLCONV d_gmoObjStyle (gmoHandle_t pgmo)
{
  int d_s[]={3};
  GAMS_UNUSED(pgmo)
  printAndReturn(gmoObjStyle,0,int )
}

/** Style of objective, see enumerated constants
 * @param pgmo gmo object handle
 */
void GMO_CALLCONV d_gmoObjStyleSet (gmoHandle_t pgmo,const int x)
{
  int d_s[]={0,3};
  GAMS_UNUSED(pgmo)
  GAMS_UNUSED(x)
  printNoReturn(gmoObjStyleSet,1)
}

/** Interface type (raw vs. processed), see enumerated constants
 * @param pgmo gmo object handle
 */
int  GMO_CALLCONV d_gmoInterface (gmoHandle_t pgmo)
{
  int d_s[]={3};
  GAMS_UNUSED(pgmo)
  printAndReturn(gmoInterface,0,int )
}

/** Interface type (raw vs. processed), see enumerated constants
 * @param pgmo gmo object handle
 */
void GMO_CALLCONV d_gmoInterfaceSet (gmoHandle_t pgmo,const int x)
{
  int d_s[]={0,3};
  GAMS_UNUSED(pgmo)
  GAMS_UNUSED(x)
  printNoReturn(gmoInterfaceSet,1)
}

/** User array index base (0 or 1
 * @param pgmo gmo object handle
 */
int  GMO_CALLCONV d_gmoIndexBase (gmoHandle_t pgmo)
{
  int d_s[]={3};
  GAMS_UNUSED(pgmo)
  printAndReturn(gmoIndexBase,0,int )
}

/** User array index base (0 or 1
 * @param pgmo gmo object handle
 */
void GMO_CALLCONV d_gmoIndexBaseSet (gmoHandle_t pgmo,const int x)
{
  int d_s[]={0,3};
  GAMS_UNUSED(pgmo)
  GAMS_UNUSED(x)
  printNoReturn(gmoIndexBaseSet,1)
}

/** Reformulate objective if possibl
 * @param pgmo gmo object handle
 */
int  GMO_CALLCONV d_gmoObjReform (gmoHandle_t pgmo)
{
  int d_s[]={15};
  GAMS_UNUSED(pgmo)
  printAndReturn(gmoObjReform,0,int )
}

/** Reformulate objective if possibl
 * @param pgmo gmo object handle
 */
void GMO_CALLCONV d_gmoObjReformSet (gmoHandle_t pgmo,const int x)
{
  int d_s[]={0,15};
  GAMS_UNUSED(pgmo)
  GAMS_UNUSED(x)
  printNoReturn(gmoObjReformSet,1)
}

/** Reformulate objective even if objective variable is the only variabl
 * @param pgmo gmo object handle
 */
int  GMO_CALLCONV d_gmoEmptyOut (gmoHandle_t pgmo)
{
  int d_s[]={15};
  GAMS_UNUSED(pgmo)
  printAndReturn(gmoEmptyOut,0,int )
}

/** Reformulate objective even if objective variable is the only variabl
 * @param pgmo gmo object handle
 */
void GMO_CALLCONV d_gmoEmptyOutSet (gmoHandle_t pgmo,const int x)
{
  int d_s[]={0,15};
  GAMS_UNUSED(pgmo)
  GAMS_UNUSED(x)
  printNoReturn(gmoEmptyOutSet,1)
}

/** Consider constant derivatives in external functions or no
 * @param pgmo gmo object handle
 */
int  GMO_CALLCONV d_gmoIgnXCDeriv (gmoHandle_t pgmo)
{
  int d_s[]={15};
  GAMS_UNUSED(pgmo)
  printAndReturn(gmoIgnXCDeriv,0,int )
}

/** Consider constant derivatives in external functions or no
 * @param pgmo gmo object handle
 */
void GMO_CALLCONV d_gmoIgnXCDerivSet (gmoHandle_t pgmo,const int x)
{
  int d_s[]={0,15};
  GAMS_UNUSED(pgmo)
  GAMS_UNUSED(x)
  printNoReturn(gmoIgnXCDerivSet,1)
}

/** Toggle Q-mode
 * @param pgmo gmo object handle
 */
int  GMO_CALLCONV d_gmoUseQ (gmoHandle_t pgmo)
{
  int d_s[]={15};
  GAMS_UNUSED(pgmo)
  printAndReturn(gmoUseQ,0,int )
}

/** Toggle Q-mode
 * @param pgmo gmo object handle
 */
void GMO_CALLCONV d_gmoUseQSet (gmoHandle_t pgmo,const int x)
{
  int d_s[]={0,15};
  GAMS_UNUSED(pgmo)
  GAMS_UNUSED(x)
  printNoReturn(gmoUseQSet,1)
}

/** Choose Q extraction algorithm (must be set before UseQ; 0: automatic; 1: ThreePass; 2: DoubleForward; 3: Concurrent)
 * @param pgmo gmo object handle
 */
int  GMO_CALLCONV d_gmoQExtractAlg (gmoHandle_t pgmo)
{
  int d_s[]={3};
  GAMS_UNUSED(pgmo)
  printAndReturn(gmoQExtractAlg,0,int )
}

/** Choose Q extraction algorithm (must be set before UseQ; 0: automatic; 1: ThreePass; 2: DoubleForward; 3: Concurrent)
 * @param pgmo gmo object handle
 */
void GMO_CALLCONV d_gmoQExtractAlgSet (gmoHandle_t pgmo,const int x)
{
  int d_s[]={0,3};
  GAMS_UNUSED(pgmo)
  GAMS_UNUSED(x)
  printNoReturn(gmoQExtractAlgSet,1)
}

/** Use alternative bound
 * @param pgmo gmo object handle
 */
int  GMO_CALLCONV d_gmoAltBounds (gmoHandle_t pgmo)
{
  int d_s[]={15};
  GAMS_UNUSED(pgmo)
  printAndReturn(gmoAltBounds,0,int )
}

/** Use alternative bound
 * @param pgmo gmo object handle
 */
void GMO_CALLCONV d_gmoAltBoundsSet (gmoHandle_t pgmo,const int x)
{
  int d_s[]={0,15};
  GAMS_UNUSED(pgmo)
  GAMS_UNUSED(x)
  printNoReturn(gmoAltBoundsSet,1)
}

/** Use alternative RH
 * @param pgmo gmo object handle
 */
int  GMO_CALLCONV d_gmoAltRHS (gmoHandle_t pgmo)
{
  int d_s[]={15};
  GAMS_UNUSED(pgmo)
  printAndReturn(gmoAltRHS,0,int )
}

/** Use alternative RH
 * @param pgmo gmo object handle
 */
void GMO_CALLCONV d_gmoAltRHSSet (gmoHandle_t pgmo,const int x)
{
  int d_s[]={0,15};
  GAMS_UNUSED(pgmo)
  GAMS_UNUSED(x)
  printNoReturn(gmoAltRHSSet,1)
}

/** Use alternative variable type
 * @param pgmo gmo object handle
 */
int  GMO_CALLCONV d_gmoAltVarTypes (gmoHandle_t pgmo)
{
  int d_s[]={15};
  GAMS_UNUSED(pgmo)
  printAndReturn(gmoAltVarTypes,0,int )
}

/** Use alternative variable type
 * @param pgmo gmo object handle
 */
void GMO_CALLCONV d_gmoAltVarTypesSet (gmoHandle_t pgmo,const int x)
{
  int d_s[]={0,15};
  GAMS_UNUSED(pgmo)
  GAMS_UNUSED(x)
  printNoReturn(gmoAltVarTypesSet,1)
}

/** Force linear representation of mode
 * @param pgmo gmo object handle
 */
int  GMO_CALLCONV d_gmoForceLinear (gmoHandle_t pgmo)
{
  int d_s[]={15};
  GAMS_UNUSED(pgmo)
  printAndReturn(gmoForceLinear,0,int )
}

/** Force linear representation of mode
 * @param pgmo gmo object handle
 */
void GMO_CALLCONV d_gmoForceLinearSet (gmoHandle_t pgmo,const int x)
{
  int d_s[]={0,15};
  GAMS_UNUSED(pgmo)
  GAMS_UNUSED(x)
  printNoReturn(gmoForceLinearSet,1)
}

/** Force continuous relaxation of mode
 * @param pgmo gmo object handle
 */
int  GMO_CALLCONV d_gmoForceCont (gmoHandle_t pgmo)
{
  int d_s[]={15};
  GAMS_UNUSED(pgmo)
  printAndReturn(gmoForceCont,0,int )
}

/** Force continuous relaxation of mode
 * @param pgmo gmo object handle
 */
void GMO_CALLCONV d_gmoForceContSet (gmoHandle_t pgmo,const int x)
{
  int d_s[]={0,15};
  GAMS_UNUSED(pgmo)
  GAMS_UNUSED(x)
  printNoReturn(gmoForceContSet,1)
}

/** Column permutation fla
 * @param pgmo gmo object handle
 */
int  GMO_CALLCONV d_gmoPermuteCols (gmoHandle_t pgmo)
{
  int d_s[]={15};
  GAMS_UNUSED(pgmo)
  printAndReturn(gmoPermuteCols,0,int )
}

/** Column permutation fla
 * @param pgmo gmo object handle
 */
void GMO_CALLCONV d_gmoPermuteColsSet (gmoHandle_t pgmo,const int x)
{
  int d_s[]={0,15};
  GAMS_UNUSED(pgmo)
  GAMS_UNUSED(x)
  printNoReturn(gmoPermuteColsSet,1)
}

/** Row permutation fla
 * @param pgmo gmo object handle
 */
int  GMO_CALLCONV d_gmoPermuteRows (gmoHandle_t pgmo)
{
  int d_s[]={15};
  GAMS_UNUSED(pgmo)
  printAndReturn(gmoPermuteRows,0,int )
}

/** Row permutation fla
 * @param pgmo gmo object handle
 */
void GMO_CALLCONV d_gmoPermuteRowsSet (gmoHandle_t pgmo,const int x)
{
  int d_s[]={0,15};
  GAMS_UNUSED(pgmo)
  GAMS_UNUSED(x)
  printNoReturn(gmoPermuteRowsSet,1)
}

/** Value for plus infinit
 * @param pgmo gmo object handle
 */
double  GMO_CALLCONV d_gmoPinf (gmoHandle_t pgmo)
{
  int d_s[]={13};
  GAMS_UNUSED(pgmo)
  printAndReturn(gmoPinf,0,double )
}

/** Value for plus infinit
 * @param pgmo gmo object handle
 */
void GMO_CALLCONV d_gmoPinfSet (gmoHandle_t pgmo,const double x)
{
  int d_s[]={0,13};
  GAMS_UNUSED(pgmo)
  GAMS_UNUSED(x)
  printNoReturn(gmoPinfSet,1)
}

/** Value for minus infinit
 * @param pgmo gmo object handle
 */
double  GMO_CALLCONV d_gmoMinf (gmoHandle_t pgmo)
{
  int d_s[]={13};
  GAMS_UNUSED(pgmo)
  printAndReturn(gmoMinf,0,double )
}

/** Value for minus infinit
 * @param pgmo gmo object handle
 */
void GMO_CALLCONV d_gmoMinfSet (gmoHandle_t pgmo,const double x)
{
  int d_s[]={0,13};
  GAMS_UNUSED(pgmo)
  GAMS_UNUSED(x)
  printNoReturn(gmoMinfSet,1)
}

/** quiet IEEE NaN
 * @param pgmo gmo object handle
 */
double  GMO_CALLCONV d_gmoQNaN (gmoHandle_t pgmo)
{
  int d_s[]={13};
  GAMS_UNUSED(pgmo)
  printAndReturn(gmoQNaN,0,double )
}

/** Double Value of N/A
 * @param pgmo gmo object handle
 */
double  GMO_CALLCONV d_gmoValNA (gmoHandle_t pgmo)
{
  int d_s[]={13};
  GAMS_UNUSED(pgmo)
  printAndReturn(gmoValNA,0,double )
}

/** Integer Value of N/A
 * @param pgmo gmo object handle
 */
int  GMO_CALLCONV d_gmoValNAInt (gmoHandle_t pgmo)
{
  int d_s[]={3};
  GAMS_UNUSED(pgmo)
  printAndReturn(gmoValNAInt,0,int )
}

/** Double Value of UNDF
 * @param pgmo gmo object handle
 */
double  GMO_CALLCONV d_gmoValUndf (gmoHandle_t pgmo)
{
  int d_s[]={13};
  GAMS_UNUSED(pgmo)
  printAndReturn(gmoValUndf,0,double )
}

/** Number of rows
 * @param pgmo gmo object handle
 */
int  GMO_CALLCONV d_gmoM (gmoHandle_t pgmo)
{
  int d_s[]={3};
  GAMS_UNUSED(pgmo)
  printAndReturn(gmoM,0,int )
}

/** Number of quadratic rows (-1 if Q information not used)
 * @param pgmo gmo object handle
 */
int  GMO_CALLCONV d_gmoQM (gmoHandle_t pgmo)
{
  int d_s[]={3};
  GAMS_UNUSED(pgmo)
  printAndReturn(gmoQM,0,int )
}

/** Number of nonlinear rows
 * @param pgmo gmo object handle
 */
int  GMO_CALLCONV d_gmoNLM (gmoHandle_t pgmo)
{
  int d_s[]={3};
  GAMS_UNUSED(pgmo)
  printAndReturn(gmoNLM,0,int )
}

/** Number of matched rows
 * @param pgmo gmo object handle
 */
int  GMO_CALLCONV d_gmoNRowMatch (gmoHandle_t pgmo)
{
  int d_s[]={3};
  GAMS_UNUSED(pgmo)
  printAndReturn(gmoNRowMatch,0,int )
}

/** Number of columns
 * @param pgmo gmo object handle
 */
int  GMO_CALLCONV d_gmoN (gmoHandle_t pgmo)
{
  int d_s[]={3};
  GAMS_UNUSED(pgmo)
  printAndReturn(gmoN,0,int )
}

/** Number of nonlinear columns
 * @param pgmo gmo object handle
 */
int  GMO_CALLCONV d_gmoNLN (gmoHandle_t pgmo)
{
  int d_s[]={3};
  GAMS_UNUSED(pgmo)
  printAndReturn(gmoNLN,0,int )
}

/** Number of discontinuous column
 * @param pgmo gmo object handle
 */
int  GMO_CALLCONV d_gmoNDisc (gmoHandle_t pgmo)
{
  int d_s[]={3};
  GAMS_UNUSED(pgmo)
  printAndReturn(gmoNDisc,0,int )
}

/** Number of fixed column
 * @param pgmo gmo object handle
 */
int  GMO_CALLCONV d_gmoNFixed (gmoHandle_t pgmo)
{
  int d_s[]={3};
  GAMS_UNUSED(pgmo)
  printAndReturn(gmoNFixed,0,int )
}

/** Number of matched column
 * @param pgmo gmo object handle
 */
int  GMO_CALLCONV d_gmoNColMatch (gmoHandle_t pgmo)
{
  int d_s[]={3};
  GAMS_UNUSED(pgmo)
  printAndReturn(gmoNColMatch,0,int )
}

/** Number of nonzeros in Jacobian matrix
 * @param pgmo gmo object handle
 */
int  GMO_CALLCONV d_gmoNZ (gmoHandle_t pgmo)
{
  int d_s[]={3};
  GAMS_UNUSED(pgmo)
  printAndReturn(gmoNZ,0,int )
}

/** Number of nonzeros in Jacobian matrix
 * @param pgmo gmo object handle
 */
INT64  GMO_CALLCONV d_gmoNZ64 (gmoHandle_t pgmo)
{
  int d_s[]={23};
  GAMS_UNUSED(pgmo)
  printAndReturn(gmoNZ64,0,INT64 )
}

/** Number of nonlinear nonzeros in Jacobian matrix
 * @param pgmo gmo object handle
 */
int  GMO_CALLCONV d_gmoNLNZ (gmoHandle_t pgmo)
{
  int d_s[]={3};
  GAMS_UNUSED(pgmo)
  printAndReturn(gmoNLNZ,0,int )
}

/** Number of nonlinear nonzeros in Jacobian matrix
 * @param pgmo gmo object handle
 */
INT64  GMO_CALLCONV d_gmoNLNZ64 (gmoHandle_t pgmo)
{
  int d_s[]={23};
  GAMS_UNUSED(pgmo)
  printAndReturn(gmoNLNZ64,0,INT64 )
}

/** Number of linear nonzeros in Jacobian matrix
 * @param pgmo gmo object handle
 */
int  GMO_CALLCONV d_gmoLNZEx (gmoHandle_t pgmo)
{
  int d_s[]={3};
  GAMS_UNUSED(pgmo)
  printAndReturn(gmoLNZEx,0,int )
}

/** Number of linear nonzeros in Jacobian matrix
 * @param pgmo gmo object handle
 */
INT64  GMO_CALLCONV d_gmoLNZEx64 (gmoHandle_t pgmo)
{
  int d_s[]={23};
  GAMS_UNUSED(pgmo)
  printAndReturn(gmoLNZEx64,0,INT64 )
}

/** Legacy overestimate for the count of linear nonzeros in Jacobian matrix, especially if gmoUseQ is true
 * @param pgmo gmo object handle
 */
int  GMO_CALLCONV d_gmoLNZ (gmoHandle_t pgmo)
{
  int d_s[]={3};
  GAMS_UNUSED(pgmo)
  printAndReturn(gmoLNZ,0,int )
}

/** Legacy overestimate for the count of linear nonzeros in Jacobian matrix, especially if gmoUseQ is true
 * @param pgmo gmo object handle
 */
INT64  GMO_CALLCONV d_gmoLNZ64 (gmoHandle_t pgmo)
{
  int d_s[]={23};
  GAMS_UNUSED(pgmo)
  printAndReturn(gmoLNZ64,0,INT64 )
}

/** Number of quadratic nonzeros in Jacobian matrix, 0 if gmoUseQ is false
 * @param pgmo gmo object handle
 */
int  GMO_CALLCONV d_gmoQNZ (gmoHandle_t pgmo)
{
  int d_s[]={3};
  GAMS_UNUSED(pgmo)
  printAndReturn(gmoQNZ,0,int )
}

/** Number of quadratic nonzeros in Jacobian matrix, 0 if gmoUseQ is false
 * @param pgmo gmo object handle
 */
INT64  GMO_CALLCONV d_gmoQNZ64 (gmoHandle_t pgmo)
{
  int d_s[]={23};
  GAMS_UNUSED(pgmo)
  printAndReturn(gmoQNZ64,0,INT64 )
}

/** Number of general nonlinear nonzeros in Jacobian matrix, equals gmoNLNZ if gmoUseQ is false
 * @param pgmo gmo object handle
 */
int  GMO_CALLCONV d_gmoGNLNZ (gmoHandle_t pgmo)
{
  int d_s[]={3};
  GAMS_UNUSED(pgmo)
  printAndReturn(gmoGNLNZ,0,int )
}

/** Number of general nonlinear nonzeros in Jacobian matrix, equals gmoNLNZ if gmoUseQ is false
 * @param pgmo gmo object handle
 */
INT64  GMO_CALLCONV d_gmoGNLNZ64 (gmoHandle_t pgmo)
{
  int d_s[]={23};
  GAMS_UNUSED(pgmo)
  printAndReturn(gmoGNLNZ64,0,INT64 )
}

/** Maximum number of nonzeros in single Q matrix (-1 on overflow)
 * @param pgmo gmo object handle
 */
int  GMO_CALLCONV d_gmoMaxQNZ (gmoHandle_t pgmo)
{
  int d_s[]={3};
  GAMS_UNUSED(pgmo)
  printAndReturn(gmoMaxQNZ,0,int )
}

/** Maximum number of nonzeros in single Q matrix
 * @param pgmo gmo object handle
 */
INT64  GMO_CALLCONV d_gmoMaxQNZ64 (gmoHandle_t pgmo)
{
  int d_s[]={23};
  GAMS_UNUSED(pgmo)
  printAndReturn(gmoMaxQNZ64,0,INT64 )
}

/** Number of nonzeros in objective gradient
 * @param pgmo gmo object handle
 */
int  GMO_CALLCONV d_gmoObjNZ (gmoHandle_t pgmo)
{
  int d_s[]={3};
  GAMS_UNUSED(pgmo)
  printAndReturn(gmoObjNZ,0,int )
}

/** Number of linear nonzeros in objective gradient
 * @param pgmo gmo object handle
 */
int  GMO_CALLCONV d_gmoObjLNZ (gmoHandle_t pgmo)
{
  int d_s[]={3};
  GAMS_UNUSED(pgmo)
  printAndReturn(gmoObjLNZ,0,int )
}

/** Number of GMOORDER_Q nonzeros in objective gradient
 * @param pgmo gmo object handle
 */
int  GMO_CALLCONV d_gmoObjQNZEx (gmoHandle_t pgmo)
{
  int d_s[]={3};
  GAMS_UNUSED(pgmo)
  printAndReturn(gmoObjQNZEx,0,int )
}

/** Number of nonlinear nonzeros in objective gradient
 * @param pgmo gmo object handle
 */
int  GMO_CALLCONV d_gmoObjNLNZ (gmoHandle_t pgmo)
{
  int d_s[]={3};
  GAMS_UNUSED(pgmo)
  printAndReturn(gmoObjNLNZ,0,int )
}

/** Number of GMOORDER_NL nonzeros in objective gradient
 * @param pgmo gmo object handle
 */
int  GMO_CALLCONV d_gmoObjNLNZEx (gmoHandle_t pgmo)
{
  int d_s[]={3};
  GAMS_UNUSED(pgmo)
  printAndReturn(gmoObjNLNZEx,0,int )
}

/** Number of nonzeros in lower triangle of Q matrix of objective (-1 if useQ false or overflow)
 * @param pgmo gmo object handle
 */
int  GMO_CALLCONV d_gmoObjQMatNZ (gmoHandle_t pgmo)
{
  int d_s[]={3};
  GAMS_UNUSED(pgmo)
  printAndReturn(gmoObjQMatNZ,0,int )
}

/** Number of nonzeros in lower triangle of Q matrix of objective (-1 if useQ false)
 * @param pgmo gmo object handle
 */
INT64  GMO_CALLCONV d_gmoObjQMatNZ64 (gmoHandle_t pgmo)
{
  int d_s[]={23};
  GAMS_UNUSED(pgmo)
  printAndReturn(gmoObjQMatNZ64,0,INT64 )
}

/** deprecated synonym for gmoObjQMatNZ
 * @param pgmo gmo object handle
 */
int  GMO_CALLCONV d_gmoObjQNZ (gmoHandle_t pgmo)
{
  int d_s[]={3};
  GAMS_UNUSED(pgmo)
  printAndReturn(gmoObjQNZ,0,int )
}

/** Number of nonzeros on diagonal of Q matrix of objective (-1 if useQ false)
 * @param pgmo gmo object handle
 */
int  GMO_CALLCONV d_gmoObjQDiagNZ (gmoHandle_t pgmo)
{
  int d_s[]={3};
  GAMS_UNUSED(pgmo)
  printAndReturn(gmoObjQDiagNZ,0,int )
}

/** Number of nonzeros in c vector of objective (-1 if Q information not used)
 * @param pgmo gmo object handle
 */
int  GMO_CALLCONV d_gmoObjCVecNZ (gmoHandle_t pgmo)
{
  int d_s[]={3};
  GAMS_UNUSED(pgmo)
  printAndReturn(gmoObjCVecNZ,0,int )
}

/** Length of constant pool in nonlinear code
 * @param pgmo gmo object handle
 */
int  GMO_CALLCONV d_gmoNLConst (gmoHandle_t pgmo)
{
  int d_s[]={3};
  GAMS_UNUSED(pgmo)
  printAndReturn(gmoNLConst,0,int )
}

/** Length of constant pool in nonlinear code
 * @param pgmo gmo object handle
 */
void GMO_CALLCONV d_gmoNLConstSet (gmoHandle_t pgmo,const int x)
{
  int d_s[]={0,3};
  GAMS_UNUSED(pgmo)
  GAMS_UNUSED(x)
  printNoReturn(gmoNLConstSet,1)
}

/** Nonlinear code siz
 * @param pgmo gmo object handle
 */
int  GMO_CALLCONV d_gmoNLCodeSize (gmoHandle_t pgmo)
{
  int d_s[]={3};
  GAMS_UNUSED(pgmo)
  printAndReturn(gmoNLCodeSize,0,int )
}

/** Nonlinear code siz
 * @param pgmo gmo object handle
 */
void GMO_CALLCONV d_gmoNLCodeSizeSet (gmoHandle_t pgmo,const int x)
{
  int d_s[]={0,3};
  GAMS_UNUSED(pgmo)
  GAMS_UNUSED(x)
  printNoReturn(gmoNLCodeSizeSet,1)
}

/** Maximum nonlinear code size for row
 * @param pgmo gmo object handle
 */
int  GMO_CALLCONV d_gmoNLCodeSizeMaxRow (gmoHandle_t pgmo)
{
  int d_s[]={3};
  GAMS_UNUSED(pgmo)
  printAndReturn(gmoNLCodeSizeMaxRow,0,int )
}

/** Index of objective variable
 * @param pgmo gmo object handle
 */
int  GMO_CALLCONV d_gmoObjVar (gmoHandle_t pgmo)
{
  int d_s[]={3};
  GAMS_UNUSED(pgmo)
  printAndReturn(gmoObjVar,0,int )
}

/** Index of objective variable
 * @param pgmo gmo object handle
 */
void GMO_CALLCONV d_gmoObjVarSet (gmoHandle_t pgmo,const int x)
{
  int d_s[]={0,3};
  GAMS_UNUSED(pgmo)
  GAMS_UNUSED(x)
  printNoReturn(gmoObjVarSet,1)
}

/** Index of objective row
 * @param pgmo gmo object handle
 */
int  GMO_CALLCONV d_gmoObjRow (gmoHandle_t pgmo)
{
  int d_s[]={3};
  GAMS_UNUSED(pgmo)
  printAndReturn(gmoObjRow,0,int )
}

/** Order of Objective, see enumerated constants
 * @param pgmo gmo object handle
 */
int  GMO_CALLCONV d_gmoGetObjOrder (gmoHandle_t pgmo)
{
  int d_s[]={3};
  GAMS_UNUSED(pgmo)
  printAndReturn(gmoGetObjOrder,0,int )
}

/** Objective constant
 * @param pgmo gmo object handle
 */
double  GMO_CALLCONV d_gmoObjConst (gmoHandle_t pgmo)
{
  int d_s[]={13};
  GAMS_UNUSED(pgmo)
  printAndReturn(gmoObjConst,0,double )
}

/** Objective constant - this is independent of useQ
 * @param pgmo gmo object handle
 */
double  GMO_CALLCONV d_gmoObjConstEx (gmoHandle_t pgmo)
{
  int d_s[]={13};
  GAMS_UNUSED(pgmo)
  printAndReturn(gmoObjConstEx,0,double )
}

/** Get constant in solvers quadratic objective
 * @param pgmo gmo object handle
 */
double  GMO_CALLCONV d_gmoObjQConst (gmoHandle_t pgmo)
{
  int d_s[]={13};
  GAMS_UNUSED(pgmo)
  printAndReturn(gmoObjQConst,0,double )
}

/** Value of Jacobian element of objective variable in objective
 * @param pgmo gmo object handle
 */
double  GMO_CALLCONV d_gmoObjJacVal (gmoHandle_t pgmo)
{
  int d_s[]={13};
  GAMS_UNUSED(pgmo)
  printAndReturn(gmoObjJacVal,0,double )
}

/** Method for returning on nonlinear evaluation errors
 * @param pgmo gmo object handle
 */
int  GMO_CALLCONV d_gmoEvalErrorMethod (gmoHandle_t pgmo)
{
  int d_s[]={3};
  GAMS_UNUSED(pgmo)
  printAndReturn(gmoEvalErrorMethod,0,int )
}

/** Method for returning on nonlinear evaluation errors
 * @param pgmo gmo object handle
 */
void GMO_CALLCONV d_gmoEvalErrorMethodSet (gmoHandle_t pgmo,const int x)
{
  int d_s[]={0,3};
  GAMS_UNUSED(pgmo)
  GAMS_UNUSED(x)
  printNoReturn(gmoEvalErrorMethodSet,1)
}

/** Maximum number of threads that can be used for evaluation
 * @param pgmo gmo object handle
 */
int  GMO_CALLCONV d_gmoEvalMaxThreads (gmoHandle_t pgmo)
{
  int d_s[]={3};
  GAMS_UNUSED(pgmo)
  printAndReturn(gmoEvalMaxThreads,0,int )
}

/** Maximum number of threads that can be used for evaluation
 * @param pgmo gmo object handle
 */
void GMO_CALLCONV d_gmoEvalMaxThreadsSet (gmoHandle_t pgmo,const int x)
{
  int d_s[]={0,3};
  GAMS_UNUSED(pgmo)
  GAMS_UNUSED(x)
  printNoReturn(gmoEvalMaxThreadsSet,1)
}

/** Number of function evaluations
 * @param pgmo gmo object handle
 */
int  GMO_CALLCONV d_gmoEvalFuncCount (gmoHandle_t pgmo)
{
  int d_s[]={3};
  GAMS_UNUSED(pgmo)
  printAndReturn(gmoEvalFuncCount,0,int )
}

/** Time used for function evaluations in s
 * @param pgmo gmo object handle
 */
double  GMO_CALLCONV d_gmoEvalFuncTimeUsed (gmoHandle_t pgmo)
{
  int d_s[]={13};
  GAMS_UNUSED(pgmo)
  printAndReturn(gmoEvalFuncTimeUsed,0,double )
}

/** Number of gradient evaluations
 * @param pgmo gmo object handle
 */
int  GMO_CALLCONV d_gmoEvalGradCount (gmoHandle_t pgmo)
{
  int d_s[]={3};
  GAMS_UNUSED(pgmo)
  printAndReturn(gmoEvalGradCount,0,int )
}

/** Time used for gradient evaluations in s
 * @param pgmo gmo object handle
 */
double  GMO_CALLCONV d_gmoEvalGradTimeUsed (gmoHandle_t pgmo)
{
  int d_s[]={13};
  GAMS_UNUSED(pgmo)
  printAndReturn(gmoEvalGradTimeUsed,0,double )
}

/** Maximum dimension of Hessian
 * @param pgmo gmo object handle
 */
int  GMO_CALLCONV d_gmoHessMaxDim (gmoHandle_t pgmo)
{
  int d_s[]={3};
  GAMS_UNUSED(pgmo)
  printAndReturn(gmoHessMaxDim,0,int )
}

/** Maximum number of nonzeros in Hessian
 * @param pgmo gmo object handle
 */
int  GMO_CALLCONV d_gmoHessMaxNz (gmoHandle_t pgmo)
{
  int d_s[]={3};
  GAMS_UNUSED(pgmo)
  printAndReturn(gmoHessMaxNz,0,int )
}

/** Maximum number of nonzeros in Hessian
 * @param pgmo gmo object handle
 */
INT64  GMO_CALLCONV d_gmoHessMaxNz64 (gmoHandle_t pgmo)
{
  int d_s[]={23};
  GAMS_UNUSED(pgmo)
  printAndReturn(gmoHessMaxNz64,0,INT64 )
}

/** Dimension of Hessian of the Lagrangian
 * @param pgmo gmo object handle
 */
int  GMO_CALLCONV d_gmoHessLagDim (gmoHandle_t pgmo)
{
  int d_s[]={3};
  GAMS_UNUSED(pgmo)
  printAndReturn(gmoHessLagDim,0,int )
}

/** Nonzeros in Hessian of the Lagrangian
 * @param pgmo gmo object handle
 */
int  GMO_CALLCONV d_gmoHessLagNz (gmoHandle_t pgmo)
{
  int d_s[]={3};
  GAMS_UNUSED(pgmo)
  printAndReturn(gmoHessLagNz,0,int )
}

/** Nonzeros in Hessian of the Lagrangian
 * @param pgmo gmo object handle
 */
INT64  GMO_CALLCONV d_gmoHessLagNz64 (gmoHandle_t pgmo)
{
  int d_s[]={23};
  GAMS_UNUSED(pgmo)
  printAndReturn(gmoHessLagNz64,0,INT64 )
}

/** Nonzeros on Diagonal of Hessian of the Lagrangian
 * @param pgmo gmo object handle
 */
int  GMO_CALLCONV d_gmoHessLagDiagNz (gmoHandle_t pgmo)
{
  int d_s[]={3};
  GAMS_UNUSED(pgmo)
  printAndReturn(gmoHessLagDiagNz,0,int )
}

/** if useQ is true, still include GMOORDER_Q rows in the Hessian
 * @param pgmo gmo object handle
 */
int  GMO_CALLCONV d_gmoHessInclQRows (gmoHandle_t pgmo)
{
  int d_s[]={15};
  GAMS_UNUSED(pgmo)
  printAndReturn(gmoHessInclQRows,0,int )
}

/** if useQ is true, still include GMOORDER_Q rows in the Hessian
 * @param pgmo gmo object handle
 */
void GMO_CALLCONV d_gmoHessInclQRowsSet (gmoHandle_t pgmo,const int x)
{
  int d_s[]={0,15};
  GAMS_UNUSED(pgmo)
  GAMS_UNUSED(x)
  printNoReturn(gmoHessInclQRowsSet,1)
}

/** EMP: Number of variational inequalities in model rim
 * @param pgmo gmo object handle
 */
int  GMO_CALLCONV d_gmoNumVIFunc (gmoHandle_t pgmo)
{
  int d_s[]={3};
  GAMS_UNUSED(pgmo)
  printAndReturn(gmoNumVIFunc,0,int )
}

/** EMP: Number of Agents/Followers
 * @param pgmo gmo object handle
 */
int  GMO_CALLCONV d_gmoNumAgents (gmoHandle_t pgmo)
{
  int d_s[]={3};
  GAMS_UNUSED(pgmo)
  printAndReturn(gmoNumAgents,0,int )
}

/** Name of option file
 * @param pgmo gmo object handle
 */
char * GMO_CALLCONV d_gmoNameOptFile (gmoHandle_t pgmo, char *buf)
{
  int d_s[]={12};
  GAMS_UNUSED(pgmo)
  GAMS_UNUSED(buf)
  printAndReturn(gmoNameOptFile,0,char *)
}

/** Name of option file
 * @param pgmo gmo object handle
 */
void GMO_CALLCONV d_gmoNameOptFileSet (gmoHandle_t pgmo,const char *x)
{
  int d_s[]={0,12};
  GAMS_UNUSED(pgmo)
  GAMS_UNUSED(x)
  printNoReturn(gmoNameOptFileSet,1)
}

/** Name of solution file
 * @param pgmo gmo object handle
 */
char * GMO_CALLCONV d_gmoNameSolFile (gmoHandle_t pgmo, char *buf)
{
  int d_s[]={12};
  GAMS_UNUSED(pgmo)
  GAMS_UNUSED(buf)
  printAndReturn(gmoNameSolFile,0,char *)
}

/** Name of solution file
 * @param pgmo gmo object handle
 */
void GMO_CALLCONV d_gmoNameSolFileSet (gmoHandle_t pgmo,const char *x)
{
  int d_s[]={0,12};
  GAMS_UNUSED(pgmo)
  GAMS_UNUSED(x)
  printNoReturn(gmoNameSolFileSet,1)
}

/** Name of external function library
 * @param pgmo gmo object handle
 */
char * GMO_CALLCONV d_gmoNameXLib (gmoHandle_t pgmo, char *buf)
{
  int d_s[]={12};
  GAMS_UNUSED(pgmo)
  GAMS_UNUSED(buf)
  printAndReturn(gmoNameXLib,0,char *)
}

/** Name of external function library
 * @param pgmo gmo object handle
 */
void GMO_CALLCONV d_gmoNameXLibSet (gmoHandle_t pgmo,const char *x)
{
  int d_s[]={0,12};
  GAMS_UNUSED(pgmo)
  GAMS_UNUSED(x)
  printNoReturn(gmoNameXLibSet,1)
}

/** Name of matrix file
 * @param pgmo gmo object handle
 */
char * GMO_CALLCONV d_gmoNameMatrix (gmoHandle_t pgmo, char *buf)
{
  int d_s[]={12};
  GAMS_UNUSED(pgmo)
  GAMS_UNUSED(buf)
  printAndReturn(gmoNameMatrix,0,char *)
}

/** Name of dictionary file
 * @param pgmo gmo object handle
 */
char * GMO_CALLCONV d_gmoNameDict (gmoHandle_t pgmo, char *buf)
{
  int d_s[]={12};
  GAMS_UNUSED(pgmo)
  GAMS_UNUSED(buf)
  printAndReturn(gmoNameDict,0,char *)
}

/** Name of dictionary file
 * @param pgmo gmo object handle
 */
void GMO_CALLCONV d_gmoNameDictSet (gmoHandle_t pgmo,const char *x)
{
  int d_s[]={0,12};
  GAMS_UNUSED(pgmo)
  GAMS_UNUSED(x)
  printNoReturn(gmoNameDictSet,1)
}

/** Name of input file (with .gms stripped)
 * @param pgmo gmo object handle
 */
char * GMO_CALLCONV d_gmoNameInput (gmoHandle_t pgmo, char *buf)
{
  int d_s[]={12};
  GAMS_UNUSED(pgmo)
  GAMS_UNUSED(buf)
  printAndReturn(gmoNameInput,0,char *)
}

/** Name of input file (with .gms stripped)
 * @param pgmo gmo object handle
 */
void GMO_CALLCONV d_gmoNameInputSet (gmoHandle_t pgmo,const char *x)
{
  int d_s[]={0,12};
  GAMS_UNUSED(pgmo)
  GAMS_UNUSED(x)
  printNoReturn(gmoNameInputSet,1)
}

/** Name of output file (with .dat stripped)
 * @param pgmo gmo object handle
 */
char * GMO_CALLCONV d_gmoNameOutput (gmoHandle_t pgmo, char *buf)
{
  int d_s[]={12};
  GAMS_UNUSED(pgmo)
  GAMS_UNUSED(buf)
  printAndReturn(gmoNameOutput,0,char *)
}

/** Pointer to constant pool
 * @param pgmo gmo object handle
 */
void * GMO_CALLCONV d_gmoPPool (gmoHandle_t pgmo)
{
  int d_s[]={1};
  GAMS_UNUSED(pgmo)
  printAndReturn(gmoPPool,0,void *)
}

/** IO mutex
 * @param pgmo gmo object handle
 */
void * GMO_CALLCONV d_gmoIOMutex (gmoHandle_t pgmo)
{
  int d_s[]={1};
  GAMS_UNUSED(pgmo)
  printAndReturn(gmoIOMutex,0,void *)
}

/** Access to error indicator
 * @param pgmo gmo object handle
 */
int  GMO_CALLCONV d_gmoError (gmoHandle_t pgmo)
{
  int d_s[]={3};
  GAMS_UNUSED(pgmo)
  printAndReturn(gmoError,0,int )
}

/** Access to error indicator
 * @param pgmo gmo object handle
 */
void GMO_CALLCONV d_gmoErrorSet (gmoHandle_t pgmo,const int x)
{
  int d_s[]={0,3};
  GAMS_UNUSED(pgmo)
  GAMS_UNUSED(x)
  printNoReturn(gmoErrorSet,1)
}

/** Provide the last error message
 * @param pgmo gmo object handle
 */
char * GMO_CALLCONV d_gmoErrorMessage (gmoHandle_t pgmo, char *buf)
{
  int d_s[]={12};
  GAMS_UNUSED(pgmo)
  GAMS_UNUSED(buf)
  printAndReturn(gmoErrorMessage,0,char *)
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

  LOADIT(gmoXCreate, "gmoXCreate");
  LOADIT(gmoXCreateD, "CgmoXCreateD");

  LOADIT(gmoXFree, "gmoXFree");
  LOADIT(gmoXCheck, "CgmoXCheck");
  LOADIT(gmoXAPIVersion, "CgmoXAPIVersion");

  if (!gmoXAPIVersion(27,errBuf,&cl))
    return 1;

#define CheckAndLoad(f,nargs,prefix) \
  if (!gmoXCheck(#f,nargs,s,errBuf)) \
    f = &d_##f; \
  else { \
    LOADIT(f,prefix #f); \
  }
  {int s[]={3,3,3,3}; CheckAndLoad(gmoInitData,3,""); }
  {int s[]={3,3,3,13,13,13,13,3,3,7,5,7}; CheckAndLoad(gmoAddRow,11,""); }
  {int s[]={3,3,13,13,13,13,3,3,13,13,3,7,5,7}; CheckAndLoad(gmoAddCol,13,""); }
  {int s[]={3,12}; CheckAndLoad(gmoCompleteData,1,"C"); }
  {int s[]={3,12}; CheckAndLoad(gmoFillMatches,1,"C"); }
  {int s[]={3,12}; CheckAndLoad(gmoLoadDataLegacy,1,"C"); }
  {int s[]={3,15,12}; CheckAndLoad(gmoLoadDataLegacyEx,2,"C"); }
  {int s[]={3,1,12}; CheckAndLoad(gmoRegisterEnvironment,2,"C"); }
  {int s[]={1}; CheckAndLoad(gmoEnvironment,0,""); }
  {int s[]={1}; CheckAndLoad(gmoViewStore,0,""); }
  {int s[]={0,2}; CheckAndLoad(gmoViewRestore,1,""); }
  {int s[]={0}; CheckAndLoad(gmoViewDump,0,""); }
  {int s[]={3,3}; CheckAndLoad(gmoGetiSolver,1,""); }
  {int s[]={3,3}; CheckAndLoad(gmoGetjSolver,1,""); }
  {int s[]={3,3}; CheckAndLoad(gmoGetiSolverQuiet,1,""); }
  {int s[]={3,3}; CheckAndLoad(gmoGetjSolverQuiet,1,""); }
  {int s[]={3,3}; CheckAndLoad(gmoGetiModel,1,""); }
  {int s[]={3,3}; CheckAndLoad(gmoGetjModel,1,""); }
  {int s[]={3,8}; CheckAndLoad(gmoSetEquPermutation,1,""); }
  {int s[]={3,8,3}; CheckAndLoad(gmoSetRvEquPermutation,2,""); }
  {int s[]={3,8}; CheckAndLoad(gmoSetVarPermutation,1,""); }
  {int s[]={3,8,3}; CheckAndLoad(gmoSetRvVarPermutation,2,""); }
  {int s[]={3}; CheckAndLoad(gmoSetNRowPerm,0,""); }
  {int s[]={3,3}; CheckAndLoad(gmoGetVarTypeCnt,1,""); }
  {int s[]={3,3}; CheckAndLoad(gmoGetEquTypeCnt,1,""); }
  {int s[]={3,4,4,4}; CheckAndLoad(gmoGetObjStat,3,""); }
  {int s[]={3,3,4,4,4}; CheckAndLoad(gmoGetRowStat,4,""); }
  {int s[]={3,3,4,4,4,4}; CheckAndLoad(gmoGetRowStatEx,5,""); }
  {int s[]={3,3,4,4,4,4}; CheckAndLoad(gmoGetColStat,5,""); }
  {int s[]={3,3}; CheckAndLoad(gmoGetRowQNZOne,1,""); }
  {int s[]={23,3}; CheckAndLoad(gmoGetRowQNZOne64,1,""); }
  {int s[]={3,3}; CheckAndLoad(gmoGetRowQDiagNZOne,1,""); }
  {int s[]={3,3}; CheckAndLoad(gmoGetRowCVecNZOne,1,""); }
  {int s[]={0,4,4,4}; CheckAndLoad(gmoGetSosCounts,3,""); }
  {int s[]={0,4,4,4,8}; CheckAndLoad(gmoGetXLibCounts,4,""); }
  {int s[]={3,8,4}; CheckAndLoad(gmoGetActiveModelType,2,""); }
  {int s[]={3,8,8,6,8}; CheckAndLoad(gmoGetMatrixRow,4,""); }
  {int s[]={3,8,8,6,8}; CheckAndLoad(gmoGetMatrixCol,4,""); }
  {int s[]={3,8,8,8,6}; CheckAndLoad(gmoGetMatrixCplex,4,""); }
  {int s[]={12}; CheckAndLoad(gmoGetObjName,0,"C"); }
  {int s[]={12,11}; CheckAndLoad(gmoGetObjNameCustom,1,"C"); }
  {int s[]={3,6,8}; CheckAndLoad(gmoGetObjVector,2,""); }
  {int s[]={3,8,6,8,4,4}; CheckAndLoad(gmoGetObjSparse,5,""); }
  {int s[]={3,8,6,8,4,4,4}; CheckAndLoad(gmoGetObjSparseEx,6,""); }
  {int s[]={3,8,8,6}; CheckAndLoad(gmoGetObjQMat,3,""); }
  {int s[]={3,8,8,6}; CheckAndLoad(gmoGetObjQ,3,""); }
  {int s[]={3,8,6}; CheckAndLoad(gmoGetObjCVec,2,""); }
  {int s[]={13}; CheckAndLoad(gmoGetObjL,0,""); }
  {int s[]={3,6}; CheckAndLoad(gmoGetEquL,1,""); }
  {int s[]={13,3}; CheckAndLoad(gmoGetEquLOne,1,""); }
  {int s[]={3,5}; CheckAndLoad(gmoSetEquL,1,""); }
  {int s[]={0,3,13}; CheckAndLoad(gmoSetEquLOne,2,""); }
  {int s[]={3,6}; CheckAndLoad(gmoGetEquM,1,""); }
  {int s[]={13,3}; CheckAndLoad(gmoGetEquMOne,1,""); }
  {int s[]={3,5}; CheckAndLoad(gmoSetEquM,1,""); }
  {int s[]={12,3}; CheckAndLoad(gmoGetEquNameOne,1,"C"); }
  {int s[]={12,3,11}; CheckAndLoad(gmoGetEquNameCustomOne,2,"C"); }
  {int s[]={3,6}; CheckAndLoad(gmoGetRhs,1,""); }
  {int s[]={13,3}; CheckAndLoad(gmoGetRhsOne,1,""); }
  {int s[]={13,3}; CheckAndLoad(gmoGetRhsOneEx,1,""); }
  {int s[]={3,5}; CheckAndLoad(gmoSetAltRHS,1,""); }
  {int s[]={0,3,13}; CheckAndLoad(gmoSetAltRHSOne,2,""); }
  {int s[]={3,6}; CheckAndLoad(gmoGetEquSlack,1,""); }
  {int s[]={13,3}; CheckAndLoad(gmoGetEquSlackOne,1,""); }
  {int s[]={3,5}; CheckAndLoad(gmoSetEquSlack,1,""); }
  {int s[]={3,8}; CheckAndLoad(gmoGetEquType,1,""); }
  {int s[]={3,3}; CheckAndLoad(gmoGetEquTypeOne,1,""); }
  {int s[]={0,8}; CheckAndLoad(gmoGetEquStat,1,""); }
  {int s[]={3,3}; CheckAndLoad(gmoGetEquStatOne,1,""); }
  {int s[]={0,7}; CheckAndLoad(gmoSetEquStat,1,""); }
  {int s[]={0,8}; CheckAndLoad(gmoGetEquCStat,1,""); }
  {int s[]={3,3}; CheckAndLoad(gmoGetEquCStatOne,1,""); }
  {int s[]={0,7}; CheckAndLoad(gmoSetEquCStat,1,""); }
  {int s[]={3,8}; CheckAndLoad(gmoGetEquMatch,1,""); }
  {int s[]={3,3}; CheckAndLoad(gmoGetEquMatchOne,1,""); }
  {int s[]={3,6}; CheckAndLoad(gmoGetEquScale,1,""); }
  {int s[]={13,3}; CheckAndLoad(gmoGetEquScaleOne,1,""); }
  {int s[]={3,6}; CheckAndLoad(gmoGetEquStage,1,""); }
  {int s[]={13,3}; CheckAndLoad(gmoGetEquStageOne,1,""); }
  {int s[]={3,3}; CheckAndLoad(gmoGetEquOrderOne,1,""); }
  {int s[]={3,3,8,6,8,4,4}; CheckAndLoad(gmoGetRowSparse,6,""); }
  {int s[]={3,3,8,6,8,4,4,4}; CheckAndLoad(gmoGetRowSparseEx,7,""); }
  {int s[]={0,3,2,14,4,4}; CheckAndLoad(gmoGetRowJacInfoOne,5,""); }
  {int s[]={3,3,8,8,6}; CheckAndLoad(gmoGetRowQMat,4,""); }
  {int s[]={3,3,8,8,6}; CheckAndLoad(gmoGetRowQ,4,""); }
  {int s[]={3,3,8,6}; CheckAndLoad(gmoGetRowCVec,3,""); }
  {int s[]={13,3}; CheckAndLoad(gmoGetRowQConst,1,""); }
  {int s[]={3,1,11,8}; CheckAndLoad(gmoGetEquIntDotOpt,3,"C"); }
  {int s[]={3,1,11,6}; CheckAndLoad(gmoGetEquDblDotOpt,3,"C"); }
  {int s[]={3,6}; CheckAndLoad(gmoGetVarL,1,""); }
  {int s[]={13,3}; CheckAndLoad(gmoGetVarLOne,1,""); }
  {int s[]={3,5}; CheckAndLoad(gmoSetVarL,1,""); }
  {int s[]={0,3,13}; CheckAndLoad(gmoSetVarLOne,2,""); }
  {int s[]={3,6}; CheckAndLoad(gmoGetVarM,1,""); }
  {int s[]={13,3}; CheckAndLoad(gmoGetVarMOne,1,""); }
  {int s[]={3,5}; CheckAndLoad(gmoSetVarM,1,""); }
  {int s[]={0,3,13}; CheckAndLoad(gmoSetVarMOne,2,""); }
  {int s[]={12,3}; CheckAndLoad(gmoGetVarNameOne,1,"C"); }
  {int s[]={12,3,11}; CheckAndLoad(gmoGetVarNameCustomOne,2,"C"); }
  {int s[]={3,6}; CheckAndLoad(gmoGetVarLower,1,""); }
  {int s[]={13,3}; CheckAndLoad(gmoGetVarLowerOne,1,""); }
  {int s[]={3,6}; CheckAndLoad(gmoGetVarUpper,1,""); }
  {int s[]={13,3}; CheckAndLoad(gmoGetVarUpperOne,1,""); }
  {int s[]={3,5,5}; CheckAndLoad(gmoSetAltVarBounds,2,""); }
  {int s[]={0,3,13}; CheckAndLoad(gmoSetAltVarLowerOne,2,""); }
  {int s[]={0,3,13}; CheckAndLoad(gmoSetAltVarUpperOne,2,""); }
  {int s[]={3,8}; CheckAndLoad(gmoGetVarType,1,""); }
  {int s[]={3,3}; CheckAndLoad(gmoGetVarTypeOne,1,""); }
  {int s[]={3,7}; CheckAndLoad(gmoSetAltVarType,1,""); }
  {int s[]={0,3,3}; CheckAndLoad(gmoSetAltVarTypeOne,2,""); }
  {int s[]={0,8}; CheckAndLoad(gmoGetVarStat,1,""); }
  {int s[]={3,3}; CheckAndLoad(gmoGetVarStatOne,1,""); }
  {int s[]={0,7}; CheckAndLoad(gmoSetVarStat,1,""); }
  {int s[]={0,3,3}; CheckAndLoad(gmoSetVarStatOne,2,""); }
  {int s[]={0,8}; CheckAndLoad(gmoGetVarCStat,1,""); }
  {int s[]={3,3}; CheckAndLoad(gmoGetVarCStatOne,1,""); }
  {int s[]={0,7}; CheckAndLoad(gmoSetVarCStat,1,""); }
  {int s[]={3,8}; CheckAndLoad(gmoGetVarMatch,1,""); }
  {int s[]={3,3}; CheckAndLoad(gmoGetVarMatchOne,1,""); }
  {int s[]={3,6}; CheckAndLoad(gmoGetVarPrior,1,""); }
  {int s[]={13,3}; CheckAndLoad(gmoGetVarPriorOne,1,""); }
  {int s[]={3,6}; CheckAndLoad(gmoGetVarScale,1,""); }
  {int s[]={13,3}; CheckAndLoad(gmoGetVarScaleOne,1,""); }
  {int s[]={3,6}; CheckAndLoad(gmoGetVarStage,1,""); }
  {int s[]={13,3}; CheckAndLoad(gmoGetVarStageOne,1,""); }
  {int s[]={3,8,8,8,6}; CheckAndLoad(gmoGetSosConstraints,4,""); }
  {int s[]={3,3}; CheckAndLoad(gmoGetVarSosSetOne,1,""); }
  {int s[]={3,3,8,6,8,4,4}; CheckAndLoad(gmoGetColSparse,6,""); }
  {int s[]={0,3,2,14,4,4}; CheckAndLoad(gmoGetColJacInfoOne,5,""); }
  {int s[]={3,1,11,8}; CheckAndLoad(gmoGetVarIntDotOpt,3,"C"); }
  {int s[]={3,1,11,6}; CheckAndLoad(gmoGetVarDblDotOpt,3,"C"); }
  {int s[]={0,15}; CheckAndLoad(gmoEvalErrorMsg,1,""); }
  {int s[]={0,15,3}; CheckAndLoad(gmoEvalErrorMsg_MT,2,""); }
  {int s[]={0,3}; CheckAndLoad(gmoEvalErrorMaskLevel,1,""); }
  {int s[]={0,3,3}; CheckAndLoad(gmoEvalErrorMaskLevel_MT,2,""); }
  {int s[]={3,5}; CheckAndLoad(gmoEvalNewPoint,1,""); }
  {int s[]={0,1}; CheckAndLoad(gmoSetExtFuncs,1,""); }
  {int s[]={3,12,14,25,25}; CheckAndLoad(gmoGetQMakerStats,4,"C"); }
  {int s[]={3,3,5,14,4}; CheckAndLoad(gmoEvalFunc,4,""); }
  {int s[]={3,3,5,14,4,3}; CheckAndLoad(gmoEvalFunc_MT,5,""); }
  {int s[]={3,3,14,4}; CheckAndLoad(gmoEvalFuncInt,3,""); }
  {int s[]={3,3,14,4,3}; CheckAndLoad(gmoEvalFuncInt_MT,4,""); }
  {int s[]={3,3,5,14,4}; CheckAndLoad(gmoEvalFuncNL,4,""); }
  {int s[]={3,3,5,14,4,3}; CheckAndLoad(gmoEvalFuncNL_MT,5,""); }
  {int s[]={3,5,14,4}; CheckAndLoad(gmoEvalFuncObj,3,""); }
  {int s[]={3,5,14,4}; CheckAndLoad(gmoEvalFuncNLObj,3,""); }
  {int s[]={3,3,5,5,14,14,4}; CheckAndLoad(gmoEvalFuncInterval,6,""); }
  {int s[]={3,3,5,5,14,14,4,3}; CheckAndLoad(gmoEvalFuncInterval_MT,7,""); }
  {int s[]={3,3,5,14,6,14,4}; CheckAndLoad(gmoEvalGrad,6,""); }
  {int s[]={3,3,5,14,6,14,4,3}; CheckAndLoad(gmoEvalGrad_MT,7,""); }
  {int s[]={3,3,5,14,6,14,4}; CheckAndLoad(gmoEvalGradNL,6,""); }
  {int s[]={3,3,5,14,6,14,4,3}; CheckAndLoad(gmoEvalGradNL_MT,7,""); }
  {int s[]={3,5,14,6,14,4}; CheckAndLoad(gmoEvalGradObj,5,""); }
  {int s[]={3,5,14,6,14,4}; CheckAndLoad(gmoEvalGradNLObj,5,""); }
  {int s[]={3,3,5,5,14,14,6,6,4}; CheckAndLoad(gmoEvalGradInterval,8,""); }
  {int s[]={3,3,5,5,14,14,6,6,4,3}; CheckAndLoad(gmoEvalGradInterval_MT,9,""); }
  {int s[]={3,6,15,4}; CheckAndLoad(gmoEvalGradNLUpdate,3,""); }
  {int s[]={3,8,8,6,21}; CheckAndLoad(gmoGetJacUpdate,4,""); }
  {int s[]={3,13,21,21}; CheckAndLoad(gmoHessLoad,3,""); }
  {int s[]={3}; CheckAndLoad(gmoHessUnload,0,""); }
  {int s[]={3,3}; CheckAndLoad(gmoHessDim,1,""); }
  {int s[]={3,3}; CheckAndLoad(gmoHessNz,1,""); }
  {int s[]={23,3}; CheckAndLoad(gmoHessNz64,1,""); }
  {int s[]={3,3,8,8,4,4}; CheckAndLoad(gmoHessStruct,5,""); }
  {int s[]={3,3,8,8,4,25}; CheckAndLoad(gmoHessStruct64,5,""); }
  {int s[]={3,3,8,8,4,4,5,6,4}; CheckAndLoad(gmoHessValue,8,""); }
  {int s[]={3,3,8,8,4,25,5,6,4}; CheckAndLoad(gmoHessValue64,8,""); }
  {int s[]={3,3,5,5,6,4}; CheckAndLoad(gmoHessVec,5,""); }
  {int s[]={3,8,8}; CheckAndLoad(gmoHessLagStruct,2,""); }
  {int s[]={3,5,5,6,13,13,4}; CheckAndLoad(gmoHessLagValue,6,""); }
  {int s[]={3,5,5,5,6,13,13,4}; CheckAndLoad(gmoHessLagVec,7,""); }
  {int s[]={3,11}; CheckAndLoad(gmoLoadEMPInfo,1,"C"); }
  {int s[]={3,8}; CheckAndLoad(gmoGetEquVI,1,""); }
  {int s[]={3,3}; CheckAndLoad(gmoGetEquVIOne,1,""); }
  {int s[]={3,8}; CheckAndLoad(gmoGetVarVI,1,""); }
  {int s[]={3,3}; CheckAndLoad(gmoGetVarVIOne,1,""); }
  {int s[]={3,8}; CheckAndLoad(gmoGetAgentType,1,""); }
  {int s[]={3,3}; CheckAndLoad(gmoGetAgentTypeOne,1,""); }
  {int s[]={3,8,8}; CheckAndLoad(gmoGetBiLevelInfo,2,""); }
  {int s[]={3,11}; CheckAndLoad(gmoDumpEMPInfoToGDX,1,"C"); }
  {int s[]={13,3}; CheckAndLoad(gmoGetHeadnTail,1,""); }
  {int s[]={0,3,13}; CheckAndLoad(gmoSetHeadnTail,2,""); }
  {int s[]={3,5}; CheckAndLoad(gmoSetSolutionPrimal,1,""); }
  {int s[]={3,5,5}; CheckAndLoad(gmoSetSolution2,2,""); }
  {int s[]={3,5,5,5,5}; CheckAndLoad(gmoSetSolution,4,""); }
  {int s[]={3,5,5,5,5,8,8,8,8}; CheckAndLoad(gmoSetSolution8,8,""); }
  {int s[]={3,3,5,5,7,7,13,13}; CheckAndLoad(gmoSetSolutionFixer,7,""); }
  {int s[]={3,3,14,14,4,4}; CheckAndLoad(gmoGetSolutionVarRec,5,""); }
  {int s[]={3,3,13,13,3,3}; CheckAndLoad(gmoSetSolutionVarRec,5,""); }
  {int s[]={3,3,14,14,4,4}; CheckAndLoad(gmoGetSolutionEquRec,5,""); }
  {int s[]={3,3,13,13,3,3}; CheckAndLoad(gmoSetSolutionEquRec,5,""); }
  {int s[]={3,8,8,8,8}; CheckAndLoad(gmoSetSolutionStatus,4,""); }
  {int s[]={0,13}; CheckAndLoad(gmoCompleteObjective,1,""); }
  {int s[]={3}; CheckAndLoad(gmoCompleteSolution,0,""); }
  {int s[]={13}; CheckAndLoad(gmoGetAbsoluteGap,0,""); }
  {int s[]={13}; CheckAndLoad(gmoGetRelativeGap,0,""); }
  {int s[]={3}; CheckAndLoad(gmoLoadSolutionLegacy,0,""); }
  {int s[]={3}; CheckAndLoad(gmoUnloadSolutionLegacy,0,""); }
  {int s[]={3,11,15,15,15}; CheckAndLoad(gmoLoadSolutionGDX,4,"C"); }
  {int s[]={3,11,15,15,15}; CheckAndLoad(gmoUnloadSolutionGDX,4,"C"); }
  {int s[]={3,11,1,3}; CheckAndLoad(gmoPrepareAllSolToGDX,3,"C"); }
  {int s[]={3,55}; CheckAndLoad(gmoAddSolutionToGDX,1,"C"); }
  {int s[]={3,12}; CheckAndLoad(gmoWriteSolDone,1,"C"); }
  {int s[]={3,11,21}; CheckAndLoad(gmoCheckSolPoolUEL,2,"C"); }
  {int s[]={1,11,3,11}; CheckAndLoad(gmoPrepareSolPoolMerge,3,"C"); }
  {int s[]={3,1}; CheckAndLoad(gmoPrepareSolPoolNextSym,1,""); }
  {int s[]={3,1,3}; CheckAndLoad(gmoUnloadSolPoolSolution,2,""); }
  {int s[]={3,1}; CheckAndLoad(gmoFinalizeSolPoolMerge,1,""); }
  {int s[]={3,3,12}; CheckAndLoad(gmoGetVarTypeTxt,2,"C"); }
  {int s[]={3,3,12}; CheckAndLoad(gmoGetEquTypeTxt,2,"C"); }
  {int s[]={3,3,12}; CheckAndLoad(gmoGetSolveStatusTxt,2,"C"); }
  {int s[]={3,3,12}; CheckAndLoad(gmoGetModelStatusTxt,2,"C"); }
  {int s[]={3,3,12}; CheckAndLoad(gmoGetModelTypeTxt,2,"C"); }
  {int s[]={3,3,12}; CheckAndLoad(gmoGetHeadNTailTxt,2,"C"); }
  {int s[]={13}; CheckAndLoad(gmoMemUsed,0,""); }
  {int s[]={13}; CheckAndLoad(gmoPeakMemUsed,0,""); }
  {int s[]={3,1,1}; CheckAndLoad(gmoSetNLObject,2,""); }
  {int s[]={3,11}; CheckAndLoad(gmoDumpQMakerGDX,1,"C"); }
  {int s[]={3,3,1,3,21,8,8,8}; CheckAndLoad(gmoGetVarEquMap,7,""); }
  {int s[]={3,1,3,21,8,8,8}; CheckAndLoad(gmoGetIndicatorMap,6,""); }
  {int s[]={3}; CheckAndLoad(gmoCrudeness,0,""); }
  {int s[]={3,3,4,8,8}; CheckAndLoad(gmoDirtyGetRowFNLInstr,4,""); }
  {int s[]={3,4,8,8}; CheckAndLoad(gmoDirtyGetObjFNLInstr,3,""); }
  {int s[]={3,3,3,7,7,1,6,3}; CheckAndLoad(gmoDirtySetRowFNLInstr,7,""); }
  {int s[]={12,3}; CheckAndLoad(gmoGetExtrLibName,1,"C"); }
  {int s[]={1,3}; CheckAndLoad(gmoGetExtrLibObjPtr,1,""); }
  {int s[]={12,3,3}; CheckAndLoad(gmoGetExtrLibFuncName,2,"C"); }
  {int s[]={1,3,11,12}; CheckAndLoad(gmoLoadExtrLibEntry,3,"C"); }
  {int s[]={1}; CheckAndLoad(gmoDict,0,""); }
  {int s[]={0,1}; CheckAndLoad(gmoDictSet,1,""); }
  {int s[]={12}; CheckAndLoad(gmoNameModel,0,"C"); }
  {int s[]={0,12}; CheckAndLoad(gmoNameModelSet,1,"C"); }
  {int s[]={3}; CheckAndLoad(gmoModelSeqNr,0,""); }
  {int s[]={0,3}; CheckAndLoad(gmoModelSeqNrSet,1,""); }
  {int s[]={3}; CheckAndLoad(gmoModelType,0,""); }
  {int s[]={0,3}; CheckAndLoad(gmoModelTypeSet,1,""); }
  {int s[]={15}; CheckAndLoad(gmoNLModelType,0,""); }
  {int s[]={3}; CheckAndLoad(gmoSense,0,""); }
  {int s[]={0,3}; CheckAndLoad(gmoSenseSet,1,""); }
  {int s[]={15}; CheckAndLoad(gmoIsQP,0,""); }
  {int s[]={3}; CheckAndLoad(gmoOptFile,0,""); }
  {int s[]={0,3}; CheckAndLoad(gmoOptFileSet,1,""); }
  {int s[]={3}; CheckAndLoad(gmoDictionary,0,""); }
  {int s[]={0,3}; CheckAndLoad(gmoDictionarySet,1,""); }
  {int s[]={3}; CheckAndLoad(gmoScaleOpt,0,""); }
  {int s[]={0,3}; CheckAndLoad(gmoScaleOptSet,1,""); }
  {int s[]={3}; CheckAndLoad(gmoPriorOpt,0,""); }
  {int s[]={0,3}; CheckAndLoad(gmoPriorOptSet,1,""); }
  {int s[]={3}; CheckAndLoad(gmoHaveBasis,0,""); }
  {int s[]={0,3}; CheckAndLoad(gmoHaveBasisSet,1,""); }
  {int s[]={3}; CheckAndLoad(gmoModelStat,0,""); }
  {int s[]={0,3}; CheckAndLoad(gmoModelStatSet,1,""); }
  {int s[]={3}; CheckAndLoad(gmoSolveStat,0,""); }
  {int s[]={0,3}; CheckAndLoad(gmoSolveStatSet,1,""); }
  {int s[]={15}; CheckAndLoad(gmoIsMPSGE,0,""); }
  {int s[]={0,15}; CheckAndLoad(gmoIsMPSGESet,1,""); }
  {int s[]={3}; CheckAndLoad(gmoObjStyle,0,""); }
  {int s[]={0,3}; CheckAndLoad(gmoObjStyleSet,1,""); }
  {int s[]={3}; CheckAndLoad(gmoInterface,0,""); }
  {int s[]={0,3}; CheckAndLoad(gmoInterfaceSet,1,""); }
  {int s[]={3}; CheckAndLoad(gmoIndexBase,0,""); }
  {int s[]={0,3}; CheckAndLoad(gmoIndexBaseSet,1,""); }
  {int s[]={15}; CheckAndLoad(gmoObjReform,0,""); }
  {int s[]={0,15}; CheckAndLoad(gmoObjReformSet,1,""); }
  {int s[]={15}; CheckAndLoad(gmoEmptyOut,0,""); }
  {int s[]={0,15}; CheckAndLoad(gmoEmptyOutSet,1,""); }
  {int s[]={15}; CheckAndLoad(gmoIgnXCDeriv,0,""); }
  {int s[]={0,15}; CheckAndLoad(gmoIgnXCDerivSet,1,""); }
  {int s[]={15}; CheckAndLoad(gmoUseQ,0,""); }
  {int s[]={0,15}; CheckAndLoad(gmoUseQSet,1,""); }
  {int s[]={3}; CheckAndLoad(gmoQExtractAlg,0,""); }
  {int s[]={0,3}; CheckAndLoad(gmoQExtractAlgSet,1,""); }
  {int s[]={15}; CheckAndLoad(gmoAltBounds,0,""); }
  {int s[]={0,15}; CheckAndLoad(gmoAltBoundsSet,1,""); }
  {int s[]={15}; CheckAndLoad(gmoAltRHS,0,""); }
  {int s[]={0,15}; CheckAndLoad(gmoAltRHSSet,1,""); }
  {int s[]={15}; CheckAndLoad(gmoAltVarTypes,0,""); }
  {int s[]={0,15}; CheckAndLoad(gmoAltVarTypesSet,1,""); }
  {int s[]={15}; CheckAndLoad(gmoForceLinear,0,""); }
  {int s[]={0,15}; CheckAndLoad(gmoForceLinearSet,1,""); }
  {int s[]={15}; CheckAndLoad(gmoForceCont,0,""); }
  {int s[]={0,15}; CheckAndLoad(gmoForceContSet,1,""); }
  {int s[]={15}; CheckAndLoad(gmoPermuteCols,0,""); }
  {int s[]={0,15}; CheckAndLoad(gmoPermuteColsSet,1,""); }
  {int s[]={15}; CheckAndLoad(gmoPermuteRows,0,""); }
  {int s[]={0,15}; CheckAndLoad(gmoPermuteRowsSet,1,""); }
  {int s[]={13}; CheckAndLoad(gmoPinf,0,""); }
  {int s[]={0,13}; CheckAndLoad(gmoPinfSet,1,""); }
  {int s[]={13}; CheckAndLoad(gmoMinf,0,""); }
  {int s[]={0,13}; CheckAndLoad(gmoMinfSet,1,""); }
  {int s[]={13}; CheckAndLoad(gmoQNaN,0,""); }
  {int s[]={13}; CheckAndLoad(gmoValNA,0,""); }
  {int s[]={3}; CheckAndLoad(gmoValNAInt,0,""); }
  {int s[]={13}; CheckAndLoad(gmoValUndf,0,""); }
  {int s[]={3}; CheckAndLoad(gmoM,0,""); }
  {int s[]={3}; CheckAndLoad(gmoQM,0,""); }
  {int s[]={3}; CheckAndLoad(gmoNLM,0,""); }
  {int s[]={3}; CheckAndLoad(gmoNRowMatch,0,""); }
  {int s[]={3}; CheckAndLoad(gmoN,0,""); }
  {int s[]={3}; CheckAndLoad(gmoNLN,0,""); }
  {int s[]={3}; CheckAndLoad(gmoNDisc,0,""); }
  {int s[]={3}; CheckAndLoad(gmoNFixed,0,""); }
  {int s[]={3}; CheckAndLoad(gmoNColMatch,0,""); }
  {int s[]={3}; CheckAndLoad(gmoNZ,0,""); }
  {int s[]={23}; CheckAndLoad(gmoNZ64,0,""); }
  {int s[]={3}; CheckAndLoad(gmoNLNZ,0,""); }
  {int s[]={23}; CheckAndLoad(gmoNLNZ64,0,""); }
  {int s[]={3}; CheckAndLoad(gmoLNZEx,0,""); }
  {int s[]={23}; CheckAndLoad(gmoLNZEx64,0,""); }
  {int s[]={3}; CheckAndLoad(gmoLNZ,0,""); }
  {int s[]={23}; CheckAndLoad(gmoLNZ64,0,""); }
  {int s[]={3}; CheckAndLoad(gmoQNZ,0,""); }
  {int s[]={23}; CheckAndLoad(gmoQNZ64,0,""); }
  {int s[]={3}; CheckAndLoad(gmoGNLNZ,0,""); }
  {int s[]={23}; CheckAndLoad(gmoGNLNZ64,0,""); }
  {int s[]={3}; CheckAndLoad(gmoMaxQNZ,0,""); }
  {int s[]={23}; CheckAndLoad(gmoMaxQNZ64,0,""); }
  {int s[]={3}; CheckAndLoad(gmoObjNZ,0,""); }
  {int s[]={3}; CheckAndLoad(gmoObjLNZ,0,""); }
  {int s[]={3}; CheckAndLoad(gmoObjQNZEx,0,""); }
  {int s[]={3}; CheckAndLoad(gmoObjNLNZ,0,""); }
  {int s[]={3}; CheckAndLoad(gmoObjNLNZEx,0,""); }
  {int s[]={3}; CheckAndLoad(gmoObjQMatNZ,0,""); }
  {int s[]={23}; CheckAndLoad(gmoObjQMatNZ64,0,""); }
  {int s[]={3}; CheckAndLoad(gmoObjQNZ,0,""); }
  {int s[]={3}; CheckAndLoad(gmoObjQDiagNZ,0,""); }
  {int s[]={3}; CheckAndLoad(gmoObjCVecNZ,0,""); }
  {int s[]={3}; CheckAndLoad(gmoNLConst,0,""); }
  {int s[]={0,3}; CheckAndLoad(gmoNLConstSet,1,""); }
  {int s[]={3}; CheckAndLoad(gmoNLCodeSize,0,""); }
  {int s[]={0,3}; CheckAndLoad(gmoNLCodeSizeSet,1,""); }
  {int s[]={3}; CheckAndLoad(gmoNLCodeSizeMaxRow,0,""); }
  {int s[]={3}; CheckAndLoad(gmoObjVar,0,""); }
  {int s[]={0,3}; CheckAndLoad(gmoObjVarSet,1,""); }
  {int s[]={3}; CheckAndLoad(gmoObjRow,0,""); }
  {int s[]={3}; CheckAndLoad(gmoGetObjOrder,0,""); }
  {int s[]={13}; CheckAndLoad(gmoObjConst,0,""); }
  {int s[]={13}; CheckAndLoad(gmoObjConstEx,0,""); }
  {int s[]={13}; CheckAndLoad(gmoObjQConst,0,""); }
  {int s[]={13}; CheckAndLoad(gmoObjJacVal,0,""); }
  {int s[]={3}; CheckAndLoad(gmoEvalErrorMethod,0,""); }
  {int s[]={0,3}; CheckAndLoad(gmoEvalErrorMethodSet,1,""); }
  {int s[]={3}; CheckAndLoad(gmoEvalMaxThreads,0,""); }
  {int s[]={0,3}; CheckAndLoad(gmoEvalMaxThreadsSet,1,""); }
  {int s[]={3}; CheckAndLoad(gmoEvalFuncCount,0,""); }
  {int s[]={13}; CheckAndLoad(gmoEvalFuncTimeUsed,0,""); }
  {int s[]={3}; CheckAndLoad(gmoEvalGradCount,0,""); }
  {int s[]={13}; CheckAndLoad(gmoEvalGradTimeUsed,0,""); }
  {int s[]={3}; CheckAndLoad(gmoHessMaxDim,0,""); }
  {int s[]={3}; CheckAndLoad(gmoHessMaxNz,0,""); }
  {int s[]={23}; CheckAndLoad(gmoHessMaxNz64,0,""); }
  {int s[]={3}; CheckAndLoad(gmoHessLagDim,0,""); }
  {int s[]={3}; CheckAndLoad(gmoHessLagNz,0,""); }
  {int s[]={23}; CheckAndLoad(gmoHessLagNz64,0,""); }
  {int s[]={3}; CheckAndLoad(gmoHessLagDiagNz,0,""); }
  {int s[]={15}; CheckAndLoad(gmoHessInclQRows,0,""); }
  {int s[]={0,15}; CheckAndLoad(gmoHessInclQRowsSet,1,""); }
  {int s[]={3}; CheckAndLoad(gmoNumVIFunc,0,""); }
  {int s[]={3}; CheckAndLoad(gmoNumAgents,0,""); }
  {int s[]={12}; CheckAndLoad(gmoNameOptFile,0,"C"); }
  {int s[]={0,12}; CheckAndLoad(gmoNameOptFileSet,1,"C"); }
  {int s[]={12}; CheckAndLoad(gmoNameSolFile,0,"C"); }
  {int s[]={0,12}; CheckAndLoad(gmoNameSolFileSet,1,"C"); }
  {int s[]={12}; CheckAndLoad(gmoNameXLib,0,"C"); }
  {int s[]={0,12}; CheckAndLoad(gmoNameXLibSet,1,"C"); }
  {int s[]={12}; CheckAndLoad(gmoNameMatrix,0,"C"); }
  {int s[]={12}; CheckAndLoad(gmoNameDict,0,"C"); }
  {int s[]={0,12}; CheckAndLoad(gmoNameDictSet,1,"C"); }
  {int s[]={12}; CheckAndLoad(gmoNameInput,0,"C"); }
  {int s[]={0,12}; CheckAndLoad(gmoNameInputSet,1,"C"); }
  {int s[]={12}; CheckAndLoad(gmoNameOutput,0,"C"); }
  {int s[]={1}; CheckAndLoad(gmoPPool,0,""); }
  {int s[]={1}; CheckAndLoad(gmoIOMutex,0,""); }
  {int s[]={3}; CheckAndLoad(gmoError,0,""); }
  {int s[]={0,3}; CheckAndLoad(gmoErrorSet,1,""); }
  {int s[]={12}; CheckAndLoad(gmoErrorMessage,0,"C"); }

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

int gmoGetReady (char *msgBuf, int msgBufSize)
{
  int rc;
  lock(libMutex);
  rc = libloader(NULL, NULL, msgBuf, msgBufSize);
  unlock(libMutex);
  return rc;
} /* gmoGetReady */

int gmoGetReadyD (const char *dirName, char *msgBuf, int msgBufSize)
{
  int rc;
  lock(libMutex);
  rc = libloader(dirName, NULL, msgBuf, msgBufSize);
  unlock(libMutex);
  return rc;
} /* gmoGetReadyD */

int gmoGetReadyL (const char *libName, char *msgBuf, int msgBufSize)
{
  char dirName[1024],fName[1024];
  int rc;
  extractFileDirFileName (libName, dirName, fName);
  lock(libMutex);
  rc = libloader(dirName, fName, msgBuf, msgBufSize);
  unlock(libMutex);
  return rc;
} /* gmoGetReadyL */

int gmoCreate (gmoHandle_t *pgmo, char *msgBuf, int msgBufSize)
{
  int gmoIsReady;

  gmoIsReady = gmoGetReady (msgBuf, msgBufSize);
  if (! gmoIsReady) {
    return 0;
  }
  assert(gmoXCreate);
  gmoXCreate(pgmo);
  if(pgmo == NULL)
  { strcpy(msgBuf,"Error while creating object"); return 0; }
  lock(objMutex);
  objectCount++;
  unlock(objMutex);
  return 1;                     /* return true on successful library load */
} /* gmoCreate */

int gmoCreateD (gmoHandle_t *pgmo, const char *dirName,
                char *msgBuf, int msgBufSize)
{
  int gmoIsReady;

  gmoIsReady = gmoGetReadyD (dirName, msgBuf, msgBufSize);
  if (! gmoIsReady) {
    return 0;
  }
  assert(gmoXCreate);
  gmoXCreate(pgmo);
  if(pgmo == NULL)
  { strcpy(msgBuf,"Error while creating object"); return 0; }
  lock(objMutex);
  objectCount++;
  unlock(objMutex);
  return 1;                     /* return true on successful library load */
} /* gmoCreateD */

int gmoCreateDD (gmoHandle_t *pgmo, const char *dirName,
                char *msgBuf, int msgBufSize)
{
  int gmoIsReady;
  gmoIsReady = gmoGetReadyD (dirName, msgBuf, msgBufSize);
  if (! gmoIsReady) {
    return 0;
  }
  assert(gmoXCreateD);
  gmoXCreateD(pgmo, dirName);
  if(pgmo == NULL)
  { strcpy(msgBuf,"Error while creating object"); return 0; }
  lock(objMutex);
  objectCount++;
  unlock(objMutex);
  return 1;                     /* return true on successful library load */
} /* gmoCreateD */


int gmoCreateL (gmoHandle_t *pgmo, const char *libName,
                char *msgBuf, int msgBufSize)
{
  int gmoIsReady;

  gmoIsReady = gmoGetReadyL (libName, msgBuf, msgBufSize);
  if (! gmoIsReady) {
    return 0;
  }
  assert(gmoXCreate);
  gmoXCreate(pgmo);
  if(pgmo == NULL)
  { strcpy(msgBuf,"Error while creating object"); return 0; }
  lock(objMutex);
  objectCount++;
  unlock(objMutex);
  return 1;                     /* return true on successful library load */
} /* gmoCreateL */

int gmoFree   (gmoHandle_t *pgmo)
{
  assert(gmoXFree);
  gmoXFree(pgmo); pgmo = NULL;
  lock(objMutex);
  objectCount--;
  unlock(objMutex);
  return 1;
} /* gmoFree */

int gmoLibraryLoaded(void)
{
  int rc;
  lock(libMutex);
  rc = isLoaded;
  unlock(libMutex);
  return rc;
} /* gmoLibraryLoaded */

int gmoLibraryUnload(void)
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
} /* gmoLibraryUnload */

int  gmoCorrectLibraryVersion(char *msgBuf, int msgBufLen)
{
  int cl;
  char localBuf[256];

  if (msgBuf && msgBufLen) msgBuf[0] = '\0';

  if (! isLoaded) {
    strncpy(msgBuf, "Library needs to be initialized first", msgBufLen);
    return 0;
  }

  if (NULL == gmoXAPIVersion) {
    strncpy(msgBuf, "Function gmoXAPIVersion not found", msgBufLen);
    return 0;
  }

  gmoXAPIVersion(GMOAPIVERSION,localBuf,&cl);
  strncpy(msgBuf, localBuf, msgBufLen);

  if (1 == cl)
    return 1;
  else
    return 0;
}

int gmoGetScreenIndicator(void)
{
  return ScreenIndicator;
}

void gmoSetScreenIndicator(int scrind)
{
  ScreenIndicator = scrind ? 1 : 0;
}

int gmoGetExceptionIndicator(void)
{
   return ExceptionIndicator;
}

void gmoSetExceptionIndicator(int excind)
{
  ExceptionIndicator = excind ? 1 : 0;
}

int gmoGetExitIndicator(void)
{
  return ExitIndicator;
}

void gmoSetExitIndicator(int extind)
{
  ExitIndicator = extind ? 1 : 0;
}

gmoErrorCallback_t gmoGetErrorCallback(void)
{
  return ErrorCallBack;
}

void gmoSetErrorCallback(gmoErrorCallback_t func)
{
  lock(exceptMutex);
  ErrorCallBack = func;
  unlock(exceptMutex);
}

int gmoGetAPIErrorCount(void)
{
  return APIErrorCount;
}

void gmoSetAPIErrorCount(int ecnt)
{
  APIErrorCount = ecnt;
}

void gmoErrorHandling(const char *msg)
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

