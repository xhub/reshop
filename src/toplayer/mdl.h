#ifndef RHP_MDL_H
#define RHP_MDL_H

/** @file mdl.h */

/**
 *  @defgroup publicAPI The public API
 *
 *  The following functions are the public API of RESHOP. It is highly
 *  recommended to only call those, and to not rely on other functions from
 *  RESHOP. The aim of this API is that it is sufficient to define any
 *  optimization problem and solve it.
 *
 *  There is a special attention to the stability of the public API. If there
 *  is a need to use a function that is not part of the public API, feel free
 *  to contact the developers
 */

#include "equvar_helpers.h"
#include "rhp_fwd.h"
#include "container.h"
#include "empinfo.h"
#include "mdl_timings.h"

typedef struct model_ops ModelOps;

typedef enum mdl_status {
   MdlStatusUnset           = 0x0,
   MdlMetaChecked           = 0x1,
   MdlChecked               = 0x2,
   MdlFinalized             = 0x4,
   MdlContainerInstantiable = 0x8,
   MdlInstantiable          = MdlContainerInstantiable,
} MdlStatus;

/* ----------------------------------------------------------------------
 * Problem type is stored in the common part as during the export it might
 * be set before the GMO object is created. Also, the GMO object is really
 * for the container. Having it stored more data makes the conceptual model
 * harder
 * ---------------------------------------------------------------------- */

typedef struct {
   ModelType mdltype;           /**< Model type (in the sense of optimization) */
   bool own_exports_dir_parent; /**< true if has ownership of export_dir     */
   bool delete_exports_dir;     /**< If true, deletes the exports_dir_parent folder */
   char *name;                  /**< Name of the model                       */
   char *exports_dir;           /**< Directory for exports                   */
   char *exports_dir_parent;    /**< Parent of the exports dir               */
} MdlCommonData;

// FIXME Is this used? If yes, then document
typedef union {
   McpInfo mcp;
} MdlDetailedInfo;

/** ReSHOP Model structure */
typedef struct rhp_mdl {
   BackendType backend;         /**< model backend                           */
   MdlStatus status;            /**< various statuses of the model           */
   unsigned id;                 /**< model ID                                */
   unsigned refcnt;             /**< reference counter                       */

   Container ctr;               /**< algebraic container                     */

   EmpInfo empinfo;             /**< the EMP information                     */
   MdlCommonData commondata;    /**< Name of the model                       */
   MdlDetailedInfo info;

   Timings *timings;            /**< Timings                                 */
   const ModelOps *ops;         /**< Backend-specific operations             */
   void *data;                  /**< Backend-specific data                   */
   Model *mdl_up;               /**< Upstream model                          */
} Model;

void mdl_release(Model *mdl);
MALLOC_ATTR(mdl_release, 1)
Model *mdl_new(BackendType backend);
Model *mdl_borrow(Model *mdl);
int mdl_trimmem(Model *mdl) NONNULL;

int mdl_solve(Model *mdl) NONNULL;
int mdl_postprocess(Model *mdl) NONNULL;

int mdl_check(Model *mdl) NONNULL;
int mdl_checkmetadata(Model *mdl) NONNULL;
int mdl_checkobjequvar(const Model *mdl, rhp_idx objvar, rhp_idx objequ) NONNULL;
int mdl_solvable(Model *mdl) NONNULL;

int mdl_finalize(Model *mdl) NONNULL;

int mdl_copysolveoptions(Model *mdl, const Model *mdl_src) NONNULL;
int mdl_copystatsfromsolver(Model *mdl, const Model *mdl_solver) NONNULL;
int mdl_copyprobtype(Model *mdl, const Model *mdl_src) NONNULL;

int mdl_export(Model *mdl, Model *mdl_dst) NONNULL;
int mdl_copyassolvable(Model *mdl, Model *mdl_src) NONNULL;
int mdl_reportvalues(Model *mdl, Model *mdl_src) NONNULL;

int mdl_gettype(const Model *mdl, ModelType *probtype) NONNULL;
int mdl_getobjjacval(const Model *mdl, double *objjacval) NONNULL;
int mdl_getoption(const Model *mdl, const char *option, void *val) NONNULL;
const char *mdl_getprobtypetxt(ModelType probtype);

int mdl_analyze_modeltype(Model *mdl) NONNULL;
int mdl_recompute_modeltype(Model *mdl) NONNULL;

int mdl_getmodelstat(const Model *mdl, int *modelstat) NONNULL;
const char *mdl_getmodelstatastxt(const Model *mdl) NONNULL;
const char *mdl_modelstattxt(const Model *mdl, int modelstat) NONNULL;
int mdl_getobjequ(const Model *mdl, rhp_idx *objequ) NONNULL;
int mdl_getobjequs(const Model *mdl, Aequ *objs) NONNULL;
int mdl_getsense(const Model *mdl, RhpSense *objsense) NONNULL;
int mdl_getobjvar(const Model *mdl, rhp_idx *objvar) NONNULL;
int mdl_getsolvername(const Model *mdl, char const ** solvername) NONNULL;
int mdl_getsolvestat(const Model *mdl, int *solvestat) NONNULL;
const char *mdl_getsolvestatastxt(const Model *mdl) NONNULL;
const char *mdl_solvestattxt(const Model *mdl, int solvestat) NONNULL;

int mdl_setmodelstat(Model *mdl, int modelstat) NONNULL;
int mdl_setsolvestat(Model *mdl, int solvestat) NONNULL;
int mdl_setname(Model *mdl, const char *name) NONNULL;
int mdl_setsense(Model *mdl, unsigned objsense) NONNULL;
int mdl_setsolvername(Model *mdl, const char *solvername) NONNULL;

/* the following group can accomodate a null mdl pointer */
unsigned mdl_getid(const Model *mdl);
const char* mdl_getname(const Model *mdl);
int mdl_getnamelen(const Model *mdl);
const char* mdl_getname2(const Model *mdl);
int mdl_getnamelen2(const Model *mdl);


rhp_idx mdl_getcurrentei(const Model *mdl, rhp_idx ei) NONNULL;
rhp_idx mdl_getcurrentvi(const Model *mdl, rhp_idx vi) NONNULL;

int mdl_settype(Model *mdl, ModelType probtype) NONNULL;
int mdl_setobjvar(Model *mdl, rhp_idx vi) NONNULL;

int mdl_ensure_exportdir(Model *mdl) NONNULL;
int mdl_export_gms(Model *mdl, const char *phase_name) NONNULL;

MathPrgm* mdl_getmpforequ(const Model *mdl, rhp_idx ei) NONNULL;
MathPrgm* mdl_getmpforvar(const Model *mdl, rhp_idx vi) NONNULL;

McpInfo* mdl_getmcpinfo(Model *mdl) NONNULL;
int mdl_solreport(Model *mdl_dst, Model *mdl_src) NONNULL;

int mdl_setdualvars(Model *mdl, Avar *v, Aequ *e) NONNULL;

static inline bool mdl_is_rhp(const Model *mdl) {
  return mdl->backend == RHP_BACKEND_RHP ||
         mdl->backend == RHP_BACKEND_JULIA ||
         mdl->backend == RHP_BACKEND_AMPL;
}

static inline const char* mdl_printvarname(const Model *mdl, rhp_idx vi) {
   return ctr_printvarname(&mdl->ctr, vi);
}

static inline const char* mdl_printequname(const Model *mdl, rhp_idx ei) {
   return ctr_printequname(&mdl->ctr, ei);
}

static inline bool mdl_metachecked(const Model *mdl) {
  return mdl->status & MdlMetaChecked;
}

static inline void mdl_setmetachecked(Model *mdl) {
  mdl->status |= MdlMetaChecked;
}

static inline void mdl_unsetmetachecked(Model *mdl) {
  mdl->status &= ~MdlMetaChecked;
}

static inline bool mdl_checked(const Model *mdl) {
  return mdl->status & MdlChecked;
}

static inline void mdl_setchecked(Model *mdl) {
  mdl->status |= MdlChecked;
}

static inline void mdl_unsetchecked(Model *mdl) {
  mdl->status &= ~MdlChecked;
}

static inline bool mdl_finalized(const Model *mdl) {
  return mdl->status & MdlFinalized;
}

static inline void mdl_setfinalized(Model *mdl) {
  mdl->status |= MdlFinalized;
}

static inline void mdl_unsetfinalized(Model *mdl) {
  mdl->status &= ~MdlFinalized;
}

static inline void mdl_unsetallchecks(Model *mdl) {
   mdl_unsetchecked(mdl);
   mdl_unsetmetachecked(mdl);
   mdl_unsetfinalized(mdl);
   empdag_unsetallchecks(&mdl->empinfo.empdag);
}

#define mdl_fmtargs(mdl) backend_name((mdl)->backend), mdl_getnamelen(mdl), mdl_getname(mdl), (mdl)->id

static inline unsigned mdl_nequs(const Model *mdl){
   return  ctr_nequs(&mdl->ctr);
}

static inline unsigned mdl_nequs_total(const Model *mdl){
   return  ctr_nequs_total(&mdl->ctr);
}

static inline unsigned mdl_nequs_max(const Model *mdl){
   return  ctr_nequs_max(&mdl->ctr);
}

static inline unsigned mdl_nvars(const Model *mdl){
   return  ctr_nvars(&mdl->ctr);
}

static inline unsigned mdl_nvars_total(const Model *mdl){
   return  ctr_nvars_total(&mdl->ctr);
}

static inline unsigned mdl_nvars_max(const Model *mdl){
   return  ctr_nvars_max(&mdl->ctr);
}

NONNULL
static inline bool mdl_valid_vi_(const Model *mdl, rhp_idx vi, const char *fn) {
   return valid_vi_(vi, ctr_nvars_total(&mdl->ctr), fn);
}

NONNULL
static inline bool mdl_valid_ei_(const Model *mdl, rhp_idx ei, const char *fn) {
   return valid_ei_(ei, ctr_nequs_total(&mdl->ctr), fn);
}

NONNULL static inline void mdl_resettype(Model *mdl)
{
   mdl_settype(mdl, MdlType_none);
}

/**
 * @brief Get the pool, the array of constant values for the NL expressions
 *
 * @param mdl   the model
 *
 * @return      the pool pointer
 */
static inline NlPool *mdl_getpool(Model *mdl) { return mdl->ctr.pool; }

/* ----------------------------------------------------------------------
 * Memory management
 * ---------------------------------------------------------------------- */
NONNULL static inline M_ArenaTempStamp mdl_memtmp_init(Model *mdl) {
   return ctr_memtmp_init(&mdl->ctr);
}

NONNULL static inline void *mdl_memtmp_get(Model *mdl, size_t sz) {
   return ctr_memtmp_get(&mdl->ctr, sz);
}

NONNULL static inline void mdl_memtmp_fini(M_ArenaTempStamp astamp) {
   ctr_memtmp_fini(astamp);
}

#endif /* RHP_MDL_H */
