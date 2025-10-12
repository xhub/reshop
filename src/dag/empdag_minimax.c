

#include "ccflib_reformulations.h"
#include "empdag.h"
#include "empinfo.h"
#include "mathprgm.h"
#include "mdl.h"
#include "mdl_rhp.h"
#include "mdl_transform.h"
#include "ovf_conjugate.h"
#include "ovf_fenchel.h"
#include "ovf_options.h"
#include "printout.h"
#include "rhp_fwd.h"
#include "timings.h"

static int rmdl_empdag_minimax_reformulate(Model *mdl)
{
   EmpDag * empdag = &mdl->empinfo.empdag;

   if (!empdag->empdag_up) {
      errormsg("[process] EMPDAG: missing upstream EMPDAG\n");
      return Error_NullPointer;
   }

   const MpIdArray * restrict mps2reform = &empdag->empdag_up->minimaxi.mps2reformulate;

   S_CHECK(rmdl_incstage(mdl));

   empdag->finalized = false;

   pr_info("EMPDAG: %u MP%s will be reformulated.\n", mps2reform->len,
           mps2reform->len > 1 ? "s" : "");

   if (O_Ovf_Reformulation == OVF_Equilibrium) {
      trace_processmsg("[process] EMPDAG: using Equilibrium reformulation\n");
      S_CHECK(ccflib_equil(mdl));
      goto _exit;
   }

   for (unsigned i = 0, len = mps2reform->len; i < len; ++i) {
      unsigned mp_id = mps2reform->arr[i];

      MathPrgm *mp = empdag->mps.arr[mp_id];

      MpType mptype = mp_gettype(mp);
      if (mptype != MpTypeCcflib) {
         TO_IMPLEMENT("EMPDAG reformulation for non-CCFLIB MP");
      }

      unsigned reformulation = O_Ovf_Reformulation;

      CcflibPrimalDualData ccfdat = {.mp_primal = mp, .mpid_dual = MpId_NA};
      OvfOpsData ovfd = {.ccfdat = &ccfdat };

      switch (reformulation) {
         case OVF_Fenchel:
            trace_processmsg("dual optimization\n");
            S_CHECK(ovf_fenchel(mdl, OvfType_Ccflib, ovfd));
            break;
         case OVF_Conjugate:
            trace_processmsg("conjugate reformulation\n");
            S_CHECK(ovf_conjugate(mdl, OvfType_Ccflib, ovfd));
            break;
         default:
            error("\r[process] ERROR: unsupported case %s for minimax reformulation\n",
                     ovf_getreformulationstr(reformulation));
            return Error_NotImplemented;
      }
   }

_exit:
   return OK;

}

int rmdl_empdag_transform(Model *mdl_reform)
{
   EmpDag *empdag = &mdl_reform->empinfo.empdag;
   const EmpDag *empdag_up = empdag->empdag_up;

   assert(empdag_up);

   trace_process("[model] %s model '%.*s' #%u: transforming EMPDAG\n",
                 mdl_fmtargs(mdl_reform->mdl_up));

   empdag->finalized = false;

  /* ----------------------------------------------------------------------
   * Instantiate MP created via the dual() operator
   * ---------------------------------------------------------------------- */

   unsigned len = empdag_up->transformations.fenchel_dual_nodal.len;
   const mpid_t * restrict mpid_arr = empdag_up->transformations.fenchel_dual_nodal.arr;

   for (unsigned i = 0; i < len; ++i) {

      mpid_t mpid = mpid_arr[i];
      MathPrgm *mp;

      S_CHECK(empdag_getmpbyid(empdag, mpid, &mp));

      S_CHECK(mp_instantiate_fenchel_dual(mp));
   }

  /* ----------------------------------------------------------------------
   * Instantiate MP that are required to be so
   * ---------------------------------------------------------------------- */

   len = empdag_up->mps2instantiate.len;
   mpid_arr = empdag_up->mps2instantiate.arr;

   for (unsigned i = 0; i < len; ++i) {
      mpid_t mpid = mpid_arr[i];
      MathPrgm *mp;

      S_CHECK(empdag_getmpbyid(empdag, mpid, &mp));

      S_CHECK(mp_instantiate(mp));
   }

  /* ----------------------------------------------------------------------
   * Add the "objfn" terms. This does not change the EMPDAG.
   * ---------------------------------------------------------------------- */
   len = empdag_up->objfn.dst.len;
   assert(len == empdag_up->objfn.src.len);
   const mpid_t * restrict mpid_src_arr = empdag_up->objfn.src.arr;
   const mpid_t * restrict mpid_dst_arr = empdag_up->objfn.dst.arr;

   for (unsigned i = 0; i < len; ++i) {
      mpid_t mpid_src = mpid_src_arr[i], mpid_dst = mpid_dst_arr[i];

      MathPrgm *mp_src, *mp_dst;
      S_CHECK(empdag_getmpbyid(empdag, mpid_src, &mp_src));
      S_CHECK(empdag_getmpbyid(empdag, mpid_dst, &mp_dst));

      S_CHECK(mp_add_objfn_mp(mp_dst, mp_src));
   }

   len = empdag_up->objfn_maps.mps.len;
   mpid_arr = empdag_up->objfn_maps.mps.arr;
   Lequ **lequs = empdag_up->objfn_maps.lequs;

   for (unsigned i = 0; i < len; ++i) {
      mpid_t mpid = mpid_arr[i];
      Lequ *le = lequs[i]; assert(le);
      MathPrgm *mp;

      S_CHECK(empdag_getmpbyid(empdag, mpid, &mp));
      S_CHECK(mp_add_objfn_map(mp, le));
   }

   if (empdag_has_adversarial_mps(empdag)) {
      double start = get_thrdtime();

      S_CHECK(rmdl_empdag_minimax_reformulate(mdl_reform));

      mdl_reform->timings->reformulation.CCF.total += get_thrdtime() - start;
   }

   return empdag_fini(empdag);
}

