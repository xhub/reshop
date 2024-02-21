#include <stdio.h>
#include <stdlib.h>

#include "reshop.h"

static void usage(const char *name)
{
   printf("Usage: %s gamsfile.gms\n", name);
}

int main(int argc, char **argv)
{
   int status;

   if (argc != 3) {
      usage(argv[0]);
      exit(0);
   }

   struct rhp_mdl *mdl = rhp_mdl_new(RHP_BACKEND_GAMS_GMO);
   status = rhp_gms_readmodel(mdl, argv[1]);

   if (status != RHP_OK) {
      rhp_mdl_free(mdl);
      printf("Reading the model has failed: status = %s (%d)\n",
             rhp_status_descr(status), status);
      return -1;
   }

   rhp_solve(mdl);
   rhp_mdl_free(mdl);

   return EXIT_SUCCESS;
}
