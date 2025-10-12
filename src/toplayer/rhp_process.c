#include "ctr_rhp.h"
#include "empinfo.h"
#include "macros.h"
#include "mdl.h"
#include "mdl_rhp.h"
#include "mdl_transform.h"
#include "ovf_transform.h"
#include "rhp_fwd.h"
#include "timings.h"
#include "rhp_process.h"


/**
 * @brief Perform reformations on a model with EMP
 *
 * @param       mdl_user           the original model (to be reformulated)
 * @param[out]  mdl_reformulated   the reformulated model
 *
 * @return      the error code
 */
int rhp_reformulate(Model *mdl_user, Model **mdl_reformulated)
{
   Model *mdl_reform = NULL;
   EmpInfo *empinfo = &mdl_user->empinfo;

   S_CHECK(rmdl_export_latex(mdl_user, "user"));

   /* ----------------------------------------------------------------------
    * We procceed as follows:
    * - First tackle any container reformulations (like flipped equations)
    * - Then, any OVF/CCF
    * ---------------------------------------------------------------------- */

   if (ctr_needtransformations(&mdl_user->ctr)) {

      double start = get_thrdtime();
      trace_process("[process] %s model %.*s #%u: container will be transformed:\n"
                    "          - %u flipped equations\n",
                    mdl_fmtargs(mdl_user),
                    mdl_user->ctr.transformations.flipped_equs.size);

      A_CHECK(mdl_reform, rhp_mdl_new(RhpBackendReSHOP));
      S_CHECK(rmdl_initfromfullmdl(mdl_reform, mdl_user));

      empinfo = &mdl_reform->empinfo;

      S_CHECK(rmdl_ctr_transform(mdl_reform));
      mdl_user->timings->reformulation.container.total = get_thrdtime() - start;

   }

   if (empinfo_has_ovf(empinfo)) {

      double start = get_thrdtime();

      pr_info("%s model %.*s #%u: OVF/CCF detected. Performing reformulations\n",
              mdl_fmtargs(mdl_user));

      if (!mdl_reform) {
         trace_process("[process] %s model %.*s #%u: Copying model for OVF/CCF "
                       "reformulations.\n", mdl_fmtargs(mdl_user));

         A_CHECK(mdl_reform, rhp_mdl_new(RhpBackendReSHOP));
         S_CHECK(rmdl_initfromfullmdl(mdl_reform, mdl_user));

      }

      S_CHECK(ovf_transform(mdl_reform));

      mdl_reform->timings->reformulation.CCF.total = get_thrdtime() - start;
   }

   if (empdag_needs_transformations(&mdl_user->empinfo.empdag)) {

      double start = get_thrdtime();

      pr_info("%s model %.*s #%u: Performing EMPDAG transformations.\n",
           mdl_fmtargs(mdl_user));

      if (!mdl_reform) {
         trace_process("[process] %s model %.*s #%u: Copying model for EMPDAG "
                       "reformulations.\n", mdl_fmtargs(mdl_user));

         A_CHECK(mdl_reform, rhp_mdl_new(RhpBackendReSHOP));
         S_CHECK(rmdl_initfromfullmdl(mdl_reform, mdl_user));

      }

      S_CHECK(rmdl_empdag_transform(mdl_reform));

      mdl_user->timings->reformulation.empdag.total = get_thrdtime() - start;
   }

   if (mdl_reform) {
      /* Reset the type */
      S_CHECK(empdag_fini(&mdl_reform->empinfo.empdag))
      S_CHECK(mdl_recompute_modeltype(mdl_reform));
   }

   *mdl_reformulated = mdl_reform;

   return OK;
}
