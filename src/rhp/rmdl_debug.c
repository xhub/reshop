#include <float.h>

#include "cmat.h"
#include "container.h"
#include "nltree.h"
#include "nltree_priv.h"
#include "equvar_metadata.h"
#include "lequ.h"
#include "mdl.h"
#include "ctrdat_rhp.h"
#include "rmdl_debug.h"
#include "status.h"

void rmdl_debug_active_vars(Container *ctr)
{

   assert(ctr_is_rhp(ctr));
   struct ctrdata_rhp *cdat = (struct ctrdata_rhp *)ctr->data;
   size_t total_n = cdat->total_n;

   error("%s :: there are:\n%zu total variables\n%zu actives ones\n",
            __func__, total_n, (size_t)ctr->n);
   errormsg("\nList of active variables:\n");

   for (size_t i = 0; i < total_n; ++i) {
      if (cdat->vars[i]) {
         error("[%5zu] %s\n", i, ctr_printvarname(ctr, i));
      }
   }

   errormsg("\nList of inactive variables:\n");

   for (size_t i = 0; i < total_n; ++i) {
      if (!cdat->vars[i]) {
         error("[%5zu] %s\n", i, ctr_printvarname(ctr, i));
      }
   }
}

void rctr_debug_active_equs(Container *ctr)
{
   assert(ctr_is_rhp(ctr));

   struct ctrdata_rhp *model = (struct ctrdata_rhp *)ctr->data;
   size_t total_m = model->total_m;

   error("[container] There are %zu equations in total and %zu are active\n",
         total_m, (size_t)ctr->m);

   if (ctr->m > total_m) {
      error("[container] MAJOR BUG: there are more active equations (%zu) than "
            "reserved ones (%zu), Please report this!\n", (size_t)ctr->m, total_m);
   }

   errormsg("\nList of active equations:\n");

   for (size_t i = 0; i < total_m; ++i) {
      if (model->equs[i]) {
         error("[%5zu] %s\n", i, ctr_printequname(ctr, i));
      }
   }


   bool has_inactive_equ = false;
   for (size_t i = 0; i < total_m; ++i) {
      if (!model->equs[i]) {
         if (!has_inactive_equ) {
            errormsg("\nList of inactive equations:\n");
            has_inactive_equ = true;
         }

         error("[%5zu] %s\n", i, ctr_printequname(ctr, i));
      }
   }

   if (!has_inactive_equ) {
      errormsg("\nNo inactive equation\n");
   }
}

int rmdl_print(Model *mdl)
{
   struct ctrdata_rhp *model = (struct ctrdata_rhp *)mdl->ctr.data;
   for (size_t i = 0; i < model->total_n; ++i)
   {
      struct ctr_mat_elt* e = model->vars[i];
      printout(PO_INFO, "showing variable %s: ", ctr_printvarname(&mdl->ctr, i));
      while (e)
      {
         printout(PO_INFO, "%s ", ctr_printequname(&mdl->ctr, e->ei));
         e = e->next_equ;
      }
      printout(PO_INFO, "\n");
   }

   for (size_t i = 0; i < model->total_m; ++i)
   {
      struct ctr_mat_elt* e = model->equs[i];
      printout(PO_INFO, "showing equation %s: ", ctr_printequname(&mdl->ctr, i));
      while (e)
      {
         printout(PO_INFO, "%s ", ctr_printvarname(&mdl->ctr, e->vi));
         e = e->next_var;
      }
      printout(PO_INFO, "\n");
   }

   return OK;
}


