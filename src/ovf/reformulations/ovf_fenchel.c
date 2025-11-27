#include <assert.h>
#include <float.h>
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

int ovf_fenchel(Model *mdl, OvfType type, OvfOpsData ovfd)
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

   Avar *ovf_args_vars;
   unsigned num_empdag_children;
   S_CHECK(ops->get_args(ovfd, &ovf_args_vars, &num_empdag_children));

   /* ----------------------------------------------------------------------
    * Analyze the conic QP structure:
    * - Find the convex cone K to which y (or y - ỹ) belongs
    * - Complete the constraints with  XXX
    *
    * In step 1.1, we look for y in R₊. If this is not the case, we look for a
    * finite lower bound. If there is none, we look for a finite upper-bound.
    *
    * \TODO(xhub) support My - ỹ ∈ K (cf Ben-Tal and Den ...)?
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
   S_CHECK_EXIT(ovf_process_indices(mdl, ovf_args_vars, equ_idx));
   S_CHECK_EXIT(ops->get_coeffs(ovfd, &coeffs));

   // XXX START ARG_IS_EXPR ONLY only

   /* ---------------------------------------------------------------------
    * 2. Add given location (OVF: where rho appears, ccflib: MP obj func) the terms
    *  x  <c,v> + 0.5 <s, Js> in the simple case
    *  x  <G(F(x)), ỹ> - .5 <ỹ, M ỹ> + <c - A ỹ, v> + 0.5 <s, Js>   if y is shifted by -ỹ.
    * --------------------------------------------------------------------- */

   unsigned n_y = fdat.primal.ydat.n_y;

   void *iter = NULL;
   unsigned iter_cnt = 0;
   rhp_idx vi_ovf = fdat.vi_ovf;
   unsigned n_dualvars_mult = avar_size(&fdat.dual.vars.v) +  avar_size(&fdat.dual.vars.w);

   do {

      if (type == OvfType_Ccflib || type == OvfType_Ccflib_Dual) { break; }
      rhp_idx ei_new;
      double ovf_coeff;

      /* For a CCFLIB MP, this instanciate the objequ and update the EMPDAG
       * For a regular OVF/CCF, this gets all the equation where the OVF var appears */
      S_CHECK_EXIT(ops->get_equ(ovfd, mdl, &iter, vi_ovf, &ovf_coeff, &ei_new, n_dualvars_mult));

      assert(ei_new < mdl_nequs_total(mdl));
      Equ *eq = &ctr->equs[ei_new];

      /* Replace var by lin */
      if (fdat.primal.has_set) {

         if (fdat.primal.ydat.has_yshift && avar_size(ovf_args_vars) > 0) {

            /* ----------------------------------------------------------------
             * Add < G(F(x)), ỹ >
             * ---------------------------------------------------------------- */

            S_CHECK_EXIT(rctr_equ_add_dot_prod_cst(ctr, eq, fdat.primal.ydat.tilde_y, n_y,
                                                   &fdat.B_lin, fdat.b_lin,
                                                   coeffs, ovf_args_vars, equ_idx, ovf_coeff));

         }

         /* ----------------------------------------------------------------
          * Add < a, v > + < ub, w >
          * ---------------------------------------------------------------- */

         S_CHECK_EXIT(rctr_equ_addnewlin_coeff(ctr, eq, &fdat.dual.vars.v, fdat.dual.a, ovf_coeff));
         S_CHECK_EXIT(rctr_equ_addnewlin_coeff(ctr, eq, &fdat.dual.vars.w, fdat.dual.ub, ovf_coeff));
      }

      /* Add the quadratic terms */
      if (fdat.primal.is_quad) {

         /* 0.5 <s, Js> */
         S_CHECK_EXIT(rctr_equ_add_quadratic(ctr, eq, &fdat.J, &fdat.dual.vars.s, ovf_coeff));

         /* with shift: Add .5 <ỹ, M ỹ> */
         if (fdat.primal.ydat.has_yshift) {
            equ_add_cst(eq, -ovf_coeff*fdat.primal.ydat.shift_quad_cst);
         }
      }

      iter_cnt++;
   } while(iter);

   if (iter_cnt > 2) {
      S_CHECK_EXIT(rctr_reserve_equs(ctr, iter_cnt - 1));
   }

   /* ---------------------------------------------------------------------
    * 2.2 Add an equation to evaluate rho
    *
    * If we want to initialize the values of the new variables, this is the
    * start for the new equations
    * --------------------------------------------------------------------- */
   S_CHECK(fenchel_gen_objfn(&fdat, mdl));
   rhp_idx ei_objfn = fdat.dual.ei_objfn;


   if (valid_vi(vi_ovf)) {

      Equ *e_objfn = &ctr->equs[ei_objfn];

      if (fdat.primal.ydat.has_yshift) {

         /* ----------------------------------------------------------------
          * Add <B(F(x)), ỹ>  as < b, ỹ> has already been added
          * ---------------------------------------------------------------- */

         S_CHECK_EXIT(rctr_equ_add_dot_prod_cst(ctr, e_objfn, fdat.primal.ydat.tilde_y,
                                                n_y, &fdat.B_lin, NULL, coeffs,
                                                ovf_args_vars, equ_idx, 1.));

      }

   /* ---------------------------------------------------------------------
    * 2.2 Add an equation to evaluate rho
    *
    * If we want to initialize the values of the new variables, this is the
    * start for the new equations
    * --------------------------------------------------------------------- */

      S_CHECK_EXIT(rctr_equ_addnewvar(ctr, e_objfn, vi_ovf, -1.));

      S_CHECK_EXIT(rctr_add_eval_equvar(ctr, ei_objfn, vi_ovf));

      /* \TODO(xhub) this is a HACK to remove this equation from the main model.
    */
      S_CHECK_EXIT(rctr_deactivate_var(ctr, vi_ovf));
      S_CHECK_EXIT(rctr_deactivate_equ(ctr, ei_objfn));
   }


   /* ---------------------------------------------------------------------
    * 3. Add constraints G( F(x) ) - A^T v - D s - M^T ỹ  ∈ (K_y)°
    *
    * Right now, we expect K to be either R₊, R₋ or {0}.
    * (K_y)° is either the polar (or dual for inf OVF) of K_y
    *
    * 3.1 Using common function, create - A^T v - D s - M^T ỹ  ∈ (K_y)°
    * 3.2 Add G( F(x) ) to them
    * --------------------------------------------------------------------- */
   S_CHECK(fenchel_gen_cons(&fdat, mdl));

   bool *equ_gen = fdat.cons_gen; assert(equ_gen);
   rhp_idx ei = aequ_fget(&fdat.dual.cons, 0);

   for (unsigned i = 0; i < n_y; ++i) {
      if (!equ_gen[i]) { continue; } /* No generated equation */

      /* --------------------------------------------------------------------
       * Iterate over the row of B_lin to copy the elements B_i F_i(x)
       * -------------------------------------------------------------------- */
      assert(ei < mdl_nequs_total(mdl));
      Equ *e = &ctr->equs[ei];
      ei++;

      unsigned *arg_idx, nargs, single_idx;
      double single_val;
      double *lcoeffs = NULL;
      S_CHECK_EXIT(rhpmat_row_needs_update(&fdat.B_lin, i, &single_idx, &single_val, &nargs,
                                           &arg_idx, &lcoeffs));

      if (nargs == 0) {
         printout(PO_DEBUG, "[Warn] %s :: row %d is empty\n", __func__, i);
         continue;
      }

      if (avar_size(ovf_args_vars) > 0) {
         S_CHECK(rctr_equ_add_maps(ctr, e, nargs, coeffs, (rhp_idx*)arg_idx, equ_idx,
                                   ovf_args_vars, lcoeffs, 1.));
      }

      /* -------------------------------------------------------------------
       * Add the constant term if it exists
       * Since we have B_lin*F(x) + b_lin on the LHS, the constant is on the LHS with a
       * minus
       * ------------------------------------------------------------------- */

      if (fdat.b_lin) {
         equ_add_cst(e, fdat.b_lin[i]);
      }

      /* -------------------------------------------------------------------
       * Keep the container consistent after adding a new constraint.
       * Only the linear part as to be taken care of, the nonlinear part has been done
       *
       * TODO: move this call
       * ------------------------------------------------------------------- */
      S_CHECK(cmat_sync_lequ(ctr, e));
   }


   /* Add the EMPDAG contributions */
   if (num_empdag_children > 0) {
      S_CHECK_EXIT(fenchel_edit_empdag(&fdat, mdl));
   }

   if (O_Ovf_Init_New_Variables) {
      assert(valid_vi(vi_ovf));
      Avar ovfvar;
      avar_setcompact(&ovfvar, 1, vi_ovf);
      Aequ ovf_objequ;
      aequ_ascompact(&ovf_objequ, 1, ei_objfn);

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
                                .objvar = vi_ovf, .objequ = ei_objfn };

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
