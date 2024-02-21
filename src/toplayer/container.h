#ifndef CONTAINER_H
#define CONTAINER_H

/** @file container.h
 *
 *  @brief Container structure
 */


#include "compat.h"
#include "equ.h"
#include "equvar_metadata.h"
#include "mdl_data.h"
#include "container_ops.h"
#include "reshop.h"
#include "rhp_fwd.h"

typedef enum rhp_backendtype BackendType;

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
  CtrChecked           = 1,
  CtrMetaChecked       = 2,
  CtrNeedEquVarNames   = 4,
  CtrHasBeenSolvedOnce = 8,
} CtrStatus;

typedef struct {
   Aequ flipped_equs;
} CtrTransformations;


/* @brief the container for the model */
typedef struct container {
   void *data;                  /**< container data         */
   const CtrOps *ops;           /**< container operations   */
   BackendType backend;         /**< Backend type                           */
   CtrStatus status;

   unsigned m;                  /**< number of (active) equations */
   unsigned n;                  /**< number of (active) variables */

   struct {
      void *mem;                /**< pointer to workspace memory             */
      size_t size;              /**< size of the workspace memory            */
      bool inuse;               /**< true if the memory is inuse (for debugging) */
   } workspace;                 /**< workspace memory area                   */

   struct nltree_pool *pool;    /**< pool of numerical values */

   Equ *equs;                   /**< an array of equations */
   Var *vars;                   /**< an array of variables */
   EquMeta *equmeta;            /**< meta data for equations          */
   VarMeta *varmeta;            /**< meta data for variables          */

   CtrTransformations transformations;  /**< Transformations to execute on the 
                                 Container */
   rhp_idx *rosetta_equs;       /**< translation for the equations */
   rhp_idx *rosetta_vars;       /**< translation for the variables */

   Aequ *func2eval;             /**< Functions / equations to evaluate */
   Avar *fixed_vars;            /**< Variables to be included              */
   Container *ctr_up;           /**< Source container                      */
} Container;

int ctr_alloc(Container *ctr, BackendType backend) NONNULL;
void ctr_dealloc(Container *ctr) NONNULL;
int ctr_resize(Container *ctr, unsigned n, unsigned m);

int ctr_equcontainvar(const Container *ctr, int e, int x, double *jacval,
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
int ctr_isequNL(const Container *ctr, rhp_idx ei, bool *isNL);

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
int ctr_getvarsmult(const Container *ctr, Avar *v, double *mult);
int ctr_getvarsbasis(const Container *ctr, Avar *v, int *basis);
int ctr_getvarsval(const Container *ctr, Avar *v, double *vals);
int ctr_getequsmult(const Container *ctr, Aequ *e, double *mult);
int ctr_getequsbasis(const Container *ctr, Aequ *e, int *basis);
int ctr_getequsval(const Container *ctr, Aequ *e, double *vals);
int ctr_getequbyname(const Container *ctr, const char* name, rhp_idx *ei);
int ctr_getallequsmult(const Container *ctr, double *mult);
int ctr_getequbasis(const Container *ctr, rhp_idx ei, int *basis);
int ctr_getallequsval(const Container *ctr, double *vals);
int ctr_getequval(const Container *ctr, rhp_idx ei, double *value);
int ctr_getequperp(const Container *ctr, rhp_idx ei, rhp_idx *vi);
int ctr_getequmult(const Container *ctr, rhp_idx ei, double *multiplier);
int ctr_getequtype(const Container *ctr, rhp_idx ei, unsigned *type, unsigned *cone);
int ctr_getcst(const Container *ctr, rhp_idx ei, double *val);
int ctr_getspecialfloats(const Container *ctr, double *minf, double *pinf, double* nan);
int ctr_getvarperp(const Container *ctr, rhp_idx vi, rhp_idx *ei);
int ctr_getallvarsmult(const Container *ctr, double *mult);
int ctr_getvarbasis(const Container *ctr, rhp_idx vi, rhp_idx *basis);
int ctr_getallvarsval(const Container *ctr, double *vals);
int ctr_getvarbounds(const Container *ctr, rhp_idx vi, double* lb, double *ub);
int ctr_getvarbyname(const Container *ctr, const char* name, rhp_idx *vi);
int ctr_getvarval(const Container *ctr, rhp_idx vi, double *value);
int ctr_getvarmult(const Container *ctr, rhp_idx vi, double *multiplier);
int ctr_getvartype(const Container *ctr, rhp_idx vi, unsigned *type);
int ctr_setcst(Container *ctr, rhp_idx ei, double val);
int ctr_setequbasis(Container *ctr, rhp_idx ei, int basis);
int ctr_setequvalue(Container *ctr, rhp_idx ei, double value);
int ctr_setequname(Container *ctr, rhp_idx ei, const char *name);
int ctr_setequmult(Container *ctr, rhp_idx ei, double multiplier);
int ctr_setequtype(Container *ctr, rhp_idx ei, unsigned type, unsigned cone);
int ctr_setequvarperp(Container *ctr, rhp_idx ei, rhp_idx vi);
int ctr_setequrhs(Container *ctr, rhp_idx ei, double val);
int ctr_setvarlb(Container *ctr, rhp_idx vi, double lb);
int ctr_setvarbasis(Container *ctr, rhp_idx vi, int basis);
int ctr_setvarvalue(Container *ctr, rhp_idx vi, double varl);
int ctr_setvarmult(Container *ctr, rhp_idx vi, double varm);
int ctr_setvarname(Container *ctr, rhp_idx vi, const char *name);
int ctr_setvartype(Container *ctr, rhp_idx vi, unsigned type);
int ctr_setvarub(Container *ctr, rhp_idx vi, double ub);

/* ----------------------------------------------------------------------
 * Container metadata/transformation section
 * ---------------------------------------------------------------------- */

int ctr_markequasflipped(Container *ctr, Aequ *e) NONNULL;

NONNULL static inline bool ctr_needreformulation(Container *ctr) {
   return ctr->transformations.flipped_equs.size > 0;
}

/* ----------------------------------------------------------------------
 * Container memory management
 * ---------------------------------------------------------------------- */

void *ctr_getmem(Container *ctr, size_t size) ALLOC_SIZE(2);
void *ctr_ensuremem(Container *ctr, size_t cur_size, size_t extra_size);
void ctr_relmem(Container *ctr);
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
   case RHP_BACKEND_RHP:
   case RHP_BACKEND_JULIA:
   case RHP_BACKEND_AMPL:
      return true;
   case RHP_BACKEND_GAMS_GMO:
   default:
      return false;
   }
}

#endif /* CONTAINER_H */
