#include "bench_utils.h"
#include "reshop.h"
#include <stdlib.h>

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


