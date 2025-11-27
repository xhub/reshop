
#include "ctr_rhp.h"
#include "empdag.h"
#include "rmdl_transform.h"
#include "mathprgm.h"
#include "mdl.h"
#include "mdl_rhp.h"
#include "status.h"
#include "toplayer_utils.h"


int rmdl_marginalVars(Model * restrict mdl)
{
   const IdxArray *marginalVars = &mdl->empinfo.equvar.marginalVars;
   EmpDag *empdag = &mdl->empinfo.empdag;
   MathPrgm *mp_marginals, *mp = NULL;
   Nash *nash;

   if (empdag_isempty(empdag)) {

      RhpSense sense;
      S_CHECK(mdl_getsense(mdl, &sense));

      if (sense != RhpMax && sense != RhpMin) {
         error("[process] ERROR: %s model %.*s #%u with sense %s has %u marginal vars. "
               "Only models with sense %s or %s are currently supported.\n",
               mdl_fmtargs(mdl), sense2str(sense), marginalVars->len, sense2str(RhpMin),
               sense2str(RhpMax));

         return Error_NotImplemented;
      }

      Avar v;
      avar_fromIdxArray(&v, marginalVars);
      S_CHECK(empdag_initFromModel(mdl, &v));
      S_CHECK(empdag_single_MP_to_Nash(empdag, "Nash_marginalVars"));

      A_CHECK(mp, empdag->mps.arr[0]);

      A_CHECK(nash, empdag->nashs.arr[0]);


   } else if (empdag->type == EmpDag_Mopec) {
      A_CHECK(nash, empdag->nashs.arr[0]);
   } else {
      TO_IMPLEMENT("marginalVar/dualvar with a complex EMPDAG");
   }

   A_CHECK(mp_marginals, empdag_newmpnamed(empdag, RhpFeasibility, "marginalVars"));

   S_CHECK(empdag_nashaddmpbyid(empdag, nash->id, mp_marginals->id));

   /* ---------------------------------------------------------------------
    * Init: Add a Nash equilibrium and a dual MP with all the marginals
    *
    * For all marginal variables λ:
    *   -  Get the corresponding equation g(x) (and whether it is flipped)
    *   - Check that λ has the right values
    *   -  Construct <λ, g(x)>
    *   - Add -<λ, g(x)> to the objective functions of both the dual MP
    *
    *  At the end, add the dual objective to the primal
    *
    * ---------------------------------------------------------------------- */

   rhp_idx *arr = marginalVars->arr;
   VarMeta * restrict vmetas = mdl->ctr.varmeta;
   rhp_idx vi_max = mdl_nvars_total(mdl), ei_max = mdl_nequs_total(mdl);

   Container *ctr = &mdl->ctr;

   rhp_idx ei_objequ_primal = IdxNA;
   if (mp) {
      ei_objequ_primal = mp_getobjequ(mp);
      S_CHECK(valid_ei_(ei_objequ_primal, ei_max, "[process]") ? OK : Error_IndexOutOfRange);
   }

   for (unsigned i = 0, len = marginalVars->len; i < len; ++i) {

      rhp_idx vi = *arr++;

      if (RHP_UNLIKELY(vi >= vi_max)) {
         error("[process] ERROR: marginal variable index %u not in range [0,%u)\n",
               vi, vi_max);
         return Error_IndexOutOfRange;
      }

      VarMeta * restrict vmeta = &vmetas[vi];
      rhp_idx ei = vmeta->dual;

      if (!chk_ei_(ei, ei_max)) {
         error("[process] ERROR: marginal variable '%s' has invalid dual equation index "
               "%u\n", mdl_printvarname(mdl, vi), ei);
      }

      /* equmeta is reallocated if the equation is flipped */

      bool flipped = mdl->ctr.equmeta[ei].ppty & EquPptyIsFlipped;
      mpid_t mpid = mdl->ctr.equmeta[ei].mp_id;


      if (flipped) {
         rhp_idx ei_new;
         S_CHECK(rmdl_equ_flip(mdl, ei, &ei_new));
         ei = ei_new;
      }

      Equ *equ = &ctr->equs[ei];

      trace_process("[process] marginalVar: processing pair (%s,%s)\n",
                    mdl_printvarname(mdl, vi), mdl_printequname(mdl, ei));

      equmeta_init(&mdl->ctr.equmeta[ei]);
      varmeta_init(vmeta);
      S_CHECK(mp_addvipair(mp_marginals, ei, vi));

      if (valid_ei(ei_objequ_primal)) {

         S_CHECK(rctr_equ_addmulv_equ_coeff(ctr, &ctr->equs[ei_objequ_primal], equ, vi, 1.));

      } else if (!valid_mpid(mpid) || mpid >= empdag->mps.len) {

         error("[process] ERROR in marginalVar: mpid for pair (%s,%s) is invalid\n",
               mdl_printvarname(mdl, vi), mdl_printequname(mdl, ei));
         return Error_RuntimeError;

      } else {

         rhp_idx ei_target = mp_getobjequ(empdag->mps.arr[mpid]);
         S_CHECK(valid_ei_(ei_target, ei_max, "[process]") ? OK : Error_IndexOutOfRange);

         S_CHECK(rctr_equ_addmulv_equ_coeff(ctr, &ctr->equs[ei_target], equ, vi, 1.));
         ctr->vars[vi].value = -ctr->vars[vi].value;

      }
   }

   return mp_finalize(mp_marginals);
}

