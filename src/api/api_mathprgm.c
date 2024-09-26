
#include "checks.h"
#include "empdag.h"
#include "mdl.h"
#include "reshop.h"
#include "mathprgm.h"

const char* rhp_mp_getname(const struct rhp_mathprgm *mp)
{
   SN_CHECK(chk_mp(mp, __func__));
   const Model *mdl = mp->mdl;
   SN_CHECK(chk_mdl(mdl, __func__));

   return empdag_getmpname(&mp->mdl->empinfo.empdag, mp->id);
}

unsigned rhp_mp_ncons(const struct rhp_mathprgm *mp)
{
   if (chk_mp(mp, __func__) != OK) { return UINT_MAX; }

   unsigned nequs = mp->equs.len;

   if (mp_isvi(mp)) {
      return mp->vi.num_cons;
   }
   if (valid_ei(mp->opt.objequ)) {
      nequs--;
   }

   return nequs;
}

unsigned rhp_mp_nmatched(const struct rhp_mathprgm *mp)
{
   if (chk_mp(mp, __func__) != OK) { return UINT_MAX; }

   if (mp_isvi(mp)) {
      return mp->vars.len - mp->vi.num_zeros;
   }

   return 0;
}

unsigned rhp_mp_nvars(const struct rhp_mathprgm *mp)
{
   if (chk_mp(mp, __func__) != OK) { return UINT_MAX; }

   return mp->vars.len;
}

RESHOP_STATIC_ASSERT((uint8_t) RhpMin == (uint8_t) RHP_MIN, "")
RESHOP_STATIC_ASSERT((uint8_t) RhpMax == (uint8_t) RHP_MAX, "")
RESHOP_STATIC_ASSERT((uint8_t) RhpFeasibility == (uint8_t) RHP_FEAS, "")
RESHOP_STATIC_ASSERT((uint8_t) RhpDualSense == (uint8_t) RHP_DUAL_SENSE, "")
RESHOP_STATIC_ASSERT((uint8_t) RhpNoSense == (uint8_t) RHP_NOSENSE, "")

RESHOP_STATIC_ASSERT((uint8_t) MpTypeOpt == (uint8_t) RHP_MP_OPT, "")
RESHOP_STATIC_ASSERT((uint8_t) MpTypeVi == (uint8_t) RHP_MP_VI, "")
#ifdef GITLAB_83
//   RHP_MP_MCP,         /**< Mixed Complementarity Problem */
//   RHP_MP_QVI,         /**< Quasi Variational Inequality */
//   RHP_MP_MPCC,        /**< Mathematical Programm with Complementarity Constraints */
//   RHP_MP_MPEC,        /**< Mathematical Programm with Equilibrium Constraints     */
RESHOP_STATIC_ASSERT(MpTypeMcp == RHP_MP_MCP, "");
RESHOP_STATIC_ASSERT(MpTypeQvi == RHP_MP_QVI, "");
RESHOP_STATIC_ASSERT(MpTypeMpcc == RHP_MP_MPCC, "");
RESHOP_STATIC_ASSERT(MpTypeMpec == RHP_MP_MPEC, "");
#endif
RESHOP_STATIC_ASSERT((uint8_t) MpTypeCcflib == (uint8_t) RHP_MP_CCFLIB, "")
RESHOP_STATIC_ASSERT((uint8_t) MpTypeUndef == (uint8_t) RHP_MP_UNDEF, "")
