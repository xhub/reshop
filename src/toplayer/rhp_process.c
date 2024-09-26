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
 * @param       mdl_orig           the original model (to be reformulated)
 * @param[out]  mdl_reformulated   the reformulated model
 *
 * @return      the error code
 */
int rhp_reformulate(Model *mdl_orig, Model **mdl_reformulated)
{
   Model *mdl_reform = NULL;
   EmpInfo *empinfo = &mdl_orig->empinfo;

   S_CHECK(rmdl_export_latex(mdl_orig, "user"));

  /* ----------------------------------------------------------------------
   * We procceed as follows:
   * - First tackle any container reformulations (like flipped equations)
   * - Then, any OVF/CCF
   * ---------------------------------------------------------------------- */

   if (ctr_needreformulation(&mdl_orig->ctr)) {
      double start = get_thrdtime();
      trace_process("[process] %s model %.*s #%u: container will be transformed:\n"
                    "          - %u flipped equations\n",
                    mdl_fmtargs(mdl_orig),
                    mdl_orig->ctr.transformations.flipped_equs.size);

      A_CHECK(mdl_reform, rhp_mdl_new(RHP_BACKEND_RHP));
      S_CHECK(rmdl_initfromfullmdl(mdl_reform, mdl_orig));

      empinfo = &mdl_reform->empinfo;

      S_CHECK(rmdl_ctr_reformulate(mdl_reform));
      mdl_orig->timings->reformulation.container.total = get_thrdtime() - start;
   }

   if (empinfo_has_ovf(empinfo)) {

      double start = get_thrdtime();

      trace_process("[process] %s model %.*s #%u: CCF detected\n",
                    mdl_fmtargs(mdl_orig));

      if (!mdl_reform) {
         trace_process("[process] %s model %.*s #%u: Copying model for "
                       "reformulations.\n", mdl_fmtargs(mdl_orig));

         A_CHECK(mdl_reform, rhp_mdl_new(RHP_BACKEND_RHP));
         S_CHECK(rmdl_initfromfullmdl(mdl_reform, mdl_orig));

         empinfo = &mdl_reform->empinfo;
      }

      S_CHECK(ovf_transform(mdl_reform));

      mdl_reform->timings->reformulation.CCF.total = get_thrdtime() - start;
   }

   if (empdag_needs_transformations(&mdl_orig->empinfo.empdag)) {

      double start = get_thrdtime();

      trace_process("[process] %s model %.*s #%u: EMPDAG transformations detected\n",
                    mdl_fmtargs(mdl_orig));

      if (!mdl_reform) {
         trace_process("[process] %s model %.*s #%u: Copying model for "
                       "EMPDAG reformulations.\n", mdl_fmtargs(mdl_orig));

         A_CHECK(mdl_reform, rhp_mdl_new(RHP_BACKEND_RHP));
         S_CHECK(rmdl_initfromfullmdl(mdl_reform, mdl_orig));

         empinfo = &mdl_reform->empinfo;
      }

      S_CHECK(rmdl_empdag_transform(mdl_reform));

      mdl_orig->timings->reformulation.empdag.total = get_thrdtime() - start;
   }

   if (mdl_reform) {
      /* Reset the type */
      S_CHECK(empdag_fini(&mdl_reform->empinfo.empdag))
      S_CHECK(mdl_reset_modeltype(mdl_reform, NULL));
   }

   *mdl_reformulated = mdl_reform;

   return OK;
}
