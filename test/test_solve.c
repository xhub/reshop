#include <stdio.h>
#include <stdlib.h>

#include "reshop.h"
#include "reshop-gams.h"

#if 0
static void _debug_output(struct rhp_mdl *mdl)
{
   char* dot_print = getenv("DOT_PRINT");
   char dotname[256];

   if (dot_print) {
      for (unsigned i = 0; i < (unsigned)mdl->m; ++i) {
         sprintf(dotname, "equ%d.dot", i);
         nltree_print_dot(mdl->equs[i].tree, dotname, mdl);
         printf("Equation %i\n", i);
      }
   }
}
#endif

static void usage(const char *name)
{
   printf("Usage: %s gamsfile.gms [empfilename]\n", name);
}

int main(int argc, char **argv)
{
   struct rhp_mdl *mdl = NULL;
   struct rhp_mdl *mdl_solver = NULL;
   int status;

   if (argc < 2 || argc > 3) {
      usage(argv[0]);
      exit(0);
   }

   mdl = rhp_mdl_new(RHP_BACKEND_GAMS_GMO);
   rhp_gms_setgamsdir(mdl, GAMSDIR);
   status = rhp_gms_readmodel(mdl, argv[1]);

   if (status != RHP_OK) {
      rhp_mdl_free(mdl);
      printf("Reading the model failed: status = %s (%d)\n",
             rhp_status_descr(status), status);
      return -1;
   }

   if (argc == 3) {
      status = rhp_gms_readempinfo(mdl, argv[2]);
   } else {
      status = rhp_gms_readempinfo(mdl, "empinfo.dat");
   }

   if (status != RHP_OK) {
      printf("Reading EMP file failed: status = %s (%d)\n ",
             rhp_status_descr(status), status);
      goto _error;
   }

   mdl_solver = rhp_mdl_new(RHP_BACKEND_GAMS_GMO);

   status = rhp_process(mdl, mdl_solver);
   if (status != RHP_OK) {
      printf("Transforming the problem failed: status = %s (%d)\n ",
             rhp_status_descr(status), status);
      goto _error;
   }

   status = rhp_solve(mdl_solver);
   if (status != RHP_OK) {
      printf("Solving the problem failed: status = %s (%d)\n ",
             rhp_status_descr(status), status);
      goto _error;
   }

   status = rhp_postprocess(mdl_solver);
   if (status != RHP_OK) {
      printf("Reporting the values failed: status = %s (%d)\n ",
             rhp_status_descr(status), status);
      goto _error;
   }

   status = 0;

_error:
   rhp_mdl_free(mdl);
   rhp_mdl_free(mdl_solver);

   return status;
}
