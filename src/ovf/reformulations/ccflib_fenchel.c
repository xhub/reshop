#include "cmat.h"
#include "ccflib_reformulations.h"
#include "equ_modif.h"
#include "fenchel_common.h"
#include "macros.h"
#include "mathprgm.h"
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

#endif

int ccflib_dualize_fenchel_empdag(Model *mdl, CcflibPrimalDualData *ccfprimaldualdat)
{
   int status = OK;
   double start = get_thrdtime();

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
   OvfOpsData ovfd = { .ccfdat = ccfprimaldualdat };
   const OvfOps *ops = &ccflib_ops;

   S_CHECK(fdat_init(&fdat, OvfType_Ccflib_Dual, ops, ovfd, mdl));
   fdat.mpid_dual = ccfprimaldualdat->mpid_dual;
   fdat.mpid_primal = ccfprimaldualdat->mp_primal->id;

   /* In this function, we finalize equations as they are generated as no mapping
    * are added */
   fdat.finalize_equ = true;

   S_CHECK(fenchel_gen_vars(&fdat, mdl));

   /* WARNING this must come after fenchel_gen_vars */
   S_CHECK(fenchel_apply_yshift(&fdat));

  /* ----------------------------------------------------------------------
   * Generate the objective function. If there is a y shift, we need to compute
   * 
   * ---------------------------------------------------------------------- */

   S_CHECK(fenchel_gen_objfn(&fdat, mdl));

    /* ---------------------------------------------------------------------
    * 3. Add constraints G( F(x) ) - A^T v - D s - M^T ỹ  ∈ (K_y)°
    *
    * Right now, we expect K to be either R₊, R₋ or {0}.
    * (K_y)° is either the polar (or dual for inf OVF) of K_y
    *
    * 3.1 Using common function, create - A^T v - D s - M^T ỹ  ∈ (K_y)°
    * 3.2 Add G( F(x) ) via the weights on the empdag arcs. The weights
    *     and index are obtained from a CSC representation of B
    *
    * --------------------------------------------------------------------- */
   S_CHECK_EXIT(fenchel_gen_cons(&fdat, mdl));


   S_CHECK_EXIT(fenchel_edit_empdag(&fdat, mdl));

   MathPrgm *mp_dual;
   S_CHECK_EXIT(empdag_getmpbyid(&mdl->empinfo.empdag, ccfprimaldualdat->mpid_dual, &mp_dual));
   S_CHECK_EXIT(mp_setobjequ(mp_dual, fdat.dual.ei_objfn));

   mp_setprobtype(mp_dual, fdat.primal.is_quad ? MdlType_qcp : MdlType_lp);

_exit:
   fdat_empty(&fdat);

   ops->trimmem(ovfd);

   simple_timing_add(&mdl->timings->reformulation.CCF.fenchel, get_thrdtime() - start);

   return status;
}
