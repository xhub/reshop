#include "filter_ops.h"
#include "reshop_config.h"

#include <float.h>
#include <math.h>
#include <stdio.h>

#include "cmat.h"
#include "cones.h"
#include "container.h"
#include "ctr_rhp.h"
#include "ctrdat_rhp.h"
#include "ctrdat_rhp_priv.h"
#include "equ.h"
#include "equvar_helpers.h"
#include "equvar_metadata.h"
#include "internal_model_common.h"
#include "lequ.h"
#include "macros.h"
#include "nltree.h"
#include "printout.h"
#include "reshop.h"
#include "status.h"
#include "var.h"

#ifndef RHP_NO_GAMS
#include "gams_rosetta.h"
#endif

//#define RHP_DEBUG
//#define DEBUG_MR

/**
 * Reserve space for the equation / variables pairs
 *
 * @param ctr     the container
 * @param size    the amount of pairs to reserve
 *
 * @return        the error code
 */
int rctr_reserve_eval_equvar(Container *ctr, unsigned size)
{
   assert(ctr_is_rhp(ctr));
   RhpContainerData *cdat = (RhpContainerData *)ctr->data;
   struct equvar_eval *equvar_evals = &cdat->equvar_evals[cdat->current_stage];

   if (equvar_evals->max <= equvar_evals->len + size) {
      equvar_evals->max = MAX(2*equvar_evals->max, equvar_evals->len + size + 2);
      /*  We may be off by 1 --xhub */
      REALLOC_(equvar_evals->pairs, struct equvar_pair,  equvar_evals->max);
   }

   return OK;
}

/**
 * @brief Add a variable to evaluate via a given equation
 *
 * @param ctr   the container
 * @param ei    the equation to use
 * @param vi    the variable to evaluate
 *
 * @return      the error code
 */
int rctr_add_eval_equvar(Container *ctr, rhp_idx ei, rhp_idx vi)
{
   assert(ctr_is_rhp(ctr) && valid_ei(ei) && valid_vi(vi));
   RhpContainerData *cdat = (RhpContainerData *)ctr->data;
   struct equvar_eval *equvar_evals = &cdat->equvar_evals[cdat->current_stage];

   if (equvar_evals->max <= equvar_evals->len) {
      equvar_evals->max = MAX(2*equvar_evals->max, equvar_evals->len + 10);
      /*  We may be off by 1 --xhub */
      REALLOC_(equvar_evals->pairs, struct equvar_pair,  equvar_evals->max);
   }
   struct equvar_pair *pairs = equvar_evals->pairs;

   if (!equvar_evals->var2evals) {
      CALLOC_(equvar_evals->var2evals, bool, ctr_nvars_max(ctr));
   }
   bool var_already_evaluated = equvar_evals->var2evals[vi];

   if (var_already_evaluated) {
      /* TODO: sorted insertion to speed up search */
      for (unsigned i = 0, len = equvar_evals->len; i < len; ++i) {
         if (pairs[i].var == vi) {
            if (ei == pairs[i].equ) { return OK; } /* Just skip */

            error("[rctr] ERROR: variable %s is already marked as evaluated via "
                  "equation %s. It cannot be also evaluated via equation %s",
                  ctr_printvarname(ctr, vi), ctr_printequname(ctr, pairs[i].equ),
                  ctr_printequname2(ctr, ei));
            return Error_RuntimeError;
         }
      }
   }

   equvar_evals->var2evals[vi] = true;

   pairs[equvar_evals->len].equ = ei;
   pairs[equvar_evals->len].var = vi;

   trace_ctr("[container] variable '%s' marked for evaluation via equation '%s'\n",
             ctr_printvarname(ctr, vi), ctr_printequname(ctr, ei));

   equvar_evals->len++;

   return OK;
}

static int cdat_resize_equs(RhpContainerData *cdat, unsigned max_m, unsigned old_max_m)
{
   unsigned diff = max_m - old_max_m;

   if (max_m == 0) {
      errormsg("[container] ERROR: cannot resize to 0.\n");
      return Error_RuntimeError;
   }

   REALLOC_(cdat->cmat.equs, CMatElt *, max_m);
   REALLOC_(cdat->equ_rosetta, struct rosetta, max_m);
   REALLOC_(cdat->equ_stage, unsigned, max_m);

   if (max_m > old_max_m) {
      memset(&cdat->cmat.equs[old_max_m], 0, diff*sizeof(CMatElt *));
      memset(&cdat->equ_rosetta[old_max_m], 0, diff*sizeof(struct rosetta));
      memset(&cdat->equ_stage[old_max_m], 0, diff*sizeof(unsigned));
   }

   if (cdat->cmat.deleted_equs) {
      REALLOC_(cdat->cmat.deleted_equs, CMatElt *, max_m);

      if (max_m > old_max_m) {
         memset(&cdat->cmat.deleted_equs[old_max_m], 0, diff * sizeof(CMatElt*));
      }
   }

   return OK;
}

static int cdat_resize_vars(RhpContainerData *cdat, unsigned max_n, unsigned old_max_n)
{
   assert(max_n > old_max_n);
   unsigned diff = max_n - old_max_n;

   REALLOC_(cdat->cmat.vars, CMatElt *, max_n);
   REALLOC_(cdat->cmat.last_equ, CMatElt *, max_n);

   memset(&cdat->cmat.vars[old_max_n], 0, diff*sizeof(CMatElt *));
   memset(&cdat->cmat.last_equ[old_max_n], 0, diff*sizeof(CMatElt *));

   if (cdat->equvar_evals && cdat->equvar_evals[cdat->current_stage].len > 0) {
      struct equvar_eval *dat = &cdat->equvar_evals[cdat->current_stage];
      REALLOC_(dat->var2evals, bool, max_n);

      memset(&dat->var2evals[old_max_n], 0, diff*sizeof(dat->var2evals[0]));
   }

   return OK;
}


int rctr_reserve_equs(Container * restrict ctr, unsigned n_equs)
{
   RhpContainerData *cdat = (RhpContainerData *)ctr->data;

   if (cdat->max_m < cdat->total_m + n_equs)
   {
      unsigned old_max_m = cdat->max_m;
      cdat->max_m = MAX(cdat->total_m + n_equs, 2*cdat->max_m);

      REALLOC_(ctr->equs, struct equ, cdat->max_m);

      for (size_t i = old_max_m; i < (size_t)cdat->max_m; ++i) {
         equ_basic_init(&ctr->equs[i]);
//         ctr->equs[i].p.cst = NAN;
      }

      if (ctr->equmeta) {
         REALLOC_(ctr->equmeta, struct equ_meta, cdat->max_m);
         for (size_t i = old_max_m; i < (size_t)cdat->max_m; ++i) {
            equmeta_init(&ctr->equmeta[i]);
         }
      }

      return cdat_resize_equs(cdat, cdat->max_m, old_max_m);
   }

   return OK;
}

int rctr_reserve_vars(Container *ctr, unsigned n_vars)
{
   RhpContainerData *cdat = (RhpContainerData *)ctr->data;

   if (cdat->max_n < cdat->total_n + n_vars)
   {
      size_t old_max_n = cdat->max_n;
      cdat->max_n = MAX(cdat->total_n + n_vars, 2*cdat->max_n);

      REALLOC_(ctr->vars, Var, cdat->max_n);

      if (ctr->varmeta) {
         REALLOC_(ctr->varmeta, struct var_meta, cdat->max_n);
         for (size_t i = old_max_n; i < (size_t)cdat->max_n; ++i) {
            varmeta_init(&ctr->varmeta[i]);
         }
      }

      return cdat_resize_vars(cdat, cdat->max_n, old_max_n);
   }

   return OK;
}

/**
 * @brief Add an equation to the model
 *
 * @param          ctr   the container object
 * @param[out]     ei    on output contains the equation index.
 * @param[out]     e     the equation structure
 * @param          type  the type of equation
 * @param          cone  the cone for the equation (CONE_NONE for mappings or booleans)
 *
 * @return               the error code
 */
int rctr_add_equ_empty(Container *ctr, rhp_idx *ei, Equ **e, EquObjectType type,
                       enum cone cone)
{
   RhpContainerData * restrict cdat = (RhpContainerData *)ctr->data;

   if (cdat->max_m <= cdat->total_m) {
      if (cdat->strict_alloc) {
         error("%s :: no more space for equation! Call ctrdat_reserve_equs before!\n",
               __func__);
         return Error_IndexOutOfRange;
      }

      S_CHECK(rctr_reserve_equs(ctr, 1));
   }

   rhp_idx ei_ = cdat->total_m;
   *ei = ei_;
   Equ *e_ = &ctr->equs[ei_];

   if (e) {
      *e = e_;
   }

   S_CHECK(cdat_equ_init(cdat, ei_));

   /* ----------------------------------------------------------------------
    * Initialize the equ struct: this must not be an inhereted function
    * ---------------------------------------------------------------------- */
   assert(!cdat_equ_isinherited(cdat, ei_));

   equ_basic_init(e_);
   e_->idx = ei_;
   e_->object = type;
   e_->cone = cone;
   e_->tree = NULL;
   A_CHECK(e_->lequ, lequ_new(0));

   /* \TODO(xhub) This has to be documented */
   cdat->total_m++;
   ctr->m++;

   return OK;

}

/**
 * @brief Add an equation to the model
 *
 * @param          ctr   the container object
 * @param          ei    on input and if positive, indicates the index where
 *                       the new equation should be stored. If negative, then
 *                       on output contains the equation index.
 * @param type           the type of equation
 * @param cone           the cone for the equation (CONE_NONE for mappings or booleans)
 *
 * @return               the error code
 */
int rctr_init_equ_empty(Container *ctr, rhp_idx ei, EquObjectType type,
                        enum cone cone)
{
   RhpContainerData * restrict ctrdat = (RhpContainerData *)ctr->data;

   S_CHECK(ei_inbounds(ei, ctrdat->max_m, __func__))

   Equ *e = &ctr->equs[ei];

   S_CHECK(cdat_equ_init(ctrdat, ei))

   /* ----------------------------------------------------------------------
    * Initialize the equ struct: this must not be an inhereted function
    * ---------------------------------------------------------------------- */
   assert(!cdat_equ_isinherited(ctrdat, ei));

   equ_basic_init(e);
   e->idx = ei;
   e->object = type;
   e->cone = cone;
   e->tree = NULL;
   A_CHECK(e->lequ, lequ_new(0));

   /* \TODO(xhub) This has to be documented */
   ctr->m++;

   return OK;
}

/**
 * @brief Perform some internal equation initialization
 *
 * @param cdat  the model representation
 * @param ei     the equation index
 *
 * @return       the error code
 */
int cdat_equ_init(RhpContainerData *cdat, rhp_idx ei)
{
   cdat->equ_stage[ei] = cdat->current_stage;
   cdat->equ_rosetta[ei].ppty = EQU_PPTY_NONE;
   cdat->equ_rosetta[ei].res.equ = ei;

   return OK;
}

/**
 * @brief Allocate enough space to accommodate equations and variables
 *
 * @param cdat   the container data
 * @param max_n  the number of variables
 * @param max_m  the number of equations
 *
 * @return       the error code
 */
int cdat_resize(RhpContainerData *cdat, unsigned max_n, unsigned max_m)
{
   unsigned old_max_n = cdat->max_n;
   unsigned old_max_m = cdat->max_m;

   cdat->max_n = max_n;
   cdat->max_m = max_m;

   if (max_m > 0 && max_m > old_max_m) {
      S_CHECK(cdat_resize_equs(cdat, max_m, old_max_m));
   } else if (max_m == 0) {
      FREE(cdat->cmat.equs);
      FREE(cdat->equ_rosetta);
      FREE(cdat->equ_stage);
   }

   if (max_n > 0 && max_n > old_max_n) {
       S_CHECK(cdat_resize_vars(cdat, max_n, old_max_n));
   } else if (max_n == 0) {
      FREE(cdat->cmat.vars);
      FREE(cdat->cmat.last_equ);
   }


   return OK;
}

int cdat_init(Container *ctr, unsigned max_n, unsigned max_m)
{
   RhpContainerData *cdat;

   CALLOC_(cdat, RhpContainerData, 1);

   S_CHECK(cmat_init(&cdat->cmat));

   ctr->data = cdat;

   cdat->n = &ctr->n;
   cdat->m = &ctr->m;

   cdat->total_n = 0;
   cdat->total_m = 0;

   /* We need to set these values before calling the resize */
   cdat->max_n = 0;
   cdat->max_m = 0;

   S_CHECK(cdat_resize(cdat, max_n, max_m));

   cdat->current_stage = 0;

   cdat->pp.remove_objvars = 0;

   cdat->mem2free = NULL;

   cdat->borrow_inherited = true;

   /* By default, we are not that strict. For the internal transformation, we are */
   cdat->strict_alloc = false;

   MALLOC_(cdat->stage_auxmdl, struct auxmdl, 1);
   cdat->stage_auxmdl[cdat->current_stage].len = 0;
   cdat->stage_auxmdl[cdat->current_stage].max = 0;
   cdat->stage_auxmdl[cdat->current_stage].filter_subset = NULL;

   cdat->equvar_evals_size = 3;
   CALLOC_(cdat->equvar_evals, struct equvar_eval, cdat->equvar_evals_size);

   if (ctr->backend == RhpBackendReSHOP) {
      cdat->var_names.v.type = VNAMES_UNSET;
      cdat->var_names.v.next = NULL;
      cdat->equ_names.v.type = VNAMES_UNSET;
      cdat->equ_names.v.next = NULL;
   }

   avar_init(&cdat->var_inherited.v);
   avar_init(&cdat->var_inherited.v_src);
   aequ_init(&cdat->equ_inherited.e);
   aequ_init(&cdat->equ_inherited.e_src);

   /* We always allocate a block, to simplify the code */
   aequ_setblock(&cdat->equname_inherited.e, 2);
   aequ_setblock(&cdat->equname_inherited.e_src, 2);

   return OK;
}


int cdat_dealloc(Container *ctr, RhpContainerData* cdat)
{
   if (!cdat) return OK;

   size_t inherited_equ_start, inherited_equ_stop;
   bool delete_equ_trees = false;
   struct e_inh *e_inh = &cdat->equ_inherited;

   /* ---------------------------------------------------------------------
    * Set the range of equations that will be freed.
    * If we borrow inherited equations, we do not free that range. Otherwise
    * we set the range to be the maximal range.
    * --------------------------------------------------------------------- */

   /* TODO GITLAB #109 */
   if (e_inh->e.size > 0) {
      assert(e_inh->e.type == EquVar_Compact);

      if (!cdat->borrow_inherited) {
         inherited_equ_start = SIZE_MAX;
         inherited_equ_stop = 0;
      } else {
         inherited_equ_start = e_inh->e.start;
         inherited_equ_stop = inherited_equ_start + e_inh->e.size;
      }

      if (ctr->ctr_up->backend == RhpBackendGamsGmo) {
        /* TODO(xhub) This is obscure  */
         delete_equ_trees = true;
      }
   } else {
      inherited_equ_start = SIZE_MAX;
      inherited_equ_stop = 0;
  }

  /* ------------------------------------------------------------------------
   * Free the inherited data structure
   * ------------------------------------------------------------------------ */
   aequ_empty(&e_inh->e_src);
   aequ_empty(&cdat->equname_inherited.e_src);
   aequ_empty(&cdat->equname_inherited.e);
   avar_empty(&cdat->var_inherited.v_src);
 
   for (size_t i = 0, len = cdat->total_m; i < len; ++i) {
      if (i < inherited_equ_start || i >= inherited_equ_stop) {
         equ_free(&ctr->equs[i]);
      }
   }


   /* ----------------------------------------------------------------------
    * Delete the nltree for equations coming from the GAMS model
    *
    * FIXME(xhub) clarify ownership here ...
    * ---------------------------------------------------------------------- */

   if (delete_equ_trees) {
      //for (size_t i = inherited_equ_start; i < inherited_equ_stop; ++i) {
      //   nltree_dealloc(ctr->equs[i].tree);
      //   ctr->equs[i].tree = NULL;
      //}
   }

   FREE(ctr->equs);

   for (size_t s = 0; s <= cdat->current_stage; ++s) {
      struct auxmdl *s_subctr = &cdat->stage_auxmdl[s];

      for (size_t i = 0; i < s_subctr->len; ++i) {
         filter_subset_release(s_subctr->filter_subset[i]);
      }

      FREE(s_subctr->filter_subset);
   }

   FREE(cdat->stage_auxmdl);

   /*  TODO(xhub) URG this should not depend on the model type */
   if (ctr->backend == RhpBackendReSHOP) {
     /* The usage of vnames is just to circumvent some compiler warnings  */
     vnames_freefrommdl(&cdat->var_names.v);
     vnames_freefrommdl(&cdat->equ_names.v);

   } else if (ctr->backend == RhpBackendJulia) {
     /*  TODO(xhub) URG this looks broken */
      assert(cdat->var_names.s.names || cdat->var_names.s.max == 0);
      assert(cdat->equ_names.s.names || cdat->equ_names.s.max == 0);

      for (size_t i = 0; i < cdat->var_names.s.max; ++i) {
         FREE(cdat->var_names.s.names[i]);
      }
      FREE(cdat->var_names.s.names);
      cdat->var_names.s.max = 0;

      for (size_t i = 0; i < cdat->equ_names.s.max; ++i) {
         FREE(cdat->equ_names.s.names[i]);
      }
      FREE(cdat->equ_names.s.names);
      cdat->equ_names.s.max = 0;
   } else {
      error("%s :: don't know how to deallocate names for backend %s\n",
            __func__, backend2str(ctr->backend));
   }

   /* ------------------------------------------------------------------------
   * Free all the loose bits of memory
   * ------------------------------------------------------------------------ */
   if (cdat->mem2free) {
      rctrdat_mem2free(cdat->mem2free);
      FREE(cdat->mem2free);
   }

   FREE(cdat->equ_rosetta);
   FREE(cdat->equ_stage);

   for (unsigned i = 0; i < cdat->equvar_evals_size; ++i) {
      FREE(cdat->equvar_evals[i].var2evals);
      FREE(cdat->equvar_evals[i].pairs);
   }

   for (size_t i = 0; i < cdat->sos1.len; ++i) {
      avar_empty(&cdat->sos1.groups[i].v);
      FREE(cdat->sos1.groups[i].w);
   }
   FREE(cdat->sos1.groups);

   for (size_t i = 0; i < cdat->sos2.len; ++i) {
      avar_empty(&cdat->sos2.groups[i].v);
      FREE(cdat->sos2.groups[i].w);
   }
   FREE(cdat->sos2.groups);

   FREE(cdat->equvar_evals);

   cmat_fini(&cdat->cmat);

   free(cdat);

   return OK;
}

/**
 * @brief Start to use a given basename for variables
 *
 * @warning this function takes ownership of the string given as argument.
 * That is, this will be passed as argument to free.
 *
 * @param cdat  model representation
 * @param name   "basename" for the variable. This function takes ownership of the pointer
 *
 * @return       the error code
 */
int cdat_varname_start(RhpContainerData* cdat, char *name)
{
  struct vnames *vnames;
  A_CHECK(vnames, vnames_getregular(&cdat->var_names.v));
  struct vnames_list *l;
  A_CHECK(l, vnames->list);

  if (l->active) {
    error("%s ERROR: a variable name is already active\n", __func__);
    free(name);
    return Error_Inconsistency;
  }

   if (!valid_vi(vnames->start)) {
    vnames->start = cdat->total_n;
  }

  return vnames_list_start(l, cdat->total_n, name);
}

int cdat_varname_end(RhpContainerData* cdat)
{
  struct vnames *vnames;
  A_CHECK(vnames, vnames_getregular(&cdat->var_names.v))
  struct vnames_list *l;
  A_CHECK(l, vnames->list);

   if (!l->active) {
      error("%s ERROR: call without an active variable name\n", __func__);
      return Error_Inconsistency;
   }

   rhp_idx end_idx = cdat->total_n-1;
   vnames->end = end_idx;
   vnames_list_stop(l, end_idx);

   return OK;
}

int cdat_equname_start(RhpContainerData* cdat, char *name)
{
  struct vnames *vnames;
  struct vnames_list *l;

   /* ---------------------------------------------------------------------
    * Get the last regular name (this creates one if needed)
    * --------------------------------------------------------------------- */

   A_CHECK(vnames, vnames_getregular(&cdat->equ_names.v));
   A_CHECK(l, vnames->list);

   if (l->active) {
      error("%s ERROR: an equation name is already active\n", __func__);
      free(name);
      return Error_Inconsistency;
   }

   /* Only Initialize if new sequence */
   if (!valid_ei(vnames->start)) {
      vnames->start = cdat->total_m;
   }

   vnames->end = IdxMaxValid; /* This allows for useful debug messages */

   return vnames_list_start(l, cdat->total_m, name);
}

int cdat_equname_end(RhpContainerData* cdat)
{
   struct vnames *vnames;
   A_CHECK(vnames, vnames_getregular(&cdat->equ_names.v))
   struct vnames_list *l;
   A_CHECK(l, vnames->list);

   if (!l->active) {
      error("%s :: call without an active equation name\n", __func__);
      return Error_Inconsistency;
   }

   rhp_idx ei_end = cdat->total_m-1;
   vnames->end = ei_end;
   vnames_list_stop(l, ei_end);

   return OK;
}

int cdat_add_subctr(RhpContainerData* cdat, struct filter_subset* fs)
{
   struct auxmdl *s_subctr = &cdat->stage_auxmdl[cdat->current_stage];

   if (s_subctr->len >= s_subctr->max) {
      s_subctr->max = MAX(2*s_subctr->max, s_subctr->len+1);
      REALLOC_(s_subctr->filter_subset, struct filter_subset*, s_subctr->max);
   }

   s_subctr->filter_subset[s_subctr->len++] = fs;

   return OK;
}

/** 
 *  @brief Iterate over the variables of an equation
 *
 *  This function exists since ctr_getrowjacinfo() operates only on active
 *  equations. Therefore, use this function only if it is needed to operate over
 *  all equations (active or deleted)
 *
 *  @param         ctr     the container
 *  @param         ei      index of the equation
 *  @param[in,out] iterator     a pointer to the next variable. Must be set to NULL
 *                         on the first call. On output, points to the next
 *                         variable (or NULL is there is none).
 *  @param[out]    val     the (linearized) value of the variable in the
 *                         equation
 *  @param[out]    vi      the variable
 *  @param[out]    nlflag  indicates whether the variable is NL
 *
 *  @return                the error code
 */
int rctr_walkallequ(const Container *ctr, rhp_idx ei, void **iterator, double *val,
                    rhp_idx *vi, bool *nlflag)
{
   assert(ctr->backend == RhpBackendReSHOP);
   const RhpContainerData *cdat = (const RhpContainerData *)ctr->data;

   CMatElt *cme = (CMatElt*)*iterator;
   assert(!cme || (valid_ei(cme->ei) && cme->ei == ei &&
            "In model_walkallequ :: unconsistency between jacptr and rowidx"));

   /* ----------------------------------------------------------------------
    * If the equation is still active, use that one
    * If it has been deleted, look through the deleted equations
    * ---------------------------------------------------------------------- */

   if (!cme) {
      if (cdat->cmat.equs[ei]) {
         cme = cdat->cmat.equs[ei];
      } else if (cdat->cmat.deleted_equs[ei]){
         cme = cdat->cmat.deleted_equs[ei];
      }
   }

   if (!cme) {
      error("[container] ERROR: no equation with index %u exists!\n", ei);
      return Error_NotFound;
   }

   *iterator = cme->next_var;
   *val = cme->value;
   *vi = cme->vi;
   *nlflag = cme_isNL(cme);

   return OK;
}


