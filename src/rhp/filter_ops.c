#include <float.h>
#include <math.h>

#include "cmat.h"
#include "container.h"
#include "equ.h"
#include "equvar_helpers.h"
#include "mdl.h"
#include "nltree.h"
#include "nltree_priv.h"
#include "filter_ops.h"
#include "gams_utils.h"
#include "instr.h"
#include "lequ.h"
#include "macros.h"
#include "mathprgm.h"
#include "mdl_rhp.h"
#include "ctrdat_rhp.h"
#include "printout.h"
#include "var.h"

struct idx_list {
   unsigned size;           /**< current number of pointer stored        */
   unsigned max;            /**< maximum size of the arrays              */
   rhp_idx *pool_idx;       /**< array of pointers to the pool index     */
   rhp_idx *vars;           /**< variable indices                        */
};

typedef struct filter_deactivated {
   IdxArray vars;    
   IdxArray equs;
   Avar v;
   Aequ e;
} FilterDeactivated;

/** filter based on a subset of variables and equations */
typedef struct filter_subset {
   unsigned offset_vars_pool;      /**< Start in the pool for the var values  */
   Container *ctr_src;             /**< Source container                      */
   struct idx_list neg_var_vals;   /**< List of negative of variables         */
   struct mp_descr descr;          /**< Description of the math programm      */
   Avar vars;                      /**< Variables to be included              */
   Aequ equs;                      /**< Equations to be included              */
   rhp_idx *vars_permutation;      /**< Optional variable permutation         */
} FilterSubset;

typedef struct filter_active {
   FilterDeactivated deactivated;  /**< Deactivated                           */
   Container *ctr;                 /**< Container                             */
   rhp_idx *vars_permutation;      /**< Optional variable permutation         */
} FilterActive;


const char *fopstype_name(FopsType type)
{
   switch (type) {
   case FopsActive: return "active";
   case FopsSubset: return "subset";
   default: return "error unknown filter ops type";
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

static unsigned filter_active_deactivatedequslen(void *data)
{
   FilterActive *dat = (FilterActive *)data;
   FilterDeactivated *deactivated = &dat->deactivated;

   return deactivated->equs.len + aequ_size(&deactivated->e);
}

static inline bool rhpctrdat_active_var(const Container *ctr, rhp_idx vi)
{
   RhpContainerData *ctrdat = ctr->data;
   return ctrdat->vars[vi];
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

static int filter_active_lequ(void *data, Equ *e, Equ *edst)
{
   FilterActive *dat = (FilterActive *)data;
   Container *ctr = dat->ctr;
   rhp_idx * restrict rosetta_vars = ctr->rosetta_vars;
   Lequ *le = e->lequ;
   size_t len;
   if (le && le->len > 0) {
      len = le->len;
   } else {
      return OK;
   }

   rhp_idx * restrict idx = le->vis;
   double * restrict vals = le->coeffs;

   if (!edst->lequ) { A_CHECK(edst->lequ, lequ_alloc(len)); }
   Lequ * lequ = edst->lequ;

   for (unsigned i = 0; i < len; ++i) {
      rhp_idx vi_new = rosetta_vars[idx[i]];
      double val = vals[i];

      if (valid_vi(vi_new) && isfinite(val)) {
         lequ_add(lequ, vi_new, val); /* no check as we do not resize */
      } else {
         if (!isnan(val)) {
            error("%s :: In an equation, the deleted variable %d has a value %e\n",
                  __func__, idx[i], val);
         }
      }
   }

   return OK;
}

/* -------------------------------------------------------------------------
 * Change the variable indices in the GAMS code
 * ------------------------------------------------------------------------- */

static int filter_active_gamsopcode(void *data, rhp_idx ei, int * restrict instrs,
                                    int * restrict args, unsigned len)
{
   FilterActive *dat = (FilterActive *)data;
   Container *ctr = dat->ctr;
   rhp_idx * restrict rosetta_vars = ctr->rosetta_vars;

   for (unsigned i = 0; i < (unsigned)len; ++i) {
      if (gams_get_optype(instrs[i]) == NLNODE_OPARG_VAR) {
         assert(valid_vi(rosetta_vars[args[i]-1]));
         args[i] = 1 + rosetta_vars[args[i]-1];
      }
   }

   /* Update the nlStore field to match the new equation index */
   assert(instrs[len-1] == nlStore);
   args[len-1] = 1 + ei;

   return OK;
}

static int filter_active_nltree(void *data, Equ *e, Equ *edst)
{
   FilterActive *dat = (FilterActive *)data;
   Container *ctr = dat->ctr;
   rhp_idx *rosetta_vars = ctr->rosetta_vars;

   A_CHECK(edst->tree, nltree_dup_rosetta(e->tree, rosetta_vars));

   return OK;
}

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

static int _get_neg_rm_var(FilterSubset *fs, rhp_idx vi)
{
   struct idx_list *list = &fs->neg_var_vals;
   Container *ctr = fs->ctr_src;

   for (unsigned i = 0; i < list->size; ++i) {
      if (list->vars[i] == vi) {
         return list->pool_idx[i];
      }
   }

   if (list->size >= list->max) {
      list->max = MAX(list->size+1, list->max);
      REALLOC_(list->pool_idx, rhp_idx, list->max);
      REALLOC_(list->vars, rhp_idx, list->max);
   }

   rhp_idx idx;
   if (rhpctrdat_active_var(ctr, vi)) {
      idx = rctr_poolidx(ctr, -ctr->vars[vi].value);
   } else {
      idx = rctr_poolidx(ctr, 0.);
   }

   list->vars[list->size] = vi;
   list->pool_idx[list->size++] = idx;

   return idx;
}

static bool filter_subset_var(void *data, rhp_idx vi)
{
   assert(valid_vi(vi));

   FilterSubset* fs = (FilterSubset *)data;
   const Container * restrict ctr_src = fs->ctr_src;
   RhpContainerData * restrict ctrdat = fs->ctr_src->data;
   bool is_active = ctr_is_rhp(ctr_src) ? ctrdat->vars[vi] != NULL : true;

   return avar_contains(&fs->vars, vi) &&
         !avar_contains(ctr_src->fixed_vars, vi) &&
          is_active; /* TODO: GITLAB #107 */
}

static bool filter_subset_equ(void *data, rhp_idx ei)
{
   assert(valid_ei(ei));

   FilterSubset* fs = (FilterSubset *)data;
   const Container * restrict ctr_src = fs->ctr_src;
   RhpContainerData * restrict ctrdat = fs->ctr_src->data;
   bool is_active = ctr_is_rhp(ctr_src) ? ctrdat->equs[ei] != NULL : true;

   /* TODO: GITLAB #107 */
   return is_active && aequ_contains(&fs->equs, ei);
}

static size_t subset_nvars_rhp(Avar *v, Avar *fixed_vars, CMatElt ** vars)
{
   size_t nvars = 0;

   for (unsigned i = 0, len = v->size; i < len; ++i) {
      rhp_idx vi = avar_fget(v, i);
      if (!avar_contains(fixed_vars, vi) && vars[vi]) { nvars++; }
   }

   return nvars;
}

static size_t subset_nequs_rhp(Aequ *e, CMatElt ** equs)
{
   size_t nequs = 0;

   for (unsigned i = 0, len = e->size; i < len; ++i) {
      rhp_idx vi = aequ_fget(e, i);
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
   RhpContainerData * restrict ctrdat = fs->ctr_src->data;
   
   size_t nvars, nequs;

   if (ctr_is_rhp(ctr_src)) {
      nvars = subset_nvars_rhp(v, ctr_src->fixed_vars, ctrdat->vars);
      nequs = subset_nequs_rhp(e, ctrdat->equs);
   } else {
      nvars = avar_size(v) - avar_size(fs->ctr_src->fixed_vars);
      nequs = aequ_size(&fs->equs);
   }

   *ctr_n = nvars;
   *ctr_m = nequs;
}

static int filter_subset_lequ(void *data, Equ *e, Equ *edst)
{
   FilterSubset *fs = (FilterSubset *)data;
   Container *ctr = fs->ctr_src;

   Lequ *lequ;
   Lequ *le = e->lequ;
   size_t len;

   if (le && le->len > 0) {
      len = le->len;
   } else {
      return OK;
   }

   rhp_idx * restrict idx = le->vis;
   double * restrict vals = le->coeffs;

   rhp_idx * restrict rosetta_vars = ctr->rosetta_vars;
   Var * restrict ctr_vars = ctr->vars;

   if (!edst->lequ) {
      A_CHECK(edst->lequ, lequ_alloc(len));
      lequ = edst->lequ;
   } else {
      lequ = edst->lequ;
   }

   for (unsigned i = 0; i < len; ++i) {
      rhp_idx vi = idx[i];
      rhp_idx vi_new = rosetta_vars[vi];
      double val = vals[i];

      if (valid_vi(vi_new)) {
         lequ_add(lequ, vi_new, val); /* no check as we do not resize */
      } else {
         if (isfinite(val) && fabs(val) > DBL_EPSILON) {
            equ_add_cst(edst, val*ctr_vars[vi].value);
         }
      }
   }

   return OK;
}

static int filter_subset_gamsopcode(void *data, rhp_idx ei, int * restrict instrs,
                                    int * restrict args, unsigned len)
{
   FilterSubset *fs = (FilterSubset *)data;
   rhp_idx * restrict rosetta_vars = fs->ctr_src->rosetta_vars;

   /* ----------------------------------------------------------------------
    * For each variable 
    * - If the variable is still active, update the variable index
    * - If the variable is not active, we need to replace the opcode, from one
    *   that takes a variable as argument to one that takes a constant
    * ---------------------------------------------------------------------- */

   for (unsigned i = 0; i < len; ++i) {

      if (gams_get_optype(instrs[i]) == NLNODE_OPARG_VAR) {
         rhp_idx vi = args[i]-1;

         if (filter_subset_var(data, vi)) {
            args[i] = 1 + rosetta_vars[vi];
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
               args[i]   = fs->offset_vars_pool + vi;
            } else if (opcode == -1) {
               instrs[i] = nlPushI;
               args[i]   = _get_neg_rm_var(fs, vi);
            } else {
               // The function gams_opcode_var_to_cst already prints the error
               return Error_Unconsistency;
            }
         }
      }
   }

   /* Update the nlStore field to match the new equation index */
   assert(instrs[len-1] == nlStore);
   args[len-1] = 1 + ei;

   return OK;
}

static int filter_subset_nltree(void *data, Equ *e, Equ *edst)
{
  TO_IMPLEMENT("");
}

FilterSubset* fops_subset_new(unsigned vlen, Avar *vars, unsigned elen,
                              Aequ *equs, struct mp_descr* mp_d)
{
   FilterSubset *fs;

   CALLOC_NULL(fs, FilterSubset, 1);

   SN_CHECK_EXIT(avar_setblock(&fs->vars, vlen));
   SN_CHECK_EXIT(aequ_setblock(&fs->equs,  elen));

   for (unsigned i = 0; i < vlen; ++i) {
      SN_CHECK_EXIT(avar_extend(&fs->vars, &vars[i]));
   }

   for (unsigned i = 0; i < elen; ++i) {
      SN_CHECK_EXIT(aequ_extendandown(&fs->equs, &equs[i]));
   }

   fs->offset_vars_pool = UINT_MAX;

   fs->neg_var_vals.size = 0;
   fs->neg_var_vals.max = 0;
   fs->neg_var_vals.pool_idx = NULL;
   fs->neg_var_vals.vars = NULL;

   memcpy(&fs->descr, mp_d, sizeof(struct mp_descr));

   /* Optional data */
   fs->vars_permutation = NULL;

   return fs;

_exit:
   fops_subset_release(fs);
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
            __func__, fopstype_name(fops->type), fopstype_name(FopsSubset));
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

void fops_subset_release(FilterSubset *fs)
{
   if (!fs) return;

   aequ_empty(&fs->equs);
   avar_empty(&fs->vars);
   FREE(fs->neg_var_vals.pool_idx);
   FREE(fs->neg_var_vals.vars);
   FREE(fs);
}

int filter_subset_init(FilterSubset *fs, Model *mdl, unsigned offset_pool)
{
   if (!mdl_is_rhp(mdl)) {
      TO_IMPLEMENT("FilterSubset with GAMS model");
   }

   fs->ctr_src = &mdl->ctr;
   fs->offset_vars_pool = offset_pool;

   Fops filt_ops;
   fops_subset_init(&filt_ops, fs);

   S_CHECK(rmdl_setfops(mdl, &filt_ops));

   empdag_reset_type(&mdl->empinfo.empdag);
   return rmdl_set_simpleprob(mdl, &fs->descr);
}

int fops_active_init(Fops *ops, Container *ctr)
{
   ops->type = FopsActive;
   FilterActive *dat;
   MALLOC_(dat, FilterActive, 1);
   dat->ctr = ctr;
   dat->vars_permutation = NULL;
   filter_deactivated_init(&dat->deactivated);

   ops->data = dat;
   ops->freedata = &filter_active_freedata;
   ops->deactivatedequslen = &filter_active_deactivatedequslen;
   ops->get_sizes = &filter_active_size;
   ops->keep_var = &filter_active_var;
   ops->keep_equ = &filter_active_equ;
   ops->vars_permutation = NULL;
   ops->transform_lequ = &filter_active_lequ;
   ops->transform_gamsopcode = &filter_active_gamsopcode;
   ops->transform_nltree = &filter_active_nltree;

   return OK;
}

static void filter_subset_freedata(void* data) {  }
static unsigned filter_subset_deactivatedequslen(void *data) { return 0; }

void fops_subset_init(Fops *ops, FilterSubset *fs)
{
   ops->type = FopsSubset;
   ops->data = fs;
   ops->deactivatedequslen = &filter_subset_deactivatedequslen;
   ops->freedata = &filter_subset_freedata;
   ops->get_sizes = &filter_subset_size;
   ops->keep_var = &filter_subset_var;
   ops->keep_equ = &filter_subset_equ;
   ops->vars_permutation = NULL;
   ops->transform_lequ = &filter_subset_lequ;
   ops->transform_gamsopcode = &filter_subset_gamsopcode;
   ops->transform_nltree = &filter_subset_nltree;
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

      mpeid_t id = uid2id(uid);
      UIntArray *mpe_children = &empdag->mpes.arcs[id];
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
      S_CHECK(dfs_equvar(empdag, mpid2uid(Vlist[i].child_id), vars, equs));
   }

   return OK;
}

int fops_subdag_init(Fops *fops, Model *mdl, daguid_t uid)
{
   int status = OK;

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
   A_CHECK_EXIT(fs, fops_subset_new(vars.len, vars.list, vars.len, equs.list, &mp_d));

   fs->ctr_src = &mdl->ctr;
   fs->offset_vars_pool = UINT_MAX;

   fops_subset_init(fops, fs);


_exit:
   avar_list_free(&vars);
   aequ_list_free(&equs);
   
   return OK;
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
