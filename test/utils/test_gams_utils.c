#include "reshop_config.h"

#include <stdlib.h>

#include "test_gams_utils.h"
#include "reshop.h"

#ifdef _WIN32
#   define strcasecmp _stricmp
#   include <string.h>
#else
#   include <strings.h>
#endif

#define ARRAY_SIZE(arr) sizeof(arr)/sizeof(arr[0])

void setup_gams(void)
{
   char *gamsdir = getenv("RHP_GAMSDIR");
   gamsdir = gamsdir ? gamsdir : GAMSDIR;

   rhp_gams_setglobalgamsdir(gamsdir);


   char *gamscntr = getenv("RHP_GAMSCNTR");
   gamscntr = gamscntr ? gamscntr : GAMSCNTR_DAT_FILE;

   rhp_gams_setglobalgamscntr(gamscntr);
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
