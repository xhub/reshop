#include <stdio.h>
#include <stdlib.h>

#include "container.h"
#include "equ.h"
#include "empinfo.h"
#include "macros.h"
#include "mdl.h"
#include "status.h"
#include "reshop-gams.h"
#include "var.h"

static void usage(const char *name)
{
   printf("Usage: %s gamsfile.gms [empfilename]\n", name);
}

int main(int argc, char **argv)
{
   int status = OK;

   if (argc < 2 || argc > 3) {
      usage(argv[0]);
      exit(0);
   }

   struct rhp_mdl *mdl;
   A_CHECK(mdl, rhp_mdl_new(RHP_BACKEND_GAMS_GMO));
   status = rhp_gms_readmodel(mdl, argv[1]);
   if (status != OK) {
      rhp_mdl_free(mdl);
      printf("Reading the model failed: status = %s (%d)\n",
             rhp_status_descr(status), status);
      goto _exit;
   }

   Container *ctr = &mdl->ctr;
   printf("N = %5u, M = %5u\n", ctr->n, ctr->m);

   for (size_t i = 0; i < ctr->n; i++) {
      printf("Var[%5zu]:", i+1);
      var_print(&ctr->vars[i]);
   }

   for (size_t i = 0; i < ctr->m; i++) {
      printf("Equ[%5zu]:", i+1);
      equ_print(&ctr->equs[i]);
   }

   if (argc == 3) {
      status = rhp_gms_readempinfo(mdl, argv[2]);
   } else {
      status = rhp_gms_readempinfo(mdl, "empinfo.dat");
   }

   if (status == OK) {
      rhp_print_emp(mdl);
   } else {
      printf("Reading EMP file failed: status = %s (%d)\n ",
             rhp_status_descr(status), status);
   }

_exit:
   rhp_mdl_free(mdl);

   return status;
}
