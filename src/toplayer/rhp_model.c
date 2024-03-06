#include "mdl_data.h"
#include "empdag.h"
#include "macros.h"
#include "mdl.h"
#include "mdl_rhp.h"
#include "rhp_alg.h"
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

   S_CHECK(rmdl_prepare_export(mdl, mdl_solver));


  /* ----------------------------------------------------------------------
   * If we solve it via GAMS and it is a MOPEC, then create the MCP
   *
   * TODO: GITLAB #87
   * ---------------------------------------------------------------------- */

   S_CHECK(mdl_exportmodel(mdl, mdl_solver));

   /* We have to do this here  */
   ModelType probtype;
   mdl_gettype(mdl_solver, &probtype);

   if (probtype == MdlType_none) {
      S_CHECK(mdl_copyprobtype(mdl_solver, mdl));
   }

   return OK;
}
