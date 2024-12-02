/* simple main program that loads a GAMS control file and
 * does some inspection on the model instance
 */

#if defined(__linux__) && !defined(_GNU_SOURCE)

#  define _GNU_SOURCE 1

#elif defined(__APPLE__) && !defined(_DARWIN_C_SOURCE)

#  define _DARWIN_C_SOURCE

#endif

#ifdef WIN32
#define WIN32_LEAN_AND_MEAN
#define VC_EXTRALEAN
#include <windows.h>
#include <processenv.h>

#include "macros.h"
#include "printout.h"

const char * mygetenv(const char *envvar)
{
   DWORD bufsize = 4096;
   char *envval;
   MALLOC_NULL(envval, char, bufsize);

   DWORD ret = GetEnvironmentVariableA(envvar, envval, bufsize);

   if (ret == 0) {
      FREE(envval);
      return NULL;
   }

   if (ret > bufsize) {
      bufsize = ret;
      REALLOC_NULL(envval, char, bufsize);
      ret = GetEnvironmentVariableA(envvar, envval, bufsize);

      if (!ret) { FREE(envval); return NULL; }
   }

   return envval;
}

void myfreeenvval(const char *envval)
{
   if (envval) { free((char*)envval); }
}

#else

#define mygetenv getenv
#define myfreeenvval(X)

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
   int rc;
   char msg[GMS_SSSIZE];
   gmoHandle_t gmo = NULL;

   const char *banner_env = mygetenv("RHP_DRIVER_BANNER");
   if (banner_env) {
      (void)fprintf(stdout, "\nReSHOP GAMS testing driver with version %s\n\n", rhp_version());
   }
   myfreeenvval(banner_env);

   if( argc < 2 ) {
      printf("usage: %s <cntrlfile>\n", argv[0]);
      return 1;
   }

   const char *silent_env = mygetenv("RHP_DRIVER_SILENT");
   bool silent = silent_env;
   myfreeenvval(silent_env);

   rc = rhp_gms_newfromcntr(argv[1], &mdl);

   if (!mdl) {
      if (!silent) { (void)fprintf(stderr, "*** ReSHOP ERROR! Could not build model from control file %s\n", argv[1]); }
      goto _exit;
   }

   /* Still try to load GMO to set values */
   gmo = rhp_gms_getgmo(mdl);
   gevHandle_t gev = rhp_gms_getgev(mdl);
   const char *gams_sysdir = rhp_gms_getsysdir(mdl);

   if (!gmoGetReadyD(gams_sysdir, msg, sizeof(msg)) ||
       !gevGetReadyD(gams_sysdir, msg, sizeof(msg))) {
      (void)fprintf(stderr, "%s\n", msg);
      goto _exit;
   }

   /* If rhp_gms_newfromcntr errored late, we can set the GMO */
   if (rc) { 
      if (!silent) { (void)fprintf(stderr, "*** ReSHOP ERROR! Could not load model in control file %s\n", argv[1]); }
      goto _exit;
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

   const char *ldpath = getenv("RHP_LDPATH");
   if (ldpath) {
      const char *ld_library_path_cur = getenv("LD_LIBRARY_PATH");
      char *ld_library_path = NULL;
      size_t ldpath_len = strlen(ldpath);

      if (ld_library_path_cur) {
         size_t len_cur = strlen(ld_library_path_cur);
         ld_library_path = malloc(sizeof(char) * (len_cur + 2 + ldpath_len));
         if (!ld_library_path) {
            (void)fprintf(stderr, "ERROR: could not allocate memory\n");
            goto _exit;
         }

         strcpy(ld_library_path, ld_library_path_cur);
         strcat(ld_library_path, ":");
         strcat(ld_library_path, ldpath);

         setenv("LD_LIBRARY_PATH", ld_library_path, 1);
      } else {
         setenv("LD_LIBRARY_PATH", ldpath, 1);
      }

      free(ld_library_path);

   }
// TODO MACOS
#endif

   mdl_solver = rhp_newsolvermdl(mdl);
   if (!mdl_solver) {
      if (!silent) { (void)snprintf(msg, sizeof msg, "*** ReSHOP ERROR: couldn't create solver model object\n"); }
      gevLogStatPChar(gev, msg);
      goto _exit;
   }

   /* Process the solver model (reformulations, ...) */
   rc = rhp_process(mdl, mdl_solver);
   if (rc) {
      if (!silent) { (void)snprintf(msg, sizeof msg, "*** ReSHOP ERROR: EMP transformation failed! Error message is %s (%d)\n",
               rhp_status_descr(rc), rc); }
      gevLogStatPChar(gev, msg);
      goto _exit;
   }

   /* Force solvelink to 2 since with ASAN we have library issues */
   rhp_gms_set_solvelink(mdl_solver, 2);

   /* Process the solver model  */
   rc = rhp_solve(mdl_solver);
   if (rc) {
      if (!silent) { (void)snprintf(msg, sizeof msg, "*** ReSHOP ERROR: solve failed! Error message is %s (%d)\n",
               rhp_status_descr(rc), rc); }
      gevLogStatPChar(gev, msg);
      goto _exit;
   }

   /* Perform the postprocessing  */
   rc = rhp_postprocess(mdl_solver);
   if (rc) {
      if (!silent) { (void)snprintf(msg, sizeof msg, "*** ReSHOP ERROR: postprocessing failed! Error message is %s (%d)\n",
               rhp_status_descr(rc), rc); }
      gevLogStatPChar(gev, msg);
      goto _exit;
   }

_exit:
   if (rc) {
      if (!silent) { (void)fprintf(stderr, "*** ReSHOP exited with rc = %u\n", rc); }
   }

   /* Special treatment: when we have a GMO, we can return the error via this way */
   if (rc && gmo) {
      gmoSolveStatSet(gmo, gmoSolveStat_InternalErr);
      gmoModelStatSet(gmo, gmoModelStat_ErrorNoSolution);
      /* TODO(GAMS review) would gmoModelStat_ErrorUnknown be better? */
      rc = 0; /* HACK: this is necessary for the GMO value to be read? */
   } 

   if (gmo) {
      gmoUnloadSolutionLegacy(gmo);
   }

   if (mdl_solver) { rhp_mdl_free(mdl_solver); }
   if (mdl) { rhp_mdl_free(mdl); }

   return rc;
}

