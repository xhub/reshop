#include <assert.h>
#include <float.h>
#include <math.h>
#include <stdbool.h>
#include <string.h>

#include "cmat.h"
#include "compat.h"
#include "consts.h"
#include "container.h"
#include "ctr_rhp.h"
#include "equ_modif.h"
#include "filter_ops.h"
#include "lequ.h"
#include "macros.h"
#include "mathprgm.h"
#include "mdl.h"
#include "ctrdat_rhp.h"
#include "ctr_rhp_add_vars.h"
#include "mdl_timings.h"
#include "ovf_common.h"
#include "ovf_fenchel.h"
#include "ovf_options.h"
#include "ovfinfo.h"
#include "printout.h"
#include "rhp_LA.h"
#include "status.h"
#include "timings.h"

//#define DEBUG_OVF_PRIMAL 

int ovf_fenchel(Model *mdl, enum OVF_TYPE type, union ovf_ops_data ovfd)
{
   double start = get_thrdtime();
   int status = OK;
   const struct ovf_ops *op;

   Container *ctr = &mdl->ctr;

   struct ctrdata_rhp *cdat = (struct ctrdata_rhp *)ctr->data;

   switch (type) {
   case OVFTYPE_OVF:
      op = &ovfdef_ops;
      break;
   default:
      TO_IMPLEMENT("user-defined OVF is not implemented");
   }

   rhp_idx vi_ovf = op->get_ovf_vidx(ovfd);
   if (!valid_vi(vi_ovf)) {
      error("%s :: the OVF variable is not set! Value = %d\n", __func__, vi_ovf);
      return Error_InvalidValue;
   }

   char ovf_name[SHORT_STRING];
   S_CHECK(ctr_copyvarname(ctr, vi_ovf, ovf_name, sizeof ovf_name));
   size_t ovf_namelen = strlen(ovf_name);

   MathPrgm *mp = NULL;
   RhpSense sense;

   S_CHECK(ovf_get_mp_and_sense(mdl, vi_ovf, &mp, &sense));

   bool has_set = false;
   struct ovf_ppty ovf_ppty;
   op->get_ppty(ovfd, &ovf_ppty);
   const bool is_quad = ovf_ppty.quad;

   /* ---------------------------------------------------------------------
    * Test compatibility between OVF and problem type
    * --------------------------------------------------------------------- */

   S_CHECK(ovf_compat_types(op->get_name(ovfd), ovf_name, sense, ovf_ppty.sense));

   /* ---------------------------------------------------------------------
    * 1. Analyze the QP structure
    * 2. Inject new variables
    * 3. Replace the rho variable
    * 4. Add constraints 
    * --------------------------------------------------------------------- */

   SpMat At, D, J, B_lin;
   rhpmat_null(&At);
   rhpmat_null(&D);
   rhpmat_null(&J);
   rhpmat_null(&B_lin);

   double *c = NULL, *b_lin = NULL;

   /* We look for A^T and c such that A^T x - c belongs to K */
   rhpmat_set_csc(&At);
   S_CHECK(op->get_set_nonbox(ovfd, &At, &c, true));

   /*  Get the Cholesky factorization of M = D^TJD */
   S_CHECK(op->get_D(ovfd, &D, &J));

   /* ----------------------------------------------------------------------
    * Get the indices of F(x):
    *   - indices
    *   - types   
    *   - n_args   number of arguments
    * ---------------------------------------------------------------------- */

   Avar *ovf_args;
   S_CHECK(op->get_args(ovfd, &ovf_args));
   unsigned n_args = avar_size(ovf_args);

   /* ----------------------------------------------------------------------
    * Analyze the QP structure:
    * - Find the convex cone K to which u (or u - \tilde{u}) belong
    * - Complete the constraints with 
    *
    * In step 1.1, we look for u in R₊. If this is not the case, we look for a
    * finite lower bound. If there is none, we look for a finite upper-bound.
    *
    * \TODO(xhub) support Mu - \tilde{u} \in K (cf Ben-Tal and Den ...)?
    *
    * OUTPUT:
    *   - u_shift    the value of \tilde{u}
    *   - var_ubnd   the bound of the variable
    *   - cones_u    the cone K
    * ---------------------------------------------------------------------- */

   size_t n_ubnd = 0;
   /* Do we need to shift the u to make it fit in the cone? */
   bool has_shift = false;

   double *u_shift = NULL, *var_ubnd = NULL, *mat_u_tilde = NULL;
   enum cone *cones_u = NULL;
   void **cones_u_data = NULL;
   unsigned *z_bnd_revidx = NULL;

   const unsigned n_u = op->size_u(ovfd, n_args);

   if (n_u == 0) {
      error("[ccf:primal] the number of variable associated with the CCF '%s' "
            "of type %s is 0. This should never happen\n."
            "Check the OVF definition if it is a custom one, or file a bug\n",
            ovf_name, op->get_name(ovfd));
      return Error_UnExpectedData;
   }

   MALLOC_EXIT(cones_u, enum cone, n_u);
   MALLOC_EXIT(cones_u_data, void*, n_u);
   MALLOC_EXIT(u_shift, double, 2*n_u);
   var_ubnd = &u_shift[n_u];
   MALLOC_EXIT(z_bnd_revidx, unsigned, n_u);

   for (size_t i = 0; i < n_u; ++i) {
      /*  TODO(xhub) allow more general cones here */
      cones_u_data[i] = NULL;
      double lb = op->get_var_lb(ovfd, i);
      double ub = op->get_var_ub(ovfd, i);

      bool lb_fin = isfinite(lb);
      bool ub_fin = isfinite(ub);

      /* Let's be on the safe side   */
      if (lb_fin && ub_fin) {

         /* Check whether we have an equality constraint  */
         if (fabs(ub - lb) < DBL_EPSILON) {
            if (fabs(lb) < DBL_EPSILON) {
               u_shift[i] = 0.;
            } else {
               u_shift[i] = lb;
               has_shift = true;
            }

            has_set = true;
            cones_u[i] = CONE_0;
            var_ubnd[i] = NAN;
            continue;

         }
      } else if (ub < lb) {
         error("[OVF/fenchel] ERROR: the bounds on u_%zu are not consistent: "
               "lb = %e > %e = ub\n", i, lb, ub);
         status = Error_InvalidValue;
         goto _exit;
      }

      if (lb_fin) {
         has_set = true;
         cones_u[i] = CONE_R_PLUS;
         if (fabs(lb) < DBL_EPSILON) {
            u_shift[i] = 0.;
         } else {
            u_shift[i] = lb;
            has_shift = true;
         }
         if (ub_fin) {
            var_ubnd[i] = ub;
            n_ubnd++;
         } else {
            var_ubnd[i] = NAN;
         }
      } else if (ub_fin) {
         has_set = true;
         cones_u[i] = CONE_R_MINUS;
         var_ubnd[i] = NAN;
         if (fabs(ub) < DBL_EPSILON) {
            u_shift[i] = 0.;
         } else {
            u_shift[i] = ub;
            has_shift = true;
         }
      } else {
         cones_u[i] = CONE_R;
         var_ubnd[i] = NAN;
         u_shift[i] = 0.;
      }
   }


   /* ---------------------------------------------------------------------
    * Add the new variables z and w:
    * - z has the same dimension has the number of constraints. We have two
    *   types of constraints:
    *     + The ones given by Ax - b ∈ K_c
    *     + The finite upper bound on u, collected during the anaysis phase in
    *       var_ubnd
    *   z belongs to the polar (or dual for inf OVF) of K_c and R_+ (upper bound)
    * - w has the same size as u when there is a quadratic part
    * --------------------------------------------------------------------- */

   unsigned n_constr, n_z = 0;

   if (At.ppty) {
     has_set = true;
     S_CHECK_EXIT(rhpmat_get_size(&At, &n_z, &n_constr));
     assert(n_z > 0 && n_constr > 0);
   } else if (!has_set) {
      if (D.ppty) {
         S_CHECK_EXIT(rhpmat_get_size(&D, &n_z, &n_constr));
         n_z = 0;
      } else {
         error("%s :: Fatal Error: no OVF set given and no "
                 "quadratic part -> the OVF function is unbounded!\n", __func__);
         status = Error_EMPIncorrectInput;
         goto _exit;
      }
   }

   unsigned nb_vars = n_ubnd + n_z;
   if (is_quad) {
      nb_vars += n_u;
   }

   if (nb_vars == 0) {
      error("[ovf/primal] the OVF with variable '%s' has no constraints and no "
            "quadratic part. It is then unbounded\n", ovf_name);
      status = Error_ModelUnbounded;
      goto _exit;
   }

   S_CHECK_EXIT(rctr_reserve_vars(ctr, nb_vars));

   /* ----------------------------------------------------------------------
    * Resize c to include the upper bound
    * ---------------------------------------------------------------------- */

   REALLOC_EXIT(c, double, nb_vars);
   CALLOC_EXIT(mat_u_tilde, double, MAX(n_u, n_z));

   /* ---------------------------------------------------------------------
    * If we want to initialize the values of the new variables, this is the
    * start for the new variables
    * --------------------------------------------------------------------- */
   unsigned start_new_vars = cdat->total_n;

   /* Add the z variable */
   unsigned z_start = 0, z_bnd_start = 0, c_bnd_start = 0, c_end = 0; /* init for cwarn */
   if (has_set) {
      z_start = cdat->total_n;
      unsigned idx_z = 0;
      char *z_name;

      /* -------------------------------------------------------------------
       * Add the multipliers for the nonbox constraints. This is always well
       * defined
       *
       * If the OVF is a sup, the multipliers belong to the polar cone
       * Otherwise, we use a trick and the multiplier belongs to the dual cone
       * ------------------------------------------------------------------- */

      NEWNAME(z_name, ovf_name, ovf_namelen, "_z");
      cdat_varname_start(cdat, z_name);
      for (unsigned j = 0; j < n_z; ++j) {
         enum cone cone;
         void *cone_data;
         S_CHECK_EXIT(op->get_cone_nonbox(ovfd, j, &cone, &cone_data));
         rhp_idx vi = IdxNA;

         switch (ovf_ppty.sense) {
         case RhpMax:
            S_CHECK_EXIT(rctr_add_multiplier_polar(ctr, cone, cone_data, &vi));
            break;
         case RhpMin:
            S_CHECK_EXIT(rctr_add_multiplier_dual(ctr, cone, cone_data, &vi));
            break;
         default: status = error_runtime(); goto _exit;
         }

         if (valid_vi(vi)) {
            c[idx_z] = c[j];
            idx_z++;
            if (mp) {
               S_CHECK_EXIT(mp_addvar(mp, vi));
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
       *   z_bnd_revidx of size c_end-c_bnd_start maps the index of c to the index of u
       * ------------------------------------------------------------------- */

      NEWNAME(z_name, ovf_name, ovf_namelen, "_zbnd");
      cdat_varname_start(cdat, z_name);
      z_bnd_start = cdat->total_n;
      c_bnd_start = idx_z;

      for (unsigned i = 0, j = 0; i < n_u; ++i) {
         if (isfinite(var_ubnd[i])) {
            rhp_idx vidx = IdxNA;

            switch (ovf_ppty.sense) {
            case RhpMax:
               S_CHECK_EXIT(rctr_add_multiplier_polar(ctr, CONE_R_MINUS, NULL, &vidx));
               break;
            case RhpMin:
               S_CHECK_EXIT(rctr_add_multiplier_dual(ctr, CONE_R_MINUS, NULL, &vidx));
               break;
            default: status = error_runtime(); goto _exit;
            }

            assert(valid_vi(vidx));
            if (mp) { S_CHECK_EXIT(mp_addvar(mp, vidx)); }

            c[idx_z] = var_ubnd[i];
            z_bnd_revidx[j] = i;
            j++;
            idx_z++;
         }
      }

      cdat_varname_end(cdat);

      c_end = idx_z;
      n_z = cdat->total_n - z_start;
   }

   /* Add the w variable if needed  */
   unsigned w_start = 0; /* init just to silence compiler warning */
   Avar w_var;
   if (is_quad) {
      w_start = cdat->total_n;
      char *w_name;
      NEWNAME(w_name, ovf_name, ovf_namelen, "_w");
      cdat_varname_start(cdat, w_name);

      S_CHECK_EXIT(rctr_add_free_vars(ctr, n_u, &w_var));
      if (mp) {
         S_CHECK_EXIT(mp_addvars(mp, &w_var));
      }

      cdat_varname_end(cdat);
   }

   /* Check whether we have B_lin or b_lin  */
   S_CHECK_EXIT(op->get_lin_transformation(ovfd, &B_lin, &b_lin));

   if (B_lin.ppty) {
      unsigned n_constr2, n_args2;
      S_CHECK_EXIT(rhpmat_get_size(&B_lin, &n_args2, &n_constr2));

      if (n_constr2 != n_u) {
         error("%s :: incompatible size: B_lin and A^T should have the same number of"
               " rows, but there are %d rows in B_lin and %d in A^T\n", __func__,
               n_constr2, n_u);
         status = Error_Inconsistency;
         goto _exit;
      }

      if (n_args2 != n_args) {
         error("%s :: incompatible size: the number of arguments (%d) and the "
               "number of columns in B_lin (%d) should be the same\n", __func__,
               n_args, n_args2);
         status = Error_Inconsistency;
         goto _exit;
      }
   }

   /* Reserve n_u+2 equations (new obj function + constraints + evaluation) */
   S_CHECK_EXIT(rctr_reserve_equs(ctr, n_u+2));


   rhp_idx *equ_idx = NULL;
   double *coeffs = NULL;
   S_CHECK_EXIT(op->get_mappings(ovfd, &equ_idx));
   S_CHECK_EXIT(ovf_process_indices(mdl, ovf_args, equ_idx));
   S_CHECK_EXIT(op->get_coeffs(ovfd, &coeffs));

   SpMat M;
   rhpmat_null(&M);

   double quad_cst = NAN;

   if (has_shift) {

      /* -------------------------------------------------------------
       * 1.a perform c -= A u_tilde for the nonbox polyhedral constraints
       * ------------------------------------------------------------- */

      S_CHECK_EXIT(rhpmat_atxpy(&At, u_shift, mat_u_tilde));

      for (unsigned i = 0; i < c_bnd_start; ++i) {
         c[i] -= mat_u_tilde[i];
      }
      memset(mat_u_tilde, 0, c_bnd_start*sizeof(double));

      /* -------------------------------------------------------------
       * 1.b 
       * the shift on the upper bound on u is easier: if it is finite,
       * then subtract the u_shift
       *
       * Since some constraints may not have multipliers, we make use of
       * z_bnd_revidx
       * ------------------------------------------------------------- */

      for (unsigned i = c_bnd_start, j = 0; i < c_end; ++i, ++j) {
         c[i] -= u_shift[z_bnd_revidx[j]];
      }

      /* -------------------------------------------------------------
       * Add -.5 <u_tilde, M u_tilde> to the RHS
       * ------------------------------------------------------------- */

      if (is_quad) {
         S_CHECK_EXIT(op->get_M(ovfd, &M));

         quad_cst = rhpmat_evalquad(&M, u_shift);

         if (!isfinite(quad_cst)) {
            error("[ccflib:fenchel] ERROR: the quadratic constant "
                  "from the shift is not finite: val = %f\n", quad_cst);
            status = Error_MathError;
            goto _exit;
         }
      }

   }

   /* ---------------------------------------------------------------------
    * 2. Replace the rho variable by
    *   o   <c,z> + 0.5 <w, Jw> in the simplest case
    *   o   <G(F(x)), u_tilde> - .5 <u_tilde, M u_tilde> + <c - A u_tilde, z> +
    *       0.5 <w, Jw> if u has to be shifted by - u_tilde.
    * --------------------------------------------------------------------- */

   void *iter = NULL;
   unsigned counter = 0;

   do {
      rhp_idx ei_new;
      double ovf_coeff;


      S_CHECK_EXIT(ovf_replace_var(mdl, vi_ovf, &iter, &ovf_coeff, &ei_new, n_z));

      Equ *eq = &ctr->equs[ei_new];

      /* Replace var by lin */
      if (has_set) {

         Avar z;
         avar_setcompact(&z, n_z, z_start);

         if (has_shift) {

            /* ----------------------------------------------------------------
             * Add < G(F(x)), u_tilde >
             * ---------------------------------------------------------------- */

            S_CHECK_EXIT(rctr_equ_add_dot_prod_cst(ctr, eq, u_shift, n_u, &B_lin, b_lin, coeffs,
                                                   ovf_args, equ_idx, ovf_coeff));

         }

         /* ----------------------------------------------------------------
          * Add < c, z >
          * ---------------------------------------------------------------- */

         S_CHECK_EXIT(rctr_equ_addnewlin_coeff(ctr, eq, &z, c, ovf_coeff));
      }

      /* Add the quadratic part */
      if (is_quad) {
         S_CHECK_EXIT(rctr_equ_add_quadratic(ctr, eq, &J, &w_var, ovf_coeff));

         if (has_shift) {
            equ_add_cst(eq, -ovf_coeff*quad_cst);
         }
      }

      counter++;
   } while(iter);

   S_CHECK_EXIT(rctr_reserve_equs(ctr, counter - 1));

   /* ---------------------------------------------------------------------
    * 2.2 Add an equation to evaluate rho
    *
    * If we want to initialize the values of the new variables, this is the
    * start for the new equations
    * --------------------------------------------------------------------- */
   unsigned start_new_equ = cdat->total_m;


   /* TODO(xhub) reuse the code before to also store that*/
   Equ *e_rho;
   rhp_idx ei_rho = IdxNA;
   S_CHECK_EXIT(rctr_add_equ_empty(ctr, &ei_rho, &e_rho, Mapping, CONE_NONE));

   if (has_set) {

      Avar z;
      avar_setcompact(&z, n_z, z_start);

      S_CHECK_EXIT(rctr_equ_addnewlvars(ctr, e_rho, &z, c));

      if (has_shift) {

         /* ----------------------------------------------------------------
          * Add <G(F(x)), u_tilde>
          * ---------------------------------------------------------------- */

         S_CHECK_EXIT(rctr_equ_add_dot_prod_cst(ctr, e_rho, u_shift, n_u, &B_lin, b_lin, coeffs,
                                                ovf_args, equ_idx, 1.));

      }
   }

   if (is_quad) {
      S_CHECK_EXIT(rctr_equ_add_quadratic(ctr, e_rho, &J, &w_var, 1.));

      if (has_shift) {
         equ_add_cst(e_rho, -quad_cst);
      }
   }

   /* ---------------------------------------------------------------------
    * 2.2 Add an equation to evaluate rho
    *
    * If we want to initialize the values of the new variables, this is the
    * start for the new equations
    * --------------------------------------------------------------------- */
   S_CHECK_EXIT(rctr_equ_addnewvar(ctr, e_rho, vi_ovf, -1.));

   S_CHECK_EXIT(rctr_add_eval_equvar(ctr, ei_rho, vi_ovf));

   /* \TODO(xhub) this is a HACK to remove this equation from the main model.
    */
   S_CHECK_EXIT(rctr_deactivate_var(ctr, vi_ovf));
   S_CHECK_EXIT(rctr_deactivate_equ(ctr, ei_rho));


   /* ---------------------------------------------------------------------
    * 3. Add constraints G( F(x) ) - A^T z - D w - M^T u_tilde  ∈ K
    *
    * Right now, we expect K to be either R₊, R₋ or {0}.
    * K is either the polar (or dual for inf OVF) of K_u
    * --------------------------------------------------------------------- */

   /* ---------------------------------------------------------------------
    * Perform the computation of M^T u_tilde
    * --------------------------------------------------------------------- */

   if (has_shift && is_quad) {
      S_CHECK_EXIT(rhpmat_atxpy(&M, u_shift, mat_u_tilde));
   }

   Lequ *lequ;

   /* ---------------------------------------------------------------------
    * Main loop to add the constraint equations
    * --------------------------------------------------------------------- */

   char *eqn_name;
   NEWNAME(eqn_name, ovf_name, ovf_namelen, "_set");
   cdat_equname_start(cdat, eqn_name);
   /* Keep track of the   */
   size_t idx_z_bnd = z_bnd_start;

   for (unsigned i = 0; i < n_u; ++i) {
      rhp_idx new_ei = IdxNA;
      Equ *e;

      /* -------------------------------------------------------------------
       * Get the polar (or dual for inf OVF) for the equation
       * ------------------------------------------------------------------- */

      enum cone eqn_cone;
      void* eqn_cone_data;

      switch (ovf_ppty.sense) {
      case RhpMax:
         S_CHECK_EXIT(cone_polar(cones_u[i], cones_u_data[i], &eqn_cone, &eqn_cone_data));
         break;
      case RhpMin:
         S_CHECK_EXIT(cone_dual(cones_u[i], cones_u_data[i], &eqn_cone, &eqn_cone_data));
         break;
      default: status = error_runtime(); goto _exit;
      }

      /* -------------------------------------------------------------------
       * If there is an equality constraint for u, then the polar/dual is the
       * whole space and there are no constraint to add. Otherwise, add the
       * constraint.
       * ------------------------------------------------------------------- */

       /* TODO(xhub) this should not be here. This has to be detected earlier*/
      if (eqn_cone == CONE_R) {
         continue;
      }

      S_CHECK_EXIT(rctr_add_equ_empty(ctr, &new_ei, &e, ConeInclusion, eqn_cone));

      if (mp) {
         S_CHECK_EXIT(mp_addconstraint(mp, new_ei));
      }

      /* -------------------------------------------------------------------
       * Compute the contribution to the linear size from A and D.
       * ------------------------------------------------------------------- */

      size_t size_At = 0;
      size_t size_d = 0;

      if (has_set && At.ppty) {
         /*  TODO(xhub) we need the row of the transpose of A, hence the
          *  column? */
         EMPMAT_GET_CSR_SIZE(At, i, size_At);
      }
      if (is_quad) {
         EMPMAT_GET_CSR_SIZE(D, i, size_d);
      }

      unsigned *arg_idx;
      unsigned args_idx_len;
      unsigned single_idx;
      double single_val;
      double *lcoeffs = NULL;
      S_CHECK_EXIT(rhpmat_row(&B_lin, i, &single_idx, &single_val, &args_idx_len,
                         &arg_idx, &lcoeffs));

      if (args_idx_len == 0) {
         printout(PO_DEBUG, "[Warn] %s :: row %d is empty\n", __func__, i);
         continue;
      }
      size_t size_new_lequ = size_At + size_d;

      equ_add_cst(e, -mat_u_tilde[i]);

      if (isfinite(var_ubnd[i])) {
         size_new_lequ++;
      }

      /* -------------------------------------------------------------------
       * Loop over the row of B_lin to estimate the size of the linear part
       * ------------------------------------------------------------------- */

      for (unsigned j = 0; j < args_idx_len; ++j) {
         int eqidx = equ_idx[arg_idx[j]];
         if (!valid_ei(eqidx)) { size_new_lequ++; continue; }
         lequ = ctr->equs[eqidx].lequ;
         if (lequ) { size_new_lequ += lequ->len - 1; }
      }

      /* -------------------------------------------------------------------
       * Allocate the linear part and get a handle on it
       * ------------------------------------------------------------------- */

      S_CHECK_EXIT(lequ_reserve(e->lequ, size_new_lequ));
      Lequ *le = e->lequ;

      /* --------------------------------------------------------------------
       * Add - A^T z - D w  into the linear equation le
       * -------------------------------------------------------------------- */

      unsigned offset = 0;

      if (has_set && At.ppty) {
         rhpmat_copy_row_neg(&At, i, le->coeffs, le->vis, &offset, z_start);
      }

      if (is_quad) {
         rhpmat_copy_row_neg(&D, i, le->coeffs, le->vis, &offset, w_start);
      }

      le->len = offset;

      /* --------------------------------------------------------------------
       * Add the upper bound on u (if it exists) as a -I u_{ub}
       * -------------------------------------------------------------------- */

      if (isfinite(var_ubnd[i])) {
          S_CHECK_EXIT(lequ_add(le, idx_z_bnd, -1));
          idx_z_bnd++;
      }

      /* --------------------------------------------------------------------
       * Iterate over the row of B_lin to copy the elements B_i F_i(x)
       * -------------------------------------------------------------------- */

      S_CHECK_EXIT(rctr_equ_add_maps(ctr, e, coeffs, args_idx_len, (rhp_idx*)arg_idx,
                                            equ_idx, ovf_args, lcoeffs, 1.));

      /* -------------------------------------------------------------------
       * Add the constant term if it exists
       * Since we have B_lin*F(x) + b_lin on the LHS, the constant is on the LHS with a
       * minus
       * ------------------------------------------------------------------- */

      if (b_lin) {
         equ_add_cst(e, b_lin[i]);
      }

      /* -------------------------------------------------------------------
       * We have now define the new constraint. We need to keep the model
       * consistent. Only the linear part as to be taken care of, the nonlinear
       * code has already done its part
       * ------------------------------------------------------------------- */

      S_CHECK_EXIT(cmat_sync_lequ(ctr, e));

   }

   /* All the equations have been added  */
   cdat_equname_end(cdat);

   unsigned nb_equs = cdat->total_m - start_new_equ;

   if (O_Ovf_Init_New_Variables) {
      Avar ovf_vars, ovfvar;
      avar_setcompact(&ovf_vars, nb_vars, start_new_vars);
      avar_setcompact(&ovfvar, 1, vi_ovf);
      Aequ ovf_equs;
      aequ_setcompact(&ovf_equs, nb_equs, start_new_equ);

      for (unsigned i = 0; i < n_args; ++i) {
         if (!valid_ei(equ_idx[i])) {
            printout(PO_INFO, "%s :: precomputing the value of the new variables"
                             " is only implemented when the arguments of the OVF"
                             " can be substituted\n", __func__);
            goto _exit;
         }
      }

      struct filter_subset* fs;
      Avar var_c[] = { ovf_vars, ovfvar };
      Aequ eqn_c[] = { ovf_equs };
      size_t vlen = sizeof(var_c)/sizeof(Avar);
      size_t elen = sizeof(eqn_c)/sizeof(Aequ);

      /* TODO: OVF_SUP check MpMin */
      struct mp_descr descr = { .mdltype = MdlType_qcp, .sense = RhpMin,
                                .objvar = vi_ovf, .objequ = ei_rho };

      A_CHECK_EXIT(fs, filter_subset_new(vlen, var_c, elen, eqn_c, &descr));

      S_CHECK_EXIT(cdat_add_subctr(cdat, fs));
   }

   /* This has been allocated, what about OVF?*/
_exit:
   FREE(cones_u);
   FREE(cones_u_data);
   FREE(u_shift);
   FREE(c);
   FREE(b_lin);
   FREE(mat_u_tilde);
   FREE(z_bnd_revidx);

   rhpmat_free(&At);
   rhpmat_free(&D);
   rhpmat_free(&J);
   rhpmat_free(&B_lin);
   rhpmat_free(&M);

   op->trimmem(ovfd);

   simple_timing_add(&mdl->timings->reformulation.CCF.fenchel, get_thrdtime() - start);

   return status;
}

