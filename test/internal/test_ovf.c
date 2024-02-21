#include <stdio.h>
#include <stdlib.h>

#include "mdl.h"
#include "reshop-gams.h"
#include "reshop.h"
#include "status.h"

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
   status = rhp_gms_readmodel(mdl, argv[1]);
   if (status != OK) {
      rhp_mdl_free(mdl);
      printf("Reading the model failed: status = %s (%d)\n",
             rhp_status_descr(status), status);
      return -1;
   }

   Container *ctr = &mdl->ctr;
   printf("N = %5u, M = %5u\n", ctr->n, ctr->m);

   if (argc == 3) {
      status = rhp_gms_readempinfo(mdl, argv[2]);
   } else {
      status = rhp_gms_readempinfo(mdl, "empinfo.dat");
   }

   if (status == OK) {
      //empinfo_print(empinfo);
   } else {
      printf("Reading EMP file failed: status = %s (%d)\n ",
             rhp_status_descr(status), status);
      return status;
   }

   mdl_solver = rhp_mdl_new(RHP_BACKEND_GAMS_GMO);

   S_CHECK(rhp_process(mdl, mdl_solver));

   O_Output_Subsolver_Log = true;

   strcpy(mdl_solver->ctr.data, "CONVERTD");
   S_CHECK(mdl_solve(mdl_solver));

   strcpy(mdl_solver->ctr.data, "CPLEX");
   S_CHECK(mdl_solve(mdl_solver));

   rhp_mdl_free(mdl);
   rhp_mdl_free(mdl_solver);


   return 0;
}
