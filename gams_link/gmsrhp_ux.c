/* simple main program that loads a GAMS control file and
 * does some inspection on the model instance
 */

#ifdef __linux__
   #ifndef _GNU_SOURCE
      #define _GNU_SOURCE 1
   #endif
#elif __apple__

   #ifndef _DARWIN_C_SOURCE
      #define _DARWIN_C_SOURCE
   #endif

#endif

#include <stdio.h>
#include <stdlib.h>
#include <string.h>

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
   gmoHandle_t gmo = NULL;

   if( argc < 2 ) {
      printf("usage: %s <cntrlfile>\n", argv[0]);
      return 1;
   }


   mdl = rhp_gms_newfromcntr(argv[1]);

   if (!mdl) {
      (void)fprintf(stderr, "*** ReSHOP ERROR! Could not load model in control file %s\n", argv[1]);
      goto _exit;
   }

   gmo = rhp_gms_getgmo(mdl);
   gevHandle_t gev = rhp_gms_getgev(mdl);
   const char *gams_sysdir = rhp_gms_getsysdir(mdl);

   if (!gmoGetReadyD(gams_sysdir, msg, sizeof(msg)) ||
       !gevGetReadyD(gams_sysdir, msg, sizeof(msg))) {
      (void)fprintf(stderr, "%s\n", msg);
      rc = 1; goto _exit;
   }

#ifdef __linux__
   if (getenv("RHP_GAMSDIR_LDPATH")) {
      const char *ld_library_path_cur = getenv("LD_LIBRARY_PATH");
      char *ld_library_path = NULL;
      size_t len_sysdir = strlen(gams_sysdir);

      if (ld_library_path_cur) {
         size_t len_cur = strlen(ld_library_path_cur);
         ld_library_path = malloc(sizeof(char) * (len_cur + 2 + len_sysdir));
         if (!ld_library_path) {
            (void)fprintf(stderr, "ERROR: could not allocate memory\n");
            rc = 1;
            goto _exit;
         }

         strcpy(ld_library_path, ld_library_path_cur);
         strcat(ld_library_path, ":");
         strcat(ld_library_path, gams_sysdir);

         setenv("LD_LIBRARY_PATH", ld_library_path, 1);
      } else {
         setenv("LD_LIBRARY_PATH", gams_sysdir, 1);
      }

   }

// TODO MACOS
#endif

   mdl_solver = rhp_getsolvermdl(mdl);
   if (!mdl_solver) {
      (void)snprintf(msg, sizeof msg, "*** ReSHOP ERROR: couldn't create solver model object\n");
      gevLogStatPChar(gev, msg);
      goto _exit;
   }

   /* Process the solver model (reformulations, ...) */
   rc = rhp_process(mdl, mdl_solver);
   if (rc) {
      (void)snprintf(msg, sizeof msg, "*** ReSHOP ERROR: EMP transformation failed! Error message is %s (%d)\n",
               rhp_status_descr(rc), rc);
      gevLogStatPChar(gev, msg);
      goto _exit;
   }

   /* Force solvelink to 2 since with ASAN we have library issues */
   rhp_gms_set_solvelink(mdl_solver, 2);

   /* Process the solver model  */
   rc = rhp_solve(mdl_solver);
   if (rc) {
      (void)snprintf(msg, sizeof msg, "*** ReSHOP ERROR: solve failed! Error message is %s (%d)\n",
               rhp_status_descr(rc), rc);
      gevLogStatPChar(gev, msg);
      goto _exit;
   }

   /* Perform the postprocessing  */
   rc = rhp_postprocess(mdl_solver);
   if (rc) {
      (void)snprintf(msg, sizeof msg, "*** ReSHOP ERROR: postprocessing failed! Error message is %s (%d)\n",
               rhp_status_descr(rc), rc);
      gevLogStatPChar(gev, msg);
      goto _exit;
   }

_exit:
   if (rc && gmo) {
      gmoSolveStatSet(gmo, gmoSolveStat_InternalErr);
      gmoModelStatSet(gmo, gmoModelStat_ErrorNoSolution);
      /* TODO(GAMS review) would gmoModelStat_ErrorUnknown be better? */
   } 

   if (gmo) {
      gmoUnloadSolutionLegacy(gmo);
   }

   if (mdl_solver) { rhp_mdl_free(mdl_solver); }
   if (mdl) { rhp_mdl_free(mdl); }

   if (rc) {
      (void)fprintf(stderr, "*** ReSHOP exited with rc = %u", rc);
   }

   return rc;
}

