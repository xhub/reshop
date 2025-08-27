#include "checks.h"
#include "empdag.h"
#include "macros.h"
#include "mdl.h"
#include "reshop.h"

/**
 * @brief Get the ID of a Nash equilibrium
 *
 * @ingroup publicAPI
 *
 * @param mpe  the Nash equilibrium
 *
 * @return     the ID, or UINT_MAX on error
 */
unsigned rhp_mpe_getid(const struct rhp_nash_equilibrium *mpe)
{
   if (chk_nash(mpe, __func__) != OK) { return UINT_MAX; }
   return mpe->id;
}

/**
 * @brief Get the name of a Nash equilibrium
 *
 * @ingroup publicAPI
 *
 * @param mpe  the Nash equilibrium
 *
 * @return     the name, or NULL on error
 */
const char* rhp_mpe_getname(const struct rhp_nash_equilibrium *mpe)
{
   SN_CHECK(chk_nash(mpe, __func__));
   unsigned mpe_id = mpe->id;
   const struct nash_namedarray *mpes = &mpe->mdl->empinfo.empdag.nashs;
   const char *mpe_name = mpe_id < mpes->len ? mpes->names[mpe_id] : NULL;

   return mpe_name;
}

/**
 * @brief Get the number of children of a Nash equilibrium
 *
 * @ingroup publicAPI
 *
 * @param mpe  the Nash equilibrium
 *
 * @return     the number of children, or UINT_MAX on error
 */
unsigned rhp_mpe_getnumchildren(const struct rhp_nash_equilibrium *mpe)
{
   if (chk_nash(mpe, __func__) != OK) { return UINT_MAX; }
   unsigned mpe_id = mpe->id;
   const struct nash_namedarray *mpes = &mpe->mdl->empinfo.empdag.nashs;
 
   return mpe_id < mpes->len ? mpes->arcs[mpe_id].len : UINT_MAX;
}

/**
 * @brief Print the content of a Nash equilibrium
 *
 * @ingroup publicAPI
 *
 * @param mpe  the Nash equilibrium
 */
void rhp_mpe_print(struct rhp_nash_equilibrium *mpe)
{
   unsigned mpe_id = mpe->id;
   const struct nash_namedarray *mpes = &mpe->mdl->empinfo.empdag.nashs;
   const char *mpe_name = mpe_id < mpes->len ? mpes->names[mpe_id] : NULL;
   logger(PO_INFO, "Nash Equilibrium '%s' (ID #%u) with %u children\n", mpe_name,
          mpe_id, mpe_id < mpes->len ? mpes->arcs[mpe_id].len : 0);
}

