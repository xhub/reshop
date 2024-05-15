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

#define GMD_MAIN
#include "gmdcc.h"

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
static gmdErrorCallback_t ErrorCallBack = NULL;
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

void gmdInitMutexes(void)
{
  int rc;
  if (0==MutexIsInitialized) {
    rc = GC_mutex_init (&libMutex);     if(0!=rc) gmdErrorHandling("Problem initializing libMutex");
    rc = GC_mutex_init (&objMutex);     if(0!=rc) gmdErrorHandling("Problem initializing objMutex");
    rc = GC_mutex_init (&exceptMutex);  if(0!=rc) gmdErrorHandling("Problem initializing exceptMutex");
    MutexIsInitialized = 1;
  }
}

void gmdFiniMutexes(void)
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
void gmdInitMutexes(void) {}
void gmdFiniMutexes(void) {}
#endif

#if !defined(GAMS_UNUSED)
#define GAMS_UNUSED(x) (void)x;
#endif

typedef void (GMD_CALLCONV *XCreate_t) (gmdHandle_t *pgmd);
static GMD_FUNCPTR(XCreate);

typedef void (GMD_CALLCONV *XCreateD_t) (gmdHandle_t *pgmd, const char *dirName);
static GMD_FUNCPTR(XCreateD);
typedef void (GMD_CALLCONV *XFree_t)   (gmdHandle_t *pgmd);
static GMD_FUNCPTR(XFree);
typedef int (GMD_CALLCONV *XAPIVersion_t) (int api, char *msg, int *cl);
static GMD_FUNCPTR(XAPIVersion);
typedef int (GMD_CALLCONV *XCheck_t) (const char *ep, int nargs, int s[], char *msg);
static GMD_FUNCPTR(XCheck);
#define printNoReturn(f,nargs) { \
  char d_msgBuf[256]; \
  strcpy(d_msgBuf,#f " could not be loaded: "); \
  XCheck(#f,nargs,d_s,d_msgBuf+strlen(d_msgBuf)); \
  gmdErrorHandling(d_msgBuf); \
}
#define printAndReturn(f,nargs,rtype) { \
  char d_msgBuf[256]; \
  strcpy(d_msgBuf,#f " could not be loaded: "); \
  XCheck(#f,nargs,d_s,d_msgBuf+strlen(d_msgBuf)); \
  gmdErrorHandling(d_msgBuf); \
  return (rtype) 0; \
}


/** Open a GDX file for data reading
 * @param pgmd gmd object handle
 * @param fileName File name of the gdx file
 */
int  GMD_CALLCONV d_gmdInitFromGDX (gmdHandle_t pgmd, const char *fileName)
{
  int d_s[]={15,11};
  GAMS_UNUSED(pgmd)
  GAMS_UNUSED(fileName)
  printAndReturn(gmdInitFromGDX,1,int )
}

/** Initialize from model dictionary
 * @param pgmd gmd object handle
 * @param gmoPtr Handle to GMO object
 */
int  GMD_CALLCONV d_gmdInitFromDict (gmdHandle_t pgmd, void *gmoPtr)
{
  int d_s[]={15,1};
  GAMS_UNUSED(pgmd)
  GAMS_UNUSED(gmoPtr)
  printAndReturn(gmdInitFromDict,1,int )
}

/** Initialize from CMEX embedded code
 * @param pgmd gmd object handle
 * @param findSymbol 
 * @param dataReadRawStart 
 * @param dataReadRaw 
 * @param dataReadDone 
 * @param getElemText 
 * @param printLog 
 * @param usrmem 
 */
int  GMD_CALLCONV d_gmdInitFromCMEX (gmdHandle_t pgmd, TFindSymbol_t findSymbol, TDataReadRawStart_t dataReadRawStart, TDataReadRaw_t dataReadRaw, TDataReadDone_t dataReadDone, TGetElemText_t getElemText, TPrintLog_t printLog, void *usrmem)
{
  int d_s[]={15,59,59,59,59,59,59,1};
  GAMS_UNUSED(pgmd)
  GAMS_UNUSED(findSymbol)
  GAMS_UNUSED(dataReadRawStart)
  GAMS_UNUSED(dataReadRaw)
  GAMS_UNUSED(dataReadDone)
  GAMS_UNUSED(getElemText)
  GAMS_UNUSED(printLog)
  GAMS_UNUSED(usrmem)
  printAndReturn(gmdInitFromCMEX,7,int )
}

/** Initialize from another database
 * @param pgmd gmd object handle
 * @param gmdSrcPtr Handle to source database
 */
int  GMD_CALLCONV d_gmdInitFromDB (gmdHandle_t pgmd, void *gmdSrcPtr)
{
  int d_s[]={15,1};
  GAMS_UNUSED(pgmd)
  GAMS_UNUSED(gmdSrcPtr)
  printAndReturn(gmdInitFromDB,1,int )
}

/** Register GMO object alternative in connection with gmdInitFromDB to gmdInitFromDict
 * @param pgmd gmd object handle
 * @param gmoPtr Handle to GMO object
 */
int  GMD_CALLCONV d_gmdRegisterGMO (gmdHandle_t pgmd, void *gmoPtr)
{
  int d_s[]={15,1};
  GAMS_UNUSED(pgmd)
  GAMS_UNUSED(gmoPtr)
  printAndReturn(gmdRegisterGMO,1,int )
}

/** Close GDX file and loads all unloaded symbols on request
 * @param pgmd gmd object handle
 * @param loadRemain Switch for forcing the load of all unloaded symbol when closing GDX
 */
int  GMD_CALLCONV d_gmdCloseGDX (gmdHandle_t pgmd, int loadRemain)
{
  int d_s[]={15,15};
  GAMS_UNUSED(pgmd)
  GAMS_UNUSED(loadRemain)
  printAndReturn(gmdCloseGDX,1,int )
}

/** Add a new symbol with domain info to the gmd object
 * @param pgmd gmd object handle
 * @param symName name of the symbol
 * @param aDim name of the symbol
 * @param stype type of a symbol
 * @param userInfo user info integer
 * @param explText explanatory text
 * @param vDomPtrIn vector of domain symbols (input for GMD)
 * @param keyStr Array of strings containing the unique elements
 * @param symPtr Handle to a symbol when reading sequentially
 */
int  GMD_CALLCONV d_gmdAddSymbolX (gmdHandle_t pgmd, const char *symName, int aDim, int stype, int userInfo, const char *explText, void **vDomPtrIn, const char *keyStr[], void **symPtr)
{
  int d_s[]={15,11,3,3,3,11,2,55,2};
  GAMS_UNUSED(pgmd)
  GAMS_UNUSED(symName)
  GAMS_UNUSED(aDim)
  GAMS_UNUSED(stype)
  GAMS_UNUSED(userInfo)
  GAMS_UNUSED(explText)
  GAMS_UNUSED(vDomPtrIn)
  GAMS_UNUSED(keyStr)
  GAMS_UNUSED(symPtr)
  printAndReturn(gmdAddSymbolX,8,int )
}

/** Add a new symbol with domain info to the gmd object (Python)
 * @param pgmd gmd object handle
 * @param symName dimension of the symbol
 * @param aDim dimension of the symbol
 * @param stype type of a symbol
 * @param userInfo user info integer
 * @param explText explanatory text
 * @param vDomPtrIn vector of domain symbols (input for GMD)
 * @param keyStr Array of strings containing the unique elements
 * @param status Indicator for error free execution (true/1 means OK, false/0 means Error)
 */
void * GMD_CALLCONV d_gmdAddSymbolXPy (gmdHandle_t pgmd, const char *symName, int aDim, int stype, int userInfo, const char *explText, void **vDomPtrIn, const char *keyStr[], int *status)
{
  int d_s[]={1,11,3,3,3,11,2,55,20};
  GAMS_UNUSED(pgmd)
  GAMS_UNUSED(symName)
  GAMS_UNUSED(aDim)
  GAMS_UNUSED(stype)
  GAMS_UNUSED(userInfo)
  GAMS_UNUSED(explText)
  GAMS_UNUSED(vDomPtrIn)
  GAMS_UNUSED(keyStr)
  GAMS_UNUSED(status)
  printAndReturn(gmdAddSymbolXPy,8,void *)
}

/** Add a new symbol to the gmd object
 * @param pgmd gmd object handle
 * @param symName name of the symbol
 * @param aDim dimension of the symbol
 * @param stype type of a symbol
 * @param userInfo user info integer
 * @param explText explanatory text
 * @param symPtr Handle to a symbol when reading sequentially
 */
int  GMD_CALLCONV d_gmdAddSymbol (gmdHandle_t pgmd, const char *symName, int aDim, int stype, int userInfo, const char *explText, void **symPtr)
{
  int d_s[]={15,11,3,3,3,11,2};
  GAMS_UNUSED(pgmd)
  GAMS_UNUSED(symName)
  GAMS_UNUSED(aDim)
  GAMS_UNUSED(stype)
  GAMS_UNUSED(userInfo)
  GAMS_UNUSED(explText)
  GAMS_UNUSED(symPtr)
  printAndReturn(gmdAddSymbol,6,int )
}

/** Add a new symbol to the gmd object (Python)
 * @param pgmd gmd object handle
 * @param symName name of the symbol
 * @param aDim dimension of the symbol
 * @param stype type of a symbol
 * @param userInfo user info integer
 * @param explText explanatory text
 * @param status Indicator for error free execution (true/1 means OK, false/0 means Error)
 */
void * GMD_CALLCONV d_gmdAddSymbolPy (gmdHandle_t pgmd, const char *symName, int aDim, int stype, int userInfo, const char *explText, int *status)
{
  int d_s[]={1,11,3,3,3,11,20};
  GAMS_UNUSED(pgmd)
  GAMS_UNUSED(symName)
  GAMS_UNUSED(aDim)
  GAMS_UNUSED(stype)
  GAMS_UNUSED(userInfo)
  GAMS_UNUSED(explText)
  GAMS_UNUSED(status)
  printAndReturn(gmdAddSymbolPy,6,void *)
}

/** Find symbol
 * @param pgmd gmd object handle
 * @param symName name of the symbol
 * @param symPtr Handle to a symbol when reading sequentially
 */
int  GMD_CALLCONV d_gmdFindSymbol (gmdHandle_t pgmd, const char *symName, void **symPtr)
{
  int d_s[]={15,11,2};
  GAMS_UNUSED(pgmd)
  GAMS_UNUSED(symName)
  GAMS_UNUSED(symPtr)
  printAndReturn(gmdFindSymbol,2,int )
}

/** Find symbol (Python)
 * @param pgmd gmd object handle
 * @param symName name of the symbol
 * @param status Indicator for error free execution (true/1 means OK, false/0 means Error)
 */
void * GMD_CALLCONV d_gmdFindSymbolPy (gmdHandle_t pgmd, const char *symName, int *status)
{
  int d_s[]={1,11,20};
  GAMS_UNUSED(pgmd)
  GAMS_UNUSED(symName)
  GAMS_UNUSED(status)
  printAndReturn(gmdFindSymbolPy,2,void *)
}

/** Find symbol including aliases
 * @param pgmd gmd object handle
 * @param symName name of the symbol
 * @param symPtr Handle to a symbol when reading sequentially
 */
int  GMD_CALLCONV d_gmdFindSymbolWithAlias (gmdHandle_t pgmd, const char *symName, void **symPtr)
{
  int d_s[]={15,11,2};
  GAMS_UNUSED(pgmd)
  GAMS_UNUSED(symName)
  GAMS_UNUSED(symPtr)
  printAndReturn(gmdFindSymbolWithAlias,2,int )
}

/** Find symbol including aliases (Python)
 * @param pgmd gmd object handle
 * @param symName name of the symbol
 * @param status Indicator for error free execution (true/1 means OK, false/0 means Error)
 */
void * GMD_CALLCONV d_gmdFindSymbolWithAliasPy (gmdHandle_t pgmd, const char *symName, int *status)
{
  int d_s[]={1,11,20};
  GAMS_UNUSED(pgmd)
  GAMS_UNUSED(symName)
  GAMS_UNUSED(status)
  printAndReturn(gmdFindSymbolWithAliasPy,2,void *)
}

/** Find symbol by index position
 * @param pgmd gmd object handle
 * @param idx index of a symbol in the GMD object / of a key from a record
 * @param symPtr Handle to a symbol when reading sequentially
 */
int  GMD_CALLCONV d_gmdGetSymbolByIndex (gmdHandle_t pgmd, int idx, void **symPtr)
{
  int d_s[]={15,3,2};
  GAMS_UNUSED(pgmd)
  GAMS_UNUSED(idx)
  GAMS_UNUSED(symPtr)
  printAndReturn(gmdGetSymbolByIndex,2,int )
}

/** Find symbol by index position (Python)
 * @param pgmd gmd object handle
 * @param idx index of a symbol in the GMD object / of a key from a record
 * @param status Indicator for error free execution (true/1 means OK, false/0 means Error)
 */
void * GMD_CALLCONV d_gmdGetSymbolByIndexPy (gmdHandle_t pgmd, int idx, int *status)
{
  int d_s[]={1,3,20};
  GAMS_UNUSED(pgmd)
  GAMS_UNUSED(idx)
  GAMS_UNUSED(status)
  printAndReturn(gmdGetSymbolByIndexPy,2,void *)
}

/** Find symbol by number position this includes the alias symbols and used GMD_NUMBER
 * @param pgmd gmd object handle
 * @param idx index of a symbol in the GMD object / of a key from a record
 * @param symPtr Handle to a symbol when reading sequentially
 */
int  GMD_CALLCONV d_gmdGetSymbolByNumber (gmdHandle_t pgmd, int idx, void **symPtr)
{
  int d_s[]={15,3,2};
  GAMS_UNUSED(pgmd)
  GAMS_UNUSED(idx)
  GAMS_UNUSED(symPtr)
  printAndReturn(gmdGetSymbolByNumber,2,int )
}

/** Find symbol by number position this includes the alias symbols and used GMD_NUMBER (Python)
 * @param pgmd gmd object handle
 * @param idx index of a symbol in the GMD object / of a key from a record
 * @param status Indicator for error free execution (true/1 means OK, false/0 means Error)
 */
void * GMD_CALLCONV d_gmdGetSymbolByNumberPy (gmdHandle_t pgmd, int idx, int *status)
{
  int d_s[]={1,3,20};
  GAMS_UNUSED(pgmd)
  GAMS_UNUSED(idx)
  GAMS_UNUSED(status)
  printAndReturn(gmdGetSymbolByNumberPy,2,void *)
}

/** Deletes all record of a symbol
 * @param pgmd gmd object handle
 * @param symPtr Handle to a symbol when reading sequentially
 */
int  GMD_CALLCONV d_gmdClearSymbol (gmdHandle_t pgmd, void *symPtr)
{
  int d_s[]={15,1};
  GAMS_UNUSED(pgmd)
  GAMS_UNUSED(symPtr)
  printAndReturn(gmdClearSymbol,1,int )
}

/** Clear target symbol and copies all record from source symbol
 * @param pgmd gmd object handle
 * @param tarSymPtr Handle to target symbol
 * @param srcSymPtr Handle to source symbol
 */
int  GMD_CALLCONV d_gmdCopySymbol (gmdHandle_t pgmd, void *tarSymPtr, void *srcSymPtr)
{
  int d_s[]={15,1,1};
  GAMS_UNUSED(pgmd)
  GAMS_UNUSED(tarSymPtr)
  GAMS_UNUSED(srcSymPtr)
  printAndReturn(gmdCopySymbol,2,int )
}

/** Find data record of a symbol
 * @param pgmd gmd object handle
 * @param symPtr Handle to a symbol when reading sequentially
 * @param keyStr Array of strings containing the unique elements
 * @param symIterPtr Handle to a SymbolIterator
 */
int  GMD_CALLCONV d_gmdFindRecord (gmdHandle_t pgmd, void *symPtr, const char *keyStr[], void **symIterPtr)
{
  int d_s[]={15,1,55,2};
  GAMS_UNUSED(pgmd)
  GAMS_UNUSED(symPtr)
  GAMS_UNUSED(keyStr)
  GAMS_UNUSED(symIterPtr)
  printAndReturn(gmdFindRecord,3,int )
}

/** Find data record of a symbol (Python)
 * @param pgmd gmd object handle
 * @param symPtr Handle to a symbol when reading sequentially
 * @param keyStr Array of strings containing the unique elements
 * @param status Indicator for error free execution (true/1 means OK, false/0 means Error)
 */
void * GMD_CALLCONV d_gmdFindRecordPy (gmdHandle_t pgmd, void *symPtr, const char *keyStr[], int *status)
{
  int d_s[]={1,1,55,20};
  GAMS_UNUSED(pgmd)
  GAMS_UNUSED(symPtr)
  GAMS_UNUSED(keyStr)
  GAMS_UNUSED(status)
  printAndReturn(gmdFindRecordPy,3,void *)
}

/** Receive the first record of a symbol
 * @param pgmd gmd object handle
 * @param symPtr Handle to a symbol when reading sequentially
 * @param symIterPtr Handle to a SymbolIterator
 */
int  GMD_CALLCONV d_gmdFindFirstRecord (gmdHandle_t pgmd, void *symPtr, void **symIterPtr)
{
  int d_s[]={15,1,2};
  GAMS_UNUSED(pgmd)
  GAMS_UNUSED(symPtr)
  GAMS_UNUSED(symIterPtr)
  printAndReturn(gmdFindFirstRecord,2,int )
}

/** Receive the first record of a symbol (Python)
 * @param pgmd gmd object handle
 * @param symPtr Handle to a symbol when reading sequentially
 * @param status Indicator for error free execution (true/1 means OK, false/0 means Error)
 */
void * GMD_CALLCONV d_gmdFindFirstRecordPy (gmdHandle_t pgmd, void *symPtr, int *status)
{
  int d_s[]={1,1,20};
  GAMS_UNUSED(pgmd)
  GAMS_UNUSED(symPtr)
  GAMS_UNUSED(status)
  printAndReturn(gmdFindFirstRecordPy,2,void *)
}

/** Receive the first record of a symbol using the slice definition
 * @param pgmd gmd object handle
 * @param symPtr Handle to a symbol when reading sequentially
 * @param keyStr Array of strings containing the unique elements
 * @param symIterPtr Handle to a SymbolIterator
 */
int  GMD_CALLCONV d_gmdFindFirstRecordSlice (gmdHandle_t pgmd, void *symPtr, const char *keyStr[], void **symIterPtr)
{
  int d_s[]={15,1,55,2};
  GAMS_UNUSED(pgmd)
  GAMS_UNUSED(symPtr)
  GAMS_UNUSED(keyStr)
  GAMS_UNUSED(symIterPtr)
  printAndReturn(gmdFindFirstRecordSlice,3,int )
}

/** Receive the first record of a symbol using the slice definition (Python)
 * @param pgmd gmd object handle
 * @param symPtr Handle to a symbol when reading sequentially
 * @param keyStr Array of strings containing the unique elements
 * @param status Indicator for error free execution (true/1 means OK, false/0 means Error)
 */
void * GMD_CALLCONV d_gmdFindFirstRecordSlicePy (gmdHandle_t pgmd, void *symPtr, const char *keyStr[], int *status)
{
  int d_s[]={1,1,55,20};
  GAMS_UNUSED(pgmd)
  GAMS_UNUSED(symPtr)
  GAMS_UNUSED(keyStr)
  GAMS_UNUSED(status)
  printAndReturn(gmdFindFirstRecordSlicePy,3,void *)
}

/** Receive the last record of a symbol
 * @param pgmd gmd object handle
 * @param symPtr Handle to a symbol when reading sequentially
 * @param symIterPtr Handle to a SymbolIterator
 */
int  GMD_CALLCONV d_gmdFindLastRecord (gmdHandle_t pgmd, void *symPtr, void **symIterPtr)
{
  int d_s[]={15,1,2};
  GAMS_UNUSED(pgmd)
  GAMS_UNUSED(symPtr)
  GAMS_UNUSED(symIterPtr)
  printAndReturn(gmdFindLastRecord,2,int )
}

/** Receive the last record of a symbol (Python)
 * @param pgmd gmd object handle
 * @param symPtr Handle to a symbol when reading sequentially
 * @param status Indicator for error free execution (true/1 means OK, false/0 means Error)
 */
void * GMD_CALLCONV d_gmdFindLastRecordPy (gmdHandle_t pgmd, void *symPtr, int *status)
{
  int d_s[]={1,1,20};
  GAMS_UNUSED(pgmd)
  GAMS_UNUSED(symPtr)
  GAMS_UNUSED(status)
  printAndReturn(gmdFindLastRecordPy,2,void *)
}

/** Receive the last record of a symbol using the slice definition
 * @param pgmd gmd object handle
 * @param symPtr Handle to a symbol when reading sequentially
 * @param keyStr Array of strings containing the unique elements
 * @param symIterPtr Handle to a SymbolIterator
 */
int  GMD_CALLCONV d_gmdFindLastRecordSlice (gmdHandle_t pgmd, void *symPtr, const char *keyStr[], void **symIterPtr)
{
  int d_s[]={15,1,55,2};
  GAMS_UNUSED(pgmd)
  GAMS_UNUSED(symPtr)
  GAMS_UNUSED(keyStr)
  GAMS_UNUSED(symIterPtr)
  printAndReturn(gmdFindLastRecordSlice,3,int )
}

/** Receive the last record of a symbol using the slice definition (Python)
 * @param pgmd gmd object handle
 * @param symPtr Handle to a symbol when reading sequentially
 * @param keyStr Array of strings containing the unique elements
 * @param status Indicator for error free execution (true/1 means OK, false/0 means Error)
 */
void * GMD_CALLCONV d_gmdFindLastRecordSlicePy (gmdHandle_t pgmd, void *symPtr, const char *keyStr[], int *status)
{
  int d_s[]={1,1,55,20};
  GAMS_UNUSED(pgmd)
  GAMS_UNUSED(symPtr)
  GAMS_UNUSED(keyStr)
  GAMS_UNUSED(status)
  printAndReturn(gmdFindLastRecordSlicePy,3,void *)
}

/** Move symbol iterator to the next record
 * @param pgmd gmd object handle
 * @param symIterPtr Handle to a SymbolIterator
 */
int  GMD_CALLCONV d_gmdRecordMoveNext (gmdHandle_t pgmd, void *symIterPtr)
{
  int d_s[]={15,1};
  GAMS_UNUSED(pgmd)
  GAMS_UNUSED(symIterPtr)
  printAndReturn(gmdRecordMoveNext,1,int )
}

/** Check if it would be possible to move symbol iterator to the next record without moving it
 * @param pgmd gmd object handle
 * @param symIterPtr Handle to a SymbolIterator
 */
int  GMD_CALLCONV d_gmdRecordHasNext (gmdHandle_t pgmd, void *symIterPtr)
{
  int d_s[]={15,1};
  GAMS_UNUSED(pgmd)
  GAMS_UNUSED(symIterPtr)
  printAndReturn(gmdRecordHasNext,1,int )
}

/** Move symbol iterator to the previous record
 * @param pgmd gmd object handle
 * @param symIterPtr Handle to a SymbolIterator
 */
int  GMD_CALLCONV d_gmdRecordMovePrev (gmdHandle_t pgmd, void *symIterPtr)
{
  int d_s[]={15,1};
  GAMS_UNUSED(pgmd)
  GAMS_UNUSED(symIterPtr)
  printAndReturn(gmdRecordMovePrev,1,int )
}

/** Compare if both pointers point to same record
 * @param pgmd gmd object handle
 * @param symIterPtrSrc Handle to a Source SymbolIterator
 * @param symIterPtrtar Handle to a Target SymbolIterator
 */
int  GMD_CALLCONV d_gmdSameRecord (gmdHandle_t pgmd, void *symIterPtrSrc, void *symIterPtrtar)
{
  int d_s[]={15,1,1};
  GAMS_UNUSED(pgmd)
  GAMS_UNUSED(symIterPtrSrc)
  GAMS_UNUSED(symIterPtrtar)
  printAndReturn(gmdSameRecord,2,int )
}

/** Check if it would be possible to move symbol iterator to the previous record without moving it
 * @param pgmd gmd object handle
 * @param symIterPtr Handle to a SymbolIterator
 */
int  GMD_CALLCONV d_gmdRecordHasPrev (gmdHandle_t pgmd, void *symIterPtr)
{
  int d_s[]={15,1};
  GAMS_UNUSED(pgmd)
  GAMS_UNUSED(symIterPtr)
  printAndReturn(gmdRecordHasPrev,1,int )
}

/** Retrieve the string number for an entry
 * @param pgmd gmd object handle
 * @param symIterPtr Handle to a SymbolIterator
 * @param txt Text found for the entry
 */
int  GMD_CALLCONV d_gmdGetElemText (gmdHandle_t pgmd, void *symIterPtr, char *txt)
{
  int d_s[]={15,1,12};
  GAMS_UNUSED(pgmd)
  GAMS_UNUSED(symIterPtr)
  GAMS_UNUSED(txt)
  printAndReturn(gmdGetElemText,2,int )
}

/** Retrieve the the level of a record
 * @param pgmd gmd object handle
 * @param symIterPtr Handle to a SymbolIterator
 * @param value double value of the specific symbol record
 */
int  GMD_CALLCONV d_gmdGetLevel (gmdHandle_t pgmd, void *symIterPtr, double *value)
{
  int d_s[]={15,1,14};
  GAMS_UNUSED(pgmd)
  GAMS_UNUSED(symIterPtr)
  GAMS_UNUSED(value)
  printAndReturn(gmdGetLevel,2,int )
}

/** Retrieve the the lower bound of a record
 * @param pgmd gmd object handle
 * @param symIterPtr Handle to a SymbolIterator
 * @param value double value of the specific symbol record
 */
int  GMD_CALLCONV d_gmdGetLower (gmdHandle_t pgmd, void *symIterPtr, double *value)
{
  int d_s[]={15,1,14};
  GAMS_UNUSED(pgmd)
  GAMS_UNUSED(symIterPtr)
  GAMS_UNUSED(value)
  printAndReturn(gmdGetLower,2,int )
}

/** Retrieve the the upper bound of a record
 * @param pgmd gmd object handle
 * @param symIterPtr Handle to a SymbolIterator
 * @param value double value of the specific symbol record
 */
int  GMD_CALLCONV d_gmdGetUpper (gmdHandle_t pgmd, void *symIterPtr, double *value)
{
  int d_s[]={15,1,14};
  GAMS_UNUSED(pgmd)
  GAMS_UNUSED(symIterPtr)
  GAMS_UNUSED(value)
  printAndReturn(gmdGetUpper,2,int )
}

/** Retrieve the the marginal of a record
 * @param pgmd gmd object handle
 * @param symIterPtr Handle to a SymbolIterator
 * @param value double value of the specific symbol record
 */
int  GMD_CALLCONV d_gmdGetMarginal (gmdHandle_t pgmd, void *symIterPtr, double *value)
{
  int d_s[]={15,1,14};
  GAMS_UNUSED(pgmd)
  GAMS_UNUSED(symIterPtr)
  GAMS_UNUSED(value)
  printAndReturn(gmdGetMarginal,2,int )
}

/** Retrieve the the scale of a record
 * @param pgmd gmd object handle
 * @param symIterPtr Handle to a SymbolIterator
 * @param value double value of the specific symbol record
 */
int  GMD_CALLCONV d_gmdGetScale (gmdHandle_t pgmd, void *symIterPtr, double *value)
{
  int d_s[]={15,1,14};
  GAMS_UNUSED(pgmd)
  GAMS_UNUSED(symIterPtr)
  GAMS_UNUSED(value)
  printAndReturn(gmdGetScale,2,int )
}

/** Retrieve the string number for an entry
 * @param pgmd gmd object handle
 * @param symIterPtr Handle to a SymbolIterator
 * @param txt Text found for the entry
 */
int  GMD_CALLCONV d_gmdSetElemText (gmdHandle_t pgmd, void *symIterPtr, const char *txt)
{
  int d_s[]={15,1,11};
  GAMS_UNUSED(pgmd)
  GAMS_UNUSED(symIterPtr)
  GAMS_UNUSED(txt)
  printAndReturn(gmdSetElemText,2,int )
}

/** Set the the level of a record
 * @param pgmd gmd object handle
 * @param symIterPtr Handle to a SymbolIterator
 * @param value double value of the specific symbol record
 */
int  GMD_CALLCONV d_gmdSetLevel (gmdHandle_t pgmd, void *symIterPtr, double value)
{
  int d_s[]={15,1,13};
  GAMS_UNUSED(pgmd)
  GAMS_UNUSED(symIterPtr)
  GAMS_UNUSED(value)
  printAndReturn(gmdSetLevel,2,int )
}

/** Set the the lower bound of a record
 * @param pgmd gmd object handle
 * @param symIterPtr Handle to a SymbolIterator
 * @param value double value of the specific symbol record
 */
int  GMD_CALLCONV d_gmdSetLower (gmdHandle_t pgmd, void *symIterPtr, double value)
{
  int d_s[]={15,1,13};
  GAMS_UNUSED(pgmd)
  GAMS_UNUSED(symIterPtr)
  GAMS_UNUSED(value)
  printAndReturn(gmdSetLower,2,int )
}

/** Set the the upper bound of a record
 * @param pgmd gmd object handle
 * @param symIterPtr Handle to a SymbolIterator
 * @param value double value of the specific symbol record
 */
int  GMD_CALLCONV d_gmdSetUpper (gmdHandle_t pgmd, void *symIterPtr, double value)
{
  int d_s[]={15,1,13};
  GAMS_UNUSED(pgmd)
  GAMS_UNUSED(symIterPtr)
  GAMS_UNUSED(value)
  printAndReturn(gmdSetUpper,2,int )
}

/** Set the the marginal of a record
 * @param pgmd gmd object handle
 * @param symIterPtr Handle to a SymbolIterator
 * @param value double value of the specific symbol record
 */
int  GMD_CALLCONV d_gmdSetMarginal (gmdHandle_t pgmd, void *symIterPtr, double value)
{
  int d_s[]={15,1,13};
  GAMS_UNUSED(pgmd)
  GAMS_UNUSED(symIterPtr)
  GAMS_UNUSED(value)
  printAndReturn(gmdSetMarginal,2,int )
}

/** Set the the scale of a record
 * @param pgmd gmd object handle
 * @param symIterPtr Handle to a SymbolIterator
 * @param value double value of the specific symbol record
 */
int  GMD_CALLCONV d_gmdSetScale (gmdHandle_t pgmd, void *symIterPtr, double value)
{
  int d_s[]={15,1,13};
  GAMS_UNUSED(pgmd)
  GAMS_UNUSED(symIterPtr)
  GAMS_UNUSED(value)
  printAndReturn(gmdSetScale,2,int )
}

/** Set the UserInfo of a symbol
 * @param pgmd gmd object handle
 * @param symPtr Handle to a symbol when reading sequentially
 * @param value double value of the specific symbol record
 */
int  GMD_CALLCONV d_gmdSetUserInfo (gmdHandle_t pgmd, void *symPtr, int value)
{
  int d_s[]={15,1,3};
  GAMS_UNUSED(pgmd)
  GAMS_UNUSED(symPtr)
  GAMS_UNUSED(value)
  printAndReturn(gmdSetUserInfo,2,int )
}

/** Add a new record to a symbol
 * @param pgmd gmd object handle
 * @param symPtr Handle to a symbol when reading sequentially
 * @param keyStr Array of strings containing the unique elements
 * @param symIterPtr Handle to a SymbolIterator
 */
int  GMD_CALLCONV d_gmdAddRecord (gmdHandle_t pgmd, void *symPtr, const char *keyStr[], void **symIterPtr)
{
  int d_s[]={15,1,55,2};
  GAMS_UNUSED(pgmd)
  GAMS_UNUSED(symPtr)
  GAMS_UNUSED(keyStr)
  GAMS_UNUSED(symIterPtr)
  printAndReturn(gmdAddRecord,3,int )
}

/** Add a new record to a symbol (Python)
 * @param pgmd gmd object handle
 * @param symPtr Handle to a symbol when reading sequentially
 * @param keyStr Array of strings containing the unique elements
 * @param status 
 */
void * GMD_CALLCONV d_gmdAddRecordPy (gmdHandle_t pgmd, void *symPtr, const char *keyStr[], int *status)
{
  int d_s[]={1,1,55,20};
  GAMS_UNUSED(pgmd)
  GAMS_UNUSED(symPtr)
  GAMS_UNUSED(keyStr)
  GAMS_UNUSED(status)
  printAndReturn(gmdAddRecordPy,3,void *)
}

/** Merge a record to a symbol
 * @param pgmd gmd object handle
 * @param symPtr Handle to a symbol when reading sequentially
 * @param keyStr Array of strings containing the unique elements
 * @param symIterPtr Handle to a SymbolIterator
 */
int  GMD_CALLCONV d_gmdMergeRecord (gmdHandle_t pgmd, void *symPtr, const char *keyStr[], void **symIterPtr)
{
  int d_s[]={15,1,55,2};
  GAMS_UNUSED(pgmd)
  GAMS_UNUSED(symPtr)
  GAMS_UNUSED(keyStr)
  GAMS_UNUSED(symIterPtr)
  printAndReturn(gmdMergeRecord,3,int )
}

/** Merge a record to a symbol (Python)
 * @param pgmd gmd object handle
 * @param symPtr Handle to a symbol when reading sequentially
 * @param keyStr Array of strings containing the unique elements
 * @param status 
 */
void * GMD_CALLCONV d_gmdMergeRecordPy (gmdHandle_t pgmd, void *symPtr, const char *keyStr[], int *status)
{
  int d_s[]={1,1,55,20};
  GAMS_UNUSED(pgmd)
  GAMS_UNUSED(symPtr)
  GAMS_UNUSED(keyStr)
  GAMS_UNUSED(status)
  printAndReturn(gmdMergeRecordPy,3,void *)
}

/** Merge a record with int keys to a symbol
 * @param pgmd gmd object handle
 * @param symPtr Handle to a symbol when reading sequentially
 * @param keyInt Array of int containing the unique elements
 * @param checkUEL 
 * @param wantSymIterPtr 
 * @param symIterPtr Handle to a SymbolIterator
 * @param haveValues 
 * @param values double values for all symbol types
 */
int  GMD_CALLCONV d_gmdMergeRecordInt (gmdHandle_t pgmd, void *symPtr, const int keyInt[], int checkUEL, int wantSymIterPtr, void **symIterPtr, int haveValues, const double values[])
{
  int d_s[]={15,1,51,15,15,2,15,53};
  GAMS_UNUSED(pgmd)
  GAMS_UNUSED(symPtr)
  GAMS_UNUSED(keyInt)
  GAMS_UNUSED(checkUEL)
  GAMS_UNUSED(wantSymIterPtr)
  GAMS_UNUSED(symIterPtr)
  GAMS_UNUSED(haveValues)
  GAMS_UNUSED(values)
  printAndReturn(gmdMergeRecordInt,7,int )
}

/** Merge a record with int keys to a symbol (Python)
 * @param pgmd gmd object handle
 * @param symPtr Handle to a symbol when reading sequentially
 * @param keyInt Array of int containing the unique elements
 * @param checkUEL 
 * @param wantSymIterPtr 
 * @param haveValues 
 * @param values double values for all symbol types
 * @param status Indicator for error free execution (true/1 means OK, false/0 means Error)
 */
void * GMD_CALLCONV d_gmdMergeRecordIntPy (gmdHandle_t pgmd, void *symPtr, const int keyInt[], int checkUEL, int wantSymIterPtr, int haveValues, const double values[], int *status)
{
  int d_s[]={1,1,51,15,15,15,53,20};
  GAMS_UNUSED(pgmd)
  GAMS_UNUSED(symPtr)
  GAMS_UNUSED(keyInt)
  GAMS_UNUSED(checkUEL)
  GAMS_UNUSED(wantSymIterPtr)
  GAMS_UNUSED(haveValues)
  GAMS_UNUSED(values)
  GAMS_UNUSED(status)
  printAndReturn(gmdMergeRecordIntPy,7,void *)
}

/** Merge a set record with int keys to a set symbol
 * @param pgmd gmd object handle
 * @param symPtr Handle to a symbol when reading sequentially
 * @param keyInt Array of int containing the unique elements
 * @param checkUEL 
 * @param wantSymIterPtr 
 * @param symIterPtr Handle to a SymbolIterator
 * @param eText 
 */
int  GMD_CALLCONV d_gmdMergeSetRecordInt (gmdHandle_t pgmd, void *symPtr, const int keyInt[], int checkUEL, int wantSymIterPtr, void **symIterPtr, const char *eText)
{
  int d_s[]={15,1,51,15,15,2,11};
  GAMS_UNUSED(pgmd)
  GAMS_UNUSED(symPtr)
  GAMS_UNUSED(keyInt)
  GAMS_UNUSED(checkUEL)
  GAMS_UNUSED(wantSymIterPtr)
  GAMS_UNUSED(symIterPtr)
  GAMS_UNUSED(eText)
  printAndReturn(gmdMergeSetRecordInt,6,int )
}

/** Merge a set record with int keys to a set symbol (Python)
 * @param pgmd gmd object handle
 * @param symPtr Handle to a symbol when reading sequentially
 * @param keyInt Array of int containing the unique elements
 * @param checkUEL 
 * @param wantSymIterPtr 
 * @param eText 
 * @param status Indicator for error free execution (true/1 means OK, false/0 means Error)
 */
void * GMD_CALLCONV d_gmdMergeSetRecordIntPy (gmdHandle_t pgmd, void *symPtr, const int keyInt[], int checkUEL, int wantSymIterPtr, const char *eText, int *status)
{
  int d_s[]={1,1,51,15,15,11,20};
  GAMS_UNUSED(pgmd)
  GAMS_UNUSED(symPtr)
  GAMS_UNUSED(keyInt)
  GAMS_UNUSED(checkUEL)
  GAMS_UNUSED(wantSymIterPtr)
  GAMS_UNUSED(eText)
  GAMS_UNUSED(status)
  printAndReturn(gmdMergeSetRecordIntPy,6,void *)
}

/** Deprecated use gmdMergeRecordInt
 * @param pgmd gmd object handle
 * @param symPtr Handle to a symbol when reading sequentially
 * @param keyInt Array of int containing the unique elements
 * @param values double values for all symbol types
 * @param eText 
 */
int  GMD_CALLCONV d_gmdAddRecordRaw (gmdHandle_t pgmd, void *symPtr, const int keyInt[], const double values[], const char *eText)
{
  int d_s[]={15,1,51,53,11};
  GAMS_UNUSED(pgmd)
  GAMS_UNUSED(symPtr)
  GAMS_UNUSED(keyInt)
  GAMS_UNUSED(values)
  GAMS_UNUSED(eText)
  printAndReturn(gmdAddRecordRaw,4,int )
}

/** Deletes one particular record
 * @param pgmd gmd object handle
 * @param symIterPtr Handle to a SymbolIterator
 */
int  GMD_CALLCONV d_gmdDeleteRecord (gmdHandle_t pgmd, void *symIterPtr)
{
  int d_s[]={15,1};
  GAMS_UNUSED(pgmd)
  GAMS_UNUSED(symIterPtr)
  printAndReturn(gmdDeleteRecord,1,int )
}

/** Retrieve the full record
 * @param pgmd gmd object handle
 * @param symIterPtr Handle to a SymbolIterator
 * @param aDim dimension of the symbol
 * @param keyInt Array of int containing the unique elements
 * @param values double values for all symbol types
 */
int  GMD_CALLCONV d_gmdGetRecordRaw (gmdHandle_t pgmd, void *symIterPtr, int aDim, int keyInt[], double values[])
{
  int d_s[]={15,1,3,52,54};
  GAMS_UNUSED(pgmd)
  GAMS_UNUSED(symIterPtr)
  GAMS_UNUSED(aDim)
  GAMS_UNUSED(keyInt)
  GAMS_UNUSED(values)
  printAndReturn(gmdGetRecordRaw,4,int )
}

/** Retrieve the keys of a record
 * @param pgmd gmd object handle
 * @param symIterPtr Handle to a SymbolIterator
 * @param aDim dimension of the symbol
 * @param keyStr Array of strings containing the unique elements
 */
int  GMD_CALLCONV d_gmdGetKeys (gmdHandle_t pgmd, void *symIterPtr, int aDim, char *keyStr[])
{
  int d_s[]={15,1,3,56};
  GAMS_UNUSED(pgmd)
  GAMS_UNUSED(symIterPtr)
  GAMS_UNUSED(aDim)
  GAMS_UNUSED(keyStr)
  printAndReturn(gmdGetKeys,3,int )
}

/** Retrieve a key element of a record
 * @param pgmd gmd object handle
 * @param symIterPtr Handle to a SymbolIterator
 * @param idx index of a symbol in the GMD object / of a key from a record
 * @param keyStr Array of strings containing the unique elements
 */
int  GMD_CALLCONV d_gmdGetKey (gmdHandle_t pgmd, void *symIterPtr, int idx, char *keyStr)
{
  int d_s[]={15,1,3,12};
  GAMS_UNUSED(pgmd)
  GAMS_UNUSED(symIterPtr)
  GAMS_UNUSED(idx)
  GAMS_UNUSED(keyStr)
  printAndReturn(gmdGetKey,3,int )
}

/** Retrieve domain information
 * @param pgmd gmd object handle
 * @param symPtr Handle to a symbol when reading sequentially
 * @param aDim dimension of the symbol
 * @param vDomPtrOut vector of domain symbols (output for GMD)
 * @param keyStr Array of strings containing the unique elements
 */
int  GMD_CALLCONV d_gmdGetDomain (gmdHandle_t pgmd, void *symPtr, int aDim, void **vDomPtrOut, char *keyStr[])
{
  int d_s[]={15,1,3,2,56};
  GAMS_UNUSED(pgmd)
  GAMS_UNUSED(symPtr)
  GAMS_UNUSED(aDim)
  GAMS_UNUSED(vDomPtrOut)
  GAMS_UNUSED(keyStr)
  printAndReturn(gmdGetDomain,4,int )
}

/** Copy the symbol iterator
 * @param pgmd gmd object handle
 * @param symIterPtrSrc Handle to a Source SymbolIterator
 * @param symIterPtrtar 
 */
int  GMD_CALLCONV d_gmdCopySymbolIterator (gmdHandle_t pgmd, void *symIterPtrSrc, void **symIterPtrtar)
{
  int d_s[]={15,1,2};
  GAMS_UNUSED(pgmd)
  GAMS_UNUSED(symIterPtrSrc)
  GAMS_UNUSED(symIterPtrtar)
  printAndReturn(gmdCopySymbolIterator,2,int )
}

/** Copy the symbol iterator (Python)
 * @param pgmd gmd object handle
 * @param symIterPtrSrc Handle to a Source SymbolIterator
 * @param status Indicator for error free execution (true/1 means OK, false/0 means Error)
 */
void * GMD_CALLCONV d_gmdCopySymbolIteratorPy (gmdHandle_t pgmd, void *symIterPtrSrc, int *status)
{
  int d_s[]={1,1,20};
  GAMS_UNUSED(pgmd)
  GAMS_UNUSED(symIterPtrSrc)
  GAMS_UNUSED(status)
  printAndReturn(gmdCopySymbolIteratorPy,2,void *)
}

/** Free the symbol iterator
 * @param pgmd gmd object handle
 * @param symIterPtr Handle to a SymbolIterator
 */
int  GMD_CALLCONV d_gmdFreeSymbolIterator (gmdHandle_t pgmd, void *symIterPtr)
{
  int d_s[]={15,1};
  GAMS_UNUSED(pgmd)
  GAMS_UNUSED(symIterPtr)
  printAndReturn(gmdFreeSymbolIterator,1,int )
}

/** Add a uel to universe
 * @param pgmd gmd object handle
 * @param uel 
 * @param uelNr 
 */
int  GMD_CALLCONV d_gmdMergeUel (gmdHandle_t pgmd, const char *uel, int *uelNr)
{
  int d_s[]={15,11,4};
  GAMS_UNUSED(pgmd)
  GAMS_UNUSED(uel)
  GAMS_UNUSED(uelNr)
  printAndReturn(gmdMergeUel,2,int )
}

/** Retrieve uel by index
 * @param pgmd gmd object handle
 * @param uelNr 
 * @param keyStr Array of strings containing the unique elements
 */
int  GMD_CALLCONV d_gmdGetUelByIndex (gmdHandle_t pgmd, int uelNr, char *keyStr)
{
  int d_s[]={15,3,12};
  GAMS_UNUSED(pgmd)
  GAMS_UNUSED(uelNr)
  GAMS_UNUSED(keyStr)
  printAndReturn(gmdGetUelByIndex,2,int )
}

/** Retrieve index of uel with label
 * @param pgmd gmd object handle
 * @param uelLabel 
 * @param uelNr 
 */
int  GMD_CALLCONV d_gmdFindUel (gmdHandle_t pgmd, const char *uelLabel, int *uelNr)
{
  int d_s[]={15,11,4};
  GAMS_UNUSED(pgmd)
  GAMS_UNUSED(uelLabel)
  GAMS_UNUSED(uelNr)
  printAndReturn(gmdFindUel,2,int )
}

/** Retrieve an array of uel counters. Counts how often uel at position n is used by symbols in vDomPtrIn.
 * @param pgmd gmd object handle
 * @param vDomPtrIn vector of symbol pointers (input for GMD)
 * @param lenvDomPtrIn Length of vDomPtrIn
 * @param uelList Array of counters.
 * @param sizeUelList size of uelList
 */
int  GMD_CALLCONV d_gmdGetSymbolsUels (gmdHandle_t pgmd, void **vDomPtrIn, int lenvDomPtrIn, int uelList[], int sizeUelList)
{
  int d_s[]={15,2,3,8,3};
  GAMS_UNUSED(pgmd)
  GAMS_UNUSED(vDomPtrIn)
  GAMS_UNUSED(lenvDomPtrIn)
  GAMS_UNUSED(uelList)
  GAMS_UNUSED(sizeUelList)
  printAndReturn(gmdGetSymbolsUels,4,int )
}

/** Query some information from object
 * @param pgmd gmd object handle
 * @param infoKey access key to an information record
 * @param ival integer valued parameter
 * @param dval double valued parameter
 * @param sval string valued parameter
 */
int  GMD_CALLCONV d_gmdInfo (gmdHandle_t pgmd, int infoKey, int *ival, double *dval, char *sval)
{
  int d_s[]={15,3,4,14,12};
  GAMS_UNUSED(pgmd)
  GAMS_UNUSED(infoKey)
  GAMS_UNUSED(ival)
  GAMS_UNUSED(dval)
  GAMS_UNUSED(sval)
  printAndReturn(gmdInfo,4,int )
}

/** Query some information from a symbol
 * @param pgmd gmd object handle
 * @param symPtr Handle to a symbol when reading sequentially
 * @param infoKey access key to an information record
 * @param ival integer valued parameter
 * @param dval double valued parameter
 * @param sval string valued parameter
 */
int  GMD_CALLCONV d_gmdSymbolInfo (gmdHandle_t pgmd, void *symPtr, int infoKey, int *ival, double *dval, char *sval)
{
  int d_s[]={15,1,3,4,14,12};
  GAMS_UNUSED(pgmd)
  GAMS_UNUSED(symPtr)
  GAMS_UNUSED(infoKey)
  GAMS_UNUSED(ival)
  GAMS_UNUSED(dval)
  GAMS_UNUSED(sval)
  printAndReturn(gmdSymbolInfo,5,int )
}

/** Query symbol dimension
 * @param pgmd gmd object handle
 * @param symPtr Handle to a symbol when reading sequentially
 * @param aDim dimension of the symbol
 */
int  GMD_CALLCONV d_gmdSymbolDim (gmdHandle_t pgmd, void *symPtr, int *aDim)
{
  int d_s[]={15,1,4};
  GAMS_UNUSED(pgmd)
  GAMS_UNUSED(symPtr)
  GAMS_UNUSED(aDim)
  printAndReturn(gmdSymbolDim,2,int )
}

/** Query symbol type
 * @param pgmd gmd object handle
 * @param symPtr Handle to a symbol when reading sequentially
 * @param stype type of a symbol
 */
int  GMD_CALLCONV d_gmdSymbolType (gmdHandle_t pgmd, void *symPtr, int *stype)
{
  int d_s[]={15,1,4};
  GAMS_UNUSED(pgmd)
  GAMS_UNUSED(symPtr)
  GAMS_UNUSED(stype)
  printAndReturn(gmdSymbolType,2,int )
}

/** Write GDX file of current database
 * @param pgmd gmd object handle
 * @param fileName File name of the gdx file
 * @param noDomChk Do not use real domains but just relaxed ones
 */
int  GMD_CALLCONV d_gmdWriteGDX (gmdHandle_t pgmd, const char *fileName, int noDomChk)
{
  int d_s[]={15,11,15};
  GAMS_UNUSED(pgmd)
  GAMS_UNUSED(fileName)
  GAMS_UNUSED(noDomChk)
  printAndReturn(gmdWriteGDX,2,int )
}

/** set special values for GMD and provide info about sp value mapping
 * @param pgmd gmd object handle
 * @param specVal Info about special value mapping effort
 * @param specValType Info about special value mapping effort
 */
int  GMD_CALLCONV d_gmdSetSpecialValuesX (gmdHandle_t pgmd, const double specVal[], int *specValType)
{
  int d_s[]={15,57,21};
  GAMS_UNUSED(pgmd)
  GAMS_UNUSED(specVal)
  GAMS_UNUSED(specValType)
  printAndReturn(gmdSetSpecialValuesX,2,int )
}

/** set special values for GMD
 * @param pgmd gmd object handle
 * @param specVal Info about special value mapping effort
 */
int  GMD_CALLCONV d_gmdSetSpecialValues (gmdHandle_t pgmd, const double specVal[])
{
  int d_s[]={15,57};
  GAMS_UNUSED(pgmd)
  GAMS_UNUSED(specVal)
  printAndReturn(gmdSetSpecialValues,1,int )
}

/** get special values for GMD
 * @param pgmd gmd object handle
 * @param specVal Info about special value mapping effort
 */
int  GMD_CALLCONV d_gmdGetSpecialValues (gmdHandle_t pgmd, double specVal[])
{
  int d_s[]={15,58};
  GAMS_UNUSED(pgmd)
  GAMS_UNUSED(specVal)
  printAndReturn(gmdGetSpecialValues,1,int )
}

/** get special values GMD currently accepts
 * @param pgmd gmd object handle
 * @param specVal Info about special value mapping effort
 */
int  GMD_CALLCONV d_gmdGetUserSpecialValues (gmdHandle_t pgmd, double specVal[])
{
  int d_s[]={15,58};
  GAMS_UNUSED(pgmd)
  GAMS_UNUSED(specVal)
  printAndReturn(gmdGetUserSpecialValues,1,int )
}

/** set debug mode
 * @param pgmd gmd object handle
 * @param debugLevel debug level
 */
int  GMD_CALLCONV d_gmdSetDebug (gmdHandle_t pgmd, int debugLevel)
{
  int d_s[]={15,3};
  GAMS_UNUSED(pgmd)
  GAMS_UNUSED(debugLevel)
  printAndReturn(gmdSetDebug,1,int )
}

/** Retrieve last error message
 * @param pgmd gmd object handle
 * @param msg Message
 */
int  GMD_CALLCONV d_gmdGetLastError (gmdHandle_t pgmd, char *msg)
{
  int d_s[]={15,12};
  GAMS_UNUSED(pgmd)
  GAMS_UNUSED(msg)
  printAndReturn(gmdGetLastError,1,int )
}

/** Print message to cmex log if initialized via gmdInitFromCMEX otherwise to stdout
 * @param pgmd gmd object handle
 * @param msg Message
 */
int  GMD_CALLCONV d_gmdPrintLog (gmdHandle_t pgmd, const char *msg)
{
  int d_s[]={15,11};
  GAMS_UNUSED(pgmd)
  GAMS_UNUSED(msg)
  printAndReturn(gmdPrintLog,1,int )
}

/** Start recording process to capture symbols that are written to
 * @param pgmd gmd object handle
 */
int  GMD_CALLCONV d_gmdStartWriteRecording (gmdHandle_t pgmd)
{
  int d_s[]={15};
  GAMS_UNUSED(pgmd)
  printAndReturn(gmdStartWriteRecording,0,int )
}

/** Stop recording process to capture symbols that are written to
 * @param pgmd gmd object handle
 */
int  GMD_CALLCONV d_gmdStopWriteRecording (gmdHandle_t pgmd)
{
  int d_s[]={15};
  GAMS_UNUSED(pgmd)
  printAndReturn(gmdStopWriteRecording,0,int )
}

/** Check domain of all symbols in DB
 * @param pgmd gmd object handle
 * @param dv Indicator that a record with domain violation was found
 */
int  GMD_CALLCONV d_gmdCheckDBDV (gmdHandle_t pgmd, int *dv)
{
  int d_s[]={15,20};
  GAMS_UNUSED(pgmd)
  GAMS_UNUSED(dv)
  printAndReturn(gmdCheckDBDV,1,int )
}

/** Check domain of a symbol
 * @param pgmd gmd object handle
 * @param symPtr Handle to a symbol when reading sequentially
 * @param dv Indicator that a record with domain violation was found
 */
int  GMD_CALLCONV d_gmdCheckSymbolDV (gmdHandle_t pgmd, void *symPtr, int *dv)
{
  int d_s[]={15,1,20};
  GAMS_UNUSED(pgmd)
  GAMS_UNUSED(symPtr)
  GAMS_UNUSED(dv)
  printAndReturn(gmdCheckSymbolDV,2,int )
}

/** Returns first domain violation in DB
 * @param pgmd gmd object handle
 * @param dvHandle Handle to retrieve multiple records with domain violation
 */
int  GMD_CALLCONV d_gmdGetFirstDBDV (gmdHandle_t pgmd, void **dvHandle)
{
  int d_s[]={15,2};
  GAMS_UNUSED(pgmd)
  GAMS_UNUSED(dvHandle)
  printAndReturn(gmdGetFirstDBDV,1,int )
}

/** Returns first domain violation in DB (Python)
 * @param pgmd gmd object handle
 * @param status Indicator for error free execution (true/1 means OK, false/0 means Error)
 */
void * GMD_CALLCONV d_gmdGetFirstDBDVPy (gmdHandle_t pgmd, int *status)
{
  int d_s[]={1,20};
  GAMS_UNUSED(pgmd)
  GAMS_UNUSED(status)
  printAndReturn(gmdGetFirstDBDVPy,1,void *)
}

/** Returns first record with domain violation in a particular symbol
 * @param pgmd gmd object handle
 * @param symPtr Handle to a symbol when reading sequentially
 * @param dvHandle Handle to retrieve multiple records with domain violation
 */
int  GMD_CALLCONV d_gmdGetFirstDVInSymbol (gmdHandle_t pgmd, void *symPtr, void **dvHandle)
{
  int d_s[]={15,1,2};
  GAMS_UNUSED(pgmd)
  GAMS_UNUSED(symPtr)
  GAMS_UNUSED(dvHandle)
  printAndReturn(gmdGetFirstDVInSymbol,2,int )
}

/** Returns first record with domain violation in a particular symbol (Python)
 * @param pgmd gmd object handle
 * @param symPtr Handle to a symbol when reading sequentially
 * @param status Indicator for error free execution (true/1 means OK, false/0 means Error)
 */
void * GMD_CALLCONV d_gmdGetFirstDVInSymbolPy (gmdHandle_t pgmd, void *symPtr, int *status)
{
  int d_s[]={1,1,20};
  GAMS_UNUSED(pgmd)
  GAMS_UNUSED(symPtr)
  GAMS_UNUSED(status)
  printAndReturn(gmdGetFirstDVInSymbolPy,2,void *)
}

/** End of domain checking needs to be called after a call to gmdGetFirstDBDV
 * @param pgmd gmd object handle
 */
int  GMD_CALLCONV d_gmdDomainCheckDone (gmdHandle_t pgmd)
{
  int d_s[]={15};
  GAMS_UNUSED(pgmd)
  printAndReturn(gmdDomainCheckDone,0,int )
}

/** If nextavail is true dvHandle has the first domain violation of the next symbol with domain violations
 * @param pgmd gmd object handle
 * @param dvHandle Handle to retrieve multiple records with domain violation
 * @param nextavail indicator for next record available
 */
int  GMD_CALLCONV d_gmdGetFirstDVInNextSymbol (gmdHandle_t pgmd, void *dvHandle, int *nextavail)
{
  int d_s[]={15,1,20};
  GAMS_UNUSED(pgmd)
  GAMS_UNUSED(dvHandle)
  GAMS_UNUSED(nextavail)
  printAndReturn(gmdGetFirstDVInNextSymbol,2,int )
}

/** If nextavail is true dvHandle has the next domain violation in the same symbol
 * @param pgmd gmd object handle
 * @param dvHandle Handle to retrieve multiple records with domain violation
 * @param nextavail indicator for next record available
 */
int  GMD_CALLCONV d_gmdMoveNextDVInSymbol (gmdHandle_t pgmd, void *dvHandle, int *nextavail)
{
  int d_s[]={15,1,20};
  GAMS_UNUSED(pgmd)
  GAMS_UNUSED(dvHandle)
  GAMS_UNUSED(nextavail)
  printAndReturn(gmdMoveNextDVInSymbol,2,int )
}

/** Free DV handle
 * @param pgmd gmd object handle
 * @param dvHandle Handle to retrieve multiple records with domain violation
 */
int  GMD_CALLCONV d_gmdFreeDVHandle (gmdHandle_t pgmd, void *dvHandle)
{
  int d_s[]={15,1};
  GAMS_UNUSED(pgmd)
  GAMS_UNUSED(dvHandle)
  printAndReturn(gmdFreeDVHandle,1,int )
}

/** Returns symbol of DV record
 * @param pgmd gmd object handle
 * @param dvHandle Handle to retrieve multiple records with domain violation
 * @param symPtr Handle to a symbol when reading sequentially
 */
int  GMD_CALLCONV d_gmdGetDVSymbol (gmdHandle_t pgmd, void *dvHandle, void **symPtr)
{
  int d_s[]={15,1,2};
  GAMS_UNUSED(pgmd)
  GAMS_UNUSED(dvHandle)
  GAMS_UNUSED(symPtr)
  printAndReturn(gmdGetDVSymbol,2,int )
}

/** Returns symbol of DV record (Python)
 * @param pgmd gmd object handle
 * @param dvHandle Handle to retrieve multiple records with domain violation
 * @param status Indicator for error free execution (true/1 means OK, false/0 means Error)
 */
void * GMD_CALLCONV d_gmdGetDVSymbolPy (gmdHandle_t pgmd, void *dvHandle, int *status)
{
  int d_s[]={1,1,20};
  GAMS_UNUSED(pgmd)
  GAMS_UNUSED(dvHandle)
  GAMS_UNUSED(status)
  printAndReturn(gmdGetDVSymbolPy,2,void *)
}

/** Returns symbol record of DV record
 * @param pgmd gmd object handle
 * @param dvHandle Handle to retrieve multiple records with domain violation
 * @param symIterPtr Handle to a SymbolIterator
 */
int  GMD_CALLCONV d_gmdGetDVSymbolRecord (gmdHandle_t pgmd, void *dvHandle, void **symIterPtr)
{
  int d_s[]={15,1,2};
  GAMS_UNUSED(pgmd)
  GAMS_UNUSED(dvHandle)
  GAMS_UNUSED(symIterPtr)
  printAndReturn(gmdGetDVSymbolRecord,2,int )
}

/** Returns symbol record of DV record (Python)
 * @param pgmd gmd object handle
 * @param dvHandle Handle to retrieve multiple records with domain violation
 * @param status Indicator for error free execution (true/1 means OK, false/0 means Error)
 */
void * GMD_CALLCONV d_gmdGetDVSymbolRecordPy (gmdHandle_t pgmd, void *dvHandle, int *status)
{
  int d_s[]={1,1,20};
  GAMS_UNUSED(pgmd)
  GAMS_UNUSED(dvHandle)
  GAMS_UNUSED(status)
  printAndReturn(gmdGetDVSymbolRecordPy,2,void *)
}

/** Returns DV indicator array
 * @param pgmd gmd object handle
 * @param dvHandle Handle to retrieve multiple records with domain violation
 * @param viol a dim vector that has nonzero at a position where domain violation occured
 */
int  GMD_CALLCONV d_gmdGetDVIndicator (gmdHandle_t pgmd, void *dvHandle, int viol[])
{
  int d_s[]={15,1,8};
  GAMS_UNUSED(pgmd)
  GAMS_UNUSED(dvHandle)
  GAMS_UNUSED(viol)
  printAndReturn(gmdGetDVIndicator,2,int )
}

/** Initialize GMO synchronization
 * @param pgmd gmd object handle
 * @param gmoPtr Handle to GMO object
 */
int  GMD_CALLCONV d_gmdInitUpdate (gmdHandle_t pgmd, void *gmoPtr)
{
  int d_s[]={15,1};
  GAMS_UNUSED(pgmd)
  GAMS_UNUSED(gmoPtr)
  printAndReturn(gmdInitUpdate,1,int )
}

/** Updates model with data from database
 * @param pgmd gmd object handle
 * @param gamsSymPtr Handle to a symbol in the GAMS model
 * @param actionType what to update
 * @param dataSymPtr Handle to a symbol in containing data
 * @param updateType how to update
 * @param noMatchCnt Number of records in symbol that were not used for updating
 */
int  GMD_CALLCONV d_gmdUpdateModelSymbol (gmdHandle_t pgmd, void *gamsSymPtr, int actionType, void *dataSymPtr, int updateType, int *noMatchCnt)
{
  int d_s[]={15,1,3,1,3,21};
  GAMS_UNUSED(pgmd)
  GAMS_UNUSED(gamsSymPtr)
  GAMS_UNUSED(actionType)
  GAMS_UNUSED(dataSymPtr)
  GAMS_UNUSED(updateType)
  GAMS_UNUSED(noMatchCnt)
  printAndReturn(gmdUpdateModelSymbol,5,int )
}

/** Call GAMS solver
 * @param pgmd gmd object handle
 * @param solvername Name of solver to execute
 */
int  GMD_CALLCONV d_gmdCallSolver (gmdHandle_t pgmd, const char *solvername)
{
  int d_s[]={15,11};
  GAMS_UNUSED(pgmd)
  GAMS_UNUSED(solvername)
  printAndReturn(gmdCallSolver,1,int )
}

/** Call GAMS solver and return time used by gevCallSolver
 * @param pgmd gmd object handle
 * @param solvername Name of solver to execute
 * @param time 
 */
int  GMD_CALLCONV d_gmdCallSolverTimed (gmdHandle_t pgmd, const char *solvername, double *time)
{
  int d_s[]={15,11,14};
  GAMS_UNUSED(pgmd)
  GAMS_UNUSED(solvername)
  GAMS_UNUSED(time)
  printAndReturn(gmdCallSolverTimed,2,int )
}

/** Copy dense symbol content into dense array
 * @param pgmd gmd object handle
 * @param cube multi-dimensional dense array
 * @param vDim vector of dimensionality of cube
 * @param symPtr Handle to a symbol when reading sequentially
 * @param field record field of variable or equation
 */
int  GMD_CALLCONV d_gmdDenseSymbolToDenseArray (gmdHandle_t pgmd, void *cube, int vDim[], void *symPtr, int field)
{
  int d_s[]={15,1,8,1,3};
  GAMS_UNUSED(pgmd)
  GAMS_UNUSED(cube)
  GAMS_UNUSED(vDim)
  GAMS_UNUSED(symPtr)
  GAMS_UNUSED(field)
  printAndReturn(gmdDenseSymbolToDenseArray,4,int )
}

/** Copy sparse symbol content with domain info into dense array
 * @param pgmd gmd object handle
 * @param cube multi-dimensional dense array
 * @param vDim vector of dimensionality of cube
 * @param symPtr Handle to a symbol when reading sequentially
 * @param vDomPtr vector of domain symbols
 * @param field record field of variable or equation
 * @param nDropped Number of dropped records
 */
int  GMD_CALLCONV d_gmdSparseSymbolToDenseArray (gmdHandle_t pgmd, void *cube, int vDim[], void *symPtr, void **vDomPtr, int field, int *nDropped)
{
  int d_s[]={15,1,8,1,2,3,4};
  GAMS_UNUSED(pgmd)
  GAMS_UNUSED(cube)
  GAMS_UNUSED(vDim)
  GAMS_UNUSED(symPtr)
  GAMS_UNUSED(vDomPtr)
  GAMS_UNUSED(field)
  GAMS_UNUSED(nDropped)
  printAndReturn(gmdSparseSymbolToDenseArray,6,int )
}

/** Copy sparse symbol content with domain and squeezed domain info into dense array
 * @param pgmd gmd object handle
 * @param cube multi-dimensional dense array
 * @param vDim vector of dimensionality of cube
 * @param symPtr Handle to a symbol when reading sequentially
 * @param vDomSqueezePtr vector of squeezed domain symbols
 * @param vDomPtr vector of domain symbols
 * @param field record field of variable or equation
 * @param nDropped Number of dropped records
 */
int  GMD_CALLCONV d_gmdSparseSymbolToSqzdArray (gmdHandle_t pgmd, void *cube, int vDim[], void *symPtr, void **vDomSqueezePtr, void **vDomPtr, int field, int *nDropped)
{
  int d_s[]={15,1,8,1,2,2,3,4};
  GAMS_UNUSED(pgmd)
  GAMS_UNUSED(cube)
  GAMS_UNUSED(vDim)
  GAMS_UNUSED(symPtr)
  GAMS_UNUSED(vDomSqueezePtr)
  GAMS_UNUSED(vDomPtr)
  GAMS_UNUSED(field)
  GAMS_UNUSED(nDropped)
  printAndReturn(gmdSparseSymbolToSqzdArray,7,int )
}

/** Copy dense array into symbol with domain info
 * @param pgmd gmd object handle
 * @param symPtr 
 * @param vDomPtr vector of domain symbols
 * @param cube multi-dimensional dense array
 * @param vDim vector of dimensionality of cube
 */
int  GMD_CALLCONV d_gmdDenseArrayToSymbol (gmdHandle_t pgmd, void *symPtr, void **vDomPtr, void *cube, int vDim[])
{
  int d_s[]={15,1,2,1,8};
  GAMS_UNUSED(pgmd)
  GAMS_UNUSED(symPtr)
  GAMS_UNUSED(vDomPtr)
  GAMS_UNUSED(cube)
  GAMS_UNUSED(vDim)
  printAndReturn(gmdDenseArrayToSymbol,4,int )
}

/** Copy slices of dense array into symbol with domain info and slice domain info
 * @param pgmd gmd object handle
 * @param symPtr 
 * @param vDomSlicePtr vector of slice domain symbols
 * @param vDomPtr vector of domain symbols
 * @param cube multi-dimensional dense array
 * @param vDim vector of dimensionality of cube
 */
int  GMD_CALLCONV d_gmdDenseArraySlicesToSymbol (gmdHandle_t pgmd, void *symPtr, void **vDomSlicePtr, void **vDomPtr, void *cube, int vDim[])
{
  int d_s[]={15,1,2,2,1,8};
  GAMS_UNUSED(pgmd)
  GAMS_UNUSED(symPtr)
  GAMS_UNUSED(vDomSlicePtr)
  GAMS_UNUSED(vDomPtr)
  GAMS_UNUSED(cube)
  GAMS_UNUSED(vDim)
  printAndReturn(gmdDenseArraySlicesToSymbol,5,int )
}

/** Returns false for failure, true otherwise
 * @param pgmd gmd object handle
 * @param symPtr Symbol for which the record storage data structure should be changed or NULL to set default storage type for new symbols. When a symbol is supplied, it will be copied into a new symbol that utilizes the now chosen record container and the old symbol will be discarded. The symbol pointer (symPtr) will be updated to point to the new symbol.
 * @param storageType Data structure used to store symbol records (RB_TREE=0, VECTOR=1, GMS_TREE=2)
 */
int  GMD_CALLCONV d_gmdSelectRecordStorage (gmdHandle_t pgmd, void **symPtr, int storageType)
{
  int d_s[]={3,2,3};
  GAMS_UNUSED(pgmd)
  GAMS_UNUSED(symPtr)
  GAMS_UNUSED(storageType)
  printAndReturn(gmdSelectRecordStorage,2,int )
}

/** Select data structure for storing symbol records (Python)
 * @param pgmd gmd object handle
 * @param symPtr Symbol for which the record storage data structure should be changed or NULL to set default storage type for new symbols. When a symbol is supplied, it will be copied into a new symbol that utilizes the now chosen record container and the old symbol will be discarded. The returned pointer has the address of the new symbol.
 * @param storageType Data structure used to store symbol records (RB_TREE=0, VECTOR=1, GMS_TREE=2)
 * @param status Indicator for error free execution (true/1 means OK, false/0 means Error)
 */
void * GMD_CALLCONV d_gmdSelectRecordStoragePy (gmdHandle_t pgmd, void *symPtr, int storageType, int *status)
{
  int d_s[]={1,1,3,20};
  GAMS_UNUSED(pgmd)
  GAMS_UNUSED(symPtr)
  GAMS_UNUSED(storageType)
  GAMS_UNUSED(status)
  printAndReturn(gmdSelectRecordStoragePy,3,void *)
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
# define GMS_DLL_BASENAME "gmdcclib"
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

  LOADIT(XCreate, "XCreate");
  LOADIT(XCreateD, "XCreateD");
   

  LOADIT(XFree, "XFree");
  LOADIT(XCheck, "C__XCheck");
  LOADIT(XAPIVersion, "C__XAPIVersion");

  if (!XAPIVersion(5,errBuf,&cl))
    return 1;

#define CheckAndLoad(f,nargs,prefix) \
  if (!XCheck(#f,nargs,s,errBuf)) \
    f = &d_##f; \
  else { \
    LOADIT(f,prefix #f); \
  }
  {int s[]={15,11}; CheckAndLoad(gmdInitFromGDX,1,"C__"); }
  {int s[]={15,1}; CheckAndLoad(gmdInitFromDict,1,"C__"); }
  {int s[]={15,59,59,59,59,59,59,1}; CheckAndLoad(gmdInitFromCMEX,7,"C__"); }
  {int s[]={15,1}; CheckAndLoad(gmdInitFromDB,1,"C__"); }
  {int s[]={15,1}; CheckAndLoad(gmdRegisterGMO,1,"C__"); }
  {int s[]={15,15}; CheckAndLoad(gmdCloseGDX,1,"C__"); }
  {int s[]={15,11,3,3,3,11,2,55,2}; CheckAndLoad(gmdAddSymbolX,8,"C__"); }
  {int s[]={1,11,3,3,3,11,2,55,20}; CheckAndLoad(gmdAddSymbolXPy,8,"C__"); }
  {int s[]={15,11,3,3,3,11,2}; CheckAndLoad(gmdAddSymbol,6,"C__"); }
  {int s[]={1,11,3,3,3,11,20}; CheckAndLoad(gmdAddSymbolPy,6,"C__"); }
  {int s[]={15,11,2}; CheckAndLoad(gmdFindSymbol,2,"C__"); }
  {int s[]={1,11,20}; CheckAndLoad(gmdFindSymbolPy,2,"C__"); }
  {int s[]={15,11,2}; CheckAndLoad(gmdFindSymbolWithAlias,2,"C__"); }
  {int s[]={1,11,20}; CheckAndLoad(gmdFindSymbolWithAliasPy,2,"C__"); }
  {int s[]={15,3,2}; CheckAndLoad(gmdGetSymbolByIndex,2,"C__"); }
  {int s[]={1,3,20}; CheckAndLoad(gmdGetSymbolByIndexPy,2,"C__"); }
  {int s[]={15,3,2}; CheckAndLoad(gmdGetSymbolByNumber,2,"C__"); }
  {int s[]={1,3,20}; CheckAndLoad(gmdGetSymbolByNumberPy,2,"C__"); }
  {int s[]={15,1}; CheckAndLoad(gmdClearSymbol,1,"C__"); }
  {int s[]={15,1,1}; CheckAndLoad(gmdCopySymbol,2,"C__"); }
  {int s[]={15,1,55,2}; CheckAndLoad(gmdFindRecord,3,"C__"); }
  {int s[]={1,1,55,20}; CheckAndLoad(gmdFindRecordPy,3,"C__"); }
  {int s[]={15,1,2}; CheckAndLoad(gmdFindFirstRecord,2,"C__"); }
  {int s[]={1,1,20}; CheckAndLoad(gmdFindFirstRecordPy,2,"C__"); }
  {int s[]={15,1,55,2}; CheckAndLoad(gmdFindFirstRecordSlice,3,"C__"); }
  {int s[]={1,1,55,20}; CheckAndLoad(gmdFindFirstRecordSlicePy,3,"C__"); }
  {int s[]={15,1,2}; CheckAndLoad(gmdFindLastRecord,2,"C__"); }
  {int s[]={1,1,20}; CheckAndLoad(gmdFindLastRecordPy,2,"C__"); }
  {int s[]={15,1,55,2}; CheckAndLoad(gmdFindLastRecordSlice,3,"C__"); }
  {int s[]={1,1,55,20}; CheckAndLoad(gmdFindLastRecordSlicePy,3,"C__"); }
  {int s[]={15,1}; CheckAndLoad(gmdRecordMoveNext,1,"C__"); }
  {int s[]={15,1}; CheckAndLoad(gmdRecordHasNext,1,"C__"); }
  {int s[]={15,1}; CheckAndLoad(gmdRecordMovePrev,1,"C__"); }
  {int s[]={15,1,1}; CheckAndLoad(gmdSameRecord,2,"C__"); }
  {int s[]={15,1}; CheckAndLoad(gmdRecordHasPrev,1,"C__"); }
  {int s[]={15,1,12}; CheckAndLoad(gmdGetElemText,2,"C__"); }
  {int s[]={15,1,14}; CheckAndLoad(gmdGetLevel,2,"C__"); }
  {int s[]={15,1,14}; CheckAndLoad(gmdGetLower,2,"C__"); }
  {int s[]={15,1,14}; CheckAndLoad(gmdGetUpper,2,"C__"); }
  {int s[]={15,1,14}; CheckAndLoad(gmdGetMarginal,2,"C__"); }
  {int s[]={15,1,14}; CheckAndLoad(gmdGetScale,2,"C__"); }
  {int s[]={15,1,11}; CheckAndLoad(gmdSetElemText,2,"C__"); }
  {int s[]={15,1,13}; CheckAndLoad(gmdSetLevel,2,"C__"); }
  {int s[]={15,1,13}; CheckAndLoad(gmdSetLower,2,"C__"); }
  {int s[]={15,1,13}; CheckAndLoad(gmdSetUpper,2,"C__"); }
  {int s[]={15,1,13}; CheckAndLoad(gmdSetMarginal,2,"C__"); }
  {int s[]={15,1,13}; CheckAndLoad(gmdSetScale,2,"C__"); }
  {int s[]={15,1,3}; CheckAndLoad(gmdSetUserInfo,2,"C__"); }
  {int s[]={15,1,55,2}; CheckAndLoad(gmdAddRecord,3,"C__"); }
  {int s[]={1,1,55,20}; CheckAndLoad(gmdAddRecordPy,3,"C__"); }
  {int s[]={15,1,55,2}; CheckAndLoad(gmdMergeRecord,3,"C__"); }
  {int s[]={1,1,55,20}; CheckAndLoad(gmdMergeRecordPy,3,"C__"); }
  {int s[]={15,1,51,15,15,2,15,53}; CheckAndLoad(gmdMergeRecordInt,7,"C__"); }
  {int s[]={1,1,51,15,15,15,53,20}; CheckAndLoad(gmdMergeRecordIntPy,7,"C__"); }
  {int s[]={15,1,51,15,15,2,11}; CheckAndLoad(gmdMergeSetRecordInt,6,"C__"); }
  {int s[]={1,1,51,15,15,11,20}; CheckAndLoad(gmdMergeSetRecordIntPy,6,"C__"); }
  {int s[]={15,1,51,53,11}; CheckAndLoad(gmdAddRecordRaw,4,"C__"); }
  {int s[]={15,1}; CheckAndLoad(gmdDeleteRecord,1,"C__"); }
  {int s[]={15,1,3,52,54}; CheckAndLoad(gmdGetRecordRaw,4,"C__"); }
  {int s[]={15,1,3,56}; CheckAndLoad(gmdGetKeys,3,"C__"); }
  {int s[]={15,1,3,12}; CheckAndLoad(gmdGetKey,3,"C__"); }
  {int s[]={15,1,3,2,56}; CheckAndLoad(gmdGetDomain,4,"C__"); }
  {int s[]={15,1,2}; CheckAndLoad(gmdCopySymbolIterator,2,"C__"); }
  {int s[]={1,1,20}; CheckAndLoad(gmdCopySymbolIteratorPy,2,"C__"); }
  {int s[]={15,1}; CheckAndLoad(gmdFreeSymbolIterator,1,"C__"); }
  {int s[]={15,11,4}; CheckAndLoad(gmdMergeUel,2,"C__"); }
  {int s[]={15,3,12}; CheckAndLoad(gmdGetUelByIndex,2,"C__"); }
  {int s[]={15,11,4}; CheckAndLoad(gmdFindUel,2,"C__"); }
  {int s[]={15,2,3,8,3}; CheckAndLoad(gmdGetSymbolsUels,4,"C__"); }
  {int s[]={15,3,4,14,12}; CheckAndLoad(gmdInfo,4,"C__"); }
  {int s[]={15,1,3,4,14,12}; CheckAndLoad(gmdSymbolInfo,5,"C__"); }
  {int s[]={15,1,4}; CheckAndLoad(gmdSymbolDim,2,"C__"); }
  {int s[]={15,1,4}; CheckAndLoad(gmdSymbolType,2,"C__"); }
  {int s[]={15,11,15}; CheckAndLoad(gmdWriteGDX,2,"C__"); }
  {int s[]={15,57,21}; CheckAndLoad(gmdSetSpecialValuesX,2,"C__"); }
  {int s[]={15,57}; CheckAndLoad(gmdSetSpecialValues,1,"C__"); }
  {int s[]={15,58}; CheckAndLoad(gmdGetSpecialValues,1,"C__"); }
  {int s[]={15,58}; CheckAndLoad(gmdGetUserSpecialValues,1,"C__"); }
  {int s[]={15,3}; CheckAndLoad(gmdSetDebug,1,"C__"); }
  {int s[]={15,12}; CheckAndLoad(gmdGetLastError,1,"C__"); }
  {int s[]={15,11}; CheckAndLoad(gmdPrintLog,1,"C__"); }
  {int s[]={15}; CheckAndLoad(gmdStartWriteRecording,0,"C__"); }
  {int s[]={15}; CheckAndLoad(gmdStopWriteRecording,0,"C__"); }
  {int s[]={15,20}; CheckAndLoad(gmdCheckDBDV,1,"C__"); }
  {int s[]={15,1,20}; CheckAndLoad(gmdCheckSymbolDV,2,"C__"); }
  {int s[]={15,2}; CheckAndLoad(gmdGetFirstDBDV,1,"C__"); }
  {int s[]={1,20}; CheckAndLoad(gmdGetFirstDBDVPy,1,"C__"); }
  {int s[]={15,1,2}; CheckAndLoad(gmdGetFirstDVInSymbol,2,"C__"); }
  {int s[]={1,1,20}; CheckAndLoad(gmdGetFirstDVInSymbolPy,2,"C__"); }
  {int s[]={15}; CheckAndLoad(gmdDomainCheckDone,0,"C__"); }
  {int s[]={15,1,20}; CheckAndLoad(gmdGetFirstDVInNextSymbol,2,"C__"); }
  {int s[]={15,1,20}; CheckAndLoad(gmdMoveNextDVInSymbol,2,"C__"); }
  {int s[]={15,1}; CheckAndLoad(gmdFreeDVHandle,1,"C__"); }
  {int s[]={15,1,2}; CheckAndLoad(gmdGetDVSymbol,2,"C__"); }
  {int s[]={1,1,20}; CheckAndLoad(gmdGetDVSymbolPy,2,"C__"); }
  {int s[]={15,1,2}; CheckAndLoad(gmdGetDVSymbolRecord,2,"C__"); }
  {int s[]={1,1,20}; CheckAndLoad(gmdGetDVSymbolRecordPy,2,"C__"); }
  {int s[]={15,1,8}; CheckAndLoad(gmdGetDVIndicator,2,"C__"); }
  {int s[]={15,1}; CheckAndLoad(gmdInitUpdate,1,"C__"); }
  {int s[]={15,1,3,1,3,21}; CheckAndLoad(gmdUpdateModelSymbol,5,"C__"); }
  {int s[]={15,11}; CheckAndLoad(gmdCallSolver,1,"C__"); }
  {int s[]={15,11,14}; CheckAndLoad(gmdCallSolverTimed,2,"C__"); }
  {int s[]={15,1,8,1,3}; CheckAndLoad(gmdDenseSymbolToDenseArray,4,"C__"); }
  {int s[]={15,1,8,1,2,3,4}; CheckAndLoad(gmdSparseSymbolToDenseArray,6,"C__"); }
  {int s[]={15,1,8,1,2,2,3,4}; CheckAndLoad(gmdSparseSymbolToSqzdArray,7,"C__"); }
  {int s[]={15,1,2,1,8}; CheckAndLoad(gmdDenseArrayToSymbol,4,"C__"); }
  {int s[]={15,1,2,2,1,8}; CheckAndLoad(gmdDenseArraySlicesToSymbol,5,"C__"); }
  {int s[]={3,2,3}; CheckAndLoad(gmdSelectRecordStorage,2,"C__"); }
  {int s[]={1,1,3,20}; CheckAndLoad(gmdSelectRecordStoragePy,3,"C__"); }

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

int gmdGetReady (char *msgBuf, int msgBufSize)
{
  int rc;
  lock(libMutex);
  rc = libloader(NULL, NULL, msgBuf, msgBufSize);
  unlock(libMutex);
  return rc;
} /* gmdGetReady */

int gmdGetReadyD (const char *dirName, char *msgBuf, int msgBufSize)
{
  int rc;
  lock(libMutex);
  rc = libloader(dirName, NULL, msgBuf, msgBufSize);
  unlock(libMutex);
  return rc;
} /* gmdGetReadyD */

int gmdGetReadyL (const char *libName, char *msgBuf, int msgBufSize)
{
  char dirName[1024],fName[1024];
  int rc;
  extractFileDirFileName (libName, dirName, fName);
  lock(libMutex);
  rc = libloader(dirName, fName, msgBuf, msgBufSize);
  unlock(libMutex);
  return rc;
} /* gmdGetReadyL */

int gmdCreate (gmdHandle_t *pgmd, char *msgBuf, int msgBufSize)
{
  int gmdIsReady;

  gmdIsReady = gmdGetReady (msgBuf, msgBufSize);
  if (! gmdIsReady) {
    return 0;
  }
  assert(XCreate);
  XCreate(pgmd);
  if(pgmd == NULL)
  { strcpy(msgBuf,"Error while creating object"); return 0; }
  lock(objMutex);
  objectCount++;
  unlock(objMutex);
  return 1;                     /* return true on successful library load */
} /* gmdCreate */

int gmdCreateD (gmdHandle_t *pgmd, const char *dirName,
                char *msgBuf, int msgBufSize)
{
  int gmdIsReady;

  gmdIsReady = gmdGetReadyD (dirName, msgBuf, msgBufSize);
  if (! gmdIsReady) {
    return 0;
  }
  assert(XCreate);
  XCreate(pgmd);
  if(pgmd == NULL)
  { strcpy(msgBuf,"Error while creating object"); return 0; }
  lock(objMutex);
  objectCount++;
  unlock(objMutex);
  return 1;                     /* return true on successful library load */
} /* gmdCreateD */

int gmdCreateDD (gmdHandle_t *pgmd, const char *dirName,
                char *msgBuf, int msgBufSize)
{
  int gmdIsReady;
  gmdIsReady = gmdGetReadyD (dirName, msgBuf, msgBufSize);
  if (! gmdIsReady) {
    return 0;
  }
  assert(XCreateD);
  XCreateD(pgmd, dirName);
  if(pgmd == NULL)
  { strcpy(msgBuf,"Error while creating object"); return 0; }
  lock(objMutex);
  objectCount++;
  unlock(objMutex);
  return 1;                     /* return true on successful library load */
} /* gmdCreateD */


int gmdCreateL (gmdHandle_t *pgmd, const char *libName,
                char *msgBuf, int msgBufSize)
{
  int gmdIsReady;

  gmdIsReady = gmdGetReadyL (libName, msgBuf, msgBufSize);
  if (! gmdIsReady) {
    return 0;
  }
  assert(XCreate);
  XCreate(pgmd);
  if(pgmd == NULL)
  { strcpy(msgBuf,"Error while creating object"); return 0; }
  lock(objMutex);
  objectCount++;
  unlock(objMutex);
  return 1;                     /* return true on successful library load */
} /* gmdCreateL */

int gmdFree   (gmdHandle_t *pgmd)
{
  assert(XFree);
  XFree(pgmd); pgmd = NULL;
  lock(objMutex);
  objectCount--;
  unlock(objMutex);
  return 1;
} /* gmdFree */

int gmdLibraryLoaded(void)
{
  int rc;
  lock(libMutex);
  rc = isLoaded;
  unlock(libMutex);
  return rc;
} /* gmdLibraryLoaded */

int gmdLibraryUnload(void)
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
} /* gmdLibraryUnload */

int  gmdCorrectLibraryVersion(char *msgBuf, int msgBufLen)
{
  int cl;
  char localBuf[256];

  if (msgBuf && msgBufLen) msgBuf[0] = '\0';

  if (! isLoaded) {
    strncpy(msgBuf, "Library needs to be initialized first", msgBufLen);
    return 0;
  }

  if (NULL == XAPIVersion) {
    strncpy(msgBuf, "Function XAPIVersion not found", msgBufLen);
    return 0;
  }

  XAPIVersion(GMDAPIVERSION,localBuf,&cl);
  strncpy(msgBuf, localBuf, msgBufLen);

  if (1 == cl)
    return 1;
  else
    return 0;
}

int gmdGetScreenIndicator(void)
{
  return ScreenIndicator;
}

void gmdSetScreenIndicator(int scrind)
{
  ScreenIndicator = scrind ? 1 : 0;
}

int gmdGetExceptionIndicator(void)
{
   return ExceptionIndicator;
}

void gmdSetExceptionIndicator(int excind)
{
  ExceptionIndicator = excind ? 1 : 0;
}

int gmdGetExitIndicator(void)
{
  return ExitIndicator;
}

void gmdSetExitIndicator(int extind)
{
  ExitIndicator = extind ? 1 : 0;
}

gmdErrorCallback_t gmdGetErrorCallback(void)
{
  return ErrorCallBack;
}

void gmdSetErrorCallback(gmdErrorCallback_t func)
{
  lock(exceptMutex);
  ErrorCallBack = func;
  unlock(exceptMutex);
}

int gmdGetAPIErrorCount(void)
{
  return APIErrorCount;
}

void gmdSetAPIErrorCount(int ecnt)
{
  APIErrorCount = ecnt;
}

void gmdErrorHandling(const char *msg)
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

