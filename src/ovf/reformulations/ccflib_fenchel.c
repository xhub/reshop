#include "cmat.h"
#include "ccflib_reformulations.h"
#include "equ_modif.h"
#include "fenchel_common.h"
#include "macros.h"
#include "mdl.h"
#include "ctrdat_rhp.h"
#include "ovf_common.h"
#include "rhp_fwd.h"
#include "timings.h"


typedef struct {
   unsigned z_start;
   unsigned z_bnd_start;
   unsigned c_bnd_start;
   unsigned c_end;
   const char *ccfname;
   unsigned ccfnamelen;
} CcfLibFenchelDat;

#if 0

int ccflib_primal_addmult_nonbox_cons(Model *mdl, CcfLibFenchelDat *dat)
{
   struct container *ctr = mdl->ctr;
   struct model_repr *model = (struct model_repr *)mdl->ctr.data;
   dat->z_start = model->total_n;
   unsigned idx_z = 0;
   char *z_name;

      /* -------------------------------------------------------------------
       * Add the multipliers for the nonbox constraints. This is always well
       * defined
       *
       * If the OVF is a sup, the multipliers belong to the polar cone
       * Otherwise, we use a trick and the multiplier belongs to the dual cone
       * ------------------------------------------------------------------- */

      NEWNAME(z_name, dat->ccfname, dat->ccfnamelen, "_z");
      model_varname_start(model, z_name);
      for (unsigned j = 0; j < n_z; ++j) {
         enum CONES cone;
         void *cone_data;
         S_CHECK_EXIT(op->get_cone_nonbox(ovfd, j, &cone, &cone_data));
         rhp_idx vidx = IdxNA;

         if (ovf_ppty.sup) {
            S_CHECK_EXIT(model_add_multiplier_polar(ctr, cone, cone_data, &vidx));
         } else {
            S_CHECK_EXIT(model_add_multiplier_dual(ctr, cone, cone_data, &vidx));
         }

         if (valid_vi(vidx)) {
            c[idx_z] = c[j];
            idx_z++;
            if (mp) {
               S_CHECK_EXIT(mp_addvar(mp, vidx));
            }
         }
      }

      model_varname_end(model);

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

      NEWNAME(z_name, ccfname, ccfnamelen, "_zbnd");
      model_varname_start(model, z_name);
      z_bnd_start = model->total_n;
      c_bnd_start = idx_z;

      for (unsigned i = 0, j = 0; i < n_u; ++i) {
         if (isfinite(var_ubnd[i])) {
            rhp_idx vidx = IdxNA;

            if (ovf_ppty.sup) {
               S_CHECK_EXIT(model_add_multiplier_polar(ctr, CONE_R_MINUS, NULL, &vidx));
            } else {
               S_CHECK_EXIT(model_add_multiplier_dual(ctr, CONE_R_MINUS, NULL, &vidx));
            }
            assert(valid_vi(vidx));
            if (mp) {
               S_CHECK_EXIT(mp_addvar(mp, vidx));
            }
            c[idx_z] = var_ubnd[i];
            z_bnd_revidx[j] = i;
            j++;
            idx_z++;
         }
      }

      model_varname_end(model);

      c_end = idx_z;
      n_z = model->total_n - z_start;

}

//#endif

int ccflib_dualize_fenchel_empdag(Model *mdl, CcflibPrimalDualData *ccfprimaldualdat)
{
   int status = OK;
   double start = get_thrdtime();

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
   OvfOpsData ovfd = { .ccfdat = ccfprimaldualdat };
   const OvfOps *ops = &ccflib_ops;

   S_CHECK(fdat_init(&fdat, OvfType_Ccflib_Dual, ops, ovfd, mdl));

   S_CHECK(fenchel_gen_vars(&fdat, mdl));

   /* WARNING this must come after fenchel_gen_vars */
   S_CHECK(fenchel_apply_yshift(&fdat));

   unsigned n_y = fdat.primal.ydat.n_y;

   Container *ctr = &mdl->ctr;
   RhpContainerData *cdat = (RhpContainerData *)ctr->data;

   /* ---------------------------------------------------------------------
    * 3. Add constraints G( F(x) ) - A^T v - D s - M^T tilde_y  ∈ (K_y)°
    *
    * Right now, we expect K to be either R₊, R₋ or {0}.
    * (K_y)° is either the polar (or dual for inf OVF) of K_y
    *
    * 3.1 Using common function, create - A^T v - D s - M^T tilde_y  ∈ (K_y)°
    * 3.2 Add G( F(x) ) via the weights on the empdag arcs. The weights
    *     and index are obtained from a CSC representation of B
    * --------------------------------------------------------------------- */
   S_CHECK(fenchel_gen_equs(&fdat, mdl));

   bool *equ_gen = fdat.equ_gen; assert(equ_gen);
   rhp_idx ei = aequ_fget(&fdat.dual.cons, 0);

   unsigned ncols = fdat.B_lin
   unsigned *csc_col_ptr = ctr_getmem(ctr, sizeof(unsigned)*ncols);
//    def csr_to_csc(M, N, csr_row_ptr, csr_col_idx, csr_data):
//    # Step 1: Initialize the output arrays
//    NNZ = len(csr_data)
//    csc_data = [0] * NNZ
//    csc_row_idx = [0] * NNZ
//    csc_col_ptr = [0] * (N + 1)
//
//    # Step 2: Count the number of entries per column
//    for col in csr_col_idx:
//        csc_col_ptr[col + 1] += 1
//
//    # Step 3: Compute the cumulative sum to get column pointers
//    for i in range(1, N + 1):
//        csc_col_ptr[i] += csc_col_ptr[i - 1]
//
//    # Step 4: Fill csc_data and csc_row_idx using the CSR data
//    for row in range(M):
//        row_start = csr_row_ptr[row]
//        row_end = csr_row_ptr[row + 1]
//        for idx in range(row_start, row_end):
//            col = csr_col_idx[idx]
//            dest = csc_col_ptr[col]
//
//            # Fill the CSC arrays
//            csc_row_idx[dest] = row
//            csc_data[dest] = csr_data[idx]
//
//            # Increment the csc_col_ptr to the next position for this column
//            csc_col_ptr[col] += 1
//
//    # Step 5: Restore csc_col_ptr to original values (shift back by one position)
//    for j in range(N, 0, -1):
//        csc_col_ptr[j] = csc_col_ptr[j - 1]
//    csc_col_ptr[0] = 0
//
//    return csc_data, csc_row_idx, csc_col_ptr


   for (unsigned i = 0; i < n_y; ++i) {
      if (!equ_gen[i]) { continue; } /* No generated equation */

      /* --------------------------------------------------------------------
       * Iterate over the row of B_lin to copy the elements B_i F_i(x)
       * -------------------------------------------------------------------- */
      assert(ei < mdl_nequs_total(mdl));
      Equ *e = &ctr->equs[ei];
      ei++;

      unsigned *row_idxs;
      unsigned row_len;
      double *row_vals = NULL;
      SpMatRowWorkingMem wrkmem;
      S_CHECK_EXIT(rhpmat_row(&fdat.B_lin, i, &wrkmem, &row_len, &row_idxs, &row_vals));

      if (row_len == 0) {
         printout(PO_DEBUG, "[Warn] %s: row %d is empty\n", __func__, i);
         continue;
      }


      S_CHECK(rctr_equ_add_maps(ctr, e, coeffs, row_len, (rhp_idx*)row_idxs,
                                equ_idx, ovf_args, row_vals, 1.));

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
 

_exit:
   fdat_empty(&fdat);

   ops->trimmem(ovfd);

   simple_timing_add(&mdl->timings->reformulation.CCF.fenchel, get_thrdtime() - start);

   return status;
}
#endif
