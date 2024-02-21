#include "bench_utils.h"
#include "reshop.h"
#include <stdlib.h>

void setup_gams(void)
{
   char *gamsdir = getenv("RHP_GAMSDIR");
   gamsdir = gamsdir ? gamsdir : GAMSDIR;

   rhp_gams_setglobalgamsdir(gamsdir);


   char *gamscntr = getenv("RHP_GAMSCNTR");
   gamscntr = gamscntr ? gamscntr : GAMSCNTR_DAT_FILE;

   rhp_gams_setglobalgamscntr(gamscntr);
}

struct rhp_mdl* bench_init(double tol)
{
   struct rhp_mdl *mdl = rhp_mdl_new(RHP_BACKEND_RHP);
   if (!mdl) { return NULL; }

   rhp_set_option_d(mdl, "rtol", tol);

   return mdl;
}

struct rhp_mdl* bench_getgamssolver(const char *solver_name)
{
   setup_gams();
   struct rhp_mdl *mdl_gams = rhp_mdl_new(RHP_BACKEND_GAMS_GMO);
   if (!mdl_gams) { return NULL; }

   rhp_mdl_setsolvername(mdl_gams, solver_name);

   return mdl_gams;
}


