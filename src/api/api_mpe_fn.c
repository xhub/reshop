#include "checks.h"
#include "empdag.h"
#include "macros.h"
#include "mdl.h"
#include "reshop.h"

unsigned rhp_mpe_getid(const struct rhp_nash_equilibrium *mpe)
{
   if (chk_nash(mpe, __func__) != OK) { return UINT_MAX; }
   return mpe->id;
}

const char* rhp_mpe_getname(const struct rhp_nash_equilibrium *mpe)
{
   SN_CHECK(chk_nash(mpe, __func__));
   unsigned mpe_id = mpe->id;
   const struct nash_namedarray *mpes = &mpe->mdl->empinfo.empdag.nashs;
   const char *mpe_name = mpe_id < mpes->len ? mpes->names[mpe_id] : NULL;

   return mpe_name;
}

unsigned rhp_mpe_getnumchildren(const struct rhp_nash_equilibrium *mpe)
{
   if (chk_nash(mpe, __func__) != OK) { return UINT_MAX; }
   unsigned mpe_id = mpe->id;
   const struct nash_namedarray *mpes = &mpe->mdl->empinfo.empdag.nashs;
   
   return mpe_id < mpes->len ? mpes->arcs[mpe_id].len : 0;
}

int rhp_mpe_print(struct rhp_nash_equilibrium *mpe)
{
   unsigned mpe_id = mpe->id;
   const struct nash_namedarray *mpes = &mpe->mdl->empinfo.empdag.nashs;
   const char *mpe_name = mpe_id < mpes->len ? mpes->names[mpe_id] : NULL;
   logger(PO_INFO, "Nash Equilibrium '%s' (ID #%u) with %u children\n", mpe_name,
          mpe_id, mpe_id < mpes->len ? mpes->arcs[mpe_id].len : 0);

   return OK;
}

