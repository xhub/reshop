#include "ctr_rhp.h"
#include "ctrdat_rhp.h"
#include "mathprgm.h"
#include "mdl_data.h"
#include "macros.h"
#include "mdl.h"
#include "mdl_rhp.h"
#include "printout.h"
#include "rhp_model.h"
#include "status.h"

/** @file rhp_model.c */


/**
 * @brief Export a (subset of a) model into another one
 *
 * @param mdl         the source model
 * @param mdl_solver  the model to be populated
 * @param fops        the filtering structure. If NULL, the active variable and
 *                    equations are kept.
 *
 * @return            the error code
 */
int rmdl_exportmodel(Model *mdl, Model *mdl_solver, Fops *fops)
{
   if (!mdl_is_rhp(mdl)) {
      errormsg("[exportmodel] ERROR: the first argument (source model) must be "
               "a RHP-like model.\n");
      return Error_WrongModelForFunction;
   }

  /* ----------------------------------------------------------------------
   * Set the fops, but after the check
   * ---------------------------------------------------------------------- */

   Fops *fops_backup = NULL;
   if (fops) {
      fops_backup = mdl->ctr.fops;
      mdl->ctr.fops = fops;
   }



  /* ----------------------------------------------------------------------
   * If we solve it via GAMS and it is a MOPEC, then create the MCP
   *
   * TODO: GITLAB #87
   * ---------------------------------------------------------------------- */

   S_CHECK(mdl_export(mdl, mdl_solver));

   if (fops_backup) {
      mdl->ctr.fops = fops_backup;
   }

   return OK;
}

/**
 * @brief Get a modifiable version of an equation
 *
 * @param      mdl  the model
 * @param      ei   the equation index
 * @param[out] e    the pointer to the equation
 *
 * @return        the error code
 */
int rmdl_get_equation_modifiable(Model* mdl, rhp_idx ei, Equ **e)
{
   assert(mdl_is_rhp(mdl));
   Container *ctr = &mdl->ctr;

   EquInfo equinfo;
   S_CHECK(rctr_get_equation(ctr, ei, &equinfo));

   /*  Oops the equation has been expanded */
   if (equinfo.expanded) {
      TO_IMPLEMENT("The case when a variable has been expanded");
   }

   rhp_idx ei_new = equinfo.ei;

   /* Copy if needed */
   if (equinfo.copy_if_modif) {

      if (!ctr->fops) {

      }

      S_CHECK(rmdl_dup_equ(mdl, &ei_new));
      DPRINT("%s :: dup equation '%.*s': %d -> %d\n", __func__,
             mdl_printequname(mdl, ei), ei, ei_new);

      /* -------------------------------------------------------------------
       * Keep the EMP tree consistent: add the new equation to the MP
       * ------------------------------------------------------------------- */

      MathPrgm *mp = mdl_getmpforequ(mdl, ei);
      if (mp) {
         if (mp_getobjequ(mp) == ei) {
            S_CHECK(mp_setobjequ(mp, ei_new));
         } else {
            S_CHECK(mp_rm_cons(mp, ei));
            S_CHECK(mp_addconstraint(mp, ei_new));
         }
      }
   } 

   *e = &ctr->equs[ei_new];

   return OK;
}
