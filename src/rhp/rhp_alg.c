#include "asprintf.h"

#include <float.h>
#include <string.h>

#include "container.h"
#include "cmat.h"
#include "ctr_rhp.h"
#include "ctrdat_rhp.h"
#include "ctrdat_gams.h"
#include "ctr_rhp_add_vars.h"
#include "empinfo.h"
#include "equvar_helpers.h"
#include "equvar_metadata.h"
#include "nltree.h"
#include "filter_ops.h"
#include "lequ.h"
#include "macros.h"
#include "mathprgm.h"
#include "mdl.h"
#include "mdl_rhp.h"
#include "rhp_alg.h"
#include "rmdl_data.h"
#include "pool.h"
#include "printout.h"
#include "status.h"
#include "timings.h"

static double _id(double val) { return val; }

int rctr_compress_check_equ(const Container *ctr_src, Container *ctr_dst, size_t skip_equ)
{
   Fops *fops;

   if (ctr_is_rhp(ctr_src)) {
      RhpContainerData *ctrdat_src = (RhpContainerData *)ctr_src->data;
      fops = ctrdat_src->fops;
   } else if (ctr_src->backend == RHP_BACKEND_GAMS_GMO) {
      fops = NULL;
   } else {
      error("Unsupported model type %s", backend_name(ctr_src->backend));
      return Error_NotImplemented;
   }

   return rctr_compress_check_equ_x(ctr_src, ctr_dst, skip_equ, fops);
}

int rctr_compress_check_equ_x(const Container *ctr_src, Container *ctr_dst,
                              size_t skip_equ, const Fops *fops)
{
   unsigned total_m;
   CMatElt **deleted_equs;

   if (ctr_is_rhp(ctr_src)) {
      RhpContainerData *ctrdat_src = (RhpContainerData *)ctr_src->data;
      total_m = ctrdat_src->total_m;
      deleted_equs = ctrdat_src->deleted_equs;
   } else if (ctr_src->backend == RHP_BACKEND_GAMS_GMO) {
      total_m = ctr_src->m;
      deleted_equs = NULL;
   } else {
      error("Unsupported model type %s", backend_name(ctr_src->backend));
      return Error_NotImplemented;
   }

   /* We consider that the deactivated equation are pretty well explained */
   ptrdiff_t n_eq = (skip_equ + ctr_dst->m) - total_m;

   if (n_eq > 0) {

      /* ----------------------------------------------------------------
       * Maybe there are equations with no variables, just constants
       * ---------------------------------------------------------------- */

      for (rhp_idx i = 0; i < total_m; ++i) {

         if (fops && !fops->keep_equ(fops->data, i)) {

            /* ----------------------------------------------------------
             * If the equation was deleted, we are happy
             * ---------------------------------------------------------- */

            if (deleted_equs && deleted_equs[i]) {
               n_eq--;
               continue;
            }

            /* ----------------------------------------------------------
             * Try to see if the equation is a vacuous constraint
             * ---------------------------------------------------------- */

            Equ *e = &ctr_src->equs[i];

            if ((!e->lequ || e->lequ->len == 0) /* empty lequ */
                  && (!e->tree || !e->tree->root)) /* empty tree  */ {

               double cst = equ_get_cst(e);

               switch (e->cone) {
               case CONE_0:
                  if (fabs(cst) > DBL_EPSILON) {
                     error("%s :: dummy constraint '%s' is not fulfilled: 0. != %e\n",
                           __func__, ctr_printequname(ctr_src, i), cst);
                     return Error_Infeasible;
                  }
                  break;
               case CONE_R_MINUS:
                  if (0. < cst) {
                     error("%s :: dummy constraint '%s' #%u is not fulfilled: 0. < %e\n",
                           __func__, ctr_printequname(ctr_src, i), i, cst);
                     return Error_Infeasible;
                  }
                  break;
               case CONE_R_PLUS:
                  if (0. > cst) {
                     error("%s :: dummy constraint '%s' #%u is not fulfilled: 0. > %e\n",
                           __func__, ctr_printequname(ctr_src, i), i, cst);
                     return Error_Infeasible;
                  }
                  break;
               case CONE_NONE:
                  error("%s :: nonsensical equation '%s' #%u: 0 ?? %e\n",
                        __func__, ctr_printequname(ctr_src, i), i, cst);
                  return Error_Unconsistency;
               default:
                  error("%s :: equation '%s' #%u is of type %d but has no "
                        "variables. This case is not implemented yet.\n",
                        __func__, ctr_printequname(ctr_src, i), i, e->object);
                  return Error_NotImplemented;
               }

               /* ----------------------------------------------------
                * We reach this point only if we have determine that
                * the equation is indeed a vacuous constraint
                * ---------------------------------------------------- */

               n_eq--;
            }
         }

         /* -------------------------------------------------------------
          * Check whether we have found every missing bits
          * ------------------------------------------------------------- */

         if (n_eq == 0) {
            break;
         }

      } /*  end for loop over all equations */

      /* -------------------------------------------------------------------
       * Now we have a major issue
       * ------------------------------------------------------------------- */

      if (n_eq > 0) {
         error("[container/export] there are %zu inactive equations(s) that "
               "can't be explained, exiting\n", n_eq);
         return Error_Unconsistency;
      }

   } else if (n_eq < 0) {
      error("%s :: the number of active equations in the compressed model is "
            "smaller than expected: %zu vs %u. We don't know how to deal with "
            "this case.\n", __func__, ctr_dst->m - n_eq, ctr_dst->m);
      return Error_Unconsistency;
   }

   /* -------------------------------------------------------------
    * Adjust the number of equation in the destination container
    *
    * TODO(Xhub) resize to free space? Also this is run unconditionally
    * ------------------------------------------------------------- */

   if (ctr_dst->m != total_m - skip_equ) {
      trace_process("[container] Updating the number of equations from %u to %zu\n",
                    ctr_dst->m, total_m - skip_equ);
      ctr_dst->m = total_m - skip_equ;
   }

   return OK;
}

int rctr_compress_check_var(size_t ctr_n, size_t total_n, size_t skip_var)
{
   /*  TODO(xhub) how to enforce this when we do our own filtering */
   if (skip_var < total_n - ctr_n) {
      error("%s :: number of inactive variable is inconsistent: via "
               "the model representation, there are %zu, via the "
               "model definition %zu as %zu - %zu\n",
               __func__, skip_var, total_n - ctr_n, total_n, ctr_n);
      return Error_Unconsistency;
   }

   return OK;
}

/**
 * @brief Make sure that the model has an objective equation
 *
 * @param          ctr     the model
 * @param          fops    the filter ops
 * @param[in,out]  objequ  on input, the current objequ, on output a valid objequ
 * @param[out]     e_obj   on output, a pointer to the equation
 *
 * @return                 the error code
 */
static NONNULL int ctr_ensure_objequ(Container *ctr, Fops *fops,
                                      rhp_idx *objequ, Equ **e_obj)
{
   rhp_idx lobjequ = *objequ;
   if (!valid_ei(lobjequ) || !fops->keep_equ(fops->data, lobjequ)) {
      S_CHECK(rctr_reserve_equs(ctr, 1));
      S_CHECK(rctr_add_equ_empty(ctr, &lobjequ, e_obj, EQ_MAPPING, CONE_NONE));
      *objequ = lobjequ;
   } else {
      Equ * restrict e = &ctr->equs[lobjequ];
      assert(e->object == EQ_MAPPING && e->cone == CONE_NONE);
      *e_obj = e;
//      if (e->object == EQ_MAPPING) {
//         e->object = EQ_CONE_INCLUSION;
//         e->cone = CONE_0;
//      }
   }

   return OK;
}

/**
 * @brief Make sure that the model has an new objective equation
 *
 * If the objective equation already existed, the objective variable
 * should be eliminated from the new objective equation
 *
 * If the objective equation did not already existed, then create it and
 * add the objective variable to it
 *
 * @param          ctr     the model
 * @param          fops    the filter ops
 * @param          objvar  the objective variable
 * @param[in,out]  objequ  on input, the current objequ, on output the new objequ
 * @param[out]     e_obj   on output, a pointer to the equation
 *
 * @return                 the error code
 */
static NONNULL
int ensure_newobjfunc(Model *mdl, Fops *fops, rhp_idx objvar, rhp_idx *objequ,
                      Equ **e_obj)
{
   rhp_idx lobjequ = *objequ;
   S_CHECK(rctr_reserve_equs(&mdl->ctr, 1));

  /* ------------------------------------------------------------------------
   * If objequ points to a valid equation, then we check that the objvar
   * is present. Otherwise, we add a new equation
   * ------------------------------------------------------------------------ */

   if (!valid_ei(lobjequ) || !fops->keep_equ(fops->data, lobjequ)) {
      S_CHECK(rctr_add_equ_empty(&mdl->ctr, &lobjequ, e_obj, EQ_MAPPING, CONE_NONE));
      assert(valid_ei(lobjequ));
      *objequ = lobjequ;
      S_CHECK(rmdl_setobjequ(mdl, lobjequ));
      return rctr_equ_addnewvar(&mdl->ctr, *e_obj, objvar, 1.);

   }

   Lequ *le = mdl->ctr.equs[lobjequ].lequ;
   double objvar_coeff;
   unsigned dummyint;
   S_CHECK(lequ_find(le, objvar, &objvar_coeff, &dummyint));

   if (!isfinite(objvar_coeff)) {
      error("%s :: objvar '%s' could not be found in equation '%s'\n",
            __func__, ctr_printvarname(&mdl->ctr, objvar), 
            ctr_printequname(&mdl->ctr, lobjequ));
      return Error_IndexOutOfRange;
   }

   S_CHECK(rmdl_dup_equ(mdl, objequ, 0, objvar));
   lobjequ = *objequ;
   Equ *e = &mdl->ctr.equs[lobjequ];
   *e_obj = e;

   double coeff_inv = -1./objvar_coeff;
   S_CHECK(lequ_scal(e->lequ, coeff_inv));
   if (e->tree && e->tree->root) {
      S_CHECK(nltree_scal(&mdl->ctr, e->tree, coeff_inv));
   }
   S_CHECK(cmat_scal(&mdl->ctr, e->idx, coeff_inv));

   double cst = equ_get_cst(e);
   equ_set_cst(e, cst*coeff_inv);

   return OK;
}

static int NONNULL rctr_add_objequvar(Container *ctr, Fops *fops,
                                      rhp_idx *objvar, rhp_idx *objequ)
{
   /* ----------------------------------------------------------------------
    * Ensure that an objective variable exists and add an objective equation
    * if it is missing.
    *
    * There are instance where there is not even an objective function, like
    * when are given a feasibility problem. In that case, we also have to
    * add a dummy objective equation.
    *
    * If the objequ index was valid, we force the evaluation of the objequ
    * based on this objvar
    * ---------------------------------------------------------------------- */

   Equ *e_obj;
   S_CHECK(ctr_ensure_objequ(ctr, fops, objequ, &e_obj));

   S_CHECK(rctr_reserve_vars(ctr, 1));
   Avar v;
   S_CHECK(rctr_add_free_vars(ctr, 1, &v));

   rhp_idx lobjvar = v.start;
   S_CHECK(rctr_equ_addnewvar(ctr, e_obj, lobjvar, -1.));
   ctr->vars[lobjvar].value = 0.;
   *objvar = lobjvar;

   return OK;
}

/* ----------------------------------------------------------------------
 * TODO(xhub) DOC internal fn
 * ---------------------------------------------------------------------- */
static int objvar_gamschk(Model *mdl, rhp_idx * restrict objvar,
                          rhp_idx * restrict objequ, Fops *fops)
{
   RhpContainerData *cdata = mdl->ctr.data;

   /* ----------------------------------------------------------------------
    * If the objective variable is not valid, we add an objective variable
    * and then 
    * ---------------------------------------------------------------------- */
   if (!valid_vi(*objvar) || (fops && !fops->keep_var(fops->data, *objvar))) {

      S_CHECK(rctr_add_objequvar(&mdl->ctr, fops, objvar, objequ));

      cdata->pp.remove_objvars++;

   } else { /* valid objvar index, but if it is a free variable, we need to replace it */
      Container *ctr = &mdl->ctr;
      Var *v = &ctr->vars[*objvar];
      if (v->type != VAR_X || v->is_conic || v->bnd.lb != -INFINITY ||
         v->bnd.ub != INFINITY) {

         Equ *e_obj;
         if (!valid_ei(*objequ) || (fops && !fops->keep_equ(fops->data, *objequ))) {
            S_CHECK(rctr_reserve_equs(ctr, 1));
            S_CHECK(rctr_add_equ_empty(ctr, objequ, &e_obj, EQ_MAPPING, CONE_NONE));
            S_CHECK(rctr_equ_addnewvar(ctr, e_obj, *objvar, 1.));
         } else {
            e_obj = &mdl->ctr.equs[*objequ];
            assert(e_obj->cone == CONE_NONE && e_obj->object == EQ_MAPPING);
            assert(!ctr->equmeta || ctr->equmeta[e_obj->idx].role == EquObjective);
         }

         S_CHECK(rctr_reserve_vars(ctr, 1));
         Avar vv;
         S_CHECK(rctr_add_free_vars(ctr, 1, &vv));

         *objvar = vv.start;
         S_CHECK(rctr_equ_addnewvar(ctr, e_obj, *objvar, -1.));
         mdl->ctr.vars[*objvar].value = 0.;

         cdata->pp.remove_objvars++;
      }
   }

   return rmdl_checkobjequvar(mdl, *objvar, *objequ);
}

static int objvarmp_gamschk(Model *mdl, MathPrgm *mp, Fops *fops)
{
   rhp_idx objvar, objequ;
   EmpDag *empdag = &mdl->empinfo.empdag;
   RhpContainerData *ctrdat = mdl->ctr.data;

   if (!mp) {
      assert(empdag->type == EmpDag_Empty);
      objvar = empdag->simple_data.objvar;
      objequ = empdag->simple_data.objequ;
   } else {
      objvar = mp_getobjvar(mp);
      objequ = mp_getobjequ(mp);
   }

   rhp_idx objvar_bck = objvar, objequ_bck = objequ;
   bool update_objequ = !valid_ei(objequ);

   S_CHECK(objvar_gamschk(mdl, &objvar, &objequ, fops));

   if (update_objequ) {
      if (mp) {
         S_CHECK(mp_setobjequ(mp, objequ));
      } else {
         trace_process("[process] %s model %.*s #%u: objequ is now %s\n",
                       mdl_fmtargs(mdl), mdl_printequname(mdl, objequ));
         S_CHECK(rmdl_setobjequ(mdl, objequ));
      }
   } else {
      assert(objequ_bck == objequ);
   }

   if (objvar_bck != objvar) {
      if (mp) {
         S_CHECK(mp_setobjvar(mp, objvar));
         S_CHECK(mp_objvarval2objequval(mp));
      } else {
         assert(mdl->empinfo.empdag.type == EmpDag_Empty);

         trace_process("[process] %s model %.*s #%u: adding objvar %s to objequ %s\n",
                       mdl_fmtargs(mdl), mdl_printvarname(mdl, objvar),
                       mdl_printequname(mdl, objequ));

         empdag->simple_data.objvar = objvar;
         ctrdat->objequ_val_eq_objvar = true;
      }
   }

   return OK;
}

static NONNULL int rmdl_prepare_export_gams(Model *mdl)
{
   Container *ctr = &mdl->ctr;
   RhpContainerData *cdat = (RhpContainerData *)ctr->data;

   /* ----------------------------------------------------------------------
    * Add an objective variable if there is none since GAMS requires one
    * Also if there is no objective equation identified, add one
    *
    * Warning: we are modifying the number of variables, so make sure we are
    * not getting any model data!
    * ---------------------------------------------------------------------- */

   /* \TODO(xhub) we need to get the resulting model_type somehow*/
   /* \TODO(xhub) this doesn't go well with fops_subset  */
   Fops *fops = cdat->fops;
   EmpDag *empdag = &mdl->empinfo.empdag;
   ProbType probtype = ((RhpModelData*)mdl->data)->probtype;
   if (probtype_isopt(probtype) && empdag->type == EmpDag_Empty) {
      return objvarmp_gamschk(mdl, NULL, fops);
   }

   return OK;

#if 0
   /* Nothing to do for an EMP model */
   if (probtype == MdlProbType_emp) {
   }

   /* ----------------------------------------------------------------------
    * We need to do the same as before for any MP
    * ---------------------------------------------------------------------- */

   for (unsigned i = 0, n_mp = empdag_getmplen(empdag); i < n_mp; ++i) {
      MathPrgm *mp = empdag_getmpfast(empdag, i);
      if (!mp) continue;

      if (mp_isopt(mp)) {
         S_CHECK(objvarmp_gamschk(mdl, mp, fops));
      }
   }

   return OK;
#endif
}

static NONNULL int ensure_deleted_var(Container *ctr, Fops *fops, rhp_idx vi)
{
   if (fops->keep_var(fops->data, vi)) {
      error("%s :: variable '%s' #%u should be inactive but is not marked as such\n",
            __func__, ctr_printvarname(ctr, vi), vi);
      return Error_Unconsistency;
   }

   return OK;
}

static NONNULL int rmdl_prepare_export_rhp(Model *mdl, Fops *fops)
{
   Container *ctr = &mdl->ctr;
   EmpInfo *empinfo = &mdl->empinfo;

   /* ----------------------------------------------------------------------
    * We need to remove objective variables and also fixed variables
    * 
    * TODO(xhub) URG : we allow ourself to modify an equation in place,
    * without copying it, which quite bad
    * ---------------------------------------------------------------------- */

   EmpDag *empdag = &empinfo->empdag;
   if (empdag->type == EmpDag_Empty) {

      rhp_idx objvar;
      rmdl_getobjvar(mdl, &objvar);

      if (valid_vi(objvar)) {

         Equ *e_obj;
         rhp_idx objequ_old;
         rmdl_getobjequ(mdl, &objequ_old);

         rhp_idx objequ;
         rmdl_getobjequ(mdl, &objequ);
         S_CHECK(ensure_newobjfunc(mdl, fops, objvar, &objequ, &e_obj));
         trace_process("[process] %s model %.*s #%u: objvar '%s' removed; "
                       "objequ is now '%s'\n", mdl_fmtargs(mdl),
                       mdl_printvarname(mdl, objvar), mdl_printequname(mdl, objequ));

         if (valid_ei(objequ_old)) {

            S_CHECK(ensure_deleted_var(ctr, fops, objvar));
            S_CHECK(rctr_add_eval_equvar(ctr, objequ_old, objvar));
         } /* TODO ensure that objvar values are set  */

         rmdl_setobjvar(mdl, IdxNA);
      }
   } else {
      unsigned mp_len = empdag_getmplen(empdag);
      assert(mp_len > 0);
      for (unsigned i = 0; i < mp_len; ++i) {
         MathPrgm *mp = empdag_getmpfast(empdag, i);
         if (!mp) continue;

         rhp_idx objvar = mp_getobjvar(mp);
         rhp_idx objequ = mp_getobjequ(mp);

         if (valid_vi(objvar) && valid_ei(objequ)) {
            Equ *e_obj;
            rhp_idx objequ_old = objequ;

            S_CHECK(ensure_newobjfunc(mdl, fops, objvar, &objequ, &e_obj));

            assert(objequ_old != objequ);
            S_CHECK(rctr_add_eval_equvar(ctr, objequ_old, objvar));
            S_CHECK(ensure_deleted_var(ctr, fops, objvar));
            /* TODO ensure that objvar values are set  */

            S_CHECK(mp_setobjvar(mp, IdxNA));
            S_CHECK(mp_setobjequ(mp, objequ));
            trace_process("[process] MP(%s): objvar '%s' removed; objequ is now '%s'\n",
                          empdag_getmpname(empdag, i), mdl_printvarname(mdl, objvar),
                          mdl_printequname(mdl, objequ));
         }
      }
   }

   /* TODO GITLAB #90 */
  /* -----------------------------------------------------------------------
   * We go over all the variables identify fixed variables
   * ----------------------------------------------------------------------- */
//  S_CHECK(ctr_fixedvars(ctr));

//  S_CHECK(rmdl_remove_fixedvars(mdl));

   return OK;
}

static int rctr_prepare_export_gams(Container *ctr, Container *ctr_gms)
{
   /* ----------------------------------------------------------------------
    * The SOS variables need special care
    * ---------------------------------------------------------------------- */

   struct ctrdata_rhp *ctrdat = (struct ctrdata_rhp *)ctr->data;
   struct ctrdata_gams *gms = (struct ctrdata_gams *)ctr_gms->data;

   if (ctrdat->sos1.len > 0) {
      CALLOC_(gms->sos_group, int, ctr_nvars(ctr_gms));
      for (size_t i = 0; i < ctrdat->sos1.len; i++) {
         Avar *v = &ctrdat->sos1.groups[i].v;
         for (size_t j = 0; j < v->size; ++j) {
            gms->sos_group[j] = i+1;
         }
      }
   }

   if (ctrdat->sos2.len > 0) {
      if (!gms->sos_group) {
         CALLOC_(gms->sos_group, int, ctr_nvars(ctr_gms));
      }
      for (size_t i = 0; i < ctrdat->sos2.len; i++) {
         Avar *v = &ctrdat->sos2.groups[i].v;
         for (size_t j = 0; j < v->size; ++j) {
            gms->sos_group[j] = i+1;
         }
      }
   }

   return OK;
}

/** 
 *  @brief Prepare a ReSHOP model for the export into another one
 *
 *
 *  @param mdl_src   the original model
 *  @param mdl_dst   the destination model
 *
 *  @return          the error code
 */
int rmdl_prepare_export(Model * restrict mdl_src, Model * restrict mdl_dst)
{
   Container *ctr = &mdl_src->ctr;
   Container *ctr_dst = &mdl_dst->ctr;

   trace_process("[process] %s model %.*s #%u: exporting to %s model %.*s #%u\n",
                 mdl_fmtargs(mdl_src), mdl_fmtargs(mdl_dst));

   /* Just make sure we increase the stage to make sure objvar are evaluated properly */
   S_CHECK(rmdl_incstage(mdl_src));

   /* TODO GITLAB #71 */
   Fops *fops = ((RhpContainerData *)ctr->data)->fops;
   assert(fops);

   /* ----------------------------------------------------------------------
    * There may be no pool in the original container.
    * We have to be careful in the following to ensure that later on it exists
    * when we want to use it.
    * ---------------------------------------------------------------------- */

   ctr_dst->pool = pool_get(ctr->pool); 

   /* ----------------------------------------------------------------------
    * If the destination container is
    * - a GAMS GMO object, we may need to add objective variable(s)
    * - a RHP, we need to remove any objective variables(s) and change
    *   the equations.
    * ---------------------------------------------------------------------- */

   switch (ctr_dst->backend) {
   case RHP_BACKEND_GAMS_GMO:
      S_CHECK(rmdl_prepare_export_gams(mdl_src));
      /* WARNING: this must be executed AFTER we ensure/remove objective variables. */
      size_t n_dst;
      size_t m_dst;
      fops->get_sizes(fops->data, &n_dst, &m_dst);
      ctr_dst->n = n_dst;
      ctr_dst->m = m_dst;
      break;
   case RHP_BACKEND_RHP:
      S_CHECK(rmdl_prepare_export_rhp(mdl_src, fops));
      break;
   default:
      error("%s :: unsupported destination model type %d\n",
               __func__, ctr_dst->backend);
      return Error_NotImplemented;
   }

   /* ----------------------------------------------------------------------
    * Transfer container preparation
    * ---------------------------------------------------------------------- */

   switch (ctr_dst->backend) {
   case RHP_BACKEND_GAMS_GMO:
      S_CHECK(rctr_prepare_export_gams(ctr, ctr_dst));
      break;
   case RHP_BACKEND_RHP:
      break;
   default: ;
   }

   return OK;
}

/**
 * @brief Compress (after optional filtering) the variables of a model into a new one
 *
 * @param ctr      the original model
 * @param ctr_dst  the destination model
 * @return         the error code
 */
int rctr_compress_vars(const Container *ctr_src, Container *ctr_dst)
{
   assert(ctr_is_rhp(ctr_dst));

   Fops *fops;

   if (ctr_is_rhp(ctr_src)) {
      RhpContainerData *cdat = (RhpContainerData *)ctr_src->data;
      fops = cdat->fops;
   } else if (ctr_src->backend == RHP_BACKEND_GAMS_GMO) {
      fops = NULL;
   } else {
      error("Unsupported model backend %s\n", backend_name(ctr_src->backend));
      return Error_NotImplemented;
   }
   
   return rctr_compress_vars_x(ctr_src, ctr_dst, fops);
}

NONNULL static inline
int copy_vars_nofops(const Container *ctr_src, Container *ctr_dst,
                     rhp_idx *rev_rosetta, size_t ctr_dst_n)
{
   size_t ctr_src_n = ctr_nvars_total(ctr_src);
   rhp_idx *rosetta_vars = ctr_src->rosetta_vars;


   for (rhp_idx i = 0; i < ctr_src_n; ++i) {

      rhp_idx vi = i;
      assert(vi < ctr_dst_n);
      rosetta_vars[i] = vi;
      rev_rosetta[vi] = i;

      S_CHECK(rctr_copyvar(ctr_dst, &ctr_src->vars[i]));
      assert(ctr_dst->vars[vi].idx == vi);

      DPRINT("%s :: new var #%d: lb = %e; ub = %e; value = %e; marginal = %e\n",
            __func__, vdst->idx, vdst->bnd.lb, vdst->bnd.ub, vdst->level, vdst->marginal);
   }

   return OK;
}

NONNULL static inline
int copy_vars_permutation(const Container *ctr_src, Container *ctr_dst,
                          rhp_idx *rev_rosetta, size_t ctr_dst_n, const Fops *fops,
                          rhp_idx *skip_var)
{
   size_t ctr_src_n = ctr_nvars_total(ctr_src);
   rhp_idx *rosetta_vars = ctr_src->rosetta_vars;
   size_t skip_var_ = 0;

   for (rhp_idx i = 0; i < ctr_src_n; ++i) {
      if (!fops->keep_var(fops->data, i)) {
         skip_var_++;
         rosetta_vars[i] = IdxDeleted;
         continue;
      }

      rhp_idx vi = fops->vars_permutation(fops->data, i);
      assert(valid_vi_(vi, ctr_dst_n, __func__));
      rosetta_vars[i] = vi;
      rev_rosetta[vi] = i;

      S_CHECK(rctr_copyvar(ctr_dst, &ctr_src->vars[i]));

      DPRINT("%s :: new var #%d: lb = %e; ub = %e; value = %e; marginal = %e\n",
            __func__, vdst->idx, vdst->bnd.lb, vdst->bnd.ub, vdst->level, vdst->marginal);
   }

   *skip_var = skip_var_;
   return OK;
}

NONNULL static inline
int copy_vars_fops(const Container *ctr_src, Container *ctr_dst,
                   rhp_idx *rev_rosetta, size_t ctr_dst_n, const Fops *fops,
                   rhp_idx *skip_var)
{
   size_t ctr_src_n = ctr_nvars_total(ctr_src);
   rhp_idx *rosetta_vars = ctr_src->rosetta_vars;
   size_t skip_var_ = 0;

   for (rhp_idx i = 0; i < ctr_src_n; ++i) {
      if (!fops->keep_var(fops->data, i)) {
         skip_var_++;
         rosetta_vars[i] = IdxDeleted;
         continue;
      }

      rhp_idx vi = i - skip_var_;
      assert(valid_vi_(vi, ctr_dst_n, __func__));
      rosetta_vars[i] = vi;
      rev_rosetta[vi] = i;

      S_CHECK(rctr_copyvar(ctr_dst, &ctr_src->vars[i]));
      assert(ctr_dst->vars[vi].idx == vi);

      DPRINT("%s :: new var #%d: lb = %e; ub = %e; value = %e; marginal = %e\n",
            __func__, vdst->idx, vdst->bnd.lb, vdst->bnd.ub, vdst->level, vdst->marginal);
   }

   *skip_var = skip_var_;
   return OK;
}

int rctr_compress_vars_x(const Container *ctr_src, Container *ctr_dst, Fops *fops)
{
   int status = OK;
   size_t ctr_dst_n, ctr_dst_m, ctr_src_n = ctr_nvars_total(ctr_src);

   if (fops) {
      fops->get_sizes(fops->data, &ctr_dst_n, &ctr_dst_m);
   } else {
      ctr_dst_n = ctr_src->n;
      ctr_dst_m = ctr_src->m;
   }

   if (ctr_dst_n == 0) {
      error("%s :: no variables in the destination model!\n", __func__);
      return Error_RuntimeError;
   }

   /* Allocates the data in the destination container  */
   /*  TODO(xhub) PERF is malloc sufficient here? */
   if (!ctr_dst->vars) {

      CALLOC_(ctr_dst->vars, Var, ctr_dst_n);

   } else if (ctr_dst_n > ((RhpContainerData*)ctr_dst->data)->max_n) {

      error("%s :: The variable space is already allocated, but too small: %zu "
            "is needed; %zu is allocated.\n", __func__, ctr_dst_n,
            ((RhpContainerData*)ctr_dst->data)->max_n);
      return Error_UnExpectedData;
   }

   rhp_idx *rev_rosetta, skip_var = 0;
   MALLOC_(rev_rosetta, rhp_idx, ctr_dst_n);

   if (!fops) {
      S_CHECK_EXIT(copy_vars_nofops(ctr_src, ctr_dst, rev_rosetta, ctr_dst_n));
   } else if (fops->vars_permutation) {
      S_CHECK_EXIT(copy_vars_permutation(ctr_src, ctr_dst, rev_rosetta, ctr_dst_n,
                                         fops, &skip_var));
   } else {
      S_CHECK_EXIT(copy_vars_fops(ctr_src, ctr_dst, rev_rosetta, ctr_dst_n,
                                  fops, &skip_var));
   }

   unsigned nvars = ctr_src_n - skip_var;  
   RhpContainerData *ctrdat_dst = (RhpContainerData*)ctr_dst->data;
   avar_setcompact(&ctrdat_dst->var_inherited.v, nvars, 0);
   avar_setandownlist(&ctrdat_dst->var_inherited.v_src, nvars, rev_rosetta);

   return rctr_compress_check_var(ctr_src->n, ctr_src_n, skip_var);

_exit:
   FREE(rev_rosetta);
   return status;
}

#if 0
int rctr_compress_equs(const Container *ctr_src, Container *ctr_dst)
{
   assert(ctr_is_rhp(ctr_dst));
   RhpContainerData *ctrdat_src = (RhpContainerData *)ctr_src->data;

   Fops *fops = ctrdat_src->fops;

   size_t ctr_dst_n;
   size_t ctr_dst_m;
   if (fops) {
      fops->get_sizes(fops->data, &ctr_dst_n, &ctr_dst_m);
   } else {
      ctr_dst_n = ctr_src->n;
      ctr_dst_m = ctr_src->m;
   }

   if (ctr_dst_m == 0) {
      error("%s :: no equation in the destination model!\n", __func__);
      return Error_RuntimeError;
   }

   if (!ctr_dst->equs) {
      CALLOC_(ctr_dst->equs, struct equ, ctr_dst_m);
   } else if (ctr_dst_m >= ((RhpContainerData*)ctr_src->data)->max_m) {
      error("%s :: The variable space is already allocated, but too small: %zu "
            "is needed; %zu is allocated.\n", __func__, ctr_dst_m,
            ((RhpContainerData*)ctr_src->data)->max_m);
      return Error_UnExpectedData;
   }

   rhp_idx * restrict rosetta_equs = ctr_src->rosetta_equs;
   rhp_idx skip_equ = 0;
   rhp_idx *rev_rosetta;
   MALLOC_(rev_rosetta, rhp_idx, ctr_dst_m);

   for (rhp_idx i = 0, len = ctrdat_src->total_m; i < len; ++i) {
      if (fops && !fops->keep_equ(fops->data, i)) {
         skip_equ++;
         rosetta_equs[i] = IdxDeleted;
         continue;
      }

      rhp_idx ei = i - skip_equ;
      rosetta_equs[i] = ei;
      rev_rosetta[ei] = i;
      assert(ei >= 0 && ei < ctr_dst_m);

      Equ * restrict edst = &ctr_dst->equs[ei];
      const Equ * restrict esrc = &ctr_src->equs[i];
      equ_copymetadata(edst, esrc, ei);

      edst->lequ = NULL;
      edst->tree = NULL;
   }

   unsigned nequs = ctrdat_src->total_m - skip_equ;  
   RhpContainerData *ctrdat_dst = (RhpContainerData*)ctr_dst->data;
   aequ_setcompact(&ctrdat_dst->equ_inherited.e, nequs, 0);
   aequ_setandownlist(&ctrdat_dst->equ_inherited.e_src, nequs, rev_rosetta);

   S_CHECK(rctr_compress_check_equ(ctr_src, ctr_dst, skip_equ));

   return OK;
}
#endif

/** 
 *  @brief Perform the presolve for an optimization problem
 *
 *  This function solves all the optimization problems that were stored as
 *  subproblems. Those are usually used to initialize variables values
 *
 *  @param ctr  the container
 *
 *  @return     the error code
 */
int rmdl_presolve(Model *mdl, unsigned backend)
{
   unsigned offset_pool;
   Container *ctr = &mdl->ctr;
   RhpContainerData *cdat = (RhpContainerData *)ctr->data;

   unsigned nb = 0;
   for (unsigned s = 0, cur_stage = cdat->current_stage; s <= cur_stage; ++s) {
      struct auxmdl *s_subctr = &cdat->stage_auxmdl[cur_stage - s];

      nb += s_subctr->len;
   }

   if (nb == 0) {
      return OK;
   }

   double start = get_walltime();

   S_CHECK(rctr_ensure_pool(ctr, NULL));

   offset_pool = ctr->pool->len;

   /* ----------------------------------------------------------------------
    * Ensure that the pool size is large enough to hold the values of all variables.
    * ---------------------------------------------------------------------- */

   if (ctr->pool->len + cdat->total_n > ctr->pool->max) {
      size_t new_max = ctr->pool->len + cdat->total_n;
      if (ctr->pool->own) {
         ctr->pool->max = new_max;
         REALLOC_(ctr->pool->data, double, ctr->pool->max);
      } else {
         struct nltree_pool *p;
         A_CHECK(p, pool_new());
         p->own = true;
         p->len = ctr->pool->len;
         p->max = new_max;
         double *orig_data = ctr->pool->data;
         MALLOC_(p->data, double, p->max);
         memcpy(p->data, orig_data, ctr->pool->len*sizeof(double));

         /* -----------------------------------------------------------------
          * switch pools
          * ----------------------------------------------------------------- */

         pool_release(ctr->pool);
         ctr->pool = p;
      }
   }

   ctr->pool->len += cdat->total_n;
   double *pool_data = ctr->pool->data;

   for (unsigned vi = 0, i = offset_pool; vi < (unsigned)cdat->total_n; ++vi, ++i) {
      pool_data[i] = ctr->vars[vi].value;
   }

   /* ----------------------------------------------------------------------
    * HACK: save existing "model" data before solving auxiliary models
    * ---------------------------------------------------------------------- */

   EmpDag empdag_orig;
   memcpy(&empdag_orig, &mdl->empinfo.empdag, sizeof(EmpDag));

   ProbType probtype_type = ((RhpModelData*)mdl->data)->probtype;

   bool cpy_fops = false;
   Fops fops_orig;
   if (cdat->fops) {
      memcpy(&fops_orig, cdat->fops, sizeof(Fops));
      cpy_fops = true;
   }


   int status = OK;
   Model *mdl_solver = mdl_new(backend);
   Container *ctr_solver = &mdl_solver->ctr;

   const char *mdl_name = mdl_getname(mdl);

  /* ----------------------------------------------------------------------
   * We need to copy this information as it will get overwritten when the
   * stage number is increased in rmdl_prepare_export()
   * ---------------------------------------------------------------------- */

   struct auxmdl *stages_auxmdl;
   unsigned cur_stage = cdat->current_stage;
   MALLOC_(stages_auxmdl, struct auxmdl, cur_stage+1);
   memcpy(stages_auxmdl, cdat->stage_auxmdl, (cur_stage+1) * sizeof(struct auxmdl));

   for (unsigned s = 0; s <= cur_stage; ++s) {
      struct auxmdl *s_subctr = &stages_auxmdl[cur_stage - s];

      /* -------------------------------------------------------------------
       * Descending order
       * ------------------------------------------------------------------- */

      for (unsigned i = 0, j = s_subctr->len-1; i < s_subctr->len; ++i, --j) {
         struct filter_subset* fs = s_subctr->filter_subset[j];
         S_CHECK_EXIT(filter_subset_init(fs, mdl, offset_pool));

         char *name;
         IO_CALL_EXIT(asprintf(&name, "%s_s%u_i%u", mdl_name, s, i));
         S_CHECK_EXIT(mdl_setname(mdl_solver, name));
         FREE(name);
         /* TODO: add option for selection subsolver */

         S_CHECK_EXIT(rmdl_prepare_export(mdl, mdl_solver));
         S_CHECK_EXIT(mdl_exportmodel(mdl, mdl_solver));

         /* TODO(Xhub) URG add option for displaying that  */
#if defined(DEBUG)
         strncpy(ctr_solver->data, "CONVERTD", GMS_SSSIZE-1);
         S_CHECK_EXIT(ctr_callsolver(&mdl_solver));
         strncpy(ctr_solver->data, "", GMS_SSSIZE-1);
         S_CHECK_EXIT(ctr_callsolver(&mdl_solver));
#endif
         /* Phase 1: report the values from the solver to the RHP */
         /* TODO(xhub) optimize and iterate over the valid equations */

         /* We read those here, since they may not exist at the beginning  */
         rhp_idx *rosetta_vars = ctr->rosetta_vars;
         rhp_idx *rosetta_equs = ctr->rosetta_equs;


         struct ctrmem CTRMEM working_mem = {.ptr = NULL, .ctr = ctr};
         size_t arrsize = MAX(ctr_solver->n, ctr_solver->m);
         A_CHECK(working_mem.ptr, ctr_getmem(ctr, 2*arrsize*sizeof(double)));

         double * restrict vals = (double*)working_mem.ptr;
         double * restrict mults = &vals[arrsize];

         S_CHECK_EXIT(ctr_getallvarsval(ctr_solver, vals));
         S_CHECK_EXIT(ctr_getallvarsmult(ctr_solver, mults));

         for (unsigned k = 0, l = 0; k < (unsigned)cdat->total_n; ++k) {

            if (valid_vi(rosetta_vars[k])) {
               /* Update the values in the container */
               Var * restrict vdst = &ctr->vars[k];
               vdst->value = vals[l];
               vdst->multiplier = mults[l];

               /* Update the values in the pool */
               ctr->pool->data[offset_pool+k] = vdst->value;

               /*  TODO(xhub) URG why is this commented?/ */
//               update_neg(neg_var_vals, -ctr_solver->vars[l].level)

               ++l;
            }
         }

         S_CHECK_EXIT(ctr_getallequsval(ctr_solver, vals));
         S_CHECK_EXIT(ctr_getallequsmult(ctr_solver, mults));

         for (unsigned k = 0, l = 0; k < (unsigned)cdat->total_m; ++k) {
            if (valid_ei(rosetta_equs[k])) {
               /* Update the values in the container */
               /*  TODO(xhub) is the level useful? */
               Equ * restrict edst = &ctr->equs[k];
               edst->value = vals[l];
               edst->multiplier = mults[l];
               ++l;
            }
         }

         /* TODO(Xhub) this doesn't look good, but now we hardcheck this.
          * Find a way to improve this. If we solve multiple time a subset
          * of a model, we might want to not allocate all the time*/
         FREE(ctr->rosetta_vars);
         FREE(ctr->rosetta_equs);

      }
   }

   FREE(stages_auxmdl);

   /* ----------------------------------------------------------------------
    * restore the model
    * ---------------------------------------------------------------------- */

   memcpy(&mdl->empinfo.empdag, &empdag_orig, sizeof(EmpDag));
   if (cpy_fops) {
      memcpy(cdat->fops, &fops_orig, sizeof(Fops));
   }

   ((RhpModelData*)mdl->data)->probtype = probtype_type;

_exit:
   mdl_release(mdl_solver);

   mdl->timings->solve.presolve_wall = get_walltime() - start;

   return status;
}
