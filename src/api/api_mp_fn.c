#include "reshop.h"

#include "checks.h"
#include "empdag.h"
#include "empinfo.h"
#include "macros.h"
#include "mathprgm.h"
#include "mdl.h"
#include "printout.h"
#include "status.h"

/** @copydoc mp_addconstraint
 *  @ingroup publicAPI */
int rhp_mp_addconstraint(MathPrgm *mp, rhp_idx ei)
{
   S_CHECK(chk_mp(mp, __func__));
   return mp_addconstraint(mp, ei);
}

/** @copydoc mp_addequ
 *  @ingroup publicAPI */
int rhp_mp_addequ(MathPrgm *mp, rhp_idx ei)
{
   S_CHECK(chk_mp(mp, __func__));
   return mp_addequ(mp, ei);
}

/** @copydoc mp_setobjequ
 *  @ingroup publicAPI */
int rhp_mp_setobjequ(MathPrgm *mp, rhp_idx objequ)
{
   S_CHECK(chk_mp(mp, __func__));
   return mp_setobjequ(mp, objequ);
}

/** @copydoc mp_setobjvar
 *  @ingroup publicAPI */
int rhp_mp_setobjvar(MathPrgm *mp, rhp_idx objvar)
{
   S_CHECK(chk_mp(mp, __func__));
   return mp_setobjvar(mp, objvar);
}

/** @copydoc mp_addvar
 *  @ingroup publicAPI */
int rhp_mp_addvar(MathPrgm *mp, rhp_idx vi)
{
   S_CHECK(chk_mp(mp, __func__));
   return mp_addvar(mp, vi);
}

/** @copydoc mp_addvars
 *  @ingroup publicAPI */
int rhp_mp_addvars(MathPrgm *mp, Avar *v)
{
   S_CHECK(chk_mp(mp, __func__));
   S_CHECK(chk_arg_nonnull(v, 2, __func__));
   return mp_addvars(mp, v);
}

/** @copydoc mp_addvipair
 *  @ingroup publicAPI */
int rhp_mp_addvipair(MathPrgm *mp, rhp_idx ei, rhp_idx vi)
{
   S_CHECK(chk_mp(mp, __func__));
   return mp_addvipair(mp, ei, vi);
}

/** @brief Add a vi pair (equ, var) to the MP
 *
 *  @ingroup publicAPI
 *
 *  @param  mp    the MP
 *  @param  e     the equation indices
 *  @param  v     the variable indices
 *
 *  @return       the error code
 */
int rhp_mp_addvipairs(MathPrgm *mp, Aequ *e, Avar *v)
{
   S_CHECK(chk_mp(mp, __func__));
   S_CHECK(chk_arg_nonnull(e, 2, __func__));
   S_CHECK(chk_arg_nonnull(v, 3, __func__));
   return mp_addvipairs(mp, e, v);
}

/** @copydoc mp_finalize
 *  @ingroup publicAPI */
int rhp_mp_finalize(struct rhp_mathprgm *mp)
{
   S_CHECK(chk_mp(mp, __func__));
   return mp_finalize(mp);
}

/** @copydoc mp_getobjequ
 *  @ingroup publicAPI */
rhp_idx rhp_mp_getobjequ(const MathPrgm *mp)
{
   if (chk_mp(mp, __func__) != OK) { return IdxError; }

   return mp_getobjequ(mp);
}

/** @copydoc mp_getid
 *  @ingroup publicAPI */
unsigned rhp_mp_getid(const MathPrgm *mp)
{
   if (chk_mp(mp, __func__) != OK) { return MpId_NA; }
   return mp_getid(mp);
}

/**
 * @brief Get the model associated with a MP
 *
 * @ingroup publicAPI
 *
 * @param mp  the MP
 *
 * @return    the Model associated with the MP
 */
const struct rhp_mdl* rhp_mp_getmdl(const struct rhp_mathprgm *mp)
{
   SN_CHECK(chk_mp(mp, __func__));
   return mp->mdl;
}

/** @copydoc mp_getsense
 *  @ingroup publicAPI */
unsigned rhp_mp_getsense(const MathPrgm *mp)
{
   if (chk_mp(mp, __func__) != OK) { return MpId_NA; }
   return mp_getsense(mp);
}

/** @copydoc mp_getobjvar
 *  @ingroup publicAPI */
rhp_idx rhp_mp_getobjvar(const MathPrgm *mp)
{
   if (chk_mp(mp, __func__) != OK) { return IdxError; }
   return mp_getobjvar(mp);
}

/** @copydoc mp_print
 *  @ingroup publicAPI */
int rhp_mp_print(MathPrgm *mp)
{
   S_CHECK(chk_mp(mp, __func__));
   S_CHECK(chk_mdl(mp->mdl, __func__));

   mp_print(mp);
   return OK;
}

/**
 * @brief Set the name of an MP
 *
 * @ingroup publicAPI
 *
 * @param mp   the MP
 * @param name the name
 *
 * @return     the error code
 */
int rhp_mp_setname(MathPrgm *mp, const char* name)
{
   Model *mdl = mp->mdl;
   S_CHECK(chk_mdl(mdl, __func__));
   S_CHECK(chk_arg_nonnull(name, 2, __func__));

   return empdag_setmpname(&mdl->empinfo.empdag, mp->id, name);
}
