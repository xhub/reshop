#include <string.h>

#include "fenchel_common.h"
#include "cmat.h"
#include "ctr_rhp.h"
#include "ctr_rhp_add_vars.h"
#include "equ_modif.h"
#include "itostr.h"
#include "lequ.h"
#include "macros.h"
#include "mathprgm.h"
#include "mdl.h"
#include "ovf_common.h"
#include "printout.h"
#include "status.h"
#include "var.h"

static void ydat_empty(yData *ydat)
{
   if (!ydat) { return; }

   FREE(ydat->cones_y);
   FREE(ydat->cones_y_data);
   FREE(ydat->tilde_y);
   FREE(ydat->mult_ub_revidx);
}

static NONNULL int ydat_init(yData *ydat)
{
   int status;

   unsigned n_y = ydat->n_y;
   assert(n_y != 0);

   memset(ydat, 0, sizeof(yData));
   ydat->n_y = n_y;

   MALLOC_EXIT(ydat->tilde_y, double, 2*n_y);
   ydat->var_ub = &ydat->tilde_y[n_y];
   MALLOC_EXIT(ydat->cones_y, Cone, n_y);
   MALLOC_EXIT(ydat->cones_y_data, void*, n_y);
   ydat->mult_ub_revidx = NULL;


   ydat->has_shift = false;
   ydat->shift_quad_cst = 0.;

   return OK;

_exit:
   ydat_empty(ydat);
   return status;
}

int fenchel_find_yshift(CcfFenchelData *fdat)
{
   yData *ydat = &fdat->primal.ydat;
   const OvfOps *ops = fdat->ops;
   OvfOpsData ovfd = fdat->ovfd;

   for (unsigned i = 0, n_y = fdat->primal.ydat.n_y; i < n_y; ++i) {
      /*  TODO(xhub) allow more general cones here */
      ydat->cones_y_data[i] = NULL;
      double lb = ops->get_var_lb(ovfd, i);
      double ub = ops->get_var_ub(ovfd, i);

      bool lb_finite = isfinite(lb);
      bool ub_finite = isfinite(ub);

      /* Let's be on the safe side   */
      if (lb_finite && ub_finite) {

         /* Check whether we have an equality constraint  */
         if (fabs(ub - lb) < DBL_EPSILON) {

            if (fabs(lb) < DBL_EPSILON) {
               ydat->tilde_y[i] = 0.;
            } else {
               ydat->tilde_y[i] = lb;
               ydat->has_shift = true;
            }

            fdat->primal.has_set = true;
            ydat->cones_y[i] = CONE_0;
            ydat->var_ub[i] = NAN;
            continue;

         }

         if (ub < lb) {
            error("[ccflib/fenchel] ERROR: the bounds on the %u-th variable are not consistent: "
                  "lb = %e > %e = ub\n", i, lb, ub);
            return Error_InvalidValue;
         }
      }

      if (lb_finite) {
         fdat->primal.has_set = true;
         ydat->cones_y[i] = CONE_R_PLUS;

         if (fabs(lb) < DBL_EPSILON) {
            ydat->tilde_y[i] = 0.;
         } else {
            ydat->tilde_y[i] = lb;
            ydat->has_shift = true;
         }

         if (ub_finite) {
            ydat->var_ub[i] = ub;
            ydat->n_y_ub++;
         } else {
            ydat->var_ub[i] = NAN;
         }

      } else if (ub_finite) {

         fdat->primal.has_set = true;
         ydat->cones_y[i] = CONE_R_MINUS;
         ydat->var_ub[i] = NAN;

         if (fabs(ub) < DBL_EPSILON) {
            ydat->tilde_y[i] = 0.;
         } else {
            ydat->tilde_y[i] = ub;
            ydat->has_shift = true;
         }

      } else {
         ydat->cones_y[i] = CONE_R;
         ydat->var_ub[i] = NAN;
         ydat->tilde_y[i] = 0.;
      }
   }

   if (ydat->n_y_ub > 0) {
      MALLOC_(ydat->mult_ub_revidx, unsigned, ydat->n_y_ub);
   }

   if (RHP_UNLIKELY(O_Output & PO_TRACE_CCF)) {
      if (ydat->has_shift) {
         trace_ccf("[ccf/fenchel] Found shift for the primal variables:\n");
         for (unsigned i = 0, n_y = fdat->primal.ydat.n_y; i < n_y; ++i) {
            trace_ccf("\t[%5u] %e\n", i, ydat->tilde_y[i]);
         }
      }
   }

   return OK;
}

int fdat_init(CcfFenchelData * restrict fdat, OvfType type, const OvfOps *ops,
              OvfOpsData ovfd, Model *mdl)
{
   int status = OK;

   fdat->type = type;
   fdat->skipped_cons = false;
   fdat->finalize_equ = false;
   fdat->ops = ops;
   fdat->ovfd = ovfd;
   unsigned n_args;
   S_CHECK(ops->get_nargs(ovfd, &n_args));

   unsigned n_y = ops->size_y(ovfd, n_args);
   fdat->primal.ydat.n_y = n_y;
   fdat->nargs = n_args;

   rhp_idx vi_ovf = ops->get_ovf_vidx(ovfd);
   fdat->vi_ovf = vi_ovf;
   fdat->dual.ei_objfn = IdxNA;

   RhpSense sense_parent;
   S_CHECK(ops->get_mp_and_sense(ovfd, mdl, vi_ovf, &fdat->mp_dst, &sense_parent));

   if (fdat->primal.ydat.n_y == 0) {
      const char *ovf_name = ops->get_name(ovfd);
      error("[ccflib/primal] the number of variable associated with the CCF '%s' "
            "of type %s is 0. This should never happen\n."
            "Check the OVF definition if it is a custom one, or file a bug\n",
            ovf_name, ops->get_name(ovfd));
      return Error_UnExpectedData;
   }

   rhpmat_null(&fdat->At);
   rhpmat_null(&fdat->D);
   rhpmat_null(&fdat->M);
   rhpmat_null(&fdat->J);
   rhpmat_null(&fdat->B_lin);

   fdat->dual.a = NULL;
   fdat->dual.ub = NULL;
   fdat->b_lin = NULL;
   fdat->tmpvec = NULL;
   fdat->cons_gen = NULL;
   fdat->equvar_basename = NULL;

   OvfPpty ovf_ppty;
   ops->get_ppty(ovfd, &ovf_ppty);
   fdat->primal.is_quad = ovf_ppty.quad;
   fdat->primal.has_set = false;
   fdat->primal.sense = ovf_ppty.sense;

   /* ---------------------------------------------------------------------
    * Test compatibility between OVF and problem type
    * --------------------------------------------------------------------- */

   S_CHECK(ovf_compat_types(ops->get_name(ovfd), ops->get_name(ovfd), sense_parent, ovf_ppty.sense));

   /* Initial variables */
   S_CHECK(ydat_init(&fdat->primal.ydat));
   avar_init(&fdat->dual.vars.v);
   avar_init(&fdat->dual.vars.w);
   avar_init(&fdat->dual.vars.s);

  /* ----------------------------------------------------------------------
   * Determine \tilde{y} such that y - \tilde{y} ∈ K_y
   * ---------------------------------------------------------------------- */

   /* WARNING, this needs to be called before getting all the matrices */
   S_CHECK(fenchel_find_yshift(fdat));

   /* We look for A and c such that A y - a belongs to K_y
    * We ask for the CSC version of A to easily iterate of the rows of A^T */
   rhpmat_set_csc(&fdat->At);
   S_CHECK(ops->get_set_nonbox(ovfd, &fdat->At, &fdat->dual.a, true));

   /* Get the Cholesky factorization of M = D^TJD */
   S_CHECK(ops->get_D(ovfd, &fdat->D, &fdat->J));

   if (fdat->primal.is_quad) {
      S_CHECK(ops->get_M(ovfd, &fdat->M));
   }

   unsigned nrows_A, ncols_A = 0;

   if (fdat->At.ppty) {
     fdat->primal.has_set = true;
     S_CHECK_EXIT(rhpmat_get_size(&fdat->At, &ncols_A, &nrows_A));
     assert(ncols_A > 0 && nrows_A > 0);

   } else if (!fdat->primal.has_set) {

      if (fdat->D.ppty) {
         unsigned dummy1, dummy2;
         S_CHECK_EXIT(rhpmat_get_size(&fdat->D, &dummy1, &dummy2));
      } else {
         const char *ovf_name = ops->get_name(ovfd);
         error("[ccflib/fenchel] ERROR: no CCF set given and no quadratic part. "
               "The CCF function associated with %s is unbounded!\n", ovf_name);
         status = Error_EMPIncorrectInput;
         goto _exit;
      }
   }

   fdat->primal.ncons = ncols_A;


   /* Check whether we have B_lin or b_lin  */
   S_CHECK_EXIT(ops->get_affine_transformation(ovfd, &fdat->B_lin, &fdat->b_lin));

   if (fdat->B_lin.ppty) {
      unsigned n_rows, n_cols;
      S_CHECK_EXIT(rhpmat_get_size(&fdat->B_lin, &n_cols, &n_rows));

      if (n_rows != n_y) {
         error("%s :: incompatible size: B_lin and A^T should have the same number of"
               " rows, but there are %d rows in B_lin and %d in A^T\n", __func__,
               n_rows, n_y);
         status = Error_Inconsistency;
         goto _exit;
      }

      if (n_cols != n_args) {
         error("%s :: incompatible size: the number of arguments (%u) and the "
               "number of columns in B_lin (%u) should be the same\n", __func__,
               n_args, n_cols);
         status = Error_Inconsistency;
         goto _exit;
      }
   }

   CALLOC_(fdat->dual.ub, double, fdat->primal.ydat.n_y_ub);
   CALLOC(fdat->tmpvec, double, MAX(n_y, ncols_A));

_exit:
   return status;
}

int fenchel_apply_yshift(CcfFenchelData *fdat)
{
   yData *ydat = &fdat->primal.ydat;
   if (!ydat->has_shift) { return OK; }

   const OvfOps *ops = fdat->ops;
   OvfOpsData ovfd = fdat->ovfd;

   /* -------------------------------------------------------------
    * 1.a Perform a -= A tilde_y  for the nonbox polyhedral constraints
    * ------------------------------------------------------------- */

   S_CHECK(rhpmat_atxpy(&fdat->At, fdat->primal.ydat.tilde_y, fdat->tmpvec));

   double *a = fdat->dual.a;
   for (unsigned i = 0, end = fdat->primal.ncons; i < end; ++i) {
      a[i] -= fdat->tmpvec[i];
   }

   /* -------------------------------------------------------------
       * 1.b 
       * the shift on the upper bound on y is easier: if it is finite,
       * then subtract tilde_y
       *
       * Since some constraints may not have multipliers, we make use of
       * v_bnd_revidx
       * ------------------------------------------------------------- */

   unsigned * restrict w_revidx = fdat->primal.ydat.mult_ub_revidx;
   double * restrict ub = fdat->dual.ub;
   for (unsigned i = 0, end = fdat->primal.ydat.n_y_ub; i < end; ++i) {
      ub[i] -= fdat->primal.ydat.tilde_y[w_revidx[i]];
   }

   /* -------------------------------------------------------------
       * Add -.5 <tilde_y, M tilde_y> to the RHS to take into account M
       * ------------------------------------------------------------- */

   if (fdat->primal.is_quad) {
      SpMat M;
      rhpmat_null(&M);

      S_CHECK(ops->get_M(ovfd, &M));

      double quad_cst = .5 * rhpmat_evalquad(&M, ydat->tilde_y);
      rhpmat_free(&M);

      if (!isfinite(quad_cst)) {
         error("[ccflib:fenchel] ERROR: the quadratic constant "
               "from the shift is not finite: val = %f\n", quad_cst);
         return Error_MathError;
      }

      fdat->primal.ydat.shift_quad_cst = quad_cst;
   }

   return OK;
}

int fenchel_gen_vars(CcfFenchelData *fdat, Model *mdl)
{
   /* ---------------------------------------------------------------------
    * Add the new variables v and s:
    * - v has the same dimension has the number of constraints. We have two
    *   types of constraints:
    *     + The ones given by Ax - b ∈ K_c
    *     + The finite upper bound on y, collected during the anaysis phase in
    *       var_ub
    *   v belongs to the polar (or dual for inf OVF) of K_c and R_+ (upper bound)
    *
    * - s has the same size as v when there is a quadratic part
    * --------------------------------------------------------------------- */
   unsigned ncons = fdat->primal.ncons;
   unsigned n_vars = fdat->primal.ydat.n_y_ub + ncons;
   if (fdat->primal.is_quad) {
      n_vars += fdat->primal.ydat.n_y;
   }

   if (n_vars == 0) {
      const char *ovf_name = fdat->ops->get_name(fdat->ovfd);
      error("[ccflib/primal] ERROR: the CCF with variable '%s' has no constraints and no "
            "quadratic part. It is then unbounded\n", ovf_name);
      return Error_ModelUnbounded;
   }

   Container *ctr = &mdl->ctr;
   S_CHECK(rctr_reserve_vars(ctr, n_vars));

   /* ----------------------------------------------------------------------
    * Resize c to include the upper bound
    * ---------------------------------------------------------------------- */

   /* ---------------------------------------------------------------------
    * If we want to initialize the values of the new variables, this is the
    * start for the new variables
    * --------------------------------------------------------------------- */
   RhpContainerData *cdat = (RhpContainerData *)ctr->data;
   const OvfOps *ops = fdat->ops;
   OvfOpsData ovfd = fdat->ovfd;
   MathPrgm *mp = fdat->mp_dst;

   /* NAMING: this should not exists */
   char *equvar_basename = NULL;
   const char *ovf_name = ops->get_name(ovfd);
   size_t ovf_namelen = strlen(ovf_name);

   MALLOC_(equvar_basename, char, ovf_namelen + 26);
   strcpy(equvar_basename, ovf_name);
   strcat(equvar_basename, "_");
   strcat(equvar_basename, fdat->type == OvfType_Ovf ? "ovf" : "ccf");
   strcat(equvar_basename, "_");
   /* TODO URG: vi_ovf does not always make sense */
   unsignedtostr((unsigned)fdat->vi_ovf, sizeof(unsigned), &equvar_basename[strlen(equvar_basename)], 20, 10);
   unsigned ovf_name_idxlen = strlen(equvar_basename);
   fdat->equvar_basename = equvar_basename;

   /* Add the v variable */
   unsigned v_start = 0, w_start = 0; /* init for cwarn */
   if (fdat->primal.has_set) {
      v_start = cdat->total_n;
      unsigned n_v_actual = 0;
      char *v_name;

      /* -------------------------------------------------------------------
       * Add the multipliers for the nonbox constraints. This is always well
       * defined
       *
       * If the OVF is a sup, the multipliers belong to the polar cone
       * Otherwise, we use a trick and the multiplier belongs to the dual cone
       * ------------------------------------------------------------------- */

      NEWNAME2(v_name, equvar_basename, ovf_name_idxlen, "_v");
      cdat_varname_start(cdat, v_name);

      double * restrict a = fdat->dual.a;
      RhpSense sense = fdat->primal.sense;

      for (unsigned j = 0; j < ncons; ++j) {
         enum cone cone;
         void *cone_data;
         S_CHECK(ops->get_cone_nonbox(ovfd, j, &cone, &cone_data));
         rhp_idx vi = IdxNA;

         switch (sense) {
         case RhpMax:
            S_CHECK(rctr_add_multiplier_polar(ctr, cone, cone_data, &vi));
            break;
         case RhpMin:
            S_CHECK(rctr_add_multiplier_dual(ctr, cone, cone_data, &vi));
            break;
         default: return error_runtime();
         }

         if (valid_vi(vi)) {
            a[n_v_actual] = a[j];
            n_v_actual++;
            if (mp) {
               S_CHECK(mp_addvar(mp, vi));
            }
         }
      }

      cdat_varname_end(cdat);
      avar_setcompact(&fdat->dual.vars.v, n_v_actual, v_start);

      /* -------------------------------------------------------------------
       * Add the multipliers for the upper bounds
       *
       * If the OVF is a sup, the multipliers belong to the polar cone
       * Otherwise, we use a trick and the multiplier belongs to the dual cone
       *
       * OUTPUT:
       *   c filled from a_bnd_start until ...
       *   v_bnd_revidx of size c_end-c_bnd_start maps the index of c to the index of u
       * ------------------------------------------------------------------- */

      NEWNAME2(v_name, equvar_basename, ovf_name_idxlen, "_vbnd");
      cdat_varname_start(cdat, v_name);
      w_start = cdat->total_n;

 
      double * restrict yvar_ub = fdat->primal.ydat.var_ub;
      double * restrict ub = fdat->dual.ub;
      unsigned n_y = fdat->primal.ydat.n_y;

      unsigned n_w_actual = 0;
      for (unsigned i = 0; i < n_y; ++i) {
         if (isfinite(yvar_ub[i])) {
            rhp_idx vi = IdxNA;

            switch (sense) {
            case RhpMax:
               S_CHECK(rctr_add_multiplier_polar(ctr, CONE_R_MINUS, NULL, &vi));
               break;
            case RhpMin:
               S_CHECK(rctr_add_multiplier_dual(ctr, CONE_R_MINUS, NULL, &vi));
               break;
            default: return error_runtime();
            }

            assert(valid_vi(vi));
            if (mp) { S_CHECK(mp_addvar(mp, vi)); }

            ub[n_w_actual] = yvar_ub[i];
            fdat->primal.ydat.mult_ub_revidx[n_w_actual] = i;
            n_w_actual++; assert(n_w_actual <= fdat->primal.ydat.n_y_ub);
         }
      }

      assert(n_w_actual == fdat->primal.ydat.n_y_ub);
      cdat_varname_end(cdat);
      avar_setcompact(&fdat->dual.vars.w, fdat->primal.ydat.n_y_ub, w_start);
   }

   /* Add the s variable if needed  */
   if (fdat->primal.is_quad) {
      char *s_name;
      NEWNAME2(s_name, equvar_basename, ovf_name_idxlen, "_s");
      cdat_varname_start(cdat, s_name);

      unsigned n_y = fdat->primal.ydat.n_y;
      S_CHECK(rctr_add_free_vars(ctr, n_y, &fdat->dual.vars.s));
      if (mp) {
         S_CHECK(mp_addvars(mp, &fdat->dual.vars.s));
      }

      cdat_varname_end(cdat);
   }

   /* Reserve n_v+2 equations (new obj function + constraints + evaluation) */
   S_CHECK(rctr_reserve_equs(ctr, ncons+2));

   return OK;
}

int fenchel_gen_cons(CcfFenchelData *fdat, Model *mdl)
{
   Container *ctr = &mdl->ctr;
   RhpContainerData *cdat = (RhpContainerData *)ctr->data;
   MathPrgm *mp_dst = fdat->mp_dst;

   /* ---------------------------------------------------------------------
    * Perform the computation of   M tilde_y
    * --------------------------------------------------------------------- */

   unsigned n_y = fdat->primal.ydat.n_y;
   memset(fdat->tmpvec, 0, sizeof(double)*n_y);

   if (fdat->primal.ydat.has_shift && fdat->primal.is_quad) {
      S_CHECK(rhpmat_atxpy(&fdat->M, fdat->primal.ydat.tilde_y, fdat->tmpvec));
   }

   char *equ_name;
   unsigned ovf_name_idxlen = strlen(fdat->equvar_basename);
   NEWNAME2(equ_name, fdat->equvar_basename, ovf_name_idxlen, "_set");
   cdat_equname_start(cdat, equ_name);

   /* Keep track of the   */
   rhp_idx w_curvi = fdat->dual.vars.w.start;
   rhp_idx v_start = fdat->dual.vars.v.start;
   rhp_idx s_start = fdat->dual.vars.s.start;

   RhpSense sense = fdat->primal.sense;

   SpMat * restrict At = &fdat->At, * restrict D = &fdat->D;
   SpMat * restrict B_lin = &fdat->B_lin;
   bool has_set = fdat->primal.has_set;
   bool is_quad = fdat->primal.is_quad;

   double * restrict var_ub = fdat->primal.ydat.var_ub;

   MALLOC_(fdat->cons_gen, bool, n_y);
   bool *cons_gen = fdat->cons_gen;

   rhp_idx ei_cons_start = cdat->total_m;

   Cone * restrict cones_y = fdat->primal.ydat.cones_y;
   void * restrict *cones_y_data = fdat->primal.ydat.cones_y_data;

   for (unsigned i = 0; i < n_y; ++i) {
      rhp_idx ei_new = IdxNA;
      Equ *e;

      /* -------------------------------------------------------------------
       * Get the polar (or dual for inf OVF) for the equation
       * ------------------------------------------------------------------- */

      Cone cons_cone;
      void* cons_cone_data;

      switch (sense) {
      case RhpMax:
         S_CHECK(cone_polar(cones_y[i], cones_y_data[i], &cons_cone, &cons_cone_data));
         break;
      case RhpMin:
         S_CHECK(cone_dual(cones_y[i], cones_y_data[i], &cons_cone, &cons_cone_data));
         break;
      default: return error_runtime();
      }

      /* -------------------------------------------------------------------
       * If there is an equality constraint for y, then the polar/dual is the
       * whole space and there are no constraint to add. Otherwise, add the
       * constraint.
       * ------------------------------------------------------------------- */

       /* TODO(xhub) this should not be here. This has to be detected earlier*/
      if (cons_cone == CONE_R) {
         cons_gen[i] = false;
         fdat->skipped_cons = true;
         continue;
      }
      cons_gen[i] = true;

      S_CHECK(rctr_add_equ_empty(ctr, &ei_new, &e, ConeInclusion, cons_cone));

      if (mp_dst) {
         S_CHECK(mp_addconstraint(mp_dst, ei_new));
      }

      /* -------------------------------------------------------------------
       * Compute the contribution to the linear size from A and D.
       * ------------------------------------------------------------------- */

      size_t size_At = 0;
      size_t size_d = 0;

      if (fdat->primal.has_set && At->ppty) {
         /*  TODO(xhub) we need the row of the transpose of A, hence the
          *  column? */
         EMPMAT_GET_CSR_SIZE(*At, i, size_At);
      }
      if (is_quad) {
         EMPMAT_GET_CSR_SIZE(*D, i, size_d);
      }

      unsigned *arg_idx;
      unsigned args_idx_len;
      unsigned single_idx;
      double single_val;
      double *lcoeffs = NULL;
      S_CHECK(rhpmat_row_needs_update(B_lin, i, &single_idx, &single_val, &args_idx_len,
                                      &arg_idx, &lcoeffs));

      if (args_idx_len == 0) {
         printout(PO_DEBUG, "[Warn] %s :: row %d is empty\n", __func__, i);
         continue;
      }
      size_t size_new_lequ = size_At + size_d;

      /* │Add - M tilde_y to the constant part */
      equ_add_cst(e, -fdat->tmpvec[i]);

      if (isfinite(var_ub[i])) {
         size_new_lequ++;
      }

      // TODO: revive that if needed
//      /* -------------------------------------------------------------------
//       * Loop over the row of B_lin to estimate the size of the linear part
//       * ------------------------------------------------------------------- */
//
//      for (unsigned j = 0; j < args_idx_len; ++j) {
//         int eqidx = equ_idx[arg_idx[j]];
//         if (!valid_ei(eqidx)) { size_new_lequ++; continue; }
//
//         Lequ *lequ = ctr->equs[eqidx].lequ;
//         if (lequ) { size_new_lequ += lequ->len - 1; }
//      }

      /* -------------------------------------------------------------------
       * Allocate the linear part and get a handle on it
       * ------------------------------------------------------------------- */

      S_CHECK(lequ_reserve(e->lequ, size_new_lequ));
      Lequ *le = e->lequ;

      /* --------------------------------------------------------------------
       * Add - A^T v - D s into the linear equation le
       *
       * TODO: what happens if not all v are generated?
       * -------------------------------------------------------------------- */

      unsigned offset = 0;

      if (has_set && At->ppty) {
         rhpmat_copy_row_neg(At, i, le->coeffs, le->vis, &offset, v_start);
      }

      if (is_quad) {
         rhpmat_copy_row_neg(D, i, le->coeffs, le->vis, &offset, s_start);
      }

      le->len = offset;

      /* --------------------------------------------------------------------
       * Add the upper bound on y (if it exists) as a -I v_{ub}
       * -------------------------------------------------------------------- */

      if (isfinite(var_ub[i])) {
          S_CHECK(lequ_add(le, w_curvi, -1));
          w_curvi++;
      }

      if (fdat->finalize_equ) {
         S_CHECK(cmat_sync_lequ(ctr, e));
      }

   }

   /* All the equations have been added  */
   cdat_equname_end(cdat);
   aequ_ascompact(&fdat->dual.cons, cdat->total_m - ei_cons_start, ei_cons_start);

   return OK;
}

/**
 * @brief Add objective function 
 *
 *   o   \f$ <c,v> + 0.5 <s, Js>\f$ in the simplest case
 *   o   \f$ - 0.5 <tilde_y, M tilde_y> + <c - A tilde_y, v> + 0.5 <s, Js> \f$
 *       if y has to be shifted by - tilde_y.
 *
 * @warning The term <G(F(x)), tilde_y> needs to be added by the callee
 *
 * @param  fdat  the fenchel data
 * @param  mdl   the model
 *
 * @return       the error code
 */
int fenchel_gen_objfn(CcfFenchelData *fdat, Model *mdl)
{
   Container *ctr = &mdl->ctr;
   Equ *e_objfn;

   S_CHECK(rctr_add_equ_empty(ctr, &fdat->dual.ei_objfn, &e_objfn, Mapping, CONE_NONE));

   if (fdat->primal.has_set) {

      /* Add < a, v > + < ub, w> */
      S_CHECK(rctr_equ_addnewlvars(ctr, e_objfn, &fdat->dual.vars.v, fdat->dual.a));
      S_CHECK(rctr_equ_addnewlvars(ctr, e_objfn, &fdat->dual.vars.w, fdat->dual.ub));

   }

   /* - 0.5 <tilde_y, M tilde_y> + < b, tilde_y> */
   if (fdat->primal.ydat.has_shift) {

      // cblas_ddot
      double b_tilde_y = 0; double *y = fdat->primal.ydat.tilde_y, *b = fdat->b_lin;
      if (b) {
         for (unsigned i = 0, len = fdat->primal.ydat.n_y; i < len; ++i) {
            b_tilde_y += y[i]*b[i];
         }
      }

      equ_add_cst(e_objfn, b_tilde_y-fdat->primal.ydat.shift_quad_cst);
   }

   /* Add 0.5 <s, Js> */
   if (fdat->primal.is_quad) {
      S_CHECK(rctr_equ_add_quadratic(ctr, e_objfn, &fdat->J, &fdat->dual.vars.s, 1.));
   }

   return OK;
}

void fdat_empty(CcfFenchelData *fdat)
{
   rhpmat_free(&fdat->At);
   rhpmat_free(&fdat->D);
   rhpmat_free(&fdat->J);
   rhpmat_free(&fdat->M);
   rhpmat_free(&fdat->B_lin);

   FREE(fdat->dual.a);
   FREE(fdat->dual.ub);
   FREE(fdat->b_lin);
   FREE(fdat->tmpvec);

   FREE(fdat->cons_gen);
   FREE(fdat->equvar_basename);

   ydat_empty(&fdat->primal.ydat);
}
