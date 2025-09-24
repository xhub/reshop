#include "reshop_config.h"

#include <assert.h>
#include <limits.h>

#include "cmat.h"
#include "equvar_metadata.h"
#include "filter_ops.h"
#include "mdl.h"
#include "mdl_rhp.h"
#include "compat.h"
#include "container.h"
#include "ctr_gams.h"
#include "ctr_rhp.h"
#include "equ.h"
#include "equ_modif.h"
#include "nltree.h"
#include "equvar_helpers.h"
#include "macros.h"
#include "ctrdat_rhp.h"
#include "pool.h"
#include "printout.h"
#include "rctr_equ_edit_priv.h"
#include "reshop_data.h"
#include "rmdl_priv.h"
#include "timings.h"


#ifndef NDEBUG
#define CTR_DEBUG(str, ...) trace_fooc("[container] DEBUG: " str "\n", __VA_ARGS__)
#else
#define CTR_DEBUG(...)
#endif


UNUSED static size_t _pool_zero(enum rhp_backendtype type)
{
   switch (type) {
   case RhpBackendGamsGmo:
      return nlconst_zero-1;
   default:
      error("%s ERROR: not implemented for container %d\n", __func__, type);
   }
   return SSIZE_MAX;
}

int rhp_chk_ctr(const Container *ctr, const char *fn)
{
   if (!ctr_is_rhp(ctr)) {
      error("%s ERROR: the container has the wrong type: expected %d, %d or %d, got %d\n",
            fn, RhpBackendReSHOP, RhpBackendJulia, RhpBackendAmpl, ctr->backend);
      return Error_InvalidValue;
   }

   return OK;
}

void rctr_print_active_vars(Container *ctr)
{
   assert(ctr_is_rhp(ctr));
   struct ctrdata_rhp *cdat = (struct ctrdata_rhp *)ctr->data;
   size_t total_n = cdat->total_n;

   error("[container] There are %zu variables in total; %zu are active\n", total_n,
         (size_t)ctr->n);
   errormsg("\nList of active variables:\n");

   for (size_t i = 0; i < total_n; ++i) {
      if (cdat->cmat.vars[i]) {
         error("[%5zu] %s\n", i, ctr_printvarname(ctr, i));
      }
   }

   errormsg("\nList of inactive variables (not present in any equations):\n");

   for (size_t i = 0; i < total_n; ++i) {
      if (!cdat->cmat.vars[i]) {
         error("[%5zu] %s\n", i, ctr_printvarname(ctr, i));
      }
   }
}

void rctr_debug_active_equs(Container *ctr)
{
   assert(ctr_is_rhp(ctr));

   struct ctrdata_rhp *cdat = (struct ctrdata_rhp *)ctr->data;
   CMat * restrict cmat = &cdat->cmat; 
   size_t total_m = cdat->total_m;

   error("[container] There are %zu equations in total; %zu are active\n", total_m,
         (size_t)ctr->m);

   if (ctr->m > total_m) {
      error("\n[container] MAJOR BUG: there are more active equations (%zu) than "
            "reserved ones (%zu), Please report this!\n", (size_t)ctr->m, total_m);
   }

   errormsg("\nList of active equations:\n");

   for (size_t i = 0; i < total_m; ++i) {
      if (cmat->equs[i]) {
         error("[%5zu] %s\n", i, ctr_printequname(ctr, i));
      }
   }

   bool has_inactive_equ = false;
   for (size_t i = 0; i < total_m; ++i) {
      if (!cmat->equs[i]) {

         if (!has_inactive_equ) {
            errormsg("\nList of inactive equations:\n");
            has_inactive_equ = true;
         }

         error("[%5zu] %s\n", i, ctr_printequname(ctr, i));
      }
   }

   if (!has_inactive_equ) {
      errormsg("\nNo inactive equation\n");
   }
}

/**
 * @brief Ensure that the nonlinear part of the equation is readily available
 *
 * This function builds the expression tree if needed. It uses the source
 * container object to make it
 *
 * @param  ctr  the container object
 * @param  e    the equation
 *
 * @return      the error code
 */
int rctr_getnl(const Container* ctr, Equ *e)
{
   assert(ctr_is_rhp(ctr));

   rhp_idx ei = e->idx, ei_up;

   /* ---------------------------------------------------------------------
    * We exit successfully whenever:
    * - there is already a tree
    * - there is no source container
    * - the index indicate that this is a new equation
    * --------------------------------------------------------------------- */

   if (e->tree ||
       ((ei_up = cdat_ei_upstream(ctr->data, ei)) && !valid_ei(ei_up))) {
      return OK;
   }

   Container *ctr_up = ctr->ctr_up;

   BackendType backend = ctr_up->backend;
   switch (backend) {

   /* ---------------------------------------------------------------------
    * If the ancestor is GAMS, then we build the tree from the opcode
    *
    * 1. Get the opcode data from the GMO
    * 2. Build the expression tree
    * --------------------------------------------------------------------- */

   case RhpBackendGamsGmo:
   {
      int len, *instrs, *args;
      Equ *e_up = &ctr_up->equs[ei_up];

      if (e_up->tree && e_up->tree->root) {
         e->tree = e_up->tree;
         return OK;
      }

      S_CHECK(gctr_getopcode(ctr_up, ei_up, &len, &instrs, &args));
      S_CHECK(equ_nltree_fromgams(e, len, instrs, args));
      e_up->tree = e->tree;

      // HACK ARENA
      ctr_relmem_recursive_old(ctr_up);

      /* No need to free instrs or args, it comes from a container workspace */
      break;
   }
   case RhpBackendReSHOP:
   case RhpBackendJulia:
   {
      /* We know there is an upstream container: recurse and update e->tree */
      Equ *e_up = &ctr_up->equs[ei_up];
      S_CHECK(rctr_getnl(ctr_up, e_up));
      e->tree = e_up->tree;
      break;
   }
   default:
      error("%s ERROR: unsupported container %s (%d)\n", __func__, backend2str(backend), backend);
      return Error_RuntimeError;
   }

   return OK;
}

void rctr_inherited_equs_are_not_borrowed(Container *ctr)
{
  assert(ctr_is_rhp(ctr));
  RhpContainerData *cdat = (RhpContainerData *)ctr->data;
  assert(cdat);


   /* TODO GITLAB #109 */
  cdat->borrow_inherited = false;
}

int rctr_evalfuncs(Container *ctr)
{
  if (!ctr->func2eval) { return OK; }

  RhpContainerData *cdat = (RhpContainerData *)ctr->data;
  size_t m = cdat->total_m;

   /* ---------------------------------------------------------------------
    * We evaluate the functions directly tagged as needing one, like for instance
    * the objective functions and also the deleted equations.
    * --------------------------------------------------------------------- */

   for (unsigned i = 0, len = aequ_size(ctr->func2eval); i < len; ++i) {
      rhp_idx ei = aequ_fget(ctr->func2eval, i);
      S_CHECK(ei_inbounds(ei, m, __func__));

      Equ *e = &ctr->equs[ei];
      S_CHECK(rctr_evalfunc(ctr, ei, &e->value));
      assert(isfinite(e->value));
//      e->value += equ_get_cst(e);
      trace_solreport("[solreport] equ %s #%u: evaluated to % 2.3e\n",
                      ctr_printequname(ctr, ei), ei, e->value);
   }

   if (!cdat->cmat.deleted_equs) { return OK; }

   /* TODO: investigate performance */
   for (rhp_idx ei = 0; ei < m; ++ei) {

      if (!cdat->cmat.deleted_equs[ei]) { continue; }

      Equ *e = &ctr->equs[ei];
      S_CHECK(rctr_evalfunc(ctr, ei, &e->value));
      assert(isfinite(e->value));
      e->value -= equ_get_cst(e);
      trace_solreport("[solreport] equ %s #%u: evaluated to % 2.3e\n",
                      ctr_printequname(ctr, ei), ei, e->value);

  }
  
  return OK;
}

/**
 * @brief Add a function to be evaluated during the postprocessing
 *
 * @param ctr  the container
 * @param ei   the equation index
 *
 * @return     the error code
 */
int rctr_func2eval_add(Container *ctr, rhp_idx ei)
{
  if (!ctr->func2eval) { 
      A_CHECK(ctr->func2eval, aequ_newblock(2));
   }

   S_CHECK(ei_inbounds(ei, ctr_nequs_max(ctr), __func__));

   Aequ o = {.type = EquVar_Compact, .start = ei, .size = 1};
   S_CHECK(aequ_extendandown(ctr->func2eval, &o));

   return OK;
}

/**
 * @brief Delete a variable from a model
 *
 * @warning this function is quite destructive, and offers no guarantee
 *
 * @param ctr   the container
 * @param vi    the variable to remove
 *
 * @return      the error code
 */
int rctr_delete_var(Container *ctr, rhp_idx vi)
{
   RhpContainerData *cdat = (RhpContainerData *)ctr->data;
   assert(cdat);

   S_CHECK(vi_inbounds(vi, cdat->total_n, __func__));

   if (ctr->vars[vi].is_deleted) {
     error("%s ERROR: variable %s is already deleted!\n", __func__,
              ctr_printvarname(ctr, vi));
     return Error_InvalidValue;
   }

   const struct ctr_mat_elt *me = cdat->cmat.vars[vi];

   /* ----------------------------------------------------------------------
    * One must be extremely cautious here
    * The model element me is going to be freed through equ_rm_var.
    * So make sure to not reference it anymore after the call to equ_rm_var
    * ---------------------------------------------------------------------- */

   while (me) {
      assert(me->vi == vi);
      Equ *e = &ctr->equs[me->ei];
      me = me->next_equ;
      S_CHECK(equ_rm_var(ctr, e, vi));
   }

   cdat->cmat.vars[vi] = NULL;

   ctr->vars[vi].is_deleted = true;

   return OK;
}


/**
 * @brief Evaluate a function at a given point
 *
 * The equation does not need to belong to the container. However, the pool
 * values are looked-up from the container
 *
 * @param ctr     the container
 * @param e       the equation
 * @param x       the point at which to evaluate the function
 * @param[out] F  the value of the function
 *
 * @return        the number of evaluation errors
 */
int rctr_evalfuncat(Container *ctr, Equ *e, const double * restrict x,
                    double * restrict F)
{
   Lequ *lequ = e->lequ;
   unsigned len = lequ ? lequ->len : 0;
   double val = 0;
   bool err = false;

   if (len > 0) {
     rhp_idx * restrict vidx = lequ->vis;
     double * restrict vals = lequ->coeffs;

     for (unsigned l = 0; l < len; ++l) {
       rhp_idx vi = vidx[l];
       /*  TODO(xhub) PERF are we at risk here?  */
       assert(isfinite(vals[l]));
       //      if (isfinite(vals[l])) {
       val += vals[l]*x[vi];
       //      }
     }
   }

   S_CHECK(rctr_getnl(ctr, e));

   if (e->tree && e->tree->root) {
      assert(ctr->pool && ctr->pool->data);
      double nlval;
      /* I sense some umin fishyness --xhub */
      S_CHECK(nltree_evalat(e->tree, x, ctr->pool->data, &nlval));
      val += nlval;
      if (!isfinite(nlval)) {
        err = true;
      }
   }

   double cst = equ_get_cst(e);
   assert(isfinite(cst));

   *F = cst + val;

   return err ? 1 : 0;
}

/**
 * @brief Evaluate a function at a given point
 *
 * The equation does not need to belong to the container. However, the pool
 * values are looked-up from the container
 *
 * @param ctr     the container
 * @param ei      the equation index
 * @param[out] F  the value of the function
 *
 * @return        the number of evaluation errors
 */
int rctr_evalfunc(Container *ctr, rhp_idx ei, double * restrict F)
{
   Equ *e = &ctr->equs[ei];
   Lequ *lequ = e->lequ;
   unsigned len = lequ ? lequ->len : 0;
   double val = 0;
   bool err = false;
   const Var * restrict vars = ctr->vars;

   if (len > 0) {
     rhp_idx * restrict vidx = lequ->vis;
     double * restrict vals = lequ->coeffs;

     for (unsigned l = 0; l < len; ++l) {
       rhp_idx vi = vidx[l];
       /*  TODO(xhub) PERF are we at risk here?  */
       assert(isfinite(vals[l]) && isfinite(vars[vi].value));
       //      if (isfinite(vals[l])) {
       val += vals[l]*vars[vi].value;
       //      }
     }
   }

   S_CHECK(rctr_getnl(ctr, e));

   if (e->tree && e->tree->root) {
    // TODO: should we worry here?
//      assert(ctr->pool && ctr->pool->data);
      double nlval;
      S_CHECK(nltree_eval(ctr, e->tree, &nlval));
      val += nlval;
      if (!isfinite(nlval)) {
        err = true;
      }
   }

   double cst = equ_get_cst(e);
   assert(isfinite(cst));

   *F = cst + val;

   return err ? 1 : 0;
}


int rctr_walkequ(const Container *ctr, rhp_idx ei, void **iterator, double *jacval,
                 rhp_idx *vi, int *nlflag)
{
   assert(ctr_is_rhp(ctr));
   const RhpContainerData *cdat = (RhpContainerData *) ctr->data;
   CMatElt *cme = (CMatElt*)*iterator;

   S_CHECK(ei_inbounds(ei, cdat->total_m, __func__));

   assert(!cme || (valid_ei(cme->ei) && cme->ei == ei &&
          "In rctr_walkequ :: inconsistency between jacptr and ei"));

   if (!cme) {
      cme = cdat->cmat.equs[ei];
      if (!cme) {
         error("[container/matrix] ERROR: equation '%s' is empty in the container matrix\n",
               ctr_printequname(ctr, ei));
         return Error_NullPointer;
      }
   }

   *iterator = cme->next_var;
   *jacval = cme->value;
   *vi = cme->vi;
   *nlflag = cme_isNL(cme);

   return OK;
}

/* TODO(xhub) this should go away as soon as we fix the hack to et */

size_t rctr_totaln(const Container *ctr)
{
   if (rhp_chk_ctr(ctr, __func__) != OK) {
      return RHP_INVALID_IDX;
   }

   RhpContainerData *cdat = (RhpContainerData *)ctr->data;

   return cdat->total_n;
}

size_t rctr_totalm(const Container *ctr)
{
   if (rhp_chk_ctr(ctr, __func__) != OK) {
      return RHP_INVALID_IDX;
   }

   RhpContainerData *cdat = (RhpContainerData *)ctr->data;

   return cdat->total_m;
}

int rctr_get_var_sos1(Container *ctr, rhp_idx vi, unsigned **grps)
{
   assert(ctr && grps);
   RhpContainerData *cdat = (RhpContainerData *)ctr->data;
   UIntArray l;

   rhp_uint_init(&l);

   for (unsigned i = 0; i < cdat->sos1.len; ++i) {
      Avar *v = &cdat->sos1.groups[i].v;
      if (valid_vi(avar_findidx(v, vi))) {
         S_CHECK(rhp_uint_add(&l, i));
      }
   }

   *grps = l.arr;
   return OK;
}

int rctr_get_var_sos2(Container *ctr, rhp_idx vi, unsigned **grps)
{
   assert(ctr && grps);
   RhpContainerData *cdat = (RhpContainerData *)ctr->data;
   UIntArray l;
   rhp_uint_init(&l);

   for (unsigned i = 0; i < cdat->sos2.len; ++i) {
      Avar *v = &cdat->sos2.groups[i].v;
      if (valid_vi(avar_findidx(v, vi))) {
         S_CHECK(rhp_uint_add(&l, i));
      }
   }

   *grps = l.arr;
   return OK;
}

int rctr_setequvarperp(Container *ctr, rhp_idx ei, rhp_idx vi)
{
   assert(valid_vi(vi));
   assert(ctr->varmeta && ctr->equmeta);
   /* TODO: promote this to real checks? */
   assert(!valid_ei(ctr->varmeta[vi].dual));

   ctr->varmeta[vi].type = VarPrimal;
   ctr->varmeta[vi].dual = ei;

   if (valid_ei(ei)) {

      if (ctr->equs[ei].object != Mapping) {
         debug("%s ERROR: equ '%s' is of type %s, should be %s\n", __func__,
               ctr_printequname(ctr, ei), equtype_name(ctr->equs[ei].object),
               equtype_name(Mapping));
      }

      struct equ_meta *equmeta = &ctr->equmeta[ei];
      EquRole equrole = equmeta->role;
      if (equrole != EquUndefined) {
         error("[container] ERROR: equ '%s' already has type '%s'. It should be unset.\n",
               ctr_printequname(ctr, ei), equrole2str(equrole));
         return Error_UnExpectedData;
      }

      assert(!valid_vi(ctr->equmeta[ei].dual)); /* TODO: make it a real check? */

      equmeta->role    = EquViFunction;
      equmeta->dual    = vi;

      ctr->varmeta[vi].ppty = VarPerpToViFunction;
      CTR_DEBUG("Var '%s' ⟂ '%s'", ctr_printvarname(ctr, vi), ctr_printequname(ctr,ei));
   } else {
      CTR_DEBUG("Var '%s' ⟂ 0 (zero function)", ctr_printvarname(ctr, vi));
      ctr->varmeta[vi].ppty = VarPerpToZeroFunctionVi;
   }

   return OK;
}

/**
 * @brief Get the index in the pool corresponding to a value.
 *
 *  If the value is not in the pool of value, it is stored in the pool
 *
 * @param ctr  the container
 * @param val  the value
 *
 * @return     return the index of the value in the pool
 */
unsigned rctr_poolidx(Container *ctr, double val)
{
   /* ----------------------------------------------------------------------
    * Make sure that a pool exists ...
    * ---------------------------------------------------------------------- */

   if (ctr_ensure_pool(ctr) != OK) return UINT_MAX;

   return pool_getidx(ctr->pool, val);
}

/**
 * @brief Add and initialize equations
 *
 * @param ctr  the container
 * @param nb   the number of equation to add to the model
 *
 * @return     the error code
 */
int rctr_add_init_equs(Container *ctr, unsigned nb)
{
   S_CHECK(rhp_chk_ctr(ctr, __func__));
   S_CHECK(rctr_reserve_equs(ctr, nb));
   /*  TODO(xhub) clarify that this is okay */

   for (unsigned i = 0; i < nb; i++) {
      rhp_idx eidx = IdxInvalid;
      S_CHECK(rctr_add_equ_empty(ctr, &eidx, NULL, EquTypeUnset, CONE_NONE));
   }

   return OK;
}

/**
 * @brief Determine whether a variable appears in an equation
 *
 * @param ctr        the container
 * @param vi         the variable index
 * @param ei         the equation index
 * @param[out]  res  true if the variable is in the equation 
 *
 * @return           the error code
 */
int rctr_var_in_equ(const Container *ctr, rhp_idx vi, rhp_idx ei, bool *res)
{
   assert(ctr_is_rhp(ctr));
   const RhpContainerData *cdat = (const RhpContainerData *)ctr->data;

   S_CHECK(vi_inbounds(vi, cdat->total_n, __func__));
   S_CHECK(ei_inbounds(ei, cdat->total_m, __func__));

   CMatElt *elt = cdat->cmat.vars[vi];
   while (elt) {
      if (elt->ei == ei) { *res = true; return OK; }
      elt = elt->next_equ;
   }

   *res = false;
   return OK;
}


int rctr_deactivate_equ(Container *ctr, rhp_idx ei)
{
   assert(ctr_is_rhp(ctr));
   RhpContainerData *cdat = (RhpContainerData *)ctr->data;

   S_CHECK(ei_inbounds(ei, cdat->total_m, __func__));

   ctr->m--;

   if (!ctr->fops) {
      MALLOC_(ctr->fops, Fops, 1);
      fops_active_init(ctr->fops, ctr);
   } else if (ctr->fops->type != FopsActive) {
      error("[container] ERROR: cannot deactivate equation with filter ops of "
            "type %s\n", fopstype2str(ctr->fops->type));
   }

   return fops_deactivate_equ(ctr->fops->data, ei);
}

int rctr_deactivate_var(Container *ctr, rhp_idx vi)
{
   assert(ctr_is_rhp(ctr));
   RhpContainerData *cdat = (RhpContainerData *)ctr->data;
   S_CHECK(vi_inbounds(vi, cdat->total_n, __func__));

   ctr->n--;

   if (!ctr->fops) {
      MALLOC_(ctr->fops, Fops, 1);
      fops_active_init(ctr->fops, ctr);
   } else if (ctr->fops->type != FopsActive) {
      error("[container] ERROR: cannot deactivate equation with filter ops of "
            "type %s\n", fopstype2str(ctr->fops->type));
   }

   return fops_deactivate_var(ctr->fops->data, vi);
}

static int process_flipped(Model *mdl)
{
   double start = get_thrdtime();

   Container *ctr = &mdl->ctr;
   Container *ctr_up = ctr->ctr_up;
   RhpContainerData *cdat = ctr->data;
   /* We just have flipped equation for now */
   Aequ *flipped_equs = &ctr_up->transformations.flipped_equs;
   unsigned new_equs = flipped_equs->size;

   S_CHECK(rctr_reserve_equs(ctr, new_equs));

   rhp_idx objequ;
   rmdl_getobjequ_nochk(mdl, &objequ);

   for (unsigned i = 0; i < new_equs; ++i) {
      rhp_idx ei = aequ_fget(flipped_equs, i);
      S_CHECK(ei_inbounds(ei, cdat->total_m, __func__));

      /* We don't deal with this case */
      if (objequ == ei) {
         error("[container] ERROR: flipping objective equation '%s' is not yet "
               "supported", mdl_printequname(mdl, ei));
         return Error_NotImplemented;
      }

      rhp_idx ei_new;
      S_CHECK(rmdl_equ_flip(mdl, ei, &ei_new));

   }

   mdl->timings->reformulation.container.flipped = get_thrdtime() - start;

   return OK;
}

int rmdl_ctr_transform(Model *mdl)
{
   assert(mdl_is_rhp(mdl));

   if (!mdl->mdl_up || !mdl->ctr.ctr_up) {
      error("[container] ERROR: empty upstream model when transforming "
            "%s model '%.*s' #%u!\n", mdl_fmtargs(mdl));
      return Error_RuntimeError;
   }
   
   bool reformulated = false;

   if (mdl->ctr.ctr_up->transformations.flipped_equs.size > 0) {
      reformulated = true;
      S_CHECK(process_flipped(mdl));
   }

   if (!reformulated) {
      error("[container] ERROR: no transformation to be performed in %s model"
            "'%.*s' #%u\n", mdl_fmtargs(mdl->mdl_up));
   }

   return OK;
}

/** @brief Get the index of the equation that corresponds, after possible
 * transformations, to an equation referenced by an index.
 *
 * The index may be the same if no transformation has been applied.
 * If the equation has been modified (possibly multiple times), then the
 * index of the up-to-date equation in the model is returned.
 *
 *  @param  ctr        the container
 *  @param  ei         the index of the original equation
 *  @param  equ    relevant data of the new equation
 *
 *  @return the status of the operation
 */
int rctr_get_equation(const Container* ctr, rhp_idx ei, EquInfo * restrict equ)
{
   RhpContainerData *cdat = (RhpContainerData *)ctr->data;
   unsigned total_m = cdat->total_m;

   S_CHECK(ei_inbounds(ei, total_m, __func__));

   /* ---------------------------------------------------------------------
    * Search for the modified equation. As we may modify multiple times the
    * same equation, we continue the search until either
    * - the equation has been expanded (not supported now)
    * - this is the final equation, indicated by the reference to itself.
    * --------------------------------------------------------------------- */

   struct rosetta *rosette = &cdat->equ_rosetta[ei];
   bool flipped = false;

   while (!(rosette->ppty & EQU_PPTY_EXPANDED) && rosette->res.equ != ei) {
      ei = rosette->res.equ;
      S_CHECK(ei_inbounds(ei, total_m, __func__));
      flipped = !flipped ? rosette->ppty & EQU_PPTY_FLIPPED : true;
      rosette = &cdat->equ_rosetta[ei];
   }

   if (rosette->ppty & EQU_PPTY_EXPANDED) {
      if (!rosette->res.list) { return Error_NullPointer; }

      equ->expanded = true;
      equ->ei = rosette->res.list[0];
      S_CHECK(ei_inbounds(equ->ei, total_m, __func__));
   } else {
      equ->ei = ei;
      equ->expanded = false;
   }

   equ->flipped = flipped;

   /* If the equation is from another stage, force copy */
   equ->copy_if_modif = rctr_equ_is_readonly(ctr, ei);

   return OK;
}


