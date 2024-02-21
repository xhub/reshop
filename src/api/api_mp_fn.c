#include "reshop.h"

#include "checks.h"
#include "empdag.h"
#include "empinfo.h"
#include "macros.h"
#include "mathprgm.h"
#include "mdl.h"
#include "printout.h"
#include "status.h"

int rhp_mp_addconstraint(MathPrgm *mp, rhp_idx ei)
{
   S_CHECK(chk_mp(mp, __func__));
   return mp_addconstraint(mp, ei);
}

int rhp_mp_addequ(MathPrgm *mp, rhp_idx ei)
{
   S_CHECK(chk_mp(mp, __func__));
   return mp_addequ(mp, ei);
}

int rhp_mp_setobjequ(MathPrgm *mp, rhp_idx ei)
{
   S_CHECK(chk_mp(mp, __func__));
   return mp_setobjequ(mp, ei);
}

int rhp_mp_setobjvar(MathPrgm *mp, rhp_idx vi)
{
   S_CHECK(chk_mp(mp, __func__));
   return mp_setobjvar(mp, vi);
}

int rhp_mp_addvar(MathPrgm *mp, rhp_idx vi)
{
   S_CHECK(chk_mp(mp, __func__));
   return mp_addvar(mp, vi);
}

int rhp_mp_addvars(MathPrgm *mp, Avar *v)
{
   S_CHECK(chk_mp(mp, __func__));
   S_CHECK(chk_arg_nonnull(v, 2, __func__));
   return mp_addvars(mp, v);
}

int rhp_mp_addvipair(MathPrgm *mp, rhp_idx ei, rhp_idx vi)
{
   S_CHECK(chk_mp(mp, __func__));
   return mp_addvipair(mp, ei, vi);
}

int rhp_mp_addvipairs(MathPrgm *mp, Aequ *e, Avar *v)
{
   S_CHECK(chk_mp(mp, __func__));
   S_CHECK(chk_arg_nonnull(e, 2, __func__));
   S_CHECK(chk_arg_nonnull(v, 3, __func__));
   return mp_addvipairs(mp, e, v);
}

int rhp_mp_finalize(struct rhp_mathprgm *mp)
{
   S_CHECK(chk_mp(mp, __func__));
   return mp_finalize(mp);
}

rhp_idx rhp_mp_getobjequ(const MathPrgm *mp)
{
   if (chk_mp(mp, __func__) != OK) { return IdxError; }

   return mp_getobjequ(mp);
}

unsigned rhp_mp_getid(const MathPrgm *mp)
{
   if (chk_mp(mp, __func__) != OK) { return MpId_NA; }
   return mp_getid(mp);
}

const struct rhp_mdl* rhp_mp_getmdl(const struct rhp_mathprgm *mp)
{
   SN_CHECK(chk_mp(mp, __func__));
   return mp->mdl;
}

unsigned rhp_mp_getsense(const MathPrgm *mp)
{
   if (chk_mp(mp, __func__) != OK) { return MpId_NA; }
   return mp_getsense(mp);
}

rhp_idx rhp_mp_getobjvar(const MathPrgm *mp)
{
   if (chk_mp(mp, __func__) != OK) { return IdxError; }
   return mp_getobjvar(mp);
}

int rhp_mp_print(MathPrgm *mp)
{
   S_CHECK(chk_mp(mp, __func__));
   S_CHECK(chk_mdl(mp->mdl, __func__));
   mp_print(mp, mp->mdl);
   return OK;
}

int rhp_mp_setname(MathPrgm *mp, const char* name)
{
   Model *mdl = mp->mdl;
   S_CHECK(chk_mdl(mdl, __func__));
   S_CHECK(chk_arg_nonnull(name, 2, __func__));

   return empdag_setmpname(&mdl->empinfo.empdag, mp->id, name);
}

int rhp_mp_settype(MathPrgm *mp, unsigned type)
{
   S_CHECK(chk_mp(mp, __func__));
   return mp_settype(mp, type);
}
