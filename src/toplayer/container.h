#ifndef CONTAINER_H
#define CONTAINER_H

/** @file container.h
 *
 *  @brief Container structure
 */


#include "allocators.h"
#include "compat.h"
#include "equ.h"
#include "equvar_metadata.h"
#include "container_ops.h"
#include "rhp_fwd.h"
#include "rhp_typedefs.h"
#include "var.h"

/**
 * Data structure provided for the consumer of workspace memory.
 *
 * In conjunction with the CTRMEM cleanup attribute, ensures that the workspace
 * memory is not used twice.
 */
struct ctrmem {
   void *ptr;
   Container *ctr;
};

/** Status of the Container */
typedef enum ctr_status {
   CtrNeedEquVarNames    = 0x4,
   CtrHasBeenSolvedOnce  = 0x8,
   CtrEquVarInherited    = 0x10,
   CtrMetaDataAvailable  = 0x20,
   CtrHasNlPool          = 0x40,
   CtrInstantiable       = CtrEquVarInherited | CtrMetaDataAvailable | CtrHasNlPool,
} CtrStatus;

typedef struct {
   Aequ flipped_equs;
} CtrTransformations;


typedef struct {
   unsigned vartypes[VAR_TYPE_LEN];

   struct {
      unsigned cones [CONE_LEN];
      unsigned types[EquTypeUnsupported+1];
      unsigned exprtypes[EquExprNonLinear+1];
   } equs;
} EquVarTypeCounts;

/* @brief the container for the model */
typedef struct container {
   void *data;                  /**< container data         */
   const CtrOps *ops;           /**< container operations   */
   BackendType backend;         /**< Backend type                           */
   CtrStatus status;

   unsigned m;                  /**< number of (active) equations */
   unsigned n;                  /**< number of (active) variables */

   struct {
      uint8_t *mem;                /**< pointer to workspace memory             */
      size_t size;              /**< size of the workspace memory            */
      bool inuse;               /**< true if the memory is inuse (for debugging) */
   } workspace;                 /**< workspace memory area                   */

   M_ArenaLink arenaL_temp;      /**< Arena temporary memory area             */
   M_ArenaLink arenaL_perm;      /**< Arena permament memory                  */
   NlPool *nlpool;               /**< pool of numerical values */

   Equ *equs;                   /**< an array of equations */
   Var *vars;                   /**< an array of variables */
   EquMeta *equmeta;            /**< meta data for equations          */
   VarMeta *varmeta;            /**< meta data for variables          */

   CtrTransformations transformations;  /**< Transformations to execute on the 
                                 Container */
   EquVarTypeCounts equvarstats; /**< Statistics about the equations and variables */
   rhp_idx *rosetta_equs;       /**< translation for the equations */
   rhp_idx *rosetta_vars;       /**< translation for the variables */
   Fops *fops;                      /**< Current filter operations            */


   Aequ *func2eval;             /**< Functions / equations to evaluate after a solve */
   Avar *fixed_vars;            /**< Variables to be included              */
   Container *ctr_up;           /**< Source container                      */
} Container;

int  ctr_init(Container *ctr, BackendType backend) NONNULL;
void ctr_fini(Container *ctr) NONNULL;
int  ctr_resize(Container *ctr, unsigned n, unsigned m) NONNULL;
int  ctr_trimmem(Container *ctr) NONNULL;

int ctr_equ_findvar(const Container *ctr, rhp_idx ei, rhp_idx vi, double *jacval,
                    int *nlflag);

/* Wrapper functions for container_ops. */
int ctr_evalequvar(Container *ctr);
int ctr_evalfunc(const Container *ctr, int si, double *x, double *f,
                 int *numerr);
int ctr_evalgrad(const Container *ctr, int si, double *x, double *f,
                 double *g, double *gx, int *numerr);
int ctr_evalgradobj(const Container *ctr, double *x, double *f, double *g,
                    double *gx, int *numerr);
int ctr_fixedvars(Container *ctr) NONNULL;
int ctr_var_iterequs(const Container *ctr, rhp_idx vi, void **jacptr,
                      double *jacval, rhp_idx *ei, int *nlflag);
int ctr_equ_itervars(const Container *ctr, rhp_idx ei, void **jacptr,
                      double *jacval, rhp_idx *vi, int *nlflag);
int ctr_equ_iscst(const Container *ctr, rhp_idx ei, bool *iscst) NONNULL;

int ctr_ensure_pool(Container *ctr);
int ctr_borrow_nlpool(Container *ctr, Container *ctr_src);
double ctr_poolval(Container *ctr, unsigned idx);

unsigned ctr_nequs(const Container *ctr);
unsigned ctr_nvars(const Container *ctr);
unsigned ctr_nvars_total(const Container *ctr);
unsigned ctr_nequs_total(const Container *ctr);
unsigned ctr_nvars_max(const Container *ctr);
unsigned ctr_nequs_max(const Container *ctr);

const char* ctr_printequname(const Container *ctr, rhp_idx ei) NONNULL;
const char* ctr_printequname2(const Container *ctr, rhp_idx ei) NONNULL;
const char* ctr_printvarname(const Container *ctr, rhp_idx vi) NONNULL;
const char* ctr_printvarname2(const Container *ctr, rhp_idx vi) NONNULL;

/* ----------------------------------------------------------------------
 * ContainerOps wrapper for equations and variables
 * ---------------------------------------------------------------------- */

int ctr_copyequname(const Container *ctr, rhp_idx ei, char *name, unsigned len) NONNULL;
int ctr_copyvarname(const Container *ctr, rhp_idx vi, char *name, unsigned len) NONNULL;
int ctr_fixvar(Container *ctr, rhp_idx vi, double val);
int ctr_equvarcounts(Container *ctr) NONNULL;
int ctr_getvarsbasis(const Container *ctr, Avar *v, int *vbasis_info);
int ctr_getvarsdual(const Container *ctr, Avar *v, double *vdual);
int ctr_getvarslevel(const Container *ctr, Avar *v, double *vlevel);
int ctr_getequsdual(const Container *ctr, Aequ *e, double *edual);
int ctr_getequsbasis(const Container *ctr, Aequ *e, int *ebasis_info);
int ctr_getequslevel(const Container *ctr, Aequ *e, double *elevel);
int ctr_getequbyname(const Container *ctr, const char* name, rhp_idx *ei);


int ctr_getallequsbasis(const Container *ctr, int *basis_infos);
int ctr_getallequsdual(const Container *ctr, double *duals);
int ctr_getallequslevel(const Container *ctr, double *levels);

int ctr_getequbasis(const Container *ctr, rhp_idx ei, int *basis_info);
int ctr_getequlevel(const Container *ctr, rhp_idx ei, double *level);
int ctr_getequperp(const Container *ctr, rhp_idx ei, rhp_idx *vi);
int ctr_getequdual(const Container *ctr, rhp_idx ei, double *dual);
int ctr_getequtype(const Container *ctr, rhp_idx ei, unsigned *type, unsigned *cone);
int ctr_getequexprtype(const Container *ctr, rhp_idx ei, EquExprType *type);
int ctr_getequcst(const Container *ctr, rhp_idx ei, double *cst);
int ctr_getspecialfloats(const Container *ctr, double *minf, double *pinf, double* nan);
int ctr_getvarperp(const Container *ctr, rhp_idx vi, rhp_idx *ei);

int ctr_getallvarsbasis(const Container *ctr, int *basis_infos);
int ctr_getallvarsdual(const Container *ctr, double *duals);
int ctr_getallvarslevel(const Container *ctr, double *levels);

int ctr_getvarbasis(const Container *ctr, rhp_idx vi, int *basis_info);
int ctr_getvarbounds(const Container *ctr, rhp_idx vi, double* lb, double *ub);
int ctr_getvarbyname(const Container *ctr, const char* name, rhp_idx *vi);
int ctr_getvarlevel(const Container *ctr, rhp_idx vi, double *level);
int ctr_getvardual(const Container *ctr, rhp_idx vi, double *dual);
int ctr_getvartype(const Container *ctr, rhp_idx vi, unsigned *type);
int ctr_getvarlb(const Container *ctr, rhp_idx vi, double *lb);
int ctr_getvarub(const Container *ctr, rhp_idx vi, double *ub);
int ctr_setequcst(Container *ctr, rhp_idx ei, double cst);
int ctr_setequbasis(Container *ctr, rhp_idx ei, int basis_info);
int ctr_setequlevel(Container *ctr, rhp_idx ei, double level);
int ctr_setequname(Container *ctr, rhp_idx ei, const char *name);
int ctr_setequdual(Container *ctr, rhp_idx ei, double dual);
int ctr_setequtype(Container *ctr, rhp_idx ei, unsigned type, unsigned cone);
int ctr_setequvarperp(Container *ctr, rhp_idx ei, rhp_idx vi);
int ctr_setequrhs(Container *ctr, rhp_idx ei, double rhs);
int ctr_setvarlb(Container *ctr, rhp_idx vi, double lb);
int ctr_setvarbasis(Container *ctr, rhp_idx vi, int basis_info);
int ctr_setvarlevel(Container *ctr, rhp_idx vi, double level);
int ctr_setvardual(Container *ctr, rhp_idx vi, double dual);
int ctr_setvarname(Container *ctr, rhp_idx vi, const char *name);
int ctr_setvartype(Container *ctr, rhp_idx vi, unsigned type);
int ctr_setvarub(Container *ctr, rhp_idx vi, double ub);
int ctr_setvarbounds(Container *ctr, rhp_idx vi, double lb, double ub);


/* ----------------------------------------------------------------------
 * Container Export
 * ---------------------------------------------------------------------- */

int ctr_gen_rosettas(Container *ctr) NONNULL;
int ctr_prepare_export(Container *ctr_src, Container *ctr_dst) NONNULL;

/* ----------------------------------------------------------------------
 * Container metadata/transformation section
 * ---------------------------------------------------------------------- */

int ctr_markequasflipped(Container *ctr, Aequ *e) NONNULL;

NONNULL static inline bool ctr_needtransformations(Container *ctr) {
   return (ctr->transformations.flipped_equs.size > 0);
}

NONNULL
int ctr_get_defined_mapping_by_var(const Container* ctr, rhp_idx vi, rhp_idx *ei);

/* ----------------------------------------------------------------------
 * Container memory management
 * ---------------------------------------------------------------------- */

void *ctr_memtmp_get(Container *ctr, size_t size) ALLOC_SIZE(2) NONNULL;
void ctr_memtmp_rel(Container *ctr, size_t size) NONNULL;
M_ArenaTempStamp ctr_memtmp_init(Container *ctr) NONNULL;
void ctr_memtmp_fini(M_ArenaTempStamp stamp);

void *ctr_getmem_old(Container *ctr, size_t size) ALLOC_SIZE(2);
void *ctr_ensuremem_old(Container *ctr, size_t cur_size, size_t extra_size);
void ctr_relmem_old(Container *ctr) NONNULL;
void ctr_relmem_recursive_old(Container *ctr) NONNULL;
void ctr_memclean(struct ctrmem *ctrmem);

/* ----------------------------------------------------------------------
 * Inline functions
 * ---------------------------------------------------------------------- */

static inline rhp_idx ctr_getcurrent_ei(const Container *ctr, size_t ei)
{
  return ctr->rosetta_equs ? ctr->rosetta_equs[ei] : (rhp_idx)ei;
}

static inline rhp_idx ctr_getcurrent_vi(const Container *ctr, size_t vi)
{
  return ctr->rosetta_vars ? ctr->rosetta_vars[vi] : (rhp_idx)vi;
}

static inline bool ctr_is_rhp(const Container *ctr)
{
   switch (ctr->backend) {
   case RhpBackendReSHOP:
   case RhpBackendJulia:
   case RhpBackendAmpl:
      return true;
   case RhpBackendGamsGmo:
   default:
      return false;
   }
}

/**
 * @brief Return true when the container has metadata
 *
 * @param ctr  the container
 *
 * @return     true if the container has metadata
 */
static inline bool ctr_hasmetadata(const Container *ctr)
{
   return ctr->equmeta && ctr->varmeta;
}

static inline NlPool *ctr_getpool(Container *ctr) { return ctr->nlpool; }

bool ctr_chk_equ_ownership(const Container *ctr, rhp_idx ei, mpid_t mpid, mpid_t *mpid_owner) NONNULL;

#endif /* CONTAINER_H */
