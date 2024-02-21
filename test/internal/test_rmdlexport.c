#include <math.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <sys/stat.h>

#include "container.h"
#include "filter_ops.h"
#include "instr.h"
#include "lequ.h"
#include "macros.h"
#include "mdl_rhp.h"
#include "model_ops_gams.h"
#include "ctrdat_rhp.h"
#include "option_common.h"
#include "printout.h"
#include "status.h"
#include "reshop-gams.h"

#define random rand

#define DELTA_EPS 1e-13

static void opcode_print(unsigned codelen, int *instrs, int *args)
{
   for (unsigned i = 0; i < codelen; i++) {
      printout(PO_INFO, "Indx [%5d]\t  Instr: %10s, Field: %5d\n", i,
               instr_code_name[instrs[i]], args[i]);
   }
}

static int _cmpvar(struct var* v1, struct var *v2)
{
   if (v1->idx != v2->idx ||
     /* bstat doesn't look stable enough
       v1->bstat != v2->bstat || */
       v1->type != v2->type ||
       fabs(v1->bnd.lb - v2->bnd.lb) > DELTA_EPS ||
       fabs(v1->bnd.ub - v2->bnd.ub) > DELTA_EPS ||
       fabs(v1->value - v2->value) > DELTA_EPS ||
       fabs(v1->multiplier - v2->multiplier) > DELTA_EPS) {
      return 1;
   }

   return 0;
}

static int _cmpeqn(Container* ctr1, Container *ctr2, int eidx)
{

   struct equ *e1 = &ctr1->equs[eidx];
   struct equ *e2 = &ctr2->equs[eidx];

   if (e1->idx != e2->idx ||
     /* bstat doesn't look stable enough
       e1->bstat != e2->bstat || */
       e1->type != e2->type ||
      /*  TODO(xhub) CONE_SUPPORT */
       fabs(e1->p.rhs - e2->p.rhs) > DELTA_EPS ||
       fabs(e1->level - e2->level) > DELTA_EPS ||
       fabs(e1->marginal - e2->marginal) > DELTA_EPS) {
      return 1;
   }

   if (e1->lequ) {

      if (!e2->lequ) {
         return 1;
      }

      if (e1->lequ->len != e2->lequ->len) {
         return 1;
      }

      if (memcmp(e1->lequ->index, e2->lequ->index, e2->lequ->len*sizeof(int)))
         return 1;

      for (size_t i = 0; i < e2->lequ->len; ++i) {
         if (fabs(e2->lequ->value[i] - e1->lequ->value[i]) > DELTA_EPS) {
            error("Equation %d, linear value %d differs: delta = %e\n", e1->idx, (unsigned)i, e2->lequ->value[i] - e1->lequ->value[i]);
            return 1;
         }
      }
   }

/* Compare opcode in gmo object  */
   int codelen1, codelen2;
   int *instrs1, *instrs2;
   int *args1, *args2;
   S_CHECK(gams_getopcode(ctr1, eidx, &codelen1, &instrs1, &args1));
   S_CHECK(gams_getopcode(ctr2, eidx, &codelen2, &instrs2, &args2));

   if (codelen1 != codelen2) {
      printout(PO_INFO, "nlequ differ for equation %d:\n", eidx);
      opcode_print(codelen1, instrs1, args1);
      opcode_print(codelen2, instrs2, args2);
      return 0;
   }

   if (memcmp(instrs1, instrs2, codelen1*sizeof(int))) {
      printout(PO_INFO, "instruction differ for equation %d:\n", eidx);
      opcode_print(codelen1, instrs1, args1);
      opcode_print(codelen2, instrs2, args2);
   }

   if (memcmp(args1, args2, codelen1*sizeof(int))) {
      printout(PO_INFO, "arguments differ for equation %d:\n", eidx);
      opcode_print(codelen1, instrs1, args1);
      opcode_print(codelen2, instrs2, args2);
   }

   return 0;
}

static int _compare_gams_ctr(Container *ctr1, Container *ctr2)
{
   if (ctr1->n != ctr2->n) {
      error("n is different: %d vs %d\n", ctr1->n, ctr2->n);
      return 1;
   }

   if (ctr1->m != ctr2->m) {
      error("m is different: %d vs %d\n", ctr1->m, ctr2->m);
      return 1;
   }

   for (size_t i = 0; i < (size_t)ctr1->n; ++i) {
    if (_cmpvar(&ctr1->vars[i], &ctr2->vars[i])) {
     error("Variable %d:\n", (int)i);
     var_print(&ctr1->vars[i]);
     var_print(&ctr2->vars[i]);
     return 1;
    }
   }


   for (size_t i = 0; i < (size_t)ctr1->m; ++i) {
      if (_cmpeqn(ctr1, ctr2, i)) {
         error("Equation %zu is different:", i);
         equ_print(&ctr1->equs[i]);
         equ_print(&ctr2->equs[i]);
         return 1;
      }
   }

   return OK;
}

static void usage(const char *filename)
{
   printf("Usage: %s gams.gms\n", filename);
}

int main(int argc, char **argv)
{
   Container *ctrsrc, *ctrsrc2, *ctrdest, *ctrmtr;
   struct rhp_mdl *mdl, mdl_2, mdl_dest, mdl_mtr;
   int status;
   char buffer[SHORT_STRING];
   char scrdir[SHORT_STRING];

   if (argc < 2 || argc > 3) {
      usage(argv[0]);
      exit(0);
   }

   ctrsrc = ctrdest = ctrmtr = ctrsrc2 = NULL;
   mdl = mdl_2 = mdl_dest = mdl_mtr = NULL;

   ctrsrc = ctr_alloc(RHP_BACKEND_GAMS);

   status = rhp_gms_readmodel(ctrsrc, argv[1]);

   if (status != OK) {
      printf("Reading the model failed: status = %s (%d)\n",
             rhp_status_descr(status), status);
      status = -1;
      goto done;
   }

   mdl = rhp_alloc(crxsrc);
   status = rhp_gms_readempinfo(mdl, "empinfo.dat");

   if (status == OK) {
      ctrmtr = ctr_alloc(RHP_BACKEND_RHP);
      ctrdest = ctr_alloc(RHP_BACKEND_GAMS);
      S_CHECK(rmdl_initfromfullmdl(ctrmtr, ctrsrc));

      struct ctr_filter_ops fops_active;
      ctr_filter_active(&fops_active, (struct model_repr *)ctrmtr->data);
      S_CHECK(model_compress(ctrmtr, ctrdest, empinfo, &fops_active));
      S_CHECK(ctr_exportmodel(ctrmtr, ctrdest));

      /* ---------------------------------------------------------------------
       * Create a scratch directory where GAMS will place working files.
       * --------------------------------------------------------------------- */

      snprintf(buffer, SHORT_STRING-1, "testmtrgmo%p%d.tmp", (void *) ctrdest, random());
      strcpy(scrdir, buffer);
      mkdir(scrdir, S_IRWXU);

      /* ---------------------------------------------------------------------
       * Create an empty convertd.opt file in the scratch directory.
       * --------------------------------------------------------------------- */

      strncpy(buffer, "convertd.opt", sizeof(buffer));
      FILE *fptr = fopen(buffer, "w");
      if (fptr == NULL) {
         error("%s :: could not create convertd.opt", __FILE__);
         return Error_FileOpenFailed;
      }

      strncat(scrdir, DIRSEP "test.gms", sizeof(scrdir) - strlen(scrdir) - 1);
      fprintf(fptr, "gams %s\n", scrdir);
      fclose(fptr);

      /* -------------------------------------------------------------------
       * Set the solver to convertd and export the model
       * ------------------------------------------------------------------- */
      O_Subsolveropt = 1;
      O_Output_Subsolver_Log = 1;
      S_CHECK(gams_setsolvername(ctrdest, "convertd"));

      S_CHECK(ctr_callsolver(ctrdest));

      ctrsrc2 = ctr_alloc(RHP_BACKEND_GAMS);
      S_CHECK(rhp_gms_readmodel(ctrsrc2, scrdir));

      S_CHECK(_compare_gams_ctr(ctrsrc, ctrsrc2));

   } else {
      printf("Reading EMP file failed: status = %s (%d)\n",
             rhp_status_descr(status), status);
      status = -1;
      goto done;
   }

 done:
   empinfo_dealloc(empinfo);
   ctr_dealloc(ctrsrc);
   ctr_dealloc(ctrdest);
   ctr_dealloc(ctrsrc2);
   ctr_dealloc(ctrmtr);

   return status;
}
