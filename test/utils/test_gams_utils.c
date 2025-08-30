#include "reshop_config.h"

#include <stdlib.h>
#include <stdio.h>

#include "test_gams_utils.h"
#include "reshop.h"

#define ARRAY_SIZE(arr) sizeof(arr)/sizeof(arr[0])

#ifdef _MSC_VER
#   define strcasecmp _stricmp
#   include <string.h>
#else
#   include <strings.h>
#endif

#ifdef _WIN32
#define WIN32_LEAN_AND_MEAN
#include <windows.h>
#include <processenv.h>

#define MALLOC_NULL(ptr,type,n)  (ptr) = malloc(sizeof(type)*(n)); \
   if (!(ptr)) { return NULL; }
#define REALLOC_NULL(ptr,type,n)  (ptr) = (type*)realloc((void*)(ptr), (n)*sizeof(type)); \
   if (!(ptr)) { return NULL; }

static const char * mygetenv(const char *envvar)
{
   DWORD bufsize = 4096;
   char *envval;
   MALLOC_NULL(envval, char, bufsize);

   DWORD ret = GetEnvironmentVariableA(envvar, envval, bufsize);

   if (ret == 0) {
      free(envval);
      return NULL;
   }

   if (ret > bufsize) {
      bufsize = ret;
      REALLOC_NULL(envval, char, bufsize);
      ret = GetEnvironmentVariableA(envvar, envval, bufsize);

      if (!ret) { free(envval); return NULL; }
   }

   return envval;
}

static void myfreeenvval(const char *envval)
{
   if (envval) { free((char*)envval); }
}

#else

#include <stdlib.h>
#define mygetenv getenv
#define myfreeenvval(X) 


#endif


void setup_gams(void)
{
   const char *gamsdir_env = mygetenv("RHP_GAMSDIR");
   const char *gamsdir = gamsdir_env ? gamsdir_env : GAMSDIR;

   rhp_gams_setglobalgamsdir(gamsdir);
   myfreeenvval(gamsdir_env);


   const char *gamscntr_env = mygetenv("RHP_GAMSCNTR");
   const char *gamscntr = gamscntr_env ? gamscntr_env : GAMSCNTR_DAT_FILE;

   rhp_gams_setglobalgamscntr(gamscntr);
   myfreeenvval(gamscntr_env);
}


bool gams_skip_solver(const char *slv, const char * testname)
{
   if (!slv) { return false; }

   const char *conopt_skip[] = {
       "test_oneobjvar1", "test_oneobjvar2"
   };

   if (!strcasecmp(slv, "conopt")) {
      for (unsigned i = 0, len = ARRAY_SIZE(conopt_skip); i < len; ++i) {
         if (!strcasecmp(testname, conopt_skip[i])) { return true; }
      }
   }

#if defined(__APPLE__) && defined(__x86_64__)
#include <strings.h>
   const char *skipped_slvs[] = {
#ifdef USES_DARLING
      "cplex", // Uses MKL
      "mosek", // segfaults
#else
      "knitro", // segfaults
      "cbc",    // segfaults
#endif
      "pathnlp", // segfaults
   };

   for (unsigned i = 0, len = ARRAY_SIZE(skipped_slvs); i < len; ++i) {
      if (!strcasecmp(slv, skipped_slvs[i])) { return true; }
   }
#endif


#ifdef _WIN32
   const char *skipped_slvs[] = {
      "minos", // forrtl: severe (47): write to READONLY file, unit 14, file XXX
   };

   for (unsigned i = 0, len = ARRAY_SIZE(skipped_slvs); i < len; ++i) {
      if (!strcasecmp(slv, skipped_slvs[i])) { return true; }
   }
#endif
   return false;
}
