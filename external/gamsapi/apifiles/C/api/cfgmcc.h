/* cfgmcc.h
 * Header file for C-style interface to the CFG library
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


#if ! defined(_CFGCC_H_)
#     define  _CFGCC_H_

#define CFGAPIVERSION 4

enum cfgProcType {
  cfgProc_none           = 0,
  cfgProc_lp             = 1,
  cfgProc_mip            = 2,
  cfgProc_rmip           = 3,
  cfgProc_nlp            = 4,
  cfgProc_mcp            = 5,
  cfgProc_mpec           = 6,
  cfgProc_rmpec          = 7,
  cfgProc_cns            = 8,
  cfgProc_dnlp           = 9,
  cfgProc_rminlp         = 10,
  cfgProc_minlp          = 11,
  cfgProc_qcp            = 12,
  cfgProc_miqcp          = 13,
  cfgProc_rmiqcp         = 14,
  cfgProc_emp            = 15,
  cfgProc_nrofmodeltypes = 16  };

#if defined(_WIN32)
# define CFG_CALLCONV __stdcall
#else
# define CFG_CALLCONV
#endif

#if defined(__cplusplus)
extern "C" {
#endif

struct cfgRec;
typedef struct cfgRec *cfgHandle_t;

typedef int (*cfgErrorCallback_t) (int ErrCount, const char *msg);

/* headers for "wrapper" routines implemented in C */
/** cfgGetReady: load library
 *  @return false on failure to load library, true on success 
 */
int cfgGetReady  (char *msgBuf, int msgBufLen);

/** cfgGetReadyD: load library from the speicified directory
 * @return false on failure to load library, true on success 
 */
int cfgGetReadyD (const char *dirName, char *msgBuf, int msgBufLen);

/** cfgGetReadyL: load library from the specified library name
 *  @return false on failure to load library, true on success 
 */
int cfgGetReadyL (const char *libName, char *msgBuf, int msgBufLen);

/** cfgCreate: load library and create cfg object handle 
 *  @return false on failure to load library, true on success 
 */
int cfgCreate    (cfgHandle_t *pcfg, char *msgBuf, int msgBufLen);

/** cfgCreateD: load library from the specified directory and create cfg object handle
 * @return false on failure to load library, true on success 
 */
int cfgCreateD   (cfgHandle_t *pcfg, const char *dirName, char *msgBuf, int msgBufLen);

/** cfgCreate: load library from the specified library name and create cfg object handle
 * @return false on failure to load library, true on success 
 */
int cfgCreateL   (cfgHandle_t *pcfg, const char *libName, char *msgBuf, int msgBufLen);

/** cfgFree: free cfg object handle 
 * @return false on failure, true on success 
 */
int cfgFree      (cfgHandle_t *pcfg);

/** Check if library has been loaded
 * @return false on failure, true on success 
 */
int cfgLibraryLoaded(void);

/** Check if library has been unloaded
 * @return false on failure, true on success 
 */
int cfgLibraryUnload(void);

/** Check if API and library have the same version, Library needs to be initialized before calling this.
 * @return true  (1) on success,
 *         false (0) on failure.
 */
int  cfgCorrectLibraryVersion(char *msgBuf, int msgBufLen);

int  cfgGetScreenIndicator   (void);
void cfgSetScreenIndicator   (int scrind);
int  cfgGetExceptionIndicator(void);
void cfgSetExceptionIndicator(int excind);
int  cfgGetExitIndicator     (void);
void cfgSetExitIndicator     (int extind);
cfgErrorCallback_t cfgGetErrorCallback(void);
void cfgSetErrorCallback(cfgErrorCallback_t func);
int  cfgGetAPIErrorCount     (void);
void cfgSetAPIErrorCount     (int ecnt);

void cfgErrorHandling(const char *msg);
void cfgInitMutexes(void);
void cfgFiniMutexes(void);

#if defined(CFG_MAIN)    /* we must define some things only once */
# define CFG_FUNCPTR(NAME)  NAME##_t NAME = NULL
#else
# define CFG_FUNCPTR(NAME)  extern NAME##_t NAME
#endif

/* Prototypes for Dummy Functions */
int  CFG_CALLCONV d_cfgReadConfig (cfgHandle_t pcfg, const char *filename);
int  CFG_CALLCONV d_cfgReadConfigGUC (cfgHandle_t pcfg, const char *filename, const char *sysDir);
int  CFG_CALLCONV d_cfgNumAlgs (cfgHandle_t pcfg);
int  CFG_CALLCONV d_cfgDefaultAlg (cfgHandle_t pcfg, int proc);
char * CFG_CALLCONV d_cfgAlgName (cfgHandle_t pcfg, int alg, char *buf);
char * CFG_CALLCONV d_cfgAlgCode (cfgHandle_t pcfg, int alg, char *buf);
int  CFG_CALLCONV d_cfgAlgHidden (cfgHandle_t pcfg, int alg);
int  CFG_CALLCONV d_cfgAlgAllowsModifyProblem (cfgHandle_t pcfg, int alg);
int  CFG_CALLCONV d_cfgAlgLibInfo (cfgHandle_t pcfg, int alg, char *name, char *prefix);
int  CFG_CALLCONV d_cfgAlgThreadSafeIndic (cfgHandle_t pcfg, int alg);
int  CFG_CALLCONV d_cfgAlgNumber (cfgHandle_t pcfg, const char *id);
int  CFG_CALLCONV d_cfgAlgCapability (cfgHandle_t pcfg, int alg, int proc);
int  CFG_CALLCONV d_cfgAlgCreate (cfgHandle_t pcfg, int alg, void **psl, const char *sysDir, char *msg);
int  CFG_CALLCONV d_cfgAlgReadyAPI (cfgHandle_t pcfg, int alg, void *psl, void *gmo);
int  CFG_CALLCONV d_cfgAlgModifyProblem (cfgHandle_t pcfg, int alg, void *psl);
int  CFG_CALLCONV d_cfgAlgCallSolver (cfgHandle_t pcfg, int alg, void *psl, void *gmo);
void  CFG_CALLCONV d_cfgAlgFree (cfgHandle_t pcfg, int alg, void **vpsl);
int  CFG_CALLCONV d_cfgDefFileName (cfgHandle_t pcfg, const char *id, char *defFileName);
char * CFG_CALLCONV d_cfgModelTypeName (cfgHandle_t pcfg, int proc, char *buf);
int  CFG_CALLCONV d_cfgModelTypeNumber (cfgHandle_t pcfg, const char *id);
int  CFG_CALLCONV d_cfgNumMsg (cfgHandle_t pcfg);
char * CFG_CALLCONV d_cfgGetMsg (cfgHandle_t pcfg, char *buf);


typedef int  (CFG_CALLCONV *cfgReadConfig_t) (cfgHandle_t pcfg, const char *filename);
/** Read GAMS configuration file
 *
 * @param pcfg cfg object handle
 * @param filename Configuration file name
 */
CFG_FUNCPTR(cfgReadConfig);

typedef int  (CFG_CALLCONV *cfgReadConfigGUC_t) (cfgHandle_t pcfg, const char *filename, const char *sysDir);
/** Read GAMS configuration file plus gamsconfig.yaml
 *
 * @param pcfg cfg object handle
 * @param filename Configuration file name
 * @param sysDir GAMS System Directory
 */
CFG_FUNCPTR(cfgReadConfigGUC);

typedef int  (CFG_CALLCONV *cfgNumAlgs_t) (cfgHandle_t pcfg);
/** Number of solvers
 *
 * @param pcfg cfg object handle
 */
CFG_FUNCPTR(cfgNumAlgs);

typedef int  (CFG_CALLCONV *cfgDefaultAlg_t) (cfgHandle_t pcfg, int proc);
/** Number of default solver for model type proc
 *
 * @param pcfg cfg object handle
 * @param proc Model type number
 */
CFG_FUNCPTR(cfgDefaultAlg);

typedef char * (CFG_CALLCONV *cfgAlgName_t) (cfgHandle_t pcfg, int alg, char *buf);
/** Name of solver
 *
 * @param pcfg cfg object handle
 * @param alg Solver number
 */
CFG_FUNCPTR(cfgAlgName);

typedef char * (CFG_CALLCONV *cfgAlgCode_t) (cfgHandle_t pcfg, int alg, char *buf);
/** Code of solver
 *
 * @param pcfg cfg object handle
 * @param alg Solver number
 */
CFG_FUNCPTR(cfgAlgCode);

typedef int  (CFG_CALLCONV *cfgAlgHidden_t) (cfgHandle_t pcfg, int alg);
/** Returns true, if alg should be hidden
 *
 * @param pcfg cfg object handle
 * @param alg Solver number
 */
CFG_FUNCPTR(cfgAlgHidden);

typedef int  (CFG_CALLCONV *cfgAlgAllowsModifyProblem_t) (cfgHandle_t pcfg, int alg);
/** Solver can modify problem
 *
 * @param pcfg cfg object handle
 * @param alg Solver number
 */
CFG_FUNCPTR(cfgAlgAllowsModifyProblem);

typedef int  (CFG_CALLCONV *cfgAlgLibInfo_t) (cfgHandle_t pcfg, int alg, char *name, char *prefix);
/** Get link library info for solver
 *
 * @param pcfg cfg object handle
 * @param alg Solver number
 * @param name 
 * @param prefix 
 */
CFG_FUNCPTR(cfgAlgLibInfo);

typedef int  (CFG_CALLCONV *cfgAlgThreadSafeIndic_t) (cfgHandle_t pcfg, int alg);
/** Get thread safety indicator for solver
 *
 * @param pcfg cfg object handle
 * @param alg Solver number
 */
CFG_FUNCPTR(cfgAlgThreadSafeIndic);

typedef int  (CFG_CALLCONV *cfgAlgNumber_t) (cfgHandle_t pcfg, const char *id);
/** Number of solver
 *
 * @param pcfg cfg object handle
 * @param id Solver name
 */
CFG_FUNCPTR(cfgAlgNumber);

typedef int  (CFG_CALLCONV *cfgAlgCapability_t) (cfgHandle_t pcfg, int alg, int proc);
/** Solver Modeltype capability matrix
 *
 * @param pcfg cfg object handle
 * @param alg Solver number
 * @param proc Model type number
 */
CFG_FUNCPTR(cfgAlgCapability);

typedef int  (CFG_CALLCONV *cfgAlgCreate_t) (cfgHandle_t pcfg, int alg, void **psl, const char *sysDir, char *msg);
/** Create solver link object
 *
 * @param pcfg cfg object handle
 * @param alg Solver number
 * @param psl 
 * @param sysDir GAMS System Directory
 * @param msg 
 */
CFG_FUNCPTR(cfgAlgCreate);

typedef int  (CFG_CALLCONV *cfgAlgReadyAPI_t) (cfgHandle_t pcfg, int alg, void *psl, void *gmo);
/** Call solver readyapi
 *
 * @param pcfg cfg object handle
 * @param alg Solver number
 * @param psl 
 * @param gmo 
 */
CFG_FUNCPTR(cfgAlgReadyAPI);

typedef int  (CFG_CALLCONV *cfgAlgModifyProblem_t) (cfgHandle_t pcfg, int alg, void *psl);
/** Call solver modifyproblem
 *
 * @param pcfg cfg object handle
 * @param alg Solver number
 * @param psl 
 */
CFG_FUNCPTR(cfgAlgModifyProblem);

typedef int  (CFG_CALLCONV *cfgAlgCallSolver_t) (cfgHandle_t pcfg, int alg, void *psl, void *gmo);
/** Call solver modifyproblem
 *
 * @param pcfg cfg object handle
 * @param alg Solver number
 * @param psl 
 * @param gmo 
 */
CFG_FUNCPTR(cfgAlgCallSolver);

typedef void  (CFG_CALLCONV *cfgAlgFree_t) (cfgHandle_t pcfg, int alg, void **vpsl);
/** Call solver modifyproblem
 *
 * @param pcfg cfg object handle
 * @param alg Solver number
 * @param vpsl 
 */
CFG_FUNCPTR(cfgAlgFree);

typedef int  (CFG_CALLCONV *cfgDefFileName_t) (cfgHandle_t pcfg, const char *id, char *defFileName);
/** Gives name of definition file for given solver (Returns true on success, false in case of problem)
 *
 * @param pcfg cfg object handle
 * @param id Solver name
 * @param defFileName Name of definition file
 */
CFG_FUNCPTR(cfgDefFileName);

typedef char * (CFG_CALLCONV *cfgModelTypeName_t) (cfgHandle_t pcfg, int proc, char *buf);
/** Modeltype name
 *
 * @param pcfg cfg object handle
 * @param proc Model type number
 */
CFG_FUNCPTR(cfgModelTypeName);

typedef int  (CFG_CALLCONV *cfgModelTypeNumber_t) (cfgHandle_t pcfg, const char *id);
/** Modeltype number
 *
 * @param pcfg cfg object handle
 * @param id Solver name
 */
CFG_FUNCPTR(cfgModelTypeNumber);

typedef int  (CFG_CALLCONV *cfgNumMsg_t) (cfgHandle_t pcfg);
/** Number of pending messages
 *
 * @param pcfg cfg object handle
 */
CFG_FUNCPTR(cfgNumMsg);

typedef char * (CFG_CALLCONV *cfgGetMsg_t) (cfgHandle_t pcfg, char *buf);
/** Pending messages
 *
 * @param pcfg cfg object handle
 */
CFG_FUNCPTR(cfgGetMsg);

#if defined(__cplusplus)
}
#endif
#endif /* #if ! defined(_CFGCC_H_) */

