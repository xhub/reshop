#include <string.h>

#include "fenchel_common.h"
#include "ctr_rhp.h"
#include "ctr_rhp_add_vars.h"
#include "itostr.h"
#include "lequ.h"
#include "macros.h"
#include "mathprgm.h"
#include "mdl.h"
#include "ovf_common.h"
#include "printout.h"
#include "status.h"

static void ydat_empty(yData *ydat)
{
   if (!ydat) { return; }

   FREE(ydat->cones_y);
   FREE(ydat->cones_y_data);
   FREE(ydat->tilde_y);
}

static NONNULL int ydat_init(yData *ydat, unsigned n_y)
{
   int status;

   memset(ydat, 0, sizeof(*ydat));
   MALLOC_EXIT(ydat->cones_y, Cone, n_y);
   MALLOC_EXIT(ydat->cones_y_data, void*, n_y);
   MALLOC_EXIT(ydat->tilde_y, double, 2*n_y);

   ydat->var_ub = &ydat->tilde_y[n_y];

   ydat->has_shift = false;

   return OK;

_exit:
   ydat_empty(ydat);
   return status;
}

int fdat_init(CcfFenchelData * restrict fdat, OvfType type,
              const OvfOps *ops, OvfOpsData ovfd, Model *mdl)
{
   int status = OK;

   fdat->type = type;
   fdat->ops = ops;
   fdat->ovfd = ovfd;
   unsigned n_args;
   S_CHECK(ops->get_nargs(ovfd, &n_args));

   const unsigned n_y = ops->size_y(ovfd, n_args);
   fdat->nargs = n_args;

   if (n_y == 0) {
      const char *ovf_name = ops->get_name(ovfd);
      error("[ccflib/primal] the number of variable associated with the CCF '%s' "
            "of type %s is 0. This should never happen\n."
            "Check the OVF definition if it is a custom one, or file a bug\n",
            ovf_name, ops->get_name(ovfd));
      return Error_UnExpectedData;
   }

   S_CHECK(ydat_init(&fdat->ydat, n_y));

   rhpmat_null(&fdat->At);
   rhpmat_null(&fdat->D);
   rhpmat_null(&fdat->J);
   rhpmat_null(&fdat->B_lin);

   fdat->a = NULL;
   fdat->b_lin = NULL;

   /* We look for A and c such that A y - c belongs to K
    * We ask for the CSC version of A to easily iterate of the rows of A^T */
   rhpmat_set_csc(&fdat->At);
   S_CHECK(ops->get_set_nonbox(ovfd, &fdat->At, &fdat->a, true));

   /* Get the Cholesky factorization of M = D^TJD */
   S_CHECK(ops->get_D(ovfd, &fdat->D, &fdat->J));

   unsigned n_cons, n_v = 0;

   if (fdat->At.ppty) {
     fdat->has_set = true;
     S_CHECK_EXIT(rhpmat_get_size(&fdat->At, &n_v, &n_cons));
     assert(n_v > 0 && n_cons > 0);

   } else if (!fdat->has_set) {

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
   return OK;

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

   REALLOC(fdat->a, double, nb_vars);
   CALLOC(fdat->tmpvec, double, MAX(n_y, n_v));

_exit:
   return status;
}

int fenchel_find_yshift(CcfFenchelData *fdat)
{
   yData *ydat = &fdat->ydat;
   const OvfOps *ops = fdat->ops;
   OvfOpsData ovfd = fdat->ovfd;

   for (unsigned i = 0, n_y = fdat->ydat.n_y; i < n_y; ++i) {
      /*  TODO(xhub) allow more general cones here */
      ydat->cones_y_data[i] = NULL;
      double lb = ops->get_var_lb(ovfd, i);
      double ub = ops->get_var_ub(ovfd, i);

      bool lb_fin = isfinite(lb);
      bool ub_fin = isfinite(ub);

      /* Let's be on the safe side   */
      if (lb_fin && ub_fin) {

         /* Check whether we have an equality constraint  */
         if (fabs(ub - lb) < DBL_EPSILON) {
            if (fabs(lb) < DBL_EPSILON) {
               ydat->tilde_y[i] = 0.;
            } else {
               ydat->tilde_y[i] = lb;
               ydat->has_shift = true;
            }

            fdat->has_set = true;
            ydat->cones_y[i] = CONE_0;
            ydat->var_ub[i] = NAN;
            continue;

         }
      } else if (ub < lb) {
         error("[ccflib/fenchel] ERROR: the bounds on the %u-th variables are not consistent: "
               "lb = %e > %e = ub\n", i, lb, ub);
         return Error_InvalidValue;
      }

      if (lb_fin) {
         fdat->has_set = true;
         ydat->cones_y[i] = CONE_R_PLUS;

         if (fabs(lb) < DBL_EPSILON) {
            ydat->tilde_y[i] = 0.;
         } else {
            ydat->tilde_y[i] = lb;
            ydat->has_shift = true;
         }

         if (ub_fin) {
            ydat->var_ub[i] = ub;
            ydat->n_y_ub++;
         } else {
            ydat->var_ub[i] = NAN;
         }

      } else if (ub_fin) {

         fdat->has_set = true;
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

   return OK;
}

int fenchel_apply_yshift(CcfFenchelData *fdat)
{
   yData *ydat = &fdat->ydat;
   if (!ydat->has_shift) { return OK; }

   int status = OK;
   const OvfOps *ops = fdat->ops;
   OvfOpsData ovfd = fdat->ovfd;

   SpMat M;
   rhpmat_null(&M);

   double quad_cst = NAN;

   /* -------------------------------------------------------------
    * 1.a Perform a -= A tilde_y  for the nonbox polyhedral constraints
    * ------------------------------------------------------------- */

   S_CHECK(rhpmat_atxpy(&fdat->At, fdat->ydat.tilde_y, fdat->tmpvec));

   for (unsigned i = 0; i < c_bnd_start; ++i) {
      fdat->a[i] -= fdat->tmpvec[i];
   }
   memset(fdat->tmpvec, 0, c_bnd_start*sizeof(double));

   /* -------------------------------------------------------------
       * 1.b 
       * the shift on the upper bound on y is easier: if it is finite,
       * then subtract tilde_y
       *
       * Since some constraints may not have multipliers, we make use of
       * v_bnd_revidx
       * ------------------------------------------------------------- */

   rhp_idx * restrict v_bnd_revidx = fdat->vdat.revidx_v_ub;
   for (unsigned i = fdat->c_bnd_start, end = fdat->c_end, j = 0; i < end; ++i, ++j) {
      c[i] -= fdat->ydat.tilde_y[v_bnd_revidx[j]];
   }

   /* -------------------------------------------------------------
       * Add -.5 <tilde_y, M tilde_y> to the RHS to take into account M
       * ------------------------------------------------------------- */

   if (fdat->is_quad) {
      S_CHECK(ops->get_M(ovfd, &M));

      quad_cst = .5 * rhpmat_evalquad(&M, ydat->tilde_y);

      if (!isfinite(quad_cst)) {
         error("[ccflib:fenchel] ERROR: the quadratic constant "
               "from the shift is not finite: val = %f\n", quad_cst);
         return Error_MathError;
      }
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
   unsigned n_v = fdat->vdat.n_v;
   unsigned n_vars = fdat->ydat.n_y_ub + n_v;
   if (fdat->is_quad) {
      n_vars += fdat->ydat.n_y;
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
   MathPrgm *mp = fdat->mp;

   unsigned start_new_vars = cdat->total_n;

   /* NAMING: this should not exists */
   char *ovf_name_idx = NULL;
   const char *ovf_name = ops->get_name(ovfd);
   size_t ovf_namelen = strlen(ovf_name);

   MALLOC_(ovf_name_idx, char, ovf_namelen + 26);
   strcpy(ovf_name_idx, ovf_name);
   strcat(ovf_name_idx, "_");
   strcat(ovf_name_idx, fdat->type == OvfType_Ovf ? "ovf" : "ccf");
   strcat(ovf_name_idx, "_");
   unsignedtostr((unsigned)fdat->idx, sizeof(unsigned), &ovf_name_idx[strlen(ovf_name_idx)], 20, 10);
   unsigned ovf_name_idxlen = strlen(ovf_name_idx);
   fdat->equvar_basename = ovf_name_idx;

   /* Add the v variable */
   unsigned v_start = 0, v_bnd_start = 0, c_bnd_start = 0, c_end = 0; /* init for cwarn */
   if (fdat->has_set) {
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

      NEWNAME2(v_name, ovf_name_idx, ovf_name_idxlen, "_v");
      cdat_varname_start(cdat, v_name);

      double * restrict a = fdat->a;
      RhpSense sense = fdat->sense;

      for (unsigned j = 0; j < n_v; ++j) {
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

      /* -------------------------------------------------------------------
       * Add the multipliers for the upper bounds
       *
       * If the OVF is a sup, the multipliers belong to the polar cone
       * Otherwise, we use a trick and the multiplier belongs to the dual cone
       *
       * OUTPUT:
       *   c filled from c_bnd_start until ...
       *   v_bnd_revidx of size c_end-c_bnd_start maps the index of c to the index of u
       * ------------------------------------------------------------------- */

      NEWNAME2(v_name, ovf_name_idx, ovf_name_idxlen, "_vbnd");
      cdat_varname_start(cdat, v_name);
      v_bnd_start = cdat->total_n;
      c_bnd_start = n_v_actual;

      double * restrict yvar_ub = fdat->ydat.var_ub;
      unsigned n_y = fdat->ydat.n_y;

      for (unsigned i = 0, j = 0; i < n_y; ++i) {
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

            a[n_v_actual] = yvar_ub[i];
            v_bnd_revidx[j] = i;
            j++;
            n_v_actual++;
         }
      }

      cdat_varname_end(cdat);

      c_end = n_v_actual;
      n_v = cdat->total_n - v_start;
   }

   /* Add the s variable if needed  */
   if (fdat->is_quad) {
      unsigned s_start = cdat->total_n;
      char *s_name;
      NEWNAME2(s_name, ovf_name_idx, ovf_name_idxlen, "_s");
      cdat_varname_start(cdat, s_name);

      unsigned n_y = fdat->ydat.n_y;
      S_CHECK(rctr_add_free_vars(ctr, n_y, &fdat->s_var));
      if (mp) {
         S_CHECK(mp_addvars(mp, &fdat->s_var));
      }

      cdat_varname_end(cdat);
   }

   avar_setcompact(&fdat->vdat.v, n_v, v_start);

   /* Reserve n_v+2 equations (new obj function + constraints + evaluation) */
   S_CHECK(rctr_reserve_equs(ctr, n_v+2));

   return OK;
}

int fenchel_gen_equs(CcfFenchelData *fdat, Model *mdl)
{
   Container *ctr = &mdl->ctr;
   RhpContainerData *cdat = (RhpContainerData *)ctr->data;
   const OvfOps *ops = fdat->ops;
   OvfOpsData ovfd = fdat->ovfd;
   MathPrgm *mp = fdat->mp;

   /* ---------------------------------------------------------------------
    * Perform the computation of   M tilde_y
    * --------------------------------------------------------------------- */

   if (fdat->ydat.has_shift && fdat->is_quad) {
      S_CHECK(rhpmat_atxpy(&fdat->M, fdat->ydat.tilde_y, fdat->tmpvec));
   }

   char *equ_name;
   unsigned ovf_name_idxlen = strlen(fdat->equvar_basename);
   NEWNAME2(equ_name, fdat->equvar_basename, ovf_name_idxlen, "_set");
   cdat_equname_start(cdat, equ_name);
   /* Keep track of the   */
   rhp_idx idx_v_bnd = fdat->vdat.start_vi_v_ub;

   rhp_idx v_start = fdat->vdat.v.start;
   rhp_idx s_start = fdat->s_var.start;

   RhpSense sense = fdat->sense;

   SpMat * restrict At = &fdat->At, * restrict D = &fdat->D;
   SpMat * restrict B_lin = &fdat->B_lin;
   bool has_set = fdat->has_set;
   bool is_quad = fdat->is_quad;

   double * restrict var_ub = fdat->ydat.var_ub;
   unsigned n_y = fdat->ydat.n_y;

   MALLOC_(fdat->equ_gen, bool, n_y);
   bool *equ_gen = fdat->equ_gen;

   fdat->ei_start = cdat->total_n;

   for (unsigned i = 0; i < n_y; ++i) {
      rhp_idx ei_new = IdxNA;
      Equ *e;

      /* -------------------------------------------------------------------
       * Get the polar (or dual for inf OVF) for the equation
       * ------------------------------------------------------------------- */

      Cone equ_cone;
      void* equ_cone_data;

      switch (sense) {
      case RhpMax:
         S_CHECK(cone_polar(fdat->ydat.cones_y[i], fdat->ydat.cones_y_data[i], &equ_cone, &equ_cone_data));
         break;
      case RhpMin:
         S_CHECK(cone_dual(fdat->ydat.cones_y[i], fdat->ydat.cones_y_data[i], &equ_cone, &equ_cone_data));
         break;
      default: return error_runtime();
      }

      /* -------------------------------------------------------------------
       * If there is an equality constraint for y, then the polar/dual is the
       * whole space and there are no constraint to add. Otherwise, add the
       * constraint.
       * ------------------------------------------------------------------- */

       /* TODO(xhub) this should not be here. This has to be detected earlier*/
      if (equ_cone == CONE_R) {
         equ_gen[i] = false;
         continue;
      }
      equ_gen[i] = true;

      S_CHECK(rctr_add_equ_empty(ctr, &ei_new, &e, ConeInclusion, equ_cone));

      if (mp) {
         S_CHECK(mp_addconstraint(mp, ei_new));
      }

      /* -------------------------------------------------------------------
       * Compute the contribution to the linear size from A and D.
       * ------------------------------------------------------------------- */

      size_t size_At = 0;
      size_t size_d = 0;

      if (fdat->has_set && At->ppty) {
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
      S_CHECK(rhpmat_row(B_lin, i, &single_idx, &single_val, &args_idx_len,
                         &arg_idx, &lcoeffs));

      if (args_idx_len == 0) {
         printout(PO_DEBUG, "[Warn] %s :: row %d is empty\n", __func__, i);
         continue;
      }
      size_t size_new_lequ = size_At + size_d;

      /* */
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
          S_CHECK(lequ_add(le, idx_v_bnd, -1));
          idx_v_bnd++;
      }

   }

   /* All the equations have been added  */
   cdat_equname_end(cdat);


}
