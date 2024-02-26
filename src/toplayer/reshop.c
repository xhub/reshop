#include "asprintf.h"

#include "checks.h"
#include "container.h"
#include "container_helpers.h"
#include "ctr_rhp.h"
#include "gams_solve.h"
#include "macros.h"
#include "mdl.h"
#include "mdl_rhp.h"
#include "rhp_alg.h"
#include "printout.h"
#include "reshop.h"
#include "rhp_model.h"
#include "rhp_process.h"
#include "status.h"
#include "timings.h"



/**
 * @brief Allocate a ReSHOP model for a given container
 *
 * @param backend     the backend for the model
 *
 * @return         the ReSHOP model
 */
Model *rhp_mdl_new(unsigned backend)
{
   return mdl_new(backend);
}


/**
 * @brief Free a reshop model and release the container and empinfo structures
 *
 * @param mdl  the reshop model to free
 */
void rhp_mdl_free(Model *mdl)
{
   if (mdl) {
      mdl_release(mdl);
   }
}

/**
 *  @brief Postprocessed the models, from the solver model back to the input one
 *
 *  Report the values (level, multipliers) of the variables and equations
 *  This function starts from the solver model, and backpropagates the values to
 *  the model, until the original one.
 *
 *  @param  mdl_solver  the model used by the solver
 *
 *  @return             the error code
 */
int rhp_postprocess(Model *mdl_solver)
{
   double start = get_thrdtime();
   S_CHECK(chk_mdl(mdl_solver, __func__));

   int mstat, sstat;
   Model *mdl = mdl_solver;
   Model *mdl_up = mdl_solver->mdl_up;
   assert(mdl_up);

   while (mdl_up) {
       trace_solreport("[solreport] %s model '%.*s' #%u: reporting values from %s "
                       "model '%.*s' #%u\n", mdl_fmtargs(mdl_up), mdl_fmtargs(mdl));

      S_CHECK(mdl_reportvalues(mdl_up, mdl));
      S_CHECK(ctr_evalequvar(&mdl_up->ctr));

      /* ------------------------------------------------------------------
       * Get the solve and model status
       * ------------------------------------------------------------------ */

      S_CHECK(mdl_getmodelstat(mdl, &mstat));
      S_CHECK(mdl_setmodelstat(mdl_up, mstat));
      S_CHECK(mdl_getsolvestat(mdl, &sstat));
      S_CHECK(mdl_setsolvestat(mdl_up, sstat));

      if (mdl_is_rhp(mdl_up)) {
         /* deleted equations and equations in func2eval */
         S_CHECK(rctr_evalfuncs(&mdl_up->ctr));
         /* if required, set the objective function value to the objvar one */
         S_CHECK(rmdl_fix_objequ_value(mdl_up));
      }



      mdl = mdl_up;
      mdl_up = mdl->mdl_up ? mdl->mdl_up : NULL;
   }

   /* ----------------------------------------------------------------------
    * Postprocessing the final, or user facing, model
    * For instance, in the case we had a manually created an objective variable,
    * we may have to hide it from the user. Same for objective equation
    *
    * TODO(xhub) URG
    * ---------------------------------------------------------------------- */

   S_CHECK(mdl_postprocess(mdl));

   S_CHECK(mdl_copystatsfromsolver(mdl, mdl_solver));

   mdl->timings->postprocessing = get_thrdtime() - start;

   if (optvalb(mdl, Options_Display_Timings)) {
      mdl_timings_print(mdl, PO_INFO);
   }
   return OK;
}

/**
 * @brief Solve the model
 *
 * @param mdl  model to solve
 *
 * @return     the error code
 */
int rhp_solve(Model *mdl)
{
   S_CHECK(chk_mdl(mdl, __func__));

   if (!mdl->mdl_up) { TO_IMPLEMENT("rhp_solve without upstream model"); }
   S_CHECK(mdl_check(mdl->mdl_up));

   return mdl_solve(mdl);
}

/**
 *  @brief Process the input model into a model for the solver
 *
 *  Firstly, if there are some EMP information, the model is transformed
 *
 *  Secondly, the solver model is created
 *
 *  @param  mdl         the user or input model
 *  @param  mdl_solver  the solver model
 *
 *  @return             the error code
 */
int rhp_process(Model *mdl, Model *mdl_solver)
{
   S_CHECK(chk_mdl(mdl, __func__));

   if (!mdl_solver) {
      errormsg("[process] ERROR: the solver model argument is NULL!\n");
      return Error_NullPointer;
   }

   /* TODO: TEST*/
   /* ---------------------------------------------------------------------
    * If the solver is not empty, errors
    * --------------------------------------------------------------------- */
   unsigned m = ctr_nequs_total(&mdl_solver->ctr), n = ctr_nvars_total(&mdl_solver->ctr);
   if (m != 0 || n != 0) {
      error("[process] ERROR solver, a %s model named '%.*s' #%u, is not empty:"
            " n = %u and m = %u", mdl_fmtargs(mdl_solver), n, m);
      return Error_UnExpectedData;
   }


   ctr_setneednames(&mdl->ctr);

   int status = OK;

   Model *mdl_local = NULL;

   S_CHECK_EXIT(mdl_check(mdl));
   S_CHECK_EXIT(mdl_checkmetadata(mdl));

   /* ---------------------------------------------------------------------
    * PART I: Perform any kind of reformulation
    * --------------------------------------------------------------------- */

   S_CHECK_EXIT(rhp_reformulate(mdl, &mdl_local));

   /* ----------------------------------------------------------------------
    * PART II: prepare the mdl_solver object
    *
    * If the model changed, then we have the following:
    * - mdl_local contains the local model
    *
    * The following will be done
    * - Compute the modeltype
    * - Compute initial values for new variables
    * - Compress and export the changed model to the solver model
    * ---------------------------------------------------------------------- */

   /* TODO: GITLAB #87  refactor the following mess */
   if (mdl_local) {

      /* -------------------------------------------------------------------
       * Compute good initial value
       *
       * TODO(Xhub) URG add option for that?
       * ------------------------------------------------------------------- */

      S_CHECK_EXIT(rmdl_presolve(mdl_local, mdl_solver->backend));

      S_CHECK_EXIT(rmdl_ensurefops_activedefault(mdl_local));

      S_CHECK(rmdl_export_latex(mdl_local, "transformed"));

      S_CHECK_EXIT(rmdl_exportmodel(mdl_local, mdl_solver, NULL));

   } else if (mdl_is_rhp(mdl)) {
   /* ----------------------------------------------------------------------
    * If the model is RHP-like and didn't change, we just filter the active
    * variables and analyze the model to set the model type
    * ---------------------------------------------------------------------- */

      /* TODO This looks like a big hack  */
      S_CHECK_EXIT(rmdl_ensurefops_activedefault(mdl));
      S_CHECK_EXIT(rmdl_analyze_modeltype(mdl, NULL));
      S_CHECK_EXIT(rmdl_exportmodel(mdl, mdl_solver, NULL));

   } else if (mdl->backend == RHP_BACKEND_GAMS_GMO) {
   /* ----------------------------------------------------------------------
    * Model from GAMS are always with only active variable and equations
    *
    * If we reached here, we didn't do any processing. If the problem type is
    * classical (aka not EMP), then we just solve the problem directly. Otherwise:
    * - If we have a continuous Nash Equilibrium problem, we form the VI/MCP
    * - If we have a bilevel problem, we build the MPEC/MPCC
    * ---------------------------------------------------------------------- */

      if (mdl_solver->backend == RHP_BACKEND_GAMS_GMO) {

         S_CHECK_EXIT(mdl_exportasgmo(mdl, mdl_solver));


      } else if (mdl_solver->backend != RHP_BACKEND_RHP) {
         errormsg("[process] ERROR: a GAMS model can be solved either by ReSHOP");
         return Error_RuntimeError;
      }

       /* ----------------------------------------------------------------------
       * Copy the options
       *
       * TODO(xhub) this is terrible
       * ---------------------------------------------------------------------- */

      S_CHECK(mdl_copysolveoptions(mdl_solver, mdl));
   } else {
      errormsg("[process] ERROR unsupported case!");
      return Error_RuntimeError;
   }

_exit:
   if (mdl_local) {
      mdl_release(mdl_local);
   }

   if (mdl->backend == RHP_BACKEND_JULIA) {
      ctr_unsetneednames(&mdl->ctr);
   }

   return status;
}
