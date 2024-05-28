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

#define GDX_MAIN
#include "gdxcc.h"

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
static gdxErrorCallback_t ErrorCallBack = NULL;
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

void gdxInitMutexes(void)
{
  int rc;
  if (0==MutexIsInitialized) {
    rc = GC_mutex_init (&libMutex);     if(0!=rc) gdxErrorHandling("Problem initializing libMutex");
    rc = GC_mutex_init (&objMutex);     if(0!=rc) gdxErrorHandling("Problem initializing objMutex");
    rc = GC_mutex_init (&exceptMutex);  if(0!=rc) gdxErrorHandling("Problem initializing exceptMutex");
    MutexIsInitialized = 1;
  }
}

void gdxFiniMutexes(void)
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
void gdxInitMutexes(void) {}
void gdxFiniMutexes(void) {}
#endif

#if !defined(GAMS_UNUSED)
#define GAMS_UNUSED(x) (void)x;
#endif

typedef void (GDX_CALLCONV *XCreate_t) (gdxHandle_t *pgdx);
static GDX_FUNCPTR(XCreate);

typedef void (GDX_CALLCONV *XCreateD_t) (gdxHandle_t *pgdx, const char *dirName);
static GDX_FUNCPTR(XCreateD);
typedef void (GDX_CALLCONV *XFree_t)   (gdxHandle_t *pgdx);
static GDX_FUNCPTR(XFree);
typedef int (GDX_CALLCONV *XAPIVersion_t) (int api, char *msg, int *cl);
static GDX_FUNCPTR(XAPIVersion);
typedef int (GDX_CALLCONV *XCheck_t) (const char *ep, int nargs, int s[], char *msg);
static GDX_FUNCPTR(XCheck);
typedef void (GDX_CALLCONV *gdxSetLoadPath_t) (const char *s);
GDX_FUNCPTR(gdxSetLoadPath);
typedef void (GDX_CALLCONV *gdxGetLoadPath_t) (char *s);
GDX_FUNCPTR(gdxGetLoadPath);

#define printNoReturn(f,nargs) { \
  char d_msgBuf[256]; \
  strcpy(d_msgBuf,#f " could not be loaded: "); \
  XCheck(#f,nargs,d_s,d_msgBuf+strlen(d_msgBuf)); \
  gdxErrorHandling(d_msgBuf); \
}
#define printAndReturn(f,nargs,rtype) { \
  char d_msgBuf[256]; \
  strcpy(d_msgBuf,#f " could not be loaded: "); \
  XCheck(#f,nargs,d_s,d_msgBuf+strlen(d_msgBuf)); \
  gdxErrorHandling(d_msgBuf); \
  return (rtype) 0; \
}


/** Add a new acronym entry. This can be used to add entries before data is written. Returns negative value (<0) if the entry is not added.
 * @param pgdx gdx object handle
 * @param AName Name of the acronym (up to 63 characters) The first character of a symbol must be a letter. Following symbol characters may be letters, digits, and underscores. Symbol names must be new and unique.
 * @param Txt Explanatory text of the acronym (up to 255 characters, mixed quotes will be unified to first occurring quote character).
 * @param AIndx Index value of the acronym.
 */
int  GDX_CALLCONV d_gdxAcronymAdd (gdxHandle_t pgdx, const char *AName, const char *Txt, int AIndx)
{
  int d_s[]={3,11,11,3};
  GAMS_UNUSED(pgdx)
  GAMS_UNUSED(AName)
  GAMS_UNUSED(Txt)
  GAMS_UNUSED(AIndx)
  printAndReturn(gdxAcronymAdd,3,int )
}

/** Number of entries in the acronym table.
 * @param pgdx gdx object handle
 */
int  GDX_CALLCONV d_gdxAcronymCount (gdxHandle_t pgdx)
{
  int d_s[]={3};
  GAMS_UNUSED(pgdx)
  printAndReturn(gdxAcronymCount,0,int )
}

/** Retrieve acronym information from the acronym table. Non-zero if the index into the acronym table is valid.
 * @param pgdx gdx object handle
 * @param N Index into acronym table (range 1..AcronymCount).
 * @param AName Name of the acronym (up to 63 characters).
 * @param Txt Explanatory text of the acronym (up to 255 characters, mixed quote chars will be unified to first occurring quote).
 * @param AIndx Index value of the acronym.
 */
int  GDX_CALLCONV d_gdxAcronymGetInfo (gdxHandle_t pgdx, int N, char *AName, char *Txt, int *AIndx)
{
  int d_s[]={3,3,12,12,4};
  GAMS_UNUSED(pgdx)
  GAMS_UNUSED(N)
  GAMS_UNUSED(AName)
  GAMS_UNUSED(Txt)
  GAMS_UNUSED(AIndx)
  printAndReturn(gdxAcronymGetInfo,4,int )
}

/** Get information how acronym values are remapped. When reading GDX data, we need to map indices for acronyms used in the GDX file to indices used by the reading program. Non-zero if the index into the acronym table is valid.
 * @param pgdx gdx object handle
 * @param N Index into acronym table; range from 1 to AcronymCount.
 * @param orgIndx The Index used in the GDX file.
 * @param newIndx The Index returned when reading GDX data.
 * @param autoIndex Non-zero if the newIndx was generated using the value of NextAutoAcronym.
 */
int  GDX_CALLCONV d_gdxAcronymGetMapping (gdxHandle_t pgdx, int N, int *orgIndx, int *newIndx, int *autoIndex)
{
  int d_s[]={3,3,4,4,4};
  GAMS_UNUSED(pgdx)
  GAMS_UNUSED(N)
  GAMS_UNUSED(orgIndx)
  GAMS_UNUSED(newIndx)
  GAMS_UNUSED(autoIndex)
  printAndReturn(gdxAcronymGetMapping,4,int )
}

/** Get index value of an acronym. Returns zero if V does not represent an acronym.
 * @param pgdx gdx object handle
 * @param V Input value possibly representing an acronym/Version string after return (gdxGetDLLVersion).
 */
int  GDX_CALLCONV d_gdxAcronymIndex (gdxHandle_t pgdx, double V)
{
  int d_s[]={3,13};
  GAMS_UNUSED(pgdx)
  GAMS_UNUSED(V)
  printAndReturn(gdxAcronymIndex,1,int )
}

/** Find the name of an acronym value. Non-zero if a name for the acronym is defined. An unnamed acronym value will return a string of the form UnknownAcronymNNN, were NNN is the index of the acronym.
 * @param pgdx gdx object handle
 * @param V Input value possibly containing an acronym/Version string after return (gdxGetDLLVersion).
 * @param AName Name of acronym value or the empty string (can be up to 63 characters).
 */
int  GDX_CALLCONV d_gdxAcronymName (gdxHandle_t pgdx, double V, char *AName)
{
  int d_s[]={3,13,12};
  GAMS_UNUSED(pgdx)
  GAMS_UNUSED(V)
  GAMS_UNUSED(AName)
  printAndReturn(gdxAcronymName,2,int )
}

/** Returns the value of the NextAutoAcronym variable and sets the variable to nv. <ul><li>When we read from a GDX file and encounter an acronym that was not defined, we need to assign a new index for that acronym. The variable NextAutoAcronym is used for this purpose and is incremented for each new undefined acronym.</li> <li>When NextAutoAcronym has a value of zero, the default, the value is ignored and the original index as stored in the GDX file is used for the index.</li></ul>
 * @param pgdx gdx object handle
 * @param NV New value for NextAutoAcronym; a value of less than zero is ignored.
 */
int  GDX_CALLCONV d_gdxAcronymNextNr (gdxHandle_t pgdx, int NV)
{
  int d_s[]={3,3};
  GAMS_UNUSED(pgdx)
  GAMS_UNUSED(NV)
  printAndReturn(gdxAcronymNextNr,1,int )
}

/** Modify acronym information in the acronym table <ul><li>When writing a GDX file, this function is used to provide the name of an acronym; in this case the Indx parameter must match.</li> <li>When reading a GDX file, this function is used to provide the acronym index, and the AName parameter must match.</li></ul>
 * @param pgdx gdx object handle
 * @param N Index into acronym table (range 1..AcronymCount).
 * @param AName Name of the acronym (up to 63 characters). The first character of a symbol must be a letter. Following symbol characters may be letters, digits, and underscores. Symbol names must be new and unique.
 * @param Txt Explanatory text of the acronym (up to 255 characters, mixed quote chars will be unified to first occurring quote).
 * @param AIndx Index value of the acronym.
 */
int  GDX_CALLCONV d_gdxAcronymSetInfo (gdxHandle_t pgdx, int N, const char *AName, const char *Txt, int AIndx)
{
  int d_s[]={3,3,11,11,3};
  GAMS_UNUSED(pgdx)
  GAMS_UNUSED(N)
  GAMS_UNUSED(AName)
  GAMS_UNUSED(Txt)
  GAMS_UNUSED(AIndx)
  printAndReturn(gdxAcronymSetInfo,4,int )
}

/** Create an acronym value based on the index (AIndx should be greater than 0). Returns the calculated acronym value (zero if AIndx is <0).
 * @param pgdx gdx object handle
 * @param AIndx Index value; should be greater than zero.
 */
double  GDX_CALLCONV d_gdxAcronymValue (gdxHandle_t pgdx, int AIndx)
{
  int d_s[]={13,3};
  GAMS_UNUSED(pgdx)
  GAMS_UNUSED(AIndx)
  printAndReturn(gdxAcronymValue,1,double )
}

/** Add an alias for a set to the symbol table. One of the two identifiers has to be a known set, an alias or "*" (universe); the other identifier is used as the new alias for the given set. The function gdxSymbolInfoX can be used to retrieve the set or alias associated with the identifier; it is returned as the UserInfo parameter.
 * @param pgdx gdx object handle
 * @param Id1 First set identifier.
 * @param Id2 Second set identifier.
 */
int  GDX_CALLCONV d_gdxAddAlias (gdxHandle_t pgdx, const char *Id1, const char *Id2)
{
  int d_s[]={3,11,11};
  GAMS_UNUSED(pgdx)
  GAMS_UNUSED(Id1)
  GAMS_UNUSED(Id2)
  printAndReturn(gdxAddAlias,2,int )
}

/** Register a string in the string table Register a string in the string table and return the integer number assigned to this string. The integer value can be used to set the associated text of a set element. The string must follow the GAMS syntax rules for explanatory text e.g. not longer than 255 characters.
 * @param pgdx gdx object handle
 * @param Txt The string to be registered (must not exceed 255 characters).
 * @param TxtNr The index number assigned to this string (output argument).
 */
int  GDX_CALLCONV d_gdxAddSetText (gdxHandle_t pgdx, const char *Txt, int *TxtNr)
{
  int d_s[]={3,11,4};
  GAMS_UNUSED(pgdx)
  GAMS_UNUSED(Txt)
  GAMS_UNUSED(TxtNr)
  printAndReturn(gdxAddSetText,2,int )
}

/** Returns the value of the AutoConvert variable and sets the variable to nv. <p>When we close a new GDX file, we look at the value of AutoConvert; if AutoConvert is non-zero, we look at the GDXCOMPRESS and GDXCONVERT environment variables to determine if conversion to an older file format is desired. We needed this logic so gdxcopy.exe can disable automatic file conversion.</p>
 * @param pgdx gdx object handle
 * @param NV New value for AutoConvert.
 */
int  GDX_CALLCONV d_gdxAutoConvert (gdxHandle_t pgdx, int NV)
{
  int d_s[]={3,3};
  GAMS_UNUSED(pgdx)
  GAMS_UNUSED(NV)
  printAndReturn(gdxAutoConvert,1,int )
}

/** Close a GDX file that was previously opened for reading or writing. Before the file is closed, any pending write operations will be finished. This does not free the GDX in-memory object. This method will automatically be called when the GDX object lifetime ends (e.g. being out of scope).
 * @param pgdx gdx object handle
 */
int  GDX_CALLCONV d_gdxClose (gdxHandle_t pgdx)
{
  int d_s[]={3};
  GAMS_UNUSED(pgdx)
  printAndReturn(gdxClose,0,int )
}

/** Query the number of error records.
 * @param pgdx gdx object handle
 */
int  GDX_CALLCONV d_gdxDataErrorCount (gdxHandle_t pgdx)
{
  int d_s[]={3};
  GAMS_UNUSED(pgdx)
  printAndReturn(gdxDataErrorCount,0,int )
}

/** Retrieve an error record. Non-zero if the record number is valid.
 * @param pgdx gdx object handle
 * @param RecNr The number of the record to be retrieved (range = 1..NrErrorRecords); this argument is ignored in gdxDataReadMap
 * @param KeyInt Index for the record (array of UEL numbers for each dimension).
 * @param Values Values for the record (level, marginal, lower-, upper-bound, scale).
 */
int  GDX_CALLCONV d_gdxDataErrorRecord (gdxHandle_t pgdx, int RecNr, int KeyInt[], double Values[])
{
  int d_s[]={3,3,52,54};
  GAMS_UNUSED(pgdx)
  GAMS_UNUSED(RecNr)
  GAMS_UNUSED(KeyInt)
  GAMS_UNUSED(Values)
  printAndReturn(gdxDataErrorRecord,3,int )
}

/** Retrieve an error record. Non-zero if the record number is valid.
 * @param pgdx gdx object handle
 * @param RecNr The number of the record to be retrieved, (range 1..NrErrorRecords); this argument is ignored in gdxDataReadMap
 * @param KeyInt Index for the record, negative uel indicates domain violation for filtered/strict read.
 * @param Values Values for the record (level, marginal, lower-, upper-bound, scale).
 */
int  GDX_CALLCONV d_gdxDataErrorRecordX (gdxHandle_t pgdx, int RecNr, int KeyInt[], double Values[])
{
  int d_s[]={3,3,52,54};
  GAMS_UNUSED(pgdx)
  GAMS_UNUSED(RecNr)
  GAMS_UNUSED(KeyInt)
  GAMS_UNUSED(Values)
  printAndReturn(gdxDataErrorRecordX,3,int )
}

/** Finish reading of a symbol in any mode (raw, mapped, string). . Returns zero if the operation is not possible.
 * @param pgdx gdx object handle
 */
int  GDX_CALLCONV d_gdxDataReadDone (gdxHandle_t pgdx)
{
  int d_s[]={3};
  GAMS_UNUSED(pgdx)
  printAndReturn(gdxDataReadDone,0,int )
}

/** Initialize the reading of a symbol in filtered mode. Returns zero if the operation is not possible.
 * @param pgdx gdx object handle
 * @param SyNr The index number of the symbol, range 0..NrSymbols; SyNr = 0 reads universe.
 * @param FilterAction Array of filter actions for each index position.
 * @param NrRecs The maximum number of records available for reading. The actual number of records may be less when a filter is applied to the records read.
 */
int  GDX_CALLCONV d_gdxDataReadFilteredStart (gdxHandle_t pgdx, int SyNr, const int FilterAction[], int *NrRecs)
{
  int d_s[]={3,3,51,4};
  GAMS_UNUSED(pgdx)
  GAMS_UNUSED(SyNr)
  GAMS_UNUSED(FilterAction)
  GAMS_UNUSED(NrRecs)
  printAndReturn(gdxDataReadFilteredStart,3,int )
}

/** Read the next record in mapped mode. Returns zero if the operation is not possible.
 * @param pgdx gdx object handle
 * @param RecNr Ignored (left in for backward compatibility).
 * @param KeyInt The index of the record.
 * @param Values The data of the record.
 * @param DimFrst The first index position in KeyInt that changed.
 */
int  GDX_CALLCONV d_gdxDataReadMap (gdxHandle_t pgdx, int RecNr, int KeyInt[], double Values[], int *DimFrst)
{
  int d_s[]={3,3,52,54,4};
  GAMS_UNUSED(pgdx)
  GAMS_UNUSED(RecNr)
  GAMS_UNUSED(KeyInt)
  GAMS_UNUSED(Values)
  GAMS_UNUSED(DimFrst)
  printAndReturn(gdxDataReadMap,4,int )
}

/** Initialize the reading of a symbol in mapped mode. Returns zero if the operation is not possible.
 * @param pgdx gdx object handle
 * @param SyNr The index number of the symbol, range 0..NrSymbols; SyNr = 0 reads universe.
 * @param NrRecs The maximum number of records available for reading. The actual number of records may be less when a filter is applied to the records read.
 */
int  GDX_CALLCONV d_gdxDataReadMapStart (gdxHandle_t pgdx, int SyNr, int *NrRecs)
{
  int d_s[]={3,3,4};
  GAMS_UNUSED(pgdx)
  GAMS_UNUSED(SyNr)
  GAMS_UNUSED(NrRecs)
  printAndReturn(gdxDataReadMapStart,2,int )
}

/** Read the next record in raw mode. Returns zero if the operation is not possible.
 * @param pgdx gdx object handle
 * @param KeyInt The index of the record in UEL numbers for each dimension.
 * @param Values The data of the record (level, marginal, lower-, upper-bound, scale).
 * @param DimFrst The first index position in KeyInt that changed.
 */
int  GDX_CALLCONV d_gdxDataReadRaw (gdxHandle_t pgdx, int KeyInt[], double Values[], int *DimFrst)
{
  int d_s[]={3,52,54,4};
  GAMS_UNUSED(pgdx)
  GAMS_UNUSED(KeyInt)
  GAMS_UNUSED(Values)
  GAMS_UNUSED(DimFrst)
  printAndReturn(gdxDataReadRaw,3,int )
}

/** Read a symbol in Raw mode using a callback procedure. Returns zero if the operation is not possible.
 * @param pgdx gdx object handle
 * @param SyNr The index number of the symbol (range 0..NrSymbols); SyNr = 0 reads universe.
 * @param DP Procedure that will be called for each data record. This procedure (return type=void) should have the following signature: <ul><li>UEL index number keys (const int ),</li> <li>values (level, marginal, lower-, upper-bound, scale) (const double )</li></ul>
 * @param NrRecs The maximum number of records available for reading. The actual number of records may be less when a filter is applied to the records read.
 */
int  GDX_CALLCONV d_gdxDataReadRawFast (gdxHandle_t pgdx, int SyNr, TDataStoreProc_t DP, int *NrRecs)
{
  int d_s[]={3,3,59,4};
  GAMS_UNUSED(pgdx)
  GAMS_UNUSED(SyNr)
  GAMS_UNUSED(DP)
  GAMS_UNUSED(NrRecs)
  printAndReturn(gdxDataReadRawFast,3,int )
}

/** Read a symbol in Raw mode using a callback procedure. Returns zero if the operation is not possible.
 * @param pgdx gdx object handle
 * @param SyNr The index number of the symbol (range 0..NrSymbols); SyNr = 0 reads universe.
 * @param DP Procedure that will be called for each data record. This function (return type=integer) should return whether reading continues (=0 for stop, >=1 otherwise) and should have the following signature: <ul><li>UEL index number keys (const int ),</li> <li>values (level, marginal, lower-, upper-bound, scale) (const double ),</li> <li>dimension of first change (int),</li> <li>pointer to custom data (void )</li> </ul>
 * @param NrRecs The number of records available for reading.
 * @param Uptr Pointer to user memory that will be passed back with the callback.
 */
int  GDX_CALLCONV d_gdxDataReadRawFastEx (gdxHandle_t pgdx, int SyNr, TDataStoreExProc_t DP, int *NrRecs, void *Uptr)
{
  int d_s[]={3,3,59,4,1};
  GAMS_UNUSED(pgdx)
  GAMS_UNUSED(SyNr)
  GAMS_UNUSED(DP)
  GAMS_UNUSED(NrRecs)
  GAMS_UNUSED(Uptr)
  printAndReturn(gdxDataReadRawFastEx,4,int )
}

/** Read a symbol in Raw mode while applying a filter using a callback procedure. Returns zero if the operation is not possible.
 * @param pgdx gdx object handle
 * @param SyNr The index number of the symbol, range from 0 to NrSymbols; SyNr = 0 reads universe.
 * @param UelFilterStr Each index can be fixed by setting the string for the unique element. Set an index position to the empty string in order not to fix that position. If the string is not-empty it should match an UEL name from the UEL table.
 * @param DP Callback procedure which will be called for each available data item. This procedure (return type=void) should have the following signature: <ul><li>UEL index number keys (const int ),</li> <li>values (level, marginal, lower-, upper-bound, scale) (const double ),</li> <li>pointer to custom data (void ).</li></ul>
 */
int  GDX_CALLCONV d_gdxDataReadRawFastFilt (gdxHandle_t pgdx, int SyNr, const char *UelFilterStr[], TDataStoreFiltProc_t DP)
{
  int d_s[]={3,3,55,59};
  GAMS_UNUSED(pgdx)
  GAMS_UNUSED(SyNr)
  GAMS_UNUSED(UelFilterStr)
  GAMS_UNUSED(DP)
  printAndReturn(gdxDataReadRawFastFilt,3,int )
}

/** Initialize the reading of a symbol in raw mode. Returns zero if the operation is not possible.
 * @param pgdx gdx object handle
 * @param SyNr The index number of the symbol, range 0..NrSymbols; SyNr = 0 reads universe.
 * @param NrRecs The maximum number of records available for reading. The actual number of records may be less when a filter is applied to the records read.
 */
int  GDX_CALLCONV d_gdxDataReadRawStart (gdxHandle_t pgdx, int SyNr, int *NrRecs)
{
  int d_s[]={3,3,4};
  GAMS_UNUSED(pgdx)
  GAMS_UNUSED(SyNr)
  GAMS_UNUSED(NrRecs)
  printAndReturn(gdxDataReadRawStart,2,int )
}

/** Read a slice of data from a data set, by fixing zero or more index positions in the data. When a data element is available, the callback procedure DP is called with the current index and the values. The indices used in the index vary from zero to the highest value minus one for that index position. This function can be called multiple times. Returns zero if the operation is not possible.
 * @param pgdx gdx object handle
 * @param UelFilterStr Each index can be fixed by setting the string for the unique element. Set an index position to the empty string in order not to fix that position.
 * @param Dimen The dimension of the index space; this is the number of index positions that is not fixed.
 * @param DP Callback procedure which will be called for each available data item. Signature is <ul><li>UEL index number keys for each symbol dimension (const int )</li> <li>5 double values (const double )</li></ul>
 */
int  GDX_CALLCONV d_gdxDataReadSlice (gdxHandle_t pgdx, const char *UelFilterStr[], int *Dimen, TDataStoreProc_t DP)
{
  int d_s[]={3,55,4,59};
  GAMS_UNUSED(pgdx)
  GAMS_UNUSED(UelFilterStr)
  GAMS_UNUSED(Dimen)
  GAMS_UNUSED(DP)
  printAndReturn(gdxDataReadSlice,3,int )
}

/** Prepare for the reading of a slice of data from a data set. The actual read of the data is done by calling gdxDataReadSlice. When finished reading, call gdxDataReadDone. Returns zero if the operation is not possible.
 * @param pgdx gdx object handle
 * @param SyNr Symbol number to read, range 1..NrSymbols; SyNr = 0 reads universe.
 * @param ElemCounts Array of integers, each position indicating the number of unique indices in that position.
 */
int  GDX_CALLCONV d_gdxDataReadSliceStart (gdxHandle_t pgdx, int SyNr, int ElemCounts[])
{
  int d_s[]={3,3,52};
  GAMS_UNUSED(pgdx)
  GAMS_UNUSED(SyNr)
  GAMS_UNUSED(ElemCounts)
  printAndReturn(gdxDataReadSliceStart,2,int )
}

/** Read the next record using strings for the unique elements. The reading should be initialized by calling DataReadStrStart. Returns zero if the operation is not possible or if there is no more data.
 * @param pgdx gdx object handle
 * @param KeyStr The index of the record as strings for the unique elements. Array of strings with one string for each dimension.
 * @param Values The data of the record (level, marginal, lower-, upper-bound, scale).
 * @param DimFrst The first index position in KeyStr that changed.
 */
int  GDX_CALLCONV d_gdxDataReadStr (gdxHandle_t pgdx, char *KeyStr[], double Values[], int *DimFrst)
{
  int d_s[]={3,56,54,4};
  GAMS_UNUSED(pgdx)
  GAMS_UNUSED(KeyStr)
  GAMS_UNUSED(Values)
  GAMS_UNUSED(DimFrst)
  printAndReturn(gdxDataReadStr,3,int )
}

/** Initialize the reading of a symbol in string mode. Returns zero if the operation is not possible.
 * @param pgdx gdx object handle
 * @param SyNr The index number of the symbol (range 0..NrSymbols); SyNr = 0 reads universe.
 * @param NrRecs The maximum number of records available for reading. The actual number of records may be less when a filter is applied to the records read.
 */
int  GDX_CALLCONV d_gdxDataReadStrStart (gdxHandle_t pgdx, int SyNr, int *NrRecs)
{
  int d_s[]={3,3,4};
  GAMS_UNUSED(pgdx)
  GAMS_UNUSED(SyNr)
  GAMS_UNUSED(NrRecs)
  printAndReturn(gdxDataReadStrStart,2,int )
}

/** Map a slice index in to the corresponding unique elements. After calling DataReadSliceStart, each index position is mapped from 0 to N(d)-1. This function maps this index space back in to unique elements represented as strings. Returns zero if the operation is not possible.
 * @param pgdx gdx object handle
 * @param SliceKeyInt The slice index to be mapped to strings with one entry for each symbol dimension.
 * @param KeyStr Array of strings containing the unique elements.
 */
int  GDX_CALLCONV d_gdxDataSliceUELS (gdxHandle_t pgdx, const int SliceKeyInt[], char *KeyStr[])
{
  int d_s[]={3,51,56};
  GAMS_UNUSED(pgdx)
  GAMS_UNUSED(SliceKeyInt)
  GAMS_UNUSED(KeyStr)
  printAndReturn(gdxDataSliceUELS,2,int )
}

/** Finish a write operation. Returns zero if the operation is not possible.
 * @param pgdx gdx object handle
 */
int  GDX_CALLCONV d_gdxDataWriteDone (gdxHandle_t pgdx)
{
  int d_s[]={3};
  GAMS_UNUSED(pgdx)
  printAndReturn(gdxDataWriteDone,0,int )
}

/** Write a data element in mapped mode. Returns zero if the operation is not possible.
 * @param pgdx gdx object handle
 * @param KeyInt The index for this element using mapped values.
 * @param Values The values for this element.
 */
int  GDX_CALLCONV d_gdxDataWriteMap (gdxHandle_t pgdx, const int KeyInt[], const double Values[])
{
  int d_s[]={3,51,53};
  GAMS_UNUSED(pgdx)
  GAMS_UNUSED(KeyInt)
  GAMS_UNUSED(Values)
  printAndReturn(gdxDataWriteMap,2,int )
}

/** Start writing a new symbol in mapped mode. Returns zero if the operation is not possible.
 * @param pgdx gdx object handle
 * @param SyId Name of the symbol (up to 63 characters) or acronym. The first character of a symbol must be a letter. Following symbol characters may be letters, digits, and underscores. Symbol names must be new and unique. Might be an empty string at gdxAcronymName.
 * @param ExplTxt Explanatory text for the symbol (up to 255 characters).
 * @param Dimen Dimension of the symbol.
 * @param Typ Type of the symbol.
 * @param UserInfo User field value storing additional data; GAMS follows the following conventions: <table> <tr><td>Type</td><td>Value(s)</td></tr> <tr><td>Aliased Set</td><td>The symbol number of the aliased set, or zero for the universe</td></tr> <tr><td>Set</td><td>Zero</td></tr> <tr><td>Parameter</td><td>Zero</td></tr> <tr><td>Variable</td><td>The variable type: binary=1, integer=2, positive=3, negative=4, free=5, sos1=6, sos2=7, semicontinous=8, semiinteger=9</td></tr> <tr><td>Equation</td><td>The equation type: eque=53, equg=54, equl=55, equn=56, equx=57, equc=58, equb=59</td></tr> </table>
 */
int  GDX_CALLCONV d_gdxDataWriteMapStart (gdxHandle_t pgdx, const char *SyId, const char *ExplTxt, int Dimen, int Typ, int UserInfo)
{
  int d_s[]={3,11,11,3,3,3};
  GAMS_UNUSED(pgdx)
  GAMS_UNUSED(SyId)
  GAMS_UNUSED(ExplTxt)
  GAMS_UNUSED(Dimen)
  GAMS_UNUSED(Typ)
  GAMS_UNUSED(UserInfo)
  printAndReturn(gdxDataWriteMapStart,5,int )
}

/** Write a data element in raw mode. Returns zero if the operation is not possible.
 * @param pgdx gdx object handle
 * @param KeyInt The index for this element.
 * @param Values The values for this element.
 */
int  GDX_CALLCONV d_gdxDataWriteRaw (gdxHandle_t pgdx, const int KeyInt[], const double Values[])
{
  int d_s[]={3,51,53};
  GAMS_UNUSED(pgdx)
  GAMS_UNUSED(KeyInt)
  GAMS_UNUSED(Values)
  printAndReturn(gdxDataWriteRaw,2,int )
}

/** Start writing a new symbol in raw mode. Returns zero if the operation is not possible.
 * @param pgdx gdx object handle
 * @param SyId Name of the symbol (up to 63 characters). The first character of a symbol must be a letter. Following symbol characters may be letters, digits, and underscores. Symbol names must be new and unique.
 * @param ExplTxt Explanatory text for the symbol (up to 255 characters).
 * @param Dimen Dimension of the symbol (up to 20).
 * @param Typ Type of the symbol (set=0, parameter=1, variable=2, equation=3, alias=4).
 * @param UserInfo User field value storing additional data; GAMS follows the following conventions: <table> <tr> <td>Type</td> <td>Value(s)</td> </tr> <tr> <td>Aliased Set</td> <td>The symbol number of the aliased set, or zero for the universe</td> </tr> <tr> <td>Set</td> <td>Zero</td> </tr> <tr> <td>Parameter</td> <td>Zero</td> </tr> <tr> <td>Variable</td> <td>The variable type: binary=1, integer=2, positive=3, negative=4, free=5, sos1=6, sos2=7, semicontinous=8, semiinteger=9 </td> </tr> <tr> <td>Equation</td> <td>The equation type: eque=53, equg=54, equl=55, equn=56, equx=57, equc=58, equb=59</td> </tr> </table>
 */
int  GDX_CALLCONV d_gdxDataWriteRawStart (gdxHandle_t pgdx, const char *SyId, const char *ExplTxt, int Dimen, int Typ, int UserInfo)
{
  int d_s[]={3,11,11,3,3,3};
  GAMS_UNUSED(pgdx)
  GAMS_UNUSED(SyId)
  GAMS_UNUSED(ExplTxt)
  GAMS_UNUSED(Dimen)
  GAMS_UNUSED(Typ)
  GAMS_UNUSED(UserInfo)
  printAndReturn(gdxDataWriteRawStart,5,int )
}

/** Write a data element in string mode. Each element string must follow the GAMS rules for unique elements. Returns zero if the operation is not possible.
 * @param pgdx gdx object handle
 * @param KeyStr The index for this element using strings for the unique elements. One entry for each symbol dimension.
 * @param Values The values for this element (level, marginal, lower-, upper-bound, scale).
 */
int  GDX_CALLCONV d_gdxDataWriteStr (gdxHandle_t pgdx, const char *KeyStr[], const double Values[])
{
  int d_s[]={3,55,53};
  GAMS_UNUSED(pgdx)
  GAMS_UNUSED(KeyStr)
  GAMS_UNUSED(Values)
  printAndReturn(gdxDataWriteStr,2,int )
}

/** Start writing a new symbol in string mode. Returns zero if the operation is not possible or failed.
 * @param pgdx gdx object handle
 * @param SyId Name of the symbol (limited to 63 characters). The first character of a symbol must be a letter. Following symbol characters may be letters, digits, and underscores. Symbol names must be new and unique.
 * @param ExplTxt Explanatory text for the symbol (limited to 255 characters). Mixed quote characters will be unified to first occurring one.
 * @param Dimen Dimension of the symbol (limited to 20).
 * @param Typ Type of the symbol (set=0, parameter=1, variable=2, equation=3, alias=4).
 * @param UserInfo Supply additional data. See gdxDataWriteRawStart for more information.
 */
int  GDX_CALLCONV d_gdxDataWriteStrStart (gdxHandle_t pgdx, const char *SyId, const char *ExplTxt, int Dimen, int Typ, int UserInfo)
{
  int d_s[]={3,11,11,3,3,3};
  GAMS_UNUSED(pgdx)
  GAMS_UNUSED(SyId)
  GAMS_UNUSED(ExplTxt)
  GAMS_UNUSED(Dimen)
  GAMS_UNUSED(Typ)
  GAMS_UNUSED(UserInfo)
  printAndReturn(gdxDataWriteStrStart,5,int )
}

/** Returns a version descriptor of the library. Always non-zero.
 * @param pgdx gdx object handle
 * @param V Contains version string after return.
 */
int  GDX_CALLCONV d_gdxGetDLLVersion (gdxHandle_t pgdx, char *V)
{
  int d_s[]={3,12};
  GAMS_UNUSED(pgdx)
  GAMS_UNUSED(V)
  printAndReturn(gdxGetDLLVersion,1,int )
}

/** Returns the number of errors.
 * @param pgdx gdx object handle
 */
int  GDX_CALLCONV d_gdxErrorCount (gdxHandle_t pgdx)
{
  int d_s[]={3};
  GAMS_UNUSED(pgdx)
  printAndReturn(gdxErrorCount,0,int )
}

/** Returns the text for a given error number. Always non-zero.
 * @param pgdx gdx object handle
 * @param ErrNr Error number.
 * @param ErrMsg Error message (output argument). Contains error text after return.
 */
int  GDX_CALLCONV d_gdxErrorStr (gdxHandle_t pgdx, int ErrNr, char *ErrMsg)
{
  int d_s[]={3,3,12};
  GAMS_UNUSED(pgdx)
  GAMS_UNUSED(ErrNr)
  GAMS_UNUSED(ErrMsg)
  printAndReturn(gdxErrorStr,2,int )
}

/** Returns file format number and compression level used. Always non-zero.
 * @param pgdx gdx object handle
 * @param FileVer File format number or zero if the file is not open.
 * @param ComprLev Compression used; 0= no compression, 1=zlib.
 */
int  GDX_CALLCONV d_gdxFileInfo (gdxHandle_t pgdx, int *FileVer, int *ComprLev)
{
  int d_s[]={3,4,4};
  GAMS_UNUSED(pgdx)
  GAMS_UNUSED(FileVer)
  GAMS_UNUSED(ComprLev)
  printAndReturn(gdxFileInfo,2,int )
}

/** Return strings for file version and file producer. Always non-zero.
 * @param pgdx gdx object handle
 * @param FileStr Version string (out argument). Known versions are V5, V6U, V6C and V7.
 * @param ProduceStr Producer string (out argument). The producer is the application that wrote the GDX file.
 */
int  GDX_CALLCONV d_gdxFileVersion (gdxHandle_t pgdx, char *FileStr, char *ProduceStr)
{
  int d_s[]={3,12,12};
  GAMS_UNUSED(pgdx)
  GAMS_UNUSED(FileStr)
  GAMS_UNUSED(ProduceStr)
  printAndReturn(gdxFileVersion,2,int )
}

/** Check if there is a filter defined based on its number as used in gdxFilterRegisterStart. Returns zero if the operation is not possible.
 * @param pgdx gdx object handle
 * @param FilterNr Filter number as used in FilterRegisterStart.
 */
int  GDX_CALLCONV d_gdxFilterExists (gdxHandle_t pgdx, int FilterNr)
{
  int d_s[]={3,3};
  GAMS_UNUSED(pgdx)
  GAMS_UNUSED(FilterNr)
  printAndReturn(gdxFilterExists,1,int )
}

/** Add a unique element to the current filter definition, zero if the index number is out of range or was never mapped into the user index space.
 * @param pgdx gdx object handle
 * @param UelMap Unique element number in the user index space or -1 if element was never mapped.
 */
int  GDX_CALLCONV d_gdxFilterRegister (gdxHandle_t pgdx, int UelMap)
{
  int d_s[]={3,3};
  GAMS_UNUSED(pgdx)
  GAMS_UNUSED(UelMap)
  printAndReturn(gdxFilterRegister,1,int )
}

/** Finish registration of unique elements for a filter. Returns zero if the operation is not possible.
 * @param pgdx gdx object handle
 */
int  GDX_CALLCONV d_gdxFilterRegisterDone (gdxHandle_t pgdx)
{
  int d_s[]={3};
  GAMS_UNUSED(pgdx)
  printAndReturn(gdxFilterRegisterDone,0,int )
}

/** Define a unique element filter. Returns zero if the operation is not possible.
 * @param pgdx gdx object handle
 * @param FilterNr Filter number to be assigned.
 */
int  GDX_CALLCONV d_gdxFilterRegisterStart (gdxHandle_t pgdx, int FilterNr)
{
  int d_s[]={3,3};
  GAMS_UNUSED(pgdx)
  GAMS_UNUSED(FilterNr)
  printAndReturn(gdxFilterRegisterStart,1,int )
}

/** Search for a symbol by name in the symbol table; the search is not case-sensitive. <ul><li>When the symbol is found, SyNr contains the symbol number and the function returns a non-zero integer.</li> <li>When the symbol is not found, the function returns zero and SyNr is set to -1.</li></ul>
 * @param pgdx gdx object handle
 * @param SyId Name of the symbol (must not exceed 63 characters).
 * @param SyNr Symbol number (>=1 if exists, 0 for universe and -1 if not found).
 */
int  GDX_CALLCONV d_gdxFindSymbol (gdxHandle_t pgdx, const char *SyId, int *SyNr)
{
  int d_s[]={3,11,4};
  GAMS_UNUSED(pgdx)
  GAMS_UNUSED(SyId)
  GAMS_UNUSED(SyNr)
  printAndReturn(gdxFindSymbol,2,int )
}

/** Retrieve the string and node number for an entry in the string table. Returns zero if the operation is not possible.
 * @param pgdx gdx object handle
 * @param TxtNr String table index.
 * @param Txt Text found for the entry. Buffer should be 256 bytes wide.
 * @param Node Node number (user space) found for the entry.
 */
int  GDX_CALLCONV d_gdxGetElemText (gdxHandle_t pgdx, int TxtNr, char *Txt, int *Node)
{
  int d_s[]={3,3,12,4};
  GAMS_UNUSED(pgdx)
  GAMS_UNUSED(TxtNr)
  GAMS_UNUSED(Txt)
  GAMS_UNUSED(Node)
  printAndReturn(gdxGetElemText,3,int )
}

/** Returns the last error number or zero if there was no error. Calling this function will clear the last error stored.
 * @param pgdx gdx object handle
 */
int  GDX_CALLCONV d_gdxGetLastError (gdxHandle_t pgdx)
{
  int d_s[]={3};
  GAMS_UNUSED(pgdx)
  printAndReturn(gdxGetLastError,0,int )
}

/** Return the number of bytes used by the data objects.
 * @param pgdx gdx object handle
 */
INT64  GDX_CALLCONV d_gdxGetMemoryUsed (gdxHandle_t pgdx)
{
  int d_s[]={23};
  GAMS_UNUSED(pgdx)
  printAndReturn(gdxGetMemoryUsed,0,INT64 )
}

/** Retrieve the internal values for special values. Always non-zero.
 * @param pgdx gdx object handle
 * @param AVals 6-element array of special values used for Undef (0), NA (1), +Inf (2), -Inf (3), Eps (4), Acronym (6).
 */
int  GDX_CALLCONV d_gdxGetSpecialValues (gdxHandle_t pgdx, double AVals[])
{
  int d_s[]={3,58};
  GAMS_UNUSED(pgdx)
  GAMS_UNUSED(AVals)
  printAndReturn(gdxGetSpecialValues,1,int )
}

/** Get the string for a unique element using a mapped index. Returns zero if the operation is not possible.
 * @param pgdx gdx object handle
 * @param UelNr Index number in user space (range 1..NrUserElem).
 * @param Uel String for the unique element which may be up to 63 characters.
 */
int  GDX_CALLCONV d_gdxGetUEL (gdxHandle_t pgdx, int UelNr, char *Uel)
{
  int d_s[]={3,3,12};
  GAMS_UNUSED(pgdx)
  GAMS_UNUSED(UelNr)
  GAMS_UNUSED(Uel)
  printAndReturn(gdxGetUEL,2,int )
}

/** Classify a value as a potential special value. Non-zero if D is a special value, zero otherwise.
 * @param pgdx gdx object handle
 * @param D Value to classify.
 * @param sv Classification.
 */
int  GDX_CALLCONV d_gdxMapValue (gdxHandle_t pgdx, double D, int *sv)
{
  int d_s[]={3,13,4};
  GAMS_UNUSED(pgdx)
  GAMS_UNUSED(D)
  GAMS_UNUSED(sv)
  printAndReturn(gdxMapValue,2,int )
}

/** Open an existing GDX file for output. Non-zero if the file can be opened, zero otherwise.
 * @param pgdx gdx object handle
 * @param FileName File name of the GDX file to be created (arbitrary length).
 * @param Producer Name of program that appends to the GDX file (should not exceed 255 characters).
 * @param ErrNr Returns an error code or zero if there is no error.
 */
int  GDX_CALLCONV d_gdxOpenAppend (gdxHandle_t pgdx, const char *FileName, const char *Producer, int *ErrNr)
{
  int d_s[]={3,11,11,4};
  GAMS_UNUSED(pgdx)
  GAMS_UNUSED(FileName)
  GAMS_UNUSED(Producer)
  GAMS_UNUSED(ErrNr)
  printAndReturn(gdxOpenAppend,3,int )
}

/** Open a GDX file for reading. Non-zero if the file can be opened, zero otherwise.
 * @param pgdx gdx object handle
 * @param FileName File name of the GDX file to be opened (arbitrary length).
 * @param ErrNr Returns an error code or zero if there is no error.
 */
int  GDX_CALLCONV d_gdxOpenRead (gdxHandle_t pgdx, const char *FileName, int *ErrNr)
{
  int d_s[]={3,11,4};
  GAMS_UNUSED(pgdx)
  GAMS_UNUSED(FileName)
  GAMS_UNUSED(ErrNr)
  printAndReturn(gdxOpenRead,2,int )
}

/** Open a GDX file for reading allowing for skipping sections. Non-zero if the file can be opened, zero otherwise.
 * @param pgdx gdx object handle
 * @param FileName File name of the GDX file to be opened (arbitrary length).
 * @param ReadMode Bitmap skip reading sections: 0-bit: string (1 skip reading string).
 * @param ErrNr Returns an error code or zero if there is no error.
 */
int  GDX_CALLCONV d_gdxOpenReadEx (gdxHandle_t pgdx, const char *FileName, int ReadMode, int *ErrNr)
{
  int d_s[]={3,11,3,4};
  GAMS_UNUSED(pgdx)
  GAMS_UNUSED(FileName)
  GAMS_UNUSED(ReadMode)
  GAMS_UNUSED(ErrNr)
  printAndReturn(gdxOpenReadEx,3,int )
}

/** Open a new GDX file for output. Non-zero if the file can be opened, zero otherwise.
 * @param pgdx gdx object handle
 * @param FileName File name of the GDX file to be created with arbitrary length.
 * @param Producer Name of program that creates the GDX file (should not exceed 255 characters).
 * @param ErrNr Returns an error code or zero if there is no error.
 */
int  GDX_CALLCONV d_gdxOpenWrite (gdxHandle_t pgdx, const char *FileName, const char *Producer, int *ErrNr)
{
  int d_s[]={3,11,11,4};
  GAMS_UNUSED(pgdx)
  GAMS_UNUSED(FileName)
  GAMS_UNUSED(Producer)
  GAMS_UNUSED(ErrNr)
  printAndReturn(gdxOpenWrite,3,int )
}

/** Create a GDX file for writing with explicitly given compression flag. Non-zero if the file can be opened, zero otherwise.
 * @param pgdx gdx object handle
 * @param FileName File name of the GDX file to be created with arbitrary length.
 * @param Producer Name of program that creates the GDX file (should not exceed 255 characters).
 * @param Compr Zero for no compression; non-zero uses compression (if available).
 * @param ErrNr Returns an error code or zero if there is no error.
 */
int  GDX_CALLCONV d_gdxOpenWriteEx (gdxHandle_t pgdx, const char *FileName, const char *Producer, int Compr, int *ErrNr)
{
  int d_s[]={3,11,11,3,4};
  GAMS_UNUSED(pgdx)
  GAMS_UNUSED(FileName)
  GAMS_UNUSED(Producer)
  GAMS_UNUSED(Compr)
  GAMS_UNUSED(ErrNr)
  printAndReturn(gdxOpenWriteEx,4,int )
}

/** Reset the internal values for special values. Always non-zero.
 * @param pgdx gdx object handle
 */
int  GDX_CALLCONV d_gdxResetSpecialValues (gdxHandle_t pgdx)
{
  int d_s[]={3};
  GAMS_UNUSED(pgdx)
  printAndReturn(gdxResetSpecialValues,0,int )
}

/** Test if any of the elements of the set has an associated text. Non-zero if the Set contains at least one unique element that has associated text, zero otherwise.
 * @param pgdx gdx object handle
 * @param SyNr Set symbol number (range 1..NrSymbols); SyNr = 0 reads universe.
 */
int  GDX_CALLCONV d_gdxSetHasText (gdxHandle_t pgdx, int SyNr)
{
  int d_s[]={3,3};
  GAMS_UNUSED(pgdx)
  GAMS_UNUSED(SyNr)
  printAndReturn(gdxSetHasText,1,int )
}

/** Set the internal values for special values when reading a GDX file. Before calling this function, initialize the array of special values by calling gdxGetSpecialValues first. Always non-zero.
 * @param pgdx gdx object handle
 * @param AVals 5-element array of special values to be used for Undef, NA, +Inf, -Inf, and Eps. Note that the values do not have to be unique.
 */
int  GDX_CALLCONV d_gdxSetReadSpecialValues (gdxHandle_t pgdx, const double AVals[])
{
  int d_s[]={3,57};
  GAMS_UNUSED(pgdx)
  GAMS_UNUSED(AVals)
  printAndReturn(gdxSetReadSpecialValues,1,int )
}

/** Set the internal values for special values. Before calling this function, initialize the array of special values by calling gdxGetSpecialValues first. Note, values in AVals have to be unique. Non-zero if all values specified are unique, zero otherwise.
 * @param pgdx gdx object handle
 * @param AVals Array of special values to be used Undef (0), NA (1), +Inf (2), -Inf (3), and EPS (4). Note that the values have to be unique and AVals should have length 7.
 */
int  GDX_CALLCONV d_gdxSetSpecialValues (gdxHandle_t pgdx, const double AVals[])
{
  int d_s[]={3,57};
  GAMS_UNUSED(pgdx)
  GAMS_UNUSED(AVals)
  printAndReturn(gdxSetSpecialValues,1,int )
}

/** Set the Node number for an entry in the string table. After registering a string with AddSetText, we can assign a node number for later retrieval. Returns zero if the operation is not possible.
 * @param pgdx gdx object handle
 * @param TxtNr Index number of the entry to be modified.
 * @param Node The new Node value for the entry.
 */
int  GDX_CALLCONV d_gdxSetTextNodeNr (gdxHandle_t pgdx, int TxtNr, int Node)
{
  int d_s[]={3,3,3};
  GAMS_UNUSED(pgdx)
  GAMS_UNUSED(TxtNr)
  GAMS_UNUSED(Node)
  printAndReturn(gdxSetTextNodeNr,2,int )
}

/** Set the amount of trace (debug) information generated. Always non-zero.
 * @param pgdx gdx object handle
 * @param N Tracing level N <= 0 no tracing N >= 3 maximum tracing.
 * @param s A string to be included in the trace output (arbitrary length).
 */
int  GDX_CALLCONV d_gdxSetTraceLevel (gdxHandle_t pgdx, int N, const char *s)
{
  int d_s[]={3,3,11};
  GAMS_UNUSED(pgdx)
  GAMS_UNUSED(N)
  GAMS_UNUSED(s)
  printAndReturn(gdxSetTraceLevel,2,int )
}

/** Returns the length of the longest UEL used for every index position for a given symbol.
 * @param pgdx gdx object handle
 * @param SyNr Symbol number (range 1..NrSymbols); SyNr = 0 reads universe.
 * @param LengthInfo The longest length for each index position. This output argument should be able to store one integer for each symbol dimension.
 */
int  GDX_CALLCONV d_gdxSymbIndxMaxLength (gdxHandle_t pgdx, int SyNr, int LengthInfo[])
{
  int d_s[]={3,3,52};
  GAMS_UNUSED(pgdx)
  GAMS_UNUSED(SyNr)
  GAMS_UNUSED(LengthInfo)
  printAndReturn(gdxSymbIndxMaxLength,2,int )
}

/** Returns the length of the longest symbol name in the GDX file.
 * @param pgdx gdx object handle
 */
int  GDX_CALLCONV d_gdxSymbMaxLength (gdxHandle_t pgdx)
{
  int d_s[]={3};
  GAMS_UNUSED(pgdx)
  printAndReturn(gdxSymbMaxLength,0,int )
}

/** Add a line of comment text for a symbol. Returns zero if the operation is not possible.
 * @param pgdx gdx object handle
 * @param SyNr The symbol number (range 1..NrSymbols); if SyNr <= 0 the current symbol being written.
 * @param Txt String to add which should not exceed a length of 255 characters.
 */
int  GDX_CALLCONV d_gdxSymbolAddComment (gdxHandle_t pgdx, int SyNr, const char *Txt)
{
  int d_s[]={3,3,11};
  GAMS_UNUSED(pgdx)
  GAMS_UNUSED(SyNr)
  GAMS_UNUSED(Txt)
  printAndReturn(gdxSymbolAddComment,2,int )
}

/** Retrieve a line of comment text for a symbol. Returns zero if the operation is not possible.
 * @param pgdx gdx object handle
 * @param SyNr The symbol number (range 1..NrSymbols); SyNr = 0 reads universe.
 * @param N Line number in the comment block (1..Count).
 * @param Txt String containing the line requested (empty on error). Buffer should be able to hold 255 characters. Potential causes for empty strings are symbol- (SyNr) or line-number (N) out of range.
 */
int  GDX_CALLCONV d_gdxSymbolGetComment (gdxHandle_t pgdx, int SyNr, int N, char *Txt)
{
  int d_s[]={3,3,3,12};
  GAMS_UNUSED(pgdx)
  GAMS_UNUSED(SyNr)
  GAMS_UNUSED(N)
  GAMS_UNUSED(Txt)
  printAndReturn(gdxSymbolGetComment,3,int )
}

/** Retrieve the domain of a symbol. Returns zero if the operation is not possible.
 * @param pgdx gdx object handle
 * @param SyNr The index number of the symbol (range 1..NrSymbols); SyNr = 0 reads universe.
 * @param DomainSyNrs Array (length=symbol dim) returning the set identifiers or "*"; DomainSyNrs[D] will contain the index number of the one dimensional set or alias used as the domain for index position D. A value of zero represents the universe "*".
 */
int  GDX_CALLCONV d_gdxSymbolGetDomain (gdxHandle_t pgdx, int SyNr, int DomainSyNrs[])
{
  int d_s[]={3,3,52};
  GAMS_UNUSED(pgdx)
  GAMS_UNUSED(SyNr)
  GAMS_UNUSED(DomainSyNrs)
  printAndReturn(gdxSymbolGetDomain,2,int )
}

/** Retrieve the domain of a symbol (using relaxed or domain information). Returns zero if the operation is not possible.
 * @param pgdx gdx object handle
 * @param SyNr The index number of the symbol (range 1..NrSymbols); SyNr = 0 reads universe.
 * @param DomainIDs DomainIDs[D] will contain the strings as they were stored with the call gdxSymbolSetDomainX. If gdxSymbolSetDomainX was never called, but gdxSymbolSetDomain was called, that information will be used instead. Length of this array should by dimensionality of the symbol. The special domain name "*" denotes the universe domain (all known UELs).
 */
int  GDX_CALLCONV d_gdxSymbolGetDomainX (gdxHandle_t pgdx, int SyNr, char *DomainIDs[])
{
  int d_s[]={3,3,56};
  GAMS_UNUSED(pgdx)
  GAMS_UNUSED(SyNr)
  GAMS_UNUSED(DomainIDs)
  printAndReturn(gdxSymbolGetDomainX,2,int )
}

/** Returns dimensionality of a symbol.
 * @param pgdx gdx object handle
 * @param SyNr The symbol number (range 0..NrSymbols); return universe info when SyNr = 0..
 */
int  GDX_CALLCONV d_gdxSymbolDim (gdxHandle_t pgdx, int SyNr)
{
  int d_s[]={3,3};
  GAMS_UNUSED(pgdx)
  GAMS_UNUSED(SyNr)
  printAndReturn(gdxSymbolDim,1,int )
}

/** Returns information (name, dimension count, type) about a symbol from the symbol table. Returns zero if the symbol number is out of range, non-zero otherwise.
 * @param pgdx gdx object handle
 * @param SyNr The symbol number (range 0..NrSymbols); return universe info () when SyNr = 0.
 * @param SyId Name of the symbol (buffer should be 64 bytes long). Magic name "*" for universe.
 * @param Dimen Dimension of the symbol (range 0..20).
 * @param Typ Symbol type (set=0, parameter=1, variable=2, equation=3, alias=4).
 */
int  GDX_CALLCONV d_gdxSymbolInfo (gdxHandle_t pgdx, int SyNr, char *SyId, int *Dimen, int *Typ)
{
  int d_s[]={3,3,12,4,4};
  GAMS_UNUSED(pgdx)
  GAMS_UNUSED(SyNr)
  GAMS_UNUSED(SyId)
  GAMS_UNUSED(Dimen)
  GAMS_UNUSED(Typ)
  printAndReturn(gdxSymbolInfo,4,int )
}

/** Returns additional information about a symbol. Returns zero if the symbol number is out of range, non-zero otherwise.
 * @param pgdx gdx object handle
 * @param SyNr The symbol number (range 0..NrSymbols); return universe info when SyNr = 0.
 * @param RecCnt Total number of records stored (unmapped); for the universe (SyNr = 0) this is the number of entries when the GDX file was opened for reading.
 * @param UserInfo User field value storing additional data; GAMS follows the following conventions: <table> <tr> <td>Type</td> <td>Value(s)</td> </tr> <tr> <td>Aliased Set</td> <td>The symbol number of the aliased set, or zero for the universe</td> </tr> <tr> <td>Set</td> <td>Zero</td> </tr> <tr> <td>Parameter</td> <td>Zero</td> </tr> <tr> <td>Variable</td> <td>The variable type: binary=1, integer=2, positive=3, negative=4, free=5, sos1=6, sos2=7, semicontinous=8, semiinteger=9 </td> </tr> <tr> <td>Equation</td> <td>The equation type: eque=53, equg=54, equl=55, equn=56, equx=57, equc=58, equb=59</td> </tr> </table>
 * @param ExplTxt Explanatory text for the symbol. Buffer for this output argument should be 256 bytes long.
 */
int  GDX_CALLCONV d_gdxSymbolInfoX (gdxHandle_t pgdx, int SyNr, int *RecCnt, int *UserInfo, char *ExplTxt)
{
  int d_s[]={3,3,4,4,12};
  GAMS_UNUSED(pgdx)
  GAMS_UNUSED(SyNr)
  GAMS_UNUSED(RecCnt)
  GAMS_UNUSED(UserInfo)
  GAMS_UNUSED(ExplTxt)
  printAndReturn(gdxSymbolInfoX,4,int )
}

/** Define the domain of a symbol for which a write data operation just started using DataWriteRawStart, DataWriteMapStart or DataWriteStrStart. Returns zero if the operation is not possible.
 * @param pgdx gdx object handle
 * @param DomainIDs Array of identifiers (domain names) or "*" (universe). One domain name for each symbol dimension.
 */
int  GDX_CALLCONV d_gdxSymbolSetDomain (gdxHandle_t pgdx, const char *DomainIDs[])
{
  int d_s[]={3,55};
  GAMS_UNUSED(pgdx)
  GAMS_UNUSED(DomainIDs)
  printAndReturn(gdxSymbolSetDomain,1,int )
}

/** Define the domain of a symbol (relaxed version). Returns zero if the operation is not possible.
 * @param pgdx gdx object handle
 * @param SyNr The index number of the symbol, range from 0 to NrSymbols; SyNr = 0 reads universe.
 * @param DomainIDs Array of identifiers (domain names) or "*" (universe). One domain name per symbol dimension with not more than 63 characters.
 */
int  GDX_CALLCONV d_gdxSymbolSetDomainX (gdxHandle_t pgdx, int SyNr, const char *DomainIDs[])
{
  int d_s[]={3,3,55};
  GAMS_UNUSED(pgdx)
  GAMS_UNUSED(SyNr)
  GAMS_UNUSED(DomainIDs)
  printAndReturn(gdxSymbolSetDomainX,2,int )
}

/** Returns the number of symbols and unique elements. Always non-zero.
 * @param pgdx gdx object handle
 * @param SyCnt Number of symbols (sets, parameters, ...) available in the GDX file.
 * @param UelCnt Number of unique elements (UELs) stored in the GDX file.
 */
int  GDX_CALLCONV d_gdxSystemInfo (gdxHandle_t pgdx, int *SyCnt, int *UelCnt)
{
  int d_s[]={3,4,4};
  GAMS_UNUSED(pgdx)
  GAMS_UNUSED(SyCnt)
  GAMS_UNUSED(UelCnt)
  printAndReturn(gdxSystemInfo,2,int )
}

/** Returns the length of the longest unique element (UEL) name.
 * @param pgdx gdx object handle
 */
int  GDX_CALLCONV d_gdxUELMaxLength (gdxHandle_t pgdx)
{
  int d_s[]={3};
  GAMS_UNUSED(pgdx)
  printAndReturn(gdxUELMaxLength,0,int )
}

/** Finish registration of unique elements. Returns zero if the operation is not possible.
 * @param pgdx gdx object handle
 */
int  GDX_CALLCONV d_gdxUELRegisterDone (gdxHandle_t pgdx)
{
  int d_s[]={3};
  GAMS_UNUSED(pgdx)
  printAndReturn(gdxUELRegisterDone,0,int )
}

/** Register unique element in mapped mode. A unique element must follow the GAMS rules when it contains quote characters. Returns zero if the operation is not possible.
 * @param pgdx gdx object handle
 * @param UMap User index number to be assigned to the unique element, -1 if not found or the element was never mapped.
 * @param Uel String for unique element (max. 63 chars and no single-/double-quote mixing).
 */
int  GDX_CALLCONV d_gdxUELRegisterMap (gdxHandle_t pgdx, int UMap, const char *Uel)
{
  int d_s[]={3,3,11};
  GAMS_UNUSED(pgdx)
  GAMS_UNUSED(UMap)
  GAMS_UNUSED(Uel)
  printAndReturn(gdxUELRegisterMap,2,int )
}

/** Start registering unique elements in mapped mode. Returns zero if the operation is not possible.
 * @param pgdx gdx object handle
 */
int  GDX_CALLCONV d_gdxUELRegisterMapStart (gdxHandle_t pgdx)
{
  int d_s[]={3};
  GAMS_UNUSED(pgdx)
  printAndReturn(gdxUELRegisterMapStart,0,int )
}

/** Register unique element in raw mode. This can only be used while writing to a GDX file. Returns zero if the operation is not possible.
 * @param pgdx gdx object handle
 * @param Uel String for unique element (UEL) which may not exceed 63 characters in length. Furthermore a UEL string must not mix single- and double-quotes.
 */
int  GDX_CALLCONV d_gdxUELRegisterRaw (gdxHandle_t pgdx, const char *Uel)
{
  int d_s[]={3,11};
  GAMS_UNUSED(pgdx)
  GAMS_UNUSED(Uel)
  printAndReturn(gdxUELRegisterRaw,1,int )
}

/** Start registering unique elements in raw mode. Returns zero if the operation is not possible.
 * @param pgdx gdx object handle
 */
int  GDX_CALLCONV d_gdxUELRegisterRawStart (gdxHandle_t pgdx)
{
  int d_s[]={3};
  GAMS_UNUSED(pgdx)
  printAndReturn(gdxUELRegisterRawStart,0,int )
}

/** Register a unique element in string mode. A unique element must follow the GAMS rules when it contains quote characters. Non-zero if the element was registered, zero otherwise.
 * @param pgdx gdx object handle
 * @param Uel String for unique element (UEL) which may not exceed a length of 63 characters. Furthermore a UEL string must not mix single- and double-quotes.
 * @param UelNr Internal index number assigned to this unique element in user space (or -1 if not found).
 */
int  GDX_CALLCONV d_gdxUELRegisterStr (gdxHandle_t pgdx, const char *Uel, int *UelNr)
{
  int d_s[]={3,11,4};
  GAMS_UNUSED(pgdx)
  GAMS_UNUSED(Uel)
  GAMS_UNUSED(UelNr)
  printAndReturn(gdxUELRegisterStr,2,int )
}

/** Start registering unique elements in string mode. Returns zero if the operation is not possible.
 * @param pgdx gdx object handle
 */
int  GDX_CALLCONV d_gdxUELRegisterStrStart (gdxHandle_t pgdx)
{
  int d_s[]={3};
  GAMS_UNUSED(pgdx)
  printAndReturn(gdxUELRegisterStrStart,0,int )
}

/** Search for unique element by its string. Non-zero if the element was found, zero otherwise.
 * @param pgdx gdx object handle
 * @param Uel String to be searched (not longer than 63 characters, don't mix single- and double-quotes).
 * @param UelNr Internal unique element number or -1 if not found.
 * @param UelMap User mapping for the element or -1 if not found or the element was never mapped.
 */
int  GDX_CALLCONV d_gdxUMFindUEL (gdxHandle_t pgdx, const char *Uel, int *UelNr, int *UelMap)
{
  int d_s[]={3,11,4,4};
  GAMS_UNUSED(pgdx)
  GAMS_UNUSED(Uel)
  GAMS_UNUSED(UelNr)
  GAMS_UNUSED(UelMap)
  printAndReturn(gdxUMFindUEL,3,int )
}

/** Get a unique element using an unmapped index. Returns zero if the operation is not possible.
 * @param pgdx gdx object handle
 * @param UelNr Element number (unmapped) (range 1..NrElem) or -1 if not found.
 * @param Uel String for unique element. Buffer should be 64 bytes long (to store maximum of 63 characters).
 * @param UelMap User mapping for this element or -1 if element was never mapped.
 */
int  GDX_CALLCONV d_gdxUMUelGet (gdxHandle_t pgdx, int UelNr, char *Uel, int *UelMap)
{
  int d_s[]={3,3,12,4};
  GAMS_UNUSED(pgdx)
  GAMS_UNUSED(UelNr)
  GAMS_UNUSED(Uel)
  GAMS_UNUSED(UelMap)
  printAndReturn(gdxUMUelGet,3,int )
}

/** Return information about the unique elements (UELs). Always non-zero.
 * @param pgdx gdx object handle
 * @param UelCnt Total number of unique elements (UELs in GDX file plus new registered UELs).
 * @param HighMap Highest user mapping index used.
 */
int  GDX_CALLCONV d_gdxUMUelInfo (gdxHandle_t pgdx, int *UelCnt, int *HighMap)
{
  int d_s[]={3,4,4};
  GAMS_UNUSED(pgdx)
  GAMS_UNUSED(UelCnt)
  GAMS_UNUSED(HighMap)
  printAndReturn(gdxUMUelInfo,2,int )
}

/** Get the unique elements for a given dimension of a given symbol.
 * @param pgdx gdx object handle
 * @param SyNr The index number of the symbol, range 1..NrSymbols; SyNr = 0 reads universe.
 * @param DimPos The dimension to use, range 1..dim.
 * @param FilterNr Number of a previously registered filter or the value DOMC_EXPAND if no filter is wanted.
 * @param DP Callback procedure which will be called once for each available element (can be nil).
 * @param NrElem Number of unique elements found.
 * @param Uptr User pointer; will be passed to the callback procedure.
 */
int  GDX_CALLCONV d_gdxGetDomainElements (gdxHandle_t pgdx, int SyNr, int DimPos, int FilterNr, TDomainIndexProc_t DP, int *NrElem, void *Uptr)
{
  int d_s[]={3,3,3,3,59,4,1};
  GAMS_UNUSED(pgdx)
  GAMS_UNUSED(SyNr)
  GAMS_UNUSED(DimPos)
  GAMS_UNUSED(FilterNr)
  GAMS_UNUSED(DP)
  GAMS_UNUSED(NrElem)
  GAMS_UNUSED(Uptr)
  printAndReturn(gdxGetDomainElements,6,int )
}

/** Returns the dimension of the currently active symbol When reading or writing data, the dimension of the current active symbol is sometimes needed to convert arguments from strings to pchars (char ) etc.
 * @param pgdx gdx object handle
 */
int  GDX_CALLCONV d_gdxCurrentDim (gdxHandle_t pgdx)
{
  int d_s[]={3};
  GAMS_UNUSED(pgdx)
  printAndReturn(gdxCurrentDim,0,int )
}

/** Rename unique element OldName to NewName.
 * @param pgdx gdx object handle
 * @param OldName Name of an existing unique element (UEL).
 * @param NewName New name for the UEL. Must not exist in UEL table yet.
 */
int  GDX_CALLCONV d_gdxRenameUEL (gdxHandle_t pgdx, const char *OldName, const char *NewName)
{
  int d_s[]={3,11,11};
  GAMS_UNUSED(pgdx)
  GAMS_UNUSED(OldName)
  GAMS_UNUSED(NewName)
  printAndReturn(gdxRenameUEL,2,int )
}

/** Get flag to store one dimensional sets as potential domains, false (0) saves lots of space for large 1-dim sets that are no domains but can create inconsistent GDX files if used incorrectly. Returns 1 (true) iff. elements of 1-dim sets should be tracked for domain checking, 0 (false) otherwise.
 * @param pgdx gdx object handle
 */
int  GDX_CALLCONV d_gdxStoreDomainSets (gdxHandle_t pgdx)
{
  int d_s[]={15};
  GAMS_UNUSED(pgdx)
  printAndReturn(gdxStoreDomainSets,0,int )
}

/** Set flag to store one dimensional sets as potential domains, false (0) saves lots of space for large 1-dim sets that are no domains but can create inconsistent GDX files if used incorrectly. Param flag 1 (true) iff. elements of 1-dim sets should be tracked for domain checking, 0 (false) otherwise.
 * @param pgdx gdx object handle
 */
void GDX_CALLCONV d_gdxStoreDomainSetsSet (gdxHandle_t pgdx,const int x)
{
  int d_s[]={0,15};
  GAMS_UNUSED(pgdx)
  GAMS_UNUSED(x)
  printNoReturn(gdxStoreDomainSetsSet,1)
}

/** Get flag to ignore using 1-dim sets as domain when their elements are not tracked (see gdxStoreDomainSets). In case the flag is enabled this is allowing potentially unsafe writing of records to symbols with one dimensional sets as domain, when GDX has no lookup table for the elements of this set. This can happen when `gdxStoreDomainSets` was disabled by the user to save memory. For backwards compatability, this is enabled by default. Return 1 (true) iff. using a 1-dim set as domain (when store domain sets option is disabled) should be ignored. Otherwise an error is raised (ERR_NODOMAINDATA).
 * @param pgdx gdx object handle
 */
int  GDX_CALLCONV d_gdxAllowBogusDomains (gdxHandle_t pgdx)
{
  int d_s[]={15};
  GAMS_UNUSED(pgdx)
  printAndReturn(gdxAllowBogusDomains,0,int )
}

/** Set flag to ignore using 1-dim sets as domain when their elements are not tracked (see gdxStoreDomainSets). Toggle allowing potentially unsafe writing of records to symbols with one dimensional sets as domain, when GDX has no lookup table for the elements of this set. This can happen when `gdxStoreDomainSets` was disabled by the user to save memory. For backwards compatability, this is enabled by default. When the user explicitly disables it, e.g. via `gdxAllowBogusDomainsSet(false)`, then using a one dimensional set as domain will cause a GDX error (ERR_NODOMAINDATA). Param flag 1 (true) iff. using a 1-dim set as domain (when store domain sets option is disabled) should be ignored. Otherwise an error is raised (ERR_NODOMAINDATA).
 * @param pgdx gdx object handle
 */
void GDX_CALLCONV d_gdxAllowBogusDomainsSet (gdxHandle_t pgdx,const int x)
{
  int d_s[]={0,15};
  GAMS_UNUSED(pgdx)
  GAMS_UNUSED(x)
  printNoReturn(gdxAllowBogusDomainsSet,1)
}

/** Flag to map all acronym values to the GAMS "Not a Number" special value. Disabled by default.
 * @param pgdx gdx object handle
 */
int  GDX_CALLCONV d_gdxMapAcronymsToNaN (gdxHandle_t pgdx)
{
  int d_s[]={15};
  GAMS_UNUSED(pgdx)
  printAndReturn(gdxMapAcronymsToNaN,0,int )
}

/** Flag to map all acronym values to the GAMS "Not a Number" special value. Disabled by default.
 * @param pgdx gdx object handle
 */
void GDX_CALLCONV d_gdxMapAcronymsToNaNSet (gdxHandle_t pgdx,const int x)
{
  int d_s[]={0,15};
  GAMS_UNUSED(pgdx)
  GAMS_UNUSED(x)
  printNoReturn(gdxMapAcronymsToNaNSet,1)
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
# define GMS_DLL_BASENAME "gdxcclib"
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

  if (!XAPIVersion(10,errBuf,&cl))
    return 1;

  LOADIT_ERR_OK(gdxSetLoadPath, "C__gdxSetLoadPath");
  LOADIT_ERR_OK(gdxGetLoadPath, "C__gdxGetLoadPath");

#define CheckAndLoad(f,nargs,prefix) \
  if (!XCheck(#f,nargs,s,errBuf)) \
    f = &d_##f; \
  else { \
    LOADIT(f,prefix #f); \
  }
  {int s[]={3,11,11,3}; CheckAndLoad(gdxAcronymAdd,3,"C__"); }
  {int s[]={3}; CheckAndLoad(gdxAcronymCount,0,"C__"); }
  {int s[]={3,3,12,12,4}; CheckAndLoad(gdxAcronymGetInfo,4,"C__"); }
  {int s[]={3,3,4,4,4}; CheckAndLoad(gdxAcronymGetMapping,4,"C__"); }
  {int s[]={3,13}; CheckAndLoad(gdxAcronymIndex,1,"C__"); }
  {int s[]={3,13,12}; CheckAndLoad(gdxAcronymName,2,"C__"); }
  {int s[]={3,3}; CheckAndLoad(gdxAcronymNextNr,1,"C__"); }
  {int s[]={3,3,11,11,3}; CheckAndLoad(gdxAcronymSetInfo,4,"C__"); }
  {int s[]={13,3}; CheckAndLoad(gdxAcronymValue,1,"C__"); }
  {int s[]={3,11,11}; CheckAndLoad(gdxAddAlias,2,"C__"); }
  {int s[]={3,11,4}; CheckAndLoad(gdxAddSetText,2,"C__"); }
  {int s[]={3,3}; CheckAndLoad(gdxAutoConvert,1,"C__"); }
  {int s[]={3}; CheckAndLoad(gdxClose,0,"C__"); }
  {int s[]={3}; CheckAndLoad(gdxDataErrorCount,0,"C__"); }
  {int s[]={3,3,52,54}; CheckAndLoad(gdxDataErrorRecord,3,"C__"); }
  {int s[]={3,3,52,54}; CheckAndLoad(gdxDataErrorRecordX,3,"C__"); }
  {int s[]={3}; CheckAndLoad(gdxDataReadDone,0,"C__"); }
  {int s[]={3,3,51,4}; CheckAndLoad(gdxDataReadFilteredStart,3,"C__"); }
  {int s[]={3,3,52,54,4}; CheckAndLoad(gdxDataReadMap,4,"C__"); }
  {int s[]={3,3,4}; CheckAndLoad(gdxDataReadMapStart,2,"C__"); }
  {int s[]={3,52,54,4}; CheckAndLoad(gdxDataReadRaw,3,"C__"); }
  {int s[]={3,3,59,4}; CheckAndLoad(gdxDataReadRawFast,3,"C__"); }
  {int s[]={3,3,59,4,1}; CheckAndLoad(gdxDataReadRawFastEx,4,"C__"); }
  {int s[]={3,3,55,59}; CheckAndLoad(gdxDataReadRawFastFilt,3,"C__"); }
  {int s[]={3,3,4}; CheckAndLoad(gdxDataReadRawStart,2,"C__"); }
  {int s[]={3,55,4,59}; CheckAndLoad(gdxDataReadSlice,3,"C__"); }
  {int s[]={3,3,52}; CheckAndLoad(gdxDataReadSliceStart,2,"C__"); }
  {int s[]={3,56,54,4}; CheckAndLoad(gdxDataReadStr,3,"C__"); }
  {int s[]={3,3,4}; CheckAndLoad(gdxDataReadStrStart,2,"C__"); }
  {int s[]={3,51,56}; CheckAndLoad(gdxDataSliceUELS,2,"C__"); }
  {int s[]={3}; CheckAndLoad(gdxDataWriteDone,0,"C__"); }
  {int s[]={3,51,53}; CheckAndLoad(gdxDataWriteMap,2,"C__"); }
  {int s[]={3,11,11,3,3,3}; CheckAndLoad(gdxDataWriteMapStart,5,"C__"); }
  {int s[]={3,51,53}; CheckAndLoad(gdxDataWriteRaw,2,"C__"); }
  {int s[]={3,11,11,3,3,3}; CheckAndLoad(gdxDataWriteRawStart,5,"C__"); }
  {int s[]={3,55,53}; CheckAndLoad(gdxDataWriteStr,2,"C__"); }
  {int s[]={3,11,11,3,3,3}; CheckAndLoad(gdxDataWriteStrStart,5,"C__"); }
  {int s[]={3,12}; CheckAndLoad(gdxGetDLLVersion,1,"C__"); }
  {int s[]={3}; CheckAndLoad(gdxErrorCount,0,"C__"); }
  {int s[]={3,3,12}; CheckAndLoad(gdxErrorStr,2,"C__"); }
  {int s[]={3,4,4}; CheckAndLoad(gdxFileInfo,2,"C__"); }
  {int s[]={3,12,12}; CheckAndLoad(gdxFileVersion,2,"C__"); }
  {int s[]={3,3}; CheckAndLoad(gdxFilterExists,1,"C__"); }
  {int s[]={3,3}; CheckAndLoad(gdxFilterRegister,1,"C__"); }
  {int s[]={3}; CheckAndLoad(gdxFilterRegisterDone,0,"C__"); }
  {int s[]={3,3}; CheckAndLoad(gdxFilterRegisterStart,1,"C__"); }
  {int s[]={3,11,4}; CheckAndLoad(gdxFindSymbol,2,"C__"); }
  {int s[]={3,3,12,4}; CheckAndLoad(gdxGetElemText,3,"C__"); }
  {int s[]={3}; CheckAndLoad(gdxGetLastError,0,"C__"); }
  {int s[]={23}; CheckAndLoad(gdxGetMemoryUsed,0,"C__"); }
  {int s[]={3,58}; CheckAndLoad(gdxGetSpecialValues,1,"C__"); }
  {int s[]={3,3,12}; CheckAndLoad(gdxGetUEL,2,"C__"); }
  {int s[]={3,13,4}; CheckAndLoad(gdxMapValue,2,"C__"); }
  {int s[]={3,11,11,4}; CheckAndLoad(gdxOpenAppend,3,"C__"); }
  {int s[]={3,11,4}; CheckAndLoad(gdxOpenRead,2,"C__"); }
  {int s[]={3,11,3,4}; CheckAndLoad(gdxOpenReadEx,3,"C__"); }
  {int s[]={3,11,11,4}; CheckAndLoad(gdxOpenWrite,3,"C__"); }
  {int s[]={3,11,11,3,4}; CheckAndLoad(gdxOpenWriteEx,4,"C__"); }
  {int s[]={3}; CheckAndLoad(gdxResetSpecialValues,0,"C__"); }
  {int s[]={3,3}; CheckAndLoad(gdxSetHasText,1,"C__"); }
  {int s[]={3,57}; CheckAndLoad(gdxSetReadSpecialValues,1,"C__"); }
  {int s[]={3,57}; CheckAndLoad(gdxSetSpecialValues,1,"C__"); }
  {int s[]={3,3,3}; CheckAndLoad(gdxSetTextNodeNr,2,"C__"); }
  {int s[]={3,3,11}; CheckAndLoad(gdxSetTraceLevel,2,"C__"); }
  {int s[]={3,3,52}; CheckAndLoad(gdxSymbIndxMaxLength,2,"C__"); }
  {int s[]={3}; CheckAndLoad(gdxSymbMaxLength,0,"C__"); }
  {int s[]={3,3,11}; CheckAndLoad(gdxSymbolAddComment,2,"C__"); }
  {int s[]={3,3,3,12}; CheckAndLoad(gdxSymbolGetComment,3,"C__"); }
  {int s[]={3,3,52}; CheckAndLoad(gdxSymbolGetDomain,2,"C__"); }
  {int s[]={3,3,56}; CheckAndLoad(gdxSymbolGetDomainX,2,"C__"); }
  {int s[]={3,3}; CheckAndLoad(gdxSymbolDim,1,"C__"); }
  {int s[]={3,3,12,4,4}; CheckAndLoad(gdxSymbolInfo,4,"C__"); }
  {int s[]={3,3,4,4,12}; CheckAndLoad(gdxSymbolInfoX,4,"C__"); }
  {int s[]={3,55}; CheckAndLoad(gdxSymbolSetDomain,1,"C__"); }
  {int s[]={3,3,55}; CheckAndLoad(gdxSymbolSetDomainX,2,"C__"); }
  {int s[]={3,4,4}; CheckAndLoad(gdxSystemInfo,2,"C__"); }
  {int s[]={3}; CheckAndLoad(gdxUELMaxLength,0,"C__"); }
  {int s[]={3}; CheckAndLoad(gdxUELRegisterDone,0,"C__"); }
  {int s[]={3,3,11}; CheckAndLoad(gdxUELRegisterMap,2,"C__"); }
  {int s[]={3}; CheckAndLoad(gdxUELRegisterMapStart,0,"C__"); }
  {int s[]={3,11}; CheckAndLoad(gdxUELRegisterRaw,1,"C__"); }
  {int s[]={3}; CheckAndLoad(gdxUELRegisterRawStart,0,"C__"); }
  {int s[]={3,11,4}; CheckAndLoad(gdxUELRegisterStr,2,"C__"); }
  {int s[]={3}; CheckAndLoad(gdxUELRegisterStrStart,0,"C__"); }
  {int s[]={3,11,4,4}; CheckAndLoad(gdxUMFindUEL,3,"C__"); }
  {int s[]={3,3,12,4}; CheckAndLoad(gdxUMUelGet,3,"C__"); }
  {int s[]={3,4,4}; CheckAndLoad(gdxUMUelInfo,2,"C__"); }
  {int s[]={3,3,3,3,59,4,1}; CheckAndLoad(gdxGetDomainElements,6,"C__"); }
  {int s[]={3}; CheckAndLoad(gdxCurrentDim,0,"C__"); }
  {int s[]={3,11,11}; CheckAndLoad(gdxRenameUEL,2,"C__"); }
  {int s[]={15}; CheckAndLoad(gdxStoreDomainSets,0,"C__"); }
  {int s[]={0,15}; CheckAndLoad(gdxStoreDomainSetsSet,1,"C__"); }
  {int s[]={15}; CheckAndLoad(gdxAllowBogusDomains,0,"C__"); }
  {int s[]={0,15}; CheckAndLoad(gdxAllowBogusDomainsSet,1,"C__"); }
  {int s[]={15}; CheckAndLoad(gdxMapAcronymsToNaN,0,"C__"); }
  {int s[]={0,15}; CheckAndLoad(gdxMapAcronymsToNaNSet,1,"C__"); }

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
       if (NULL != gdxSetLoadPath && NULL != dllPath && '\0' != *dllPath) {
         gdxSetLoadPath(dllPath);
       }
       else {                            /* no setLoadPath call found */
         myrc |= 2;
       }
    }
    else {                              /* library load failed */
      myrc |= 1;
    }
  }
  return (myrc & 1) == 0;
}

int gdxGetReady (char *msgBuf, int msgBufSize)
{
  int rc;
  lock(libMutex);
  rc = libloader(NULL, NULL, msgBuf, msgBufSize);
  unlock(libMutex);
  return rc;
} /* gdxGetReady */

int gdxGetReadyD (const char *dirName, char *msgBuf, int msgBufSize)
{
  int rc;
  lock(libMutex);
  rc = libloader(dirName, NULL, msgBuf, msgBufSize);
  unlock(libMutex);
  return rc;
} /* gdxGetReadyD */

int gdxGetReadyL (const char *libName, char *msgBuf, int msgBufSize)
{
  char dirName[1024],fName[1024];
  int rc;
  extractFileDirFileName (libName, dirName, fName);
  lock(libMutex);
  rc = libloader(dirName, fName, msgBuf, msgBufSize);
  unlock(libMutex);
  return rc;
} /* gdxGetReadyL */

int gdxCreate (gdxHandle_t *pgdx, char *msgBuf, int msgBufSize)
{
  int gdxIsReady;

  gdxIsReady = gdxGetReady (msgBuf, msgBufSize);
  if (! gdxIsReady) {
    return 0;
  }
  assert(XCreate);
  XCreate(pgdx);
  if(pgdx == NULL)
  { strcpy(msgBuf,"Error while creating object"); return 0; }
  lock(objMutex);
  objectCount++;
  unlock(objMutex);
  return 1;                     /* return true on successful library load */
} /* gdxCreate */

int gdxCreateD (gdxHandle_t *pgdx, const char *dirName,
                char *msgBuf, int msgBufSize)
{
  int gdxIsReady;

  gdxIsReady = gdxGetReadyD (dirName, msgBuf, msgBufSize);
  if (! gdxIsReady) {
    return 0;
  }
  assert(XCreate);
  XCreate(pgdx);
  if(pgdx == NULL)
  { strcpy(msgBuf,"Error while creating object"); return 0; }
  lock(objMutex);
  objectCount++;
  unlock(objMutex);
  return 1;                     /* return true on successful library load */
} /* gdxCreateD */

int gdxCreateDD (gdxHandle_t *pgdx, const char *dirName,
                char *msgBuf, int msgBufSize)
{
  int gdxIsReady;
  gdxIsReady = gdxGetReadyD (dirName, msgBuf, msgBufSize);
  if (! gdxIsReady) {
    return 0;
  }
  assert(XCreateD);
  XCreateD(pgdx, dirName);
  if(pgdx == NULL)
  { strcpy(msgBuf,"Error while creating object"); return 0; }
  lock(objMutex);
  objectCount++;
  unlock(objMutex);
  return 1;                     /* return true on successful library load */
} /* gdxCreateD */


int gdxCreateL (gdxHandle_t *pgdx, const char *libName,
                char *msgBuf, int msgBufSize)
{
  int gdxIsReady;

  gdxIsReady = gdxGetReadyL (libName, msgBuf, msgBufSize);
  if (! gdxIsReady) {
    return 0;
  }
  assert(XCreate);
  XCreate(pgdx);
  if(pgdx == NULL)
  { strcpy(msgBuf,"Error while creating object"); return 0; }
  lock(objMutex);
  objectCount++;
  unlock(objMutex);
  return 1;                     /* return true on successful library load */
} /* gdxCreateL */

int gdxFree   (gdxHandle_t *pgdx)
{
  assert(XFree);
  XFree(pgdx); pgdx = NULL;
  lock(objMutex);
  objectCount--;
  unlock(objMutex);
  return 1;
} /* gdxFree */

int gdxLibraryLoaded(void)
{
  int rc;
  lock(libMutex);
  rc = isLoaded;
  unlock(libMutex);
  return rc;
} /* gdxLibraryLoaded */

int gdxLibraryUnload(void)
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
} /* gdxLibraryUnload */

int  gdxCorrectLibraryVersion(char *msgBuf, int msgBufLen)
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

  XAPIVersion(GDXAPIVERSION,localBuf,&cl);
  strncpy(msgBuf, localBuf, msgBufLen);

  if (1 == cl)
    return 1;
  else
    return 0;
}

int gdxGetScreenIndicator(void)
{
  return ScreenIndicator;
}

void gdxSetScreenIndicator(int scrind)
{
  ScreenIndicator = scrind ? 1 : 0;
}

int gdxGetExceptionIndicator(void)
{
   return ExceptionIndicator;
}

void gdxSetExceptionIndicator(int excind)
{
  ExceptionIndicator = excind ? 1 : 0;
}

int gdxGetExitIndicator(void)
{
  return ExitIndicator;
}

void gdxSetExitIndicator(int extind)
{
  ExitIndicator = extind ? 1 : 0;
}

gdxErrorCallback_t gdxGetErrorCallback(void)
{
  return ErrorCallBack;
}

void gdxSetErrorCallback(gdxErrorCallback_t func)
{
  lock(exceptMutex);
  ErrorCallBack = func;
  unlock(exceptMutex);
}

int gdxGetAPIErrorCount(void)
{
  return APIErrorCount;
}

void gdxSetAPIErrorCount(int ecnt)
{
  APIErrorCount = ecnt;
}

void gdxErrorHandling(const char *msg)
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
