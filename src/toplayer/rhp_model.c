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
    * First of all, some checks:
    * - on the source model
    * - on the metadata
    * 
    * First the empdag is finalized 
    * ---------------------------------------------------------------------- */

   S_CHECK(empdag_finalize(mdl));

   S_CHECK(mdl_check(mdl));
   S_CHECK(mdl_checkmetadata(mdl));

  /* ----------------------------------------------------------------------
   * Set the fops, but after the check
   * ---------------------------------------------------------------------- */

   if (fops) {
      S_CHECK(rmdl_setfops(mdl, fops));
   }

   S_CHECK(rmdl_prepare_export(mdl, mdl_solver));


  /* ----------------------------------------------------------------------
   * If we solve it via GAMS and it is a MOPEC, then create the MCP
   *
   * TODO: GITLAB #87
   * TODO GITLAB #88
   * ---------------------------------------------------------------------- */

#if 0
   /* We have to do this here  */
   ProbType probtype_src;
   mdl_getprobtype(mdl, &probtype_src);

   if (probtype_src == MdlProbType_emp && mdl_solver->backend == RHP_BACKEND_GAMS_GMO &&
      !getenv("RHP_NO_JAMS_EMULATION")) {

      const EmpDag *empdag = &mdl->empinfo.empdag;

      /* TODO this is a bit hackish */
      switch (empdag->type) {
      case EmpDag_Mopec:
      case EmpDag_Vi:
      case EmpDag_Simple_Vi:
         return mdl_exportasgmo(mdl, mdl_solver); 
      default: ;
      }

      error("[process] ERROR: don't know how to solve %s model '%.*s' #%u of "
            "type %s with empdag type %s\n", mdl_fmtargs(mdl),
            probtype_name(probtype_src), empdag_typename(empdag->type));

      return Error_NotImplemented;
   }
#endif

   S_CHECK(mdl_exportmodel(mdl, mdl_solver));

   /* We have to do this here  */
   ProbType probtype;
   mdl_getprobtype(mdl_solver, &probtype);

   if (probtype == MdlProbType_none) {
      S_CHECK(mdl_copyprobtype(mdl_solver, mdl));
   }

   return OK;
}
