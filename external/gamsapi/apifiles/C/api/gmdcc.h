/* gmdcc.h
 * Header file for C-style interface to the GMD library
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


#if ! defined(_GMDCC_H_)
#     define  _GMDCC_H_

#define GMDAPIVERSION 5

enum gmdActionType {
  GMD_PARAM  = 0,
  GMD_UPPER  = 1,
  GMD_LOWER  = 2,
  GMD_FIXED  = 3,
  GMD_PRIMAL = 4,
  GMD_DUAL   = 5  };

enum gmdUpdateType {
  GMD_DEFAULT    = 0,
  GMD_BASECASE   = 1,
  GMD_ACCUMULATE = 2  };

enum gmdInfoX {
  GMD_NRSYMBOLS            = 0,
  GMD_NRUELS               = 1,
  GMD_NRSYMBOLSWITHALIAS   = 2,
  GMD_DEFAULT_STORAGE_TYPE = 3,
  GMD_INITEXTERN           = 4  };

enum gmdSymInfo {
  GMD_NAME      = 0,
  GMD_DIM       = 1,
  GMD_TYPE      = 2,
  GMD_NRRECORDS = 3,
  GMD_USERINFO  = 4,
  GMD_EXPLTEXT  = 5,
  GMD_NUMBER    = 6,
  GMD_WRITTENTO = 7  };

enum gmdStorageTypes {
  GMD_RB_TREE  = 0,
  GMD_VECTOR   = 1,
  GMD_GMS_TREE = 2  };

#include "gclgms.h"

#if defined(_WIN32)
# define GMD_CALLCONV __stdcall
#else
# define GMD_CALLCONV
#endif

#if defined(__cplusplus)
extern "C" {
#endif

struct gmdRec;
typedef struct gmdRec *gmdHandle_t;

typedef int (*gmdErrorCallback_t) (int ErrCount, const char *msg);

/* headers for "wrapper" routines implemented in C */
/** gmdGetReady: load library
 *  @return false on failure to load library, true on success 
 */
int gmdGetReady  (char *msgBuf, int msgBufLen);

/** gmdGetReadyD: load library from the speicified directory
 * @return false on failure to load library, true on success 
 */
int gmdGetReadyD (const char *dirName, char *msgBuf, int msgBufLen);

/** gmdGetReadyL: load library from the specified library name
 *  @return false on failure to load library, true on success 
 */
int gmdGetReadyL (const char *libName, char *msgBuf, int msgBufLen);

/** gmdCreate: load library and create gmd object handle 
 *  @return false on failure to load library, true on success 
 */
int gmdCreate    (gmdHandle_t *pgmd, char *msgBuf, int msgBufLen);

/** gmdCreateD: load library from the specified directory and create gmd object handle
 * @return false on failure to load library, true on success 
 */
int gmdCreateD   (gmdHandle_t *pgmd, const char *dirName, char *msgBuf, int msgBufLen);

/** gmdCreateDD: load library from the specified directory and create gmd object handle
 * @return false on failure to load library, true on success 
 */
int gmdCreateDD  (gmdHandle_t *pgmd, const char *dirName, char *msgBuf, int msgBufLen);

/** gmdCreate: load library from the specified library name and create gmd object handle
 * @return false on failure to load library, true on success 
 */
int gmdCreateL   (gmdHandle_t *pgmd, const char *libName, char *msgBuf, int msgBufLen);

/** gmdFree: free gmd object handle 
 * @return false on failure, true on success 
 */
int gmdFree      (gmdHandle_t *pgmd);

/** Check if library has been loaded
 * @return false on failure, true on success 
 */
int gmdLibraryLoaded(void);

/** Check if library has been unloaded
 * @return false on failure, true on success 
 */
int gmdLibraryUnload(void);

/** Check if API and library have the same version, Library needs to be initialized before calling this.
 * @return true  (1) on success,
 *         false (0) on failure.
 */
int  gmdCorrectLibraryVersion(char *msgBuf, int msgBufLen);

int  gmdGetScreenIndicator   (void);
void gmdSetScreenIndicator   (int scrind);
int  gmdGetExceptionIndicator(void);
void gmdSetExceptionIndicator(int excind);
int  gmdGetExitIndicator     (void);
void gmdSetExitIndicator     (int extind);
gmdErrorCallback_t gmdGetErrorCallback(void);
void gmdSetErrorCallback(gmdErrorCallback_t func);
int  gmdGetAPIErrorCount     (void);
void gmdSetAPIErrorCount     (int ecnt);

void gmdErrorHandling(const char *msg);
void gmdInitMutexes(void);
void gmdFiniMutexes(void);

#if defined(GMD_MAIN)    /* we must define some things only once */
# define GMD_FUNCPTR(NAME)  NAME##_t NAME = NULL
#else
# define GMD_FUNCPTR(NAME)  extern NAME##_t NAME
#endif

/* function typedefs and pointer definitions */
typedef int (GMD_CALLCONV *TFindSymbol_t) (const char *syId, int *syNr, void *usrmem);
typedef int (GMD_CALLCONV *TDataReadRawStart_t) (int syNr, int *nrRecs, void *usrmem);
typedef int (GMD_CALLCONV *TDataReadRaw_t) (int keyInt[], double values[], int *dimFirst, void *usrmem);
typedef int (GMD_CALLCONV *TDataReadDone_t) (void *usrmem);
typedef int (GMD_CALLCONV *TGetElemText_t) (int eTextNr, char *eText, int eTextSize, void *usrmem);
typedef int (GMD_CALLCONV *TPrintLog_t) (const char *msg, void *usrmem);

/* Prototypes for Dummy Functions */
int  GMD_CALLCONV d_gmdInitFromGDX (gmdHandle_t pgmd, const char *fileName);
int  GMD_CALLCONV d_gmdInitFromDict (gmdHandle_t pgmd, void *gmoPtr);
int  GMD_CALLCONV d_gmdInitFromCMEX (gmdHandle_t pgmd, TFindSymbol_t findSymbol, TDataReadRawStart_t dataReadRawStart, TDataReadRaw_t dataReadRaw, TDataReadDone_t dataReadDone, TGetElemText_t getElemText, TPrintLog_t printLog, void *usrmem);
int  GMD_CALLCONV d_gmdInitFromDB (gmdHandle_t pgmd, void *gmdSrcPtr);
int  GMD_CALLCONV d_gmdRegisterGMO (gmdHandle_t pgmd, void *gmoPtr);
int  GMD_CALLCONV d_gmdCloseGDX (gmdHandle_t pgmd, int loadRemain);
int  GMD_CALLCONV d_gmdAddSymbolX (gmdHandle_t pgmd, const char *symName, int aDim, int stype, int userInfo, const char *explText, void **vDomPtrIn, const char *keyStr[], void **symPtr);
void * GMD_CALLCONV d_gmdAddSymbolXPy (gmdHandle_t pgmd, const char *symName, int aDim, int stype, int userInfo, const char *explText, void **vDomPtrIn, const char *keyStr[], int *status);
int  GMD_CALLCONV d_gmdAddSymbol (gmdHandle_t pgmd, const char *symName, int aDim, int stype, int userInfo, const char *explText, void **symPtr);
void * GMD_CALLCONV d_gmdAddSymbolPy (gmdHandle_t pgmd, const char *symName, int aDim, int stype, int userInfo, const char *explText, int *status);
int  GMD_CALLCONV d_gmdFindSymbol (gmdHandle_t pgmd, const char *symName, void **symPtr);
void * GMD_CALLCONV d_gmdFindSymbolPy (gmdHandle_t pgmd, const char *symName, int *status);
int  GMD_CALLCONV d_gmdFindSymbolWithAlias (gmdHandle_t pgmd, const char *symName, void **symPtr);
void * GMD_CALLCONV d_gmdFindSymbolWithAliasPy (gmdHandle_t pgmd, const char *symName, int *status);
int  GMD_CALLCONV d_gmdGetSymbolByIndex (gmdHandle_t pgmd, int idx, void **symPtr);
void * GMD_CALLCONV d_gmdGetSymbolByIndexPy (gmdHandle_t pgmd, int idx, int *status);
int  GMD_CALLCONV d_gmdGetSymbolByNumber (gmdHandle_t pgmd, int idx, void **symPtr);
void * GMD_CALLCONV d_gmdGetSymbolByNumberPy (gmdHandle_t pgmd, int idx, int *status);
int  GMD_CALLCONV d_gmdClearSymbol (gmdHandle_t pgmd, void *symPtr);
int  GMD_CALLCONV d_gmdCopySymbol (gmdHandle_t pgmd, void *tarSymPtr, void *srcSymPtr);
int  GMD_CALLCONV d_gmdFindRecord (gmdHandle_t pgmd, void *symPtr, const char *keyStr[], void **symIterPtr);
void * GMD_CALLCONV d_gmdFindRecordPy (gmdHandle_t pgmd, void *symPtr, const char *keyStr[], int *status);
int  GMD_CALLCONV d_gmdFindFirstRecord (gmdHandle_t pgmd, void *symPtr, void **symIterPtr);
void * GMD_CALLCONV d_gmdFindFirstRecordPy (gmdHandle_t pgmd, void *symPtr, int *status);
int  GMD_CALLCONV d_gmdFindFirstRecordSlice (gmdHandle_t pgmd, void *symPtr, const char *keyStr[], void **symIterPtr);
void * GMD_CALLCONV d_gmdFindFirstRecordSlicePy (gmdHandle_t pgmd, void *symPtr, const char *keyStr[], int *status);
int  GMD_CALLCONV d_gmdFindLastRecord (gmdHandle_t pgmd, void *symPtr, void **symIterPtr);
void * GMD_CALLCONV d_gmdFindLastRecordPy (gmdHandle_t pgmd, void *symPtr, int *status);
int  GMD_CALLCONV d_gmdFindLastRecordSlice (gmdHandle_t pgmd, void *symPtr, const char *keyStr[], void **symIterPtr);
void * GMD_CALLCONV d_gmdFindLastRecordSlicePy (gmdHandle_t pgmd, void *symPtr, const char *keyStr[], int *status);
int  GMD_CALLCONV d_gmdRecordMoveNext (gmdHandle_t pgmd, void *symIterPtr);
int  GMD_CALLCONV d_gmdRecordHasNext (gmdHandle_t pgmd, void *symIterPtr);
int  GMD_CALLCONV d_gmdRecordMovePrev (gmdHandle_t pgmd, void *symIterPtr);
int  GMD_CALLCONV d_gmdSameRecord (gmdHandle_t pgmd, void *symIterPtrSrc, void *symIterPtrtar);
int  GMD_CALLCONV d_gmdRecordHasPrev (gmdHandle_t pgmd, void *symIterPtr);
int  GMD_CALLCONV d_gmdGetElemText (gmdHandle_t pgmd, void *symIterPtr, char *txt);
int  GMD_CALLCONV d_gmdGetLevel (gmdHandle_t pgmd, void *symIterPtr, double *value);
int  GMD_CALLCONV d_gmdGetLower (gmdHandle_t pgmd, void *symIterPtr, double *value);
int  GMD_CALLCONV d_gmdGetUpper (gmdHandle_t pgmd, void *symIterPtr, double *value);
int  GMD_CALLCONV d_gmdGetMarginal (gmdHandle_t pgmd, void *symIterPtr, double *value);
int  GMD_CALLCONV d_gmdGetScale (gmdHandle_t pgmd, void *symIterPtr, double *value);
int  GMD_CALLCONV d_gmdSetElemText (gmdHandle_t pgmd, void *symIterPtr, const char *txt);
int  GMD_CALLCONV d_gmdSetLevel (gmdHandle_t pgmd, void *symIterPtr, double value);
int  GMD_CALLCONV d_gmdSetLower (gmdHandle_t pgmd, void *symIterPtr, double value);
int  GMD_CALLCONV d_gmdSetUpper (gmdHandle_t pgmd, void *symIterPtr, double value);
int  GMD_CALLCONV d_gmdSetMarginal (gmdHandle_t pgmd, void *symIterPtr, double value);
int  GMD_CALLCONV d_gmdSetScale (gmdHandle_t pgmd, void *symIterPtr, double value);
int  GMD_CALLCONV d_gmdSetUserInfo (gmdHandle_t pgmd, void *symPtr, int value);
int  GMD_CALLCONV d_gmdAddRecord (gmdHandle_t pgmd, void *symPtr, const char *keyStr[], void **symIterPtr);
void * GMD_CALLCONV d_gmdAddRecordPy (gmdHandle_t pgmd, void *symPtr, const char *keyStr[], int *status);
int  GMD_CALLCONV d_gmdMergeRecord (gmdHandle_t pgmd, void *symPtr, const char *keyStr[], void **symIterPtr);
void * GMD_CALLCONV d_gmdMergeRecordPy (gmdHandle_t pgmd, void *symPtr, const char *keyStr[], int *status);
int  GMD_CALLCONV d_gmdMergeRecordInt (gmdHandle_t pgmd, void *symPtr, const int keyInt[], int checkUEL, int wantSymIterPtr, void **symIterPtr, int haveValues, const double values[]);
void * GMD_CALLCONV d_gmdMergeRecordIntPy (gmdHandle_t pgmd, void *symPtr, const int keyInt[], int checkUEL, int wantSymIterPtr, int haveValues, const double values[], int *status);
int  GMD_CALLCONV d_gmdMergeSetRecordInt (gmdHandle_t pgmd, void *symPtr, const int keyInt[], int checkUEL, int wantSymIterPtr, void **symIterPtr, const char *eText);
void * GMD_CALLCONV d_gmdMergeSetRecordIntPy (gmdHandle_t pgmd, void *symPtr, const int keyInt[], int checkUEL, int wantSymIterPtr, const char *eText, int *status);
int  GMD_CALLCONV d_gmdAddRecordRaw (gmdHandle_t pgmd, void *symPtr, const int keyInt[], const double values[], const char *eText);
int  GMD_CALLCONV d_gmdDeleteRecord (gmdHandle_t pgmd, void *symIterPtr);
int  GMD_CALLCONV d_gmdGetRecordRaw (gmdHandle_t pgmd, void *symIterPtr, int aDim, int keyInt[], double values[]);
int  GMD_CALLCONV d_gmdGetKeys (gmdHandle_t pgmd, void *symIterPtr, int aDim, char *keyStr[]);
int  GMD_CALLCONV d_gmdGetKey (gmdHandle_t pgmd, void *symIterPtr, int idx, char *keyStr);
int  GMD_CALLCONV d_gmdGetDomain (gmdHandle_t pgmd, void *symPtr, int aDim, void **vDomPtrOut, char *keyStr[]);
int  GMD_CALLCONV d_gmdCopySymbolIterator (gmdHandle_t pgmd, void *symIterPtrSrc, void **symIterPtrtar);
void * GMD_CALLCONV d_gmdCopySymbolIteratorPy (gmdHandle_t pgmd, void *symIterPtrSrc, int *status);
int  GMD_CALLCONV d_gmdFreeSymbolIterator (gmdHandle_t pgmd, void *symIterPtr);
int  GMD_CALLCONV d_gmdMergeUel (gmdHandle_t pgmd, const char *uel, int *uelNr);
int  GMD_CALLCONV d_gmdGetUelByIndex (gmdHandle_t pgmd, int uelNr, char *keyStr);
int  GMD_CALLCONV d_gmdFindUel (gmdHandle_t pgmd, const char *uelLabel, int *uelNr);
int  GMD_CALLCONV d_gmdGetSymbolsUels (gmdHandle_t pgmd, void **vDomPtrIn, int lenvDomPtrIn, int uelList[], int sizeUelList);
int  GMD_CALLCONV d_gmdInfo (gmdHandle_t pgmd, int infoKey, int *ival, double *dval, char *sval);
int  GMD_CALLCONV d_gmdSymbolInfo (gmdHandle_t pgmd, void *symPtr, int infoKey, int *ival, double *dval, char *sval);
int  GMD_CALLCONV d_gmdSymbolDim (gmdHandle_t pgmd, void *symPtr, int *aDim);
int  GMD_CALLCONV d_gmdSymbolType (gmdHandle_t pgmd, void *symPtr, int *stype);
int  GMD_CALLCONV d_gmdWriteGDX (gmdHandle_t pgmd, const char *fileName, int noDomChk);
int  GMD_CALLCONV d_gmdSetSpecialValuesX (gmdHandle_t pgmd, const double specVal[], int *specValType);
int  GMD_CALLCONV d_gmdSetSpecialValues (gmdHandle_t pgmd, const double specVal[]);
int  GMD_CALLCONV d_gmdGetSpecialValues (gmdHandle_t pgmd, double specVal[]);
int  GMD_CALLCONV d_gmdGetUserSpecialValues (gmdHandle_t pgmd, double specVal[]);
int  GMD_CALLCONV d_gmdSetDebug (gmdHandle_t pgmd, int debugLevel);
int  GMD_CALLCONV d_gmdGetLastError (gmdHandle_t pgmd, char *msg);
int  GMD_CALLCONV d_gmdPrintLog (gmdHandle_t pgmd, const char *msg);
int  GMD_CALLCONV d_gmdStartWriteRecording (gmdHandle_t pgmd);
int  GMD_CALLCONV d_gmdStopWriteRecording (gmdHandle_t pgmd);
int  GMD_CALLCONV d_gmdCheckDBDV (gmdHandle_t pgmd, int *dv);
int  GMD_CALLCONV d_gmdCheckSymbolDV (gmdHandle_t pgmd, void *symPtr, int *dv);
int  GMD_CALLCONV d_gmdGetFirstDBDV (gmdHandle_t pgmd, void **dvHandle);
void * GMD_CALLCONV d_gmdGetFirstDBDVPy (gmdHandle_t pgmd, int *status);
int  GMD_CALLCONV d_gmdGetFirstDVInSymbol (gmdHandle_t pgmd, void *symPtr, void **dvHandle);
void * GMD_CALLCONV d_gmdGetFirstDVInSymbolPy (gmdHandle_t pgmd, void *symPtr, int *status);
int  GMD_CALLCONV d_gmdDomainCheckDone (gmdHandle_t pgmd);
int  GMD_CALLCONV d_gmdGetFirstDVInNextSymbol (gmdHandle_t pgmd, void *dvHandle, int *nextavail);
int  GMD_CALLCONV d_gmdMoveNextDVInSymbol (gmdHandle_t pgmd, void *dvHandle, int *nextavail);
int  GMD_CALLCONV d_gmdFreeDVHandle (gmdHandle_t pgmd, void *dvHandle);
int  GMD_CALLCONV d_gmdGetDVSymbol (gmdHandle_t pgmd, void *dvHandle, void **symPtr);
void * GMD_CALLCONV d_gmdGetDVSymbolPy (gmdHandle_t pgmd, void *dvHandle, int *status);
int  GMD_CALLCONV d_gmdGetDVSymbolRecord (gmdHandle_t pgmd, void *dvHandle, void **symIterPtr);
void * GMD_CALLCONV d_gmdGetDVSymbolRecordPy (gmdHandle_t pgmd, void *dvHandle, int *status);
int  GMD_CALLCONV d_gmdGetDVIndicator (gmdHandle_t pgmd, void *dvHandle, int viol[]);
int  GMD_CALLCONV d_gmdInitUpdate (gmdHandle_t pgmd, void *gmoPtr);
int  GMD_CALLCONV d_gmdUpdateModelSymbol (gmdHandle_t pgmd, void *gamsSymPtr, int actionType, void *dataSymPtr, int updateType, int *noMatchCnt);
int  GMD_CALLCONV d_gmdCallSolver (gmdHandle_t pgmd, const char *solvername);
int  GMD_CALLCONV d_gmdCallSolverTimed (gmdHandle_t pgmd, const char *solvername, double *time);
int  GMD_CALLCONV d_gmdDenseSymbolToDenseArray (gmdHandle_t pgmd, void *cube, int vDim[], void *symPtr, int field);
int  GMD_CALLCONV d_gmdSparseSymbolToDenseArray (gmdHandle_t pgmd, void *cube, int vDim[], void *symPtr, void **vDomPtr, int field, int *nDropped);
int  GMD_CALLCONV d_gmdSparseSymbolToSqzdArray (gmdHandle_t pgmd, void *cube, int vDim[], void *symPtr, void **vDomSqueezePtr, void **vDomPtr, int field, int *nDropped);
int  GMD_CALLCONV d_gmdDenseArrayToSymbol (gmdHandle_t pgmd, void *symPtr, void **vDomPtr, void *cube, int vDim[]);
int  GMD_CALLCONV d_gmdDenseArraySlicesToSymbol (gmdHandle_t pgmd, void *symPtr, void **vDomSlicePtr, void **vDomPtr, void *cube, int vDim[]);
int  GMD_CALLCONV d_gmdSelectRecordStorage (gmdHandle_t pgmd, void **symPtr, int storageType);
void * GMD_CALLCONV d_gmdSelectRecordStoragePy (gmdHandle_t pgmd, void *symPtr, int storageType, int *status);


typedef int  (GMD_CALLCONV *gmdInitFromGDX_t) (gmdHandle_t pgmd, const char *fileName);
/** Open a GDX file for data reading
 *
 * @param pgmd gmd object handle
 * @param fileName File name of the gdx file
 */
GMD_FUNCPTR(gmdInitFromGDX);

typedef int  (GMD_CALLCONV *gmdInitFromDict_t) (gmdHandle_t pgmd, void *gmoPtr);
/** Initialize from model dictionary
 *
 * @param pgmd gmd object handle
 * @param gmoPtr Handle to GMO object
 */
GMD_FUNCPTR(gmdInitFromDict);

typedef int  (GMD_CALLCONV *gmdInitFromCMEX_t) (gmdHandle_t pgmd, TFindSymbol_t findSymbol, TDataReadRawStart_t dataReadRawStart, TDataReadRaw_t dataReadRaw, TDataReadDone_t dataReadDone, TGetElemText_t getElemText, TPrintLog_t printLog, void *usrmem);
/** Initialize from CMEX embedded code
 *
 * @param pgmd gmd object handle
 * @param findSymbol 
 * @param dataReadRawStart 
 * @param dataReadRaw 
 * @param dataReadDone 
 * @param getElemText 
 * @param printLog 
 * @param usrmem 
 */
GMD_FUNCPTR(gmdInitFromCMEX);

typedef int  (GMD_CALLCONV *gmdInitFromDB_t) (gmdHandle_t pgmd, void *gmdSrcPtr);
/** Initialize from another database
 *
 * @param pgmd gmd object handle
 * @param gmdSrcPtr Handle to source database
 */
GMD_FUNCPTR(gmdInitFromDB);

typedef int  (GMD_CALLCONV *gmdRegisterGMO_t) (gmdHandle_t pgmd, void *gmoPtr);
/** Register GMO object alternative in connection with gmdInitFromDB to gmdInitFromDict
 *
 * @param pgmd gmd object handle
 * @param gmoPtr Handle to GMO object
 */
GMD_FUNCPTR(gmdRegisterGMO);

typedef int  (GMD_CALLCONV *gmdCloseGDX_t) (gmdHandle_t pgmd, int loadRemain);
/** Close GDX file and loads all unloaded symbols on request
 *
 * @param pgmd gmd object handle
 * @param loadRemain Switch for forcing the load of all unloaded symbol when closing GDX
 */
GMD_FUNCPTR(gmdCloseGDX);

typedef int  (GMD_CALLCONV *gmdAddSymbolX_t) (gmdHandle_t pgmd, const char *symName, int aDim, int stype, int userInfo, const char *explText, void **vDomPtrIn, const char *keyStr[], void **symPtr);
/** Add a new symbol with domain info to the gmd object
 *
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
GMD_FUNCPTR(gmdAddSymbolX);

typedef void * (GMD_CALLCONV *gmdAddSymbolXPy_t) (gmdHandle_t pgmd, const char *symName, int aDim, int stype, int userInfo, const char *explText, void **vDomPtrIn, const char *keyStr[], int *status);
/** Add a new symbol with domain info to the gmd object (Python)
 *
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
GMD_FUNCPTR(gmdAddSymbolXPy);

typedef int  (GMD_CALLCONV *gmdAddSymbol_t) (gmdHandle_t pgmd, const char *symName, int aDim, int stype, int userInfo, const char *explText, void **symPtr);
/** Add a new symbol to the gmd object
 *
 * @param pgmd gmd object handle
 * @param symName name of the symbol
 * @param aDim dimension of the symbol
 * @param stype type of a symbol
 * @param userInfo user info integer
 * @param explText explanatory text
 * @param symPtr Handle to a symbol when reading sequentially
 */
GMD_FUNCPTR(gmdAddSymbol);

typedef void * (GMD_CALLCONV *gmdAddSymbolPy_t) (gmdHandle_t pgmd, const char *symName, int aDim, int stype, int userInfo, const char *explText, int *status);
/** Add a new symbol to the gmd object (Python)
 *
 * @param pgmd gmd object handle
 * @param symName name of the symbol
 * @param aDim dimension of the symbol
 * @param stype type of a symbol
 * @param userInfo user info integer
 * @param explText explanatory text
 * @param status Indicator for error free execution (true/1 means OK, false/0 means Error)
 */
GMD_FUNCPTR(gmdAddSymbolPy);

typedef int  (GMD_CALLCONV *gmdFindSymbol_t) (gmdHandle_t pgmd, const char *symName, void **symPtr);
/** Find symbol
 *
 * @param pgmd gmd object handle
 * @param symName name of the symbol
 * @param symPtr Handle to a symbol when reading sequentially
 */
GMD_FUNCPTR(gmdFindSymbol);

typedef void * (GMD_CALLCONV *gmdFindSymbolPy_t) (gmdHandle_t pgmd, const char *symName, int *status);
/** Find symbol (Python)
 *
 * @param pgmd gmd object handle
 * @param symName name of the symbol
 * @param status Indicator for error free execution (true/1 means OK, false/0 means Error)
 */
GMD_FUNCPTR(gmdFindSymbolPy);

typedef int  (GMD_CALLCONV *gmdFindSymbolWithAlias_t) (gmdHandle_t pgmd, const char *symName, void **symPtr);
/** Find symbol including aliases
 *
 * @param pgmd gmd object handle
 * @param symName name of the symbol
 * @param symPtr Handle to a symbol when reading sequentially
 */
GMD_FUNCPTR(gmdFindSymbolWithAlias);

typedef void * (GMD_CALLCONV *gmdFindSymbolWithAliasPy_t) (gmdHandle_t pgmd, const char *symName, int *status);
/** Find symbol including aliases (Python)
 *
 * @param pgmd gmd object handle
 * @param symName name of the symbol
 * @param status Indicator for error free execution (true/1 means OK, false/0 means Error)
 */
GMD_FUNCPTR(gmdFindSymbolWithAliasPy);

typedef int  (GMD_CALLCONV *gmdGetSymbolByIndex_t) (gmdHandle_t pgmd, int idx, void **symPtr);
/** Find symbol by index position
 *
 * @param pgmd gmd object handle
 * @param idx index of a symbol in the GMD object / of a key from a record
 * @param symPtr Handle to a symbol when reading sequentially
 */
GMD_FUNCPTR(gmdGetSymbolByIndex);

typedef void * (GMD_CALLCONV *gmdGetSymbolByIndexPy_t) (gmdHandle_t pgmd, int idx, int *status);
/** Find symbol by index position (Python)
 *
 * @param pgmd gmd object handle
 * @param idx index of a symbol in the GMD object / of a key from a record
 * @param status Indicator for error free execution (true/1 means OK, false/0 means Error)
 */
GMD_FUNCPTR(gmdGetSymbolByIndexPy);

typedef int  (GMD_CALLCONV *gmdGetSymbolByNumber_t) (gmdHandle_t pgmd, int idx, void **symPtr);
/** Find symbol by number position this includes the alias symbols and used GMD_NUMBER
 *
 * @param pgmd gmd object handle
 * @param idx index of a symbol in the GMD object / of a key from a record
 * @param symPtr Handle to a symbol when reading sequentially
 */
GMD_FUNCPTR(gmdGetSymbolByNumber);

typedef void * (GMD_CALLCONV *gmdGetSymbolByNumberPy_t) (gmdHandle_t pgmd, int idx, int *status);
/** Find symbol by number position this includes the alias symbols and used GMD_NUMBER (Python)
 *
 * @param pgmd gmd object handle
 * @param idx index of a symbol in the GMD object / of a key from a record
 * @param status Indicator for error free execution (true/1 means OK, false/0 means Error)
 */
GMD_FUNCPTR(gmdGetSymbolByNumberPy);

typedef int  (GMD_CALLCONV *gmdClearSymbol_t) (gmdHandle_t pgmd, void *symPtr);
/** Deletes all record of a symbol
 *
 * @param pgmd gmd object handle
 * @param symPtr Handle to a symbol when reading sequentially
 */
GMD_FUNCPTR(gmdClearSymbol);

typedef int  (GMD_CALLCONV *gmdCopySymbol_t) (gmdHandle_t pgmd, void *tarSymPtr, void *srcSymPtr);
/** Clear target symbol and copies all record from source symbol
 *
 * @param pgmd gmd object handle
 * @param tarSymPtr Handle to target symbol
 * @param srcSymPtr Handle to source symbol
 */
GMD_FUNCPTR(gmdCopySymbol);

typedef int  (GMD_CALLCONV *gmdFindRecord_t) (gmdHandle_t pgmd, void *symPtr, const char *keyStr[], void **symIterPtr);
/** Find data record of a symbol
 *
 * @param pgmd gmd object handle
 * @param symPtr Handle to a symbol when reading sequentially
 * @param keyStr Array of strings containing the unique elements
 * @param symIterPtr Handle to a SymbolIterator
 */
GMD_FUNCPTR(gmdFindRecord);

typedef void * (GMD_CALLCONV *gmdFindRecordPy_t) (gmdHandle_t pgmd, void *symPtr, const char *keyStr[], int *status);
/** Find data record of a symbol (Python)
 *
 * @param pgmd gmd object handle
 * @param symPtr Handle to a symbol when reading sequentially
 * @param keyStr Array of strings containing the unique elements
 * @param status Indicator for error free execution (true/1 means OK, false/0 means Error)
 */
GMD_FUNCPTR(gmdFindRecordPy);

typedef int  (GMD_CALLCONV *gmdFindFirstRecord_t) (gmdHandle_t pgmd, void *symPtr, void **symIterPtr);
/** Receive the first record of a symbol
 *
 * @param pgmd gmd object handle
 * @param symPtr Handle to a symbol when reading sequentially
 * @param symIterPtr Handle to a SymbolIterator
 */
GMD_FUNCPTR(gmdFindFirstRecord);

typedef void * (GMD_CALLCONV *gmdFindFirstRecordPy_t) (gmdHandle_t pgmd, void *symPtr, int *status);
/** Receive the first record of a symbol (Python)
 *
 * @param pgmd gmd object handle
 * @param symPtr Handle to a symbol when reading sequentially
 * @param status Indicator for error free execution (true/1 means OK, false/0 means Error)
 */
GMD_FUNCPTR(gmdFindFirstRecordPy);

typedef int  (GMD_CALLCONV *gmdFindFirstRecordSlice_t) (gmdHandle_t pgmd, void *symPtr, const char *keyStr[], void **symIterPtr);
/** Receive the first record of a symbol using the slice definition
 *
 * @param pgmd gmd object handle
 * @param symPtr Handle to a symbol when reading sequentially
 * @param keyStr Array of strings containing the unique elements
 * @param symIterPtr Handle to a SymbolIterator
 */
GMD_FUNCPTR(gmdFindFirstRecordSlice);

typedef void * (GMD_CALLCONV *gmdFindFirstRecordSlicePy_t) (gmdHandle_t pgmd, void *symPtr, const char *keyStr[], int *status);
/** Receive the first record of a symbol using the slice definition (Python)
 *
 * @param pgmd gmd object handle
 * @param symPtr Handle to a symbol when reading sequentially
 * @param keyStr Array of strings containing the unique elements
 * @param status Indicator for error free execution (true/1 means OK, false/0 means Error)
 */
GMD_FUNCPTR(gmdFindFirstRecordSlicePy);

typedef int  (GMD_CALLCONV *gmdFindLastRecord_t) (gmdHandle_t pgmd, void *symPtr, void **symIterPtr);
/** Receive the last record of a symbol
 *
 * @param pgmd gmd object handle
 * @param symPtr Handle to a symbol when reading sequentially
 * @param symIterPtr Handle to a SymbolIterator
 */
GMD_FUNCPTR(gmdFindLastRecord);

typedef void * (GMD_CALLCONV *gmdFindLastRecordPy_t) (gmdHandle_t pgmd, void *symPtr, int *status);
/** Receive the last record of a symbol (Python)
 *
 * @param pgmd gmd object handle
 * @param symPtr Handle to a symbol when reading sequentially
 * @param status Indicator for error free execution (true/1 means OK, false/0 means Error)
 */
GMD_FUNCPTR(gmdFindLastRecordPy);

typedef int  (GMD_CALLCONV *gmdFindLastRecordSlice_t) (gmdHandle_t pgmd, void *symPtr, const char *keyStr[], void **symIterPtr);
/** Receive the last record of a symbol using the slice definition
 *
 * @param pgmd gmd object handle
 * @param symPtr Handle to a symbol when reading sequentially
 * @param keyStr Array of strings containing the unique elements
 * @param symIterPtr Handle to a SymbolIterator
 */
GMD_FUNCPTR(gmdFindLastRecordSlice);

typedef void * (GMD_CALLCONV *gmdFindLastRecordSlicePy_t) (gmdHandle_t pgmd, void *symPtr, const char *keyStr[], int *status);
/** Receive the last record of a symbol using the slice definition (Python)
 *
 * @param pgmd gmd object handle
 * @param symPtr Handle to a symbol when reading sequentially
 * @param keyStr Array of strings containing the unique elements
 * @param status Indicator for error free execution (true/1 means OK, false/0 means Error)
 */
GMD_FUNCPTR(gmdFindLastRecordSlicePy);

typedef int  (GMD_CALLCONV *gmdRecordMoveNext_t) (gmdHandle_t pgmd, void *symIterPtr);
/** Move symbol iterator to the next record
 *
 * @param pgmd gmd object handle
 * @param symIterPtr Handle to a SymbolIterator
 */
GMD_FUNCPTR(gmdRecordMoveNext);

typedef int  (GMD_CALLCONV *gmdRecordHasNext_t) (gmdHandle_t pgmd, void *symIterPtr);
/** Check if it would be possible to move symbol iterator to the next record without moving it
 *
 * @param pgmd gmd object handle
 * @param symIterPtr Handle to a SymbolIterator
 */
GMD_FUNCPTR(gmdRecordHasNext);

typedef int  (GMD_CALLCONV *gmdRecordMovePrev_t) (gmdHandle_t pgmd, void *symIterPtr);
/** Move symbol iterator to the previous record
 *
 * @param pgmd gmd object handle
 * @param symIterPtr Handle to a SymbolIterator
 */
GMD_FUNCPTR(gmdRecordMovePrev);

typedef int  (GMD_CALLCONV *gmdSameRecord_t) (gmdHandle_t pgmd, void *symIterPtrSrc, void *symIterPtrtar);
/** Compare if both pointers point to same record
 *
 * @param pgmd gmd object handle
 * @param symIterPtrSrc Handle to a Source SymbolIterator
 * @param symIterPtrtar Handle to a Target SymbolIterator
 */
GMD_FUNCPTR(gmdSameRecord);

typedef int  (GMD_CALLCONV *gmdRecordHasPrev_t) (gmdHandle_t pgmd, void *symIterPtr);
/** Check if it would be possible to move symbol iterator to the previous record without moving it
 *
 * @param pgmd gmd object handle
 * @param symIterPtr Handle to a SymbolIterator
 */
GMD_FUNCPTR(gmdRecordHasPrev);

typedef int  (GMD_CALLCONV *gmdGetElemText_t) (gmdHandle_t pgmd, void *symIterPtr, char *txt);
/** Retrieve the string number for an entry
 *
 * @param pgmd gmd object handle
 * @param symIterPtr Handle to a SymbolIterator
 * @param txt Text found for the entry
 */
GMD_FUNCPTR(gmdGetElemText);

typedef int  (GMD_CALLCONV *gmdGetLevel_t) (gmdHandle_t pgmd, void *symIterPtr, double *value);
/** Retrieve the the level of a record
 *
 * @param pgmd gmd object handle
 * @param symIterPtr Handle to a SymbolIterator
 * @param value double value of the specific symbol record
 */
GMD_FUNCPTR(gmdGetLevel);

typedef int  (GMD_CALLCONV *gmdGetLower_t) (gmdHandle_t pgmd, void *symIterPtr, double *value);
/** Retrieve the the lower bound of a record
 *
 * @param pgmd gmd object handle
 * @param symIterPtr Handle to a SymbolIterator
 * @param value double value of the specific symbol record
 */
GMD_FUNCPTR(gmdGetLower);

typedef int  (GMD_CALLCONV *gmdGetUpper_t) (gmdHandle_t pgmd, void *symIterPtr, double *value);
/** Retrieve the the upper bound of a record
 *
 * @param pgmd gmd object handle
 * @param symIterPtr Handle to a SymbolIterator
 * @param value double value of the specific symbol record
 */
GMD_FUNCPTR(gmdGetUpper);

typedef int  (GMD_CALLCONV *gmdGetMarginal_t) (gmdHandle_t pgmd, void *symIterPtr, double *value);
/** Retrieve the the marginal of a record
 *
 * @param pgmd gmd object handle
 * @param symIterPtr Handle to a SymbolIterator
 * @param value double value of the specific symbol record
 */
GMD_FUNCPTR(gmdGetMarginal);

typedef int  (GMD_CALLCONV *gmdGetScale_t) (gmdHandle_t pgmd, void *symIterPtr, double *value);
/** Retrieve the the scale of a record
 *
 * @param pgmd gmd object handle
 * @param symIterPtr Handle to a SymbolIterator
 * @param value double value of the specific symbol record
 */
GMD_FUNCPTR(gmdGetScale);

typedef int  (GMD_CALLCONV *gmdSetElemText_t) (gmdHandle_t pgmd, void *symIterPtr, const char *txt);
/** Retrieve the string number for an entry
 *
 * @param pgmd gmd object handle
 * @param symIterPtr Handle to a SymbolIterator
 * @param txt Text found for the entry
 */
GMD_FUNCPTR(gmdSetElemText);

typedef int  (GMD_CALLCONV *gmdSetLevel_t) (gmdHandle_t pgmd, void *symIterPtr, double value);
/** Set the the level of a record
 *
 * @param pgmd gmd object handle
 * @param symIterPtr Handle to a SymbolIterator
 * @param value double value of the specific symbol record
 */
GMD_FUNCPTR(gmdSetLevel);

typedef int  (GMD_CALLCONV *gmdSetLower_t) (gmdHandle_t pgmd, void *symIterPtr, double value);
/** Set the the lower bound of a record
 *
 * @param pgmd gmd object handle
 * @param symIterPtr Handle to a SymbolIterator
 * @param value double value of the specific symbol record
 */
GMD_FUNCPTR(gmdSetLower);

typedef int  (GMD_CALLCONV *gmdSetUpper_t) (gmdHandle_t pgmd, void *symIterPtr, double value);
/** Set the the upper bound of a record
 *
 * @param pgmd gmd object handle
 * @param symIterPtr Handle to a SymbolIterator
 * @param value double value of the specific symbol record
 */
GMD_FUNCPTR(gmdSetUpper);

typedef int  (GMD_CALLCONV *gmdSetMarginal_t) (gmdHandle_t pgmd, void *symIterPtr, double value);
/** Set the the marginal of a record
 *
 * @param pgmd gmd object handle
 * @param symIterPtr Handle to a SymbolIterator
 * @param value double value of the specific symbol record
 */
GMD_FUNCPTR(gmdSetMarginal);

typedef int  (GMD_CALLCONV *gmdSetScale_t) (gmdHandle_t pgmd, void *symIterPtr, double value);
/** Set the the scale of a record
 *
 * @param pgmd gmd object handle
 * @param symIterPtr Handle to a SymbolIterator
 * @param value double value of the specific symbol record
 */
GMD_FUNCPTR(gmdSetScale);

typedef int  (GMD_CALLCONV *gmdSetUserInfo_t) (gmdHandle_t pgmd, void *symPtr, int value);
/** Set the UserInfo of a symbol
 *
 * @param pgmd gmd object handle
 * @param symPtr Handle to a symbol when reading sequentially
 * @param value double value of the specific symbol record
 */
GMD_FUNCPTR(gmdSetUserInfo);

typedef int  (GMD_CALLCONV *gmdAddRecord_t) (gmdHandle_t pgmd, void *symPtr, const char *keyStr[], void **symIterPtr);
/** Add a new record to a symbol
 *
 * @param pgmd gmd object handle
 * @param symPtr Handle to a symbol when reading sequentially
 * @param keyStr Array of strings containing the unique elements
 * @param symIterPtr Handle to a SymbolIterator
 */
GMD_FUNCPTR(gmdAddRecord);

typedef void * (GMD_CALLCONV *gmdAddRecordPy_t) (gmdHandle_t pgmd, void *symPtr, const char *keyStr[], int *status);
/** Add a new record to a symbol (Python)
 *
 * @param pgmd gmd object handle
 * @param symPtr Handle to a symbol when reading sequentially
 * @param keyStr Array of strings containing the unique elements
 * @param status 
 */
GMD_FUNCPTR(gmdAddRecordPy);

typedef int  (GMD_CALLCONV *gmdMergeRecord_t) (gmdHandle_t pgmd, void *symPtr, const char *keyStr[], void **symIterPtr);
/** Merge a record to a symbol
 *
 * @param pgmd gmd object handle
 * @param symPtr Handle to a symbol when reading sequentially
 * @param keyStr Array of strings containing the unique elements
 * @param symIterPtr Handle to a SymbolIterator
 */
GMD_FUNCPTR(gmdMergeRecord);

typedef void * (GMD_CALLCONV *gmdMergeRecordPy_t) (gmdHandle_t pgmd, void *symPtr, const char *keyStr[], int *status);
/** Merge a record to a symbol (Python)
 *
 * @param pgmd gmd object handle
 * @param symPtr Handle to a symbol when reading sequentially
 * @param keyStr Array of strings containing the unique elements
 * @param status 
 */
GMD_FUNCPTR(gmdMergeRecordPy);

typedef int  (GMD_CALLCONV *gmdMergeRecordInt_t) (gmdHandle_t pgmd, void *symPtr, const int keyInt[], int checkUEL, int wantSymIterPtr, void **symIterPtr, int haveValues, const double values[]);
/** Merge a record with int keys to a symbol
 *
 * @param pgmd gmd object handle
 * @param symPtr Handle to a symbol when reading sequentially
 * @param keyInt Array of int containing the unique elements
 * @param checkUEL 
 * @param wantSymIterPtr 
 * @param symIterPtr Handle to a SymbolIterator
 * @param haveValues 
 * @param values double values for all symbol types
 */
GMD_FUNCPTR(gmdMergeRecordInt);

typedef void * (GMD_CALLCONV *gmdMergeRecordIntPy_t) (gmdHandle_t pgmd, void *symPtr, const int keyInt[], int checkUEL, int wantSymIterPtr, int haveValues, const double values[], int *status);
/** Merge a record with int keys to a symbol (Python)
 *
 * @param pgmd gmd object handle
 * @param symPtr Handle to a symbol when reading sequentially
 * @param keyInt Array of int containing the unique elements
 * @param checkUEL 
 * @param wantSymIterPtr 
 * @param haveValues 
 * @param values double values for all symbol types
 * @param status Indicator for error free execution (true/1 means OK, false/0 means Error)
 */
GMD_FUNCPTR(gmdMergeRecordIntPy);

typedef int  (GMD_CALLCONV *gmdMergeSetRecordInt_t) (gmdHandle_t pgmd, void *symPtr, const int keyInt[], int checkUEL, int wantSymIterPtr, void **symIterPtr, const char *eText);
/** Merge a set record with int keys to a set symbol
 *
 * @param pgmd gmd object handle
 * @param symPtr Handle to a symbol when reading sequentially
 * @param keyInt Array of int containing the unique elements
 * @param checkUEL 
 * @param wantSymIterPtr 
 * @param symIterPtr Handle to a SymbolIterator
 * @param eText 
 */
GMD_FUNCPTR(gmdMergeSetRecordInt);

typedef void * (GMD_CALLCONV *gmdMergeSetRecordIntPy_t) (gmdHandle_t pgmd, void *symPtr, const int keyInt[], int checkUEL, int wantSymIterPtr, const char *eText, int *status);
/** Merge a set record with int keys to a set symbol (Python)
 *
 * @param pgmd gmd object handle
 * @param symPtr Handle to a symbol when reading sequentially
 * @param keyInt Array of int containing the unique elements
 * @param checkUEL 
 * @param wantSymIterPtr 
 * @param eText 
 * @param status Indicator for error free execution (true/1 means OK, false/0 means Error)
 */
GMD_FUNCPTR(gmdMergeSetRecordIntPy);

typedef int  (GMD_CALLCONV *gmdAddRecordRaw_t) (gmdHandle_t pgmd, void *symPtr, const int keyInt[], const double values[], const char *eText);
/** Deprecated use gmdMergeRecordInt
 *
 * @param pgmd gmd object handle
 * @param symPtr Handle to a symbol when reading sequentially
 * @param keyInt Array of int containing the unique elements
 * @param values double values for all symbol types
 * @param eText 
 */
GMD_FUNCPTR(gmdAddRecordRaw);

typedef int  (GMD_CALLCONV *gmdDeleteRecord_t) (gmdHandle_t pgmd, void *symIterPtr);
/** Deletes one particular record
 *
 * @param pgmd gmd object handle
 * @param symIterPtr Handle to a SymbolIterator
 */
GMD_FUNCPTR(gmdDeleteRecord);

typedef int  (GMD_CALLCONV *gmdGetRecordRaw_t) (gmdHandle_t pgmd, void *symIterPtr, int aDim, int keyInt[], double values[]);
/** Retrieve the full record
 *
 * @param pgmd gmd object handle
 * @param symIterPtr Handle to a SymbolIterator
 * @param aDim dimension of the symbol
 * @param keyInt Array of int containing the unique elements
 * @param values double values for all symbol types
 */
GMD_FUNCPTR(gmdGetRecordRaw);

typedef int  (GMD_CALLCONV *gmdGetKeys_t) (gmdHandle_t pgmd, void *symIterPtr, int aDim, char *keyStr[]);
/** Retrieve the keys of a record
 *
 * @param pgmd gmd object handle
 * @param symIterPtr Handle to a SymbolIterator
 * @param aDim dimension of the symbol
 * @param keyStr Array of strings containing the unique elements
 */
GMD_FUNCPTR(gmdGetKeys);

typedef int  (GMD_CALLCONV *gmdGetKey_t) (gmdHandle_t pgmd, void *symIterPtr, int idx, char *keyStr);
/** Retrieve a key element of a record
 *
 * @param pgmd gmd object handle
 * @param symIterPtr Handle to a SymbolIterator
 * @param idx index of a symbol in the GMD object / of a key from a record
 * @param keyStr Array of strings containing the unique elements
 */
GMD_FUNCPTR(gmdGetKey);

typedef int  (GMD_CALLCONV *gmdGetDomain_t) (gmdHandle_t pgmd, void *symPtr, int aDim, void **vDomPtrOut, char *keyStr[]);
/** Retrieve domain information
 *
 * @param pgmd gmd object handle
 * @param symPtr Handle to a symbol when reading sequentially
 * @param aDim dimension of the symbol
 * @param vDomPtrOut vector of domain symbols (output for GMD)
 * @param keyStr Array of strings containing the unique elements
 */
GMD_FUNCPTR(gmdGetDomain);

typedef int  (GMD_CALLCONV *gmdCopySymbolIterator_t) (gmdHandle_t pgmd, void *symIterPtrSrc, void **symIterPtrtar);
/** Copy the symbol iterator
 *
 * @param pgmd gmd object handle
 * @param symIterPtrSrc Handle to a Source SymbolIterator
 * @param symIterPtrtar 
 */
GMD_FUNCPTR(gmdCopySymbolIterator);

typedef void * (GMD_CALLCONV *gmdCopySymbolIteratorPy_t) (gmdHandle_t pgmd, void *symIterPtrSrc, int *status);
/** Copy the symbol iterator (Python)
 *
 * @param pgmd gmd object handle
 * @param symIterPtrSrc Handle to a Source SymbolIterator
 * @param status Indicator for error free execution (true/1 means OK, false/0 means Error)
 */
GMD_FUNCPTR(gmdCopySymbolIteratorPy);

typedef int  (GMD_CALLCONV *gmdFreeSymbolIterator_t) (gmdHandle_t pgmd, void *symIterPtr);
/** Free the symbol iterator
 *
 * @param pgmd gmd object handle
 * @param symIterPtr Handle to a SymbolIterator
 */
GMD_FUNCPTR(gmdFreeSymbolIterator);

typedef int  (GMD_CALLCONV *gmdMergeUel_t) (gmdHandle_t pgmd, const char *uel, int *uelNr);
/** Add a uel to universe
 *
 * @param pgmd gmd object handle
 * @param uel 
 * @param uelNr 
 */
GMD_FUNCPTR(gmdMergeUel);

typedef int  (GMD_CALLCONV *gmdGetUelByIndex_t) (gmdHandle_t pgmd, int uelNr, char *keyStr);
/** Retrieve uel by index
 *
 * @param pgmd gmd object handle
 * @param uelNr 
 * @param keyStr Array of strings containing the unique elements
 */
GMD_FUNCPTR(gmdGetUelByIndex);

typedef int  (GMD_CALLCONV *gmdFindUel_t) (gmdHandle_t pgmd, const char *uelLabel, int *uelNr);
/** Retrieve index of uel with label
 *
 * @param pgmd gmd object handle
 * @param uelLabel 
 * @param uelNr 
 */
GMD_FUNCPTR(gmdFindUel);

typedef int  (GMD_CALLCONV *gmdGetSymbolsUels_t) (gmdHandle_t pgmd, void **vDomPtrIn, int lenvDomPtrIn, int uelList[], int sizeUelList);
/** Retrieve an array of uel counters. Counts how often uel at position n is used by symbols in vDomPtrIn.
 *
 * @param pgmd gmd object handle
 * @param vDomPtrIn vector of symbol pointers (input for GMD)
 * @param lenvDomPtrIn Length of vDomPtrIn
 * @param uelList Array of counters.
 * @param sizeUelList size of uelList
 */
GMD_FUNCPTR(gmdGetSymbolsUels);

typedef int  (GMD_CALLCONV *gmdInfo_t) (gmdHandle_t pgmd, int infoKey, int *ival, double *dval, char *sval);
/** Query some information from object
 *
 * @param pgmd gmd object handle
 * @param infoKey access key to an information record
 * @param ival integer valued parameter
 * @param dval double valued parameter
 * @param sval string valued parameter
 */
GMD_FUNCPTR(gmdInfo);

typedef int  (GMD_CALLCONV *gmdSymbolInfo_t) (gmdHandle_t pgmd, void *symPtr, int infoKey, int *ival, double *dval, char *sval);
/** Query some information from a symbol
 *
 * @param pgmd gmd object handle
 * @param symPtr Handle to a symbol when reading sequentially
 * @param infoKey access key to an information record
 * @param ival integer valued parameter
 * @param dval double valued parameter
 * @param sval string valued parameter
 */
GMD_FUNCPTR(gmdSymbolInfo);

typedef int  (GMD_CALLCONV *gmdSymbolDim_t) (gmdHandle_t pgmd, void *symPtr, int *aDim);
/** Query symbol dimension
 *
 * @param pgmd gmd object handle
 * @param symPtr Handle to a symbol when reading sequentially
 * @param aDim dimension of the symbol
 */
GMD_FUNCPTR(gmdSymbolDim);

typedef int  (GMD_CALLCONV *gmdSymbolType_t) (gmdHandle_t pgmd, void *symPtr, int *stype);
/** Query symbol type
 *
 * @param pgmd gmd object handle
 * @param symPtr Handle to a symbol when reading sequentially
 * @param stype type of a symbol
 */
GMD_FUNCPTR(gmdSymbolType);

typedef int  (GMD_CALLCONV *gmdWriteGDX_t) (gmdHandle_t pgmd, const char *fileName, int noDomChk);
/** Write GDX file of current database
 *
 * @param pgmd gmd object handle
 * @param fileName File name of the gdx file
 * @param noDomChk Do not use real domains but just relaxed ones
 */
GMD_FUNCPTR(gmdWriteGDX);

typedef int  (GMD_CALLCONV *gmdSetSpecialValuesX_t) (gmdHandle_t pgmd, const double specVal[], int *specValType);
/** set special values for GMD and provide info about sp value mapping
 *
 * @param pgmd gmd object handle
 * @param specVal Info about special value mapping effort
 * @param specValType Info about special value mapping effort
 */
GMD_FUNCPTR(gmdSetSpecialValuesX);

typedef int  (GMD_CALLCONV *gmdSetSpecialValues_t) (gmdHandle_t pgmd, const double specVal[]);
/** set special values for GMD
 *
 * @param pgmd gmd object handle
 * @param specVal Info about special value mapping effort
 */
GMD_FUNCPTR(gmdSetSpecialValues);

typedef int  (GMD_CALLCONV *gmdGetSpecialValues_t) (gmdHandle_t pgmd, double specVal[]);
/** get special values for GMD
 *
 * @param pgmd gmd object handle
 * @param specVal Info about special value mapping effort
 */
GMD_FUNCPTR(gmdGetSpecialValues);

typedef int  (GMD_CALLCONV *gmdGetUserSpecialValues_t) (gmdHandle_t pgmd, double specVal[]);
/** get special values GMD currently accepts
 *
 * @param pgmd gmd object handle
 * @param specVal Info about special value mapping effort
 */
GMD_FUNCPTR(gmdGetUserSpecialValues);

typedef int  (GMD_CALLCONV *gmdSetDebug_t) (gmdHandle_t pgmd, int debugLevel);
/** set debug mode
 *
 * @param pgmd gmd object handle
 * @param debugLevel debug level
 */
GMD_FUNCPTR(gmdSetDebug);

typedef int  (GMD_CALLCONV *gmdGetLastError_t) (gmdHandle_t pgmd, char *msg);
/** Retrieve last error message
 *
 * @param pgmd gmd object handle
 * @param msg Message
 */
GMD_FUNCPTR(gmdGetLastError);

typedef int  (GMD_CALLCONV *gmdPrintLog_t) (gmdHandle_t pgmd, const char *msg);
/** Print message to cmex log if initialized via gmdInitFromCMEX otherwise to stdout
 *
 * @param pgmd gmd object handle
 * @param msg Message
 */
GMD_FUNCPTR(gmdPrintLog);

typedef int  (GMD_CALLCONV *gmdStartWriteRecording_t) (gmdHandle_t pgmd);
/** Start recording process to capture symbols that are written to
 *
 * @param pgmd gmd object handle
 */
GMD_FUNCPTR(gmdStartWriteRecording);

typedef int  (GMD_CALLCONV *gmdStopWriteRecording_t) (gmdHandle_t pgmd);
/** Stop recording process to capture symbols that are written to
 *
 * @param pgmd gmd object handle
 */
GMD_FUNCPTR(gmdStopWriteRecording);

typedef int  (GMD_CALLCONV *gmdCheckDBDV_t) (gmdHandle_t pgmd, int *dv);
/** Check domain of all symbols in DB
 *
 * @param pgmd gmd object handle
 * @param dv Indicator that a record with domain violation was found
 */
GMD_FUNCPTR(gmdCheckDBDV);

typedef int  (GMD_CALLCONV *gmdCheckSymbolDV_t) (gmdHandle_t pgmd, void *symPtr, int *dv);
/** Check domain of a symbol
 *
 * @param pgmd gmd object handle
 * @param symPtr Handle to a symbol when reading sequentially
 * @param dv Indicator that a record with domain violation was found
 */
GMD_FUNCPTR(gmdCheckSymbolDV);

typedef int  (GMD_CALLCONV *gmdGetFirstDBDV_t) (gmdHandle_t pgmd, void **dvHandle);
/** Returns first domain violation in DB
 *
 * @param pgmd gmd object handle
 * @param dvHandle Handle to retrieve multiple records with domain violation
 */
GMD_FUNCPTR(gmdGetFirstDBDV);

typedef void * (GMD_CALLCONV *gmdGetFirstDBDVPy_t) (gmdHandle_t pgmd, int *status);
/** Returns first domain violation in DB (Python)
 *
 * @param pgmd gmd object handle
 * @param status Indicator for error free execution (true/1 means OK, false/0 means Error)
 */
GMD_FUNCPTR(gmdGetFirstDBDVPy);

typedef int  (GMD_CALLCONV *gmdGetFirstDVInSymbol_t) (gmdHandle_t pgmd, void *symPtr, void **dvHandle);
/** Returns first record with domain violation in a particular symbol
 *
 * @param pgmd gmd object handle
 * @param symPtr Handle to a symbol when reading sequentially
 * @param dvHandle Handle to retrieve multiple records with domain violation
 */
GMD_FUNCPTR(gmdGetFirstDVInSymbol);

typedef void * (GMD_CALLCONV *gmdGetFirstDVInSymbolPy_t) (gmdHandle_t pgmd, void *symPtr, int *status);
/** Returns first record with domain violation in a particular symbol (Python)
 *
 * @param pgmd gmd object handle
 * @param symPtr Handle to a symbol when reading sequentially
 * @param status Indicator for error free execution (true/1 means OK, false/0 means Error)
 */
GMD_FUNCPTR(gmdGetFirstDVInSymbolPy);

typedef int  (GMD_CALLCONV *gmdDomainCheckDone_t) (gmdHandle_t pgmd);
/** End of domain checking needs to be called after a call to gmdGetFirstDBDV
 *
 * @param pgmd gmd object handle
 */
GMD_FUNCPTR(gmdDomainCheckDone);

typedef int  (GMD_CALLCONV *gmdGetFirstDVInNextSymbol_t) (gmdHandle_t pgmd, void *dvHandle, int *nextavail);
/** If nextavail is true dvHandle has the first domain violation of the next symbol with domain violations
 *
 * @param pgmd gmd object handle
 * @param dvHandle Handle to retrieve multiple records with domain violation
 * @param nextavail indicator for next record available
 */
GMD_FUNCPTR(gmdGetFirstDVInNextSymbol);

typedef int  (GMD_CALLCONV *gmdMoveNextDVInSymbol_t) (gmdHandle_t pgmd, void *dvHandle, int *nextavail);
/** If nextavail is true dvHandle has the next domain violation in the same symbol
 *
 * @param pgmd gmd object handle
 * @param dvHandle Handle to retrieve multiple records with domain violation
 * @param nextavail indicator for next record available
 */
GMD_FUNCPTR(gmdMoveNextDVInSymbol);

typedef int  (GMD_CALLCONV *gmdFreeDVHandle_t) (gmdHandle_t pgmd, void *dvHandle);
/** Free DV handle
 *
 * @param pgmd gmd object handle
 * @param dvHandle Handle to retrieve multiple records with domain violation
 */
GMD_FUNCPTR(gmdFreeDVHandle);

typedef int  (GMD_CALLCONV *gmdGetDVSymbol_t) (gmdHandle_t pgmd, void *dvHandle, void **symPtr);
/** Returns symbol of DV record
 *
 * @param pgmd gmd object handle
 * @param dvHandle Handle to retrieve multiple records with domain violation
 * @param symPtr Handle to a symbol when reading sequentially
 */
GMD_FUNCPTR(gmdGetDVSymbol);

typedef void * (GMD_CALLCONV *gmdGetDVSymbolPy_t) (gmdHandle_t pgmd, void *dvHandle, int *status);
/** Returns symbol of DV record (Python)
 *
 * @param pgmd gmd object handle
 * @param dvHandle Handle to retrieve multiple records with domain violation
 * @param status Indicator for error free execution (true/1 means OK, false/0 means Error)
 */
GMD_FUNCPTR(gmdGetDVSymbolPy);

typedef int  (GMD_CALLCONV *gmdGetDVSymbolRecord_t) (gmdHandle_t pgmd, void *dvHandle, void **symIterPtr);
/** Returns symbol record of DV record
 *
 * @param pgmd gmd object handle
 * @param dvHandle Handle to retrieve multiple records with domain violation
 * @param symIterPtr Handle to a SymbolIterator
 */
GMD_FUNCPTR(gmdGetDVSymbolRecord);

typedef void * (GMD_CALLCONV *gmdGetDVSymbolRecordPy_t) (gmdHandle_t pgmd, void *dvHandle, int *status);
/** Returns symbol record of DV record (Python)
 *
 * @param pgmd gmd object handle
 * @param dvHandle Handle to retrieve multiple records with domain violation
 * @param status Indicator for error free execution (true/1 means OK, false/0 means Error)
 */
GMD_FUNCPTR(gmdGetDVSymbolRecordPy);

typedef int  (GMD_CALLCONV *gmdGetDVIndicator_t) (gmdHandle_t pgmd, void *dvHandle, int viol[]);
/** Returns DV indicator array
 *
 * @param pgmd gmd object handle
 * @param dvHandle Handle to retrieve multiple records with domain violation
 * @param viol a dim vector that has nonzero at a position where domain violation occured
 */
GMD_FUNCPTR(gmdGetDVIndicator);

typedef int  (GMD_CALLCONV *gmdInitUpdate_t) (gmdHandle_t pgmd, void *gmoPtr);
/** Initialize GMO synchronization
 *
 * @param pgmd gmd object handle
 * @param gmoPtr Handle to GMO object
 */
GMD_FUNCPTR(gmdInitUpdate);

typedef int  (GMD_CALLCONV *gmdUpdateModelSymbol_t) (gmdHandle_t pgmd, void *gamsSymPtr, int actionType, void *dataSymPtr, int updateType, int *noMatchCnt);
/** Updates model with data from database
 *
 * @param pgmd gmd object handle
 * @param gamsSymPtr Handle to a symbol in the GAMS model
 * @param actionType what to update
 * @param dataSymPtr Handle to a symbol in containing data
 * @param updateType how to update
 * @param noMatchCnt Number of records in symbol that were not used for updating
 */
GMD_FUNCPTR(gmdUpdateModelSymbol);

typedef int  (GMD_CALLCONV *gmdCallSolver_t) (gmdHandle_t pgmd, const char *solvername);
/** Call GAMS solver
 *
 * @param pgmd gmd object handle
 * @param solvername Name of solver to execute
 */
GMD_FUNCPTR(gmdCallSolver);

typedef int  (GMD_CALLCONV *gmdCallSolverTimed_t) (gmdHandle_t pgmd, const char *solvername, double *time);
/** Call GAMS solver and return time used by gevCallSolver
 *
 * @param pgmd gmd object handle
 * @param solvername Name of solver to execute
 * @param time 
 */
GMD_FUNCPTR(gmdCallSolverTimed);

typedef int  (GMD_CALLCONV *gmdDenseSymbolToDenseArray_t) (gmdHandle_t pgmd, void *cube, int vDim[], void *symPtr, int field);
/** Copy dense symbol content into dense array
 *
 * @param pgmd gmd object handle
 * @param cube multi-dimensional dense array
 * @param vDim vector of dimensionality of cube
 * @param symPtr Handle to a symbol when reading sequentially
 * @param field record field of variable or equation
 */
GMD_FUNCPTR(gmdDenseSymbolToDenseArray);

typedef int  (GMD_CALLCONV *gmdSparseSymbolToDenseArray_t) (gmdHandle_t pgmd, void *cube, int vDim[], void *symPtr, void **vDomPtr, int field, int *nDropped);
/** Copy sparse symbol content with domain info into dense array
 *
 * @param pgmd gmd object handle
 * @param cube multi-dimensional dense array
 * @param vDim vector of dimensionality of cube
 * @param symPtr Handle to a symbol when reading sequentially
 * @param vDomPtr vector of domain symbols
 * @param field record field of variable or equation
 * @param nDropped Number of dropped records
 */
GMD_FUNCPTR(gmdSparseSymbolToDenseArray);

typedef int  (GMD_CALLCONV *gmdSparseSymbolToSqzdArray_t) (gmdHandle_t pgmd, void *cube, int vDim[], void *symPtr, void **vDomSqueezePtr, void **vDomPtr, int field, int *nDropped);
/** Copy sparse symbol content with domain and squeezed domain info into dense array
 *
 * @param pgmd gmd object handle
 * @param cube multi-dimensional dense array
 * @param vDim vector of dimensionality of cube
 * @param symPtr Handle to a symbol when reading sequentially
 * @param vDomSqueezePtr vector of squeezed domain symbols
 * @param vDomPtr vector of domain symbols
 * @param field record field of variable or equation
 * @param nDropped Number of dropped records
 */
GMD_FUNCPTR(gmdSparseSymbolToSqzdArray);

typedef int  (GMD_CALLCONV *gmdDenseArrayToSymbol_t) (gmdHandle_t pgmd, void *symPtr, void **vDomPtr, void *cube, int vDim[]);
/** Copy dense array into symbol with domain info
 *
 * @param pgmd gmd object handle
 * @param symPtr 
 * @param vDomPtr vector of domain symbols
 * @param cube multi-dimensional dense array
 * @param vDim vector of dimensionality of cube
 */
GMD_FUNCPTR(gmdDenseArrayToSymbol);

typedef int  (GMD_CALLCONV *gmdDenseArraySlicesToSymbol_t) (gmdHandle_t pgmd, void *symPtr, void **vDomSlicePtr, void **vDomPtr, void *cube, int vDim[]);
/** Copy slices of dense array into symbol with domain info and slice domain info
 *
 * @param pgmd gmd object handle
 * @param symPtr 
 * @param vDomSlicePtr vector of slice domain symbols
 * @param vDomPtr vector of domain symbols
 * @param cube multi-dimensional dense array
 * @param vDim vector of dimensionality of cube
 */
GMD_FUNCPTR(gmdDenseArraySlicesToSymbol);

typedef int  (GMD_CALLCONV *gmdSelectRecordStorage_t) (gmdHandle_t pgmd, void **symPtr, int storageType);
/** Returns false for failure, true otherwise
 *
 * @param pgmd gmd object handle
 * @param symPtr Symbol for which the record storage data structure should be changed or NULL to set default storage type for new symbols. When a symbol is supplied, it will be copied into a new symbol that utilizes the now chosen record container and the old symbol will be discarded. The symbol pointer (symPtr) will be updated to point to the new symbol.
 * @param storageType Data structure used to store symbol records (RB_TREE=0, VECTOR=1, GMS_TREE=2)
 */
GMD_FUNCPTR(gmdSelectRecordStorage);

typedef void * (GMD_CALLCONV *gmdSelectRecordStoragePy_t) (gmdHandle_t pgmd, void *symPtr, int storageType, int *status);
/** Select data structure for storing symbol records (Python)
 *
 * @param pgmd gmd object handle
 * @param symPtr Symbol for which the record storage data structure should be changed or NULL to set default storage type for new symbols. When a symbol is supplied, it will be copied into a new symbol that utilizes the now chosen record container and the old symbol will be discarded. The returned pointer has the address of the new symbol.
 * @param storageType Data structure used to store symbol records (RB_TREE=0, VECTOR=1, GMS_TREE=2)
 * @param status Indicator for error free execution (true/1 means OK, false/0 means Error)
 */
GMD_FUNCPTR(gmdSelectRecordStoragePy);

#if defined(__cplusplus)
}
#endif
#endif /* #if ! defined(_GMDCC_H_) */

