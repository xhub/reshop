#include <assert.h>
#include <float.h>
#include <math.h>
#include <stdbool.h>
#include <string.h>

#include "cmat.h"
#include "compat.h"
#include "container.h"
#include "ctr_rhp.h"
#include "equ_modif.h"
#include "fenchel_common.h"
#include "filter_ops.h"
#include "macros.h"
#include "mdl.h"
#include "ctrdat_rhp.h"
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

int ovf_fenchel(Model *mdl, enum OVF_TYPE type, OvfOpsData ovfd)
{
   double start = get_thrdtime();
   int status = OK;
   const OvfOps *ops;

   Container *ctr = &mdl->ctr;

   RhpContainerData *cdat = (RhpContainerData *)ctr->data;

   switch (type) {
   case OvfType_Ovf:
      ops = &ovfdef_ops;
      break;
   case OvfType_Ccflib:
   case OvfType_Ccflib_Dual:
      ops = &ccflib_ops;
      break;
   default:
      TO_IMPLEMENT("user-defined OVF is not implemented");
   }

   /* ---------------------------------------------------------------------
    * 1. Analyze the QP structure
    * 2. Inject new variables
    * 3. Replace the rho variable
    * 4. Add constraints 
    * --------------------------------------------------------------------- */

   /* ----------------------------------------------------------------------
    * Get the indices of F(x):
    *   - indices
    *   - types
    *   - n_args   number of arguments
    * ---------------------------------------------------------------------- */

   Avar *ovf_args;
   S_CHECK(ops->get_args(ovfd, &ovf_args));

   /* ----------------------------------------------------------------------
    * Analyze the conic QP structure:
    * - Find the convex cone K to which y (or y - \tilde{y}) belongs
    * - Complete the constraints with  XXX
    *
    * In step 1.1, we look for y in R₊. If this is not the case, we look for a
    * finite lower bound. If there is none, we look for a finite upper-bound.
    *
    * \TODO(xhub) support My - \tilde{y} ∈ K (cf Ben-Tal and Den ...)?
    *
    * OUTPUT:
    *   - u_shift    the value of \tilde{u}
    *   - var_ubnd   the bound of the variable
    *   - cones_u    the cone K
    * ---------------------------------------------------------------------- */

   CcfFenchelData fdat;
   S_CHECK(fdat_init(&fdat, type, ops, ovfd, mdl));

   S_CHECK_EXIT(fenchel_gen_vars(&fdat, mdl));

   /* WARNING this must come after fenchel_gen_vars */
   S_CHECK_EXIT(fenchel_apply_yshift(&fdat));

   rhp_idx *equ_idx = NULL;
   double *coeffs = NULL;
   S_CHECK_EXIT(ops->get_mappings(ovfd, &equ_idx));
   S_CHECK_EXIT(ovf_process_indices(mdl, ovf_args, equ_idx));
   S_CHECK_EXIT(ops->get_coeffs(ovfd, &coeffs));

   // XXX START ARG_IS_EXPR ONLY only

   /* ---------------------------------------------------------------------
    * 2. Add given location (OVF: where rho appears, ccflib: MP obj func) the terms
    *   o   <c,v> + 0.5 <s, Js> in the simplest case
    *   o   <G(F(x)), tilde_y> - .5 <tilde_y, M tilde_y> + <c - A tilde_y, v> +
    *       0.5 <s, Js> if y has to be shifted by - tilde_y.
    * --------------------------------------------------------------------- */

   unsigned n_y = fdat.primal.ydat.n_y;

   void *iter = NULL;
   unsigned counter = 0;
   rhp_idx vi_ovf = fdat.vi_ovf;
   unsigned n_dualvars_mult = avar_size(&fdat.dual.vars.v) +  avar_size(&fdat.dual.vars.w);

   do {
      rhp_idx ei_new;
      double ovf_coeff;

      S_CHECK_EXIT(ops->get_equ(ovfd, mdl, &iter, vi_ovf, &ovf_coeff, &ei_new, n_dualvars_mult));

      assert(ei_new < mdl_nequs_total(mdl));
      Equ *eq = &ctr->equs[ei_new];

      /* Replace var by lin */
      if (fdat.primal.has_set) {

         if (fdat.primal.ydat.has_shift) {

            /* ----------------------------------------------------------------
             * Add < G(F(x)), tilde_y >
             * ---------------------------------------------------------------- */

            S_CHECK_EXIT(rctr_equ_add_dot_prod_cst(ctr, eq, fdat.primal.ydat.tilde_y, n_y,
                                                   &fdat.B_lin, fdat.b_lin,
                                                   coeffs, ovf_args, equ_idx, ovf_coeff));

         }

         /* ----------------------------------------------------------------
          * Add < a, v > + < ub, w >
          * ---------------------------------------------------------------- */

         S_CHECK_EXIT(rctr_equ_addnewlin_coeff(ctr, eq, &fdat.dual.vars.v, fdat.dual.a, ovf_coeff));
         S_CHECK_EXIT(rctr_equ_addnewlin_coeff(ctr, eq, &fdat.dual.vars.w, fdat.dual.ub, ovf_coeff));
      }

      /* Add the quadratic part */
      if (fdat.primal.is_quad) {
         S_CHECK_EXIT(rctr_equ_add_quadratic(ctr, eq, &fdat.J, &fdat.dual.vars.s, ovf_coeff));

         if (fdat.primal.ydat.has_shift) {
            equ_add_cst(eq, -ovf_coeff*fdat.primal.ydat.shift_quad_cst);
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
   rhp_idx ei_rho = IdxNA;

   if (valid_vi(vi_ovf)) {
      /* TODO(xhub) reuse the code before to also store that*/
      Equ *e_rho;
      S_CHECK_EXIT(rctr_add_equ_empty(ctr, &ei_rho, &e_rho, Mapping, CONE_NONE));

      if (fdat.primal.has_set) {

         /* Add < a, v > + < ub, w> */
         S_CHECK_EXIT(rctr_equ_addnewlvars(ctr, e_rho, &fdat.dual.vars.v, fdat.dual.a));
         S_CHECK_EXIT(rctr_equ_addnewlvars(ctr, e_rho, &fdat.dual.vars.w, fdat.dual.ub));

         if (fdat.primal.ydat.has_shift) {

            /* ----------------------------------------------------------------
          * Add <G(F(x)), tilde_y>
          * ---------------------------------------------------------------- */

            S_CHECK_EXIT(rctr_equ_add_dot_prod_cst(ctr, e_rho, fdat.primal.ydat.tilde_y,
                                                   n_y, &fdat.B_lin, fdat.b_lin, coeffs,
                                                   ovf_args, equ_idx, 1.));

         }
      }

      if (fdat.primal.is_quad) {
         S_CHECK_EXIT(rctr_equ_add_quadratic(ctr, e_rho, &fdat.J, &fdat.dual.vars.s, 1.));

         if (fdat.primal.ydat.has_shift) {
            equ_add_cst(e_rho, -fdat.primal.ydat.shift_quad_cst);
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
   }


   /* ---------------------------------------------------------------------
    * 3. Add constraints G( F(x) ) - A^T v - D s - M^T tilde_y  ∈ (K_y)°
    *
    * Right now, we expect K to be either R₊, R₋ or {0}.
    * (K_y)° is either the polar (or dual for inf OVF) of K_y
    *
    * 3.1 Using common function, create - A^T v - D s - M^T tilde_y  ∈ (K_y)°
    * 3.2 Add G( F(x) ) to them
    * --------------------------------------------------------------------- */
   S_CHECK(fenchel_gen_equs(&fdat, mdl));

   bool *equ_gen = fdat.equ_gen; assert(equ_gen);
   rhp_idx ei = aequ_fget(&fdat.dual.cons, 0);

   for (unsigned i = 0; i < n_y; ++i) {
      if (!equ_gen[i]) { continue; } /* No generated equation */

      /* --------------------------------------------------------------------
       * Iterate over the row of B_lin to copy the elements B_i F_i(x)
       * -------------------------------------------------------------------- */
      assert(ei < mdl_nequs_total(mdl));
      Equ *e = &ctr->equs[ei];
      ei++;

      unsigned *arg_idx;
      unsigned args_idx_len;
      unsigned single_idx;
      double single_val;
      double *lcoeffs = NULL;
      S_CHECK_EXIT(rhpmat_row(&fdat.B_lin, i, &single_idx, &single_val, &args_idx_len,
                         &arg_idx, &lcoeffs));

      if (args_idx_len == 0) {
         printout(PO_DEBUG, "[Warn] %s :: row %d is empty\n", __func__, i);
         continue;
      }
      S_CHECK(rctr_equ_add_maps(ctr, e, coeffs, args_idx_len, (rhp_idx*)arg_idx,
                                equ_idx, ovf_args, lcoeffs, 1.));

      /* -------------------------------------------------------------------
       * Add the constant term if it exists
       * Since we have B_lin*F(x) + b_lin on the LHS, the constant is on the LHS with a
       * minus
       * ------------------------------------------------------------------- */

      if (fdat.b_lin) {
         equ_add_cst(e, fdat.b_lin[i]);
      }

      /* -------------------------------------------------------------------
       * We have now define the new constraint. We need to keep the model
       * consistent. Only the linear part as to be taken care of, the nonlinear
       * code has already done its part
       *
       * TODO: move this call
       * ------------------------------------------------------------------- */
      S_CHECK(cmat_sync_lequ(ctr, e));

   }

   if (O_Ovf_Init_New_Variables) {
      assert(valid_vi(vi_ovf));
      Avar ovfvar;
      avar_setcompact(&ovfvar, 1, vi_ovf);
      Aequ ovf_objequ;
      aequ_setcompact(&ovf_objequ, 1, ei_rho);

      for (unsigned i = 0; i < fdat.nargs; ++i) {
         if (!valid_ei(equ_idx[i])) {
            printout(PO_INFO, "%s :: precomputing the value of the new variables"
                             " is only implemented when the arguments of the OVF"
                             " can be substituted\n", __func__);
            goto _exit;
         }
      }

      struct filter_subset* fs;
      Avar var_c[] = { fdat.dual.vars.v, fdat.dual.vars.w, fdat.dual.vars.s, ovfvar };
      Aequ eqn_c[] = { fdat.dual.cons, ovf_objequ };
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
   fdat_empty(&fdat);

   ops->trimmem(ovfd);

   simple_timing_add(&mdl->timings->reformulation.CCF.fenchel, get_thrdtime() - start);

   return status;
}

