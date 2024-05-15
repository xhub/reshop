/* gevmcc.h
 * Header file for C-style interface to the GEV library
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


#if ! defined(_GEVCC_H_)
#     define  _GEVCC_H_

#define GEVAPIVERSION 8

enum gevLogStatMode {
  gevdoErr  = 0,
  gevdoStat = 1,
  gevdoLog  = 2  };

enum gevCallSolverMode {
  gevSolverSameStreams = 0,
  gevSolverQuiet       = 1,
  gevSolverOwnFile     = 2  };

enum gevCallSolverSolveLink {
  gevSolveLinkCallScript    = 1,
  gevSolveLinkCallModule    = 2,
  gevSolveLinkAsyncGrid     = 3,
  gevSolveLinkAsyncSimulate = 4,
  gevSolveLinkLoadLibrary   = 5  };

#define gevPageWidth        "PageWidth"  /* gevOptions */
#define gevPageSize         "PageSize"
#define gevsubsysFile       "subsysFile"
#define gevNameScrDir       "NameScrDir"
#define gevNameSysDir       "NameSysDir"
#define gevNameCurDir       "NameCurDir"
#define gevNameWrkDir       "NameWrkDir"
#define gevLogOption        "LogOption"
#define gevNameLogFile      "NameLogFile"
#define gevNameCtrFile      "NameCtrFile"
#define gevNameMatrix       "NameMatrix"
#define gevNameInstr        "NameInstr"
#define gevNameStaFile      "NameStaFile"
#define gevlicenseFile      "licenseFile"
#define gevKeep             "Keep"
#define gevIDEFlag          "IDEFlag"
#define gevIterLim          "IterLim"
#define gevDomLim           "DomLim"
#define gevResLim           "ResLim"
#define gevOptCR            "OptCR"
#define gevOptCA            "OptCA"
#define gevSysOut           "SysOut"
#define gevNodeLim          "NodeLim"
#define gevWorkFactor       "WorkFactor"
#define gevWorkSpace        "WorkSpace"
#define gevSavePoint        "SavePoint"
#define gevHeapLimit        "HeapLimit"
#define gevNameScrExt       "NameScrExt"
#define gevInteger1         "Integer1"
#define gevInteger2         "Integer2"
#define gevInteger3         "Integer3"
#define gevInteger4         "Integer4"
#define gevInteger5         "Integer5"
#define gevFDDelta          "FDDelta"
#define gevFDOpt            "FDOpt"
#define gevAlgFileType      "AlgFileType"
#define gevGamsVersion      "GamsVersion"
#define gevGenSolver        "GenSolver"
#define gevCurSolver        "CurSolver"
#define gevThreadsRaw       "ThreadsRaw"
#define gevUseCutOff        "UseCutOff"
#define gevUseCheat         "UseCheat"
#define gevNameGamsDate     "NameGamsDate"
#define gevNameGamsTime     "NameGamsTime"
#define gevLicense1         "License1"
#define gevLicense2         "License2"
#define gevLicense3         "License3"
#define gevLicense4         "License4"
#define gevLicense5         "License5"
#define gevLicense6         "License6"
#define gevNameParams       "NameParams"
#define gevNameScenFile     "NameScenFile"
#define gevNameExtFFile     "NameExtFFile"
#define gevisDefaultLicense "isDefaultLicense"
#define gevisDefaultSubsys  "isDefaultSubsys"
#define gevCheat            "Cheat"
#define gevCutOff           "CutOff"
#define gevReal1            "Real1"
#define gevReal2            "Real2"
#define gevReal3            "Real3"
#define gevReal4            "Real4"
#define gevReal5            "Real5"
#define gevReform           "Reform"
#define gevTryInt           "TryInt"

#if defined(_WIN32)
# define GEV_CALLCONV __stdcall
#else
# define GEV_CALLCONV
#endif

#if defined(__cplusplus)
extern "C" {
#endif

struct gevRec;
typedef struct gevRec *gevHandle_t;

typedef int (*gevErrorCallback_t) (int ErrCount, const char *msg);

/* headers for "wrapper" routines implemented in C */
/** gevGetReady: load library
 *  @return false on failure to load library, true on success 
 */
int gevGetReady  (char *msgBuf, int msgBufLen);

/** gevGetReadyD: load library from the speicified directory
 * @return false on failure to load library, true on success 
 */
int gevGetReadyD (const char *dirName, char *msgBuf, int msgBufLen);

/** gevGetReadyL: load library from the specified library name
 *  @return false on failure to load library, true on success 
 */
int gevGetReadyL (const char *libName, char *msgBuf, int msgBufLen);

/** gevCreate: load library and create gev object handle 
 *  @return false on failure to load library, true on success 
 */
int gevCreate    (gevHandle_t *pgev, char *msgBuf, int msgBufLen);

/** gevCreateD: load library from the specified directory and create gev object handle
 * @return false on failure to load library, true on success 
 */
int gevCreateD   (gevHandle_t *pgev, const char *dirName, char *msgBuf, int msgBufLen);

/** gevCreateDD: load library from the specified directory and create gev object handle
 * @return false on failure to load library, true on success 
 */
int gevCreateDD  (gevHandle_t *pgev, const char *dirName, char *msgBuf, int msgBufLen);

/** gevCreate: load library from the specified library name and create gev object handle
 * @return false on failure to load library, true on success 
 */
int gevCreateL   (gevHandle_t *pgev, const char *libName, char *msgBuf, int msgBufLen);

/** gevFree: free gev object handle 
 * @return false on failure, true on success 
 */
int gevFree      (gevHandle_t *pgev);

/** Check if library has been loaded
 * @return false on failure, true on success 
 */
int gevLibraryLoaded(void);

/** Check if library has been unloaded
 * @return false on failure, true on success 
 */
int gevLibraryUnload(void);

/** Check if API and library have the same version, Library needs to be initialized before calling this.
 * @return true  (1) on success,
 *         false (0) on failure.
 */
int  gevCorrectLibraryVersion(char *msgBuf, int msgBufLen);

int  gevGetScreenIndicator   (void);
void gevSetScreenIndicator   (int scrind);
int  gevGetExceptionIndicator(void);
void gevSetExceptionIndicator(int excind);
int  gevGetExitIndicator     (void);
void gevSetExitIndicator     (int extind);
gevErrorCallback_t gevGetErrorCallback(void);
void gevSetErrorCallback(gevErrorCallback_t func);
int  gevGetAPIErrorCount     (void);
void gevSetAPIErrorCount     (int ecnt);

void gevErrorHandling(const char *msg);
void gevInitMutexes(void);
void gevFiniMutexes(void);

#if defined(GEV_MAIN)    /* we must define some things only once */
# define GEV_FUNCPTR(NAME)  NAME##_t NAME = NULL
#else
# define GEV_FUNCPTR(NAME)  extern NAME##_t NAME
#endif

/* function typedefs and pointer definitions */
typedef void (GEV_CALLCONV *Tgevlswrite_t) (const char *msg, int mode, void *usrmem);

/* Prototypes for Dummy Functions */
void  GEV_CALLCONV d_gevRegisterWriteCallback (gevHandle_t pgev, Tgevlswrite_t lsw, int logenabled, void *usrmem);
void  GEV_CALLCONV d_gevCompleteEnvironment (gevHandle_t pgev, void *palg, void *ivec, void *rvec, void *svec);
int  GEV_CALLCONV d_gevInitEnvironmentLegacy (gevHandle_t pgev, const char *cntrfn);
int  GEV_CALLCONV d_gevSwitchLogStat (gevHandle_t pgev, int lo, const char *logfn, int logappend, const char *statfn, int statappend, Tgevlswrite_t lsw, void *usrmem, void **lshandle);
int  GEV_CALLCONV d_gevSwitchLogStatEx (gevHandle_t pgev, int lo, const char *logfn, int logappend, const char *statfn, int statappend, Tgevlswrite_t lsw, void *usrmem, void **lshandle, int doStack);
void * GEV_CALLCONV d_gevGetLShandle (gevHandle_t pgev);
int  GEV_CALLCONV d_gevRestoreLogStat (gevHandle_t pgev, void **lshandle);
int  GEV_CALLCONV d_gevRestoreLogStatRewrite (gevHandle_t pgev, void **lshandle);
void  GEV_CALLCONV d_gevLog (gevHandle_t pgev, const char *s);
void  GEV_CALLCONV d_gevLogPChar (gevHandle_t pgev, const char *p);
void  GEV_CALLCONV d_gevStat (gevHandle_t pgev, const char *s);
void  GEV_CALLCONV d_gevStatC (gevHandle_t pgev, const char *s);
void  GEV_CALLCONV d_gevStatPChar (gevHandle_t pgev, const char *p);
void  GEV_CALLCONV d_gevStatAudit (gevHandle_t pgev, const char *s);
void  GEV_CALLCONV d_gevStatCon (gevHandle_t pgev);
void  GEV_CALLCONV d_gevStatCoff (gevHandle_t pgev);
void  GEV_CALLCONV d_gevStatEOF (gevHandle_t pgev);
void  GEV_CALLCONV d_gevStatSysout (gevHandle_t pgev);
void  GEV_CALLCONV d_gevStatAddE (gevHandle_t pgev, int mi, const char *s);
void  GEV_CALLCONV d_gevStatAddV (gevHandle_t pgev, int mj, const char *s);
void  GEV_CALLCONV d_gevStatAddJ (gevHandle_t pgev, int mi, int mj, const char *s);
void  GEV_CALLCONV d_gevStatEject (gevHandle_t pgev);
void  GEV_CALLCONV d_gevStatEdit (gevHandle_t pgev, const char c);
void  GEV_CALLCONV d_gevStatE (gevHandle_t pgev, const char *s, int mi, const char *s2);
void  GEV_CALLCONV d_gevStatV (gevHandle_t pgev, const char *s, int mj, const char *s2);
void  GEV_CALLCONV d_gevStatT (gevHandle_t pgev);
void  GEV_CALLCONV d_gevStatA (gevHandle_t pgev, const char *s);
void  GEV_CALLCONV d_gevStatB (gevHandle_t pgev, const char *s);
void  GEV_CALLCONV d_gevLogStat (gevHandle_t pgev, const char *s);
void  GEV_CALLCONV d_gevLogStatNoC (gevHandle_t pgev, const char *s);
void  GEV_CALLCONV d_gevLogStatPChar (gevHandle_t pgev, const char *p);
void  GEV_CALLCONV d_gevLogStatFlush (gevHandle_t pgev);
char * GEV_CALLCONV d_gevGetAnchor (gevHandle_t pgev, const char *s, char *buf);
void  GEV_CALLCONV d_gevLSTAnchor (gevHandle_t pgev, const char *s);
int  GEV_CALLCONV d_gevStatAppend (gevHandle_t pgev, const char *statfn, char *msg);
void  GEV_CALLCONV d_gevMIPReport (gevHandle_t pgev, void *gmoptr, double fixobj, int fixiter, double agap, double rgap);
int  GEV_CALLCONV d_gevGetSlvExeInfo (gevHandle_t pgev, const char *solvername, char *exename);
int  GEV_CALLCONV d_gevGetSlvLibInfo (gevHandle_t pgev, const char *solvername, char *libname, char *prefix, int *ifversion);
int  GEV_CALLCONV d_gevCapabilityCheck (gevHandle_t pgev, int modeltype, const char *solvername, int *capable);
int  GEV_CALLCONV d_gevSolverVisibility (gevHandle_t pgev, const char *solvername, int *hidden, int *defaultok);
int  GEV_CALLCONV d_gevNumSolvers (gevHandle_t pgev);
char * GEV_CALLCONV d_gevGetSolver (gevHandle_t pgev, int modeltype, char *buf);
char * GEV_CALLCONV d_gevGetCurrentSolver (gevHandle_t pgev, void *gmoptr, char *buf);
char * GEV_CALLCONV d_gevGetSolverDefault (gevHandle_t pgev, int modeltype, char *buf);
int  GEV_CALLCONV d_gevSolver2Id (gevHandle_t pgev, const char *solvername);
char * GEV_CALLCONV d_gevId2Solver (gevHandle_t pgev, int solverid, char *buf);
char * GEV_CALLCONV d_gevCallSolverNextGridDir (gevHandle_t pgev, char *buf);
int  GEV_CALLCONV d_gevCallSolver (gevHandle_t pgev, void *gmoptr, const char *cntrfn, const char *solvername, int solvelink, int Logging, const char *logfn, const char *statfn, double reslim, int iterlim, int domlim, double optcr, double optca, void **jobhandle, char *msg);
int  GEV_CALLCONV d_gevCallSolverHandleStatus (gevHandle_t pgev, void *jobhandle);
int  GEV_CALLCONV d_gevCallSolverHandleDelete (gevHandle_t pgev, void **jobhandle);
int  GEV_CALLCONV d_gevCallSolverHandleCollect (gevHandle_t pgev, void **jobhandle, void *gmoptr);
int  GEV_CALLCONV d_gevGetIntOpt (gevHandle_t pgev, const char *optname);
double  GEV_CALLCONV d_gevGetDblOpt (gevHandle_t pgev, const char *optname);
char * GEV_CALLCONV d_gevGetStrOpt (gevHandle_t pgev, const char *optname, char *buf);
void  GEV_CALLCONV d_gevSetIntOpt (gevHandle_t pgev, const char *optname, int ival);
void  GEV_CALLCONV d_gevSetDblOpt (gevHandle_t pgev, const char *optname, double rval);
void  GEV_CALLCONV d_gevSetStrOpt (gevHandle_t pgev, const char *optname, const char *sval);
void  GEV_CALLCONV d_gevSynchronizeOpt (gevHandle_t pgev, void *optptr);
double  GEV_CALLCONV d_gevTimeJNow (gevHandle_t pgev);
double  GEV_CALLCONV d_gevTimeDiff (gevHandle_t pgev);
double  GEV_CALLCONV d_gevTimeDiffStart (gevHandle_t pgev);
void  GEV_CALLCONV d_gevTimeSetStart (gevHandle_t pgev);
void  GEV_CALLCONV d_gevTerminateUninstall (gevHandle_t pgev);
void  GEV_CALLCONV d_gevTerminateInstall (gevHandle_t pgev);
void  GEV_CALLCONV d_gevTerminateSet (gevHandle_t pgev, void *intr, void *ehdler);
int  GEV_CALLCONV d_gevTerminateGet (gevHandle_t pgev);
void  GEV_CALLCONV d_gevTerminateClear (gevHandle_t pgev);
void  GEV_CALLCONV d_gevTerminateRaise (gevHandle_t pgev);
void  GEV_CALLCONV d_gevTerminateGetHandler (gevHandle_t pgev, void **intr, void **ehdler);
char * GEV_CALLCONV d_gevGetScratchName (gevHandle_t pgev, const char *s, char *buf);
int  GEV_CALLCONV d_gevWriteModelInstance (gevHandle_t pgev, const char *mifn, void *gmoptr, int *nlcodelen);
int  GEV_CALLCONV d_gevDuplicateScratchDir (gevHandle_t pgev, const char *scrdir, const char *logfn, char *cntrfn);
int  GEV_CALLCONV d_gevInitJacLegacy (gevHandle_t pgev, void **evalptr, void *gmoptr);
void  GEV_CALLCONV d_gevSetColRowPermLegacy (gevHandle_t pgev, void *evalptr, int n, int cgms2slv[], int m, int rgms2slv[]);
void  GEV_CALLCONV d_gevSetJacPermLegacy (gevHandle_t pgev, void *evalptr, int njacs, int jacs[], int jgms2slv[]);
int  GEV_CALLCONV d_gevEvalNewPointLegacy (gevHandle_t pgev, void *evalptr, double x[]);
int  GEV_CALLCONV d_gevEvalJacLegacy (gevHandle_t pgev, void *evalptr, int si, double x[], double *f, double jac[], int *domviol, int *njacsupd);
int  GEV_CALLCONV d_gevEvalJacLegacyX (gevHandle_t pgev, void *evalptr, int cnt, int rowidx[], double x[], double fvec[], double jac[], int *domviol, int *njacsupd);
int  GEV_CALLCONV d_gevNextNLLegacy (gevHandle_t pgev, void *evalptr, int si);
int  GEV_CALLCONV d_gevRowGms2SlvLegacy (gevHandle_t pgev, void *evalptr, int si);
void  GEV_CALLCONV d_gevFreeJacLegacy (gevHandle_t pgev, void **evalptr);
void * GEV_CALLCONV d_gevGetALGX (gevHandle_t pgev);
void GEV_CALLCONV d_gevSkipIOLegacySet (gevHandle_t pgev, const int x);
int  GEV_CALLCONV d_gevThreads (gevHandle_t pgev);
double  GEV_CALLCONV d_gevNSolves (gevHandle_t pgev);


typedef void  (GEV_CALLCONV *gevRegisterWriteCallback_t) (gevHandle_t pgev, Tgevlswrite_t lsw, int logenabled, void *usrmem);
/** Register callback for log and status streams
 *
 * @param pgev gev object handle
 * @param lsw Pointer to callback for log and status streams
 * @param logenabled Flag to enable log or not
 * @param usrmem User memory
 */
GEV_FUNCPTR(gevRegisterWriteCallback);

typedef void  (GEV_CALLCONV *gevCompleteEnvironment_t) (gevHandle_t pgev, void *palg, void *ivec, void *rvec, void *svec);
/** Complete initialization of environment
 *
 * @param pgev gev object handle
 * @param palg Pointer to ALGX structure (GAMS Internal)
 * @param ivec Array of integer options
 * @param rvec Array of real/double options
 * @param svec Array of string options
 */
GEV_FUNCPTR(gevCompleteEnvironment);

typedef int  (GEV_CALLCONV *gevInitEnvironmentLegacy_t) (gevHandle_t pgev, const char *cntrfn);
/** Initialization in legacy mode (from control file)
 *
 * @param pgev gev object handle
 * @param cntrfn Name of control file
 */
GEV_FUNCPTR(gevInitEnvironmentLegacy);

typedef int  (GEV_CALLCONV *gevSwitchLogStat_t) (gevHandle_t pgev, int lo, const char *logfn, int logappend, const char *statfn, int statappend, Tgevlswrite_t lsw, void *usrmem, void **lshandle);
/** Switch log and status streams to another file or callback
 *
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
GEV_FUNCPTR(gevSwitchLogStat);

typedef int  (GEV_CALLCONV *gevSwitchLogStatEx_t) (gevHandle_t pgev, int lo, const char *logfn, int logappend, const char *statfn, int statappend, Tgevlswrite_t lsw, void *usrmem, void **lshandle, int doStack);
/** Switch log and status streams to another file or callback
 *
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
GEV_FUNCPTR(gevSwitchLogStatEx);

typedef void * (GEV_CALLCONV *gevGetLShandle_t) (gevHandle_t pgev);
/** Returns handle to last log and status stream stored by gevSwitchLogStat (Workaround for problem with vptr in Python)
 *
 * @param pgev gev object handle
 */
GEV_FUNCPTR(gevGetLShandle);

typedef int  (GEV_CALLCONV *gevRestoreLogStat_t) (gevHandle_t pgev, void **lshandle);
/** Restore log status stream settings
 *
 * @param pgev gev object handle
 * @param lshandle Log and status handle for later restoring
 */
GEV_FUNCPTR(gevRestoreLogStat);

typedef int  (GEV_CALLCONV *gevRestoreLogStatRewrite_t) (gevHandle_t pgev, void **lshandle);
/** Restore log status stream settings but never append to former log
 *
 * @param pgev gev object handle
 * @param lshandle Log and status handle for later restoring
 */
GEV_FUNCPTR(gevRestoreLogStatRewrite);

typedef void  (GEV_CALLCONV *gevLog_t) (gevHandle_t pgev, const char *s);
/** Send string to log stream
 *
 * @param pgev gev object handle
 * @param s String
 */
GEV_FUNCPTR(gevLog);

typedef void  (GEV_CALLCONV *gevLogPChar_t) (gevHandle_t pgev, const char *p);
/** Send PChar to log stream, no newline added
 *
 * @param pgev gev object handle
 * @param p Pointer to array of characters
 */
GEV_FUNCPTR(gevLogPChar);

typedef void  (GEV_CALLCONV *gevStat_t) (gevHandle_t pgev, const char *s);
/** Send string to status stream
 *
 * @param pgev gev object handle
 * @param s String
 */
GEV_FUNCPTR(gevStat);

typedef void  (GEV_CALLCONV *gevStatC_t) (gevHandle_t pgev, const char *s);
/** Send string to status and copy to listing file
 *
 * @param pgev gev object handle
 * @param s String
 */
GEV_FUNCPTR(gevStatC);

typedef void  (GEV_CALLCONV *gevStatPChar_t) (gevHandle_t pgev, const char *p);
/** Send PChar to status stream, no newline added
 *
 * @param pgev gev object handle
 * @param p Pointer to array of characters
 */
GEV_FUNCPTR(gevStatPChar);

typedef void  (GEV_CALLCONV *gevStatAudit_t) (gevHandle_t pgev, const char *s);
/** GAMS internal status stream operation {=0}
 *
 * @param pgev gev object handle
 * @param s String
 */
GEV_FUNCPTR(gevStatAudit);

typedef void  (GEV_CALLCONV *gevStatCon_t) (gevHandle_t pgev);
/** GAMS internal status stream operation {=1}
 *
 * @param pgev gev object handle
 */
GEV_FUNCPTR(gevStatCon);

typedef void  (GEV_CALLCONV *gevStatCoff_t) (gevHandle_t pgev);
/** GAMS internal status stream operation {=2}
 *
 * @param pgev gev object handle
 */
GEV_FUNCPTR(gevStatCoff);

typedef void  (GEV_CALLCONV *gevStatEOF_t) (gevHandle_t pgev);
/** GAMS internal status stream operation {=3}
 *
 * @param pgev gev object handle
 */
GEV_FUNCPTR(gevStatEOF);

typedef void  (GEV_CALLCONV *gevStatSysout_t) (gevHandle_t pgev);
/** GAMS internal status stream operation {=4}
 *
 * @param pgev gev object handle
 */
GEV_FUNCPTR(gevStatSysout);

typedef void  (GEV_CALLCONV *gevStatAddE_t) (gevHandle_t pgev, int mi, const char *s);
/** GAMS internal status stream operation {=5}
 *
 * @param pgev gev object handle
 * @param mi Index or constraint
 * @param s String
 */
GEV_FUNCPTR(gevStatAddE);

typedef void  (GEV_CALLCONV *gevStatAddV_t) (gevHandle_t pgev, int mj, const char *s);
/** GAMS internal status stream operation {=6}
 *
 * @param pgev gev object handle
 * @param mj Index or variable
 * @param s String
 */
GEV_FUNCPTR(gevStatAddV);

typedef void  (GEV_CALLCONV *gevStatAddJ_t) (gevHandle_t pgev, int mi, int mj, const char *s);
/** GAMS internal status stream operation {=7}
 *
 * @param pgev gev object handle
 * @param mi Index or constraint
 * @param mj Index or variable
 * @param s String
 */
GEV_FUNCPTR(gevStatAddJ);

typedef void  (GEV_CALLCONV *gevStatEject_t) (gevHandle_t pgev);
/** GAMS internal status stream operation {=8}
 *
 * @param pgev gev object handle
 */
GEV_FUNCPTR(gevStatEject);

typedef void  (GEV_CALLCONV *gevStatEdit_t) (gevHandle_t pgev, const char c);
/** GAMS internal status stream operation {=9}
 *
 * @param pgev gev object handle
 * @param c Character
 */
GEV_FUNCPTR(gevStatEdit);

typedef void  (GEV_CALLCONV *gevStatE_t) (gevHandle_t pgev, const char *s, int mi, const char *s2);
/** GAMS internal status stream operation {=E}
 *
 * @param pgev gev object handle
 * @param s String
 * @param mi Index or constraint
 * @param s2 String
 */
GEV_FUNCPTR(gevStatE);

typedef void  (GEV_CALLCONV *gevStatV_t) (gevHandle_t pgev, const char *s, int mj, const char *s2);
/** GAMS internal status stream operation {=V}
 *
 * @param pgev gev object handle
 * @param s String
 * @param mj Index or variable
 * @param s2 String
 */
GEV_FUNCPTR(gevStatV);

typedef void  (GEV_CALLCONV *gevStatT_t) (gevHandle_t pgev);
/** GAMS internal status stream operation {=T}
 *
 * @param pgev gev object handle
 */
GEV_FUNCPTR(gevStatT);

typedef void  (GEV_CALLCONV *gevStatA_t) (gevHandle_t pgev, const char *s);
/** GAMS internal status stream operation {=A}
 *
 * @param pgev gev object handle
 * @param s String
 */
GEV_FUNCPTR(gevStatA);

typedef void  (GEV_CALLCONV *gevStatB_t) (gevHandle_t pgev, const char *s);
/** GAMS internal status stream operation {=B}
 *
 * @param pgev gev object handle
 * @param s String
 */
GEV_FUNCPTR(gevStatB);

typedef void  (GEV_CALLCONV *gevLogStat_t) (gevHandle_t pgev, const char *s);
/** Send string to log and status streams and copy to listing file
 *
 * @param pgev gev object handle
 * @param s String
 */
GEV_FUNCPTR(gevLogStat);

typedef void  (GEV_CALLCONV *gevLogStatNoC_t) (gevHandle_t pgev, const char *s);
/** Send string to log and status streams
 *
 * @param pgev gev object handle
 * @param s String
 */
GEV_FUNCPTR(gevLogStatNoC);

typedef void  (GEV_CALLCONV *gevLogStatPChar_t) (gevHandle_t pgev, const char *p);
/** Send string to log and status streams, no newline added
 *
 * @param pgev gev object handle
 * @param p Pointer to array of characters
 */
GEV_FUNCPTR(gevLogStatPChar);

typedef void  (GEV_CALLCONV *gevLogStatFlush_t) (gevHandle_t pgev);
/** Flush status streams (does not work with callback)
 *
 * @param pgev gev object handle
 */
GEV_FUNCPTR(gevLogStatFlush);

typedef char * (GEV_CALLCONV *gevGetAnchor_t) (gevHandle_t pgev, const char *s, char *buf);
/** Get anchor line for log (points to file and is clickable in GAMS IDE)
 *
 * @param pgev gev object handle
 * @param s String
 */
GEV_FUNCPTR(gevGetAnchor);

typedef void  (GEV_CALLCONV *gevLSTAnchor_t) (gevHandle_t pgev, const char *s);
/** Put a line to log that points to the current lst line"
 *
 * @param pgev gev object handle
 * @param s String
 */
GEV_FUNCPTR(gevLSTAnchor);

typedef int  (GEV_CALLCONV *gevStatAppend_t) (gevHandle_t pgev, const char *statfn, char *msg);
/** Append status file to current status file
 *
 * @param pgev gev object handle
 * @param statfn Status file name
 * @param msg Message
 */
GEV_FUNCPTR(gevStatAppend);

typedef void  (GEV_CALLCONV *gevMIPReport_t) (gevHandle_t pgev, void *gmoptr, double fixobj, int fixiter, double agap, double rgap);
/** Print MIP report to log and lst
 *
 * @param pgev gev object handle
 * @param gmoptr Pointer to GAMS modeling object
 * @param fixobj 
 * @param fixiter 
 * @param agap 
 * @param rgap 
 */
GEV_FUNCPTR(gevMIPReport);

typedef int  (GEV_CALLCONV *gevGetSlvExeInfo_t) (gevHandle_t pgev, const char *solvername, char *exename);
/** Name of solver executable
 *
 * @param pgev gev object handle
 * @param solvername Name of solver
 * @param exename Name of solver executable
 */
GEV_FUNCPTR(gevGetSlvExeInfo);

typedef int  (GEV_CALLCONV *gevGetSlvLibInfo_t) (gevHandle_t pgev, const char *solvername, char *libname, char *prefix, int *ifversion);
/** Solver library name, prefix, and API version
 *
 * @param pgev gev object handle
 * @param solvername Name of solver
 * @param libname Name of solver library
 * @param prefix Prefix of solver
 * @param ifversion Version of solver interface
 */
GEV_FUNCPTR(gevGetSlvLibInfo);

typedef int  (GEV_CALLCONV *gevCapabilityCheck_t) (gevHandle_t pgev, int modeltype, const char *solvername, int *capable);
/** Check if solver is capable to handle model type given
 *
 * @param pgev gev object handle
 * @param modeltype Modeltype
 * @param solvername Name of solver
 * @param capable Flag whether solver is capable or not
 */
GEV_FUNCPTR(gevCapabilityCheck);

typedef int  (GEV_CALLCONV *gevSolverVisibility_t) (gevHandle_t pgev, const char *solvername, int *hidden, int *defaultok);
/** Provide information if solver is hidden
 *
 * @param pgev gev object handle
 * @param solvername Name of solver
 * @param hidden 
 * @param defaultok 
 */
GEV_FUNCPTR(gevSolverVisibility);

typedef int  (GEV_CALLCONV *gevNumSolvers_t) (gevHandle_t pgev);
/** Number of solvers in the system
 *
 * @param pgev gev object handle
 */
GEV_FUNCPTR(gevNumSolvers);

typedef char * (GEV_CALLCONV *gevGetSolver_t) (gevHandle_t pgev, int modeltype, char *buf);
/** Name of the solver chosen for modeltype (if non is chosen, it is the default)
 *
 * @param pgev gev object handle
 * @param modeltype Modeltype
 */
GEV_FUNCPTR(gevGetSolver);

typedef char * (GEV_CALLCONV *gevGetCurrentSolver_t) (gevHandle_t pgev, void *gmoptr, char *buf);
/** Name of the select solver
 *
 * @param pgev gev object handle
 * @param gmoptr Pointer to GAMS modeling object
 */
GEV_FUNCPTR(gevGetCurrentSolver);

typedef char * (GEV_CALLCONV *gevGetSolverDefault_t) (gevHandle_t pgev, int modeltype, char *buf);
/** Name of the default solver for modeltype
 *
 * @param pgev gev object handle
 * @param modeltype Modeltype
 */
GEV_FUNCPTR(gevGetSolverDefault);

typedef int  (GEV_CALLCONV *gevSolver2Id_t) (gevHandle_t pgev, const char *solvername);
/** Internal ID of solver, 0 for failure
 *
 * @param pgev gev object handle
 * @param solvername Name of solver
 */
GEV_FUNCPTR(gevSolver2Id);

typedef char * (GEV_CALLCONV *gevId2Solver_t) (gevHandle_t pgev, int solverid, char *buf);
/** Solver name
 *
 * @param pgev gev object handle
 * @param solverid Internal ID of solver
 */
GEV_FUNCPTR(gevId2Solver);

typedef char * (GEV_CALLCONV *gevCallSolverNextGridDir_t) (gevHandle_t pgev, char *buf);
/** Creates grid directory for next gevCallSolver call and returns name (if called with gevSolveLinkAsyncGrid or gevSolveLinkAsyncSimulate)
 *
 * @param pgev gev object handle
 */
GEV_FUNCPTR(gevCallSolverNextGridDir);

typedef int  (GEV_CALLCONV *gevCallSolver_t) (gevHandle_t pgev, void *gmoptr, const char *cntrfn, const char *solvername, int solvelink, int Logging, const char *logfn, const char *statfn, double reslim, int iterlim, int domlim, double optcr, double optca, void **jobhandle, char *msg);
/** Call GAMS solver on GMO model or control file
 *
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
GEV_FUNCPTR(gevCallSolver);

typedef int  (GEV_CALLCONV *gevCallSolverHandleStatus_t) (gevHandle_t pgev, void *jobhandle);
/** Check status of solver job if called with gevSolveLinkAsyncGrid (0 job is done, 1 unknown job handle, 2 job is running)
 *
 * @param pgev gev object handle
 * @param jobhandle Handle to solver job in case of solvelink=gevSolveLinkAsyncGrid
 */
GEV_FUNCPTR(gevCallSolverHandleStatus);

typedef int  (GEV_CALLCONV *gevCallSolverHandleDelete_t) (gevHandle_t pgev, void **jobhandle);
/** Delete instance of solver job if called with gevSolveLinkAsyncGrid (0 deleted, 1 unknown job handle, 2 deletion failed)
 *
 * @param pgev gev object handle
 * @param jobhandle Handle to solver job in case of solvelink=gevSolveLinkAsyncGrid
 */
GEV_FUNCPTR(gevCallSolverHandleDelete);

typedef int  (GEV_CALLCONV *gevCallSolverHandleCollect_t) (gevHandle_t pgev, void **jobhandle, void *gmoptr);
/** Collect solution from solver job if called with gevSolveLinkAsyncGrid (0 loaded, 1 unknown job handle, 2 job is running, 3 other error), delete instance
 *
 * @param pgev gev object handle
 * @param jobhandle Handle to solver job in case of solvelink=gevSolveLinkAsyncGrid
 * @param gmoptr Pointer to GAMS modeling object
 */
GEV_FUNCPTR(gevCallSolverHandleCollect);

typedef int  (GEV_CALLCONV *gevGetIntOpt_t) (gevHandle_t pgev, const char *optname);
/** Get integer valued option (see enumerated constants)
 *
 * @param pgev gev object handle
 * @param optname Name of option (see enumerated constants)
 */
GEV_FUNCPTR(gevGetIntOpt);

typedef double  (GEV_CALLCONV *gevGetDblOpt_t) (gevHandle_t pgev, const char *optname);
/** Get double valued option (see enumerated constants)
 *
 * @param pgev gev object handle
 * @param optname Name of option (see enumerated constants)
 */
GEV_FUNCPTR(gevGetDblOpt);

typedef char * (GEV_CALLCONV *gevGetStrOpt_t) (gevHandle_t pgev, const char *optname, char *buf);
/** Get string valued option (see enumerated constants)
 *
 * @param pgev gev object handle
 * @param optname Name of option (see enumerated constants)
 */
GEV_FUNCPTR(gevGetStrOpt);

typedef void  (GEV_CALLCONV *gevSetIntOpt_t) (gevHandle_t pgev, const char *optname, int ival);
/** Set integer valued option (see enumerated constants)
 *
 * @param pgev gev object handle
 * @param optname Name of option (see enumerated constants)
 * @param ival Integer value
 */
GEV_FUNCPTR(gevSetIntOpt);

typedef void  (GEV_CALLCONV *gevSetDblOpt_t) (gevHandle_t pgev, const char *optname, double rval);
/** Set double valued option (see enumerated constants)
 *
 * @param pgev gev object handle
 * @param optname Name of option (see enumerated constants)
 * @param rval Real/Double value
 */
GEV_FUNCPTR(gevSetDblOpt);

typedef void  (GEV_CALLCONV *gevSetStrOpt_t) (gevHandle_t pgev, const char *optname, const char *sval);
/** Set string valued option (see enumerated constants)
 *
 * @param pgev gev object handle
 * @param optname Name of option (see enumerated constants)
 * @param sval String value
 */
GEV_FUNCPTR(gevSetStrOpt);

typedef void  (GEV_CALLCONV *gevSynchronizeOpt_t) (gevHandle_t pgev, void *optptr);
/** Copy environment options to passed in option object
 *
 * @param pgev gev object handle
 * @param optptr Pointer to option object
 */
GEV_FUNCPTR(gevSynchronizeOpt);

typedef double  (GEV_CALLCONV *gevTimeJNow_t) (gevHandle_t pgev);
/** GAMS Julian time
 *
 * @param pgev gev object handle
 */
GEV_FUNCPTR(gevTimeJNow);

typedef double  (GEV_CALLCONV *gevTimeDiff_t) (gevHandle_t pgev);
/** Time difference in seconds since creation or last call to gevTimeDiff
 *
 * @param pgev gev object handle
 */
GEV_FUNCPTR(gevTimeDiff);

typedef double  (GEV_CALLCONV *gevTimeDiffStart_t) (gevHandle_t pgev);
/** Time difference in seconds since creation of object
 *
 * @param pgev gev object handle
 */
GEV_FUNCPTR(gevTimeDiffStart);

typedef void  (GEV_CALLCONV *gevTimeSetStart_t) (gevHandle_t pgev);
/** Reset timer (overwrites time stamp from creation)
 *
 * @param pgev gev object handle
 */
GEV_FUNCPTR(gevTimeSetStart);

typedef void  (GEV_CALLCONV *gevTerminateUninstall_t) (gevHandle_t pgev);
/** Uninstalls an already registered interrupt handler
 *
 * @param pgev gev object handle
 */
GEV_FUNCPTR(gevTerminateUninstall);

typedef void  (GEV_CALLCONV *gevTerminateInstall_t) (gevHandle_t pgev);
/** Installs an already registered interrupt handler
 *
 * @param pgev gev object handle
 */
GEV_FUNCPTR(gevTerminateInstall);

typedef void  (GEV_CALLCONV *gevTerminateSet_t) (gevHandle_t pgev, void *intr, void *ehdler);
/** Register a pointer to some memory that will indicate an interrupt and the pointer to a interrupt handler and installs it
 *
 * @param pgev gev object handle
 * @param intr Pointer to some memory indicating an interrupt
 * @param ehdler Pointer to interrupt handler
 */
GEV_FUNCPTR(gevTerminateSet);

typedef int  (GEV_CALLCONV *gevTerminateGet_t) (gevHandle_t pgev);
/** Check if one should interrupt
 *
 * @param pgev gev object handle
 */
GEV_FUNCPTR(gevTerminateGet);

typedef void  (GEV_CALLCONV *gevTerminateClear_t) (gevHandle_t pgev);
/** Resets the interrupt counter
 *
 * @param pgev gev object handle
 */
GEV_FUNCPTR(gevTerminateClear);

typedef void  (GEV_CALLCONV *gevTerminateRaise_t) (gevHandle_t pgev);
/** Increases the interrupt counter
 *
 * @param pgev gev object handle
 */
GEV_FUNCPTR(gevTerminateRaise);

typedef void  (GEV_CALLCONV *gevTerminateGetHandler_t) (gevHandle_t pgev, void **intr, void **ehdler);
/** Get installed termination handler
 *
 * @param pgev gev object handle
 * @param intr Pointer to some memory indicating an interrupt
 * @param ehdler Pointer to interrupt handler
 */
GEV_FUNCPTR(gevTerminateGetHandler);

typedef char * (GEV_CALLCONV *gevGetScratchName_t) (gevHandle_t pgev, const char *s, char *buf);
/** Get scratch file name plus scratch extension including path of scratch directory
 *
 * @param pgev gev object handle
 * @param s String
 */
GEV_FUNCPTR(gevGetScratchName);

typedef int  (GEV_CALLCONV *gevWriteModelInstance_t) (gevHandle_t pgev, const char *mifn, void *gmoptr, int *nlcodelen);
/** Creates model instance file
 *
 * @param pgev gev object handle
 * @param mifn Model instance file name
 * @param gmoptr Pointer to GAMS modeling object
 * @param nlcodelen Length of nonlinear code
 */
GEV_FUNCPTR(gevWriteModelInstance);

typedef int  (GEV_CALLCONV *gevDuplicateScratchDir_t) (gevHandle_t pgev, const char *scrdir, const char *logfn, char *cntrfn);
/** Duplicates a scratch directory and points to read only files in source scratch directory
 *
 * @param pgev gev object handle
 * @param scrdir Scratch directory
 * @param logfn Log file name
 * @param cntrfn Name of control file
 */
GEV_FUNCPTR(gevDuplicateScratchDir);

typedef int  (GEV_CALLCONV *gevInitJacLegacy_t) (gevHandle_t pgev, void **evalptr, void *gmoptr);
/** Legacy Jacobian Evaluation: Initialize row wise Jacobian structure
 *
 * @param pgev gev object handle
 * @param evalptr Pointer to structure for legacy Jacobian evaluation
 * @param gmoptr Pointer to GAMS modeling object
 */
GEV_FUNCPTR(gevInitJacLegacy);

typedef void  (GEV_CALLCONV *gevSetColRowPermLegacy_t) (gevHandle_t pgev, void *evalptr, int n, int cgms2slv[], int m, int rgms2slv[]);
/** Legacy Jacobian Evaluation: Set column and row permutation GAMS to solver
 *
 * @param pgev gev object handle
 * @param evalptr Pointer to structure for legacy Jacobian evaluation
 * @param n Number of variables
 * @param cgms2slv GAMS to solver permutation of columns
 * @param m Number of constraints
 * @param rgms2slv GAMS to solver permutation of rows
 */
GEV_FUNCPTR(gevSetColRowPermLegacy);

typedef void  (GEV_CALLCONV *gevSetJacPermLegacy_t) (gevHandle_t pgev, void *evalptr, int njacs, int jacs[], int jgms2slv[]);
/** Legacy Jacobian Evaluation: Set Jacobian permutation GAMS to solver
 *
 * @param pgev gev object handle
 * @param evalptr Pointer to structure for legacy Jacobian evaluation
 * @param njacs Number of Jacobian elements in jacs and jgms2slv arrays
 * @param jacs Array of original indices of Jacobian elements (1-based), length njacs
 * @param jgms2slv GAMS to solver permutation of Jacobian elements, length njacs
 */
GEV_FUNCPTR(gevSetJacPermLegacy);

typedef int  (GEV_CALLCONV *gevEvalNewPointLegacy_t) (gevHandle_t pgev, void *evalptr, double x[]);
/** Legacy Jacobian Evaluation: Set new point and do point copy magic
 *
 * @param pgev gev object handle
 * @param evalptr Pointer to structure for legacy Jacobian evaluation
 * @param x Input values for variables
 */
GEV_FUNCPTR(gevEvalNewPointLegacy);

typedef int  (GEV_CALLCONV *gevEvalJacLegacy_t) (gevHandle_t pgev, void *evalptr, int si, double x[], double *f, double jac[], int *domviol, int *njacsupd);
/** Legacy Jacobian Evaluation: Evaluate row and store in Jacobian structure
 *
 * @param pgev gev object handle
 * @param evalptr Pointer to structure for legacy Jacobian evaluation
 * @param si Solve index for row i
 * @param x Input values for variables
 * @param f Function value
 * @param jac Array to store the gradients
 * @param domviol Domain violations
 * @param njacsupd Number of Jacobian elements updated
 */
GEV_FUNCPTR(gevEvalJacLegacy);

typedef int  (GEV_CALLCONV *gevEvalJacLegacyX_t) (gevHandle_t pgev, void *evalptr, int cnt, int rowidx[], double x[], double fvec[], double jac[], int *domviol, int *njacsupd);
/** Legacy Jacobian Evaluation: Evaluate set of rows and store in Jacobian structure
 *
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
GEV_FUNCPTR(gevEvalJacLegacyX);

typedef int  (GEV_CALLCONV *gevNextNLLegacy_t) (gevHandle_t pgev, void *evalptr, int si);
/** Legacy Jacobian Evaluation: Provide next nonlinear row, start with M
 *
 * @param pgev gev object handle
 * @param evalptr Pointer to structure for legacy Jacobian evaluation
 * @param si Solve index for row i
 */
GEV_FUNCPTR(gevNextNLLegacy);

typedef int  (GEV_CALLCONV *gevRowGms2SlvLegacy_t) (gevHandle_t pgev, void *evalptr, int si);
/** Legacy Jacobian Evaluation: Provide permuted row index
 *
 * @param pgev gev object handle
 * @param evalptr Pointer to structure for legacy Jacobian evaluation
 * @param si Solve index for row i
 */
GEV_FUNCPTR(gevRowGms2SlvLegacy);

typedef void  (GEV_CALLCONV *gevFreeJacLegacy_t) (gevHandle_t pgev, void **evalptr);
/** Legacy Jacobian Evaluation: Free row wise Jacobian structure
 *
 * @param pgev gev object handle
 * @param evalptr Pointer to structure for legacy Jacobian evaluation
 */
GEV_FUNCPTR(gevFreeJacLegacy);

typedef void * (GEV_CALLCONV *gevGetALGX_t) (gevHandle_t pgev);
/** Pass pointer to ALGX structure
 *
 * @param pgev gev object handle
 */
GEV_FUNCPTR(gevGetALGX);

typedef void (GEV_CALLCONV *gevSkipIOLegacySet_t) (gevHandle_t pgev, const int x);
GEV_FUNCPTR(gevSkipIOLegacySet);

typedef int  (GEV_CALLCONV *gevThreads_t) (gevHandle_t pgev);
/** Number of threads (1..n); if option gevThreadsRaw = 0, this function gives the number of available processors
 *
 * @param pgev gev object handle
 */
GEV_FUNCPTR(gevThreads);

typedef double  (GEV_CALLCONV *gevNSolves_t) (gevHandle_t pgev);
/** Number of solves
 *
 * @param pgev gev object handle
 */
GEV_FUNCPTR(gevNSolves);

#if defined(__cplusplus)
}
#endif
#endif /* #if ! defined(_GEVCC_H_) */

