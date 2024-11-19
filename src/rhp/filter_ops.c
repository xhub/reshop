#include <float.h>
#include <math.h>

#include "cmat.h"
#include "container.h"
#include "equ.h"
#include "equvar_helpers.h"
#include "mdl.h"
#include "nltree_priv.h"
#include "filter_ops.h"
#include "gams_utils.h"
#include "instr.h"
#include "macros.h"
#include "mathprgm.h"
#include "mdl_rhp.h"
#include "ctrdat_rhp.h"
#include "pool.h"
#include "printout.h"
#include "var.h"

typedef struct filter_deactivated {
   IdxArray vars;    
   IdxArray equs;
   Avar v;
   Aequ e;
} FilterDeactivated;

typedef struct {
   unsigned offset_vars_pool;      /**< Start in the pool for the var values  */
   unsigned size;           /**< current number of pointer stored        */
   unsigned max;            /**< maximum size of the arrays              */
   unsigned *pool_idx;       /**< array of pointers to the pool index     */
   rhp_idx *vis;           /**< variable indices                        */
   NlPool *pool;
} NlPoolVars;


/** filter based on a subset of variables and equations */
typedef struct filter_subset {
   struct mp_descr descr;          /**< Description of the math programm      */
   Avar vars;                      /**< Variables to be included              */
   Aequ equs;                      /**< Equations to be included              */
   NlPoolVars nlpoolvars;          /**< Non-linear pool variables             */
   Container *ctr_src;             /**< Source container                      */
   rhp_idx *vars_permutation;      /**< Optional variable permutation         */
} FilterSubset;

typedef struct filter_active {
   FilterDeactivated deactivated;  /**< Deactivated                           */
   Container *ctr;                 /**< Container                             */
   rhp_idx *vars_permutation;      /**< Optional variable permutation         */
} FilterActive;

typedef struct {
   daguid_t subdag_root;
   Fops *parent;
   FilterSubset *fs;
} FilterEmpDagSubDag;

typedef struct {
   mpid_t mpid;
   IdxArray *mp_equs;
   IdxArray *mp_vars;
   Container *ctr;
   Fops *parent;
} FilterEmpDagSingleMp;

typedef struct {
   MpIdArray mpids;
   Fops *parent;
} FilterEmpDagNash;


const char *fopstype2str(FopsType type)
{
   switch (type) {
   case FopsActive:         return "active";
   case FopsSubset:         return "subset";
   case FopsEmpDagNash:     return "EmpDag Nash";
   case FopsEmpDagSingleMp: return "Single MP";
   case FopsEmpDagSubDag:   return "EmpDag Subdag";
   default:                 return "error unknown filter ops type";
   }
}

int fops_deactivate_equ(void *data, rhp_idx ei)
{
   FilterDeactivated *deactivated = &((FilterActive *)data)->deactivated;

   return rhp_idx_addsorted(&deactivated->equs, ei);
}

int fops_deactivate_var(void *data, rhp_idx vi)
{
   FilterDeactivated *deactivated = &((FilterActive *)data)->deactivated;
   return rhp_idx_addsorted(&deactivated->vars, vi);
}

static inline bool deactivated_var(FilterDeactivated *dat, rhp_idx vi)
{
   return (dat->vars.len > 0 && UINT_MAX > rhp_idx_findsorted(&dat->vars, vi))
           || avar_contains(&dat->v, vi);
}

static inline bool deactivated_equ(FilterDeactivated *dat, rhp_idx ei)
{
   return (dat->equs.len > 0 && UINT_MAX > rhp_idx_findsorted(&dat->equs, ei))
           || aequ_contains(&dat->e, ei);
}

static void filter_deactivated_init(FilterDeactivated *deact)
{
   rhp_idx_init(&deact->vars);
   rhp_idx_init(&deact->equs);
   avar_init(&deact->v);
   aequ_init(&deact->e);
}

static void filter_active_freedata(void *data)
{
   if (!data) { return; }
   FilterActive *dat = (FilterActive *)data;
   FilterDeactivated *deactivated = &dat->deactivated;
   avar_empty(&deactivated->v);
   aequ_empty(&deactivated->e);
   rhp_idx_empty(&deactivated->vars);
   rhp_idx_empty(&deactivated->equs);

   FREE(data);
}

UNUSED static unsigned filter_active_deactivatedequslen(void *data)
{
   FilterActive *dat = (FilterActive *)data;
   FilterDeactivated *deactivated = &dat->deactivated;

   return deactivated->equs.len + aequ_size(&deactivated->e);
}

static inline bool rctr_active_var(const Container *ctr, rhp_idx vi)
{
   RhpContainerData *cdat = ctr->data;
   return cdat->vars[vi];
}

/*  TODO(xhub) PERF measure impact of indirection */
static bool filter_active_var(void *data, rhp_idx vi)
{
   FilterActive *dat = (FilterActive *)data;
   RhpContainerData *cdat = dat->ctr->data;
   assert(valid_vi_(vi, cdat->total_n, __func__));

   return cdat->vars[vi] && !deactivated_var(&dat->deactivated, vi);
}

static bool filter_active_equ(void *data, rhp_idx ei)
{
   FilterActive *dat = (FilterActive *)data;
   RhpContainerData *cdat = dat->ctr->data;
   assert(valid_ei_(ei, cdat->total_m, __func__));

   return cdat->equs[ei] && !deactivated_equ(&dat->deactivated, ei);
}

static void filter_active_size(void *data, size_t* n, size_t* m)
{
   FilterActive *dat = (FilterActive *)data;
   Container *ctr = dat->ctr;
   *n = (size_t)ctr->n;
   *m = (size_t)ctr->m;
}

/* -------------------------------------------------------------------------
 * ConLequ on edst from the active variables
 * ------------------------------------------------------------------------- */

// TODO: delete? Commented on 2024.03.12
//static int filter_active_lequ(void *data, Equ *e, Equ *edst)
//{
//   FilterActive *dat = (FilterActive *)data;
//   Container *ctr = dat->ctr;
//   rhp_idx * restrict rosetta_vars = ctr->rosetta_vars;
//   Lequ *le = e->lequ;
//   size_t len;
//   if (le && le->len > 0) {
//      len = le->len;
//   } else {
//      return OK;
//   }
//
//   rhp_idx * restrict idx = le->vis;
//   double * restrict vals = le->coeffs;
//
//   if (!edst->lequ) { A_CHECK(edst->lequ, lequ_alloc(len)); }
//   Lequ * lequ = edst->lequ;
//
//   for (unsigned i = 0; i < len; ++i) {
//      rhp_idx vi_new = rosetta_vars[idx[i]];
//      double val = vals[i];
//
//      if (valid_vi(vi_new) && isfinite(val)) {
//         lequ_add(lequ, vi_new, val); /* no check as we do not resize */
//      } else {
//         if (!isnan(val)) {
//            error("%s :: In an equation, the deleted variable %d has a value %e\n",
//                  __func__, idx[i], val);
//         }
//      }
//   }
//
//   return OK;
//}

/* -------------------------------------------------------------------------
 * Change the variable indices in the GAMS code
 * ------------------------------------------------------------------------- */

static void perm_gamsopcode_rosetta(const rhp_idx * restrict rosetta_vars,
                                      rhp_idx ei, unsigned len,
                                      const int instrs[VMT(static restrict len)],
                                      int args[VMT(static restrict len)])
{
   for (unsigned i = 0; i < len; ++i) {
      if (gams_get_optype(instrs[i]) == NLNODE_OPARG_VAR) {
         assert(valid_vi(rosetta_vars[args[i]-1]));
         args[i] = 1 + rosetta_vars[args[i]-1];
      }
   }

   /* Update the nlStore field to match the new equation index */
   assert(instrs[len-1] == nlStore);
   args[len-1] = 1 + ei;
}

UNUSED static
void filter_gamsopcode_rosetta_perm(const rhp_idx * restrict rosetta_vars,
                                    const rhp_idx * restrict vperm,
                                    rhp_idx ei, unsigned len,
                                    const int instrs[VMT(static restrict len)],
                                    int args[VMT(static restrict len)])
{
   for (unsigned i = 0; i < len; ++i) {
      if (gams_get_optype(instrs[i]) == NLNODE_OPARG_VAR) {
         assert(valid_vi(rosetta_vars[args[i]-1]));
         args[i] = 1 + vperm[rosetta_vars[args[i]-1]];
      }
   }

   /* Update the nlStore field to match the new equation index */
   assert(instrs[len-1] == nlStore);
   args[len-1] = 1 + ei;
}

static int perm_active_gamsopcode(void *data, rhp_idx ei, unsigned len,
                                    int instrs[VMT(static restrict len)],
                                    int args[VMT(static restrict len)])
{
   FilterActive *dat = (FilterActive *)data;
   Container *ctr = dat->ctr;
   rhp_idx * restrict rosetta_vars = ctr->rosetta_vars;

   perm_gamsopcode_rosetta(rosetta_vars, ei, len, instrs, args);

   return OK;
}

// TODO delete? Commented on 2024.03.12
//static int filter_active_nltree(void *data, Equ *e, Equ *edst)
//{
//   FilterActive *dat = (FilterActive *)data;
//   Container *ctr = dat->ctr;
//   rhp_idx *rosetta_vars = ctr->rosetta_vars;
//
//   A_CHECK(edst->tree, nltree_dup_rosetta(e->tree, rosetta_vars));
//
//   return OK;
//}

static rhp_idx filter_active_vperm(void *data, rhp_idx vi)
{
   FilterActive *dat = data;
   if (vi >= ctr_nvars_total(dat->ctr)) { return IdxOutOfRange; }

   return dat->vars_permutation[vi];
}

static int filter_active_setvarpermutation(Fops *fops, rhp_idx *vperm)
{
   FilterActive *dat = fops->data;
   if (fops->vars_permutation || dat->vars_permutation) {
      error("[fops] ERROR in %s: permutation data is non-NULL\n", __func__);
      return Error_RuntimeError;
   }

   dat->vars_permutation = vperm;
   fops->vars_permutation = &filter_active_vperm;

   return OK;
}

/* -------------------------------------------------------------------------
 * subset functions definition
 * ------------------------------------------------------------------------- */

static unsigned get_poolidx_negvar(NlPoolVars *dat, rhp_idx vi)
{
   for (unsigned i = 0, len = dat->size; i < len; ++i) {
      if (dat->vis[i] == vi) {
         return dat->pool_idx[i];
      }
   }

   if (dat->size >= dat->max) {
      dat->max = MAX(dat->size+1, dat->max);
      REALLOC_(dat->pool_idx, unsigned, dat->max);
      REALLOC_(dat->vis, rhp_idx, dat->max);
   }

   unsigned idx = pool_getidx(dat->pool, SNAN);

   dat->vis[dat->size] = vi;
   dat->pool_idx[dat->size++] = idx;

   return idx;
}

static bool filter_subset_var(void *data, rhp_idx vi)
{
   assert(valid_vi(vi));

   FilterSubset* fs = (FilterSubset *)data;
   const Container * restrict ctr_src = fs->ctr_src;
   RhpContainerData * restrict cdat = fs->ctr_src->data;
   bool is_active = ctr_is_rhp(ctr_src) ? cdat->vars[vi] != NULL : true;

   return avar_contains(&fs->vars, vi) &&
         !avar_contains(ctr_src->fixed_vars, vi) &&
          is_active; /* TODO: GITLAB #107 */
}

static bool filter_subset_equ(void *data, rhp_idx ei)
{
   assert(valid_ei(ei));

   FilterSubset* fs = (FilterSubset *)data;
   const Container * restrict ctr_src = fs->ctr_src;
   RhpContainerData * restrict cdat = fs->ctr_src->data;
   bool is_active = ctr_is_rhp(ctr_src) ? cdat->equs[ei] != NULL : true;

   /* TODO: GITLAB #107 */
   return is_active && aequ_contains(&fs->equs, ei);
}

static NONNULL size_t subset_nvars_rhp(Avar *v, Avar *fixed_vars, CMatElt **vars)
{
   size_t nvars = 0;

   for (unsigned i = 0, len = v->size; i < len; ++i) {
      rhp_idx vi = avar_fget(v, i);
      if (!avar_contains(fixed_vars, vi) && vars[vi]) { nvars++; }
   }

   return nvars;
}

static NONNULL size_t subset_nequs_rhp(Aequ *e, CMatElt ** equs)
{
   size_t nequs = 0;

   for (unsigned i = 0, len = e->size; i < len; ++i) {
      //TODO(URG) review
      UNUSED rhp_idx vi = aequ_fget(e, i);
      nequs++;
      //if (equs[vi]) { nequs++; }
   }

   return nequs;
}

static void filter_subset_size(void *data, size_t* ctr_n, size_t* ctr_m)
{
   FilterSubset *fs = (FilterSubset *)data;

   Avar *v = &fs->vars;
   Aequ *e = &fs->equs;
   const Container * restrict ctr_src = fs->ctr_src;
   RhpContainerData * restrict cdat = ctr_src->data;
   
   size_t nvars, nequs;

   if (ctr_is_rhp(ctr_src)) {
      nvars = subset_nvars_rhp(v, ctr_src->fixed_vars, cdat->vars);
      nequs = subset_nequs_rhp(e, cdat->equs);
   } else {
      nvars = avar_size(v) - avar_size(fs->ctr_src->fixed_vars);
      nequs = aequ_size(&fs->equs);
   }

   *ctr_n = nvars;
   *ctr_m = nequs;
}

// TODO delete? Commented on 2024.03.12
//static int filter_subset_lequ(void *data, Equ *e, Equ *edst)
//{
//   FilterSubset *fs = (FilterSubset *)data;
//   Container *ctr = fs->ctr_src;
//
//   Lequ *lequ;
//   Lequ *le = e->lequ;
//   size_t len;
//
//   if (le && le->len > 0) {
//      len = le->len;
//   } else {
//      return OK;
//   }
//
//   rhp_idx * restrict idx = le->vis;
//   double * restrict vals = le->coeffs;
//
//   rhp_idx * restrict rosetta_vars = ctr->rosetta_vars;
//   Var * restrict ctr_vars = ctr->vars;
//
//   if (!edst->lequ) {
//      A_CHECK(edst->lequ, lequ_alloc(len));
//      lequ = edst->lequ;
//   } else {
//      lequ = edst->lequ;
//   }
//
//   for (unsigned i = 0; i < len; ++i) {
//      rhp_idx vi = idx[i];
//      rhp_idx vi_new = rosetta_vars[vi];
//      double val = vals[i];
//
//      if (valid_vi(vi_new)) {
//         lequ_add(lequ, vi_new, val); /* no check as we do not resize */
//      } else {
//         if (isfinite(val) && fabs(val) > DBL_EPSILON) {
//            equ_add_cst(edst, val*ctr_vars[vi].value);
//         }
//      }
//   }
//
//   return OK;
//}

static int filter_gamsopcode_rosetta(const rhp_idx * restrict rosetta_vars,
                                     rhp_idx ei, unsigned len, NlPoolVars *datpool,
                                    int instrs[restrict len],
                                    int args[restrict len])
{
   /* ----------------------------------------------------------------------
    * For each variable 
    * - If the variable is still active, update the variable index
    * - If the variable is not active, we need to replace the opcode, from one
    *   that takes a variable as argument to one that takes a constant
    * ---------------------------------------------------------------------- */

   int offset_pool = (int)datpool->offset_vars_pool;
   for (unsigned i = 0; i < len; ++i) {

      if (gams_get_optype(instrs[i]) == NLNODE_OPARG_VAR) {
         rhp_idx vi = args[i]-1;
         rhp_idx vi_new = rosetta_vars[vi];

         if (valid_vi(vi_new)) {
            args[i] = 1 + vi_new;
         } else {
            int opcode = gams_opcode_var_to_cst(instrs[i]);

            /* -------------------------------------------------------------
             * Possible outcomes:
             * 1. opcode is valid and we give as argument the pool index
             * 2. the opcode was nlUminV (-v). Then we need to inject -value
             * 3. there is an error
             * ------------------------------------------------------------- */

            if (opcode >= 0 && opcode < nlOpcode_size) {
               instrs[i] = opcode;
               args[i]   = offset_pool + vi;
            } else if (opcode == -1) {
               instrs[i] = nlPushI;
               args[i]   = (int)get_poolidx_negvar(datpool, vi);
            } else {
               // The function gams_opcode_var_to_cst already prints the error
               return Error_Inconsistency;
            }
         }
      }
   }

   /* Update the nlStore field to match the new equation index */
   assert(instrs[len-1] == nlStore);
   args[len-1] = 1 + ei;

   return OK;
}

static int filter_subset_gamsopcode(void *data, rhp_idx ei, unsigned len,
                                    int * restrict instrs,
                                    int * restrict args)
{
   FilterSubset *fs = (FilterSubset *)data;
   rhp_idx * restrict rosetta_vars = fs->ctr_src->rosetta_vars;

   return filter_gamsopcode_rosetta(rosetta_vars, ei, len, &fs->nlpoolvars, instrs, args);
}

// TODO delete? Commented on 2024.03.12
//static int filter_subset_nltree(void *data, Equ *e, Equ *edst)
//{
//  TO_IMPLEMENT("");
//}

static void filter_subset_freedata(void* data)
{
   if (!data) { return; }
   FilterSubset *fs = data;
   filter_subset_release(fs);
}

FilterSubset* filter_subset_new(unsigned vlen, Avar vars[vlen], unsigned elen,
                                Aequ equs[elen], struct mp_descr* mp_d)
{
   FilterSubset *fs;

   CALLOC_NULL(fs, FilterSubset, 1);

   SN_CHECK_EXIT(avar_setblock(&fs->vars, vlen));
   SN_CHECK_EXIT(aequ_setblock(&fs->equs,  elen));

   for (unsigned i = 0; i < vlen; ++i) {
      Avar *v = &vars[i];
      if (v->size == 0) { continue; }
      SN_CHECK_EXIT(avar_extend(&fs->vars, v));
   }

   for (unsigned i = 0; i < elen; ++i) {
      Aequ *e = &equs[i];
      if (e->size == 0) { continue; }
      SN_CHECK_EXIT(aequ_extendandown(&fs->equs, e));
   }

   fs->nlpoolvars.offset_vars_pool = UINT_MAX;

   fs->nlpoolvars.size = 0;
   fs->nlpoolvars.max = 0;
   fs->nlpoolvars.pool_idx = NULL;
   fs->nlpoolvars.vis = NULL;

   memcpy(&fs->descr, mp_d, sizeof(struct mp_descr));

   /* Optional data */
   fs->vars_permutation = NULL;

   return fs;

_exit:
   /* Calling filter_subset_release here makes GCC whiney ...*/
   aequ_empty(&fs->equs);
   avar_empty(&fs->vars);
   FREE(fs->nlpoolvars.pool_idx);
   FREE(fs->nlpoolvars.vis);
   FREE(fs);

   return NULL;
}

static rhp_idx filter_subset_vperm(void *data, rhp_idx vi)
{
   FilterSubset *fs = data;
   if (vi >= ctr_nvars_total(fs->ctr_src)) { return IdxOutOfRange; }

   return fs->vars_permutation[vi];
}

static int filter_subset_setvarpermutation(Fops *fops, rhp_idx *vperm)
{
   if (fops->type != FopsSubset) {
      error("[fops] ERROR in %s: unexpected fops of type %s, was expecting %s",
            __func__, fopstype2str(fops->type), fopstype2str(FopsSubset));
      return Error_RuntimeError;
   }

   FilterSubset *fs = fops->data;
   if (fops->vars_permutation || fs->vars_permutation) {
      error("[fops] ERROR in %s: permutation data is non-NULL\n", __func__);
      return Error_RuntimeError;
   }

   fs->vars_permutation = vperm;
   fops->vars_permutation = &filter_subset_vperm;

   return OK;
}

void filter_subset_release(FilterSubset *fs)
{
   if (!fs) return;

   aequ_empty(&fs->equs);
   avar_empty(&fs->vars);
   FREE(fs->nlpoolvars.pool_idx);
   FREE(fs->nlpoolvars.vis);
   FREE(fs);
}

int filter_subset_activate(FilterSubset *fs, Model *mdl, unsigned offset_pool)
{
   if (!mdl_is_rhp(mdl)) {
      TO_IMPLEMENT("FilterSubset with GAMS model");
   }

   Container *ctr = &mdl->ctr;
   fs->ctr_src = &mdl->ctr;
   fs->nlpoolvars.offset_vars_pool = offset_pool;

   Fops fops;
   fops_subset_init(&fops, fs);

   if (!ctr->fops) {
      MALLOC_(ctr->fops, Fops, 1);
   }

   memcpy(ctr->fops, &fops, sizeof(Fops));

   empdag_reset_type(&mdl->empinfo.empdag);
   return rmdl_set_simpleprob(mdl, &fs->descr);
}

int fops_active_init(Fops *fops, Container *ctr)
{
   fops->type = FopsActive;
   FilterActive *dat;
   MALLOC_(dat, FilterActive, 1);
   dat->ctr = ctr;
   dat->vars_permutation = NULL;
   filter_deactivated_init(&dat->deactivated);

   fops->data = dat;
   fops->freedata = &filter_active_freedata;
//   fops->deactivatedequslen = &filter_active_deactivatedequslen;
   fops->get_sizes = &filter_active_size;
   fops->keep_var = &filter_active_var;
   fops->keep_equ = &filter_active_equ;
   fops->vars_permutation = NULL;
   fops->transform_gamsopcode = &perm_active_gamsopcode;
   //fops->transform_lequ = &filter_active_lequ;
   //fops->transform_nltree = &filter_active_nltree;

   return OK;
}

UNUSED static unsigned filter_subset_deactivatedequslen(void *data) { return 0; }

void fops_subset_init(Fops *fops, FilterSubset *fs)
{
   fops->type = FopsSubset;
   fops->data = fs;
//   fops->deactivatedequslen = &filter_subset_deactivatedequslen;
   fops->freedata = &filter_subset_freedata;
   fops->get_sizes = &filter_subset_size;
   fops->keep_var = &filter_subset_var;
   fops->keep_equ = &filter_subset_equ;
   fops->vars_permutation = NULL;
   fops->transform_gamsopcode = &filter_subset_gamsopcode;
   //fops->transform_lequ = &filter_subset_lequ;
   //fops->transform_nltree = &filter_subset_nltree;
}

struct avar_list {
   unsigned len;
   unsigned max;
   Avar *list;
};

#define RHP_LOCAL_SCOPE
#define RHP_LIST_PREFIX avar_list
#define RHP_LIST_TYPE avar_list
#define RHP_ELT_TYPE Avar
#define RHP_ELT_INVALID ((Avar){.size = UINT_MAX, .type = EquVar_Unset})
#include "list_generic.inc"

struct aequ_list {
   unsigned len;
   unsigned max;
   Aequ *list;
};

#define RHP_LOCAL_SCOPE
#define RHP_LIST_PREFIX aequ_list
#define RHP_LIST_TYPE aequ_list
#define RHP_ELT_TYPE Aequ
#define RHP_ELT_INVALID ((Aequ){.size = UINT_MAX, .type = EquVar_Unset})
#include "list_generic.inc"

static int dfs_equvar(EmpDag *empdag, daguid_t uid, struct avar_list *vars,
                      struct aequ_list *equs)
{
   daguid_t * restrict children;
   struct VFedges * restrict Varcs;
   unsigned num_children;

   if (uidisMP(uid)) {
      mpid_t id = uid2id(uid);
      UIntArray *Carcs = &empdag->mps.Carcs[id];
      children = Carcs->arr;
      num_children = Carcs->len;

      Varcs = &empdag->mps.Varcs[id];

      MathPrgm *mp = empdag->mps.arr[id];

      IdxArray mpvars = mp->vars;
      Avar v;
      avar_setlist(&v, mpvars.len, mpvars.arr);
      S_CHECK(avar_list_add(vars, v));

      IdxArray mpequs = mp->equs;
      Aequ e;
      aequ_setlist(&e, mpequs.len, mpequs.arr);
      S_CHECK(aequ_list_add(equs, e));

   } else {

      nashid_t id = uid2id(uid);
      UIntArray *mpe_children = &empdag->nashs.arcs[id];
      children = mpe_children->arr;
      num_children = mpe_children->len;

      assert(num_children > 0);

      Varcs = NULL;
   }


   for (unsigned i = 0; i < num_children; ++i) {
      S_CHECK(dfs_equvar(empdag, children[i], vars, equs));
   }

   if (!Varcs) { return OK; }

   struct rhp_empdag_arcVF *Vlist = Varcs->arr;
   for (unsigned i = 0, len = Varcs->len; i < len; ++i) {
      S_CHECK(dfs_equvar(empdag, mpid2uid(Vlist[i].mpid_child), vars, equs));
   }

   return OK;
}

static int dfs_equ(EmpDag *empdag, daguid_t uid, struct aequ_list *equs)
{
   daguid_t * restrict children;
   struct VFedges * restrict Varcs;
   unsigned num_children;

   if (uidisMP(uid)) {
      mpid_t id = uid2id(uid);
      UIntArray *Carcs = &empdag->mps.Carcs[id];
      children = Carcs->arr;
      num_children = Carcs->len;

      Varcs = &empdag->mps.Varcs[id];

      MathPrgm *mp = empdag->mps.arr[id];

      IdxArray mpequs = mp->equs;
      Aequ e;
      aequ_setlist(&e, mpequs.len, mpequs.arr);
      S_CHECK(aequ_list_add(equs, e));

   } else {

      nashid_t id = uid2id(uid);
      UIntArray *mpe_children = &empdag->nashs.arcs[id];
      children = mpe_children->arr;
      num_children = mpe_children->len;

      assert(num_children > 0);

      Varcs = NULL;
   }


   for (unsigned i = 0; i < num_children; ++i) {
      S_CHECK(dfs_equ(empdag, children[i], equs));
   }

   if (!Varcs) { return OK; }

   struct rhp_empdag_arcVF *Vlist = Varcs->arr;
   for (unsigned i = 0, len = Varcs->len; i < len; ++i) {
      S_CHECK(dfs_equ(empdag, mpid2uid(Vlist[i].mpid_child), equs));
   }

   return OK;
}


static void subdag_freedata(void *data)
{
   if (!data) { return; }
   FilterEmpDagSubDag *dat = data;
   filter_subset_freedata(dat->fs);
   free(dat);
}

static void subdag_get_sizes(void *data, size_t* n, size_t* m)
{
   FilterEmpDagSubDag *dat = data;
   filter_subset_size(dat->fs, n, m);
}

static bool subdag_keep_var(void *data, rhp_idx vi)
{
   FilterEmpDagSubDag *dat = data;
   return filter_subset_var(dat->fs, vi);
}

static bool subdag_keep_equ(void *data, rhp_idx ei)
{
   FilterEmpDagSubDag *dat = data;
   return filter_subset_equ(dat->fs, ei);
}

static bool subdag_keep_var_parentfops(void *data, rhp_idx vi)
{
   FilterEmpDagSubDag *dat = data;
   Fops *fops_up = dat->parent;
   return filter_subset_var(dat->fs, vi) && fops_up->keep_var(fops_up->data, vi);
}

static bool subdag_keep_equ_parentfops(void *data, rhp_idx ei)
{
   FilterEmpDagSubDag *dat = data;
   Fops *fops_up = dat->parent;
   return filter_subset_equ(dat->fs, ei) && fops_up->keep_equ(fops_up->data, ei);
}

static int subdag_transform_gamsopcode(void *data, rhp_idx ei, unsigned len,
                                       int * restrict instrs,
                                       int * restrict args)
{
   FilterEmpDagSubDag *dat = data;
   return filter_subset_gamsopcode(&dat->fs, ei, len, instrs, args);
}
Fops* fops_subdag_new(Model *mdl, daguid_t uid)
{
   int status = OK;
   Fops *fops;
   MALLOC_NULL(fops, Fops, 1);
   fops->type = FopsEmpDagSubDag;

   EmpDag *empdag = &mdl->empinfo.empdag;

   struct avar_list vars;
   struct aequ_list equs;
   avar_list_init(&vars);
   aequ_list_init(&equs);

   S_CHECK_EXIT(dfs_equvar(empdag, uid, &vars, &equs));
   assert(vars.list && equs.list);

   struct mp_descr mp_d;
   mp_descr_invalid(&mp_d);

   FilterSubset* fs;
   A_CHECK_EXIT(fs, filter_subset_new(vars.len, vars.list, equs.len, equs.list, &mp_d));

   fs->ctr_src = &mdl->ctr;
   fs->nlpoolvars.offset_vars_pool = UINT_MAX;

   FilterEmpDagSubDag *dat;
   MALLOC_EXIT(dat, FilterEmpDagSubDag, 1);
   fops->data = dat;

   dat->fs = fs;
   dat->parent = mdl->ctr.fops;
   dat->subdag_root = uid;

   fops->freedata = subdag_freedata;
   fops->get_sizes = &subdag_get_sizes;
   if (dat->parent) {
      fops->keep_var = &subdag_keep_var_parentfops;
      fops->keep_equ = &subdag_keep_equ_parentfops;
   } else {
      fops->keep_var = &subdag_keep_var;
      fops->keep_equ = &subdag_keep_equ;
   }
   fops->vars_permutation = NULL;
   fops->transform_gamsopcode = &subdag_transform_gamsopcode;
 
_exit:
   avar_list_free(&vars);
   aequ_list_free(&equs);
   
   if (status != OK) {
      free(fops);
      return NULL;
   }

   return fops;
}
static void subdag_active_get_sizes(void *data, size_t* n, size_t* m)
{
   FilterEmpDagSubDag *dat = data;
   Container *ctr = dat->fs->ctr_src;
   *n = (size_t)ctr->n;

   *m = dat->fs->equs.size;
}

static bool subdag_active_keep_var(void *data, rhp_idx vi)
{
   FilterEmpDagSubDag *dat = data;
   return rctr_active_var(dat->fs->ctr_src, vi);
}


Fops* fops_subdag_activevars_new(Model *mdl, daguid_t uid)
{
   int status = OK;
   Fops *fops;
   MALLOC_NULL(fops, Fops, 1);
   fops->type = FopsEmpDagSubDag;

   EmpDag *empdag = &mdl->empinfo.empdag;

   struct aequ_list equs;
   aequ_list_init(&equs);

   S_CHECK_EXIT(dfs_equ(empdag, uid, &equs));
   assert(equs.list);

   struct mp_descr mp_d;
   mp_descr_invalid(&mp_d);

   FilterSubset* fs;
   A_CHECK_EXIT(fs, filter_subset_new(0, NULL, equs.len, equs.list, &mp_d));

   fs->ctr_src = &mdl->ctr;
   fs->nlpoolvars.offset_vars_pool = UINT_MAX;

   FilterEmpDagSubDag *dat;
   MALLOC_EXIT(dat, FilterEmpDagSubDag, 1);
   fops->data = dat;

   dat->fs = fs;
   dat->parent = mdl->ctr.fops;
   dat->subdag_root = uid;

   fops->data = dat;
   fops->freedata = &subdag_freedata;
   fops->get_sizes = &subdag_active_get_sizes;
   fops->keep_var = &subdag_active_keep_var;

   if (dat->parent) {
      fops->keep_equ = &subdag_keep_equ_parentfops;
   } else {
      fops->keep_equ = &subdag_keep_equ;
   }
   fops->vars_permutation = NULL;
   fops->transform_gamsopcode = &subdag_transform_gamsopcode;
//   fops->deactivatedequslen = &filter_subset_deactivatedequslen;
   //fops->transform_lequ = &filter_subset_lequ;
   //fops->transform_nltree = &filter_subset_nltree;

_exit:
   aequ_list_free(&equs);
   
   if (status != OK) {
      free(fops);
      return NULL;
   }

   return fops;
}


int fops_setvarpermutation(Fops *fops, rhp_idx *vperm)
{
   switch(fops->type) {
   case FopsActive:
      return filter_active_setvarpermutation(fops, vperm);
   case FopsSubset:
      return filter_subset_setvarpermutation(fops, vperm);
   default:
      error("%s :: unknown fops type %u", __func__, fops->type);
      return Error_RuntimeError;
   }


}

mpid_t fops_singleMP_getmpid(Fops *fops)
{
   if (fops->type != FopsEmpDagSingleMp) {
      error("[fops] ERROR in %s: expecting %s, got %s\n", __func__,
            fopstype2str(FopsEmpDagSingleMp), fopstype2str(fops->type));
      return MpId_NA;
   }

   FilterEmpDagSingleMp *dat = (FilterEmpDagSingleMp *)fops->data;
   return dat->mpid;
}

const MpIdArray* fops_Nash_getmpids(Fops *fops)
{
   if (fops->type != FopsEmpDagNash) {
      error("[fops] ERROR in %s: expecting %s, got %s\n", __func__,
            fopstype2str(FopsEmpDagNash), fopstype2str(fops->type));
      return NULL;
   }

   FilterEmpDagNash *dat = (FilterEmpDagNash *)fops->data;
   return &dat->mpids;
}

daguid_t fops_subdag_getrootuid(Fops *fops)
{
   if (fops->type != FopsEmpDagSubDag) {
      error("[fops] ERROR in %s: expecting %s, got %s\n", __func__,
            fopstype2str(FopsEmpDagNash), fopstype2str(fops->type));
      return EMPDAG_UID_NONE;
   }

   FilterEmpDagSubDag *dat = (FilterEmpDagSubDag *)fops->data;
   return dat->subdag_root;
}

Fops* fops_getparent(Fops *fops)
{
   switch (fops->type) {
   case FopsEmpDagSubDag: return ((FilterEmpDagSubDag*)fops->data)->parent;
   case FopsEmpDagSingleMp: return ((FilterEmpDagSingleMp*)fops->data)->parent;
   case FopsEmpDagNash: return ((FilterEmpDagNash*)fops->data)->parent;
   default: return NULL;
   }
}

static bool filter_empdag_singleMP_equ(void *data, rhp_idx ei)
{
   FilterEmpDagSingleMp *dat = data;

   return rhp_idx_findsorted(dat->mp_equs, ei) != UINT_MAX;

}

static void singleMP_get_sizes(void *data, size_t* n, size_t* m)
{
   FilterEmpDagSingleMp *dat = data;
   Container *ctr = dat->ctr;
   *n = (size_t)ctr->n;

   *m = dat->mp_equs->len;
}

Fops *fops_singleMP_activevars_new(Model *mdl, mpid_t mpid)
{
   Fops *fops = NULL;

   Fops *fops_mdl = mdl->ctr.fops;

   if (!fops_mdl && mdl_is_rhp(mdl)) {
      size_t total_n = ctr_nvars_total(&mdl->ctr);

      if (total_n != mdl->ctr.n) {
         SN_CHECK(rmdl_ensurefops_activedefault(mdl));
      }
   }

   CALLOC_NULL(fops, Fops, 1);

   fops->type = FopsEmpDagSingleMpAllVars;

   MALLOC_EXIT_NULL(fops->data, FilterEmpDagSingleMp, 1);
   FilterEmpDagSingleMp *dat = fops->data;

   dat->mpid = mpid;

   fops->freedata = &filter_active_freedata;
   //fops->deactivatedequslen = &filter_active_deactivatedequslen;
   fops->get_sizes = &singleMP_get_sizes;
   fops->keep_var = &filter_active_var;
   fops->keep_equ = &filter_empdag_singleMP_equ;
   fops->vars_permutation = NULL;
   fops->transform_gamsopcode = &perm_active_gamsopcode;
   //fops->transform_lequ = &filter_active_lequ;
   //fops->transform_nltree = &filter_active_nltree;

   return fops;

_exit:
   if (fops) {
      if (fops->data) {
         fops->freedata(fops->data);
      }
      free(fops);
   }

   return NULL;
}
