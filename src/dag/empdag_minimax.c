

#include "ccflib_reformulations.h"
#include "empdag.h"
#include "empinfo.h"
#include "mathprgm.h"
#include "mdl.h"
#include "mdl_rhp.h"
#include "ovf_conjugate.h"
#include "ovf_fenchel.h"
#include "ovf_options.h"
#include "printout.h"
#include "rhp_fwd.h"

int empdag_minimax_reformulate(Model *mdl)
{
   EmpDag * empdag = &mdl->empinfo.empdag;

   if (!empdag->empdag_up) {
      errormsg("[process] EMPDAG: missing upstream EMPDAG\n");
      return Error_NullPointer;
   }

   const UIntArray * restrict mps2reform = &empdag->empdag_up->mps2reformulate;

   S_CHECK(rmdl_incstage(mdl));

   empdag->finalized = false;

   trace_process("[process] EMPDAG: there are %u MP to reformulate\n", mps2reform->len);

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

      switch (reformulation) {
         case OVF_Fenchel:
            trace_processmsg("dual optimization\n");
            S_CHECK(ovf_fenchel(mdl, OVFTYPE_CCFLIB,
                                    (union ovf_ops_data){.mp = mp}));
            break;
         case OVF_Conjugate:
            trace_processmsg("conjugate reformulation\n");
            S_CHECK(ovf_conjugate(mdl, OVFTYPE_CCFLIB,
                                       (union ovf_ops_data){.mp = mp}));
            break;
         default:
            error("\r%s :: unsupported case %s\n", __func__,
                     ovf_getreformulationstr(reformulation));
            return Error_NotImplemented;
      }
   }

_exit:
   S_CHECK(empdag_fini(empdag));

   return empdag_exportasdot(mdl);
}
