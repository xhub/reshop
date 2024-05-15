/* dctmcc.h
 * Header file for C-style interface to the DCT library
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


#if ! defined(_DCTCC_H_)
#     define  _DCTCC_H_

#define DCTAPIVERSION 2

enum dcttypes {
  dctunknownSymType = 0,
  dctfuncSymType    = 1,
  dctsetSymType     = 2,
  dctacrSymType     = 3,
  dctparmSymType    = 4,
  dctvarSymType     = 5,
  dcteqnSymType     = 6,
  dctaliasSymType   = 127  };
#if defined(_WIN32) && defined(__GNUC__)
# include <stdio.h>
#endif


#include "gclgms.h"

#if defined(_WIN32)
# define DCT_CALLCONV __stdcall
#else
# define DCT_CALLCONV
#endif
#if defined(_WIN32)
typedef __int64 INT64;
#else
typedef signed long int INT64;
#endif

#if defined(__cplusplus)
extern "C" {
#endif

struct dctRec;
typedef struct dctRec *dctHandle_t;

typedef int (*dctErrorCallback_t) (int ErrCount, const char *msg);

/* headers for "wrapper" routines implemented in C */
/** dctGetReady: load library
 *  @return false on failure to load library, true on success 
 */
int dctGetReady  (char *msgBuf, int msgBufLen);

/** dctGetReadyD: load library from the speicified directory
 * @return false on failure to load library, true on success 
 */
int dctGetReadyD (const char *dirName, char *msgBuf, int msgBufLen);

/** dctGetReadyL: load library from the specified library name
 *  @return false on failure to load library, true on success 
 */
int dctGetReadyL (const char *libName, char *msgBuf, int msgBufLen);

/** dctCreate: load library and create dct object handle 
 *  @return false on failure to load library, true on success 
 */
int dctCreate    (dctHandle_t *pdct, char *msgBuf, int msgBufLen);

/** dctCreateD: load library from the specified directory and create dct object handle
 * @return false on failure to load library, true on success 
 */
int dctCreateD   (dctHandle_t *pdct, const char *dirName, char *msgBuf, int msgBufLen);

/** dctCreateDD: load library from the specified directory and create dct object handle
 * @return false on failure to load library, true on success 
 */
int dctCreateDD  (dctHandle_t *pdct, const char *dirName, char *msgBuf, int msgBufLen);

/** dctCreate: load library from the specified library name and create dct object handle
 * @return false on failure to load library, true on success 
 */
int dctCreateL   (dctHandle_t *pdct, const char *libName, char *msgBuf, int msgBufLen);

/** dctFree: free dct object handle 
 * @return false on failure, true on success 
 */
int dctFree      (dctHandle_t *pdct);

/** Check if library has been loaded
 * @return false on failure, true on success 
 */
int dctLibraryLoaded(void);

/** Check if library has been unloaded
 * @return false on failure, true on success 
 */
int dctLibraryUnload(void);

/** Check if API and library have the same version, Library needs to be initialized before calling this.
 * @return true  (1) on success,
 *         false (0) on failure.
 */
int  dctCorrectLibraryVersion(char *msgBuf, int msgBufLen);

int  dctGetScreenIndicator   (void);
void dctSetScreenIndicator   (int scrind);
int  dctGetExceptionIndicator(void);
void dctSetExceptionIndicator(int excind);
int  dctGetExitIndicator     (void);
void dctSetExitIndicator     (int extind);
dctErrorCallback_t dctGetErrorCallback(void);
void dctSetErrorCallback(dctErrorCallback_t func);
int  dctGetAPIErrorCount     (void);
void dctSetAPIErrorCount     (int ecnt);

void dctErrorHandling(const char *msg);
void dctInitMutexes(void);
void dctFiniMutexes(void);

#if defined(DCT_MAIN)    /* we must define some things only once */
# define DCT_FUNCPTR(NAME)  NAME##_t NAME = NULL
#else
# define DCT_FUNCPTR(NAME)  extern NAME##_t NAME
#endif

/* Prototypes for Dummy Functions */
int  DCT_CALLCONV d_dctLoadEx (dctHandle_t pdct, const char *fName, char *Msg, int Msg_i);
int  DCT_CALLCONV d_dctLoadWithHandle (dctHandle_t pdct, void *gdxptr, char *Msg, int Msg_i);
int  DCT_CALLCONV d_dctNUels (dctHandle_t pdct);
int  DCT_CALLCONV d_dctUelIndex (dctHandle_t pdct, const char *uelLabel);
int  DCT_CALLCONV d_dctUelLabel (dctHandle_t pdct, int uelIndex, char *q, char *uelLabel, int uelLabel_i);
int  DCT_CALLCONV d_dctNLSyms (dctHandle_t pdct);
int  DCT_CALLCONV d_dctSymDim (dctHandle_t pdct, int symIndex);
int  DCT_CALLCONV d_dctSymIndex (dctHandle_t pdct, const char *symName);
int  DCT_CALLCONV d_dctSymName (dctHandle_t pdct, int symIndex, char *symName, int symName_i);
int  DCT_CALLCONV d_dctSymText (dctHandle_t pdct, int symIndex, char *q, char *symTxt, int symTxt_i);
int  DCT_CALLCONV d_dctSymType (dctHandle_t pdct, int symIndex);
int  DCT_CALLCONV d_dctSymUserInfo (dctHandle_t pdct, int symIndex);
int  DCT_CALLCONV d_dctSymEntries (dctHandle_t pdct, int symIndex);
int  DCT_CALLCONV d_dctSymOffset (dctHandle_t pdct, int symIndex);
int  DCT_CALLCONV d_dctSymDomNames (dctHandle_t pdct, int symIndex, char *symDoms[], int *symDim);
int  DCT_CALLCONV d_dctSymDomIdx (dctHandle_t pdct, int symIndex, int symDomIdx[], int *symDim);
int  DCT_CALLCONV d_dctDomNameCount (dctHandle_t pdct);
int  DCT_CALLCONV d_dctDomName (dctHandle_t pdct, int domIndex, char *domName, int domName_i);
int  DCT_CALLCONV d_dctColIndex (dctHandle_t pdct, int symIndex, const int uelIndices[]);
int  DCT_CALLCONV d_dctRowIndex (dctHandle_t pdct, int symIndex, const int uelIndices[]);
int  DCT_CALLCONV d_dctColUels (dctHandle_t pdct, int j, int *symIndex, int uelIndices[], int *symDim);
int  DCT_CALLCONV d_dctRowUels (dctHandle_t pdct, int i, int *symIndex, int uelIndices[], int *symDim);
void * DCT_CALLCONV d_dctFindFirstRowCol (dctHandle_t pdct, int symIndex, const int uelIndices[], int *rcIndex);
int  DCT_CALLCONV d_dctFindNextRowCol (dctHandle_t pdct, void *findHandle, int *rcIndex);
void  DCT_CALLCONV d_dctFindClose (dctHandle_t pdct, void *findHandle);
double  DCT_CALLCONV d_dctMemUsed (dctHandle_t pdct);
void  DCT_CALLCONV d_dctSetBasicCounts (dctHandle_t pdct, int NRows, int NCols, int NBlocks);
int  DCT_CALLCONV d_dctSetBasicCountsEx (dctHandle_t pdct, int NRows, int NCols, INT64 NBlocks, char *Msg, int Msg_i);
void  DCT_CALLCONV d_dctAddUel (dctHandle_t pdct, const char *uelLabel, const char q);
void  DCT_CALLCONV d_dctAddSymbol (dctHandle_t pdct, const char *symName, int symTyp, int symDim, int userInfo, const char *symTxt);
void  DCT_CALLCONV d_dctAddSymbolData (dctHandle_t pdct, const int uelIndices[]);
int  DCT_CALLCONV d_dctAddSymbolDoms (dctHandle_t pdct, const char *symName, const char *symDoms[], int symDim, char *Msg, int Msg_i);
void  DCT_CALLCONV d_dctWriteGDX (dctHandle_t pdct, const char *fName, char *Msg);
void  DCT_CALLCONV d_dctWriteGDXWithHandle (dctHandle_t pdct, void *gdxptr, char *Msg);
int  DCT_CALLCONV d_dctNRows (dctHandle_t pdct);
int  DCT_CALLCONV d_dctNCols (dctHandle_t pdct);
int  DCT_CALLCONV d_dctLrgDim (dctHandle_t pdct);


typedef int  (DCT_CALLCONV *dctLoadEx_t) (dctHandle_t pdct, const char *fName, char *Msg, int Msg_i);
/** Read data from dictionary file specified by fName
 *
 * @param pdct dct object handle
 * @param fName Name of file
 * @param Msg Message
 */
DCT_FUNCPTR(dctLoadEx);

typedef int  (DCT_CALLCONV *dctLoadWithHandle_t) (dctHandle_t pdct, void *gdxptr, char *Msg, int Msg_i);
/** Read data from given open GDX object
 *
 * @param pdct dct object handle
 * @param gdxptr GDX handle
 * @param Msg Message
 */
DCT_FUNCPTR(dctLoadWithHandle);

typedef int  (DCT_CALLCONV *dctNUels_t) (dctHandle_t pdct);
/** Number of UELs in dictionary
 *
 * @param pdct dct object handle
 */
DCT_FUNCPTR(dctNUels);

typedef int  (DCT_CALLCONV *dctUelIndex_t) (dctHandle_t pdct, const char *uelLabel);
/** Return UEL index of uelLabel in [1..numUels], 0 on failure
 *
 * @param pdct dct object handle
 * @param uelLabel Label of unique element
 */
DCT_FUNCPTR(dctUelIndex);

typedef int  (DCT_CALLCONV *dctUelLabel_t) (dctHandle_t pdct, int uelIndex, char *q, char *uelLabel, int uelLabel_i);
/** Get UEL label associated with index: return 0 on success, 1 otherwise
 *
 * @param pdct dct object handle
 * @param uelIndex Index of unique element
 * @param q Quote character
 * @param uelLabel Label of unique element
 */
DCT_FUNCPTR(dctUelLabel);

typedef int  (DCT_CALLCONV *dctNLSyms_t) (dctHandle_t pdct);
/** Return number of symbols (variables and equations only)
 *
 * @param pdct dct object handle
 */
DCT_FUNCPTR(dctNLSyms);

typedef int  (DCT_CALLCONV *dctSymDim_t) (dctHandle_t pdct, int symIndex);
/** Return dimension of specified symbol, -1 on failure
 *
 * @param pdct dct object handle
 * @param symIndex Index of symbol
 */
DCT_FUNCPTR(dctSymDim);

typedef int  (DCT_CALLCONV *dctSymIndex_t) (dctHandle_t pdct, const char *symName);
/** Return index of symbol name in [1..numSyms], <=0 on failure
 *
 * @param pdct dct object handle
 * @param symName Name of symbol
 */
DCT_FUNCPTR(dctSymIndex);

typedef int  (DCT_CALLCONV *dctSymName_t) (dctHandle_t pdct, int symIndex, char *symName, int symName_i);
/** Get symbol name associated with index: return 0 on success, 1 otherwise
 *
 * @param pdct dct object handle
 * @param symIndex Index of symbol
 * @param symName Name of symbol
 */
DCT_FUNCPTR(dctSymName);

typedef int  (DCT_CALLCONV *dctSymText_t) (dctHandle_t pdct, int symIndex, char *q, char *symTxt, int symTxt_i);
/** Get symbol text and quote character associated with index: return 0 on success, 1 otherwise
 *
 * @param pdct dct object handle
 * @param symIndex Index of symbol
 * @param q Quote character
 * @param symTxt Explanator text of symbol
 */
DCT_FUNCPTR(dctSymText);

typedef int  (DCT_CALLCONV *dctSymType_t) (dctHandle_t pdct, int symIndex);
/** Return the symbol type associated with index, or -1 on failure
 *
 * @param pdct dct object handle
 * @param symIndex Index of symbol
 */
DCT_FUNCPTR(dctSymType);

typedef int  (DCT_CALLCONV *dctSymUserInfo_t) (dctHandle_t pdct, int symIndex);
/** Return the symbol userinfo (used for the variable or equation subtype), or -1 on failure
 *
 * @param pdct dct object handle
 * @param symIndex Index of symbol
 */
DCT_FUNCPTR(dctSymUserInfo);

typedef int  (DCT_CALLCONV *dctSymEntries_t) (dctHandle_t pdct, int symIndex);
/** Return the number of records stored for the specified symbol, or -1 on failure
 *
 * @param pdct dct object handle
 * @param symIndex Index of symbol
 */
DCT_FUNCPTR(dctSymEntries);

typedef int  (DCT_CALLCONV *dctSymOffset_t) (dctHandle_t pdct, int symIndex);
/** First row or column number for a symbol (0-based), -1 on failure
 *
 * @param pdct dct object handle
 * @param symIndex Index of symbol
 */
DCT_FUNCPTR(dctSymOffset);

typedef int  (DCT_CALLCONV *dctSymDomNames_t) (dctHandle_t pdct, int symIndex, char *symDoms[], int *symDim);
/** Get the relaxed domain names - as strings - for the specified symbol: return 0 on success, 1 otherwise
 *
 * @param pdct dct object handle
 * @param symIndex Index of symbol
 * @param symDoms 
 * @param symDim Dimension of symbol
 */
DCT_FUNCPTR(dctSymDomNames);

typedef int  (DCT_CALLCONV *dctSymDomIdx_t) (dctHandle_t pdct, int symIndex, int symDomIdx[], int *symDim);
/** Get the relaxed domain names - as 1-based indices - for the specified symbol: return 0 on success, 1 otherwise
 *
 * @param pdct dct object handle
 * @param symIndex Index of symbol
 * @param symDomIdx 
 * @param symDim Dimension of symbol
 */
DCT_FUNCPTR(dctSymDomIdx);

typedef int  (DCT_CALLCONV *dctDomNameCount_t) (dctHandle_t pdct);
/** Return the count of domain names used in the dictionary
 *
 * @param pdct dct object handle
 */
DCT_FUNCPTR(dctDomNameCount);

typedef int  (DCT_CALLCONV *dctDomName_t) (dctHandle_t pdct, int domIndex, char *domName, int domName_i);
/** Get the domain name string that goes with the specified index: return 0 on success, nonzero otherwise
 *
 * @param pdct dct object handle
 * @param domIndex 
 * @param domName 
 */
DCT_FUNCPTR(dctDomName);

typedef int  (DCT_CALLCONV *dctColIndex_t) (dctHandle_t pdct, int symIndex, const int uelIndices[]);
/** Return the column index (0-based) corresponding to the specified symbol index and UEL indices, or -1 on failure
 *
 * @param pdct dct object handle
 * @param symIndex Index of symbol
 * @param uelIndices Indices of unique element
 */
DCT_FUNCPTR(dctColIndex);

typedef int  (DCT_CALLCONV *dctRowIndex_t) (dctHandle_t pdct, int symIndex, const int uelIndices[]);
/** Return the row index (0-based) corresponding to the specified symbol index and UEL indices, or -1 on failure
 *
 * @param pdct dct object handle
 * @param symIndex Index of symbol
 * @param uelIndices Indices of unique element
 */
DCT_FUNCPTR(dctRowIndex);

typedef int  (DCT_CALLCONV *dctColUels_t) (dctHandle_t pdct, int j, int *symIndex, int uelIndices[], int *symDim);
/** Get the symbol index and dimension and the UEL indices for column j (0-based): return 0 on success, 1 otherwise
 *
 * @param pdct dct object handle
 * @param j Col index
 * @param symIndex Index of symbol
 * @param uelIndices Indices of unique element
 * @param symDim Dimension of symbol
 */
DCT_FUNCPTR(dctColUels);

typedef int  (DCT_CALLCONV *dctRowUels_t) (dctHandle_t pdct, int i, int *symIndex, int uelIndices[], int *symDim);
/** Get the symbol index and dimension and the UEL indices for row i,(0-based): return 0 on success, 1 otherwise
 *
 * @param pdct dct object handle
 * @param i Row index
 * @param symIndex Index of symbol
 * @param uelIndices Indices of unique element
 * @param symDim Dimension of symbol
 */
DCT_FUNCPTR(dctRowUels);

typedef void * (DCT_CALLCONV *dctFindFirstRowCol_t) (dctHandle_t pdct, int symIndex, const int uelIndices[], int *rcIndex);
/** Find first row/column for the specified symbol matching the uelIndices (uelIndices[k] = 0 is wildcard): return handle on success, nil otherwise. Since the routine can fail you should first check rcIndex and then the returned handle.
 *
 * @param pdct dct object handle
 * @param symIndex Index of symbol
 * @param uelIndices Indices of unique element
 * @param rcIndex Row or column index
 */
DCT_FUNCPTR(dctFindFirstRowCol);

typedef int  (DCT_CALLCONV *dctFindNextRowCol_t) (dctHandle_t pdct, void *findHandle, int *rcIndex);
/** Find next row/column in the search defined by findHandle (see dctFindFirstRowCol): return 1 on success, 0 on failure
 *
 * @param pdct dct object handle
 * @param findHandle Handle obtained when starting to search. Can be used to further search and terminate search
 * @param rcIndex Row or column index
 */
DCT_FUNCPTR(dctFindNextRowCol);

typedef void  (DCT_CALLCONV *dctFindClose_t) (dctHandle_t pdct, void *findHandle);
/** Close the findHandle obtained from dctFindFirstRowCol
 *
 * @param pdct dct object handle
 * @param findHandle Handle obtained when starting to search. Can be used to further search and terminate search
 */
DCT_FUNCPTR(dctFindClose);

typedef double  (DCT_CALLCONV *dctMemUsed_t) (dctHandle_t pdct);
/** Memory used by the object in MB (1,000,000 bytes)
 *
 * @param pdct dct object handle
 */
DCT_FUNCPTR(dctMemUsed);

typedef void  (DCT_CALLCONV *dctSetBasicCounts_t) (dctHandle_t pdct, int NRows, int NCols, int NBlocks);
/** Initialization
 *
 * @param pdct dct object handle
 * @param NRows Number of rows
 * @param NCols Number of columns
 * @param NBlocks Number of UELs in all equations and variables (dim*nrrec per symbol)
 */
DCT_FUNCPTR(dctSetBasicCounts);

typedef int  (DCT_CALLCONV *dctSetBasicCountsEx_t) (dctHandle_t pdct, int NRows, int NCols, INT64 NBlocks, char *Msg, int Msg_i);
/** Initialization
 *
 * @param pdct dct object handle
 * @param NRows Number of rows
 * @param NCols Number of columns
 * @param NBlocks Number of UELs in all equations and variables (dim*nrrec per symbol)
 * @param Msg Message
 */
DCT_FUNCPTR(dctSetBasicCountsEx);

typedef void  (DCT_CALLCONV *dctAddUel_t) (dctHandle_t pdct, const char *uelLabel, const char q);
/** Add UEL
 *
 * @param pdct dct object handle
 * @param uelLabel Label of unique element
 * @param q Quote character
 */
DCT_FUNCPTR(dctAddUel);

typedef void  (DCT_CALLCONV *dctAddSymbol_t) (dctHandle_t pdct, const char *symName, int symTyp, int symDim, int userInfo, const char *symTxt);
/** Add symbol
 *
 * @param pdct dct object handle
 * @param symName Name of symbol
 * @param symTyp type of symbol (see enumerated constants)
 * @param symDim Dimension of symbol
 * @param userInfo 
 * @param symTxt Explanator text of symbol
 */
DCT_FUNCPTR(dctAddSymbol);

typedef void  (DCT_CALLCONV *dctAddSymbolData_t) (dctHandle_t pdct, const int uelIndices[]);
/** Add symbol data
 *
 * @param pdct dct object handle
 * @param uelIndices Indices of unique element
 */
DCT_FUNCPTR(dctAddSymbolData);

typedef int  (DCT_CALLCONV *dctAddSymbolDoms_t) (dctHandle_t pdct, const char *symName, const char *symDoms[], int symDim, char *Msg, int Msg_i);
/** Add symbol domains
 *
 * @param pdct dct object handle
 * @param symName Name of symbol
 * @param symDoms 
 * @param symDim Dimension of symbol
 * @param Msg Message
 */
DCT_FUNCPTR(dctAddSymbolDoms);

typedef void  (DCT_CALLCONV *dctWriteGDX_t) (dctHandle_t pdct, const char *fName, char *Msg);
/** Write dictionary file in GDX format
 *
 * @param pdct dct object handle
 * @param fName Name of file
 * @param Msg Message
 */
DCT_FUNCPTR(dctWriteGDX);

typedef void  (DCT_CALLCONV *dctWriteGDXWithHandle_t) (dctHandle_t pdct, void *gdxptr, char *Msg);
/** Write dictionary to an open GDX object
 *
 * @param pdct dct object handle
 * @param gdxptr GDX handle
 * @param Msg Message
 */
DCT_FUNCPTR(dctWriteGDXWithHandle);

typedef int  (DCT_CALLCONV *dctNRows_t) (dctHandle_t pdct);
/** Number of rows
 *
 * @param pdct dct object handle
 */
DCT_FUNCPTR(dctNRows);

typedef int  (DCT_CALLCONV *dctNCols_t) (dctHandle_t pdct);
/** Number of columns
 *
 * @param pdct dct object handle
 */
DCT_FUNCPTR(dctNCols);

typedef int  (DCT_CALLCONV *dctLrgDim_t) (dctHandle_t pdct);
/** Largest dimension of any symbol
 *
 * @param pdct dct object handle
 */
DCT_FUNCPTR(dctLrgDim);

#if defined(__cplusplus)
}
#endif
#endif /* #if ! defined(_DCTCC_H_) */

