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

#define DCT_MAIN
#include "dctmcc.h"

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
static dctErrorCallback_t ErrorCallBack = NULL;
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

void dctInitMutexes(void)
{
  int rc;
  if (0==MutexIsInitialized) {
    rc = GC_mutex_init (&libMutex);     if(0!=rc) dctErrorHandling("Problem initializing libMutex");
    rc = GC_mutex_init (&objMutex);     if(0!=rc) dctErrorHandling("Problem initializing objMutex");
    rc = GC_mutex_init (&exceptMutex);  if(0!=rc) dctErrorHandling("Problem initializing exceptMutex");
    MutexIsInitialized = 1;
  }
}

void dctFiniMutexes(void)
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
void dctInitMutexes(void) {}
void dctFiniMutexes(void) {}
#endif

#if !defined(GAMS_UNUSED)
#define GAMS_UNUSED(x) (void)x;
#endif

typedef void (DCT_CALLCONV *dctXCreate_t) (dctHandle_t *pdct);
static DCT_FUNCPTR(dctXCreate);

typedef void (DCT_CALLCONV *dctXCreateD_t) (dctHandle_t *pdct, const char *dirName);
static DCT_FUNCPTR(dctXCreateD);
typedef void (DCT_CALLCONV *dctXFree_t)   (dctHandle_t *pdct);
static DCT_FUNCPTR(dctXFree);
typedef int (DCT_CALLCONV *dctXAPIVersion_t) (int api, char *msg, int *cl);
static DCT_FUNCPTR(dctXAPIVersion);
typedef int (DCT_CALLCONV *dctXCheck_t) (const char *ep, int nargs, int s[], char *msg);
static DCT_FUNCPTR(dctXCheck);
#define printNoReturn(f,nargs) { \
  char d_msgBuf[256]; \
  strcpy(d_msgBuf,#f " could not be loaded: "); \
  dctXCheck(#f,nargs,d_s,d_msgBuf+strlen(d_msgBuf)); \
  dctErrorHandling(d_msgBuf); \
}
#define printAndReturn(f,nargs,rtype) { \
  char d_msgBuf[256]; \
  strcpy(d_msgBuf,#f " could not be loaded: "); \
  dctXCheck(#f,nargs,d_s,d_msgBuf+strlen(d_msgBuf)); \
  dctErrorHandling(d_msgBuf); \
  return (rtype) 0; \
}


/** Read data from dictionary file specified by fName
 * @param pdct dct object handle
 * @param fName Name of file
 * @param Msg Message
 */
int  DCT_CALLCONV d_dctLoadEx (dctHandle_t pdct, const char *fName, char *Msg, int Msg_i)
{
  int d_s[]={3,11,17};
  GAMS_UNUSED(pdct)
  GAMS_UNUSED(fName)
  GAMS_UNUSED(Msg)
  GAMS_UNUSED(Msg_i)
  printAndReturn(dctLoadEx,2,int )
}

/** Read data from given open GDX object
 * @param pdct dct object handle
 * @param gdxptr GDX handle
 * @param Msg Message
 */
int  DCT_CALLCONV d_dctLoadWithHandle (dctHandle_t pdct, void *gdxptr, char *Msg, int Msg_i)
{
  int d_s[]={3,1,17};
  GAMS_UNUSED(pdct)
  GAMS_UNUSED(gdxptr)
  GAMS_UNUSED(Msg)
  GAMS_UNUSED(Msg_i)
  printAndReturn(dctLoadWithHandle,2,int )
}

/** Number of UELs in dictionary
 * @param pdct dct object handle
 */
int  DCT_CALLCONV d_dctNUels (dctHandle_t pdct)
{
  int d_s[]={3};
  GAMS_UNUSED(pdct)
  printAndReturn(dctNUels,0,int )
}

/** Return UEL index of uelLabel in [1..numUels], 0 on failure
 * @param pdct dct object handle
 * @param uelLabel Label of unique element
 */
int  DCT_CALLCONV d_dctUelIndex (dctHandle_t pdct, const char *uelLabel)
{
  int d_s[]={3,11};
  GAMS_UNUSED(pdct)
  GAMS_UNUSED(uelLabel)
  printAndReturn(dctUelIndex,1,int )
}

/** Get UEL label associated with index: return 0 on success, 1 otherwise
 * @param pdct dct object handle
 * @param uelIndex Index of unique element
 * @param q Quote character
 * @param uelLabel Label of unique element
 */
int  DCT_CALLCONV d_dctUelLabel (dctHandle_t pdct, int uelIndex, char *q, char *uelLabel, int uelLabel_i)
{
  int d_s[]={3,3,19,17};
  GAMS_UNUSED(pdct)
  GAMS_UNUSED(uelIndex)
  GAMS_UNUSED(q)
  GAMS_UNUSED(uelLabel)
  GAMS_UNUSED(uelLabel_i)
  printAndReturn(dctUelLabel,3,int )
}

/** Return number of symbols (variables and equations only)
 * @param pdct dct object handle
 */
int  DCT_CALLCONV d_dctNLSyms (dctHandle_t pdct)
{
  int d_s[]={3};
  GAMS_UNUSED(pdct)
  printAndReturn(dctNLSyms,0,int )
}

/** Return dimension of specified symbol, -1 on failure
 * @param pdct dct object handle
 * @param symIndex Index of symbol
 */
int  DCT_CALLCONV d_dctSymDim (dctHandle_t pdct, int symIndex)
{
  int d_s[]={3,3};
  GAMS_UNUSED(pdct)
  GAMS_UNUSED(symIndex)
  printAndReturn(dctSymDim,1,int )
}

/** Return index of symbol name in [1..numSyms], <=0 on failure
 * @param pdct dct object handle
 * @param symName Name of symbol
 */
int  DCT_CALLCONV d_dctSymIndex (dctHandle_t pdct, const char *symName)
{
  int d_s[]={3,11};
  GAMS_UNUSED(pdct)
  GAMS_UNUSED(symName)
  printAndReturn(dctSymIndex,1,int )
}

/** Get symbol name associated with index: return 0 on success, 1 otherwise
 * @param pdct dct object handle
 * @param symIndex Index of symbol
 * @param symName Name of symbol
 */
int  DCT_CALLCONV d_dctSymName (dctHandle_t pdct, int symIndex, char *symName, int symName_i)
{
  int d_s[]={3,3,17};
  GAMS_UNUSED(pdct)
  GAMS_UNUSED(symIndex)
  GAMS_UNUSED(symName)
  GAMS_UNUSED(symName_i)
  printAndReturn(dctSymName,2,int )
}

/** Get symbol text and quote character associated with index: return 0 on success, 1 otherwise
 * @param pdct dct object handle
 * @param symIndex Index of symbol
 * @param q Quote character
 * @param symTxt Explanator text of symbol
 */
int  DCT_CALLCONV d_dctSymText (dctHandle_t pdct, int symIndex, char *q, char *symTxt, int symTxt_i)
{
  int d_s[]={3,3,19,17};
  GAMS_UNUSED(pdct)
  GAMS_UNUSED(symIndex)
  GAMS_UNUSED(q)
  GAMS_UNUSED(symTxt)
  GAMS_UNUSED(symTxt_i)
  printAndReturn(dctSymText,3,int )
}

/** Return the symbol type associated with index, or -1 on failure
 * @param pdct dct object handle
 * @param symIndex Index of symbol
 */
int  DCT_CALLCONV d_dctSymType (dctHandle_t pdct, int symIndex)
{
  int d_s[]={3,3};
  GAMS_UNUSED(pdct)
  GAMS_UNUSED(symIndex)
  printAndReturn(dctSymType,1,int )
}

/** Return the symbol userinfo (used for the variable or equation subtype), or -1 on failure
 * @param pdct dct object handle
 * @param symIndex Index of symbol
 */
int  DCT_CALLCONV d_dctSymUserInfo (dctHandle_t pdct, int symIndex)
{
  int d_s[]={3,3};
  GAMS_UNUSED(pdct)
  GAMS_UNUSED(symIndex)
  printAndReturn(dctSymUserInfo,1,int )
}

/** Return the number of records stored for the specified symbol, or -1 on failure
 * @param pdct dct object handle
 * @param symIndex Index of symbol
 */
int  DCT_CALLCONV d_dctSymEntries (dctHandle_t pdct, int symIndex)
{
  int d_s[]={3,3};
  GAMS_UNUSED(pdct)
  GAMS_UNUSED(symIndex)
  printAndReturn(dctSymEntries,1,int )
}

/** First row or column number for a symbol (0-based), -1 on failure
 * @param pdct dct object handle
 * @param symIndex Index of symbol
 */
int  DCT_CALLCONV d_dctSymOffset (dctHandle_t pdct, int symIndex)
{
  int d_s[]={3,3};
  GAMS_UNUSED(pdct)
  GAMS_UNUSED(symIndex)
  printAndReturn(dctSymOffset,1,int )
}

/** Get the relaxed domain names - as strings - for the specified symbol: return 0 on success, 1 otherwise
 * @param pdct dct object handle
 * @param symIndex Index of symbol
 * @param symDoms 
 * @param symDim Dimension of symbol
 */
int  DCT_CALLCONV d_dctSymDomNames (dctHandle_t pdct, int symIndex, char *symDoms[], int *symDim)
{
  int d_s[]={3,3,56,4};
  GAMS_UNUSED(pdct)
  GAMS_UNUSED(symIndex)
  GAMS_UNUSED(symDoms)
  GAMS_UNUSED(symDim)
  printAndReturn(dctSymDomNames,3,int )
}

/** Get the relaxed domain names - as 1-based indices - for the specified symbol: return 0 on success, 1 otherwise
 * @param pdct dct object handle
 * @param symIndex Index of symbol
 * @param symDomIdx 
 * @param symDim Dimension of symbol
 */
int  DCT_CALLCONV d_dctSymDomIdx (dctHandle_t pdct, int symIndex, int symDomIdx[], int *symDim)
{
  int d_s[]={3,3,52,4};
  GAMS_UNUSED(pdct)
  GAMS_UNUSED(symIndex)
  GAMS_UNUSED(symDomIdx)
  GAMS_UNUSED(symDim)
  printAndReturn(dctSymDomIdx,3,int )
}

/** Return the count of domain names used in the dictionary
 * @param pdct dct object handle
 */
int  DCT_CALLCONV d_dctDomNameCount (dctHandle_t pdct)
{
  int d_s[]={3};
  GAMS_UNUSED(pdct)
  printAndReturn(dctDomNameCount,0,int )
}

/** Get the domain name string that goes with the specified index: return 0 on success, nonzero otherwise
 * @param pdct dct object handle
 * @param domIndex 
 * @param domName 
 */
int  DCT_CALLCONV d_dctDomName (dctHandle_t pdct, int domIndex, char *domName, int domName_i)
{
  int d_s[]={3,3,17};
  GAMS_UNUSED(pdct)
  GAMS_UNUSED(domIndex)
  GAMS_UNUSED(domName)
  GAMS_UNUSED(domName_i)
  printAndReturn(dctDomName,2,int )
}

/** Return the column index (0-based) corresponding to the specified symbol index and UEL indices, or -1 on failure
 * @param pdct dct object handle
 * @param symIndex Index of symbol
 * @param uelIndices Indices of unique element
 */
int  DCT_CALLCONV d_dctColIndex (dctHandle_t pdct, int symIndex, const int uelIndices[])
{
  int d_s[]={3,3,51};
  GAMS_UNUSED(pdct)
  GAMS_UNUSED(symIndex)
  GAMS_UNUSED(uelIndices)
  printAndReturn(dctColIndex,2,int )
}

/** Return the row index (0-based) corresponding to the specified symbol index and UEL indices, or -1 on failure
 * @param pdct dct object handle
 * @param symIndex Index of symbol
 * @param uelIndices Indices of unique element
 */
int  DCT_CALLCONV d_dctRowIndex (dctHandle_t pdct, int symIndex, const int uelIndices[])
{
  int d_s[]={3,3,51};
  GAMS_UNUSED(pdct)
  GAMS_UNUSED(symIndex)
  GAMS_UNUSED(uelIndices)
  printAndReturn(dctRowIndex,2,int )
}

/** Get the symbol index and dimension and the UEL indices for column j (0-based): return 0 on success, 1 otherwise
 * @param pdct dct object handle
 * @param j Col index
 * @param symIndex Index of symbol
 * @param uelIndices Indices of unique element
 * @param symDim Dimension of symbol
 */
int  DCT_CALLCONV d_dctColUels (dctHandle_t pdct, int j, int *symIndex, int uelIndices[], int *symDim)
{
  int d_s[]={3,3,4,52,4};
  GAMS_UNUSED(pdct)
  GAMS_UNUSED(j)
  GAMS_UNUSED(symIndex)
  GAMS_UNUSED(uelIndices)
  GAMS_UNUSED(symDim)
  printAndReturn(dctColUels,4,int )
}

/** Get the symbol index and dimension and the UEL indices for row i,(0-based): return 0 on success, 1 otherwise
 * @param pdct dct object handle
 * @param i Row index
 * @param symIndex Index of symbol
 * @param uelIndices Indices of unique element
 * @param symDim Dimension of symbol
 */
int  DCT_CALLCONV d_dctRowUels (dctHandle_t pdct, int i, int *symIndex, int uelIndices[], int *symDim)
{
  int d_s[]={3,3,4,52,4};
  GAMS_UNUSED(pdct)
  GAMS_UNUSED(i)
  GAMS_UNUSED(symIndex)
  GAMS_UNUSED(uelIndices)
  GAMS_UNUSED(symDim)
  printAndReturn(dctRowUels,4,int )
}

/** Find first row/column for the specified symbol matching the uelIndices (uelIndices[k] = 0 is wildcard): return handle on success, nil otherwise. Since the routine can fail you should first check rcIndex and then the returned handle.
 * @param pdct dct object handle
 * @param symIndex Index of symbol
 * @param uelIndices Indices of unique element
 * @param rcIndex Row or column index
 */
void * DCT_CALLCONV d_dctFindFirstRowCol (dctHandle_t pdct, int symIndex, const int uelIndices[], int *rcIndex)
{
  int d_s[]={1,3,51,4};
  GAMS_UNUSED(pdct)
  GAMS_UNUSED(symIndex)
  GAMS_UNUSED(uelIndices)
  GAMS_UNUSED(rcIndex)
  printAndReturn(dctFindFirstRowCol,3,void *)
}

/** Find next row/column in the search defined by findHandle (see dctFindFirstRowCol): return 1 on success, 0 on failure
 * @param pdct dct object handle
 * @param findHandle Handle obtained when starting to search. Can be used to further search and terminate search
 * @param rcIndex Row or column index
 */
int  DCT_CALLCONV d_dctFindNextRowCol (dctHandle_t pdct, void *findHandle, int *rcIndex)
{
  int d_s[]={3,1,4};
  GAMS_UNUSED(pdct)
  GAMS_UNUSED(findHandle)
  GAMS_UNUSED(rcIndex)
  printAndReturn(dctFindNextRowCol,2,int )
}

/** Close the findHandle obtained from dctFindFirstRowCol
 * @param pdct dct object handle
 * @param findHandle Handle obtained when starting to search. Can be used to further search and terminate search
 */
void  DCT_CALLCONV d_dctFindClose (dctHandle_t pdct, void *findHandle)
{
  int d_s[]={0,1};
  GAMS_UNUSED(pdct)
  GAMS_UNUSED(findHandle)
  printNoReturn(dctFindClose,1)
}

/** Memory used by the object in MB (1,000,000 bytes)
 * @param pdct dct object handle
 */
double  DCT_CALLCONV d_dctMemUsed (dctHandle_t pdct)
{
  int d_s[]={13};
  GAMS_UNUSED(pdct)
  printAndReturn(dctMemUsed,0,double )
}

/** Initialization
 * @param pdct dct object handle
 * @param NRows Number of rows
 * @param NCols Number of columns
 * @param NBlocks Number of UELs in all equations and variables (dim*nrrec per symbol)
 */
void  DCT_CALLCONV d_dctSetBasicCounts (dctHandle_t pdct, int NRows, int NCols, int NBlocks)
{
  int d_s[]={0,3,3,3};
  GAMS_UNUSED(pdct)
  GAMS_UNUSED(NRows)
  GAMS_UNUSED(NCols)
  GAMS_UNUSED(NBlocks)
  printNoReturn(dctSetBasicCounts,3)
}

/** Initialization
 * @param pdct dct object handle
 * @param NRows Number of rows
 * @param NCols Number of columns
 * @param NBlocks Number of UELs in all equations and variables (dim*nrrec per symbol)
 * @param Msg Message
 */
int  DCT_CALLCONV d_dctSetBasicCountsEx (dctHandle_t pdct, int NRows, int NCols, INT64 NBlocks, char *Msg, int Msg_i)
{
  int d_s[]={15,3,3,23,17};
  GAMS_UNUSED(pdct)
  GAMS_UNUSED(NRows)
  GAMS_UNUSED(NCols)
  GAMS_UNUSED(NBlocks)
  GAMS_UNUSED(Msg)
  GAMS_UNUSED(Msg_i)
  printAndReturn(dctSetBasicCountsEx,4,int )
}

/** Add UEL
 * @param pdct dct object handle
 * @param uelLabel Label of unique element
 * @param q Quote character
 */
void  DCT_CALLCONV d_dctAddUel (dctHandle_t pdct, const char *uelLabel, const char q)
{
  int d_s[]={0,11,18};
  GAMS_UNUSED(pdct)
  GAMS_UNUSED(uelLabel)
  GAMS_UNUSED(q)
  printNoReturn(dctAddUel,2)
}

/** Add symbol
 * @param pdct dct object handle
 * @param symName Name of symbol
 * @param symTyp type of symbol (see enumerated constants)
 * @param symDim Dimension of symbol
 * @param userInfo 
 * @param symTxt Explanator text of symbol
 */
void  DCT_CALLCONV d_dctAddSymbol (dctHandle_t pdct, const char *symName, int symTyp, int symDim, int userInfo, const char *symTxt)
{
  int d_s[]={0,11,3,3,3,11};
  GAMS_UNUSED(pdct)
  GAMS_UNUSED(symName)
  GAMS_UNUSED(symTyp)
  GAMS_UNUSED(symDim)
  GAMS_UNUSED(userInfo)
  GAMS_UNUSED(symTxt)
  printNoReturn(dctAddSymbol,5)
}

/** Add symbol data
 * @param pdct dct object handle
 * @param uelIndices Indices of unique element
 */
void  DCT_CALLCONV d_dctAddSymbolData (dctHandle_t pdct, const int uelIndices[])
{
  int d_s[]={0,51};
  GAMS_UNUSED(pdct)
  GAMS_UNUSED(uelIndices)
  printNoReturn(dctAddSymbolData,1)
}

/** Add symbol domains
 * @param pdct dct object handle
 * @param symName Name of symbol
 * @param symDoms 
 * @param symDim Dimension of symbol
 * @param Msg Message
 */
int  DCT_CALLCONV d_dctAddSymbolDoms (dctHandle_t pdct, const char *symName, const char *symDoms[], int symDim, char *Msg, int Msg_i)
{
  int d_s[]={15,11,55,3,17};
  GAMS_UNUSED(pdct)
  GAMS_UNUSED(symName)
  GAMS_UNUSED(symDoms)
  GAMS_UNUSED(symDim)
  GAMS_UNUSED(Msg)
  GAMS_UNUSED(Msg_i)
  printAndReturn(dctAddSymbolDoms,4,int )
}

/** Write dictionary file in GDX format
 * @param pdct dct object handle
 * @param fName Name of file
 * @param Msg Message
 */
void  DCT_CALLCONV d_dctWriteGDX (dctHandle_t pdct, const char *fName, char *Msg)
{
  int d_s[]={0,11,12};
  GAMS_UNUSED(pdct)
  GAMS_UNUSED(fName)
  GAMS_UNUSED(Msg)
  printNoReturn(dctWriteGDX,2)
}

/** Write dictionary to an open GDX object
 * @param pdct dct object handle
 * @param gdxptr GDX handle
 * @param Msg Message
 */
void  DCT_CALLCONV d_dctWriteGDXWithHandle (dctHandle_t pdct, void *gdxptr, char *Msg)
{
  int d_s[]={0,1,12};
  GAMS_UNUSED(pdct)
  GAMS_UNUSED(gdxptr)
  GAMS_UNUSED(Msg)
  printNoReturn(dctWriteGDXWithHandle,2)
}

/** Number of rows
 * @param pdct dct object handle
 */
int  DCT_CALLCONV d_dctNRows (dctHandle_t pdct)
{
  int d_s[]={3};
  GAMS_UNUSED(pdct)
  printAndReturn(dctNRows,0,int )
}

/** Number of columns
 * @param pdct dct object handle
 */
int  DCT_CALLCONV d_dctNCols (dctHandle_t pdct)
{
  int d_s[]={3};
  GAMS_UNUSED(pdct)
  printAndReturn(dctNCols,0,int )
}

/** Largest dimension of any symbol
 * @param pdct dct object handle
 */
int  DCT_CALLCONV d_dctLrgDim (dctHandle_t pdct)
{
  int d_s[]={3};
  GAMS_UNUSED(pdct)
  printAndReturn(dctLrgDim,0,int )
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
# define GMS_DLL_BASENAME "dctmdclib"
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

  LOADIT(dctXCreate, "dctXCreate");
  LOADIT(dctXCreateD, "CdctXCreateD");

  LOADIT(dctXFree, "dctXFree");
  LOADIT(dctXCheck, "CdctXCheck");
  LOADIT(dctXAPIVersion, "CdctXAPIVersion");

  if (!dctXAPIVersion(2,errBuf,&cl))
    return 1;

#define CheckAndLoad(f,nargs,prefix) \
  if (!dctXCheck(#f,nargs,s,errBuf)) \
    f = &d_##f; \
  else { \
    LOADIT(f,prefix #f); \
  }
  {int s[]={3,11,17}; CheckAndLoad(dctLoadEx,2,"C"); }
  {int s[]={3,1,17}; CheckAndLoad(dctLoadWithHandle,2,"C"); }
  {int s[]={3}; CheckAndLoad(dctNUels,0,""); }
  {int s[]={3,11}; CheckAndLoad(dctUelIndex,1,"C"); }
  {int s[]={3,3,19,17}; CheckAndLoad(dctUelLabel,3,"C"); }
  {int s[]={3}; CheckAndLoad(dctNLSyms,0,""); }
  {int s[]={3,3}; CheckAndLoad(dctSymDim,1,""); }
  {int s[]={3,11}; CheckAndLoad(dctSymIndex,1,"C"); }
  {int s[]={3,3,17}; CheckAndLoad(dctSymName,2,"C"); }
  {int s[]={3,3,19,17}; CheckAndLoad(dctSymText,3,"C"); }
  {int s[]={3,3}; CheckAndLoad(dctSymType,1,""); }
  {int s[]={3,3}; CheckAndLoad(dctSymUserInfo,1,""); }
  {int s[]={3,3}; CheckAndLoad(dctSymEntries,1,""); }
  {int s[]={3,3}; CheckAndLoad(dctSymOffset,1,""); }
  {int s[]={3,3,56,4}; CheckAndLoad(dctSymDomNames,3,"C"); }
  {int s[]={3,3,52,4}; CheckAndLoad(dctSymDomIdx,3,""); }
  {int s[]={3}; CheckAndLoad(dctDomNameCount,0,""); }
  {int s[]={3,3,17}; CheckAndLoad(dctDomName,2,"C"); }
  {int s[]={3,3,51}; CheckAndLoad(dctColIndex,2,""); }
  {int s[]={3,3,51}; CheckAndLoad(dctRowIndex,2,""); }
  {int s[]={3,3,4,52,4}; CheckAndLoad(dctColUels,4,""); }
  {int s[]={3,3,4,52,4}; CheckAndLoad(dctRowUels,4,""); }
  {int s[]={1,3,51,4}; CheckAndLoad(dctFindFirstRowCol,3,""); }
  {int s[]={3,1,4}; CheckAndLoad(dctFindNextRowCol,2,""); }
  {int s[]={0,1}; CheckAndLoad(dctFindClose,1,""); }
  {int s[]={13}; CheckAndLoad(dctMemUsed,0,""); }
  {int s[]={0,3,3,3}; CheckAndLoad(dctSetBasicCounts,3,""); }
  {int s[]={15,3,3,23,17}; CheckAndLoad(dctSetBasicCountsEx,4,"C"); }
  {int s[]={0,11,18}; CheckAndLoad(dctAddUel,2,"C"); }
  {int s[]={0,11,3,3,3,11}; CheckAndLoad(dctAddSymbol,5,"C"); }
  {int s[]={0,51}; CheckAndLoad(dctAddSymbolData,1,""); }
  {int s[]={15,11,55,3,17}; CheckAndLoad(dctAddSymbolDoms,4,"C"); }
  {int s[]={0,11,12}; CheckAndLoad(dctWriteGDX,2,"C"); }
  {int s[]={0,1,12}; CheckAndLoad(dctWriteGDXWithHandle,2,"C"); }
  {int s[]={3}; CheckAndLoad(dctNRows,0,""); }
  {int s[]={3}; CheckAndLoad(dctNCols,0,""); }
  {int s[]={3}; CheckAndLoad(dctLrgDim,0,""); }

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

int dctGetReady (char *msgBuf, int msgBufSize)
{
  int rc;
  lock(libMutex);
  rc = libloader(NULL, NULL, msgBuf, msgBufSize);
  unlock(libMutex);
  return rc;
} /* dctGetReady */

int dctGetReadyD (const char *dirName, char *msgBuf, int msgBufSize)
{
  int rc;
  lock(libMutex);
  rc = libloader(dirName, NULL, msgBuf, msgBufSize);
  unlock(libMutex);
  return rc;
} /* dctGetReadyD */

int dctGetReadyL (const char *libName, char *msgBuf, int msgBufSize)
{
  char dirName[1024],fName[1024];
  int rc;
  extractFileDirFileName (libName, dirName, fName);
  lock(libMutex);
  rc = libloader(dirName, fName, msgBuf, msgBufSize);
  unlock(libMutex);
  return rc;
} /* dctGetReadyL */

int dctCreate (dctHandle_t *pdct, char *msgBuf, int msgBufSize)
{
  int dctIsReady;

  dctIsReady = dctGetReady (msgBuf, msgBufSize);
  if (! dctIsReady) {
    return 0;
  }
  assert(dctXCreate);
  dctXCreate(pdct);
  if(pdct == NULL)
  { strcpy(msgBuf,"Error while creating object"); return 0; }
  lock(objMutex);
  objectCount++;
  unlock(objMutex);
  return 1;                     /* return true on successful library load */
} /* dctCreate */

int dctCreateD (dctHandle_t *pdct, const char *dirName,
                char *msgBuf, int msgBufSize)
{
  int dctIsReady;

  dctIsReady = dctGetReadyD (dirName, msgBuf, msgBufSize);
  if (! dctIsReady) {
    return 0;
  }
  assert(dctXCreate);
  dctXCreate(pdct);
  if(pdct == NULL)
  { strcpy(msgBuf,"Error while creating object"); return 0; }
  lock(objMutex);
  objectCount++;
  unlock(objMutex);
  return 1;                     /* return true on successful library load */
} /* dctCreateD */

int dctCreateDD (dctHandle_t *pdct, const char *dirName,
                char *msgBuf, int msgBufSize)
{
  int dctIsReady;
  dctIsReady = dctGetReadyD (dirName, msgBuf, msgBufSize);
  if (! dctIsReady) {
    return 0;
  }
  assert(dctXCreateD);
  dctXCreateD(pdct, dirName);
  if(pdct == NULL)
  { strcpy(msgBuf,"Error while creating object"); return 0; }
  lock(objMutex);
  objectCount++;
  unlock(objMutex);
  return 1;                     /* return true on successful library load */
} /* dctCreateD */


int dctCreateL (dctHandle_t *pdct, const char *libName,
                char *msgBuf, int msgBufSize)
{
  int dctIsReady;

  dctIsReady = dctGetReadyL (libName, msgBuf, msgBufSize);
  if (! dctIsReady) {
    return 0;
  }
  assert(dctXCreate);
  dctXCreate(pdct);
  if(pdct == NULL)
  { strcpy(msgBuf,"Error while creating object"); return 0; }
  lock(objMutex);
  objectCount++;
  unlock(objMutex);
  return 1;                     /* return true on successful library load */
} /* dctCreateL */

int dctFree   (dctHandle_t *pdct)
{
  assert(dctXFree);
  dctXFree(pdct); pdct = NULL;
  lock(objMutex);
  objectCount--;
  unlock(objMutex);
  return 1;
} /* dctFree */

int dctLibraryLoaded(void)
{
  int rc;
  lock(libMutex);
  rc = isLoaded;
  unlock(libMutex);
  return rc;
} /* dctLibraryLoaded */

int dctLibraryUnload(void)
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
} /* dctLibraryUnload */

int  dctCorrectLibraryVersion(char *msgBuf, int msgBufLen)
{
  int cl;
  char localBuf[256];

  if (msgBuf && msgBufLen) msgBuf[0] = '\0';

  if (! isLoaded) {
    strncpy(msgBuf, "Library needs to be initialized first", msgBufLen);
    return 0;
  }

  if (NULL == dctXAPIVersion) {
    strncpy(msgBuf, "Function dctXAPIVersion not found", msgBufLen);
    return 0;
  }

  dctXAPIVersion(DCTAPIVERSION,localBuf,&cl);
  strncpy(msgBuf, localBuf, msgBufLen);

  if (1 == cl)
    return 1;
  else
    return 0;
}

int dctGetScreenIndicator(void)
{
  return ScreenIndicator;
}

void dctSetScreenIndicator(int scrind)
{
  ScreenIndicator = scrind ? 1 : 0;
}

int dctGetExceptionIndicator(void)
{
   return ExceptionIndicator;
}

void dctSetExceptionIndicator(int excind)
{
  ExceptionIndicator = excind ? 1 : 0;
}

int dctGetExitIndicator(void)
{
  return ExitIndicator;
}

void dctSetExitIndicator(int extind)
{
  ExitIndicator = extind ? 1 : 0;
}

dctErrorCallback_t dctGetErrorCallback(void)
{
  return ErrorCallBack;
}

void dctSetErrorCallback(dctErrorCallback_t func)
{
  lock(exceptMutex);
  ErrorCallBack = func;
  unlock(exceptMutex);
}

int dctGetAPIErrorCount(void)
{
  return APIErrorCount;
}

void dctSetAPIErrorCount(int ecnt)
{
  APIErrorCount = ecnt;
}

void dctErrorHandling(const char *msg)
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

