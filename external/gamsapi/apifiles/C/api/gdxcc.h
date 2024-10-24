/* gdxcc.h
 * Header file for C-style interface to the GDX library
 * 
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


#if ! defined(_GDXCC_H_)
#     define  _GDXCC_H_

#define GDXAPIVERSION 11
#if defined(_WIN32) && defined(__GNUC__)
# include <stdio.h>
#endif


#include "gclgms.h"

#if defined(_WIN32)
# define GDX_CALLCONV __stdcall
#else
# define GDX_CALLCONV
#endif
#if defined(_WIN32)
typedef __int64 INT64;
#else
typedef signed long int INT64;
#endif

#if defined(__cplusplus)
extern "C" {
#endif

struct gdxRec;
typedef struct gdxRec *gdxHandle_t;

typedef int (*gdxErrorCallback_t) (int ErrCount, const char *msg);

/* headers for "wrapper" routines implemented in C */
/** gdxGetReady: load library
 *  @return false on failure to load library, true on success 
 */
int gdxGetReady  (char *msgBuf, int msgBufLen);

/** gdxGetReadyD: load library from the speicified directory
 * @return false on failure to load library, true on success 
 */
int gdxGetReadyD (const char *dirName, char *msgBuf, int msgBufLen);

/** gdxGetReadyL: load library from the specified library name
 *  @return false on failure to load library, true on success 
 */
int gdxGetReadyL (const char *libName, char *msgBuf, int msgBufLen);

/** gdxCreate: load library and create gdx object handle 
 *  @return false on failure to load library, true on success 
 */
int gdxCreate    (gdxHandle_t *pgdx, char *msgBuf, int msgBufLen);

/** gdxCreateD: load library from the specified directory and create gdx object handle
 * @return false on failure to load library, true on success 
 */
int gdxCreateD   (gdxHandle_t *pgdx, const char *dirName, char *msgBuf, int msgBufLen);

/** gdxCreateDD: load library from the specified directory and create gdx object handle
 * @return false on failure to load library, true on success 
 */
int gdxCreateDD  (gdxHandle_t *pgdx, const char *dirName, char *msgBuf, int msgBufLen);

/** gdxCreate: load library from the specified library name and create gdx object handle
 * @return false on failure to load library, true on success 
 */
int gdxCreateL   (gdxHandle_t *pgdx, const char *libName, char *msgBuf, int msgBufLen);

/** gdxFree: free gdx object handle 
 * @return false on failure, true on success 
 */
int gdxFree      (gdxHandle_t *pgdx);

/** Check if library has been loaded
 * @return false on failure, true on success 
 */
int gdxLibraryLoaded(void);

/** Check if library has been unloaded
 * @return false on failure, true on success 
 */
int gdxLibraryUnload(void);

/** Check if API and library have the same version, Library needs to be initialized before calling this.
 * @return true  (1) on success,
 *         false (0) on failure.
 */
int  gdxCorrectLibraryVersion(char *msgBuf, int msgBufLen);

int  gdxGetScreenIndicator   (void);
void gdxSetScreenIndicator   (int scrind);
int  gdxGetExceptionIndicator(void);
void gdxSetExceptionIndicator(int excind);
int  gdxGetExitIndicator     (void);
void gdxSetExitIndicator     (int extind);
gdxErrorCallback_t gdxGetErrorCallback(void);
void gdxSetErrorCallback(gdxErrorCallback_t func);
int  gdxGetAPIErrorCount     (void);
void gdxSetAPIErrorCount     (int ecnt);

void gdxErrorHandling(const char *msg);
void gdxInitMutexes(void);
void gdxFiniMutexes(void);

#if defined(GDX_MAIN)    /* we must define some things only once */
# define GDX_FUNCPTR(NAME)  NAME##_t NAME = NULL
#else
# define GDX_FUNCPTR(NAME)  extern NAME##_t NAME
#endif

/* function typedefs and pointer definitions */
typedef void (GDX_CALLCONV *TDataStoreProc_t) (const int Indx[], const double Vals[]);
typedef int (GDX_CALLCONV *TDataStoreExProc_t) (const int Indx[], const double Vals[], int DimFrst, void *Uptr);
typedef int (GDX_CALLCONV *TDataStoreFiltProc_t) (const int Indx[], const double Vals[], void *Uptr);
typedef void (GDX_CALLCONV *TDomainIndexProc_t) (int RawIndex, int MappedIndex, void *Uptr);

/* Prototypes for Dummy Functions */
int  GDX_CALLCONV d_gdxAcronymAdd (gdxHandle_t pgdx, const char *AName, const char *Txt, int AIndx);
int  GDX_CALLCONV d_gdxAcronymCount (gdxHandle_t pgdx);
int  GDX_CALLCONV d_gdxAcronymGetInfo (gdxHandle_t pgdx, int N, char *AName, char *Txt, int *AIndx);
int  GDX_CALLCONV d_gdxAcronymGetMapping (gdxHandle_t pgdx, int N, int *orgIndx, int *newIndx, int *autoIndex);
int  GDX_CALLCONV d_gdxAcronymIndex (gdxHandle_t pgdx, double V);
int  GDX_CALLCONV d_gdxAcronymName (gdxHandle_t pgdx, double V, char *AName);
int  GDX_CALLCONV d_gdxAcronymNextNr (gdxHandle_t pgdx, int NV);
int  GDX_CALLCONV d_gdxAcronymSetInfo (gdxHandle_t pgdx, int N, const char *AName, const char *Txt, int AIndx);
double  GDX_CALLCONV d_gdxAcronymValue (gdxHandle_t pgdx, int AIndx);
int  GDX_CALLCONV d_gdxAddAlias (gdxHandle_t pgdx, const char *Id1, const char *Id2);
int  GDX_CALLCONV d_gdxAddSetText (gdxHandle_t pgdx, const char *Txt, int *TxtNr);
int  GDX_CALLCONV d_gdxAutoConvert (gdxHandle_t pgdx, int NV);
int  GDX_CALLCONV d_gdxClose (gdxHandle_t pgdx);
int  GDX_CALLCONV d_gdxDataErrorCount (gdxHandle_t pgdx);
int  GDX_CALLCONV d_gdxDataErrorRecord (gdxHandle_t pgdx, int RecNr, int KeyInt[], double Values[]);
int  GDX_CALLCONV d_gdxDataErrorRecordX (gdxHandle_t pgdx, int RecNr, int KeyInt[], double Values[]);
int  GDX_CALLCONV d_gdxDataReadDone (gdxHandle_t pgdx);
int  GDX_CALLCONV d_gdxDataReadFilteredStart (gdxHandle_t pgdx, int SyNr, const int FilterAction[], int *NrRecs);
int  GDX_CALLCONV d_gdxDataReadMap (gdxHandle_t pgdx, int RecNr, int KeyInt[], double Values[], int *DimFrst);
int  GDX_CALLCONV d_gdxDataReadMapStart (gdxHandle_t pgdx, int SyNr, int *NrRecs);
int  GDX_CALLCONV d_gdxDataReadRaw (gdxHandle_t pgdx, int KeyInt[], double Values[], int *DimFrst);
int  GDX_CALLCONV d_gdxDataReadRawFast (gdxHandle_t pgdx, int SyNr, TDataStoreProc_t DP, int *NrRecs);
int  GDX_CALLCONV d_gdxDataReadRawFastEx (gdxHandle_t pgdx, int SyNr, TDataStoreExProc_t DP, int *NrRecs, void *Uptr);
int  GDX_CALLCONV d_gdxDataReadRawFastFilt (gdxHandle_t pgdx, int SyNr, const char *UelFilterStr[], TDataStoreFiltProc_t DP);
int  GDX_CALLCONV d_gdxDataReadRawStart (gdxHandle_t pgdx, int SyNr, int *NrRecs);
int  GDX_CALLCONV d_gdxDataReadSlice (gdxHandle_t pgdx, const char *UelFilterStr[], int *Dimen, TDataStoreProc_t DP);
int  GDX_CALLCONV d_gdxDataReadSliceStart (gdxHandle_t pgdx, int SyNr, int ElemCounts[]);
int  GDX_CALLCONV d_gdxDataReadStr (gdxHandle_t pgdx, char *KeyStr[], double Values[], int *DimFrst);
int  GDX_CALLCONV d_gdxDataReadStrStart (gdxHandle_t pgdx, int SyNr, int *NrRecs);
int  GDX_CALLCONV d_gdxDataSliceUELS (gdxHandle_t pgdx, const int SliceKeyInt[], char *KeyStr[]);
int  GDX_CALLCONV d_gdxDataWriteDone (gdxHandle_t pgdx);
int  GDX_CALLCONV d_gdxDataWriteMap (gdxHandle_t pgdx, const int KeyInt[], const double Values[]);
int  GDX_CALLCONV d_gdxDataWriteMapStart (gdxHandle_t pgdx, const char *SyId, const char *ExplTxt, int Dimen, int Typ, int UserInfo);
int  GDX_CALLCONV d_gdxDataWriteRaw (gdxHandle_t pgdx, const int KeyInt[], const double Values[]);
int  GDX_CALLCONV d_gdxDataWriteRawStart (gdxHandle_t pgdx, const char *SyId, const char *ExplTxt, int Dimen, int Typ, int UserInfo);
int  GDX_CALLCONV d_gdxDataWriteRawStartKeyBounds (gdxHandle_t pgdx, const char *SyId, const char *ExplTxt, int Dimen, int Typ, int UserInfo, const int MinUELIndices[], const int MaxUELIndices[]);
int  GDX_CALLCONV d_gdxDataWriteStr (gdxHandle_t pgdx, const char *KeyStr[], const double Values[]);
int  GDX_CALLCONV d_gdxDataWriteStrStart (gdxHandle_t pgdx, const char *SyId, const char *ExplTxt, int Dimen, int Typ, int UserInfo);
int  GDX_CALLCONV d_gdxGetDLLVersion (gdxHandle_t pgdx, char *V);
int  GDX_CALLCONV d_gdxErrorCount (gdxHandle_t pgdx);
int  GDX_CALLCONV d_gdxErrorStr (gdxHandle_t pgdx, int ErrNr, char *ErrMsg);
int  GDX_CALLCONV d_gdxFileInfo (gdxHandle_t pgdx, int *FileVer, int *ComprLev);
int  GDX_CALLCONV d_gdxFileVersion (gdxHandle_t pgdx, char *FileStr, char *ProduceStr);
int  GDX_CALLCONV d_gdxFilterExists (gdxHandle_t pgdx, int FilterNr);
int  GDX_CALLCONV d_gdxFilterRegister (gdxHandle_t pgdx, int UelMap);
int  GDX_CALLCONV d_gdxFilterRegisterDone (gdxHandle_t pgdx);
int  GDX_CALLCONV d_gdxFilterRegisterStart (gdxHandle_t pgdx, int FilterNr);
int  GDX_CALLCONV d_gdxFindSymbol (gdxHandle_t pgdx, const char *SyId, int *SyNr);
int  GDX_CALLCONV d_gdxGetElemText (gdxHandle_t pgdx, int TxtNr, char *Txt, int *Node);
int  GDX_CALLCONV d_gdxGetLastError (gdxHandle_t pgdx);
INT64  GDX_CALLCONV d_gdxGetMemoryUsed (gdxHandle_t pgdx);
int  GDX_CALLCONV d_gdxGetSpecialValues (gdxHandle_t pgdx, double AVals[]);
int  GDX_CALLCONV d_gdxGetUEL (gdxHandle_t pgdx, int UelNr, char *Uel);
int  GDX_CALLCONV d_gdxMapValue (gdxHandle_t pgdx, double D, int *sv);
int  GDX_CALLCONV d_gdxOpenAppend (gdxHandle_t pgdx, const char *FileName, const char *Producer, int *ErrNr);
int  GDX_CALLCONV d_gdxOpenRead (gdxHandle_t pgdx, const char *FileName, int *ErrNr);
int  GDX_CALLCONV d_gdxOpenReadEx (gdxHandle_t pgdx, const char *FileName, int ReadMode, int *ErrNr);
int  GDX_CALLCONV d_gdxOpenWrite (gdxHandle_t pgdx, const char *FileName, const char *Producer, int *ErrNr);
int  GDX_CALLCONV d_gdxOpenWriteEx (gdxHandle_t pgdx, const char *FileName, const char *Producer, int Compr, int *ErrNr);
int  GDX_CALLCONV d_gdxResetSpecialValues (gdxHandle_t pgdx);
int  GDX_CALLCONV d_gdxSetHasText (gdxHandle_t pgdx, int SyNr);
int  GDX_CALLCONV d_gdxSetReadSpecialValues (gdxHandle_t pgdx, const double AVals[]);
int  GDX_CALLCONV d_gdxSetSpecialValues (gdxHandle_t pgdx, const double AVals[]);
int  GDX_CALLCONV d_gdxSetTextNodeNr (gdxHandle_t pgdx, int TxtNr, int Node);
int  GDX_CALLCONV d_gdxSetTraceLevel (gdxHandle_t pgdx, int N, const char *s);
int  GDX_CALLCONV d_gdxSymbIndxMaxLength (gdxHandle_t pgdx, int SyNr, int LengthInfo[]);
int  GDX_CALLCONV d_gdxSymbMaxLength (gdxHandle_t pgdx);
int  GDX_CALLCONV d_gdxSymbolAddComment (gdxHandle_t pgdx, int SyNr, const char *Txt);
int  GDX_CALLCONV d_gdxSymbolGetComment (gdxHandle_t pgdx, int SyNr, int N, char *Txt);
int  GDX_CALLCONV d_gdxSymbolGetDomain (gdxHandle_t pgdx, int SyNr, int DomainSyNrs[]);
int  GDX_CALLCONV d_gdxSymbolGetDomainX (gdxHandle_t pgdx, int SyNr, char *DomainIDs[]);
int  GDX_CALLCONV d_gdxSymbolDim (gdxHandle_t pgdx, int SyNr);
int  GDX_CALLCONV d_gdxSymbolInfo (gdxHandle_t pgdx, int SyNr, char *SyId, int *Dimen, int *Typ);
int  GDX_CALLCONV d_gdxSymbolInfoX (gdxHandle_t pgdx, int SyNr, int *RecCnt, int *UserInfo, char *ExplTxt);
int  GDX_CALLCONV d_gdxSymbolSetDomain (gdxHandle_t pgdx, const char *DomainIDs[]);
int  GDX_CALLCONV d_gdxSymbolSetDomainX (gdxHandle_t pgdx, int SyNr, const char *DomainIDs[]);
int  GDX_CALLCONV d_gdxSystemInfo (gdxHandle_t pgdx, int *SyCnt, int *UelCnt);
int  GDX_CALLCONV d_gdxUELMaxLength (gdxHandle_t pgdx);
int  GDX_CALLCONV d_gdxUELRegisterDone (gdxHandle_t pgdx);
int  GDX_CALLCONV d_gdxUELRegisterMap (gdxHandle_t pgdx, int UMap, const char *Uel);
int  GDX_CALLCONV d_gdxUELRegisterMapStart (gdxHandle_t pgdx);
int  GDX_CALLCONV d_gdxUELRegisterRaw (gdxHandle_t pgdx, const char *Uel);
int  GDX_CALLCONV d_gdxUELRegisterRawStart (gdxHandle_t pgdx);
int  GDX_CALLCONV d_gdxUELRegisterStr (gdxHandle_t pgdx, const char *Uel, int *UelNr);
int  GDX_CALLCONV d_gdxUELRegisterStrStart (gdxHandle_t pgdx);
int  GDX_CALLCONV d_gdxUMFindUEL (gdxHandle_t pgdx, const char *Uel, int *UelNr, int *UelMap);
int  GDX_CALLCONV d_gdxUMUelGet (gdxHandle_t pgdx, int UelNr, char *Uel, int *UelMap);
int  GDX_CALLCONV d_gdxUMUelInfo (gdxHandle_t pgdx, int *UelCnt, int *HighMap);
int  GDX_CALLCONV d_gdxGetDomainElements (gdxHandle_t pgdx, int SyNr, int DimPos, int FilterNr, TDomainIndexProc_t DP, int *NrElem, void *Uptr);
int  GDX_CALLCONV d_gdxCurrentDim (gdxHandle_t pgdx);
int  GDX_CALLCONV d_gdxRenameUEL (gdxHandle_t pgdx, const char *OldName, const char *NewName);
int  GDX_CALLCONV d_gdxStoreDomainSets (gdxHandle_t pgdx);
void GDX_CALLCONV d_gdxStoreDomainSetsSet (gdxHandle_t pgdx, const int x);
int  GDX_CALLCONV d_gdxAllowBogusDomains (gdxHandle_t pgdx);
void GDX_CALLCONV d_gdxAllowBogusDomainsSet (gdxHandle_t pgdx, const int x);
int  GDX_CALLCONV d_gdxMapAcronymsToNaN (gdxHandle_t pgdx);
void GDX_CALLCONV d_gdxMapAcronymsToNaNSet (gdxHandle_t pgdx, const int x);


typedef int  (GDX_CALLCONV *gdxAcronymAdd_t) (gdxHandle_t pgdx, const char *AName, const char *Txt, int AIndx);
/** Add a new acronym entry. This can be used to add entries before data is written. Returns negative value (<0) if the entry is not added.
 *
 * @param pgdx gdx object handle
 * @param AName Name of the acronym (up to 63 characters) The first character of a symbol must be a letter. Following symbol characters may be letters, digits, and underscores. Symbol names must be new and unique.
 * @param Txt Explanatory text of the acronym (up to 255 characters, mixed quotes will be unified to first occurring quote character).
 * @param AIndx Index value of the acronym.
 * @return <ul><li>0 If the entry is not added because of a duplicate name using the same value fo the index.</li> <li>-1 If the entry is not added because of a duplicate name using a different value for the index.</li> <li>Otherwise the index into the acronym table (1..gdxAcronymCount).</li></ul>
 */
GDX_FUNCPTR(gdxAcronymAdd);

typedef int  (GDX_CALLCONV *gdxAcronymCount_t) (gdxHandle_t pgdx);
/** Number of entries in the acronym table.
 *
 * @param pgdx gdx object handle
 * @return The number of entries in the acronym table.
 */
GDX_FUNCPTR(gdxAcronymCount);

typedef int  (GDX_CALLCONV *gdxAcronymGetInfo_t) (gdxHandle_t pgdx, int N, char *AName, char *Txt, int *AIndx);
/** Retrieve acronym information from the acronym table. Non-zero if the index into the acronym table is valid.
 *
 * @param pgdx gdx object handle
 * @param N Index into acronym table (range 1..AcronymCount).
 * @param AName Name of the acronym (up to 63 characters).
 * @param Txt Explanatory text of the acronym (up to 255 characters, mixed quote chars will be unified to first occurring quote).
 * @param AIndx Index value of the acronym.
 * @return Non-zero if the index into the acronym table is valid; false otherwise.
 */
GDX_FUNCPTR(gdxAcronymGetInfo);

typedef int  (GDX_CALLCONV *gdxAcronymGetMapping_t) (gdxHandle_t pgdx, int N, int *orgIndx, int *newIndx, int *autoIndex);
/** Get information how acronym values are remapped. When reading GDX data, we need to map indices for acronyms used in the GDX file to indices used by the reading program. Non-zero if the index into the acronym table is valid.
 *
 * @param pgdx gdx object handle
 * @param N Index into acronym table; range from 1 to AcronymCount.
 * @param orgIndx The Index used in the GDX file.
 * @param newIndx The Index returned when reading GDX data.
 * @param autoIndex Non-zero if the newIndx was generated using the value of NextAutoAcronym.
 * @return Non-zero if the index into the acronym table is valid; false otherwise.
 */
GDX_FUNCPTR(gdxAcronymGetMapping);

typedef int  (GDX_CALLCONV *gdxAcronymIndex_t) (gdxHandle_t pgdx, double V);
/** Get index value of an acronym. Returns zero if V does not represent an acronym.
 *
 * @param pgdx gdx object handle
 * @param V Input value possibly representing an acronym/Version string after return (gdxGetDLLVersion).
 * @return Index of acronym value V; zero if V does not represent an acronym.
 */
GDX_FUNCPTR(gdxAcronymIndex);

typedef int  (GDX_CALLCONV *gdxAcronymName_t) (gdxHandle_t pgdx, double V, char *AName);
/** Find the name of an acronym value. Non-zero if a name for the acronym is defined. An unnamed acronym value will return a string of the form UnknownAcronymNNN, were NNN is the index of the acronym.
 *
 * @param pgdx gdx object handle
 * @param V Input value possibly containing an acronym/Version string after return (gdxGetDLLVersion).
 * @param AName Name of acronym value or the empty string (can be up to 63 characters).
 * @return Return non-zero if a name for the acronym is defined. Return zero if V does not represent an acronym value or a name is not defined. An unnamed acronym value will return a string of the form UnknownAcronymNNN; were NNN is the index of the acronym.
 */
GDX_FUNCPTR(gdxAcronymName);

typedef int  (GDX_CALLCONV *gdxAcronymNextNr_t) (gdxHandle_t pgdx, int NV);
/** Returns the value of the NextAutoAcronym variable and sets the variable to nv. <ul><li>When we read from a GDX file and encounter an acronym that was not defined, we need to assign a new index for that acronym. The variable NextAutoAcronym is used for this purpose and is incremented for each new undefined acronym.</li> <li>When NextAutoAcronym has a value of zero, the default, the value is ignored and the original index as stored in the GDX file is used for the index.</li></ul>
 *
 * @param pgdx gdx object handle
 * @param NV New value for NextAutoAcronym; a value of less than zero is ignored.
 * @return Previous value of NextAutoAcronym.
 */
GDX_FUNCPTR(gdxAcronymNextNr);

typedef int  (GDX_CALLCONV *gdxAcronymSetInfo_t) (gdxHandle_t pgdx, int N, const char *AName, const char *Txt, int AIndx);
/** Modify acronym information in the acronym table <ul><li>When writing a GDX file, this function is used to provide the name of an acronym; in this case the Indx parameter must match.</li> <li>When reading a GDX file, this function is used to provide the acronym index, and the AName parameter must match.</li></ul>
 *
 * @param pgdx gdx object handle
 * @param N Index into acronym table (range 1..AcronymCount).
 * @param AName Name of the acronym (up to 63 characters). The first character of a symbol must be a letter. Following symbol characters may be letters, digits, and underscores. Symbol names must be new and unique.
 * @param Txt Explanatory text of the acronym (up to 255 characters, mixed quote chars will be unified to first occurring quote).
 * @param AIndx Index value of the acronym.
 * @return Non-zero if the index into the acronym table is valid; false otherwise.
 */
GDX_FUNCPTR(gdxAcronymSetInfo);

typedef double  (GDX_CALLCONV *gdxAcronymValue_t) (gdxHandle_t pgdx, int AIndx);
/** Create an acronym value based on the index (AIndx should be greater than 0). Returns the calculated acronym value (zero if AIndx is <0).
 *
 * @param pgdx gdx object handle
 * @param AIndx Index value; should be greater than zero.
 * @return The calculated acronym value; zero if Indx is not positive.
 */
GDX_FUNCPTR(gdxAcronymValue);

typedef int  (GDX_CALLCONV *gdxAddAlias_t) (gdxHandle_t pgdx, const char *Id1, const char *Id2);
/** Add an alias for a set to the symbol table. One of the two identifiers has to be a known set, an alias or "*" (universe); the other identifier is used as the new alias for the given set. The function gdxSymbolInfoX can be used to retrieve the set or alias associated with the identifier; it is returned as the UserInfo parameter.
 *
 * @param pgdx gdx object handle
 * @param Id1 First set identifier.
 * @param Id2 Second set identifier.
 * @return Non-zero if the operation is possible, zero otherwise.
 */
GDX_FUNCPTR(gdxAddAlias);

typedef int  (GDX_CALLCONV *gdxAddSetText_t) (gdxHandle_t pgdx, const char *Txt, int *TxtNr);
/** Register a string in the string table Register a string in the string table and return the integer number assigned to this string. The integer value can be used to set the associated text of a set element. The string must follow the GAMS syntax rules for explanatory text e.g. not longer than 255 characters.
 *
 * @param pgdx gdx object handle
 * @param Txt The string to be registered (must not exceed 255 characters).
 * @param TxtNr The index number assigned to this string (output argument).
 * @return Non-zero if the operation is possible, zero otherwise.
 */
GDX_FUNCPTR(gdxAddSetText);

typedef int  (GDX_CALLCONV *gdxAutoConvert_t) (gdxHandle_t pgdx, int NV);
/** Returns the value of the AutoConvert variable and sets the variable to nv. <p>When we close a new GDX file, we look at the value of AutoConvert; if AutoConvert is non-zero, we look at the GDXCOMPRESS and GDXCONVERT environment variables to determine if conversion to an older file format is desired. We needed this logic so gdxcopy.exe can disable automatic file conversion.</p>
 *
 * @param pgdx gdx object handle
 * @param NV New value for AutoConvert.
 * @return Previous value of AutoConvert.
 */
GDX_FUNCPTR(gdxAutoConvert);

typedef int  (GDX_CALLCONV *gdxClose_t) (gdxHandle_t pgdx);
/** Close a GDX file that was previously opened for reading or writing. Before the file is closed, any pending write operations will be finished. This does not free the GDX in-memory object. This method will automatically be called when the GDX object lifetime ends (e.g. being out of scope).
 *
 * @param pgdx gdx object handle
 * @return Returns the value of gdxGetLastError.
 */
GDX_FUNCPTR(gdxClose);

typedef int  (GDX_CALLCONV *gdxDataErrorCount_t) (gdxHandle_t pgdx);
/** Query the number of error records.
 *
 * @param pgdx gdx object handle
 * @return The number of error records available.
 */
GDX_FUNCPTR(gdxDataErrorCount);

typedef int  (GDX_CALLCONV *gdxDataErrorRecord_t) (gdxHandle_t pgdx, int RecNr, int KeyInt[], double Values[]);
/** Retrieve an error record. Non-zero if the record number is valid.
 *
 * @param pgdx gdx object handle
 * @param RecNr The number of the record to be retrieved (range = 1..NrErrorRecords); this argument is ignored in gdxDataReadMap
 * @param KeyInt Index for the record (array of UEL numbers for each dimension).
 * @param Values Values for the record (level, marginal, lower-, upper-bound, scale).
 * @return Non-zero if the record number is valid, zero otherwise.
 */
GDX_FUNCPTR(gdxDataErrorRecord);

typedef int  (GDX_CALLCONV *gdxDataErrorRecordX_t) (gdxHandle_t pgdx, int RecNr, int KeyInt[], double Values[]);
/** Retrieve an error record. Non-zero if the record number is valid.
 *
 * @param pgdx gdx object handle
 * @param RecNr The number of the record to be retrieved, (range 1..NrErrorRecords); this argument is ignored in gdxDataReadMap
 * @param KeyInt Index for the record, negative uel indicates domain violation for filtered/strict read.
 * @param Values Values for the record (level, marginal, lower-, upper-bound, scale).
 * @return Non-zero if the record number is valid, zero otherwise.
 */
GDX_FUNCPTR(gdxDataErrorRecordX);

typedef int  (GDX_CALLCONV *gdxDataReadDone_t) (gdxHandle_t pgdx);
/** Finish reading of a symbol in any mode (raw, mapped, string). . Returns zero if the operation is not possible.
 *
 * @param pgdx gdx object handle
 * @return Non-zero if the operation is possible, zero otherwise.
 */
GDX_FUNCPTR(gdxDataReadDone);

typedef int  (GDX_CALLCONV *gdxDataReadFilteredStart_t) (gdxHandle_t pgdx, int SyNr, const int FilterAction[], int *NrRecs);
/** Initialize the reading of a symbol in filtered mode. Returns zero if the operation is not possible.
 *
 * @param pgdx gdx object handle
 * @param SyNr The index number of the symbol, range 0..NrSymbols; SyNr = 0 reads universe.
 * @param FilterAction Array of filter actions for each index position.
 * @param NrRecs The maximum number of records available for reading. The actual number of records may be less when a filter is applied to the records read.
 * @return Non-zero if the operation is possible, zero otherwise.
 */
GDX_FUNCPTR(gdxDataReadFilteredStart);

typedef int  (GDX_CALLCONV *gdxDataReadMap_t) (gdxHandle_t pgdx, int RecNr, int KeyInt[], double Values[], int *DimFrst);
/** Read the next record in mapped mode. Returns zero if the operation is not possible.
 *
 * @param pgdx gdx object handle
 * @param RecNr Ignored (left in for backward compatibility).
 * @param KeyInt The index of the record.
 * @param Values The data of the record.
 * @param DimFrst The first index position in KeyInt that changed.
 * @return Non-zero if the operation is possible, zero otherwise.
 */
GDX_FUNCPTR(gdxDataReadMap);

typedef int  (GDX_CALLCONV *gdxDataReadMapStart_t) (gdxHandle_t pgdx, int SyNr, int *NrRecs);
/** Initialize the reading of a symbol in mapped mode. Returns zero if the operation is not possible.
 *
 * @param pgdx gdx object handle
 * @param SyNr The index number of the symbol, range 0..NrSymbols; SyNr = 0 reads universe.
 * @param NrRecs The maximum number of records available for reading. The actual number of records may be less when a filter is applied to the records read.
 * @return Non-zero if the operation is possible, zero otherwise.
 */
GDX_FUNCPTR(gdxDataReadMapStart);

typedef int  (GDX_CALLCONV *gdxDataReadRaw_t) (gdxHandle_t pgdx, int KeyInt[], double Values[], int *DimFrst);
/** Read the next record in raw mode. Returns zero if the operation is not possible.
 *
 * @param pgdx gdx object handle
 * @param KeyInt The index of the record in UEL numbers for each dimension.
 * @param Values The data of the record (level, marginal, lower-, upper-bound, scale).
 * @param DimFrst The first index position in KeyInt that changed.
 * @return Non-zero if the operation is possible, zero otherwise (e.g. no records left).
 */
GDX_FUNCPTR(gdxDataReadRaw);

typedef int  (GDX_CALLCONV *gdxDataReadRawFast_t) (gdxHandle_t pgdx, int SyNr, TDataStoreProc_t DP, int *NrRecs);
/** Read a symbol in Raw mode using a callback procedure. Returns zero if the operation is not possible.
 *
 * @param pgdx gdx object handle
 * @param SyNr The index number of the symbol (range 0..NrSymbols); SyNr = 0 reads universe.
 * @param DP Procedure that will be called for each data record. This procedure (return type=void) should have the following signature: <ul><li>UEL index number keys (const int ),</li> <li>values (level, marginal, lower-, upper-bound, scale) (const double )</li></ul>
 * @param NrRecs The maximum number of records available for reading. The actual number of records may be less when a filter is applied to the records read.
 * @return Non-zero if the operation is possible, zero otherwise.
 */
GDX_FUNCPTR(gdxDataReadRawFast);

typedef int  (GDX_CALLCONV *gdxDataReadRawFastEx_t) (gdxHandle_t pgdx, int SyNr, TDataStoreExProc_t DP, int *NrRecs, void *Uptr);
/** Read a symbol in Raw mode using a callback procedure. Returns zero if the operation is not possible.
 *
 * @param pgdx gdx object handle
 * @param SyNr The index number of the symbol (range 0..NrSymbols); SyNr = 0 reads universe.
 * @param DP Procedure that will be called for each data record. This function (return type=integer) should return whether reading continues (=0 for stop, >=1 otherwise) and should have the following signature: <ul><li>UEL index number keys (const int ),</li> <li>values (level, marginal, lower-, upper-bound, scale) (const double ),</li> <li>dimension of first change (int),</li> <li>pointer to custom data (void )</li> </ul>
 * @param NrRecs The number of records available for reading.
 * @param Uptr Pointer to user memory that will be passed back with the callback.
 * @return Non-zero if the operation is possible, zero otherwise.
 */
GDX_FUNCPTR(gdxDataReadRawFastEx);

typedef int  (GDX_CALLCONV *gdxDataReadRawFastFilt_t) (gdxHandle_t pgdx, int SyNr, const char *UelFilterStr[], TDataStoreFiltProc_t DP);
/** Read a symbol in Raw mode while applying a filter using a callback procedure. Returns zero if the operation is not possible.
 *
 * @param pgdx gdx object handle
 * @param SyNr The index number of the symbol, range from 0 to NrSymbols; SyNr = 0 reads universe.
 * @param UelFilterStr Each index can be fixed by setting the string for the unique element. Set an index position to the empty string in order not to fix that position. If the string is not-empty it should match an UEL name from the UEL table.
 * @param DP Callback procedure which will be called for each available data item. This procedure (return type=void) should have the following signature: <ul><li>UEL index number keys (const int ),</li> <li>values (level, marginal, lower-, upper-bound, scale) (const double ),</li> <li>pointer to custom data (void ).</li></ul>
 * @return Non-zero if the operation is possible, zero otherwise.
 */
GDX_FUNCPTR(gdxDataReadRawFastFilt);

typedef int  (GDX_CALLCONV *gdxDataReadRawStart_t) (gdxHandle_t pgdx, int SyNr, int *NrRecs);
/** Initialize the reading of a symbol in raw mode. Returns zero if the operation is not possible.
 *
 * @param pgdx gdx object handle
 * @param SyNr The index number of the symbol, range 0..NrSymbols; SyNr = 0 reads universe.
 * @param NrRecs The maximum number of records available for reading. The actual number of records may be less when a filter is applied to the records read.
 * @return Non-zero if the operation is possible, zero otherwise.
 */
GDX_FUNCPTR(gdxDataReadRawStart);

typedef int  (GDX_CALLCONV *gdxDataReadSlice_t) (gdxHandle_t pgdx, const char *UelFilterStr[], int *Dimen, TDataStoreProc_t DP);
/** Read a slice of data from a data set, by fixing zero or more index positions in the data. When a data element is available, the callback procedure DP is called with the current index and the values. The indices used in the index vary from zero to the highest value minus one for that index position. This function can be called multiple times. Returns zero if the operation is not possible.
 *
 * @param pgdx gdx object handle
 * @param UelFilterStr Each index can be fixed by setting the string for the unique element. Set an index position to the empty string in order not to fix that position.
 * @param Dimen The dimension of the index space; this is the number of index positions that is not fixed.
 * @param DP Callback procedure which will be called for each available data item. Signature is <ul><li>UEL index number keys for each symbol dimension (const int )</li> <li>5 double values (const double )</li></ul>
 * @return Non-zero if the operation is possible, zero otherwise.
 */
GDX_FUNCPTR(gdxDataReadSlice);

typedef int  (GDX_CALLCONV *gdxDataReadSliceStart_t) (gdxHandle_t pgdx, int SyNr, int ElemCounts[]);
/** Prepare for the reading of a slice of data from a data set. The actual read of the data is done by calling gdxDataReadSlice. When finished reading, call gdxDataReadDone. Returns zero if the operation is not possible.
 *
 * @param pgdx gdx object handle
 * @param SyNr Symbol number to read, range 1..NrSymbols; SyNr = 0 reads universe.
 * @param ElemCounts Array of integers, each position indicating the number of unique indices in that position.
 * @return Non-zero if the operation is possible, zero otherwise.
 */
GDX_FUNCPTR(gdxDataReadSliceStart);

typedef int  (GDX_CALLCONV *gdxDataReadStr_t) (gdxHandle_t pgdx, char *KeyStr[], double Values[], int *DimFrst);
/** Read the next record using strings for the unique elements. The reading should be initialized by calling DataReadStrStart. Returns zero if the operation is not possible or if there is no more data.
 *
 * @param pgdx gdx object handle
 * @param KeyStr The index of the record as strings for the unique elements. Array of strings with one string for each dimension.
 * @param Values The data of the record (level, marginal, lower-, upper-bound, scale).
 * @param DimFrst The first index position in KeyStr that changed.
 * @return Non-zero if the operation is possible; return zero if the operation is not possible or if there is no more data.
 */
GDX_FUNCPTR(gdxDataReadStr);

typedef int  (GDX_CALLCONV *gdxDataReadStrStart_t) (gdxHandle_t pgdx, int SyNr, int *NrRecs);
/** Initialize the reading of a symbol in string mode. Returns zero if the operation is not possible.
 *
 * @param pgdx gdx object handle
 * @param SyNr The index number of the symbol (range 0..NrSymbols); SyNr = 0 reads universe.
 * @param NrRecs The maximum number of records available for reading. The actual number of records may be less when a filter is applied to the records read.
 * @return Non-zero if the operation is possible, zero otherwise.
 */
GDX_FUNCPTR(gdxDataReadStrStart);

typedef int  (GDX_CALLCONV *gdxDataSliceUELS_t) (gdxHandle_t pgdx, const int SliceKeyInt[], char *KeyStr[]);
/** Map a slice index in to the corresponding unique elements. After calling DataReadSliceStart, each index position is mapped from 0 to N(d)-1. This function maps this index space back in to unique elements represented as strings. Returns zero if the operation is not possible.
 *
 * @param pgdx gdx object handle
 * @param SliceKeyInt The slice index to be mapped to strings with one entry for each symbol dimension.
 * @param KeyStr Array of strings containing the unique elements.
 * @return Non-zero if the operation is possible, zero otherwise.
 */
GDX_FUNCPTR(gdxDataSliceUELS);

typedef int  (GDX_CALLCONV *gdxDataWriteDone_t) (gdxHandle_t pgdx);
/** Finish a write operation. Returns zero if the operation is not possible.
 *
 * @param pgdx gdx object handle
 * @return Non-zero if the operation is possible, zero otherwise.
 */
GDX_FUNCPTR(gdxDataWriteDone);

typedef int  (GDX_CALLCONV *gdxDataWriteMap_t) (gdxHandle_t pgdx, const int KeyInt[], const double Values[]);
/** Write a data element in mapped mode. Returns zero if the operation is not possible.
 *
 * @param pgdx gdx object handle
 * @param KeyInt The index for this element using mapped values.
 * @param Values The values for this element.
 * @return Non-zero if the operation is possible, zero otherwise.
 */
GDX_FUNCPTR(gdxDataWriteMap);

typedef int  (GDX_CALLCONV *gdxDataWriteMapStart_t) (gdxHandle_t pgdx, const char *SyId, const char *ExplTxt, int Dimen, int Typ, int UserInfo);
/** Start writing a new symbol in mapped mode. Returns zero if the operation is not possible.
 *
 * @param pgdx gdx object handle
 * @param SyId Name of the symbol (up to 63 characters) or acronym. The first character of a symbol must be a letter. Following symbol characters may be letters, digits, and underscores. Symbol names must be new and unique. Might be an empty string at gdxAcronymName.
 * @param ExplTxt Explanatory text for the symbol (up to 255 characters).
 * @param Dimen Dimension of the symbol.
 * @param Typ Type of the symbol.
 * @param UserInfo User field value storing additional data; GAMS follows the following conventions: <table> <tr><td>Type</td><td>Value(s)</td></tr> <tr><td>Aliased Set</td><td>The symbol number of the aliased set, or zero for the universe</td></tr> <tr><td>Set</td><td>Zero</td></tr> <tr><td>Parameter</td><td>Zero</td></tr> <tr><td>Variable</td><td>The variable type: binary=1, integer=2, positive=3, negative=4, free=5, sos1=6, sos2=7, semicontinous=8, semiinteger=9</td></tr> <tr><td>Equation</td><td>The equation type: eque=53, equg=54, equl=55, equn=56, equx=57, equc=58, equb=59</td></tr> </table>
 * @return Non-zero if the operation is possible, zero otherwise
 */
GDX_FUNCPTR(gdxDataWriteMapStart);

typedef int  (GDX_CALLCONV *gdxDataWriteRaw_t) (gdxHandle_t pgdx, const int KeyInt[], const double Values[]);
/** Write a data element in raw mode. Returns zero if the operation is not possible.
 *
 * @param pgdx gdx object handle
 * @param KeyInt The index for this element.
 * @param Values The values for this element.
 * @return Non-zero if the operation is possible, zero otherwise.
 */
GDX_FUNCPTR(gdxDataWriteRaw);

typedef int  (GDX_CALLCONV *gdxDataWriteRawStart_t) (gdxHandle_t pgdx, const char *SyId, const char *ExplTxt, int Dimen, int Typ, int UserInfo);
/** Start writing a new symbol in raw mode. Returns zero if the operation is not possible.
 *
 * @param pgdx gdx object handle
 * @param SyId Name of the symbol (up to 63 characters). The first character of a symbol must be a letter. Following symbol characters may be letters, digits, and underscores. Symbol names must be new and unique.
 * @param ExplTxt Explanatory text for the symbol (up to 255 characters).
 * @param Dimen Dimension of the symbol (up to 20).
 * @param Typ Type of the symbol (set=0, parameter=1, variable=2, equation=3, alias=4).
 * @param UserInfo User field value storing additional data; GAMS follows the following conventions: <table> <tr> <td>Type</td> <td>Value(s)</td> </tr> <tr> <td>Aliased Set</td> <td>The symbol number of the aliased set, or zero for the universe</td> </tr> <tr> <td>Set</td> <td>Zero</td> </tr> <tr> <td>Parameter</td> <td>Zero</td> </tr> <tr> <td>Variable</td> <td>The variable type: binary=1, integer=2, positive=3, negative=4, free=5, sos1=6, sos2=7, semicontinous=8, semiinteger=9 </td> </tr> <tr> <td>Equation</td> <td>The equation type: eque=53, equg=54, equl=55, equn=56, equx=57, equc=58, equb=59</td> </tr> </table>
 * @return Non-zero if the operation is possible, zero otherwise.
 */
GDX_FUNCPTR(gdxDataWriteRawStart);

typedef int  (GDX_CALLCONV *gdxDataWriteRawStartKeyBounds_t) (gdxHandle_t pgdx, const char *SyId, const char *ExplTxt, int Dimen, int Typ, int UserInfo, const int MinUELIndices[], const int MaxUELIndices[]);
/** Start writing a new symbol in raw mode with bounds for UEL key indices being known in advance.
 *
 * This helps potentially reducing the required storage for the keys as smaller integral datatypes can be used.
 *
 * Returns zero if the operation is not possible.
 *
 * @param pgdx gdx object handle
 * @param SyId Name of the symbol (up to 63 characters). The first character of a symbol must be a letter. Following symbol characters may be letters, digits, and underscores. Symbol names must be new and unique.
 * @param ExplTxt Explanatory text for the symbol (up to 255 characters).
 * @param Dimen Dimension of the symbol (up to 20).
 * @param Typ Type of the symbol (set=0, parameter=1, variable=2, equation=3, alias=4).
 * @param UserInfo User field value storing additional data; GAMS follows the following conventions: <table> <tr> <td>Type</td> <td>Value(s)</td> </tr> <tr> <td>Aliased Set</td> <td>The symbol number of the aliased set, or zero for the universe</td> </tr> <tr> <td>Set</td> <td>Zero</td> </tr> <tr> <td>Parameter</td> <td>Zero</td> </tr> <tr> <td>Variable</td> <td>The variable type: binary=1, integer=2, positive=3, negative=4, free=5, sos1=6, sos2=7, semicontinous=8, semiinteger=9 </td> </tr> <tr> <td>Equation</td> <td>The equation type: eque=53, equg=54, equl=55, equn=56, equx=57, equc=58, equb=59</td> </tr> </table>
 * @param MinUELIndices Minimum UEL indices for each symbol dimension. Can help with shrinking storage for keys.
 * @param MaxUELIndices Maximum UEL indices for each symbol dimension. Can help with shrinking storage for keys.
 * @return Non-zero if the operation is possible, zero otherwise.
 */
GDX_FUNCPTR(gdxDataWriteRawStartKeyBounds);

typedef int  (GDX_CALLCONV *gdxDataWriteStr_t) (gdxHandle_t pgdx, const char *KeyStr[], const double Values[]);
/** Write a data element in string mode. Each element string must follow the GAMS rules for unique elements. Returns zero if the operation is not possible.
 *
 * @param pgdx gdx object handle
 * @param KeyStr The index for this element using strings for the unique elements. One entry for each symbol dimension.
 * @param Values The values for this element (level, marginal, lower-, upper-bound, scale).
 * @return Non-zero if the operation is possible, zero otherwise.
 */
GDX_FUNCPTR(gdxDataWriteStr);

typedef int  (GDX_CALLCONV *gdxDataWriteStrStart_t) (gdxHandle_t pgdx, const char *SyId, const char *ExplTxt, int Dimen, int Typ, int UserInfo);
/** Start writing a new symbol in string mode. Returns zero if the operation is not possible or failed.
 *
 * @param pgdx gdx object handle
 * @param SyId Name of the symbol (limited to 63 characters). The first character of a symbol must be a letter. Following symbol characters may be letters, digits, and underscores. Symbol names must be new and unique.
 * @param ExplTxt Explanatory text for the symbol (limited to 255 characters). Mixed quote characters will be unified to first occurring one.
 * @param Dimen Dimension of the symbol (limited to 20).
 * @param Typ Type of the symbol (set=0, parameter=1, variable=2, equation=3, alias=4).
 * @param UserInfo Supply additional data. See gdxDataWriteRawStart for more information.
 * @return Non-zero if the operation is possible, zero otherwise.
 */
GDX_FUNCPTR(gdxDataWriteStrStart);

typedef int  (GDX_CALLCONV *gdxGetDLLVersion_t) (gdxHandle_t pgdx, char *V);
/** Returns a version descriptor of the library. Always non-zero.
 *
 * @param pgdx gdx object handle
 * @param V Contains version string after return.
 * @return Always returns non-zero.
 */
GDX_FUNCPTR(gdxGetDLLVersion);

typedef int  (GDX_CALLCONV *gdxErrorCount_t) (gdxHandle_t pgdx);
/** Returns the number of errors.
 *
 * @param pgdx gdx object handle
 * @return Total number of errors encountered.
 */
GDX_FUNCPTR(gdxErrorCount);

typedef int  (GDX_CALLCONV *gdxErrorStr_t) (gdxHandle_t pgdx, int ErrNr, char *ErrMsg);
/** Returns the text for a given error number. Always non-zero.
 *
 * @param pgdx gdx object handle
 * @param ErrNr Error number.
 * @param ErrMsg Error message (output argument). Contains error text after return.
 * @return Always returns non-zero.
 */
GDX_FUNCPTR(gdxErrorStr);

typedef int  (GDX_CALLCONV *gdxFileInfo_t) (gdxHandle_t pgdx, int *FileVer, int *ComprLev);
/** Returns file format number and compression level used. Always non-zero.
 *
 * @param pgdx gdx object handle
 * @param FileVer File format number or zero if the file is not open.
 * @param ComprLev Compression used; 0= no compression, 1=zlib.
 * @return Always returns non-zero.
 */
GDX_FUNCPTR(gdxFileInfo);

typedef int  (GDX_CALLCONV *gdxFileVersion_t) (gdxHandle_t pgdx, char *FileStr, char *ProduceStr);
/** Return strings for file version and file producer. Always non-zero.
 *
 * @param pgdx gdx object handle
 * @param FileStr Version string (out argument). Known versions are V5, V6U, V6C and V7.
 * @param ProduceStr Producer string (out argument). The producer is the application that wrote the GDX file.
 * @return Always non-zero.
 */
GDX_FUNCPTR(gdxFileVersion);

typedef int  (GDX_CALLCONV *gdxFilterExists_t) (gdxHandle_t pgdx, int FilterNr);
/** Check if there is a filter defined based on its number as used in gdxFilterRegisterStart. Returns zero if the operation is not possible.
 *
 * @param pgdx gdx object handle
 * @param FilterNr Filter number as used in FilterRegisterStart.
 * @return Non-zero if the operation is possible, zero otherwise.
 */
GDX_FUNCPTR(gdxFilterExists);

typedef int  (GDX_CALLCONV *gdxFilterRegister_t) (gdxHandle_t pgdx, int UelMap);
/** Add a unique element to the current filter definition, zero if the index number is out of range or was never mapped into the user index space.
 *
 * @param pgdx gdx object handle
 * @param UelMap Unique element number in the user index space or -1 if element was never mapped.
 * @return Non-zero if the operation is possible, zero otherwise.
 */
GDX_FUNCPTR(gdxFilterRegister);

typedef int  (GDX_CALLCONV *gdxFilterRegisterDone_t) (gdxHandle_t pgdx);
/** Finish registration of unique elements for a filter. Returns zero if the operation is not possible.
 *
 * @param pgdx gdx object handle
 * @return Non-zero if the operation is possible, zero otherwise.
 */
GDX_FUNCPTR(gdxFilterRegisterDone);

typedef int  (GDX_CALLCONV *gdxFilterRegisterStart_t) (gdxHandle_t pgdx, int FilterNr);
/** Define a unique element filter. Returns zero if the operation is not possible.
 *
 * @param pgdx gdx object handle
 * @param FilterNr Filter number to be assigned.
 * @return Non-zero if the operation is possible, zero otherwise.
 */
GDX_FUNCPTR(gdxFilterRegisterStart);

typedef int  (GDX_CALLCONV *gdxFindSymbol_t) (gdxHandle_t pgdx, const char *SyId, int *SyNr);
/** Search for a symbol by name in the symbol table; the search is not case-sensitive. <ul><li>When the symbol is found, SyNr contains the symbol number and the function returns a non-zero integer.</li> <li>When the symbol is not found, the function returns zero and SyNr is set to -1.</li></ul>
 *
 * @param pgdx gdx object handle
 * @param SyId Name of the symbol (must not exceed 63 characters).
 * @param SyNr Symbol number (>=1 if exists, 0 for universe and -1 if not found).
 * @return Non-zero if the symbol is found, zero otherwise.
 */
GDX_FUNCPTR(gdxFindSymbol);

typedef int  (GDX_CALLCONV *gdxGetElemText_t) (gdxHandle_t pgdx, int TxtNr, char *Txt, int *Node);
/** Retrieve the string and node number for an entry in the string table. Returns zero if the operation is not possible.
 *
 * @param pgdx gdx object handle
 * @param TxtNr String table index.
 * @param Txt Text found for the entry. Buffer should be 256 bytes wide.
 * @param Node Node number (user space) found for the entry.
 * @return Non-zero if the operation is possible, zero otherwise.
 */
GDX_FUNCPTR(gdxGetElemText);

typedef int  (GDX_CALLCONV *gdxGetLastError_t) (gdxHandle_t pgdx);
/** Returns the last error number or zero if there was no error. Calling this function will clear the last error stored.
 *
 * @param pgdx gdx object handle
 * @return The error number, or zero if there was no error.
 */
GDX_FUNCPTR(gdxGetLastError);

typedef INT64  (GDX_CALLCONV *gdxGetMemoryUsed_t) (gdxHandle_t pgdx);
/** Return the number of bytes used by the data objects.
 *
 * @param pgdx gdx object handle
 * @return The number of bytes used by the data objects.
 */
GDX_FUNCPTR(gdxGetMemoryUsed);

typedef int  (GDX_CALLCONV *gdxGetSpecialValues_t) (gdxHandle_t pgdx, double AVals[]);
/** Retrieve the internal values for special values. Always non-zero.
 *
 * @param pgdx gdx object handle
 * @param AVals 6-element array of special values used for Undef (0), NA (1), +Inf (2), -Inf (3), Eps (4), Acronym (6).
 * @return Always non-zero.
 */
GDX_FUNCPTR(gdxGetSpecialValues);

typedef int  (GDX_CALLCONV *gdxGetUEL_t) (gdxHandle_t pgdx, int UelNr, char *Uel);
/** Get the string for a unique element using a mapped index. Returns zero if the operation is not possible.
 *
 * @param pgdx gdx object handle
 * @param UelNr Index number in user space (range 1..NrUserElem).
 * @param Uel String for the unique element which may be up to 63 characters.
 * @return Return non-zero if the index is in a valid range, zero otherwise.
 */
GDX_FUNCPTR(gdxGetUEL);

typedef int  (GDX_CALLCONV *gdxMapValue_t) (gdxHandle_t pgdx, double D, int *sv);
/** Classify a value as a potential special value. Non-zero if D is a special value, zero otherwise.
 *
 * @param pgdx gdx object handle
 * @param D Value to classify.
 * @param sv Classification.
 * @return Returns non-zero if D is a special value, zero otherwise.
 */
GDX_FUNCPTR(gdxMapValue);

typedef int  (GDX_CALLCONV *gdxOpenAppend_t) (gdxHandle_t pgdx, const char *FileName, const char *Producer, int *ErrNr);
/** Open an existing GDX file for output. Non-zero if the file can be opened, zero otherwise.
 *
 * @param pgdx gdx object handle
 * @param FileName File name of the GDX file to be created (arbitrary length).
 * @param Producer Name of program that appends to the GDX file (should not exceed 255 characters).
 * @param ErrNr Returns an error code or zero if there is no error.
 * @return Returns non-zero if the file can be opened; zero otherwise.
 */
GDX_FUNCPTR(gdxOpenAppend);

typedef int  (GDX_CALLCONV *gdxOpenRead_t) (gdxHandle_t pgdx, const char *FileName, int *ErrNr);
/** Open a GDX file for reading. Non-zero if the file can be opened, zero otherwise.
 *
 * @param pgdx gdx object handle
 * @param FileName File name of the GDX file to be opened (arbitrary length).
 * @param ErrNr Returns an error code or zero if there is no error.
 * @return Returns non-zero if the file can be opened; zero otherwise.
 */
GDX_FUNCPTR(gdxOpenRead);

typedef int  (GDX_CALLCONV *gdxOpenReadEx_t) (gdxHandle_t pgdx, const char *FileName, int ReadMode, int *ErrNr);
/** Open a GDX file for reading allowing for skipping sections. Non-zero if the file can be opened, zero otherwise.
 *
 * @param pgdx gdx object handle
 * @param FileName File name of the GDX file to be opened (arbitrary length).
 * @param ReadMode Bitmap skip reading sections: 0-bit: string (1 skip reading string).
 * @param ErrNr Returns an error code or zero if there is no error.
 * @return Returns non-zero if the file can be opened; zero otherwise.
 */
GDX_FUNCPTR(gdxOpenReadEx);

typedef int  (GDX_CALLCONV *gdxOpenWrite_t) (gdxHandle_t pgdx, const char *FileName, const char *Producer, int *ErrNr);
/** Open a new GDX file for output. Non-zero if the file can be opened, zero otherwise.
 *
 * @param pgdx gdx object handle
 * @param FileName File name of the GDX file to be created with arbitrary length.
 * @param Producer Name of program that creates the GDX file (should not exceed 255 characters).
 * @param ErrNr Returns an error code or zero if there is no error.
 * @return Returns non-zero if the file can be opened; zero otherwise.
 */
GDX_FUNCPTR(gdxOpenWrite);

typedef int  (GDX_CALLCONV *gdxOpenWriteEx_t) (gdxHandle_t pgdx, const char *FileName, const char *Producer, int Compr, int *ErrNr);
/** Create a GDX file for writing with explicitly given compression flag. Non-zero if the file can be opened, zero otherwise.
 *
 * @param pgdx gdx object handle
 * @param FileName File name of the GDX file to be created with arbitrary length.
 * @param Producer Name of program that creates the GDX file (should not exceed 255 characters).
 * @param Compr Zero for no compression; non-zero uses compression (if available).
 * @param ErrNr Returns an error code or zero if there is no error.
 * @return Returns non-zero if the file can be opened; zero otherwise.
 */
GDX_FUNCPTR(gdxOpenWriteEx);

typedef int  (GDX_CALLCONV *gdxResetSpecialValues_t) (gdxHandle_t pgdx);
/** Reset the internal values for special values. Always non-zero.
 *
 * @param pgdx gdx object handle
 * @return Always non-zero.
 */
GDX_FUNCPTR(gdxResetSpecialValues);

typedef int  (GDX_CALLCONV *gdxSetHasText_t) (gdxHandle_t pgdx, int SyNr);
/** Test if any of the elements of the set has an associated text. Non-zero if the Set contains at least one unique element that has associated text, zero otherwise.
 *
 * @param pgdx gdx object handle
 * @param SyNr Set symbol number (range 1..NrSymbols); SyNr = 0 reads universe.
 * @return Non-zero if the set contains at least one element that has associated text, zero otherwise.
 */
GDX_FUNCPTR(gdxSetHasText);

typedef int  (GDX_CALLCONV *gdxSetReadSpecialValues_t) (gdxHandle_t pgdx, const double AVals[]);
/** Set the internal values for special values when reading a GDX file. Before calling this function, initialize the array of special values by calling gdxGetSpecialValues first. Always non-zero.
 *
 * @param pgdx gdx object handle
 * @param AVals 5-element array of special values to be used for Undef, NA, +Inf, -Inf, and Eps. Note that the values do not have to be unique.
 * @return Always non-zero.
 */
GDX_FUNCPTR(gdxSetReadSpecialValues);

typedef int  (GDX_CALLCONV *gdxSetSpecialValues_t) (gdxHandle_t pgdx, const double AVals[]);
/** Set the internal values for special values. Before calling this function, initialize the array of special values by calling gdxGetSpecialValues first. Note, values in AVals have to be unique. Non-zero if all values specified are unique, zero otherwise.
 *
 * @param pgdx gdx object handle
 * @param AVals Array of special values to be used Undef (0), NA (1), +Inf (2), -Inf (3), and EPS (4). Note that the values have to be unique and AVals should have length 7.
 * @return Non-zero if all values specified are unique, zero otherwise.
 */
GDX_FUNCPTR(gdxSetSpecialValues);

typedef int  (GDX_CALLCONV *gdxSetTextNodeNr_t) (gdxHandle_t pgdx, int TxtNr, int Node);
/** Set the Node number for an entry in the string table. After registering a string with AddSetText, we can assign a node number for later retrieval. Returns zero if the operation is not possible.
 *
 * @param pgdx gdx object handle
 * @param TxtNr Index number of the entry to be modified.
 * @param Node The new Node value for the entry.
 * @return Non-zero if the operation is possible, zero otherwise.
 */
GDX_FUNCPTR(gdxSetTextNodeNr);

typedef int  (GDX_CALLCONV *gdxSetTraceLevel_t) (gdxHandle_t pgdx, int N, const char *s);
/** Set the amount of trace (debug) information generated. Always non-zero.
 *
 * @param pgdx gdx object handle
 * @param N Tracing level N <= 0 no tracing N >= 3 maximum tracing.
 * @param s A string to be included in the trace output (arbitrary length).
 * @return Always non-zero.
 */
GDX_FUNCPTR(gdxSetTraceLevel);

typedef int  (GDX_CALLCONV *gdxSymbIndxMaxLength_t) (gdxHandle_t pgdx, int SyNr, int LengthInfo[]);
/** Returns the length of the longest UEL used for every index position for a given symbol.
 *
 * @param pgdx gdx object handle
 * @param SyNr Symbol number (range 1..NrSymbols); SyNr = 0 reads universe.
 * @param LengthInfo The longest length for each index position. This output argument should be able to store one integer for each symbol dimension.
 * @return The length of the longest UEL found in the data (over all dimensions).
 */
GDX_FUNCPTR(gdxSymbIndxMaxLength);

typedef int  (GDX_CALLCONV *gdxSymbMaxLength_t) (gdxHandle_t pgdx);
/** Returns the length of the longest symbol name in the GDX file.
 *
 * @param pgdx gdx object handle
 * @return The number of characters in the longest symbol name.
 */
GDX_FUNCPTR(gdxSymbMaxLength);

typedef int  (GDX_CALLCONV *gdxSymbolAddComment_t) (gdxHandle_t pgdx, int SyNr, const char *Txt);
/** Add a line of comment text for a symbol. Returns zero if the operation is not possible.
 *
 * @param pgdx gdx object handle
 * @param SyNr The symbol number (range 1..NrSymbols); if SyNr <= 0 the current symbol being written.
 * @param Txt String to add which should not exceed a length of 255 characters.
 * @return Non-zero if the operation is possible, zero otherwise.
 */
GDX_FUNCPTR(gdxSymbolAddComment);

typedef int  (GDX_CALLCONV *gdxSymbolGetComment_t) (gdxHandle_t pgdx, int SyNr, int N, char *Txt);
/** Retrieve a line of comment text for a symbol. Returns zero if the operation is not possible.
 *
 * @param pgdx gdx object handle
 * @param SyNr The symbol number (range 1..NrSymbols); SyNr = 0 reads universe.
 * @param N Line number in the comment block (1..Count).
 * @param Txt String containing the line requested (empty on error). Buffer should be able to hold 255 characters. Potential causes for empty strings are symbol- (SyNr) or line-number (N) out of range.
 * @return Non-zero if the operation is possible, zero otherwise.
 */
GDX_FUNCPTR(gdxSymbolGetComment);

typedef int  (GDX_CALLCONV *gdxSymbolGetDomain_t) (gdxHandle_t pgdx, int SyNr, int DomainSyNrs[]);
/** Retrieve the domain of a symbol. Returns zero if the operation is not possible.
 *
 * @param pgdx gdx object handle
 * @param SyNr The index number of the symbol (range 1..NrSymbols); SyNr = 0 reads universe.
 * @param DomainSyNrs Array (length=symbol dim) returning the set identifiers or "*"; DomainSyNrs[D] will contain the index number of the one dimensional set or alias used as the domain for index position D. A value of zero represents the universe "*".
 * @return Non-zero if the operation is possible, zero otherwise.
 */
GDX_FUNCPTR(gdxSymbolGetDomain);

typedef int  (GDX_CALLCONV *gdxSymbolGetDomainX_t) (gdxHandle_t pgdx, int SyNr, char *DomainIDs[]);
/** Retrieve the domain of a symbol (using relaxed or domain information). Returns zero if the operation is not possible.
 *
 * @param pgdx gdx object handle
 * @param SyNr The index number of the symbol (range 1..NrSymbols); SyNr = 0 reads universe.
 * @param DomainIDs DomainIDs[D] will contain the strings as they were stored with the call gdxSymbolSetDomainX. If gdxSymbolSetDomainX was never called, but gdxSymbolSetDomain was called, that information will be used instead. Length of this array should by dimensionality of the symbol. The special domain name "*" denotes the universe domain (all known UELs).
 * @return <ul><li>0: If operation was not possible (Bad SyNr)</li> <li>1: No domain information was available</li> <li>2: Data used was defined using gdxSymbolSetDomainX</li> <li>3: Data used was defined using gdxSymbolSetDomain</li></ul>
 */
GDX_FUNCPTR(gdxSymbolGetDomainX);

typedef int  (GDX_CALLCONV *gdxSymbolDim_t) (gdxHandle_t pgdx, int SyNr);
/** Returns dimensionality of a symbol.
 *
 * @param pgdx gdx object handle
 * @param SyNr The symbol number (range 0..NrSymbols); return universe info when SyNr = 0..
 * @return -1 if the symbol number is not in the correct range, the symbol dimension otherwise.
 */
GDX_FUNCPTR(gdxSymbolDim);

typedef int  (GDX_CALLCONV *gdxSymbolInfo_t) (gdxHandle_t pgdx, int SyNr, char *SyId, int *Dimen, int *Typ);
/** Returns information (name, dimension count, type) about a symbol from the symbol table. Returns zero if the symbol number is out of range, non-zero otherwise.
 *
 * @param pgdx gdx object handle
 * @param SyNr The symbol number (range 0..NrSymbols); return universe info () when SyNr = 0.
 * @param SyId Name of the symbol (buffer should be 64 bytes long). Magic name "*" for universe.
 * @param Dimen Dimension of the symbol (range 0..20).
 * @param Typ Symbol type (set=0, parameter=1, variable=2, equation=3, alias=4).
 * @return Zero if the symbol number is not in the correct range, non-zero otherwise.
 */
GDX_FUNCPTR(gdxSymbolInfo);

typedef int  (GDX_CALLCONV *gdxSymbolInfoX_t) (gdxHandle_t pgdx, int SyNr, int *RecCnt, int *UserInfo, char *ExplTxt);
/** Returns additional information about a symbol. Returns zero if the symbol number is out of range, non-zero otherwise.
 *
 * @param pgdx gdx object handle
 * @param SyNr The symbol number (range 0..NrSymbols); return universe info when SyNr = 0.
 * @param RecCnt Total number of records stored (unmapped); for the universe (SyNr = 0) this is the number of entries when the GDX file was opened for reading.
 * @param UserInfo User field value storing additional data; GAMS follows the following conventions: <table> <tr> <td>Type</td> <td>Value(s)</td> </tr> <tr> <td>Aliased Set</td> <td>The symbol number of the aliased set, or zero for the universe</td> </tr> <tr> <td>Set</td> <td>Zero</td> </tr> <tr> <td>Parameter</td> <td>Zero</td> </tr> <tr> <td>Variable</td> <td>The variable type: binary=1, integer=2, positive=3, negative=4, free=5, sos1=6, sos2=7, semicontinous=8, semiinteger=9 </td> </tr> <tr> <td>Equation</td> <td>The equation type: eque=53, equg=54, equl=55, equn=56, equx=57, equc=58, equb=59</td> </tr> </table>
 * @param ExplTxt Explanatory text for the symbol. Buffer for this output argument should be 256 bytes long.
 * @return Zero if the symbol number is not in the correct range, non-zero otherwise.
 */
GDX_FUNCPTR(gdxSymbolInfoX);

typedef int  (GDX_CALLCONV *gdxSymbolSetDomain_t) (gdxHandle_t pgdx, const char *DomainIDs[]);
/** Define the domain of a symbol for which a write data operation just started using DataWriteRawStart, DataWriteMapStart or DataWriteStrStart. Returns zero if the operation is not possible.
 *
 * @param pgdx gdx object handle
 * @param DomainIDs Array of identifiers (domain names) or "*" (universe). One domain name for each symbol dimension.
 * @return Non-zero if the operation is possible, zero otherwise.
 */
GDX_FUNCPTR(gdxSymbolSetDomain);

typedef int  (GDX_CALLCONV *gdxSymbolSetDomainX_t) (gdxHandle_t pgdx, int SyNr, const char *DomainIDs[]);
/** Define the domain of a symbol (relaxed version). Returns zero if the operation is not possible.
 *
 * @param pgdx gdx object handle
 * @param SyNr The index number of the symbol, range from 0 to NrSymbols; SyNr = 0 reads universe.
 * @param DomainIDs Array of identifiers (domain names) or "*" (universe). One domain name per symbol dimension with not more than 63 characters.
 * @return Non-zero if the operation is possible, zero otherwise.
 */
GDX_FUNCPTR(gdxSymbolSetDomainX);

typedef int  (GDX_CALLCONV *gdxSystemInfo_t) (gdxHandle_t pgdx, int *SyCnt, int *UelCnt);
/** Returns the number of symbols and unique elements. Always non-zero.
 *
 * @param pgdx gdx object handle
 * @param SyCnt Number of symbols (sets, parameters, ...) available in the GDX file.
 * @param UelCnt Number of unique elements (UELs) stored in the GDX file.
 * @return Returns a non-zero value.
 */
GDX_FUNCPTR(gdxSystemInfo);

typedef int  (GDX_CALLCONV *gdxUELMaxLength_t) (gdxHandle_t pgdx);
/** Returns the length of the longest unique element (UEL) name.
 *
 * @param pgdx gdx object handle
 * @return The length of the longest UEL name in the UEL table.
 */
GDX_FUNCPTR(gdxUELMaxLength);

typedef int  (GDX_CALLCONV *gdxUELRegisterDone_t) (gdxHandle_t pgdx);
/** Finish registration of unique elements. Returns zero if the operation is not possible.
 *
 * @param pgdx gdx object handle
 * @return Non-zero if the operation is possible, zero otherwise.
 */
GDX_FUNCPTR(gdxUELRegisterDone);

typedef int  (GDX_CALLCONV *gdxUELRegisterMap_t) (gdxHandle_t pgdx, int UMap, const char *Uel);
/** Register unique element in mapped mode. A unique element must follow the GAMS rules when it contains quote characters. Returns zero if the operation is not possible.
 *
 * @param pgdx gdx object handle
 * @param UMap User index number to be assigned to the unique element, -1 if not found or the element was never mapped.
 * @param Uel String for unique element (max. 63 chars and no single-/double-quote mixing).
 * @return Non-zero if the operation is possible, zero otherwise.
 */
GDX_FUNCPTR(gdxUELRegisterMap);

typedef int  (GDX_CALLCONV *gdxUELRegisterMapStart_t) (gdxHandle_t pgdx);
/** Start registering unique elements in mapped mode. Returns zero if the operation is not possible.
 *
 * @param pgdx gdx object handle
 * @return Non-zero if the operation is possible, zero otherwise.
 */
GDX_FUNCPTR(gdxUELRegisterMapStart);

typedef int  (GDX_CALLCONV *gdxUELRegisterRaw_t) (gdxHandle_t pgdx, const char *Uel);
/** Register unique element in raw mode. This can only be used while writing to a GDX file. Returns zero if the operation is not possible.
 *
 * @param pgdx gdx object handle
 * @param Uel String for unique element (UEL) which may not exceed 63 characters in length. Furthermore a UEL string must not mix single- and double-quotes.
 * @return Non-zero if the operation is possible, zero otherwise.
 */
GDX_FUNCPTR(gdxUELRegisterRaw);

typedef int  (GDX_CALLCONV *gdxUELRegisterRawStart_t) (gdxHandle_t pgdx);
/** Start registering unique elements in raw mode. Returns zero if the operation is not possible.
 *
 * @param pgdx gdx object handle
 * @return Non-zero if the operation is possible, zero otherwise.
 */
GDX_FUNCPTR(gdxUELRegisterRawStart);

typedef int  (GDX_CALLCONV *gdxUELRegisterStr_t) (gdxHandle_t pgdx, const char *Uel, int *UelNr);
/** Register a unique element in string mode. A unique element must follow the GAMS rules when it contains quote characters. Non-zero if the element was registered, zero otherwise.
 *
 * @param pgdx gdx object handle
 * @param Uel String for unique element (UEL) which may not exceed a length of 63 characters. Furthermore a UEL string must not mix single- and double-quotes.
 * @param UelNr Internal index number assigned to this unique element in user space (or -1 if not found).
 * @return Non-zero if the element was registered, zero otherwise.
 */
GDX_FUNCPTR(gdxUELRegisterStr);

typedef int  (GDX_CALLCONV *gdxUELRegisterStrStart_t) (gdxHandle_t pgdx);
/** Start registering unique elements in string mode. Returns zero if the operation is not possible.
 *
 * @param pgdx gdx object handle
 * @return Non-zero if the operation is possible, zero otherwise.
 */
GDX_FUNCPTR(gdxUELRegisterStrStart);

typedef int  (GDX_CALLCONV *gdxUMFindUEL_t) (gdxHandle_t pgdx, const char *Uel, int *UelNr, int *UelMap);
/** Search for unique element by its string. Non-zero if the element was found, zero otherwise.
 *
 * @param pgdx gdx object handle
 * @param Uel String to be searched (not longer than 63 characters, don't mix single- and double-quotes).
 * @param UelNr Internal unique element number or -1 if not found.
 * @param UelMap User mapping for the element or -1 if not found or the element was never mapped.
 * @return Non-zero if the element was found, zero otherwise.
 */
GDX_FUNCPTR(gdxUMFindUEL);

typedef int  (GDX_CALLCONV *gdxUMUelGet_t) (gdxHandle_t pgdx, int UelNr, char *Uel, int *UelMap);
/** Get a unique element using an unmapped index. Returns zero if the operation is not possible.
 *
 * @param pgdx gdx object handle
 * @param UelNr Element number (unmapped) (range 1..NrElem) or -1 if not found.
 * @param Uel String for unique element. Buffer should be 64 bytes long (to store maximum of 63 characters).
 * @param UelMap User mapping for this element or -1 if element was never mapped.
 * @return Non-zero if the operation is possible, zero otherwise.
 */
GDX_FUNCPTR(gdxUMUelGet);

typedef int  (GDX_CALLCONV *gdxUMUelInfo_t) (gdxHandle_t pgdx, int *UelCnt, int *HighMap);
/** Return information about the unique elements (UELs). Always non-zero.
 *
 * @param pgdx gdx object handle
 * @param UelCnt Total number of unique elements (UELs in GDX file plus new registered UELs).
 * @param HighMap Highest user mapping index used.
 * @return Always returns non-zero.
 */
GDX_FUNCPTR(gdxUMUelInfo);

typedef int  (GDX_CALLCONV *gdxGetDomainElements_t) (gdxHandle_t pgdx, int SyNr, int DimPos, int FilterNr, TDomainIndexProc_t DP, int *NrElem, void *Uptr);
/** Get the unique elements for a given dimension of a given symbol.
 *
 * @param pgdx gdx object handle
 * @param SyNr The index number of the symbol, range 1..NrSymbols; SyNr = 0 reads universe.
 * @param DimPos The dimension to use, range 1..dim.
 * @param FilterNr Number of a previously registered filter or the value DOMC_EXPAND if no filter is wanted.
 * @param DP Callback procedure which will be called once for each available element (can be nil).
 * @param NrElem Number of unique elements found.
 * @param Uptr User pointer; will be passed to the callback procedure.
 * @return Non-zero if the operation is possible, zero otherwise.
 */
GDX_FUNCPTR(gdxGetDomainElements);

typedef int  (GDX_CALLCONV *gdxCurrentDim_t) (gdxHandle_t pgdx);
/** Returns the dimension of the currently active symbol When reading or writing data, the dimension of the current active symbol is sometimes needed to convert arguments from strings to pchars (char ) etc.
 *
 * @param pgdx gdx object handle
 * @return Dimension of current active symbol.
 */
GDX_FUNCPTR(gdxCurrentDim);

typedef int  (GDX_CALLCONV *gdxRenameUEL_t) (gdxHandle_t pgdx, const char *OldName, const char *NewName);
/** Rename unique element OldName to NewName.
 *
 * @param pgdx gdx object handle
 * @param OldName Name of an existing unique element (UEL).
 * @param NewName New name for the UEL. Must not exist in UEL table yet.
 * @return Zero if the renaming was possible; non-zero is an error indicator.
 */
GDX_FUNCPTR(gdxRenameUEL);

typedef int  (GDX_CALLCONV *gdxStoreDomainSets_t) (gdxHandle_t pgdx);
/** Get flag to store one dimensional sets as potential domains, false (0) saves lots of space for large 1-dim sets that are no domains but can create inconsistent GDX files if used incorrectly. Returns 1 (true) iff. elements of 1-dim sets should be tracked for domain checking, 0 (false) otherwise.
 *
 * @param pgdx gdx object handle
 */
GDX_FUNCPTR(gdxStoreDomainSets);

typedef void (GDX_CALLCONV *gdxStoreDomainSetsSet_t) (gdxHandle_t pgdx, const int x);
GDX_FUNCPTR(gdxStoreDomainSetsSet);

typedef int  (GDX_CALLCONV *gdxAllowBogusDomains_t) (gdxHandle_t pgdx);
/** Get flag to ignore using 1-dim sets as domain when their elements are not tracked (see gdxStoreDomainSets). In case the flag is enabled this is allowing potentially unsafe writing of records to symbols with one dimensional sets as domain, when GDX has no lookup table for the elements of this set. This can happen when `gdxStoreDomainSets` was disabled by the user to save memory. For backwards compatability, this is enabled by default. Return 1 (true) iff. using a 1-dim set as domain (when store domain sets option is disabled) should be ignored. Otherwise an error is raised (ERR_NODOMAINDATA).
 *
 * @param pgdx gdx object handle
 */
GDX_FUNCPTR(gdxAllowBogusDomains);

typedef void (GDX_CALLCONV *gdxAllowBogusDomainsSet_t) (gdxHandle_t pgdx, const int x);
GDX_FUNCPTR(gdxAllowBogusDomainsSet);

typedef int  (GDX_CALLCONV *gdxMapAcronymsToNaN_t) (gdxHandle_t pgdx);
/** Flag to map all acronym values to the GAMS "Not a Number" special value. Disabled by default.
 *
 * @param pgdx gdx object handle
 */
GDX_FUNCPTR(gdxMapAcronymsToNaN);

typedef void (GDX_CALLCONV *gdxMapAcronymsToNaNSet_t) (gdxHandle_t pgdx, const int x);
GDX_FUNCPTR(gdxMapAcronymsToNaNSet);

#if defined(__cplusplus)
}
#endif
#endif /* #if ! defined(_GDXCC_H_) */

