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

static inline unsigned filter_colrow(unsigned clen, unsigned cidxs[VMT(static restrict clen)],
                                     double cvals[VMT(static restrict clen)],
                                     const bool filter[VMT(static restrict clen)])
{
   /* Iterate over the row and see if a constraint was trivial and not generated */
   unsigned skipped_cons = 0;
   for (unsigned i = 0; i < clen; ++i) {
      unsigned ridx = cidxs[i];
      if (!filter[ridx]) { /* Skip */
         skipped_cons++;
         continue;
      }

      unsigned i_new = i-skipped_cons;
      cvals[i_new] = cvals[i];
      cidxs[i_new] = ridx;
   }

   return clen - skipped_cons;
}

int ccflib_dualize_fenchel_empdag(Model *mdl, CcflibPrimalDualData *ccfprimaldualdat)
{
   int status = OK;
   double start = get_thrdtime();
   M_ArenaTempStamp atmp = ctr_memtemp_begin(&mdl->ctr);

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

   /* In this function, we finalize equations as they are generated as no mapping
    * are added */
   fdat.finalize_equ = true;

   S_CHECK(fenchel_gen_vars(&fdat, mdl));

   /* WARNING this must come after fenchel_gen_vars */
   S_CHECK(fenchel_apply_yshift(&fdat));

   Container *ctr = &mdl->ctr;

  /* ----------------------------------------------------------------------
   * Generate the objective function. If there is a y shift, we need to compute
   * 
   * ---------------------------------------------------------------------- */

   S_CHECK(fenchel_gen_objfn(&fdat, mdl));

    /* ---------------------------------------------------------------------
    * 3. Add constraints G( F(x) ) - A^T v - D s - M^T tilde_y  ∈ (K_y)°
    *
    * Right now, we expect K to be either R₊, R₋ or {0}.
    * (K_y)° is either the polar (or dual for inf OVF) of K_y
    *
    * 3.1 Using common function, create - A^T v - D s - M^T tilde_y  ∈ (K_y)°
    * 3.2 Add G( F(x) ) via the weights on the empdag arcs. The weights
    *     and index are obtained from a CSC representation of B
    *
    * --------------------------------------------------------------------- */
   S_CHECK(fenchel_gen_cons(&fdat, mdl));

   bool *equ_gen = fdat.cons_gen; assert(equ_gen);
   rhp_idx ei_cons_start = aequ_fget(&fdat.dual.cons, 0);

  /* ----------------------------------------------------------------------
   * We add the arcs per VF in F_i, as these are the children in the EMPDAG.
   * It makes it easier to store them as 
   * ---------------------------------------------------------------------- */

   RHP_INT nVF;

   if (spmat_isset(&fdat.B_lin)) {
      S_CHECK(rhpmat_ensure_cscA(&ctr->arenaL_temp, &fdat.B_lin));
      nVF = spmat_ncols(&fdat.B_lin);
   } else {
      nVF = fdat.nargs;
   }

   /* ----------------------------------------------------------------------
   * We go over each columns of B, and generate the empdag weight.
   * Except, some equation might not have been generated, hence we need
   * to take them out.
   * ---------------------------------------------------------------------- */
   mpid_t mpid_dual = ccfprimaldualdat->mpid_dual, mpid_primal = ccfprimaldualdat->mp_primal->id;
   EmpDag *empdag = &mdl->empinfo.empdag; assert(empdag->empdag_up);
   const ArcVFData *Varcs_primal = empdag->empdag_up->mps.Varcs[mpid_primal].arr;
   VarcArray *Varcs = &empdag->mps.Varcs[mpid_dual]; assert(Varcs->len == 0);
   assert(nVF == empdag->empdag_up->mps.Varcs[mpid_primal].len);
   DagUidArray *rarcs = empdag->mps.rarcs;
   daguid_t rarc_primal = rarcVFuid(mpid2uid(mpid_primal));
   empdag->mps.Varcs[mpid_primal].len = 0;

   for (unsigned j = 0; j < nVF; ++j, ++Varcs_primal) {

      unsigned *cidxs;
      unsigned clen;
      double *cvals = NULL;
      SpMatColRowWorkingMem wrkmem;
      S_CHECK_EXIT(rhpmat_col(&fdat.B_lin, j, &wrkmem, &clen, &cidxs, &cvals));

     /* ---------------------------------------------------------------------
      * If we have a shift, we might have to add in the objective the value
      * --------------------------------------------------------------------- */

      double objCoeff = 0;

      if (fdat.primal.ydat.has_shift) {
         double * restrict y_shift = fdat.primal.ydat.tilde_y;

         for (unsigned i = 0; i < clen; ++i) {
            unsigned idx = cidxs[i];
            assert(idx < fdat.primal.ydat.n_y);
            objCoeff += y_shift[idx] * cvals[idx];
         }

      }

      if (fdat.skipped_cons) {
         clen = filter_colrow(clen, cidxs, cvals, fdat.cons_gen);
      }

      /* We need the indices to be in the equation index space */
      for (unsigned i = 0; i < clen; ++i) {
         cidxs[i] += ei_cons_start;
      }

      /* --------------------------------------------------------------------
       * Add the EMPDAG arc between nodes
       * -------------------------------------------------------------------- */

      bool inObjFn = fabs(objCoeff) > DBL_EPSILON;

      unsigned narcVFequ = clen + (inObjFn ? 1 : 0);

      if (narcVFequ == 0) {
         printout(PO_DEBUG, "[Warn] %s: row %d is empty\n", __func__, j);
         continue;
      }

      /* --------------------------------------------------------------------
       * Add the EMPDAG arc between nodes
       * -------------------------------------------------------------------- */
      ArcVFData arcvf_ccf;
      if (narcVFequ == 1) { /* Simple arc */
         rhp_idx ei_arc;
         double cst_arc;
         if (clen == 1) {
            ei_arc = cidxs[0];
            cst_arc = cvals[0];
         } else {
            ei_arc = fdat.dual.ei_objfn;
            cst_arc = objCoeff;
         }

         arcVFb_init(&arcvf_ccf, ei_arc);
         arcVFb_setcst(&arcvf_ccf, cst_arc);

      } else {

         DblArrayBlock* dblarrs;
         Aequ *equs;
         if (inObjFn) {
            dblarrs = dblarrs_new(atmp.arena, clen, cvals, 1, &objCoeff);
            A_CHECK_EXIT(equs, aequ_newblockA(atmp.arena, clen, cidxs, 1, &fdat.dual.ei_objfn));
         } else {
            dblarrs = dblarrs_new(atmp.arena, clen, cvals);
            A_CHECK_EXIT(equs, aequ_newblockA(atmp.arena, clen, cidxs));
         }

         S_CHECK_EXIT(arcVFmb_init_from_aequ(&ctr->arenaL_perm, &arcvf_ccf, equs, dblarrs));
      }

      S_CHECK_EXIT(arcVF_mul_arcVF(&arcvf_ccf, Varcs_primal));

      mpid_t mpid_child = Varcs_primal->mpid_child;

      /* Remove the primal MP from the parent list */
      S_CHECK(mpidarray_rmsorted(&rarcs[mpid_child], rarc_primal));

      arcvf_ccf.mpid_child = mpid_child;
      S_CHECK(empdag_mpVFmpbyid(empdag, mpid_dual, &arcvf_ccf));
   }
 
   MathPrgm *mp_dual;
   S_CHECK_EXIT(empdag_getmpbyid(empdag, mpid_dual, &mp_dual));
   S_CHECK_EXIT(mp_setobjequ(mp_dual, fdat.dual.ei_objfn));

_exit:
   fdat_empty(&fdat);

   ops->trimmem(ovfd);

   simple_timing_add(&mdl->timings->reformulation.CCF.fenchel, get_thrdtime() - start);

   ctr_memtemp_end(atmp);

   return status;
}
