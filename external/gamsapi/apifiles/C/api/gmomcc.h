/* gmomcc.h
 * Header file for C-style interface to the GMO library
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


#if ! defined(_GMOCC_H_)
#     define  _GMOCC_H_

#define GMOAPIVERSION 27

enum gmoEquType {
  gmoequ_E = 0,
  gmoequ_G = 1,
  gmoequ_L = 2,
  gmoequ_N = 3,
  gmoequ_X = 4,
  gmoequ_C = 5,
  gmoequ_B = 6  };

enum gmoVarType {
  gmovar_X  = 0,
  gmovar_B  = 1,
  gmovar_I  = 2,
  gmovar_S1 = 3,
  gmovar_S2 = 4,
  gmovar_SC = 5,
  gmovar_SI = 6  };

enum gmoEquOrder {
  gmoorder_ERR = 0,
  gmoorder_L   = 1,
  gmoorder_Q   = 2,
  gmoorder_NL  = 3  };

enum gmoVarFreeType {
  gmovar_X_F = 0,
  gmovar_X_N = 1,
  gmovar_X_P = 2  };

enum gmoVarEquBasisStatus {
  gmoBstat_Lower = 0,
  gmoBstat_Upper = 1,
  gmoBstat_Basic = 2,
  gmoBstat_Super = 3  };

enum gmoVarEquStatus {
  gmoCstat_OK     = 0,
  gmoCstat_NonOpt = 1,
  gmoCstat_Infeas = 2,
  gmoCstat_UnBnd  = 3  };

enum gmoObjectiveType {
  gmoObjType_Var = 0,
  gmoObjType_Fun = 2  };

enum gmoInterfaceType {
  gmoIFace_Processed = 0,
  gmoIFace_Raw       = 1  };

enum gmoObjectiveSense {
  gmoObj_Min  = 0,
  gmoObj_Max  = 1,
  gmoObj_None = 255  };

enum gmoSolverStatus {
  gmoSolveStat_Normal      = 1,
  gmoSolveStat_Iteration   = 2,
  gmoSolveStat_Resource    = 3,
  gmoSolveStat_Solver      = 4,
  gmoSolveStat_EvalError   = 5,
  gmoSolveStat_Capability  = 6,
  gmoSolveStat_License     = 7,
  gmoSolveStat_User        = 8,
  gmoSolveStat_SetupErr    = 9,
  gmoSolveStat_SolverErr   = 10,
  gmoSolveStat_InternalErr = 11,
  gmoSolveStat_Skipped     = 12,
  gmoSolveStat_SystemErr   = 13  };

enum gmoModelStatus {
  gmoModelStat_OptimalGlobal        = 1,
  gmoModelStat_OptimalLocal         = 2,
  gmoModelStat_Unbounded            = 3,
  gmoModelStat_InfeasibleGlobal     = 4,
  gmoModelStat_InfeasibleLocal      = 5,
  gmoModelStat_InfeasibleIntermed   = 6,
  gmoModelStat_Feasible             = 7,
  gmoModelStat_Integer              = 8,
  gmoModelStat_NonIntegerIntermed   = 9,
  gmoModelStat_IntegerInfeasible    = 10,
  gmoModelStat_LicenseError         = 11,
  gmoModelStat_ErrorUnknown         = 12,
  gmoModelStat_ErrorNoSolution      = 13,
  gmoModelStat_NoSolutionReturned   = 14,
  gmoModelStat_SolvedUnique         = 15,
  gmoModelStat_Solved               = 16,
  gmoModelStat_SolvedSingular       = 17,
  gmoModelStat_UnboundedNoSolution  = 18,
  gmoModelStat_InfeasibleNoSolution = 19  };

enum gmoHeadnTail {
  gmoHiterused  = 3,
  gmoHresused   = 4,
  gmoHobjval    = 5,
  gmoHdomused   = 6,
  gmoHmarginals = 9,
  gmoHetalg     = 10,
  gmoTmipnod    = 11,
  gmoTninf      = 12,
  gmoTnopt      = 13,
  gmoTmipbest   = 15,
  gmoTsinf      = 20,
  gmoTrobj      = 22  };

enum gmoHTcard {
  gmonumheader = 10,
  gmonumtail   = 12  };

enum gmoProcType {
  gmoProc_none           = 0,
  gmoProc_lp             = 1,
  gmoProc_mip            = 2,
  gmoProc_rmip           = 3,
  gmoProc_nlp            = 4,
  gmoProc_mcp            = 5,
  gmoProc_mpec           = 6,
  gmoProc_rmpec          = 7,
  gmoProc_cns            = 8,
  gmoProc_dnlp           = 9,
  gmoProc_rminlp         = 10,
  gmoProc_minlp          = 11,
  gmoProc_qcp            = 12,
  gmoProc_miqcp          = 13,
  gmoProc_rmiqcp         = 14,
  gmoProc_emp            = 15,
  gmoProc_nrofmodeltypes = 16  };

enum gmoEMPAgentType {
  gmoMinAgent = 0,
  gmoMaxAgent = 1,
  gmoVIAgent  = 2  };

enum gmoEvalErrorMethodNum {
  gmoEVALERRORMETHOD_KEEPGOING = 0,
  gmoEVALERRORMETHOD_FASTSTOP  = 1  };
#if defined(_WIN32) && defined(__GNUC__)
# include <stdio.h>
#endif


#include "gclgms.h"

#if defined(_WIN32)
# define GMO_CALLCONV __stdcall
#else
# define GMO_CALLCONV
#endif
#if defined(_WIN32)
typedef __int64 INT64;
#else
typedef signed long int INT64;
#endif

#if defined(__cplusplus)
extern "C" {
#endif

struct gmoRec;
typedef struct gmoRec *gmoHandle_t;

typedef int (*gmoErrorCallback_t) (int ErrCount, const char *msg);

/* headers for "wrapper" routines implemented in C */
/** gmoGetReady: load library
 *  @return false on failure to load library, true on success 
 */
int gmoGetReady  (char *msgBuf, int msgBufLen);

/** gmoGetReadyD: load library from the speicified directory
 * @return false on failure to load library, true on success 
 */
int gmoGetReadyD (const char *dirName, char *msgBuf, int msgBufLen);

/** gmoGetReadyL: load library from the specified library name
 *  @return false on failure to load library, true on success 
 */
int gmoGetReadyL (const char *libName, char *msgBuf, int msgBufLen);

/** gmoCreate: load library and create gmo object handle 
 *  @return false on failure to load library, true on success 
 */
int gmoCreate    (gmoHandle_t *pgmo, char *msgBuf, int msgBufLen);

/** gmoCreateD: load library from the specified directory and create gmo object handle
 * @return false on failure to load library, true on success 
 */
int gmoCreateD   (gmoHandle_t *pgmo, const char *dirName, char *msgBuf, int msgBufLen);

/** gmoCreateDD: load library from the specified directory and create gmo object handle
 * @return false on failure to load library, true on success 
 */
int gmoCreateDD  (gmoHandle_t *pgmo, const char *dirName, char *msgBuf, int msgBufLen);

/** gmoCreate: load library from the specified library name and create gmo object handle
 * @return false on failure to load library, true on success 
 */
int gmoCreateL   (gmoHandle_t *pgmo, const char *libName, char *msgBuf, int msgBufLen);

/** gmoFree: free gmo object handle 
 * @return false on failure, true on success 
 */
int gmoFree      (gmoHandle_t *pgmo);

/** Check if library has been loaded
 * @return false on failure, true on success 
 */
int gmoLibraryLoaded(void);

/** Check if library has been unloaded
 * @return false on failure, true on success 
 */
int gmoLibraryUnload(void);

/** Check if API and library have the same version, Library needs to be initialized before calling this.
 * @return true  (1) on success,
 *         false (0) on failure.
 */
int  gmoCorrectLibraryVersion(char *msgBuf, int msgBufLen);

int  gmoGetScreenIndicator   (void);
void gmoSetScreenIndicator   (int scrind);
int  gmoGetExceptionIndicator(void);
void gmoSetExceptionIndicator(int excind);
int  gmoGetExitIndicator     (void);
void gmoSetExitIndicator     (int extind);
gmoErrorCallback_t gmoGetErrorCallback(void);
void gmoSetErrorCallback(gmoErrorCallback_t func);
int  gmoGetAPIErrorCount     (void);
void gmoSetAPIErrorCount     (int ecnt);

void gmoErrorHandling(const char *msg);
void gmoInitMutexes(void);
void gmoFiniMutexes(void);

#if defined(GMO_MAIN)    /* we must define some things only once */
# define GMO_FUNCPTR(NAME)  NAME##_t NAME = NULL
#else
# define GMO_FUNCPTR(NAME)  extern NAME##_t NAME
#endif

/* Prototypes for Dummy Functions */
int  GMO_CALLCONV d_gmoInitData (gmoHandle_t pgmo, int rows, int cols, int codelen);
int  GMO_CALLCONV d_gmoAddRow (gmoHandle_t pgmo, int etyp, int ematch, double eslack, double escale, double erhs, double emarg, int ebas, int enz, const int colidx[], const double jacval[], const int nlflag[]);
int  GMO_CALLCONV d_gmoAddCol (gmoHandle_t pgmo, int vtyp, double vlo, double vl, double vup, double vmarg, int vbas, int vsos, double vprior, double vscale, int vnz, const int rowidx[], const double jacval[], const int nlflag[]);
int  GMO_CALLCONV d_gmoCompleteData (gmoHandle_t pgmo, char *msg);
int  GMO_CALLCONV d_gmoFillMatches (gmoHandle_t pgmo, char *msg);
int  GMO_CALLCONV d_gmoLoadDataLegacy (gmoHandle_t pgmo, char *msg);
int  GMO_CALLCONV d_gmoLoadDataLegacyEx (gmoHandle_t pgmo, int fillMatches, char *msg);
int  GMO_CALLCONV d_gmoRegisterEnvironment (gmoHandle_t pgmo, void *gevptr, char *msg);
void * GMO_CALLCONV d_gmoEnvironment (gmoHandle_t pgmo);
void * GMO_CALLCONV d_gmoViewStore (gmoHandle_t pgmo);
void  GMO_CALLCONV d_gmoViewRestore (gmoHandle_t pgmo, void **viewptr);
void  GMO_CALLCONV d_gmoViewDump (gmoHandle_t pgmo);
int  GMO_CALLCONV d_gmoGetiSolver (gmoHandle_t pgmo, int mi);
int  GMO_CALLCONV d_gmoGetjSolver (gmoHandle_t pgmo, int mj);
int  GMO_CALLCONV d_gmoGetiSolverQuiet (gmoHandle_t pgmo, int mi);
int  GMO_CALLCONV d_gmoGetjSolverQuiet (gmoHandle_t pgmo, int mj);
int  GMO_CALLCONV d_gmoGetiModel (gmoHandle_t pgmo, int si);
int  GMO_CALLCONV d_gmoGetjModel (gmoHandle_t pgmo, int sj);
int  GMO_CALLCONV d_gmoSetEquPermutation (gmoHandle_t pgmo, int permut[]);
int  GMO_CALLCONV d_gmoSetRvEquPermutation (gmoHandle_t pgmo, int rvpermut[], int len);
int  GMO_CALLCONV d_gmoSetVarPermutation (gmoHandle_t pgmo, int permut[]);
int  GMO_CALLCONV d_gmoSetRvVarPermutation (gmoHandle_t pgmo, int rvpermut[], int len);
int  GMO_CALLCONV d_gmoSetNRowPerm (gmoHandle_t pgmo);
int  GMO_CALLCONV d_gmoGetVarTypeCnt (gmoHandle_t pgmo, int vtyp);
int  GMO_CALLCONV d_gmoGetEquTypeCnt (gmoHandle_t pgmo, int etyp);
int  GMO_CALLCONV d_gmoGetObjStat (gmoHandle_t pgmo, int *nz, int *qnz, int *nlnz);
int  GMO_CALLCONV d_gmoGetRowStat (gmoHandle_t pgmo, int si, int *nz, int *qnz, int *nlnz);
int  GMO_CALLCONV d_gmoGetRowStatEx (gmoHandle_t pgmo, int si, int *nz, int *lnz, int *qnz, int *nlnz);
int  GMO_CALLCONV d_gmoGetColStat (gmoHandle_t pgmo, int sj, int *nz, int *qnz, int *nlnz, int *objnz);
int  GMO_CALLCONV d_gmoGetRowQNZOne (gmoHandle_t pgmo, int si);
INT64  GMO_CALLCONV d_gmoGetRowQNZOne64 (gmoHandle_t pgmo, int si);
int  GMO_CALLCONV d_gmoGetRowQDiagNZOne (gmoHandle_t pgmo, int si);
int  GMO_CALLCONV d_gmoGetRowCVecNZOne (gmoHandle_t pgmo, int si);
void  GMO_CALLCONV d_gmoGetSosCounts (gmoHandle_t pgmo, int *numsos1, int *numsos2, int *nzsos);
void  GMO_CALLCONV d_gmoGetXLibCounts (gmoHandle_t pgmo, int *rows, int *cols, int *nz, int orgcolind[]);
int  GMO_CALLCONV d_gmoGetActiveModelType (gmoHandle_t pgmo, int checkv[], int *actModelType);
int  GMO_CALLCONV d_gmoGetMatrixRow (gmoHandle_t pgmo, int rowstart[], int colidx[], double jacval[], int nlflag[]);
int  GMO_CALLCONV d_gmoGetMatrixCol (gmoHandle_t pgmo, int colstart[], int rowidx[], double jacval[], int nlflag[]);
int  GMO_CALLCONV d_gmoGetMatrixCplex (gmoHandle_t pgmo, int colstart[], int collength[], int rowidx[], double jacval[]);
char * GMO_CALLCONV d_gmoGetObjName (gmoHandle_t pgmo, char *buf);
char * GMO_CALLCONV d_gmoGetObjNameCustom (gmoHandle_t pgmo, const char *suffix, char *buf);
int  GMO_CALLCONV d_gmoGetObjVector (gmoHandle_t pgmo, double jacval[], int nlflag[]);
int  GMO_CALLCONV d_gmoGetObjSparse (gmoHandle_t pgmo, int colidx[], double jacval[], int nlflag[], int *nz, int *nlnz);
int  GMO_CALLCONV d_gmoGetObjSparseEx (gmoHandle_t pgmo, int colidx[], double gradval[], int nlflag[], int *nz, int *qnz, int *nlnz);
int  GMO_CALLCONV d_gmoGetObjQMat (gmoHandle_t pgmo, int varidx1[], int varidx2[], double coefs[]);
int  GMO_CALLCONV d_gmoGetObjQ (gmoHandle_t pgmo, int varidx1[], int varidx2[], double coefs[]);
int  GMO_CALLCONV d_gmoGetObjCVec (gmoHandle_t pgmo, int varidx[], double coefs[]);
double  GMO_CALLCONV d_gmoGetObjL (gmoHandle_t pgmo);
int  GMO_CALLCONV d_gmoGetEquL (gmoHandle_t pgmo, double e[]);
double  GMO_CALLCONV d_gmoGetEquLOne (gmoHandle_t pgmo, int si);
int  GMO_CALLCONV d_gmoSetEquL (gmoHandle_t pgmo, const double el[]);
void  GMO_CALLCONV d_gmoSetEquLOne (gmoHandle_t pgmo, int si, double el);
int  GMO_CALLCONV d_gmoGetEquM (gmoHandle_t pgmo, double pi[]);
double  GMO_CALLCONV d_gmoGetEquMOne (gmoHandle_t pgmo, int si);
int  GMO_CALLCONV d_gmoSetEquM (gmoHandle_t pgmo, const double emarg[]);
char * GMO_CALLCONV d_gmoGetEquNameOne (gmoHandle_t pgmo, int si, char *buf);
char * GMO_CALLCONV d_gmoGetEquNameCustomOne (gmoHandle_t pgmo, int si, const char *suffix, char *buf);
int  GMO_CALLCONV d_gmoGetRhs (gmoHandle_t pgmo, double mdblvec[]);
double  GMO_CALLCONV d_gmoGetRhsOne (gmoHandle_t pgmo, int si);
double  GMO_CALLCONV d_gmoGetRhsOneEx (gmoHandle_t pgmo, int si);
int  GMO_CALLCONV d_gmoSetAltRHS (gmoHandle_t pgmo, const double mdblvec[]);
void  GMO_CALLCONV d_gmoSetAltRHSOne (gmoHandle_t pgmo, int si, double erhs);
int  GMO_CALLCONV d_gmoGetEquSlack (gmoHandle_t pgmo, double mdblvec[]);
double  GMO_CALLCONV d_gmoGetEquSlackOne (gmoHandle_t pgmo, int si);
int  GMO_CALLCONV d_gmoSetEquSlack (gmoHandle_t pgmo, const double mdblvec[]);
int  GMO_CALLCONV d_gmoGetEquType (gmoHandle_t pgmo, int mintvec[]);
int  GMO_CALLCONV d_gmoGetEquTypeOne (gmoHandle_t pgmo, int si);
void  GMO_CALLCONV d_gmoGetEquStat (gmoHandle_t pgmo, int mintvec[]);
int  GMO_CALLCONV d_gmoGetEquStatOne (gmoHandle_t pgmo, int si);
void  GMO_CALLCONV d_gmoSetEquStat (gmoHandle_t pgmo, const int mintvec[]);
void  GMO_CALLCONV d_gmoGetEquCStat (gmoHandle_t pgmo, int mintvec[]);
int  GMO_CALLCONV d_gmoGetEquCStatOne (gmoHandle_t pgmo, int si);
void  GMO_CALLCONV d_gmoSetEquCStat (gmoHandle_t pgmo, const int mintvec[]);
int  GMO_CALLCONV d_gmoGetEquMatch (gmoHandle_t pgmo, int mintvec[]);
int  GMO_CALLCONV d_gmoGetEquMatchOne (gmoHandle_t pgmo, int si);
int  GMO_CALLCONV d_gmoGetEquScale (gmoHandle_t pgmo, double mdblvec[]);
double  GMO_CALLCONV d_gmoGetEquScaleOne (gmoHandle_t pgmo, int si);
int  GMO_CALLCONV d_gmoGetEquStage (gmoHandle_t pgmo, double mdblvec[]);
double  GMO_CALLCONV d_gmoGetEquStageOne (gmoHandle_t pgmo, int si);
int  GMO_CALLCONV d_gmoGetEquOrderOne (gmoHandle_t pgmo, int si);
int  GMO_CALLCONV d_gmoGetRowSparse (gmoHandle_t pgmo, int si, int colidx[], double jacval[], int nlflag[], int *nz, int *nlnz);
int  GMO_CALLCONV d_gmoGetRowSparseEx (gmoHandle_t pgmo, int si, int colidx[], double jacval[], int nlflag[], int *nz, int *qnz, int *nlnz);
void  GMO_CALLCONV d_gmoGetRowJacInfoOne (gmoHandle_t pgmo, int si, void **jacptr, double *jacval, int *colidx, int *nlflag);
int  GMO_CALLCONV d_gmoGetRowQMat (gmoHandle_t pgmo, int si, int varidx1[], int varidx2[], double coefs[]);
int  GMO_CALLCONV d_gmoGetRowQ (gmoHandle_t pgmo, int si, int varidx1[], int varidx2[], double coefs[]);
int  GMO_CALLCONV d_gmoGetRowCVec (gmoHandle_t pgmo, int si, int varidx[], double coefs[]);
double  GMO_CALLCONV d_gmoGetRowQConst (gmoHandle_t pgmo, int si);
int  GMO_CALLCONV d_gmoGetEquIntDotOpt (gmoHandle_t pgmo, void *optptr, const char *dotopt, int optvals[]);
int  GMO_CALLCONV d_gmoGetEquDblDotOpt (gmoHandle_t pgmo, void *optptr, const char *dotopt, double optvals[]);
int  GMO_CALLCONV d_gmoGetVarL (gmoHandle_t pgmo, double x[]);
double  GMO_CALLCONV d_gmoGetVarLOne (gmoHandle_t pgmo, int sj);
int  GMO_CALLCONV d_gmoSetVarL (gmoHandle_t pgmo, const double x[]);
void  GMO_CALLCONV d_gmoSetVarLOne (gmoHandle_t pgmo, int sj, double vl);
int  GMO_CALLCONV d_gmoGetVarM (gmoHandle_t pgmo, double dj[]);
double  GMO_CALLCONV d_gmoGetVarMOne (gmoHandle_t pgmo, int sj);
int  GMO_CALLCONV d_gmoSetVarM (gmoHandle_t pgmo, const double dj[]);
void  GMO_CALLCONV d_gmoSetVarMOne (gmoHandle_t pgmo, int sj, double vmarg);
char * GMO_CALLCONV d_gmoGetVarNameOne (gmoHandle_t pgmo, int sj, char *buf);
char * GMO_CALLCONV d_gmoGetVarNameCustomOne (gmoHandle_t pgmo, int sj, const char *suffix, char *buf);
int  GMO_CALLCONV d_gmoGetVarLower (gmoHandle_t pgmo, double lovec[]);
double  GMO_CALLCONV d_gmoGetVarLowerOne (gmoHandle_t pgmo, int sj);
int  GMO_CALLCONV d_gmoGetVarUpper (gmoHandle_t pgmo, double upvec[]);
double  GMO_CALLCONV d_gmoGetVarUpperOne (gmoHandle_t pgmo, int sj);
int  GMO_CALLCONV d_gmoSetAltVarBounds (gmoHandle_t pgmo, const double lovec[], const double upvec[]);
void  GMO_CALLCONV d_gmoSetAltVarLowerOne (gmoHandle_t pgmo, int sj, double vlo);
void  GMO_CALLCONV d_gmoSetAltVarUpperOne (gmoHandle_t pgmo, int sj, double vup);
int  GMO_CALLCONV d_gmoGetVarType (gmoHandle_t pgmo, int nintvec[]);
int  GMO_CALLCONV d_gmoGetVarTypeOne (gmoHandle_t pgmo, int sj);
int  GMO_CALLCONV d_gmoSetAltVarType (gmoHandle_t pgmo, const int nintvec[]);
void  GMO_CALLCONV d_gmoSetAltVarTypeOne (gmoHandle_t pgmo, int sj, int vtyp);
void  GMO_CALLCONV d_gmoGetVarStat (gmoHandle_t pgmo, int nintvec[]);
int  GMO_CALLCONV d_gmoGetVarStatOne (gmoHandle_t pgmo, int sj);
void  GMO_CALLCONV d_gmoSetVarStat (gmoHandle_t pgmo, const int nintvec[]);
void  GMO_CALLCONV d_gmoSetVarStatOne (gmoHandle_t pgmo, int sj, int vstat);
void  GMO_CALLCONV d_gmoGetVarCStat (gmoHandle_t pgmo, int nintvec[]);
int  GMO_CALLCONV d_gmoGetVarCStatOne (gmoHandle_t pgmo, int sj);
void  GMO_CALLCONV d_gmoSetVarCStat (gmoHandle_t pgmo, const int nintvec[]);
int  GMO_CALLCONV d_gmoGetVarMatch (gmoHandle_t pgmo, int nintvec[]);
int  GMO_CALLCONV d_gmoGetVarMatchOne (gmoHandle_t pgmo, int sj);
int  GMO_CALLCONV d_gmoGetVarPrior (gmoHandle_t pgmo, double ndblvec[]);
double  GMO_CALLCONV d_gmoGetVarPriorOne (gmoHandle_t pgmo, int sj);
int  GMO_CALLCONV d_gmoGetVarScale (gmoHandle_t pgmo, double ndblvec[]);
double  GMO_CALLCONV d_gmoGetVarScaleOne (gmoHandle_t pgmo, int sj);
int  GMO_CALLCONV d_gmoGetVarStage (gmoHandle_t pgmo, double ndblvec[]);
double  GMO_CALLCONV d_gmoGetVarStageOne (gmoHandle_t pgmo, int sj);
int  GMO_CALLCONV d_gmoGetSosConstraints (gmoHandle_t pgmo, int sostype[], int sosbeg[], int sosind[], double soswt[]);
int  GMO_CALLCONV d_gmoGetVarSosSetOne (gmoHandle_t pgmo, int sj);
int  GMO_CALLCONV d_gmoGetColSparse (gmoHandle_t pgmo, int sj, int rowidx[], double jacval[], int nlflag[], int *nz, int *nlnz);
void  GMO_CALLCONV d_gmoGetColJacInfoOne (gmoHandle_t pgmo, int sj, void **jacptr, double *jacval, int *rowidx, int *nlflag);
int  GMO_CALLCONV d_gmoGetVarIntDotOpt (gmoHandle_t pgmo, void *optptr, const char *dotopt, int optvals[]);
int  GMO_CALLCONV d_gmoGetVarDblDotOpt (gmoHandle_t pgmo, void *optptr, const char *dotopt, double optvals[]);
void  GMO_CALLCONV d_gmoEvalErrorMsg (gmoHandle_t pgmo, int domsg);
void  GMO_CALLCONV d_gmoEvalErrorMsg_MT (gmoHandle_t pgmo, int domsg, int tidx);
void  GMO_CALLCONV d_gmoEvalErrorMaskLevel (gmoHandle_t pgmo, int MaskLevel);
void  GMO_CALLCONV d_gmoEvalErrorMaskLevel_MT (gmoHandle_t pgmo, int MaskLevel, int tidx);
int  GMO_CALLCONV d_gmoEvalNewPoint (gmoHandle_t pgmo, const double x[]);
void  GMO_CALLCONV d_gmoSetExtFuncs (gmoHandle_t pgmo, void *extfunmgr);
int  GMO_CALLCONV d_gmoGetQMakerStats (gmoHandle_t pgmo, char *algName, double *algTime, INT64 *winnerCount3Pass, INT64 *winnerCountDblFwd);
int  GMO_CALLCONV d_gmoEvalFunc (gmoHandle_t pgmo, int si, const double x[], double *f, int *numerr);
int  GMO_CALLCONV d_gmoEvalFunc_MT (gmoHandle_t pgmo, int si, const double x[], double *f, int *numerr, int tidx);
int  GMO_CALLCONV d_gmoEvalFuncInt (gmoHandle_t pgmo, int si, double *f, int *numerr);
int  GMO_CALLCONV d_gmoEvalFuncInt_MT (gmoHandle_t pgmo, int si, double *f, int *numerr, int tidx);
int  GMO_CALLCONV d_gmoEvalFuncNL (gmoHandle_t pgmo, int si, const double x[], double *fnl, int *numerr);
int  GMO_CALLCONV d_gmoEvalFuncNL_MT (gmoHandle_t pgmo, int si, const double x[], double *fnl, int *numerr, int tidx);
int  GMO_CALLCONV d_gmoEvalFuncObj (gmoHandle_t pgmo, const double x[], double *f, int *numerr);
int  GMO_CALLCONV d_gmoEvalFuncNLObj (gmoHandle_t pgmo, const double x[], double *fnl, int *numerr);
int  GMO_CALLCONV d_gmoEvalFuncInterval (gmoHandle_t pgmo, int si, const double xmin[], const double xmax[], double *fmin, double *fmax, int *numerr);
int  GMO_CALLCONV d_gmoEvalFuncInterval_MT (gmoHandle_t pgmo, int si, const double xmin[], const double xmax[], double *fmin, double *fmax, int *numerr, int tidx);
int  GMO_CALLCONV d_gmoEvalGrad (gmoHandle_t pgmo, int si, const double x[], double *f, double g[], double *gx, int *numerr);
int  GMO_CALLCONV d_gmoEvalGrad_MT (gmoHandle_t pgmo, int si, const double x[], double *f, double g[], double *gx, int *numerr, int tidx);
int  GMO_CALLCONV d_gmoEvalGradNL (gmoHandle_t pgmo, int si, const double x[], double *fnl, double g[], double *gxnl, int *numerr);
int  GMO_CALLCONV d_gmoEvalGradNL_MT (gmoHandle_t pgmo, int si, const double x[], double *fnl, double g[], double *gxnl, int *numerr, int tidx);
int  GMO_CALLCONV d_gmoEvalGradObj (gmoHandle_t pgmo, const double x[], double *f, double g[], double *gx, int *numerr);
int  GMO_CALLCONV d_gmoEvalGradNLObj (gmoHandle_t pgmo, const double x[], double *fnl, double g[], double *gxnl, int *numerr);
int  GMO_CALLCONV d_gmoEvalGradInterval (gmoHandle_t pgmo, int si, const double xmin[], const double xmax[], double *fmin, double *fmax, double gmin[], double gmax[], int *numerr);
int  GMO_CALLCONV d_gmoEvalGradInterval_MT (gmoHandle_t pgmo, int si, const double xmin[], const double xmax[], double *fmin, double *fmax, double gmin[], double gmax[], int *numerr, int tidx);
int  GMO_CALLCONV d_gmoEvalGradNLUpdate (gmoHandle_t pgmo, double rhsdelta[], int dojacupd, int *numerr);
int  GMO_CALLCONV d_gmoGetJacUpdate (gmoHandle_t pgmo, int rowidx[], int colidx[], double jacval[], int *len);
int  GMO_CALLCONV d_gmoHessLoad (gmoHandle_t pgmo, double maxJacMult, int *do2dir, int *doHess);
int  GMO_CALLCONV d_gmoHessUnload (gmoHandle_t pgmo);
int  GMO_CALLCONV d_gmoHessDim (gmoHandle_t pgmo, int si);
int  GMO_CALLCONV d_gmoHessNz (gmoHandle_t pgmo, int si);
INT64  GMO_CALLCONV d_gmoHessNz64 (gmoHandle_t pgmo, int si);
int  GMO_CALLCONV d_gmoHessStruct (gmoHandle_t pgmo, int si, int hridx[], int hcidx[], int *hessdim, int *hessnz);
int  GMO_CALLCONV d_gmoHessStruct64 (gmoHandle_t pgmo, int si, int hridx[], int hcidx[], int *hessdim, INT64 *hessnz);
int  GMO_CALLCONV d_gmoHessValue (gmoHandle_t pgmo, int si, int hridx[], int hcidx[], int *hessdim, int *hessnz, const double x[], double hessval[], int *numerr);
int  GMO_CALLCONV d_gmoHessValue64 (gmoHandle_t pgmo, int si, int hridx[], int hcidx[], int *hessdim, INT64 *hessnz, const double x[], double hessval[], int *numerr);
int  GMO_CALLCONV d_gmoHessVec (gmoHandle_t pgmo, int si, const double x[], const double dx[], double Wdx[], int *numerr);
int  GMO_CALLCONV d_gmoHessLagStruct (gmoHandle_t pgmo, int WRindex[], int WCindex[]);
int  GMO_CALLCONV d_gmoHessLagValue (gmoHandle_t pgmo, const double x[], const double pi[], double w[], double objweight, double conweight, int *numerr);
int  GMO_CALLCONV d_gmoHessLagVec (gmoHandle_t pgmo, const double x[], const double pi[], const double dx[], double Wdx[], double objweight, double conweight, int *numerr);
int  GMO_CALLCONV d_gmoLoadEMPInfo (gmoHandle_t pgmo, const char *empinfofname);
int  GMO_CALLCONV d_gmoGetEquVI (gmoHandle_t pgmo, int mintvec[]);
int  GMO_CALLCONV d_gmoGetEquVIOne (gmoHandle_t pgmo, int si);
int  GMO_CALLCONV d_gmoGetVarVI (gmoHandle_t pgmo, int nintvec[]);
int  GMO_CALLCONV d_gmoGetVarVIOne (gmoHandle_t pgmo, int sj);
int  GMO_CALLCONV d_gmoGetAgentType (gmoHandle_t pgmo, int agentvec[]);
int  GMO_CALLCONV d_gmoGetAgentTypeOne (gmoHandle_t pgmo, int aidx);
int  GMO_CALLCONV d_gmoGetBiLevelInfo (gmoHandle_t pgmo, int nintvec[], int mintvec[]);
int  GMO_CALLCONV d_gmoDumpEMPInfoToGDX (gmoHandle_t pgmo, const char *gdxfname);
double  GMO_CALLCONV d_gmoGetHeadnTail (gmoHandle_t pgmo, int htrec);
void  GMO_CALLCONV d_gmoSetHeadnTail (gmoHandle_t pgmo, int htrec, double dval);
int  GMO_CALLCONV d_gmoSetSolutionPrimal (gmoHandle_t pgmo, const double x[]);
int  GMO_CALLCONV d_gmoSetSolution2 (gmoHandle_t pgmo, const double x[], const double pi[]);
int  GMO_CALLCONV d_gmoSetSolution (gmoHandle_t pgmo, const double x[], const double dj[], const double pi[], const double e[]);
int  GMO_CALLCONV d_gmoSetSolution8 (gmoHandle_t pgmo, const double x[], const double dj[], const double pi[], const double e[], int xb[], int xs[], int yb[], int ys[]);
int  GMO_CALLCONV d_gmoSetSolutionFixer (gmoHandle_t pgmo, int modelstathint, const double x[], const double pi[], const int xb[], const int yb[], double infTol, double optTol);
int  GMO_CALLCONV d_gmoGetSolutionVarRec (gmoHandle_t pgmo, int sj, double *vl, double *vmarg, int *vstat, int *vcstat);
int  GMO_CALLCONV d_gmoSetSolutionVarRec (gmoHandle_t pgmo, int sj, double vl, double vmarg, int vstat, int vcstat);
int  GMO_CALLCONV d_gmoGetSolutionEquRec (gmoHandle_t pgmo, int si, double *el, double *emarg, int *estat, int *ecstat);
int  GMO_CALLCONV d_gmoSetSolutionEquRec (gmoHandle_t pgmo, int si, double el, double emarg, int estat, int ecstat);
int  GMO_CALLCONV d_gmoSetSolutionStatus (gmoHandle_t pgmo, int xb[], int xs[], int yb[], int ys[]);
void  GMO_CALLCONV d_gmoCompleteObjective (gmoHandle_t pgmo, double locobjval);
int  GMO_CALLCONV d_gmoCompleteSolution (gmoHandle_t pgmo);
double  GMO_CALLCONV d_gmoGetAbsoluteGap (gmoHandle_t pgmo);
double  GMO_CALLCONV d_gmoGetRelativeGap (gmoHandle_t pgmo);
int  GMO_CALLCONV d_gmoLoadSolutionLegacy (gmoHandle_t pgmo);
int  GMO_CALLCONV d_gmoUnloadSolutionLegacy (gmoHandle_t pgmo);
int  GMO_CALLCONV d_gmoLoadSolutionGDX (gmoHandle_t pgmo, const char *gdxfname, int dorows, int docols, int doht);
int  GMO_CALLCONV d_gmoUnloadSolutionGDX (gmoHandle_t pgmo, const char *gdxfname, int dorows, int docols, int doht);
int  GMO_CALLCONV d_gmoPrepareAllSolToGDX (gmoHandle_t pgmo, const char *gdxfname, void *scengdx, int dictid);
int  GMO_CALLCONV d_gmoAddSolutionToGDX (gmoHandle_t pgmo, const char *scenuel[]);
int  GMO_CALLCONV d_gmoWriteSolDone (gmoHandle_t pgmo, char *msg);
int  GMO_CALLCONV d_gmoCheckSolPoolUEL (gmoHandle_t pgmo, const char *prefix, int *numsym);
void * GMO_CALLCONV d_gmoPrepareSolPoolMerge (gmoHandle_t pgmo, const char *gdxfname, int numsol, const char *prefix);
int  GMO_CALLCONV d_gmoPrepareSolPoolNextSym (gmoHandle_t pgmo, void *handle);
int  GMO_CALLCONV d_gmoUnloadSolPoolSolution (gmoHandle_t pgmo, void *handle, int numsol);
int  GMO_CALLCONV d_gmoFinalizeSolPoolMerge (gmoHandle_t pgmo, void *handle);
int  GMO_CALLCONV d_gmoGetVarTypeTxt (gmoHandle_t pgmo, int sj, char *s);
int  GMO_CALLCONV d_gmoGetEquTypeTxt (gmoHandle_t pgmo, int si, char *s);
int  GMO_CALLCONV d_gmoGetSolveStatusTxt (gmoHandle_t pgmo, int solvestat, char *s);
int  GMO_CALLCONV d_gmoGetModelStatusTxt (gmoHandle_t pgmo, int modelstat, char *s);
int  GMO_CALLCONV d_gmoGetModelTypeTxt (gmoHandle_t pgmo, int modeltype, char *s);
int  GMO_CALLCONV d_gmoGetHeadNTailTxt (gmoHandle_t pgmo, int htrec, char *s);
double  GMO_CALLCONV d_gmoMemUsed (gmoHandle_t pgmo);
double  GMO_CALLCONV d_gmoPeakMemUsed (gmoHandle_t pgmo);
int  GMO_CALLCONV d_gmoSetNLObject (gmoHandle_t pgmo, void *nlobject, void *nlpool);
int  GMO_CALLCONV d_gmoDumpQMakerGDX (gmoHandle_t pgmo, const char *gdxfname);
int  GMO_CALLCONV d_gmoGetVarEquMap (gmoHandle_t pgmo, int maptype, void *optptr, int strict, int *nmappings, int rowindex[], int colindex[], int mapval[]);
int  GMO_CALLCONV d_gmoGetIndicatorMap (gmoHandle_t pgmo, void *optptr, int indicstrict, int *numindic, int rowindic[], int colindic[], int indiconval[]);
int  GMO_CALLCONV d_gmoCrudeness (gmoHandle_t pgmo);
int  GMO_CALLCONV d_gmoDirtyGetRowFNLInstr (gmoHandle_t pgmo, int si, int *len, int opcode[], int field[]);
int  GMO_CALLCONV d_gmoDirtyGetObjFNLInstr (gmoHandle_t pgmo, int *len, int opcode[], int field[]);
int  GMO_CALLCONV d_gmoDirtySetRowFNLInstr (gmoHandle_t pgmo, int si, int len, const int opcode[], const int field[], void *nlpool, double nlpoolvec[], int len2);
char * GMO_CALLCONV d_gmoGetExtrLibName (gmoHandle_t pgmo, int libidx, char *buf);
void * GMO_CALLCONV d_gmoGetExtrLibObjPtr (gmoHandle_t pgmo, int libidx);
char * GMO_CALLCONV d_gmoGetExtrLibFuncName (gmoHandle_t pgmo, int libidx, int funcidx, char *buf);
void * GMO_CALLCONV d_gmoLoadExtrLibEntry (gmoHandle_t pgmo, int libidx, const char *name, char *msg);
void * GMO_CALLCONV d_gmoDict (gmoHandle_t pgmo);
void GMO_CALLCONV d_gmoDictSet (gmoHandle_t pgmo, const void *x);
char * GMO_CALLCONV d_gmoNameModel (gmoHandle_t pgmo, char *buf);
void GMO_CALLCONV d_gmoNameModelSet (gmoHandle_t pgmo, const char *x);
int  GMO_CALLCONV d_gmoModelSeqNr (gmoHandle_t pgmo);
void GMO_CALLCONV d_gmoModelSeqNrSet (gmoHandle_t pgmo, const int x);
int  GMO_CALLCONV d_gmoModelType (gmoHandle_t pgmo);
void GMO_CALLCONV d_gmoModelTypeSet (gmoHandle_t pgmo, const int x);
int  GMO_CALLCONV d_gmoNLModelType (gmoHandle_t pgmo);
int  GMO_CALLCONV d_gmoSense (gmoHandle_t pgmo);
void GMO_CALLCONV d_gmoSenseSet (gmoHandle_t pgmo, const int x);
int  GMO_CALLCONV d_gmoIsQP (gmoHandle_t pgmo);
int  GMO_CALLCONV d_gmoOptFile (gmoHandle_t pgmo);
void GMO_CALLCONV d_gmoOptFileSet (gmoHandle_t pgmo, const int x);
int  GMO_CALLCONV d_gmoDictionary (gmoHandle_t pgmo);
void GMO_CALLCONV d_gmoDictionarySet (gmoHandle_t pgmo, const int x);
int  GMO_CALLCONV d_gmoScaleOpt (gmoHandle_t pgmo);
void GMO_CALLCONV d_gmoScaleOptSet (gmoHandle_t pgmo, const int x);
int  GMO_CALLCONV d_gmoPriorOpt (gmoHandle_t pgmo);
void GMO_CALLCONV d_gmoPriorOptSet (gmoHandle_t pgmo, const int x);
int  GMO_CALLCONV d_gmoHaveBasis (gmoHandle_t pgmo);
void GMO_CALLCONV d_gmoHaveBasisSet (gmoHandle_t pgmo, const int x);
int  GMO_CALLCONV d_gmoModelStat (gmoHandle_t pgmo);
void GMO_CALLCONV d_gmoModelStatSet (gmoHandle_t pgmo, const int x);
int  GMO_CALLCONV d_gmoSolveStat (gmoHandle_t pgmo);
void GMO_CALLCONV d_gmoSolveStatSet (gmoHandle_t pgmo, const int x);
int  GMO_CALLCONV d_gmoIsMPSGE (gmoHandle_t pgmo);
void GMO_CALLCONV d_gmoIsMPSGESet (gmoHandle_t pgmo, const int x);
int  GMO_CALLCONV d_gmoObjStyle (gmoHandle_t pgmo);
void GMO_CALLCONV d_gmoObjStyleSet (gmoHandle_t pgmo, const int x);
int  GMO_CALLCONV d_gmoInterface (gmoHandle_t pgmo);
void GMO_CALLCONV d_gmoInterfaceSet (gmoHandle_t pgmo, const int x);
int  GMO_CALLCONV d_gmoIndexBase (gmoHandle_t pgmo);
void GMO_CALLCONV d_gmoIndexBaseSet (gmoHandle_t pgmo, const int x);
int  GMO_CALLCONV d_gmoObjReform (gmoHandle_t pgmo);
void GMO_CALLCONV d_gmoObjReformSet (gmoHandle_t pgmo, const int x);
int  GMO_CALLCONV d_gmoEmptyOut (gmoHandle_t pgmo);
void GMO_CALLCONV d_gmoEmptyOutSet (gmoHandle_t pgmo, const int x);
int  GMO_CALLCONV d_gmoIgnXCDeriv (gmoHandle_t pgmo);
void GMO_CALLCONV d_gmoIgnXCDerivSet (gmoHandle_t pgmo, const int x);
int  GMO_CALLCONV d_gmoUseQ (gmoHandle_t pgmo);
void GMO_CALLCONV d_gmoUseQSet (gmoHandle_t pgmo, const int x);
int  GMO_CALLCONV d_gmoQExtractAlg (gmoHandle_t pgmo);
void GMO_CALLCONV d_gmoQExtractAlgSet (gmoHandle_t pgmo, const int x);
int  GMO_CALLCONV d_gmoAltBounds (gmoHandle_t pgmo);
void GMO_CALLCONV d_gmoAltBoundsSet (gmoHandle_t pgmo, const int x);
int  GMO_CALLCONV d_gmoAltRHS (gmoHandle_t pgmo);
void GMO_CALLCONV d_gmoAltRHSSet (gmoHandle_t pgmo, const int x);
int  GMO_CALLCONV d_gmoAltVarTypes (gmoHandle_t pgmo);
void GMO_CALLCONV d_gmoAltVarTypesSet (gmoHandle_t pgmo, const int x);
int  GMO_CALLCONV d_gmoForceLinear (gmoHandle_t pgmo);
void GMO_CALLCONV d_gmoForceLinearSet (gmoHandle_t pgmo, const int x);
int  GMO_CALLCONV d_gmoForceCont (gmoHandle_t pgmo);
void GMO_CALLCONV d_gmoForceContSet (gmoHandle_t pgmo, const int x);
int  GMO_CALLCONV d_gmoPermuteCols (gmoHandle_t pgmo);
void GMO_CALLCONV d_gmoPermuteColsSet (gmoHandle_t pgmo, const int x);
int  GMO_CALLCONV d_gmoPermuteRows (gmoHandle_t pgmo);
void GMO_CALLCONV d_gmoPermuteRowsSet (gmoHandle_t pgmo, const int x);
double  GMO_CALLCONV d_gmoPinf (gmoHandle_t pgmo);
void GMO_CALLCONV d_gmoPinfSet (gmoHandle_t pgmo, const double x);
double  GMO_CALLCONV d_gmoMinf (gmoHandle_t pgmo);
void GMO_CALLCONV d_gmoMinfSet (gmoHandle_t pgmo, const double x);
double  GMO_CALLCONV d_gmoQNaN (gmoHandle_t pgmo);
double  GMO_CALLCONV d_gmoValNA (gmoHandle_t pgmo);
int  GMO_CALLCONV d_gmoValNAInt (gmoHandle_t pgmo);
double  GMO_CALLCONV d_gmoValUndf (gmoHandle_t pgmo);
int  GMO_CALLCONV d_gmoM (gmoHandle_t pgmo);
int  GMO_CALLCONV d_gmoQM (gmoHandle_t pgmo);
int  GMO_CALLCONV d_gmoNLM (gmoHandle_t pgmo);
int  GMO_CALLCONV d_gmoNRowMatch (gmoHandle_t pgmo);
int  GMO_CALLCONV d_gmoN (gmoHandle_t pgmo);
int  GMO_CALLCONV d_gmoNLN (gmoHandle_t pgmo);
int  GMO_CALLCONV d_gmoNDisc (gmoHandle_t pgmo);
int  GMO_CALLCONV d_gmoNFixed (gmoHandle_t pgmo);
int  GMO_CALLCONV d_gmoNColMatch (gmoHandle_t pgmo);
int  GMO_CALLCONV d_gmoNZ (gmoHandle_t pgmo);
INT64  GMO_CALLCONV d_gmoNZ64 (gmoHandle_t pgmo);
int  GMO_CALLCONV d_gmoNLNZ (gmoHandle_t pgmo);
INT64  GMO_CALLCONV d_gmoNLNZ64 (gmoHandle_t pgmo);
int  GMO_CALLCONV d_gmoLNZEx (gmoHandle_t pgmo);
INT64  GMO_CALLCONV d_gmoLNZEx64 (gmoHandle_t pgmo);
int  GMO_CALLCONV d_gmoLNZ (gmoHandle_t pgmo);
INT64  GMO_CALLCONV d_gmoLNZ64 (gmoHandle_t pgmo);
int  GMO_CALLCONV d_gmoQNZ (gmoHandle_t pgmo);
INT64  GMO_CALLCONV d_gmoQNZ64 (gmoHandle_t pgmo);
int  GMO_CALLCONV d_gmoGNLNZ (gmoHandle_t pgmo);
INT64  GMO_CALLCONV d_gmoGNLNZ64 (gmoHandle_t pgmo);
int  GMO_CALLCONV d_gmoMaxQNZ (gmoHandle_t pgmo);
INT64  GMO_CALLCONV d_gmoMaxQNZ64 (gmoHandle_t pgmo);
int  GMO_CALLCONV d_gmoObjNZ (gmoHandle_t pgmo);
int  GMO_CALLCONV d_gmoObjLNZ (gmoHandle_t pgmo);
int  GMO_CALLCONV d_gmoObjQNZEx (gmoHandle_t pgmo);
int  GMO_CALLCONV d_gmoObjNLNZ (gmoHandle_t pgmo);
int  GMO_CALLCONV d_gmoObjNLNZEx (gmoHandle_t pgmo);
int  GMO_CALLCONV d_gmoObjQMatNZ (gmoHandle_t pgmo);
INT64  GMO_CALLCONV d_gmoObjQMatNZ64 (gmoHandle_t pgmo);
int  GMO_CALLCONV d_gmoObjQNZ (gmoHandle_t pgmo);
int  GMO_CALLCONV d_gmoObjQDiagNZ (gmoHandle_t pgmo);
int  GMO_CALLCONV d_gmoObjCVecNZ (gmoHandle_t pgmo);
int  GMO_CALLCONV d_gmoNLConst (gmoHandle_t pgmo);
void GMO_CALLCONV d_gmoNLConstSet (gmoHandle_t pgmo, const int x);
int  GMO_CALLCONV d_gmoNLCodeSize (gmoHandle_t pgmo);
void GMO_CALLCONV d_gmoNLCodeSizeSet (gmoHandle_t pgmo, const int x);
int  GMO_CALLCONV d_gmoNLCodeSizeMaxRow (gmoHandle_t pgmo);
int  GMO_CALLCONV d_gmoObjVar (gmoHandle_t pgmo);
void GMO_CALLCONV d_gmoObjVarSet (gmoHandle_t pgmo, const int x);
int  GMO_CALLCONV d_gmoObjRow (gmoHandle_t pgmo);
int  GMO_CALLCONV d_gmoGetObjOrder (gmoHandle_t pgmo);
double  GMO_CALLCONV d_gmoObjConst (gmoHandle_t pgmo);
double  GMO_CALLCONV d_gmoObjConstEx (gmoHandle_t pgmo);
double  GMO_CALLCONV d_gmoObjQConst (gmoHandle_t pgmo);
double  GMO_CALLCONV d_gmoObjJacVal (gmoHandle_t pgmo);
int  GMO_CALLCONV d_gmoEvalErrorMethod (gmoHandle_t pgmo);
void GMO_CALLCONV d_gmoEvalErrorMethodSet (gmoHandle_t pgmo, const int x);
int  GMO_CALLCONV d_gmoEvalMaxThreads (gmoHandle_t pgmo);
void GMO_CALLCONV d_gmoEvalMaxThreadsSet (gmoHandle_t pgmo, const int x);
int  GMO_CALLCONV d_gmoEvalFuncCount (gmoHandle_t pgmo);
double  GMO_CALLCONV d_gmoEvalFuncTimeUsed (gmoHandle_t pgmo);
int  GMO_CALLCONV d_gmoEvalGradCount (gmoHandle_t pgmo);
double  GMO_CALLCONV d_gmoEvalGradTimeUsed (gmoHandle_t pgmo);
int  GMO_CALLCONV d_gmoHessMaxDim (gmoHandle_t pgmo);
int  GMO_CALLCONV d_gmoHessMaxNz (gmoHandle_t pgmo);
INT64  GMO_CALLCONV d_gmoHessMaxNz64 (gmoHandle_t pgmo);
int  GMO_CALLCONV d_gmoHessLagDim (gmoHandle_t pgmo);
int  GMO_CALLCONV d_gmoHessLagNz (gmoHandle_t pgmo);
INT64  GMO_CALLCONV d_gmoHessLagNz64 (gmoHandle_t pgmo);
int  GMO_CALLCONV d_gmoHessLagDiagNz (gmoHandle_t pgmo);
int  GMO_CALLCONV d_gmoHessInclQRows (gmoHandle_t pgmo);
void GMO_CALLCONV d_gmoHessInclQRowsSet (gmoHandle_t pgmo, const int x);
int  GMO_CALLCONV d_gmoNumVIFunc (gmoHandle_t pgmo);
int  GMO_CALLCONV d_gmoNumAgents (gmoHandle_t pgmo);
char * GMO_CALLCONV d_gmoNameOptFile (gmoHandle_t pgmo, char *buf);
void GMO_CALLCONV d_gmoNameOptFileSet (gmoHandle_t pgmo, const char *x);
char * GMO_CALLCONV d_gmoNameSolFile (gmoHandle_t pgmo, char *buf);
void GMO_CALLCONV d_gmoNameSolFileSet (gmoHandle_t pgmo, const char *x);
char * GMO_CALLCONV d_gmoNameXLib (gmoHandle_t pgmo, char *buf);
void GMO_CALLCONV d_gmoNameXLibSet (gmoHandle_t pgmo, const char *x);
char * GMO_CALLCONV d_gmoNameMatrix (gmoHandle_t pgmo, char *buf);
char * GMO_CALLCONV d_gmoNameDict (gmoHandle_t pgmo, char *buf);
void GMO_CALLCONV d_gmoNameDictSet (gmoHandle_t pgmo, const char *x);
char * GMO_CALLCONV d_gmoNameInput (gmoHandle_t pgmo, char *buf);
void GMO_CALLCONV d_gmoNameInputSet (gmoHandle_t pgmo, const char *x);
char * GMO_CALLCONV d_gmoNameOutput (gmoHandle_t pgmo, char *buf);
void * GMO_CALLCONV d_gmoPPool (gmoHandle_t pgmo);
void * GMO_CALLCONV d_gmoIOMutex (gmoHandle_t pgmo);
int  GMO_CALLCONV d_gmoError (gmoHandle_t pgmo);
void GMO_CALLCONV d_gmoErrorSet (gmoHandle_t pgmo, const int x);
char * GMO_CALLCONV d_gmoErrorMessage (gmoHandle_t pgmo, char *buf);


typedef int  (GMO_CALLCONV *gmoInitData_t) (gmoHandle_t pgmo, int rows, int cols, int codelen);
/** Initialize GMO data
 *
 * @param pgmo gmo object handle
 * @param rows Number of rows
 * @param cols Number of columns
 * @param codelen length of NL code
 */
GMO_FUNCPTR(gmoInitData);

typedef int  (GMO_CALLCONV *gmoAddRow_t) (gmoHandle_t pgmo, int etyp, int ematch, double eslack, double escale, double erhs, double emarg, int ebas, int enz, const int colidx[], const double jacval[], const int nlflag[]);
/** Add a row
 *
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
GMO_FUNCPTR(gmoAddRow);

typedef int  (GMO_CALLCONV *gmoAddCol_t) (gmoHandle_t pgmo, int vtyp, double vlo, double vl, double vup, double vmarg, int vbas, int vsos, double vprior, double vscale, int vnz, const int rowidx[], const double jacval[], const int nlflag[]);
/** Add a column
 *
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
GMO_FUNCPTR(gmoAddCol);

typedef int  (GMO_CALLCONV *gmoCompleteData_t) (gmoHandle_t pgmo, char *msg);
/** Complete GMO data instance
 *
 * @param pgmo gmo object handle
 * @param msg Message
 */
GMO_FUNCPTR(gmoCompleteData);

typedef int  (GMO_CALLCONV *gmoFillMatches_t) (gmoHandle_t pgmo, char *msg);
/** Complete matching information for MCP
 *
 * @param pgmo gmo object handle
 * @param msg Message
 */
GMO_FUNCPTR(gmoFillMatches);

typedef int  (GMO_CALLCONV *gmoLoadDataLegacy_t) (gmoHandle_t pgmo, char *msg);
/** Read instance from scratch files - Legacy Mode - without gmoFillMatches
 *
 * @param pgmo gmo object handle
 * @param msg Message
 */
GMO_FUNCPTR(gmoLoadDataLegacy);

typedef int  (GMO_CALLCONV *gmoLoadDataLegacyEx_t) (gmoHandle_t pgmo, int fillMatches, char *msg);
/** Read instance from scratch files - Legacy Mode
 *
 * @param pgmo gmo object handle
 * @param fillMatches controls gmoFillMatches call during the load
 * @param msg Message
 */
GMO_FUNCPTR(gmoLoadDataLegacyEx);

typedef int  (GMO_CALLCONV *gmoRegisterEnvironment_t) (gmoHandle_t pgmo, void *gevptr, char *msg);
/** Register GAMS environment
 *
 * @param pgmo gmo object handle
 * @param gevptr 
 * @param msg Message
 */
GMO_FUNCPTR(gmoRegisterEnvironment);

typedef void * (GMO_CALLCONV *gmoEnvironment_t) (gmoHandle_t pgmo);
/** Get GAMS environment object pointer
 *
 * @param pgmo gmo object handle
 */
GMO_FUNCPTR(gmoEnvironment);

typedef void * (GMO_CALLCONV *gmoViewStore_t) (gmoHandle_t pgmo);
/** Store current view in view object
 *
 * @param pgmo gmo object handle
 */
GMO_FUNCPTR(gmoViewStore);

typedef void  (GMO_CALLCONV *gmoViewRestore_t) (gmoHandle_t pgmo, void **viewptr);
/** Restore view
 *
 * @param pgmo gmo object handle
 * @param viewptr Pointer to structure storing the view of a model
 */
GMO_FUNCPTR(gmoViewRestore);

typedef void  (GMO_CALLCONV *gmoViewDump_t) (gmoHandle_t pgmo);
/** Dump current view to stdout
 *
 * @param pgmo gmo object handle
 */
GMO_FUNCPTR(gmoViewDump);

typedef int  (GMO_CALLCONV *gmoGetiSolver_t) (gmoHandle_t pgmo, int mi);
/** Get equation index in solver space
 *
 * @param pgmo gmo object handle
 * @param mi Index of row in original/GAMS space
 */
GMO_FUNCPTR(gmoGetiSolver);

typedef int  (GMO_CALLCONV *gmoGetjSolver_t) (gmoHandle_t pgmo, int mj);
/** Get variable index in solver space
 *
 * @param pgmo gmo object handle
 * @param mj Index of column original/GAMS client space
 */
GMO_FUNCPTR(gmoGetjSolver);

typedef int  (GMO_CALLCONV *gmoGetiSolverQuiet_t) (gmoHandle_t pgmo, int mi);
/** Get equation index in solver space (without error message; negative if it fails)
 *
 * @param pgmo gmo object handle
 * @param mi Index of row in original/GAMS space
 */
GMO_FUNCPTR(gmoGetiSolverQuiet);

typedef int  (GMO_CALLCONV *gmoGetjSolverQuiet_t) (gmoHandle_t pgmo, int mj);
/** Get variable index in solver space (without error message; negative if it fails)
 *
 * @param pgmo gmo object handle
 * @param mj Index of column original/GAMS client space
 */
GMO_FUNCPTR(gmoGetjSolverQuiet);

typedef int  (GMO_CALLCONV *gmoGetiModel_t) (gmoHandle_t pgmo, int si);
/** Get equation index in model (original) space
 *
 * @param pgmo gmo object handle
 * @param si Index of row in client space
 */
GMO_FUNCPTR(gmoGetiModel);

typedef int  (GMO_CALLCONV *gmoGetjModel_t) (gmoHandle_t pgmo, int sj);
/** Get variable index in model (original) space
 *
 * @param pgmo gmo object handle
 * @param sj Index of column in client space
 */
GMO_FUNCPTR(gmoGetjModel);

typedef int  (GMO_CALLCONV *gmoSetEquPermutation_t) (gmoHandle_t pgmo, int permut[]);
/** Set Permutation vectors for equations (model view)
 *
 * @param pgmo gmo object handle
 * @param permut Permutation vector (original/GAMS to client)
 */
GMO_FUNCPTR(gmoSetEquPermutation);

typedef int  (GMO_CALLCONV *gmoSetRvEquPermutation_t) (gmoHandle_t pgmo, int rvpermut[], int len);
/** Set Permutation vectors for equations (solver view)
 *
 * @param pgmo gmo object handle
 * @param rvpermut Reverse permutation vector (client to original/GAMS)
 * @param len Length of array
 */
GMO_FUNCPTR(gmoSetRvEquPermutation);

typedef int  (GMO_CALLCONV *gmoSetVarPermutation_t) (gmoHandle_t pgmo, int permut[]);
/** Set Permutation vectors for variables (model view)
 *
 * @param pgmo gmo object handle
 * @param permut Permutation vector (original/GAMS to client)
 */
GMO_FUNCPTR(gmoSetVarPermutation);

typedef int  (GMO_CALLCONV *gmoSetRvVarPermutation_t) (gmoHandle_t pgmo, int rvpermut[], int len);
/** Set Permutation vectors for variables (solver view)
 *
 * @param pgmo gmo object handle
 * @param rvpermut Reverse permutation vector (client to original/GAMS)
 * @param len Length of array
 */
GMO_FUNCPTR(gmoSetRvVarPermutation);

typedef int  (GMO_CALLCONV *gmoSetNRowPerm_t) (gmoHandle_t pgmo);
/** Set Permutation to skip =n= rows
 *
 * @param pgmo gmo object handle
 */
GMO_FUNCPTR(gmoSetNRowPerm);

typedef int  (GMO_CALLCONV *gmoGetVarTypeCnt_t) (gmoHandle_t pgmo, int vtyp);
/** Get variable type count
 *
 * @param pgmo gmo object handle
 * @param vtyp Type of variable (see enumerated constants)
 */
GMO_FUNCPTR(gmoGetVarTypeCnt);

typedef int  (GMO_CALLCONV *gmoGetEquTypeCnt_t) (gmoHandle_t pgmo, int etyp);
/** Get equation type count
 *
 * @param pgmo gmo object handle
 * @param etyp Type of equation (see enumerated constants)
 */
GMO_FUNCPTR(gmoGetEquTypeCnt);

typedef int  (GMO_CALLCONV *gmoGetObjStat_t) (gmoHandle_t pgmo, int *nz, int *qnz, int *nlnz);
/** Get obj counts
 *
 * @param pgmo gmo object handle
 * @param nz Number of nonzeros
 * @param qnz Number of quadratic nonzeros in Jacobian matrix
 * @param nlnz Number of nonlinear nonzeros
 */
GMO_FUNCPTR(gmoGetObjStat);

typedef int  (GMO_CALLCONV *gmoGetRowStat_t) (gmoHandle_t pgmo, int si, int *nz, int *qnz, int *nlnz);
/** Get row counts
 *
 * @param pgmo gmo object handle
 * @param si Index of row in client space
 * @param nz Number of nonzeros
 * @param qnz Number of quadratic nonzeros in Jacobian matrix
 * @param nlnz Number of nonlinear nonzeros
 */
GMO_FUNCPTR(gmoGetRowStat);

typedef int  (GMO_CALLCONV *gmoGetRowStatEx_t) (gmoHandle_t pgmo, int si, int *nz, int *lnz, int *qnz, int *nlnz);
/** Get Jacobian row NZ counts: total and by GMOORDER_XX
 *
 * @param pgmo gmo object handle
 * @param si Index of row in client space
 * @param nz Number of nonzeros
 * @param lnz 
 * @param qnz Number of quadratic nonzeros in Jacobian matrix
 * @param nlnz Number of nonlinear nonzeros
 */
GMO_FUNCPTR(gmoGetRowStatEx);

typedef int  (GMO_CALLCONV *gmoGetColStat_t) (gmoHandle_t pgmo, int sj, int *nz, int *qnz, int *nlnz, int *objnz);
/** Get column counts objnz = -1 if linear +1 if non-linear 0 otherwise
 *
 * @param pgmo gmo object handle
 * @param sj Index of column in client space
 * @param nz Number of nonzeros
 * @param qnz Number of quadratic nonzeros in Jacobian matrix
 * @param nlnz Number of nonlinear nonzeros
 * @param objnz Nonzeros in objective
 */
GMO_FUNCPTR(gmoGetColStat);

typedef int  (GMO_CALLCONV *gmoGetRowQNZOne_t) (gmoHandle_t pgmo, int si);
/** Number of NZ in Q matrix of row si (-1 if Q information not used or overflow)
 *
 * @param pgmo gmo object handle
 * @param si Index of row in client space
 */
GMO_FUNCPTR(gmoGetRowQNZOne);

typedef INT64  (GMO_CALLCONV *gmoGetRowQNZOne64_t) (gmoHandle_t pgmo, int si);
/** Number of NZ in Q matrix of row si (-1 if Q information not used)
 *
 * @param pgmo gmo object handle
 * @param si Index of row in client space
 */
GMO_FUNCPTR(gmoGetRowQNZOne64);

typedef int  (GMO_CALLCONV *gmoGetRowQDiagNZOne_t) (gmoHandle_t pgmo, int si);
/** Number of NZ on diagonal of Q matrix of row si (-1 if Q information not used)
 *
 * @param pgmo gmo object handle
 * @param si Index of row in client space
 */
GMO_FUNCPTR(gmoGetRowQDiagNZOne);

typedef int  (GMO_CALLCONV *gmoGetRowCVecNZOne_t) (gmoHandle_t pgmo, int si);
/** Number of NZ in c vector of row si (-1 if Q information not used)
 *
 * @param pgmo gmo object handle
 * @param si Index of row in client space
 */
GMO_FUNCPTR(gmoGetRowCVecNZOne);

typedef void  (GMO_CALLCONV *gmoGetSosCounts_t) (gmoHandle_t pgmo, int *numsos1, int *numsos2, int *nzsos);
/** Get SOS count information
 *
 * @param pgmo gmo object handle
 * @param numsos1 Number of SOS1 sets
 * @param numsos2 Number of SOS2 sets
 * @param nzsos Number of variables in SOS1/2 sets
 */
GMO_FUNCPTR(gmoGetSosCounts);

typedef void  (GMO_CALLCONV *gmoGetXLibCounts_t) (gmoHandle_t pgmo, int *rows, int *cols, int *nz, int orgcolind[]);
/** Get external function information
 *
 * @param pgmo gmo object handle
 * @param rows Number of rows
 * @param cols Number of columns
 * @param nz Number of nonzeros
 * @param orgcolind 
 */
GMO_FUNCPTR(gmoGetXLibCounts);

typedef int  (GMO_CALLCONV *gmoGetActiveModelType_t) (gmoHandle_t pgmo, int checkv[], int *actModelType);
/** Get model type in case of scenario solve generated models
 *
 * @param pgmo gmo object handle
 * @param checkv a vector with column indicators to be treated as constant
 * @param actModelType active model type in case of scenario dict type emp model
 */
GMO_FUNCPTR(gmoGetActiveModelType);

typedef int  (GMO_CALLCONV *gmoGetMatrixRow_t) (gmoHandle_t pgmo, int rowstart[], int colidx[], double jacval[], int nlflag[]);
/** Get constraint matrix in row order with row start only and NL indicator
 *
 * @param pgmo gmo object handle
 * @param rowstart Index of Jacobian row starts with
 * @param colidx Column index/indices of Jacobian(s)
 * @param jacval Value(s) of Jacobian(s)
 * @param nlflag NL flag(s) of Jacobian(s) (0 : linear,!=0 : nonlinear)
 */
GMO_FUNCPTR(gmoGetMatrixRow);

typedef int  (GMO_CALLCONV *gmoGetMatrixCol_t) (gmoHandle_t pgmo, int colstart[], int rowidx[], double jacval[], int nlflag[]);
/** Get constraint matrix in column order with columns start only and NL indicator
 *
 * @param pgmo gmo object handle
 * @param colstart Index of Jacobian column starts with
 * @param rowidx Row index/indices of Jacobian
 * @param jacval Value(s) of Jacobian(s)
 * @param nlflag NL flag(s) of Jacobian(s) (0 : linear,!=0 : nonlinear)
 */
GMO_FUNCPTR(gmoGetMatrixCol);

typedef int  (GMO_CALLCONV *gmoGetMatrixCplex_t) (gmoHandle_t pgmo, int colstart[], int collength[], int rowidx[], double jacval[]);
/** Get constraint matrix in column order with column start and end (colstart length is n+1)
 *
 * @param pgmo gmo object handle
 * @param colstart Index of Jacobian column starts with
 * @param collength Number of Jacobians in column
 * @param rowidx Row index/indices of Jacobian
 * @param jacval Value(s) of Jacobian(s)
 */
GMO_FUNCPTR(gmoGetMatrixCplex);

typedef char * (GMO_CALLCONV *gmoGetObjName_t) (gmoHandle_t pgmo, char *buf);
/** Get name of objective
 *
 * @param pgmo gmo object handle
 */
GMO_FUNCPTR(gmoGetObjName);

typedef char * (GMO_CALLCONV *gmoGetObjNameCustom_t) (gmoHandle_t pgmo, const char *suffix, char *buf);
/** Get name of objective with user specified suffix
 *
 * @param pgmo gmo object handle
 * @param suffix Suffix appended to name, could be .l, .m etc.
 */
GMO_FUNCPTR(gmoGetObjNameCustom);

typedef int  (GMO_CALLCONV *gmoGetObjVector_t) (gmoHandle_t pgmo, double jacval[], int nlflag[]);
/** Get objective function vector (dense)
 *
 * @param pgmo gmo object handle
 * @param jacval Value(s) of Jacobian(s)
 * @param nlflag NL flag(s) of Jacobian(s) (0 : linear,!=0 : nonlinear)
 */
GMO_FUNCPTR(gmoGetObjVector);

typedef int  (GMO_CALLCONV *gmoGetObjSparse_t) (gmoHandle_t pgmo, int colidx[], double jacval[], int nlflag[], int *nz, int *nlnz);
/** Get Jacobians information of objective function (sparse)
 *
 * @param pgmo gmo object handle
 * @param colidx Column index/indices of Jacobian(s)
 * @param jacval Value(s) of Jacobian(s)
 * @param nlflag NL flag(s) of Jacobian(s) (0 : linear,!=0 : nonlinear)
 * @param nz Number of nonzeros
 * @param nlnz Number of nonlinear nonzeros
 */
GMO_FUNCPTR(gmoGetObjSparse);

typedef int  (GMO_CALLCONV *gmoGetObjSparseEx_t) (gmoHandle_t pgmo, int colidx[], double gradval[], int nlflag[], int *nz, int *qnz, int *nlnz);
/** Get information for gradient of objective function (sparse)
 *
 * @param pgmo gmo object handle
 * @param colidx Column index/indices of Jacobian(s)
 * @param gradval 
 * @param nlflag NL flag(s) of Jacobian(s) (0 : linear,!=0 : nonlinear)
 * @param nz Number of nonzeros
 * @param qnz Number of quadratic nonzeros in Jacobian matrix
 * @param nlnz Number of nonlinear nonzeros
 */
GMO_FUNCPTR(gmoGetObjSparseEx);

typedef int  (GMO_CALLCONV *gmoGetObjQMat_t) (gmoHandle_t pgmo, int varidx1[], int varidx2[], double coefs[]);
/** Get lower triangle of Q matrix of objective
 *
 * @param pgmo gmo object handle
 * @param varidx1 First variable indices
 * @param varidx2 Second variable indices
 * @param coefs Coefficients
 */
GMO_FUNCPTR(gmoGetObjQMat);

typedef int  (GMO_CALLCONV *gmoGetObjQ_t) (gmoHandle_t pgmo, int varidx1[], int varidx2[], double coefs[]);
/** deprecated synonym for gmoGetObjQMat
 *
 * @param pgmo gmo object handle
 * @param varidx1 First variable indices
 * @param varidx2 Second variable indices
 * @param coefs Coefficients
 */
GMO_FUNCPTR(gmoGetObjQ);

typedef int  (GMO_CALLCONV *gmoGetObjCVec_t) (gmoHandle_t pgmo, int varidx[], double coefs[]);
/** Get c vector of quadratic objective
 *
 * @param pgmo gmo object handle
 * @param varidx 
 * @param coefs Coefficients
 */
GMO_FUNCPTR(gmoGetObjCVec);

typedef double  (GMO_CALLCONV *gmoGetObjL_t) (gmoHandle_t pgmo);
/** Get objective activity level
 *
 * @param pgmo gmo object handle
 */
GMO_FUNCPTR(gmoGetObjL);

typedef int  (GMO_CALLCONV *gmoGetEquL_t) (gmoHandle_t pgmo, double e[]);
/** Get equation activity levels
 *
 * @param pgmo gmo object handle
 * @param e Level values of equations
 */
GMO_FUNCPTR(gmoGetEquL);

typedef double  (GMO_CALLCONV *gmoGetEquLOne_t) (gmoHandle_t pgmo, int si);
/** Get individual equation activity levels
 *
 * @param pgmo gmo object handle
 * @param si Index of row in client space
 */
GMO_FUNCPTR(gmoGetEquLOne);

typedef int  (GMO_CALLCONV *gmoSetEquL_t) (gmoHandle_t pgmo, const double el[]);
/** Set equation activity levels
 *
 * @param pgmo gmo object handle
 * @param el Level of equation
 */
GMO_FUNCPTR(gmoSetEquL);

typedef void  (GMO_CALLCONV *gmoSetEquLOne_t) (gmoHandle_t pgmo, int si, double el);
/** Set individual equation activity levels
 *
 * @param pgmo gmo object handle
 * @param si Index of row in client space
 * @param el Level of equation
 */
GMO_FUNCPTR(gmoSetEquLOne);

typedef int  (GMO_CALLCONV *gmoGetEquM_t) (gmoHandle_t pgmo, double pi[]);
/** Get equation marginals
 *
 * @param pgmo gmo object handle
 * @param pi Marginal values of equations
 */
GMO_FUNCPTR(gmoGetEquM);

typedef double  (GMO_CALLCONV *gmoGetEquMOne_t) (gmoHandle_t pgmo, int si);
/** Get individual equation marginal
 *
 * @param pgmo gmo object handle
 * @param si Index of row in client space
 */
GMO_FUNCPTR(gmoGetEquMOne);

typedef int  (GMO_CALLCONV *gmoSetEquM_t) (gmoHandle_t pgmo, const double emarg[]);
/** Set equation marginals (pass NULL to set to NA)
 *
 * @param pgmo gmo object handle
 * @param emarg Marginal of equation
 */
GMO_FUNCPTR(gmoSetEquM);

typedef char * (GMO_CALLCONV *gmoGetEquNameOne_t) (gmoHandle_t pgmo, int si, char *buf);
/** Get individual equation name
 *
 * @param pgmo gmo object handle
 * @param si Index of row in client space
 */
GMO_FUNCPTR(gmoGetEquNameOne);

typedef char * (GMO_CALLCONV *gmoGetEquNameCustomOne_t) (gmoHandle_t pgmo, int si, const char *suffix, char *buf);
/** Get individual equation name with quotes and user specified suffix
 *
 * @param pgmo gmo object handle
 * @param si Index of row in client space
 * @param suffix Suffix appended to name, could be .l, .m etc.
 */
GMO_FUNCPTR(gmoGetEquNameCustomOne);

typedef int  (GMO_CALLCONV *gmoGetRhs_t) (gmoHandle_t pgmo, double mdblvec[]);
/** Get right hand sides
 *
 * @param pgmo gmo object handle
 * @param mdblvec Array of doubles, len=number of rows in user view
 */
GMO_FUNCPTR(gmoGetRhs);

typedef double  (GMO_CALLCONV *gmoGetRhsOne_t) (gmoHandle_t pgmo, int si);
/** Get individual equation right hand side
 *
 * @param pgmo gmo object handle
 * @param si Index of row in client space
 */
GMO_FUNCPTR(gmoGetRhsOne);

typedef double  (GMO_CALLCONV *gmoGetRhsOneEx_t) (gmoHandle_t pgmo, int si);
/** Get individual equation RHS - independent of useQ'
 *
 * @param pgmo gmo object handle
 * @param si Index of row in client space
 */
GMO_FUNCPTR(gmoGetRhsOneEx);

typedef int  (GMO_CALLCONV *gmoSetAltRHS_t) (gmoHandle_t pgmo, const double mdblvec[]);
/** Set alternative RHS
 *
 * @param pgmo gmo object handle
 * @param mdblvec Array of doubles, len=number of rows in user view
 */
GMO_FUNCPTR(gmoSetAltRHS);

typedef void  (GMO_CALLCONV *gmoSetAltRHSOne_t) (gmoHandle_t pgmo, int si, double erhs);
/** Set individual alternative RHS
 *
 * @param pgmo gmo object handle
 * @param si Index of row in client space
 * @param erhs RHS of equation
 */
GMO_FUNCPTR(gmoSetAltRHSOne);

typedef int  (GMO_CALLCONV *gmoGetEquSlack_t) (gmoHandle_t pgmo, double mdblvec[]);
/** Get equation slacks
 *
 * @param pgmo gmo object handle
 * @param mdblvec Array of doubles, len=number of rows in user view
 */
GMO_FUNCPTR(gmoGetEquSlack);

typedef double  (GMO_CALLCONV *gmoGetEquSlackOne_t) (gmoHandle_t pgmo, int si);
/** Get individual equation slack
 *
 * @param pgmo gmo object handle
 * @param si Index of row in client space
 */
GMO_FUNCPTR(gmoGetEquSlackOne);

typedef int  (GMO_CALLCONV *gmoSetEquSlack_t) (gmoHandle_t pgmo, const double mdblvec[]);
/** Set equation slacks
 *
 * @param pgmo gmo object handle
 * @param mdblvec Array of doubles, len=number of rows in user view
 */
GMO_FUNCPTR(gmoSetEquSlack);

typedef int  (GMO_CALLCONV *gmoGetEquType_t) (gmoHandle_t pgmo, int mintvec[]);
/** Get equation type
 *
 * @param pgmo gmo object handle
 * @param mintvec Array of integers, len=number of rows in user view
 */
GMO_FUNCPTR(gmoGetEquType);

typedef int  (GMO_CALLCONV *gmoGetEquTypeOne_t) (gmoHandle_t pgmo, int si);
/** Get individual equation type
 *
 * @param pgmo gmo object handle
 * @param si Index of row in client space
 */
GMO_FUNCPTR(gmoGetEquTypeOne);

typedef void  (GMO_CALLCONV *gmoGetEquStat_t) (gmoHandle_t pgmo, int mintvec[]);
/** Get equation basis status
 *
 * @param pgmo gmo object handle
 * @param mintvec Array of integers, len=number of rows in user view
 */
GMO_FUNCPTR(gmoGetEquStat);

typedef int  (GMO_CALLCONV *gmoGetEquStatOne_t) (gmoHandle_t pgmo, int si);
/** Get individual basis equation status
 *
 * @param pgmo gmo object handle
 * @param si Index of row in client space
 */
GMO_FUNCPTR(gmoGetEquStatOne);

typedef void  (GMO_CALLCONV *gmoSetEquStat_t) (gmoHandle_t pgmo, const int mintvec[]);
/** Set equation basis status
 *
 * @param pgmo gmo object handle
 * @param mintvec Array of integers, len=number of rows in user view
 */
GMO_FUNCPTR(gmoSetEquStat);

typedef void  (GMO_CALLCONV *gmoGetEquCStat_t) (gmoHandle_t pgmo, int mintvec[]);
/** Get equation status
 *
 * @param pgmo gmo object handle
 * @param mintvec Array of integers, len=number of rows in user view
 */
GMO_FUNCPTR(gmoGetEquCStat);

typedef int  (GMO_CALLCONV *gmoGetEquCStatOne_t) (gmoHandle_t pgmo, int si);
/** Get individual equation status
 *
 * @param pgmo gmo object handle
 * @param si Index of row in client space
 */
GMO_FUNCPTR(gmoGetEquCStatOne);

typedef void  (GMO_CALLCONV *gmoSetEquCStat_t) (gmoHandle_t pgmo, const int mintvec[]);
/** Set equation status
 *
 * @param pgmo gmo object handle
 * @param mintvec Array of integers, len=number of rows in user view
 */
GMO_FUNCPTR(gmoSetEquCStat);

typedef int  (GMO_CALLCONV *gmoGetEquMatch_t) (gmoHandle_t pgmo, int mintvec[]);
/** Get equation match
 *
 * @param pgmo gmo object handle
 * @param mintvec Array of integers, len=number of rows in user view
 */
GMO_FUNCPTR(gmoGetEquMatch);

typedef int  (GMO_CALLCONV *gmoGetEquMatchOne_t) (gmoHandle_t pgmo, int si);
/** Get individual equation match
 *
 * @param pgmo gmo object handle
 * @param si Index of row in client space
 */
GMO_FUNCPTR(gmoGetEquMatchOne);

typedef int  (GMO_CALLCONV *gmoGetEquScale_t) (gmoHandle_t pgmo, double mdblvec[]);
/** Get equation scale
 *
 * @param pgmo gmo object handle
 * @param mdblvec Array of doubles, len=number of rows in user view
 */
GMO_FUNCPTR(gmoGetEquScale);

typedef double  (GMO_CALLCONV *gmoGetEquScaleOne_t) (gmoHandle_t pgmo, int si);
/** Get individual equation scale
 *
 * @param pgmo gmo object handle
 * @param si Index of row in client space
 */
GMO_FUNCPTR(gmoGetEquScaleOne);

typedef int  (GMO_CALLCONV *gmoGetEquStage_t) (gmoHandle_t pgmo, double mdblvec[]);
/** Get equation stage
 *
 * @param pgmo gmo object handle
 * @param mdblvec Array of doubles, len=number of rows in user view
 */
GMO_FUNCPTR(gmoGetEquStage);

typedef double  (GMO_CALLCONV *gmoGetEquStageOne_t) (gmoHandle_t pgmo, int si);
/** Get individual equation stage
 *
 * @param pgmo gmo object handle
 * @param si Index of row in client space
 */
GMO_FUNCPTR(gmoGetEquStageOne);

typedef int  (GMO_CALLCONV *gmoGetEquOrderOne_t) (gmoHandle_t pgmo, int si);
/** Returns 0 on error, 1 linear, 2 quadratic, 3 nonlinear'
 *
 * @param pgmo gmo object handle
 * @param si Index of row in client space
 */
GMO_FUNCPTR(gmoGetEquOrderOne);

typedef int  (GMO_CALLCONV *gmoGetRowSparse_t) (gmoHandle_t pgmo, int si, int colidx[], double jacval[], int nlflag[], int *nz, int *nlnz);
/** Get Jacobians information of row (sparse)
 *
 * @param pgmo gmo object handle
 * @param si Index of row in client space
 * @param colidx Column index/indices of Jacobian(s)
 * @param jacval Value(s) of Jacobian(s)
 * @param nlflag NL flag(s) of Jacobian(s) (0 : linear,!=0 : nonlinear)
 * @param nz Number of nonzeros
 * @param nlnz Number of nonlinear nonzeros
 */
GMO_FUNCPTR(gmoGetRowSparse);

typedef int  (GMO_CALLCONV *gmoGetRowSparseEx_t) (gmoHandle_t pgmo, int si, int colidx[], double jacval[], int nlflag[], int *nz, int *qnz, int *nlnz);
/** Get info for one row of Jacobian (sparse)
 *
 * @param pgmo gmo object handle
 * @param si Index of row in client space
 * @param colidx Column index/indices of Jacobian(s)
 * @param jacval Value(s) of Jacobian(s)
 * @param nlflag NL flag(s) of Jacobian(s) (0 : linear,!=0 : nonlinear)
 * @param nz Number of nonzeros
 * @param qnz Number of quadratic nonzeros in Jacobian matrix
 * @param nlnz Number of nonlinear nonzeros
 */
GMO_FUNCPTR(gmoGetRowSparseEx);

typedef void  (GMO_CALLCONV *gmoGetRowJacInfoOne_t) (gmoHandle_t pgmo, int si, void **jacptr, double *jacval, int *colidx, int *nlflag);
/** Get Jacobian information of row one by one
 *
 * @param pgmo gmo object handle
 * @param si Index of row in client space
 * @param jacptr Pointer to next Jacobian
 * @param jacval Value(s) of Jacobian(s)
 * @param colidx Column index/indices of Jacobian(s)
 * @param nlflag NL flag(s) of Jacobian(s) (0 : linear,!=0 : nonlinear)
 */
GMO_FUNCPTR(gmoGetRowJacInfoOne);

typedef int  (GMO_CALLCONV *gmoGetRowQMat_t) (gmoHandle_t pgmo, int si, int varidx1[], int varidx2[], double coefs[]);
/** Get lower triangle of Q matrix of row si
 *
 * @param pgmo gmo object handle
 * @param si Index of row in client space
 * @param varidx1 First variable indices
 * @param varidx2 Second variable indices
 * @param coefs Coefficients
 */
GMO_FUNCPTR(gmoGetRowQMat);

typedef int  (GMO_CALLCONV *gmoGetRowQ_t) (gmoHandle_t pgmo, int si, int varidx1[], int varidx2[], double coefs[]);
/** deprecated synonym for gmoGetRowQMat
 *
 * @param pgmo gmo object handle
 * @param si Index of row in client space
 * @param varidx1 First variable indices
 * @param varidx2 Second variable indices
 * @param coefs Coefficients
 */
GMO_FUNCPTR(gmoGetRowQ);

typedef int  (GMO_CALLCONV *gmoGetRowCVec_t) (gmoHandle_t pgmo, int si, int varidx[], double coefs[]);
/** Get c vector of the quadratic form for row si
 *
 * @param pgmo gmo object handle
 * @param si Index of row in client space
 * @param varidx 
 * @param coefs Coefficients
 */
GMO_FUNCPTR(gmoGetRowCVec);

typedef double  (GMO_CALLCONV *gmoGetRowQConst_t) (gmoHandle_t pgmo, int si);
/** Get the constant of the quadratic form for row si
 *
 * @param pgmo gmo object handle
 * @param si Index of row in client space
 */
GMO_FUNCPTR(gmoGetRowQConst);

typedef int  (GMO_CALLCONV *gmoGetEquIntDotOpt_t) (gmoHandle_t pgmo, void *optptr, const char *dotopt, int optvals[]);
/** Get equation integer values for dot optio
 *
 * @param pgmo gmo object handle
 * @param optptr Option object pointer
 * @param dotopt Dot option name
 * @param optvals Option values
 */
GMO_FUNCPTR(gmoGetEquIntDotOpt);

typedef int  (GMO_CALLCONV *gmoGetEquDblDotOpt_t) (gmoHandle_t pgmo, void *optptr, const char *dotopt, double optvals[]);
/** Get equation double values for dot optio
 *
 * @param pgmo gmo object handle
 * @param optptr Option object pointer
 * @param dotopt Dot option name
 * @param optvals Option values
 */
GMO_FUNCPTR(gmoGetEquDblDotOpt);

typedef int  (GMO_CALLCONV *gmoGetVarL_t) (gmoHandle_t pgmo, double x[]);
/** Get variable level values
 *
 * @param pgmo gmo object handle
 * @param x Level values of variables
 */
GMO_FUNCPTR(gmoGetVarL);

typedef double  (GMO_CALLCONV *gmoGetVarLOne_t) (gmoHandle_t pgmo, int sj);
/** Get individual variable level
 *
 * @param pgmo gmo object handle
 * @param sj Index of column in client space
 */
GMO_FUNCPTR(gmoGetVarLOne);

typedef int  (GMO_CALLCONV *gmoSetVarL_t) (gmoHandle_t pgmo, const double x[]);
/** Set variable level values
 *
 * @param pgmo gmo object handle
 * @param x Level values of variables
 */
GMO_FUNCPTR(gmoSetVarL);

typedef void  (GMO_CALLCONV *gmoSetVarLOne_t) (gmoHandle_t pgmo, int sj, double vl);
/** Set individual variable level
 *
 * @param pgmo gmo object handle
 * @param sj Index of column in client space
 * @param vl Level of variable
 */
GMO_FUNCPTR(gmoSetVarLOne);

typedef int  (GMO_CALLCONV *gmoGetVarM_t) (gmoHandle_t pgmo, double dj[]);
/** Get variable marginals
 *
 * @param pgmo gmo object handle
 * @param dj Marginal values of variables
 */
GMO_FUNCPTR(gmoGetVarM);

typedef double  (GMO_CALLCONV *gmoGetVarMOne_t) (gmoHandle_t pgmo, int sj);
/** Get individual variable marginal
 *
 * @param pgmo gmo object handle
 * @param sj Index of column in client space
 */
GMO_FUNCPTR(gmoGetVarMOne);

typedef int  (GMO_CALLCONV *gmoSetVarM_t) (gmoHandle_t pgmo, const double dj[]);
/** Set variable marginals (pass null to set to NA)'
 *
 * @param pgmo gmo object handle
 * @param dj Marginal values of variables
 */
GMO_FUNCPTR(gmoSetVarM);

typedef void  (GMO_CALLCONV *gmoSetVarMOne_t) (gmoHandle_t pgmo, int sj, double vmarg);
/** Set individual variable marginal
 *
 * @param pgmo gmo object handle
 * @param sj Index of column in client space
 * @param vmarg Marginal of variable
 */
GMO_FUNCPTR(gmoSetVarMOne);

typedef char * (GMO_CALLCONV *gmoGetVarNameOne_t) (gmoHandle_t pgmo, int sj, char *buf);
/** Get individual column name
 *
 * @param pgmo gmo object handle
 * @param sj Index of column in client space
 */
GMO_FUNCPTR(gmoGetVarNameOne);

typedef char * (GMO_CALLCONV *gmoGetVarNameCustomOne_t) (gmoHandle_t pgmo, int sj, const char *suffix, char *buf);
/** Get individual column name with quotes and user specified suffix
 *
 * @param pgmo gmo object handle
 * @param sj Index of column in client space
 * @param suffix Suffix appended to name, could be .l, .m etc.
 */
GMO_FUNCPTR(gmoGetVarNameCustomOne);

typedef int  (GMO_CALLCONV *gmoGetVarLower_t) (gmoHandle_t pgmo, double lovec[]);
/** Get variable lower bounds
 *
 * @param pgmo gmo object handle
 * @param lovec Lower bound values of variables
 */
GMO_FUNCPTR(gmoGetVarLower);

typedef double  (GMO_CALLCONV *gmoGetVarLowerOne_t) (gmoHandle_t pgmo, int sj);
/** Get individual variable lower bound
 *
 * @param pgmo gmo object handle
 * @param sj Index of column in client space
 */
GMO_FUNCPTR(gmoGetVarLowerOne);

typedef int  (GMO_CALLCONV *gmoGetVarUpper_t) (gmoHandle_t pgmo, double upvec[]);
/** Get variable upper bounds
 *
 * @param pgmo gmo object handle
 * @param upvec Upper bound values of variables
 */
GMO_FUNCPTR(gmoGetVarUpper);

typedef double  (GMO_CALLCONV *gmoGetVarUpperOne_t) (gmoHandle_t pgmo, int sj);
/** Get individual variable upper bound
 *
 * @param pgmo gmo object handle
 * @param sj Index of column in client space
 */
GMO_FUNCPTR(gmoGetVarUpperOne);

typedef int  (GMO_CALLCONV *gmoSetAltVarBounds_t) (gmoHandle_t pgmo, const double lovec[], const double upvec[]);
/** Set alternative variable lower and upper bounds
 *
 * @param pgmo gmo object handle
 * @param lovec Lower bound values of variables
 * @param upvec Upper bound values of variables
 */
GMO_FUNCPTR(gmoSetAltVarBounds);

typedef void  (GMO_CALLCONV *gmoSetAltVarLowerOne_t) (gmoHandle_t pgmo, int sj, double vlo);
/** Set individual alternative variable lower bound
 *
 * @param pgmo gmo object handle
 * @param sj Index of column in client space
 * @param vlo Lower bound of variable
 */
GMO_FUNCPTR(gmoSetAltVarLowerOne);

typedef void  (GMO_CALLCONV *gmoSetAltVarUpperOne_t) (gmoHandle_t pgmo, int sj, double vup);
/** Set individual alternative variable upper bound
 *
 * @param pgmo gmo object handle
 * @param sj Index of column in client space
 * @param vup Upper bound of variable
 */
GMO_FUNCPTR(gmoSetAltVarUpperOne);

typedef int  (GMO_CALLCONV *gmoGetVarType_t) (gmoHandle_t pgmo, int nintvec[]);
/** Get variable type
 *
 * @param pgmo gmo object handle
 * @param nintvec Array of integers, len=number of columns in user view
 */
GMO_FUNCPTR(gmoGetVarType);

typedef int  (GMO_CALLCONV *gmoGetVarTypeOne_t) (gmoHandle_t pgmo, int sj);
/** Get individual variable type
 *
 * @param pgmo gmo object handle
 * @param sj Index of column in client space
 */
GMO_FUNCPTR(gmoGetVarTypeOne);

typedef int  (GMO_CALLCONV *gmoSetAltVarType_t) (gmoHandle_t pgmo, const int nintvec[]);
/** Set alternative variable type
 *
 * @param pgmo gmo object handle
 * @param nintvec Array of integers, len=number of columns in user view
 */
GMO_FUNCPTR(gmoSetAltVarType);

typedef void  (GMO_CALLCONV *gmoSetAltVarTypeOne_t) (gmoHandle_t pgmo, int sj, int vtyp);
/** Set individual alternative variable type
 *
 * @param pgmo gmo object handle
 * @param sj Index of column in client space
 * @param vtyp Type of variable (see enumerated constants)
 */
GMO_FUNCPTR(gmoSetAltVarTypeOne);

typedef void  (GMO_CALLCONV *gmoGetVarStat_t) (gmoHandle_t pgmo, int nintvec[]);
/** Get variable basis status
 *
 * @param pgmo gmo object handle
 * @param nintvec Array of integers, len=number of columns in user view
 */
GMO_FUNCPTR(gmoGetVarStat);

typedef int  (GMO_CALLCONV *gmoGetVarStatOne_t) (gmoHandle_t pgmo, int sj);
/** Get individual variable basis status
 *
 * @param pgmo gmo object handle
 * @param sj Index of column in client space
 */
GMO_FUNCPTR(gmoGetVarStatOne);

typedef void  (GMO_CALLCONV *gmoSetVarStat_t) (gmoHandle_t pgmo, const int nintvec[]);
/** Set variable basis status
 *
 * @param pgmo gmo object handle
 * @param nintvec Array of integers, len=number of columns in user view
 */
GMO_FUNCPTR(gmoSetVarStat);

typedef void  (GMO_CALLCONV *gmoSetVarStatOne_t) (gmoHandle_t pgmo, int sj, int vstat);
/** Set individual variable basis status
 *
 * @param pgmo gmo object handle
 * @param sj Index of column in client space
 * @param vstat Basis status of variable (see enumerated constants)
 */
GMO_FUNCPTR(gmoSetVarStatOne);

typedef void  (GMO_CALLCONV *gmoGetVarCStat_t) (gmoHandle_t pgmo, int nintvec[]);
/** Get variable status
 *
 * @param pgmo gmo object handle
 * @param nintvec Array of integers, len=number of columns in user view
 */
GMO_FUNCPTR(gmoGetVarCStat);

typedef int  (GMO_CALLCONV *gmoGetVarCStatOne_t) (gmoHandle_t pgmo, int sj);
/** Get individual variable status
 *
 * @param pgmo gmo object handle
 * @param sj Index of column in client space
 */
GMO_FUNCPTR(gmoGetVarCStatOne);

typedef void  (GMO_CALLCONV *gmoSetVarCStat_t) (gmoHandle_t pgmo, const int nintvec[]);
/** Set variable status
 *
 * @param pgmo gmo object handle
 * @param nintvec Array of integers, len=number of columns in user view
 */
GMO_FUNCPTR(gmoSetVarCStat);

typedef int  (GMO_CALLCONV *gmoGetVarMatch_t) (gmoHandle_t pgmo, int nintvec[]);
/** Get variable match
 *
 * @param pgmo gmo object handle
 * @param nintvec Array of integers, len=number of columns in user view
 */
GMO_FUNCPTR(gmoGetVarMatch);

typedef int  (GMO_CALLCONV *gmoGetVarMatchOne_t) (gmoHandle_t pgmo, int sj);
/** Get individual variable match
 *
 * @param pgmo gmo object handle
 * @param sj Index of column in client space
 */
GMO_FUNCPTR(gmoGetVarMatchOne);

typedef int  (GMO_CALLCONV *gmoGetVarPrior_t) (gmoHandle_t pgmo, double ndblvec[]);
/** Get variable branching priority
 *
 * @param pgmo gmo object handle
 * @param ndblvec Array of doubles, len=number of columns in user view
 */
GMO_FUNCPTR(gmoGetVarPrior);

typedef double  (GMO_CALLCONV *gmoGetVarPriorOne_t) (gmoHandle_t pgmo, int sj);
/** Get individual variable branching priority
 *
 * @param pgmo gmo object handle
 * @param sj Index of column in client space
 */
GMO_FUNCPTR(gmoGetVarPriorOne);

typedef int  (GMO_CALLCONV *gmoGetVarScale_t) (gmoHandle_t pgmo, double ndblvec[]);
/** Get variable scale
 *
 * @param pgmo gmo object handle
 * @param ndblvec Array of doubles, len=number of columns in user view
 */
GMO_FUNCPTR(gmoGetVarScale);

typedef double  (GMO_CALLCONV *gmoGetVarScaleOne_t) (gmoHandle_t pgmo, int sj);
/** Get individual variable scale
 *
 * @param pgmo gmo object handle
 * @param sj Index of column in client space
 */
GMO_FUNCPTR(gmoGetVarScaleOne);

typedef int  (GMO_CALLCONV *gmoGetVarStage_t) (gmoHandle_t pgmo, double ndblvec[]);
/** Get variable stage
 *
 * @param pgmo gmo object handle
 * @param ndblvec Array of doubles, len=number of columns in user view
 */
GMO_FUNCPTR(gmoGetVarStage);

typedef double  (GMO_CALLCONV *gmoGetVarStageOne_t) (gmoHandle_t pgmo, int sj);
/** Get individual variable stage
 *
 * @param pgmo gmo object handle
 * @param sj Index of column in client space
 */
GMO_FUNCPTR(gmoGetVarStageOne);

typedef int  (GMO_CALLCONV *gmoGetSosConstraints_t) (gmoHandle_t pgmo, int sostype[], int sosbeg[], int sosind[], double soswt[]);
/** Get SOS constraints
 *
 * @param pgmo gmo object handle
 * @param sostype SOS type 1 or 2
 * @param sosbeg Variable index start of SOS set
 * @param sosind Variable indices
 * @param soswt Weight in SOS set
 */
GMO_FUNCPTR(gmoGetSosConstraints);

typedef int  (GMO_CALLCONV *gmoGetVarSosSetOne_t) (gmoHandle_t pgmo, int sj);
/** Get SOS set for individual variable
 *
 * @param pgmo gmo object handle
 * @param sj Index of column in client space
 */
GMO_FUNCPTR(gmoGetVarSosSetOne);

typedef int  (GMO_CALLCONV *gmoGetColSparse_t) (gmoHandle_t pgmo, int sj, int rowidx[], double jacval[], int nlflag[], int *nz, int *nlnz);
/** Get Jacobians information of column (sparse)
 *
 * @param pgmo gmo object handle
 * @param sj Index of column in client space
 * @param rowidx Row index/indices of Jacobian
 * @param jacval Value(s) of Jacobian(s)
 * @param nlflag NL flag(s) of Jacobian(s) (0 : linear,!=0 : nonlinear)
 * @param nz Number of nonzeros
 * @param nlnz Number of nonlinear nonzeros
 */
GMO_FUNCPTR(gmoGetColSparse);

typedef void  (GMO_CALLCONV *gmoGetColJacInfoOne_t) (gmoHandle_t pgmo, int sj, void **jacptr, double *jacval, int *rowidx, int *nlflag);
/** Get Jacobian information of column one by one
 *
 * @param pgmo gmo object handle
 * @param sj Index of column in client space
 * @param jacptr Pointer to next Jacobian
 * @param jacval Value(s) of Jacobian(s)
 * @param rowidx Row index/indices of Jacobian
 * @param nlflag NL flag(s) of Jacobian(s) (0 : linear,!=0 : nonlinear)
 */
GMO_FUNCPTR(gmoGetColJacInfoOne);

typedef int  (GMO_CALLCONV *gmoGetVarIntDotOpt_t) (gmoHandle_t pgmo, void *optptr, const char *dotopt, int optvals[]);
/** Get variable integer values for dot option
 *
 * @param pgmo gmo object handle
 * @param optptr Option object pointer
 * @param dotopt Dot option name
 * @param optvals Option values
 */
GMO_FUNCPTR(gmoGetVarIntDotOpt);

typedef int  (GMO_CALLCONV *gmoGetVarDblDotOpt_t) (gmoHandle_t pgmo, void *optptr, const char *dotopt, double optvals[]);
/** Get variable double values for dot option
 *
 * @param pgmo gmo object handle
 * @param optptr Option object pointer
 * @param dotopt Dot option name
 * @param optvals Option values
 */
GMO_FUNCPTR(gmoGetVarDblDotOpt);

typedef void  (GMO_CALLCONV *gmoEvalErrorMsg_t) (gmoHandle_t pgmo, int domsg);
/** Control writing messages for evaluation errors, default=true
 *
 * @param pgmo gmo object handle
 * @param domsg Flag whether to write messages
 */
GMO_FUNCPTR(gmoEvalErrorMsg);

typedef void  (GMO_CALLCONV *gmoEvalErrorMsg_MT_t) (gmoHandle_t pgmo, int domsg, int tidx);
/** Control writing messages for evaluation errors, default=true
 *
 * @param pgmo gmo object handle
 * @param domsg Flag whether to write messages
 * @param tidx Index of thread
 */
GMO_FUNCPTR(gmoEvalErrorMsg_MT);

typedef void  (GMO_CALLCONV *gmoEvalErrorMaskLevel_t) (gmoHandle_t pgmo, int MaskLevel);
/** Set mask to ignore errors >= evalErrorMaskLevel when incrementing numerr
 *
 * @param pgmo gmo object handle
 * @param MaskLevel Ignore evaluation errors less that this value
 */
GMO_FUNCPTR(gmoEvalErrorMaskLevel);

typedef void  (GMO_CALLCONV *gmoEvalErrorMaskLevel_MT_t) (gmoHandle_t pgmo, int MaskLevel, int tidx);
/** Set mask to ignore errors >= evalErrorMaskLevel when incrementing numerr
 *
 * @param pgmo gmo object handle
 * @param MaskLevel Ignore evaluation errors less that this value
 * @param tidx Index of thread
 */
GMO_FUNCPTR(gmoEvalErrorMaskLevel_MT);

typedef int  (GMO_CALLCONV *gmoEvalNewPoint_t) (gmoHandle_t pgmo, const double x[]);
/** New point for the next evaluation call
 *
 * @param pgmo gmo object handle
 * @param x Level values of variables
 */
GMO_FUNCPTR(gmoEvalNewPoint);

typedef void  (GMO_CALLCONV *gmoSetExtFuncs_t) (gmoHandle_t pgmo, void *extfunmgr);
/** Set external function manager object
 *
 * @param pgmo gmo object handle
 * @param extfunmgr 
 */
GMO_FUNCPTR(gmoSetExtFuncs);

typedef int  (GMO_CALLCONV *gmoGetQMakerStats_t) (gmoHandle_t pgmo, char *algName, double *algTime, INT64 *winnerCount3Pass, INT64 *winnerCountDblFwd);
/** Get QMaker stats
 *
 * @param pgmo gmo object handle
 * @param algName the name of the QMaker algorithm used
 * @param algTime the wall-clock time in seconds used by QMaker
 * @param winnerCount3Pass count of rows where new 3-pass alg was the winner
 * @param winnerCountDblFwd count of rows where old double-forward alg was the winner
 */
GMO_FUNCPTR(gmoGetQMakerStats);

typedef int  (GMO_CALLCONV *gmoEvalFunc_t) (gmoHandle_t pgmo, int si, const double x[], double *f, int *numerr);
/** Evaluate the constraint si (excluding RHS)
 *
 * @param pgmo gmo object handle
 * @param si Index of row in client space
 * @param x Level values of variables
 * @param f Function value
 * @param numerr Number of errors evaluating the nonlinear function
 */
GMO_FUNCPTR(gmoEvalFunc);

typedef int  (GMO_CALLCONV *gmoEvalFunc_MT_t) (gmoHandle_t pgmo, int si, const double x[], double *f, int *numerr, int tidx);
/** Evaluate the constraint si (excluding RHS)
 *
 * @param pgmo gmo object handle
 * @param si Index of row in client space
 * @param x Level values of variables
 * @param f Function value
 * @param numerr Number of errors evaluating the nonlinear function
 * @param tidx Index of thread
 */
GMO_FUNCPTR(gmoEvalFunc_MT);

typedef int  (GMO_CALLCONV *gmoEvalFuncInt_t) (gmoHandle_t pgmo, int si, double *f, int *numerr);
/** Evaluate the constraint si using the GMO internal variable levels (excluding RHS)
 *
 * @param pgmo gmo object handle
 * @param si Index of row in client space
 * @param f Function value
 * @param numerr Number of errors evaluating the nonlinear function
 */
GMO_FUNCPTR(gmoEvalFuncInt);

typedef int  (GMO_CALLCONV *gmoEvalFuncInt_MT_t) (gmoHandle_t pgmo, int si, double *f, int *numerr, int tidx);
/** Evaluate the constraint si using the GMO internal variable levels (excluding RHS)
 *
 * @param pgmo gmo object handle
 * @param si Index of row in client space
 * @param f Function value
 * @param numerr Number of errors evaluating the nonlinear function
 * @param tidx Index of thread
 */
GMO_FUNCPTR(gmoEvalFuncInt_MT);

typedef int  (GMO_CALLCONV *gmoEvalFuncNL_t) (gmoHandle_t pgmo, int si, const double x[], double *fnl, int *numerr);
/** Evaluate the nonlinear function component of constraint si
 *
 * @param pgmo gmo object handle
 * @param si Index of row in client space
 * @param x Level values of variables
 * @param fnl Part of the function value depending on the nonlinear variables
 * @param numerr Number of errors evaluating the nonlinear function
 */
GMO_FUNCPTR(gmoEvalFuncNL);

typedef int  (GMO_CALLCONV *gmoEvalFuncNL_MT_t) (gmoHandle_t pgmo, int si, const double x[], double *fnl, int *numerr, int tidx);
/** Evaluate the nonlinear function component of constraint si
 *
 * @param pgmo gmo object handle
 * @param si Index of row in client space
 * @param x Level values of variables
 * @param fnl Part of the function value depending on the nonlinear variables
 * @param numerr Number of errors evaluating the nonlinear function
 * @param tidx Index of thread
 */
GMO_FUNCPTR(gmoEvalFuncNL_MT);

typedef int  (GMO_CALLCONV *gmoEvalFuncObj_t) (gmoHandle_t pgmo, const double x[], double *f, int *numerr);
/** Evaluate objective function component
 *
 * @param pgmo gmo object handle
 * @param x Level values of variables
 * @param f Function value
 * @param numerr Number of errors evaluating the nonlinear function
 */
GMO_FUNCPTR(gmoEvalFuncObj);

typedef int  (GMO_CALLCONV *gmoEvalFuncNLObj_t) (gmoHandle_t pgmo, const double x[], double *fnl, int *numerr);
/** Evaluate nonlinear objective function component
 *
 * @param pgmo gmo object handle
 * @param x Level values of variables
 * @param fnl Part of the function value depending on the nonlinear variables
 * @param numerr Number of errors evaluating the nonlinear function
 */
GMO_FUNCPTR(gmoEvalFuncNLObj);

typedef int  (GMO_CALLCONV *gmoEvalFuncInterval_t) (gmoHandle_t pgmo, int si, const double xmin[], const double xmax[], double *fmin, double *fmax, int *numerr);
/** Evaluate the function value of constraint si on the giving interval
 *
 * @param pgmo gmo object handle
 * @param si Index of row in client space
 * @param xmin Minimum input level values of variables
 * @param xmax Maximum input level values of variables
 * @param fmin Minimum function value
 * @param fmax Maximum function value
 * @param numerr Number of errors evaluating the nonlinear function
 */
GMO_FUNCPTR(gmoEvalFuncInterval);

typedef int  (GMO_CALLCONV *gmoEvalFuncInterval_MT_t) (gmoHandle_t pgmo, int si, const double xmin[], const double xmax[], double *fmin, double *fmax, int *numerr, int tidx);
/** Evaluate the function value of constraint si on the giving interval
 *
 * @param pgmo gmo object handle
 * @param si Index of row in client space
 * @param xmin Minimum input level values of variables
 * @param xmax Maximum input level values of variables
 * @param fmin Minimum function value
 * @param fmax Maximum function value
 * @param numerr Number of errors evaluating the nonlinear function
 * @param tidx Index of thread
 */
GMO_FUNCPTR(gmoEvalFuncInterval_MT);

typedef int  (GMO_CALLCONV *gmoEvalGrad_t) (gmoHandle_t pgmo, int si, const double x[], double *f, double g[], double *gx, int *numerr);
/** Update the nonlinear gradients of constraint si and evaluate function value
 *
 * @param pgmo gmo object handle
 * @param si Index of row in client space
 * @param x Level values of variables
 * @param f Function value
 * @param g Gradient values
 * @param gx Inner product of the gradient with the input variables
 * @param numerr Number of errors evaluating the nonlinear function
 */
GMO_FUNCPTR(gmoEvalGrad);

typedef int  (GMO_CALLCONV *gmoEvalGrad_MT_t) (gmoHandle_t pgmo, int si, const double x[], double *f, double g[], double *gx, int *numerr, int tidx);
/** Update the nonlinear gradients of constraint si and evaluate function value
 *
 * @param pgmo gmo object handle
 * @param si Index of row in client space
 * @param x Level values of variables
 * @param f Function value
 * @param g Gradient values
 * @param gx Inner product of the gradient with the input variables
 * @param numerr Number of errors evaluating the nonlinear function
 * @param tidx Index of thread
 */
GMO_FUNCPTR(gmoEvalGrad_MT);

typedef int  (GMO_CALLCONV *gmoEvalGradNL_t) (gmoHandle_t pgmo, int si, const double x[], double *fnl, double g[], double *gxnl, int *numerr);
/** Update the nonlinear gradients of constraint si and evaluate nonlinear function and gradient value
 *
 * @param pgmo gmo object handle
 * @param si Index of row in client space
 * @param x Level values of variables
 * @param fnl Part of the function value depending on the nonlinear variables
 * @param g Gradient values
 * @param gxnl Inner product of the gradient with the input variables, nonlinear variables only
 * @param numerr Number of errors evaluating the nonlinear function
 */
GMO_FUNCPTR(gmoEvalGradNL);

typedef int  (GMO_CALLCONV *gmoEvalGradNL_MT_t) (gmoHandle_t pgmo, int si, const double x[], double *fnl, double g[], double *gxnl, int *numerr, int tidx);
/** Update the nonlinear gradients of constraint si and evaluate nonlinear function and gradient value
 *
 * @param pgmo gmo object handle
 * @param si Index of row in client space
 * @param x Level values of variables
 * @param fnl Part of the function value depending on the nonlinear variables
 * @param g Gradient values
 * @param gxnl Inner product of the gradient with the input variables, nonlinear variables only
 * @param numerr Number of errors evaluating the nonlinear function
 * @param tidx Index of thread
 */
GMO_FUNCPTR(gmoEvalGradNL_MT);

typedef int  (GMO_CALLCONV *gmoEvalGradObj_t) (gmoHandle_t pgmo, const double x[], double *f, double g[], double *gx, int *numerr);
/** Update the gradients of the objective function and evaluate function and gradient value
 *
 * @param pgmo gmo object handle
 * @param x Level values of variables
 * @param f Function value
 * @param g Gradient values
 * @param gx Inner product of the gradient with the input variables
 * @param numerr Number of errors evaluating the nonlinear function
 */
GMO_FUNCPTR(gmoEvalGradObj);

typedef int  (GMO_CALLCONV *gmoEvalGradNLObj_t) (gmoHandle_t pgmo, const double x[], double *fnl, double g[], double *gxnl, int *numerr);
/** Update the nonlinear gradients of the objective function and evaluate function and gradient value
 *
 * @param pgmo gmo object handle
 * @param x Level values of variables
 * @param fnl Part of the function value depending on the nonlinear variables
 * @param g Gradient values
 * @param gxnl Inner product of the gradient with the input variables, nonlinear variables only
 * @param numerr Number of errors evaluating the nonlinear function
 */
GMO_FUNCPTR(gmoEvalGradNLObj);

typedef int  (GMO_CALLCONV *gmoEvalGradInterval_t) (gmoHandle_t pgmo, int si, const double xmin[], const double xmax[], double *fmin, double *fmax, double gmin[], double gmax[], int *numerr);
/** Evaluate the function and gradient value of constraint si on the giving interval
 *
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
GMO_FUNCPTR(gmoEvalGradInterval);

typedef int  (GMO_CALLCONV *gmoEvalGradInterval_MT_t) (gmoHandle_t pgmo, int si, const double xmin[], const double xmax[], double *fmin, double *fmax, double gmin[], double gmax[], int *numerr, int tidx);
/** Evaluate the function and gradient value of constraint si on the giving interval
 *
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
GMO_FUNCPTR(gmoEvalGradInterval_MT);

typedef int  (GMO_CALLCONV *gmoEvalGradNLUpdate_t) (gmoHandle_t pgmo, double rhsdelta[], int dojacupd, int *numerr);
/** Evaluate all nonlinear gradients and return change vector plus optional update of Jacobians
 *
 * @param pgmo gmo object handle
 * @param rhsdelta Taylor expansion constants
 * @param dojacupd Flag whether to update Jacobians
 * @param numerr Number of errors evaluating the nonlinear function
 */
GMO_FUNCPTR(gmoEvalGradNLUpdate);

typedef int  (GMO_CALLCONV *gmoGetJacUpdate_t) (gmoHandle_t pgmo, int rowidx[], int colidx[], double jacval[], int *len);
/** Retrieve the updated Jacobian elements
 *
 * @param pgmo gmo object handle
 * @param rowidx Row index/indices of Jacobian
 * @param colidx Column index/indices of Jacobian(s)
 * @param jacval Value(s) of Jacobian(s)
 * @param len Length of array
 */
GMO_FUNCPTR(gmoGetJacUpdate);

typedef int  (GMO_CALLCONV *gmoHessLoad_t) (gmoHandle_t pgmo, double maxJacMult, int *do2dir, int *doHess);
/** Initialize Hessians
 *
 * @param pgmo gmo object handle
 * @param maxJacMult Multiplier to define memory limit for Hessian (0=no limit)
 * @param do2dir Flag whether 2nd derivatives are wanted/available
 * @param doHess Flag whether Hessians are wanted/available
 */
GMO_FUNCPTR(gmoHessLoad);

typedef int  (GMO_CALLCONV *gmoHessUnload_t) (gmoHandle_t pgmo);
/** Unload Hessians
 *
 * @param pgmo gmo object handle
 */
GMO_FUNCPTR(gmoHessUnload);

typedef int  (GMO_CALLCONV *gmoHessDim_t) (gmoHandle_t pgmo, int si);
/** Hessian dimension of row
 *
 * @param pgmo gmo object handle
 * @param si Index of row in client space
 */
GMO_FUNCPTR(gmoHessDim);

typedef int  (GMO_CALLCONV *gmoHessNz_t) (gmoHandle_t pgmo, int si);
/** Hessian nonzeros of row
 *
 * @param pgmo gmo object handle
 * @param si Index of row in client space
 */
GMO_FUNCPTR(gmoHessNz);

typedef INT64  (GMO_CALLCONV *gmoHessNz64_t) (gmoHandle_t pgmo, int si);
/** Hessian nonzeros of row
 *
 * @param pgmo gmo object handle
 * @param si Index of row in client space
 */
GMO_FUNCPTR(gmoHessNz64);

typedef int  (GMO_CALLCONV *gmoHessStruct_t) (gmoHandle_t pgmo, int si, int hridx[], int hcidx[], int *hessdim, int *hessnz);
/** Get Hessian Structure
 *
 * @param pgmo gmo object handle
 * @param si Index of row in client space
 * @param hridx Hessian row indices
 * @param hcidx Hessian column indices
 * @param hessdim Dimension of Hessian
 * @param hessnz Number of nonzeros in Hessian
 */
GMO_FUNCPTR(gmoHessStruct);

typedef int  (GMO_CALLCONV *gmoHessStruct64_t) (gmoHandle_t pgmo, int si, int hridx[], int hcidx[], int *hessdim, INT64 *hessnz);
/** Get Hessian Structure
 *
 * @param pgmo gmo object handle
 * @param si Index of row in client space
 * @param hridx Hessian row indices
 * @param hcidx Hessian column indices
 * @param hessdim Dimension of Hessian
 * @param hessnz Number of nonzeros in Hessian
 */
GMO_FUNCPTR(gmoHessStruct64);

typedef int  (GMO_CALLCONV *gmoHessValue_t) (gmoHandle_t pgmo, int si, int hridx[], int hcidx[], int *hessdim, int *hessnz, const double x[], double hessval[], int *numerr);
/** Get Hessian Value
 *
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
GMO_FUNCPTR(gmoHessValue);

typedef int  (GMO_CALLCONV *gmoHessValue64_t) (gmoHandle_t pgmo, int si, int hridx[], int hcidx[], int *hessdim, INT64 *hessnz, const double x[], double hessval[], int *numerr);
/** Get Hessian Value
 *
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
GMO_FUNCPTR(gmoHessValue64);

typedef int  (GMO_CALLCONV *gmoHessVec_t) (gmoHandle_t pgmo, int si, const double x[], const double dx[], double Wdx[], int *numerr);
/** Get Hessian-vector product
 *
 * @param pgmo gmo object handle
 * @param si Index of row in client space
 * @param x Level values of variables
 * @param dx Direction in x-space for directional derivative of Lagrangian (W*dx)
 * @param Wdx Directional derivative of the Lagrangian in direction dx (W*dx)
 * @param numerr Number of errors evaluating the nonlinear function
 */
GMO_FUNCPTR(gmoHessVec);

typedef int  (GMO_CALLCONV *gmoHessLagStruct_t) (gmoHandle_t pgmo, int WRindex[], int WCindex[]);
/** Get Hessian of the Lagrangian Value structure
 *
 * @param pgmo gmo object handle
 * @param WRindex Row indices for the upper triangle of the symmetric matrix W (the Hessian of the Lagrangian), ordered by rows and within rows by columns
 * @param WCindex Col indices for the upper triangle of the symmetric matrix W (the Hessian of the Lagrangian), ordered by rows and within rows by columns
 */
GMO_FUNCPTR(gmoHessLagStruct);

typedef int  (GMO_CALLCONV *gmoHessLagValue_t) (gmoHandle_t pgmo, const double x[], const double pi[], double w[], double objweight, double conweight, int *numerr);
/** Get Hessian of the Lagrangian Value
 *
 * @param pgmo gmo object handle
 * @param x Level values of variables
 * @param pi Marginal values of equations
 * @param w Values for the structural nonzeros of the upper triangle of the symmetric matrix W (the Hessian of the Lagrangian), ordered by rows and within rows by columns
 * @param objweight Weight for objective in Hessian of the Lagrangian (=1 for GAMS convention)
 * @param conweight Weight for constraints in Hessian of the Lagrangian (=1 for GAMS convention)
 * @param numerr Number of errors evaluating the nonlinear function
 */
GMO_FUNCPTR(gmoHessLagValue);

typedef int  (GMO_CALLCONV *gmoHessLagVec_t) (gmoHandle_t pgmo, const double x[], const double pi[], const double dx[], double Wdx[], double objweight, double conweight, int *numerr);
/** Get Hessian of the Lagrangian-vector product
 *
 * @param pgmo gmo object handle
 * @param x Level values of variables
 * @param pi Marginal values of equations
 * @param dx Direction in x-space for directional derivative of Lagrangian (W*dx)
 * @param Wdx Directional derivative of the Lagrangian in direction dx (W*dx)
 * @param objweight Weight for objective in Hessian of the Lagrangian (=1 for GAMS convention)
 * @param conweight Weight for constraints in Hessian of the Lagrangian (=1 for GAMS convention)
 * @param numerr Number of errors evaluating the nonlinear function
 */
GMO_FUNCPTR(gmoHessLagVec);

typedef int  (GMO_CALLCONV *gmoLoadEMPInfo_t) (gmoHandle_t pgmo, const char *empinfofname);
/** Load EMP information
 *
 * @param pgmo gmo object handle
 * @param empinfofname Name of EMP information file, if empty assume the default name and location
 */
GMO_FUNCPTR(gmoLoadEMPInfo);

typedef int  (GMO_CALLCONV *gmoGetEquVI_t) (gmoHandle_t pgmo, int mintvec[]);
/** Get VI mapping for all rows (-1 if not a VI function)
 *
 * @param pgmo gmo object handle
 * @param mintvec Array of integers, len=number of rows in user view
 */
GMO_FUNCPTR(gmoGetEquVI);

typedef int  (GMO_CALLCONV *gmoGetEquVIOne_t) (gmoHandle_t pgmo, int si);
/** Get VI mapping for individual row (-1 if not a VI function)
 *
 * @param pgmo gmo object handle
 * @param si Index of row in client space
 */
GMO_FUNCPTR(gmoGetEquVIOne);

typedef int  (GMO_CALLCONV *gmoGetVarVI_t) (gmoHandle_t pgmo, int nintvec[]);
/** Get VI mapping for all cols (-1 if not a VI variable)
 *
 * @param pgmo gmo object handle
 * @param nintvec Array of integers, len=number of columns in user view
 */
GMO_FUNCPTR(gmoGetVarVI);

typedef int  (GMO_CALLCONV *gmoGetVarVIOne_t) (gmoHandle_t pgmo, int sj);
/** Get VI mapping for individual cols (-1 if not a VI variable)
 *
 * @param pgmo gmo object handle
 * @param sj Index of column in client space
 */
GMO_FUNCPTR(gmoGetVarVIOne);

typedef int  (GMO_CALLCONV *gmoGetAgentType_t) (gmoHandle_t pgmo, int agentvec[]);
/** Get Agent Type of all agent (see gmoNumAgents)
 *
 * @param pgmo gmo object handle
 * @param agentvec Array of agent types of length gmoNumAgents
 */
GMO_FUNCPTR(gmoGetAgentType);

typedef int  (GMO_CALLCONV *gmoGetAgentTypeOne_t) (gmoHandle_t pgmo, int aidx);
/** Get Agent Type of agent
 *
 * @param pgmo gmo object handle
 * @param aidx Index of agent
 */
GMO_FUNCPTR(gmoGetAgentTypeOne);

typedef int  (GMO_CALLCONV *gmoGetBiLevelInfo_t) (gmoHandle_t pgmo, int nintvec[], int mintvec[]);
/** Get equation and variable mapping to agents
 *
 * @param pgmo gmo object handle
 * @param nintvec Array of integers, len=number of columns in user view
 * @param mintvec Array of integers, len=number of rows in user view
 */
GMO_FUNCPTR(gmoGetBiLevelInfo);

typedef int  (GMO_CALLCONV *gmoDumpEMPInfoToGDX_t) (gmoHandle_t pgmo, const char *gdxfname);
/** Dump EMPInfo GDX File
 *
 * @param pgmo gmo object handle
 * @param gdxfname Name of GDX file
 */
GMO_FUNCPTR(gmoDumpEMPInfoToGDX);

typedef double  (GMO_CALLCONV *gmoGetHeadnTail_t) (gmoHandle_t pgmo, int htrec);
/** Get value of solution head or tail record, except for modelstat and solvestat (see enumerated constants)
 *
 * @param pgmo gmo object handle
 * @param htrec Solution head or tail record, (see enumerated constants)
 */
GMO_FUNCPTR(gmoGetHeadnTail);

typedef void  (GMO_CALLCONV *gmoSetHeadnTail_t) (gmoHandle_t pgmo, int htrec, double dval);
/** Set value of solution head or tail record, except for modelstat and solvestat (see enumerated constants)
 *
 * @param pgmo gmo object handle
 * @param htrec Solution head or tail record, (see enumerated constants)
 * @param dval Double value
 */
GMO_FUNCPTR(gmoSetHeadnTail);

typedef int  (GMO_CALLCONV *gmoSetSolutionPrimal_t) (gmoHandle_t pgmo, const double x[]);
/** Set solution values for variable levels
 *
 * @param pgmo gmo object handle
 * @param x Level values of variables
 */
GMO_FUNCPTR(gmoSetSolutionPrimal);

typedef int  (GMO_CALLCONV *gmoSetSolution2_t) (gmoHandle_t pgmo, const double x[], const double pi[]);
/** Set solution values for variable levels and equation marginals
 *
 * @param pgmo gmo object handle
 * @param x Level values of variables
 * @param pi Marginal values of equations
 */
GMO_FUNCPTR(gmoSetSolution2);

typedef int  (GMO_CALLCONV *gmoSetSolution_t) (gmoHandle_t pgmo, const double x[], const double dj[], const double pi[], const double e[]);
/** Set solution values for variable and equation levels as well as marginals
 *
 * @param pgmo gmo object handle
 * @param x Level values of variables
 * @param dj Marginal values of variables
 * @param pi Marginal values of equations
 * @param e Level values of equations
 */
GMO_FUNCPTR(gmoSetSolution);

typedef int  (GMO_CALLCONV *gmoSetSolution8_t) (gmoHandle_t pgmo, const double x[], const double dj[], const double pi[], const double e[], int xb[], int xs[], int yb[], int ys[]);
/** Set solution values for variable and equation levels, marginals and statuses
 *
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
GMO_FUNCPTR(gmoSetSolution8);

typedef int  (GMO_CALLCONV *gmoSetSolutionFixer_t) (gmoHandle_t pgmo, int modelstathint, const double x[], const double pi[], const int xb[], const int yb[], double infTol, double optTol);
/** Construct and set solution based on available inputs
 *
 * @param pgmo gmo object handle
 * @param modelstathint Model status used as a hint
 * @param x Level values of variables
 * @param pi Marginal values of equations
 * @param xb Basis statuses of variables (see enumerated constants)
 * @param yb Basis statuses of equations (see enumerated constants)
 * @param infTol Infeasibility tolerance
 * @param optTol Optimality tolerance
 */
GMO_FUNCPTR(gmoSetSolutionFixer);

typedef int  (GMO_CALLCONV *gmoGetSolutionVarRec_t) (gmoHandle_t pgmo, int sj, double *vl, double *vmarg, int *vstat, int *vcstat);
/** Get variable solution values (level, marginals and statuses)
 *
 * @param pgmo gmo object handle
 * @param sj Index of column in client space
 * @param vl Level of variable
 * @param vmarg Marginal of variable
 * @param vstat Basis status of variable (see enumerated constants)
 * @param vcstat Status of variable (see enumerated constants)
 */
GMO_FUNCPTR(gmoGetSolutionVarRec);

typedef int  (GMO_CALLCONV *gmoSetSolutionVarRec_t) (gmoHandle_t pgmo, int sj, double vl, double vmarg, int vstat, int vcstat);
/** Set variable solution values (level, marginals and statuses)
 *
 * @param pgmo gmo object handle
 * @param sj Index of column in client space
 * @param vl Level of variable
 * @param vmarg Marginal of variable
 * @param vstat Basis status of variable (see enumerated constants)
 * @param vcstat Status of variable (see enumerated constants)
 */
GMO_FUNCPTR(gmoSetSolutionVarRec);

typedef int  (GMO_CALLCONV *gmoGetSolutionEquRec_t) (gmoHandle_t pgmo, int si, double *el, double *emarg, int *estat, int *ecstat);
/** Get equation solution values (level, marginals and statuses)
 *
 * @param pgmo gmo object handle
 * @param si Index of row in client space
 * @param el Level of equation
 * @param emarg Marginal of equation
 * @param estat Basis status of equation (see enumerated constants)
 * @param ecstat Status of equation (see enumerated constants)
 */
GMO_FUNCPTR(gmoGetSolutionEquRec);

typedef int  (GMO_CALLCONV *gmoSetSolutionEquRec_t) (gmoHandle_t pgmo, int si, double el, double emarg, int estat, int ecstat);
/** Set equation solution values (level, marginals and statuses)
 *
 * @param pgmo gmo object handle
 * @param si Index of row in client space
 * @param el Level of equation
 * @param emarg Marginal of equation
 * @param estat Basis status of equation (see enumerated constants)
 * @param ecstat Status of equation (see enumerated constants)
 */
GMO_FUNCPTR(gmoSetSolutionEquRec);

typedef int  (GMO_CALLCONV *gmoSetSolutionStatus_t) (gmoHandle_t pgmo, int xb[], int xs[], int yb[], int ys[]);
/** Set solution values sfor variable and equation statuses
 *
 * @param pgmo gmo object handle
 * @param xb Basis statuses of variables (see enumerated constants)
 * @param xs Statuses of variables (see enumerated constants)
 * @param yb Basis statuses of equations (see enumerated constants)
 * @param ys Statuses of equation (see enumerated constants)
 */
GMO_FUNCPTR(gmoSetSolutionStatus);

typedef void  (GMO_CALLCONV *gmoCompleteObjective_t) (gmoHandle_t pgmo, double locobjval);
/** Complete objective row/col for models with objective function
 *
 * @param pgmo gmo object handle
 * @param locobjval Objective value
 */
GMO_FUNCPTR(gmoCompleteObjective);

typedef int  (GMO_CALLCONV *gmoCompleteSolution_t) (gmoHandle_t pgmo);
/** Complete solution (e.g. for cols/rows not in view)
 *
 * @param pgmo gmo object handle
 */
GMO_FUNCPTR(gmoCompleteSolution);

typedef double  (GMO_CALLCONV *gmoGetAbsoluteGap_t) (gmoHandle_t pgmo);
/** Compute absolute gap w.r.t. objective value and objective estimate in head or tail records
 *
 * @param pgmo gmo object handle
 */
GMO_FUNCPTR(gmoGetAbsoluteGap);

typedef double  (GMO_CALLCONV *gmoGetRelativeGap_t) (gmoHandle_t pgmo);
/** Compute relative gap w.r.t. objective value and objective estimate in head or tail records
 *
 * @param pgmo gmo object handle
 */
GMO_FUNCPTR(gmoGetRelativeGap);

typedef int  (GMO_CALLCONV *gmoLoadSolutionLegacy_t) (gmoHandle_t pgmo);
/** Load solution from legacy solution file
 *
 * @param pgmo gmo object handle
 */
GMO_FUNCPTR(gmoLoadSolutionLegacy);

typedef int  (GMO_CALLCONV *gmoUnloadSolutionLegacy_t) (gmoHandle_t pgmo);
/** Unload solution to legacy solution file
 *
 * @param pgmo gmo object handle
 */
GMO_FUNCPTR(gmoUnloadSolutionLegacy);

typedef int  (GMO_CALLCONV *gmoLoadSolutionGDX_t) (gmoHandle_t pgmo, const char *gdxfname, int dorows, int docols, int doht);
/** Load solution to GDX solution file (optional: rows, cols and-or header and tail info)
 *
 * @param pgmo gmo object handle
 * @param gdxfname Name of GDX file
 * @param dorows Flag whether to read/write row information from/to solution file
 * @param docols Flag whether to read/write column information from/to solution file
 * @param doht Flag whether to read/write head and tail information from/to solution file
 */
GMO_FUNCPTR(gmoLoadSolutionGDX);

typedef int  (GMO_CALLCONV *gmoUnloadSolutionGDX_t) (gmoHandle_t pgmo, const char *gdxfname, int dorows, int docols, int doht);
/** Unload solution to GDX solution file (optional: rows, cols and-or header and tail info)
 *
 * @param pgmo gmo object handle
 * @param gdxfname Name of GDX file
 * @param dorows Flag whether to read/write row information from/to solution file
 * @param docols Flag whether to read/write column information from/to solution file
 * @param doht Flag whether to read/write head and tail information from/to solution file
 */
GMO_FUNCPTR(gmoUnloadSolutionGDX);

typedef int  (GMO_CALLCONV *gmoPrepareAllSolToGDX_t) (gmoHandle_t pgmo, const char *gdxfname, void *scengdx, int dictid);
/** Initialize writing of multiple solutions (e.g. scenarios) to a GDX file
 *
 * @param pgmo gmo object handle
 * @param gdxfname Name of GDX file
 * @param scengdx Pointer to GDX solution file containing multiple solutions, e.g. scenarios
 * @param dictid GDX symbol number of dict
 */
GMO_FUNCPTR(gmoPrepareAllSolToGDX);

typedef int  (GMO_CALLCONV *gmoAddSolutionToGDX_t) (gmoHandle_t pgmo, const char *scenuel[]);
/** Add a solution (e.g. scenario) to the GDX file'
 *
 * @param pgmo gmo object handle
 * @param scenuel Scenario labels
 */
GMO_FUNCPTR(gmoAddSolutionToGDX);

typedef int  (GMO_CALLCONV *gmoWriteSolDone_t) (gmoHandle_t pgmo, char *msg);
/** Finalize writing of multiple solutions (e.g. scenarios) to a GDX file
 *
 * @param pgmo gmo object handle
 * @param msg Message
 */
GMO_FUNCPTR(gmoWriteSolDone);

typedef int  (GMO_CALLCONV *gmoCheckSolPoolUEL_t) (gmoHandle_t pgmo, const char *prefix, int *numsym);
/** heck scenario UEL against dictionary uels and report number of varaible symbols
 *
 * @param pgmo gmo object handle
 * @param prefix 
 * @param numsym Number of symbols
 */
GMO_FUNCPTR(gmoCheckSolPoolUEL);

typedef void * (GMO_CALLCONV *gmoPrepareSolPoolMerge_t) (gmoHandle_t pgmo, const char *gdxfname, int numsol, const char *prefix);
/** Prepare merged solution pool GDX file
 *
 * @param pgmo gmo object handle
 * @param gdxfname Name of GDX file
 * @param numsol Number of solutions in solution pool
 * @param prefix 
 */
GMO_FUNCPTR(gmoPrepareSolPoolMerge);

typedef int  (GMO_CALLCONV *gmoPrepareSolPoolNextSym_t) (gmoHandle_t pgmo, void *handle);
/** Write solution to merged solution pool GDX file
 *
 * @param pgmo gmo object handle
 * @param handle 
 */
GMO_FUNCPTR(gmoPrepareSolPoolNextSym);

typedef int  (GMO_CALLCONV *gmoUnloadSolPoolSolution_t) (gmoHandle_t pgmo, void *handle, int numsol);
/** Write solution to merged solution pool GDX file
 *
 * @param pgmo gmo object handle
 * @param handle 
 * @param numsol Number of solutions in solution pool
 */
GMO_FUNCPTR(gmoUnloadSolPoolSolution);

typedef int  (GMO_CALLCONV *gmoFinalizeSolPoolMerge_t) (gmoHandle_t pgmo, void *handle);
/** Finalize merged solution pool GDX file
 *
 * @param pgmo gmo object handle
 * @param handle 
 */
GMO_FUNCPTR(gmoFinalizeSolPoolMerge);

typedef int  (GMO_CALLCONV *gmoGetVarTypeTxt_t) (gmoHandle_t pgmo, int sj, char *s);
/** String for variable type
 *
 * @param pgmo gmo object handle
 * @param sj Index of column in client space
 * @param s String
 */
GMO_FUNCPTR(gmoGetVarTypeTxt);

typedef int  (GMO_CALLCONV *gmoGetEquTypeTxt_t) (gmoHandle_t pgmo, int si, char *s);
/** String for equation type
 *
 * @param pgmo gmo object handle
 * @param si Index of row in client space
 * @param s String
 */
GMO_FUNCPTR(gmoGetEquTypeTxt);

typedef int  (GMO_CALLCONV *gmoGetSolveStatusTxt_t) (gmoHandle_t pgmo, int solvestat, char *s);
/** String for solvestatus
 *
 * @param pgmo gmo object handle
 * @param solvestat Solver status
 * @param s String
 */
GMO_FUNCPTR(gmoGetSolveStatusTxt);

typedef int  (GMO_CALLCONV *gmoGetModelStatusTxt_t) (gmoHandle_t pgmo, int modelstat, char *s);
/** String for modelstatus
 *
 * @param pgmo gmo object handle
 * @param modelstat Model status
 * @param s String
 */
GMO_FUNCPTR(gmoGetModelStatusTxt);

typedef int  (GMO_CALLCONV *gmoGetModelTypeTxt_t) (gmoHandle_t pgmo, int modeltype, char *s);
/** String for modeltype
 *
 * @param pgmo gmo object handle
 * @param modeltype 
 * @param s String
 */
GMO_FUNCPTR(gmoGetModelTypeTxt);

typedef int  (GMO_CALLCONV *gmoGetHeadNTailTxt_t) (gmoHandle_t pgmo, int htrec, char *s);
/** String for solution head or tail record
 *
 * @param pgmo gmo object handle
 * @param htrec Solution head or tail record, (see enumerated constants)
 * @param s String
 */
GMO_FUNCPTR(gmoGetHeadNTailTxt);

typedef double  (GMO_CALLCONV *gmoMemUsed_t) (gmoHandle_t pgmo);
/** Get current memory consumption of GMO in MB
 *
 * @param pgmo gmo object handle
 */
GMO_FUNCPTR(gmoMemUsed);

typedef double  (GMO_CALLCONV *gmoPeakMemUsed_t) (gmoHandle_t pgmo);
/** Get peak memory consumption of GMO in MB
 *
 * @param pgmo gmo object handle
 */
GMO_FUNCPTR(gmoPeakMemUsed);

typedef int  (GMO_CALLCONV *gmoSetNLObject_t) (gmoHandle_t pgmo, void *nlobject, void *nlpool);
/** Set NL Object and constant pool
 *
 * @param pgmo gmo object handle
 * @param nlobject Object of nonlinear instructions
 * @param nlpool Constant pool object for constants in nonlinear instruction
 */
GMO_FUNCPTR(gmoSetNLObject);

typedef int  (GMO_CALLCONV *gmoDumpQMakerGDX_t) (gmoHandle_t pgmo, const char *gdxfname);
/** Dump QMaker GDX File
 *
 * @param pgmo gmo object handle
 * @param gdxfname Name of GDX file
 */
GMO_FUNCPTR(gmoDumpQMakerGDX);

typedef int  (GMO_CALLCONV *gmoGetVarEquMap_t) (gmoHandle_t pgmo, int maptype, void *optptr, int strict, int *nmappings, int rowindex[], int colindex[], int mapval[]);
/** Get variable equation mapping list
 *
 * @param pgmo gmo object handle
 * @param maptype Type of variable equation mapping
 * @param optptr Option object pointer
 * @param strict 
 * @param nmappings 
 * @param rowindex 
 * @param colindex 
 * @param mapval 
 */
GMO_FUNCPTR(gmoGetVarEquMap);

typedef int  (GMO_CALLCONV *gmoGetIndicatorMap_t) (gmoHandle_t pgmo, void *optptr, int indicstrict, int *numindic, int rowindic[], int colindic[], int indiconval[]);
/** Get indicator constraint list
 *
 * @param pgmo gmo object handle
 * @param optptr Option object pointer
 * @param indicstrict 1: Make the indicator reading strict. 0: accept duplicates, unmatched vars and equs, etc
 * @param numindic Number of indicator constraints
 * @param rowindic map with row indicies
 * @param colindic map with column indicies
 * @param indiconval 0 or 1 value for binary variable to activate the constraint
 */
GMO_FUNCPTR(gmoGetIndicatorMap);

typedef int  (GMO_CALLCONV *gmoCrudeness_t) (gmoHandle_t pgmo);
/** mature = 0 ... 100 = crude/not secure evaluations (non-GAMS evaluators)
 *
 * @param pgmo gmo object handle
 */
GMO_FUNCPTR(gmoCrudeness);

typedef int  (GMO_CALLCONV *gmoDirtyGetRowFNLInstr_t) (gmoHandle_t pgmo, int si, int *len, int opcode[], int field[]);
/** Temporary function to get row function only code, do not use it
 *
 * @param pgmo gmo object handle
 * @param si Index of row in client space
 * @param len Length of array
 * @param opcode Nonlinear code operation
 * @param field Nonlinear code field
 */
GMO_FUNCPTR(gmoDirtyGetRowFNLInstr);

typedef int  (GMO_CALLCONV *gmoDirtyGetObjFNLInstr_t) (gmoHandle_t pgmo, int *len, int opcode[], int field[]);
/** Temporary function to get row function only code, do not use it
 *
 * @param pgmo gmo object handle
 * @param len Length of array
 * @param opcode Nonlinear code operation
 * @param field Nonlinear code field
 */
GMO_FUNCPTR(gmoDirtyGetObjFNLInstr);

typedef int  (GMO_CALLCONV *gmoDirtySetRowFNLInstr_t) (gmoHandle_t pgmo, int si, int len, const int opcode[], const int field[], void *nlpool, double nlpoolvec[], int len2);
/** Temporary function to set row function only code, do not use it
 *
 * @param pgmo gmo object handle
 * @param si Index of row in client space
 * @param len Length of array
 * @param opcode Nonlinear code operation
 * @param field Nonlinear code field
 * @param nlpool Constant pool object for constants in nonlinear instruction
 * @param nlpoolvec Constant pool array for constants in nonlinear instruction
 * @param len2 Length of second array
 */
GMO_FUNCPTR(gmoDirtySetRowFNLInstr);

typedef char * (GMO_CALLCONV *gmoGetExtrLibName_t) (gmoHandle_t pgmo, int libidx, char *buf);
/** Get file name stub of extrinsic function library
 *
 * @param pgmo gmo object handle
 * @param libidx Library index
 */
GMO_FUNCPTR(gmoGetExtrLibName);

typedef void * (GMO_CALLCONV *gmoGetExtrLibObjPtr_t) (gmoHandle_t pgmo, int libidx);
/** Get data object pointer of extrinsic function library
 *
 * @param pgmo gmo object handle
 * @param libidx Library index
 */
GMO_FUNCPTR(gmoGetExtrLibObjPtr);

typedef char * (GMO_CALLCONV *gmoGetExtrLibFuncName_t) (gmoHandle_t pgmo, int libidx, int funcidx, char *buf);
/** Get name of extrinsic function
 *
 * @param pgmo gmo object handle
 * @param libidx Library index
 * @param funcidx Function index
 */
GMO_FUNCPTR(gmoGetExtrLibFuncName);

typedef void * (GMO_CALLCONV *gmoLoadExtrLibEntry_t) (gmoHandle_t pgmo, int libidx, const char *name, char *msg);
/** Load a function from an extrinsic function library
 *
 * @param pgmo gmo object handle
 * @param libidx Library index
 * @param name 
 * @param msg Message
 */
GMO_FUNCPTR(gmoLoadExtrLibEntry);

typedef void * (GMO_CALLCONV *gmoDict_t) (gmoHandle_t pgmo);
/** Load GAMS dictionary object and obtain pointer to it
 *
 * @param pgmo gmo object handle
 */
GMO_FUNCPTR(gmoDict);

typedef void (GMO_CALLCONV *gmoDictSet_t) (gmoHandle_t pgmo, const void *x);
GMO_FUNCPTR(gmoDictSet);

typedef char * (GMO_CALLCONV *gmoNameModel_t) (gmoHandle_t pgmo, char *buf);
/** Name of model
 *
 * @param pgmo gmo object handle
 */
GMO_FUNCPTR(gmoNameModel);

typedef void (GMO_CALLCONV *gmoNameModelSet_t) (gmoHandle_t pgmo, const char *x);
GMO_FUNCPTR(gmoNameModelSet);

typedef int  (GMO_CALLCONV *gmoModelSeqNr_t) (gmoHandle_t pgmo);
/** Sequence number of model (0..n)
 *
 * @param pgmo gmo object handle
 */
GMO_FUNCPTR(gmoModelSeqNr);

typedef void (GMO_CALLCONV *gmoModelSeqNrSet_t) (gmoHandle_t pgmo, const int x);
GMO_FUNCPTR(gmoModelSeqNrSet);

typedef int  (GMO_CALLCONV *gmoModelType_t) (gmoHandle_t pgmo);
/** Type of Model
 *
 * @param pgmo gmo object handle
 */
GMO_FUNCPTR(gmoModelType);

typedef void (GMO_CALLCONV *gmoModelTypeSet_t) (gmoHandle_t pgmo, const int x);
GMO_FUNCPTR(gmoModelTypeSet);

typedef int  (GMO_CALLCONV *gmoNLModelType_t) (gmoHandle_t pgmo);
/** Type of Model
 *
 * @param pgmo gmo object handle
 */
GMO_FUNCPTR(gmoNLModelType);

typedef int  (GMO_CALLCONV *gmoSense_t) (gmoHandle_t pgmo);
/** Direction of optimization, see enumerated constants
 *
 * @param pgmo gmo object handle
 */
GMO_FUNCPTR(gmoSense);

typedef void (GMO_CALLCONV *gmoSenseSet_t) (gmoHandle_t pgmo, const int x);
GMO_FUNCPTR(gmoSenseSet);

typedef int  (GMO_CALLCONV *gmoIsQP_t) (gmoHandle_t pgmo);
/** Is this a QP or not
 *
 * @param pgmo gmo object handle
 */
GMO_FUNCPTR(gmoIsQP);

typedef int  (GMO_CALLCONV *gmoOptFile_t) (gmoHandle_t pgmo);
/** Number of option file
 *
 * @param pgmo gmo object handle
 */
GMO_FUNCPTR(gmoOptFile);

typedef void (GMO_CALLCONV *gmoOptFileSet_t) (gmoHandle_t pgmo, const int x);
GMO_FUNCPTR(gmoOptFileSet);

typedef int  (GMO_CALLCONV *gmoDictionary_t) (gmoHandle_t pgmo);
/** Dictionary flag
 *
 * @param pgmo gmo object handle
 */
GMO_FUNCPTR(gmoDictionary);

typedef void (GMO_CALLCONV *gmoDictionarySet_t) (gmoHandle_t pgmo, const int x);
GMO_FUNCPTR(gmoDictionarySet);

typedef int  (GMO_CALLCONV *gmoScaleOpt_t) (gmoHandle_t pgmo);
/** Scaling flag
 *
 * @param pgmo gmo object handle
 */
GMO_FUNCPTR(gmoScaleOpt);

typedef void (GMO_CALLCONV *gmoScaleOptSet_t) (gmoHandle_t pgmo, const int x);
GMO_FUNCPTR(gmoScaleOptSet);

typedef int  (GMO_CALLCONV *gmoPriorOpt_t) (gmoHandle_t pgmo);
/** Priority Flag
 *
 * @param pgmo gmo object handle
 */
GMO_FUNCPTR(gmoPriorOpt);

typedef void (GMO_CALLCONV *gmoPriorOptSet_t) (gmoHandle_t pgmo, const int x);
GMO_FUNCPTR(gmoPriorOptSet);

typedef int  (GMO_CALLCONV *gmoHaveBasis_t) (gmoHandle_t pgmo);
/** Do we have basis
 *
 * @param pgmo gmo object handle
 */
GMO_FUNCPTR(gmoHaveBasis);

typedef void (GMO_CALLCONV *gmoHaveBasisSet_t) (gmoHandle_t pgmo, const int x);
GMO_FUNCPTR(gmoHaveBasisSet);

typedef int  (GMO_CALLCONV *gmoModelStat_t) (gmoHandle_t pgmo);
/** Model status, see enumerated constants
 *
 * @param pgmo gmo object handle
 */
GMO_FUNCPTR(gmoModelStat);

typedef void (GMO_CALLCONV *gmoModelStatSet_t) (gmoHandle_t pgmo, const int x);
GMO_FUNCPTR(gmoModelStatSet);

typedef int  (GMO_CALLCONV *gmoSolveStat_t) (gmoHandle_t pgmo);
/** Solver status, see enumerated constants
 *
 * @param pgmo gmo object handle
 */
GMO_FUNCPTR(gmoSolveStat);

typedef void (GMO_CALLCONV *gmoSolveStatSet_t) (gmoHandle_t pgmo, const int x);
GMO_FUNCPTR(gmoSolveStatSet);

typedef int  (GMO_CALLCONV *gmoIsMPSGE_t) (gmoHandle_t pgmo);
/** Is this an MPSGE model
 *
 * @param pgmo gmo object handle
 */
GMO_FUNCPTR(gmoIsMPSGE);

typedef void (GMO_CALLCONV *gmoIsMPSGESet_t) (gmoHandle_t pgmo, const int x);
GMO_FUNCPTR(gmoIsMPSGESet);

typedef int  (GMO_CALLCONV *gmoObjStyle_t) (gmoHandle_t pgmo);
/** Style of objective, see enumerated constants
 *
 * @param pgmo gmo object handle
 */
GMO_FUNCPTR(gmoObjStyle);

typedef void (GMO_CALLCONV *gmoObjStyleSet_t) (gmoHandle_t pgmo, const int x);
GMO_FUNCPTR(gmoObjStyleSet);

typedef int  (GMO_CALLCONV *gmoInterface_t) (gmoHandle_t pgmo);
/** Interface type (raw vs. processed), see enumerated constants
 *
 * @param pgmo gmo object handle
 */
GMO_FUNCPTR(gmoInterface);

typedef void (GMO_CALLCONV *gmoInterfaceSet_t) (gmoHandle_t pgmo, const int x);
GMO_FUNCPTR(gmoInterfaceSet);

typedef int  (GMO_CALLCONV *gmoIndexBase_t) (gmoHandle_t pgmo);
/** User array index base (0 or 1
 *
 * @param pgmo gmo object handle
 */
GMO_FUNCPTR(gmoIndexBase);

typedef void (GMO_CALLCONV *gmoIndexBaseSet_t) (gmoHandle_t pgmo, const int x);
GMO_FUNCPTR(gmoIndexBaseSet);

typedef int  (GMO_CALLCONV *gmoObjReform_t) (gmoHandle_t pgmo);
/** Reformulate objective if possibl
 *
 * @param pgmo gmo object handle
 */
GMO_FUNCPTR(gmoObjReform);

typedef void (GMO_CALLCONV *gmoObjReformSet_t) (gmoHandle_t pgmo, const int x);
GMO_FUNCPTR(gmoObjReformSet);

typedef int  (GMO_CALLCONV *gmoEmptyOut_t) (gmoHandle_t pgmo);
/** Reformulate objective even if objective variable is the only variabl
 *
 * @param pgmo gmo object handle
 */
GMO_FUNCPTR(gmoEmptyOut);

typedef void (GMO_CALLCONV *gmoEmptyOutSet_t) (gmoHandle_t pgmo, const int x);
GMO_FUNCPTR(gmoEmptyOutSet);

typedef int  (GMO_CALLCONV *gmoIgnXCDeriv_t) (gmoHandle_t pgmo);
/** Consider constant derivatives in external functions or no
 *
 * @param pgmo gmo object handle
 */
GMO_FUNCPTR(gmoIgnXCDeriv);

typedef void (GMO_CALLCONV *gmoIgnXCDerivSet_t) (gmoHandle_t pgmo, const int x);
GMO_FUNCPTR(gmoIgnXCDerivSet);

typedef int  (GMO_CALLCONV *gmoUseQ_t) (gmoHandle_t pgmo);
/** Toggle Q-mode
 *
 * @param pgmo gmo object handle
 */
GMO_FUNCPTR(gmoUseQ);

typedef void (GMO_CALLCONV *gmoUseQSet_t) (gmoHandle_t pgmo, const int x);
GMO_FUNCPTR(gmoUseQSet);

typedef int  (GMO_CALLCONV *gmoQExtractAlg_t) (gmoHandle_t pgmo);
/** Choose Q extraction algorithm (must be set before UseQ; 0: automatic; 1: ThreePass; 2: DoubleForward; 3: Concurrent)
 *
 * @param pgmo gmo object handle
 */
GMO_FUNCPTR(gmoQExtractAlg);

typedef void (GMO_CALLCONV *gmoQExtractAlgSet_t) (gmoHandle_t pgmo, const int x);
GMO_FUNCPTR(gmoQExtractAlgSet);

typedef int  (GMO_CALLCONV *gmoAltBounds_t) (gmoHandle_t pgmo);
/** Use alternative bound
 *
 * @param pgmo gmo object handle
 */
GMO_FUNCPTR(gmoAltBounds);

typedef void (GMO_CALLCONV *gmoAltBoundsSet_t) (gmoHandle_t pgmo, const int x);
GMO_FUNCPTR(gmoAltBoundsSet);

typedef int  (GMO_CALLCONV *gmoAltRHS_t) (gmoHandle_t pgmo);
/** Use alternative RH
 *
 * @param pgmo gmo object handle
 */
GMO_FUNCPTR(gmoAltRHS);

typedef void (GMO_CALLCONV *gmoAltRHSSet_t) (gmoHandle_t pgmo, const int x);
GMO_FUNCPTR(gmoAltRHSSet);

typedef int  (GMO_CALLCONV *gmoAltVarTypes_t) (gmoHandle_t pgmo);
/** Use alternative variable type
 *
 * @param pgmo gmo object handle
 */
GMO_FUNCPTR(gmoAltVarTypes);

typedef void (GMO_CALLCONV *gmoAltVarTypesSet_t) (gmoHandle_t pgmo, const int x);
GMO_FUNCPTR(gmoAltVarTypesSet);

typedef int  (GMO_CALLCONV *gmoForceLinear_t) (gmoHandle_t pgmo);
/** Force linear representation of mode
 *
 * @param pgmo gmo object handle
 */
GMO_FUNCPTR(gmoForceLinear);

typedef void (GMO_CALLCONV *gmoForceLinearSet_t) (gmoHandle_t pgmo, const int x);
GMO_FUNCPTR(gmoForceLinearSet);

typedef int  (GMO_CALLCONV *gmoForceCont_t) (gmoHandle_t pgmo);
/** Force continuous relaxation of mode
 *
 * @param pgmo gmo object handle
 */
GMO_FUNCPTR(gmoForceCont);

typedef void (GMO_CALLCONV *gmoForceContSet_t) (gmoHandle_t pgmo, const int x);
GMO_FUNCPTR(gmoForceContSet);

typedef int  (GMO_CALLCONV *gmoPermuteCols_t) (gmoHandle_t pgmo);
/** Column permutation fla
 *
 * @param pgmo gmo object handle
 */
GMO_FUNCPTR(gmoPermuteCols);

typedef void (GMO_CALLCONV *gmoPermuteColsSet_t) (gmoHandle_t pgmo, const int x);
GMO_FUNCPTR(gmoPermuteColsSet);

typedef int  (GMO_CALLCONV *gmoPermuteRows_t) (gmoHandle_t pgmo);
/** Row permutation fla
 *
 * @param pgmo gmo object handle
 */
GMO_FUNCPTR(gmoPermuteRows);

typedef void (GMO_CALLCONV *gmoPermuteRowsSet_t) (gmoHandle_t pgmo, const int x);
GMO_FUNCPTR(gmoPermuteRowsSet);

typedef double  (GMO_CALLCONV *gmoPinf_t) (gmoHandle_t pgmo);
/** Value for plus infinit
 *
 * @param pgmo gmo object handle
 */
GMO_FUNCPTR(gmoPinf);

typedef void (GMO_CALLCONV *gmoPinfSet_t) (gmoHandle_t pgmo, const double x);
GMO_FUNCPTR(gmoPinfSet);

typedef double  (GMO_CALLCONV *gmoMinf_t) (gmoHandle_t pgmo);
/** Value for minus infinit
 *
 * @param pgmo gmo object handle
 */
GMO_FUNCPTR(gmoMinf);

typedef void (GMO_CALLCONV *gmoMinfSet_t) (gmoHandle_t pgmo, const double x);
GMO_FUNCPTR(gmoMinfSet);

typedef double  (GMO_CALLCONV *gmoQNaN_t) (gmoHandle_t pgmo);
/** quiet IEEE NaN
 *
 * @param pgmo gmo object handle
 */
GMO_FUNCPTR(gmoQNaN);

typedef double  (GMO_CALLCONV *gmoValNA_t) (gmoHandle_t pgmo);
/** Double Value of N/A
 *
 * @param pgmo gmo object handle
 */
GMO_FUNCPTR(gmoValNA);

typedef int  (GMO_CALLCONV *gmoValNAInt_t) (gmoHandle_t pgmo);
/** Integer Value of N/A
 *
 * @param pgmo gmo object handle
 */
GMO_FUNCPTR(gmoValNAInt);

typedef double  (GMO_CALLCONV *gmoValUndf_t) (gmoHandle_t pgmo);
/** Double Value of UNDF
 *
 * @param pgmo gmo object handle
 */
GMO_FUNCPTR(gmoValUndf);

typedef int  (GMO_CALLCONV *gmoM_t) (gmoHandle_t pgmo);
/** Number of rows
 *
 * @param pgmo gmo object handle
 */
GMO_FUNCPTR(gmoM);

typedef int  (GMO_CALLCONV *gmoQM_t) (gmoHandle_t pgmo);
/** Number of quadratic rows (-1 if Q information not used)
 *
 * @param pgmo gmo object handle
 */
GMO_FUNCPTR(gmoQM);

typedef int  (GMO_CALLCONV *gmoNLM_t) (gmoHandle_t pgmo);
/** Number of nonlinear rows
 *
 * @param pgmo gmo object handle
 */
GMO_FUNCPTR(gmoNLM);

typedef int  (GMO_CALLCONV *gmoNRowMatch_t) (gmoHandle_t pgmo);
/** Number of matched rows
 *
 * @param pgmo gmo object handle
 */
GMO_FUNCPTR(gmoNRowMatch);

typedef int  (GMO_CALLCONV *gmoN_t) (gmoHandle_t pgmo);
/** Number of columns
 *
 * @param pgmo gmo object handle
 */
GMO_FUNCPTR(gmoN);

typedef int  (GMO_CALLCONV *gmoNLN_t) (gmoHandle_t pgmo);
/** Number of nonlinear columns
 *
 * @param pgmo gmo object handle
 */
GMO_FUNCPTR(gmoNLN);

typedef int  (GMO_CALLCONV *gmoNDisc_t) (gmoHandle_t pgmo);
/** Number of discontinuous column
 *
 * @param pgmo gmo object handle
 */
GMO_FUNCPTR(gmoNDisc);

typedef int  (GMO_CALLCONV *gmoNFixed_t) (gmoHandle_t pgmo);
/** Number of fixed column
 *
 * @param pgmo gmo object handle
 */
GMO_FUNCPTR(gmoNFixed);

typedef int  (GMO_CALLCONV *gmoNColMatch_t) (gmoHandle_t pgmo);
/** Number of matched column
 *
 * @param pgmo gmo object handle
 */
GMO_FUNCPTR(gmoNColMatch);

typedef int  (GMO_CALLCONV *gmoNZ_t) (gmoHandle_t pgmo);
/** Number of nonzeros in Jacobian matrix
 *
 * @param pgmo gmo object handle
 */
GMO_FUNCPTR(gmoNZ);

typedef INT64  (GMO_CALLCONV *gmoNZ64_t) (gmoHandle_t pgmo);
/** Number of nonzeros in Jacobian matrix
 *
 * @param pgmo gmo object handle
 */
GMO_FUNCPTR(gmoNZ64);

typedef int  (GMO_CALLCONV *gmoNLNZ_t) (gmoHandle_t pgmo);
/** Number of nonlinear nonzeros in Jacobian matrix
 *
 * @param pgmo gmo object handle
 */
GMO_FUNCPTR(gmoNLNZ);

typedef INT64  (GMO_CALLCONV *gmoNLNZ64_t) (gmoHandle_t pgmo);
/** Number of nonlinear nonzeros in Jacobian matrix
 *
 * @param pgmo gmo object handle
 */
GMO_FUNCPTR(gmoNLNZ64);

typedef int  (GMO_CALLCONV *gmoLNZEx_t) (gmoHandle_t pgmo);
/** Number of linear nonzeros in Jacobian matrix
 *
 * @param pgmo gmo object handle
 */
GMO_FUNCPTR(gmoLNZEx);

typedef INT64  (GMO_CALLCONV *gmoLNZEx64_t) (gmoHandle_t pgmo);
/** Number of linear nonzeros in Jacobian matrix
 *
 * @param pgmo gmo object handle
 */
GMO_FUNCPTR(gmoLNZEx64);

typedef int  (GMO_CALLCONV *gmoLNZ_t) (gmoHandle_t pgmo);
/** Legacy overestimate for the count of linear nonzeros in Jacobian matrix, especially if gmoUseQ is true
 *
 * @param pgmo gmo object handle
 */
GMO_FUNCPTR(gmoLNZ);

typedef INT64  (GMO_CALLCONV *gmoLNZ64_t) (gmoHandle_t pgmo);
/** Legacy overestimate for the count of linear nonzeros in Jacobian matrix, especially if gmoUseQ is true
 *
 * @param pgmo gmo object handle
 */
GMO_FUNCPTR(gmoLNZ64);

typedef int  (GMO_CALLCONV *gmoQNZ_t) (gmoHandle_t pgmo);
/** Number of quadratic nonzeros in Jacobian matrix, 0 if gmoUseQ is false
 *
 * @param pgmo gmo object handle
 */
GMO_FUNCPTR(gmoQNZ);

typedef INT64  (GMO_CALLCONV *gmoQNZ64_t) (gmoHandle_t pgmo);
/** Number of quadratic nonzeros in Jacobian matrix, 0 if gmoUseQ is false
 *
 * @param pgmo gmo object handle
 */
GMO_FUNCPTR(gmoQNZ64);

typedef int  (GMO_CALLCONV *gmoGNLNZ_t) (gmoHandle_t pgmo);
/** Number of general nonlinear nonzeros in Jacobian matrix, equals gmoNLNZ if gmoUseQ is false
 *
 * @param pgmo gmo object handle
 */
GMO_FUNCPTR(gmoGNLNZ);

typedef INT64  (GMO_CALLCONV *gmoGNLNZ64_t) (gmoHandle_t pgmo);
/** Number of general nonlinear nonzeros in Jacobian matrix, equals gmoNLNZ if gmoUseQ is false
 *
 * @param pgmo gmo object handle
 */
GMO_FUNCPTR(gmoGNLNZ64);

typedef int  (GMO_CALLCONV *gmoMaxQNZ_t) (gmoHandle_t pgmo);
/** Maximum number of nonzeros in single Q matrix (-1 on overflow)
 *
 * @param pgmo gmo object handle
 */
GMO_FUNCPTR(gmoMaxQNZ);

typedef INT64  (GMO_CALLCONV *gmoMaxQNZ64_t) (gmoHandle_t pgmo);
/** Maximum number of nonzeros in single Q matrix
 *
 * @param pgmo gmo object handle
 */
GMO_FUNCPTR(gmoMaxQNZ64);

typedef int  (GMO_CALLCONV *gmoObjNZ_t) (gmoHandle_t pgmo);
/** Number of nonzeros in objective gradient
 *
 * @param pgmo gmo object handle
 */
GMO_FUNCPTR(gmoObjNZ);

typedef int  (GMO_CALLCONV *gmoObjLNZ_t) (gmoHandle_t pgmo);
/** Number of linear nonzeros in objective gradient
 *
 * @param pgmo gmo object handle
 */
GMO_FUNCPTR(gmoObjLNZ);

typedef int  (GMO_CALLCONV *gmoObjQNZEx_t) (gmoHandle_t pgmo);
/** Number of GMOORDER_Q nonzeros in objective gradient
 *
 * @param pgmo gmo object handle
 */
GMO_FUNCPTR(gmoObjQNZEx);

typedef int  (GMO_CALLCONV *gmoObjNLNZ_t) (gmoHandle_t pgmo);
/** Number of nonlinear nonzeros in objective gradient
 *
 * @param pgmo gmo object handle
 */
GMO_FUNCPTR(gmoObjNLNZ);

typedef int  (GMO_CALLCONV *gmoObjNLNZEx_t) (gmoHandle_t pgmo);
/** Number of GMOORDER_NL nonzeros in objective gradient
 *
 * @param pgmo gmo object handle
 */
GMO_FUNCPTR(gmoObjNLNZEx);

typedef int  (GMO_CALLCONV *gmoObjQMatNZ_t) (gmoHandle_t pgmo);
/** Number of nonzeros in lower triangle of Q matrix of objective (-1 if useQ false or overflow)
 *
 * @param pgmo gmo object handle
 */
GMO_FUNCPTR(gmoObjQMatNZ);

typedef INT64  (GMO_CALLCONV *gmoObjQMatNZ64_t) (gmoHandle_t pgmo);
/** Number of nonzeros in lower triangle of Q matrix of objective (-1 if useQ false)
 *
 * @param pgmo gmo object handle
 */
GMO_FUNCPTR(gmoObjQMatNZ64);

typedef int  (GMO_CALLCONV *gmoObjQNZ_t) (gmoHandle_t pgmo);
/** deprecated synonym for gmoObjQMatNZ
 *
 * @param pgmo gmo object handle
 */
GMO_FUNCPTR(gmoObjQNZ);

typedef int  (GMO_CALLCONV *gmoObjQDiagNZ_t) (gmoHandle_t pgmo);
/** Number of nonzeros on diagonal of Q matrix of objective (-1 if useQ false)
 *
 * @param pgmo gmo object handle
 */
GMO_FUNCPTR(gmoObjQDiagNZ);

typedef int  (GMO_CALLCONV *gmoObjCVecNZ_t) (gmoHandle_t pgmo);
/** Number of nonzeros in c vector of objective (-1 if Q information not used)
 *
 * @param pgmo gmo object handle
 */
GMO_FUNCPTR(gmoObjCVecNZ);

typedef int  (GMO_CALLCONV *gmoNLConst_t) (gmoHandle_t pgmo);
/** Length of constant pool in nonlinear code
 *
 * @param pgmo gmo object handle
 */
GMO_FUNCPTR(gmoNLConst);

typedef void (GMO_CALLCONV *gmoNLConstSet_t) (gmoHandle_t pgmo, const int x);
GMO_FUNCPTR(gmoNLConstSet);

typedef int  (GMO_CALLCONV *gmoNLCodeSize_t) (gmoHandle_t pgmo);
/** Nonlinear code siz
 *
 * @param pgmo gmo object handle
 */
GMO_FUNCPTR(gmoNLCodeSize);

typedef void (GMO_CALLCONV *gmoNLCodeSizeSet_t) (gmoHandle_t pgmo, const int x);
GMO_FUNCPTR(gmoNLCodeSizeSet);

typedef int  (GMO_CALLCONV *gmoNLCodeSizeMaxRow_t) (gmoHandle_t pgmo);
/** Maximum nonlinear code size for row
 *
 * @param pgmo gmo object handle
 */
GMO_FUNCPTR(gmoNLCodeSizeMaxRow);

typedef int  (GMO_CALLCONV *gmoObjVar_t) (gmoHandle_t pgmo);
/** Index of objective variable
 *
 * @param pgmo gmo object handle
 */
GMO_FUNCPTR(gmoObjVar);

typedef void (GMO_CALLCONV *gmoObjVarSet_t) (gmoHandle_t pgmo, const int x);
GMO_FUNCPTR(gmoObjVarSet);

typedef int  (GMO_CALLCONV *gmoObjRow_t) (gmoHandle_t pgmo);
/** Index of objective row
 *
 * @param pgmo gmo object handle
 */
GMO_FUNCPTR(gmoObjRow);

typedef int  (GMO_CALLCONV *gmoGetObjOrder_t) (gmoHandle_t pgmo);
/** Order of Objective, see enumerated constants
 *
 * @param pgmo gmo object handle
 */
GMO_FUNCPTR(gmoGetObjOrder);

typedef double  (GMO_CALLCONV *gmoObjConst_t) (gmoHandle_t pgmo);
/** Objective constant
 *
 * @param pgmo gmo object handle
 */
GMO_FUNCPTR(gmoObjConst);

typedef double  (GMO_CALLCONV *gmoObjConstEx_t) (gmoHandle_t pgmo);
/** Objective constant - this is independent of useQ
 *
 * @param pgmo gmo object handle
 */
GMO_FUNCPTR(gmoObjConstEx);

typedef double  (GMO_CALLCONV *gmoObjQConst_t) (gmoHandle_t pgmo);
/** Get constant in solvers quadratic objective
 *
 * @param pgmo gmo object handle
 */
GMO_FUNCPTR(gmoObjQConst);

typedef double  (GMO_CALLCONV *gmoObjJacVal_t) (gmoHandle_t pgmo);
/** Value of Jacobian element of objective variable in objective
 *
 * @param pgmo gmo object handle
 */
GMO_FUNCPTR(gmoObjJacVal);

typedef int  (GMO_CALLCONV *gmoEvalErrorMethod_t) (gmoHandle_t pgmo);
/** Method for returning on nonlinear evaluation errors
 *
 * @param pgmo gmo object handle
 */
GMO_FUNCPTR(gmoEvalErrorMethod);

typedef void (GMO_CALLCONV *gmoEvalErrorMethodSet_t) (gmoHandle_t pgmo, const int x);
GMO_FUNCPTR(gmoEvalErrorMethodSet);

typedef int  (GMO_CALLCONV *gmoEvalMaxThreads_t) (gmoHandle_t pgmo);
/** Maximum number of threads that can be used for evaluation
 *
 * @param pgmo gmo object handle
 */
GMO_FUNCPTR(gmoEvalMaxThreads);

typedef void (GMO_CALLCONV *gmoEvalMaxThreadsSet_t) (gmoHandle_t pgmo, const int x);
GMO_FUNCPTR(gmoEvalMaxThreadsSet);

typedef int  (GMO_CALLCONV *gmoEvalFuncCount_t) (gmoHandle_t pgmo);
/** Number of function evaluations
 *
 * @param pgmo gmo object handle
 */
GMO_FUNCPTR(gmoEvalFuncCount);

typedef double  (GMO_CALLCONV *gmoEvalFuncTimeUsed_t) (gmoHandle_t pgmo);
/** Time used for function evaluations in s
 *
 * @param pgmo gmo object handle
 */
GMO_FUNCPTR(gmoEvalFuncTimeUsed);

typedef int  (GMO_CALLCONV *gmoEvalGradCount_t) (gmoHandle_t pgmo);
/** Number of gradient evaluations
 *
 * @param pgmo gmo object handle
 */
GMO_FUNCPTR(gmoEvalGradCount);

typedef double  (GMO_CALLCONV *gmoEvalGradTimeUsed_t) (gmoHandle_t pgmo);
/** Time used for gradient evaluations in s
 *
 * @param pgmo gmo object handle
 */
GMO_FUNCPTR(gmoEvalGradTimeUsed);

typedef int  (GMO_CALLCONV *gmoHessMaxDim_t) (gmoHandle_t pgmo);
/** Maximum dimension of Hessian
 *
 * @param pgmo gmo object handle
 */
GMO_FUNCPTR(gmoHessMaxDim);

typedef int  (GMO_CALLCONV *gmoHessMaxNz_t) (gmoHandle_t pgmo);
/** Maximum number of nonzeros in Hessian
 *
 * @param pgmo gmo object handle
 */
GMO_FUNCPTR(gmoHessMaxNz);

typedef INT64  (GMO_CALLCONV *gmoHessMaxNz64_t) (gmoHandle_t pgmo);
/** Maximum number of nonzeros in Hessian
 *
 * @param pgmo gmo object handle
 */
GMO_FUNCPTR(gmoHessMaxNz64);

typedef int  (GMO_CALLCONV *gmoHessLagDim_t) (gmoHandle_t pgmo);
/** Dimension of Hessian of the Lagrangian
 *
 * @param pgmo gmo object handle
 */
GMO_FUNCPTR(gmoHessLagDim);

typedef int  (GMO_CALLCONV *gmoHessLagNz_t) (gmoHandle_t pgmo);
/** Nonzeros in Hessian of the Lagrangian
 *
 * @param pgmo gmo object handle
 */
GMO_FUNCPTR(gmoHessLagNz);

typedef INT64  (GMO_CALLCONV *gmoHessLagNz64_t) (gmoHandle_t pgmo);
/** Nonzeros in Hessian of the Lagrangian
 *
 * @param pgmo gmo object handle
 */
GMO_FUNCPTR(gmoHessLagNz64);

typedef int  (GMO_CALLCONV *gmoHessLagDiagNz_t) (gmoHandle_t pgmo);
/** Nonzeros on Diagonal of Hessian of the Lagrangian
 *
 * @param pgmo gmo object handle
 */
GMO_FUNCPTR(gmoHessLagDiagNz);

typedef int  (GMO_CALLCONV *gmoHessInclQRows_t) (gmoHandle_t pgmo);
/** if useQ is true, still include GMOORDER_Q rows in the Hessian
 *
 * @param pgmo gmo object handle
 */
GMO_FUNCPTR(gmoHessInclQRows);

typedef void (GMO_CALLCONV *gmoHessInclQRowsSet_t) (gmoHandle_t pgmo, const int x);
GMO_FUNCPTR(gmoHessInclQRowsSet);

typedef int  (GMO_CALLCONV *gmoNumVIFunc_t) (gmoHandle_t pgmo);
/** EMP: Number of variational inequalities in model rim
 *
 * @param pgmo gmo object handle
 */
GMO_FUNCPTR(gmoNumVIFunc);

typedef int  (GMO_CALLCONV *gmoNumAgents_t) (gmoHandle_t pgmo);
/** EMP: Number of Agents/Followers
 *
 * @param pgmo gmo object handle
 */
GMO_FUNCPTR(gmoNumAgents);

typedef char * (GMO_CALLCONV *gmoNameOptFile_t) (gmoHandle_t pgmo, char *buf);
/** Name of option file
 *
 * @param pgmo gmo object handle
 */
GMO_FUNCPTR(gmoNameOptFile);

typedef void (GMO_CALLCONV *gmoNameOptFileSet_t) (gmoHandle_t pgmo, const char *x);
GMO_FUNCPTR(gmoNameOptFileSet);

typedef char * (GMO_CALLCONV *gmoNameSolFile_t) (gmoHandle_t pgmo, char *buf);
/** Name of solution file
 *
 * @param pgmo gmo object handle
 */
GMO_FUNCPTR(gmoNameSolFile);

typedef void (GMO_CALLCONV *gmoNameSolFileSet_t) (gmoHandle_t pgmo, const char *x);
GMO_FUNCPTR(gmoNameSolFileSet);

typedef char * (GMO_CALLCONV *gmoNameXLib_t) (gmoHandle_t pgmo, char *buf);
/** Name of external function library
 *
 * @param pgmo gmo object handle
 */
GMO_FUNCPTR(gmoNameXLib);

typedef void (GMO_CALLCONV *gmoNameXLibSet_t) (gmoHandle_t pgmo, const char *x);
GMO_FUNCPTR(gmoNameXLibSet);

typedef char * (GMO_CALLCONV *gmoNameMatrix_t) (gmoHandle_t pgmo, char *buf);
/** Name of matrix file
 *
 * @param pgmo gmo object handle
 */
GMO_FUNCPTR(gmoNameMatrix);

typedef char * (GMO_CALLCONV *gmoNameDict_t) (gmoHandle_t pgmo, char *buf);
/** Name of dictionary file
 *
 * @param pgmo gmo object handle
 */
GMO_FUNCPTR(gmoNameDict);

typedef void (GMO_CALLCONV *gmoNameDictSet_t) (gmoHandle_t pgmo, const char *x);
GMO_FUNCPTR(gmoNameDictSet);

typedef char * (GMO_CALLCONV *gmoNameInput_t) (gmoHandle_t pgmo, char *buf);
/** Name of input file (with .gms stripped)
 *
 * @param pgmo gmo object handle
 */
GMO_FUNCPTR(gmoNameInput);

typedef void (GMO_CALLCONV *gmoNameInputSet_t) (gmoHandle_t pgmo, const char *x);
GMO_FUNCPTR(gmoNameInputSet);

typedef char * (GMO_CALLCONV *gmoNameOutput_t) (gmoHandle_t pgmo, char *buf);
/** Name of output file (with .dat stripped)
 *
 * @param pgmo gmo object handle
 */
GMO_FUNCPTR(gmoNameOutput);

typedef void * (GMO_CALLCONV *gmoPPool_t) (gmoHandle_t pgmo);
/** Pointer to constant pool
 *
 * @param pgmo gmo object handle
 */
GMO_FUNCPTR(gmoPPool);

typedef void * (GMO_CALLCONV *gmoIOMutex_t) (gmoHandle_t pgmo);
/** IO mutex
 *
 * @param pgmo gmo object handle
 */
GMO_FUNCPTR(gmoIOMutex);

typedef int  (GMO_CALLCONV *gmoError_t) (gmoHandle_t pgmo);
/** Access to error indicator
 *
 * @param pgmo gmo object handle
 */
GMO_FUNCPTR(gmoError);

typedef void (GMO_CALLCONV *gmoErrorSet_t) (gmoHandle_t pgmo, const int x);
GMO_FUNCPTR(gmoErrorSet);

typedef char * (GMO_CALLCONV *gmoErrorMessage_t) (gmoHandle_t pgmo, char *buf);
/** Provide the last error message
 *
 * @param pgmo gmo object handle
 */
GMO_FUNCPTR(gmoErrorMessage);

#if defined(__cplusplus)
}
#endif
#endif /* #if ! defined(_GMOCC_H_) */

