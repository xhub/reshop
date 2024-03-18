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
      S_CHECK(mdl_solreport(mdl_up, mdl));

      mdl = mdl_up;
      mdl_up = mdl->mdl_up;
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
   S_CHECK(mdl_solvable(mdl->mdl_up));

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
   int status = OK;

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
    * - Compute initial values for new variables
    * - Compress and export the changed model to the solver model
    * ---------------------------------------------------------------------- */

   if (mdl_local) {

      /* -------------------------------------------------------------------
       * Compute good initial value
       *
       * TODO(Xhub) URG add option for that?
       * ------------------------------------------------------------------- */

      S_CHECK_EXIT(rmdl_presolve(mdl_local, mdl_solver->backend));

      S_CHECK(rmdl_export_latex(mdl_local, "transformed"));

   }

   /* TODO: GITLAB #87  refactor the following mess */
   Model *mdl4export = mdl_local ? mdl_local : mdl;

   S_CHECK_EXIT(mdl_copyassolvable(mdl_solver, mdl4export));

_exit:
   if (mdl_local) {
      mdl_release(mdl_local);
   }

   if (mdl->backend == RHP_BACKEND_JULIA) {
      ctr_unsetneednames(&mdl->ctr);
   }

   return status;
}
