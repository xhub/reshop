#include "rctr_equ_edit_priv.h"
#include "cmat.h"
#include "container.h"
#include "lequ.h"
#include "macros.h"
#include "nltree.h"
#include "printout.h"

int rctr_equ_scal(Container *ctr, Equ *e, double coeff)
{
   assert(coeff);

   lequ_scal(e->lequ, coeff);

   if (e->tree && e->tree->root) {
      S_CHECK(nltree_scal(ctr, e->tree, coeff));
   }
   S_CHECK(cmat_scal(ctr, e->idx, coeff));

   double cst = equ_get_cst(e);
   equ_set_cst(e, cst*coeff);

   return OK;
}




