/* simple main program that loads a GAMS control file and
 * does some inspection on the model instance
 */

#include <stdio.h>
#include <stdlib.h>

#include "reshop.h"
#include "reshop-gams.h"

#if defined(__GNUC__) || defined(__clang__) || defined(__INTEL_LLVM_COMPILER)
#   define UNUSED __attribute__((unused))
#else
#   define UNUSED
#endif

int main(int argc, char** argv)
{
   struct rhp_mdl *mdl = NULL, *mdl_solver = NULL;
   int rc = EXIT_FAILURE;
   char msg[GMS_SSSIZE];

   if( argc < 2 )
   {
      printf("usage: %s <cntrlfile>\n", argv[0]);
      return 1;
   }

   mdl = rhp_gms_newfromcntr(argv[1]);

   if (!mdl) {
      fprintf(stderr, "*** ReSHOP ERROR! Could not load control file %s\n", argv[1]);
      goto _exit;
   }

   gmoHandle_t gmo = rhp_gms_getgmo(mdl);
   gevHandle_t gev = rhp_gms_getgev(mdl);
   const char *gams_sysdir = rhp_gms_getsysdir(mdl);

   if (!gmoGetReadyD(gams_sysdir, msg, sizeof(msg)) ||
       !gevGetReadyD(gams_sysdir, msg, sizeof(msg))) {
      fprintf(stderr, "%s\n", msg);
      rc = 1; goto _exit;
   }

   mdl_solver = rhp_getsolvermdl(mdl);
   if (!mdl_solver) {
      snprintf(msg, sizeof msg, "*** ReSHOP ERROR: couldn't create solver model object\n");
      gevLogStatPChar(gev, msg);
      goto _exit;
   }

   /* Process the solver model (reformulations, ...) */
   rc = rhp_process(mdl, mdl_solver);
   if (rc) {
      snprintf(msg, sizeof msg, "*** ReSHOP ERROR: EMP transformation failed! Error message is %s (%d)\n",
               rhp_status_descr(rc), rc);
      gevLogStatPChar(gev, msg);
      goto _exit;
   }

   /* Process the solver model  */
   rc = rhp_solve(mdl_solver);
   if (rc) {
      snprintf(msg, sizeof msg, "*** ReSHOP ERROR: solve failed! Error message is %s (%d)\n",
               rhp_status_descr(rc), rc);
      gevLogStatPChar(gev, msg);
      goto _exit;
   }

   /* Perform the postprocessing  */
   rc = rhp_postprocess(mdl_solver);
   if (rc) {
      snprintf(msg, sizeof msg, "*** ReSHOP ERROR: postprocessing failed! Error message is %s (%d)\n",
               rhp_status_descr(rc), rc);
      gevLogStatPChar(gev, msg);
      goto _exit;
   }

_exit:
   if (rc) {
      gmoSolveStatSet(gmo, gmoSolveStat_InternalErr);
      gmoModelStatSet(gmo, gmoModelStat_ErrorNoSolution);
      /* TODO(GAMS review) would gmoModelStat_ErrorUnknown be better? */
   }

   if (mdl_solver) { rhp_mdl_free(mdl_solver); }
   if (mdl) { rhp_mdl_free(mdl); }

   return rc;
}

