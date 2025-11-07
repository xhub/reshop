/* simple main program that loads a GAMS control file and
 * does some inspection on the model instance
 */

#include "reshop_priv.h"
#if defined(__linux__) && !defined(_GNU_SOURCE)

#  define _GNU_SOURCE 1

#elif defined(__APPLE__) && !defined(_DARWIN_C_SOURCE)

#  define _DARWIN_C_SOURCE

#endif

#ifdef WIN32
#define WIN32_LEAN_AND_MEAN
#include <windows.h>
#include <processenv.h>

#include "macros.h"
#include "printout.h"

static const char * mygetenv_(const char *envvar)
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

static void myfreeenvval_(const char *envval)
{
   if (envval) { free((char*)envval); }
}

#else

#define mygetenv_ getenv
#define myfreeenvval_(X)

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

#ifdef __linux__
#define ENVVAR_DST "LD_LIBRARY_PATH"
#elif defined(__APPLE__)
#define ENVVAR_DST "DYLD_FALLBACK_LIBRARY_PATH"
#endif

#ifdef ENVVAR_DST
static int inject_ldpath(const char *envvar_dst, const char *gams_sysdir)
{
   if (getenv("RHP_GAMSDIR_LDPATH")) {
      const char *ld_library_path_cur = getenv(envvar_dst);
      char *ld_library_path = NULL;
      size_t len_sysdir = strlen(gams_sysdir);

      if (ld_library_path_cur) {
         size_t len_cur = strlen(ld_library_path_cur);
         ld_library_path = malloc(sizeof(char) * (len_cur + 2 + len_sysdir));
         if (!ld_library_path) {
            (void)fprintf(stderr, "ERROR: could not allocate memory\n");
            return 1;
         }

         strcpy(ld_library_path, ld_library_path_cur);
         strcat(ld_library_path, ":");
         strcat(ld_library_path, gams_sysdir);

         setenv("envvar_dst", ld_library_path, 1);
      } else {
         setenv("envvar_dst", gams_sysdir, 1);
      }

   }

   const char *ldpath = getenv("RHP_LDPATH");
   if (ldpath) {
      const char *ld_library_path_cur = getenv(envvar_dst);
      char *ld_library_path = NULL;
      size_t ldpath_len = strlen(ldpath);

      if (ld_library_path_cur) {
         size_t len_cur = strlen(ld_library_path_cur);
         ld_library_path = malloc(sizeof(char) * (len_cur + 2 + ldpath_len));
         if (!ld_library_path) {
            (void)fprintf(stderr, "ERROR: could not allocate memory\n");
            return 1;
         }

         strcpy(ld_library_path, ld_library_path_cur);
         strcat(ld_library_path, ":");
         strcat(ld_library_path, ldpath);

         setenv("envvar_dst", ld_library_path, 1);
      } else {
         setenv("envvar_dst", ldpath, 1);
      }

      free(ld_library_path);

   }

   return 0;
}
#endif

int main(int argc, char** argv)
{
   struct rhp_mdl *mdl = NULL, *mdl_solver = NULL;
   int rc;
   char msg[GMS_SSSIZE];
   gmoHandle_t gmo = NULL;
   gevHandle_t gev = NULL;

   const char *banner_env = mygetenv_("RHP_DRIVER_BANNER");
   if (banner_env) {
      (void)fprintf(stdout, "\nReSHOP GAMS testing driver with version %s\n\n", rhp_version());
   }
   myfreeenvval_(banner_env);

   if( argc < 2 ) {
      printf("usage: %s <cntrlfile>\n", argv[0]);
      return 1;
   }

   const char *silent_env = mygetenv_("RHP_DRIVER_SILENT");
   bool silent = silent_env;
   myfreeenvval_(silent_env);

   rc = rhp_gms_newfromcntr(argv[1], &mdl);

   if (!mdl) {
      if (!silent) { (void)fprintf(stderr, "*** ReSHOP ERROR! Could not build model from control file %s\n", argv[1]); }
      goto _exit;
   }

   /* Still try to load GMO to set values */
   gmo = rhp_gms_getgmo(mdl);
   gev = rhp_gms_getgev(mdl);
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

#ifdef ENVVAR_DST
   if (inject_ldpath(ENVVAR_DST, gams_sysdir)) {
      goto _exit;
   }
#endif

   if (rhp_syncenv()) {
      gevStatCon(gev);
      gevLogStatNoC(gev, "\n\n*** ReSHOP ERROR: Failed to sync with environment variables");
      gevStatCoff(gev);
      rc = 1; goto _exit;
   }

   /* From now on we can use the GAMS log. */
   /* (data, printfn, flushgams, use_asciicolor)*/
   int logoption = gevGetIntOpt(gev, gevLogOption);
   unsigned flags = 0;
   if (logoption < 3) {
      flags |= RhpPrintNoStdOutErr;
   }

   rhp_set_printops(gev, printgams, flushgams, flags);

   /* Set some basic info */
   char userinfo[64];
   (void)snprintf(userinfo, sizeof(userinfo), "GAMS %d", gevGetIntOpt(gev, gevGamsVersion));
   rhp_set_userinfo(userinfo);

   /* Print the reshop banner */
   rhp_print_banner();

   mdl_solver = rhp_newsolvermdl(mdl);
   if (!mdl_solver) {
      if (!silent) { (void)snprintf(msg, sizeof msg, "*** ReSHOP ERROR: couldn't create solver model object\n"); }
      gevStatCon(gev);
      gevLogStatPChar(gev, msg);
      gevStatCoff(gev);
      goto _exit;
   }

   /* Process the solver model (reformulations, ...) */
   rc = rhp_process(mdl, mdl_solver);
   if (rc) {
      if (!silent) { (void)snprintf(msg, sizeof msg, "*** ReSHOP ERROR: EMP transformation failed! Error message is %s (%d)\n",
               rhp_status_descr(rc), rc); }
      gevStatCon(gev);
      gevLogStatPChar(gev, msg);
      gevStatCoff(gev);
      goto _exit;
   }

   /* Force solvelink to 2 since with ASAN we have library issues */
   rhp_gms_set_solvelink(mdl_solver, 2);

   /* Process the solver model  */
   rc = rhp_solve(mdl_solver);
   if (rc) {
      if (!silent) { (void)snprintf(msg, sizeof msg, "*** ReSHOP ERROR: solve failed! Error message is %s (%d)\n",
               rhp_status_descr(rc), rc); }
      gevStatCon(gev);
      gevLogStatPChar(gev, msg);
      gevStatCoff(gev);
      goto _exit;
   }

   /* Perform the postprocessing  */
   rc = rhp_postprocess(mdl_solver);
   if (rc) {
      if (!silent) { (void)snprintf(msg, sizeof msg, "*** ReSHOP ERROR: postprocessing failed! Error message is %s (%d)\n",
               rhp_status_descr(rc), rc); }
      gevStatCon(gev);
      gevLogStatPChar(gev, msg);
      gevStatCoff(gev);
      goto _exit;
   }

_exit:
   if (rc) {
      if (!silent) { (void)fprintf(stderr, "*** ReSHOP exited with rc = %d\n", rc); }
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

   /* Without this call, the status file is empty in some instances */
   if (gev) {
      gevLogStatFlush(gev);
   }

   if (mdl_solver) { rhp_mdl_free(mdl_solver); }
   if (mdl) { rhp_mdl_free(mdl); }

   return rc;
}

