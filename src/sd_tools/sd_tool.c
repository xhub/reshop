#include <assert.h>
#include <float.h>
#include <math.h>

#include "sd_tool.h"
#include "container.h"
#include "opcode_diff_ops.h"
#include "lequ.h"
#include "macros.h"
#include "mdl.h"
#include "ctrdat_rhp.h"
#include "printout.h"
#include "status.h"



struct sd_tool* sd_tool_alloc(enum sd_tool_type adt_type, Container *ctr,
                              int eidx)
{
   struct sd_tool *adt;

   MALLOC_NULL(adt, struct sd_tool, 1);

   switch (adt_type) {
   case SDT_ANY:
   case SDT_OPCODE:
      adt->ops = &opcode_diff_ops;
      break;
   case SDT_CASADI:
   case SDT_CPPAD:
      errormsg("Using CaSaDi or CppAD for AD is not yet implemented");
      goto _exit;
   default:
      error("%s :: invalid value %d for ad_tool_type", __func__,
                          adt_type);
      goto _exit;
   }

   int rc = adt->ops->allocdata(adt, ctr, eidx);
   if (rc != OK) {
      error("%s :: call to allocdata for adt_type = %d failed with"
                         "error code %s (%d)", __func__, adt_type,
                         rhp_status_descr(rc), rc);
      goto _exit;
   }

   adt->lequ = ctr->equs[eidx].lequ;

   return adt;

_exit:
   FREE(adt);
   return NULL;
}

struct sd_tool* sd_tool_alloc_fromequ(enum sd_tool_type adt_type, Container *ctr,
                                      const Equ *e)
{
   struct sd_tool *adt;

   MALLOC_NULL(adt, struct sd_tool, 1);

   switch (adt_type) {
   case SDT_ANY:
   case SDT_OPCODE:
      adt->ops = &opcode_diff_ops;
      break;
   case SDT_CASADI:
   case SDT_CPPAD:
      errormsg("Using CaSaDi or CppAD for AD is not yet implemented");
      goto _exit;
   default:
      error("%s :: invalid value %d for ad_tool_type", __func__,
                          adt_type);
      goto _exit;
   }

   int rc = adt->ops->allocdata_fromequ(adt, ctr, e);
   if (rc != OK) {
      error("%s :: call to allocdata for adt_type = %d failed with"
                         "error code %s (%d)", __func__, adt_type,
                         rhp_status_descr(rc), rc);
      goto _exit;
   }

   adt->lequ = e->lequ;

   return adt;

_exit:
   FREE(adt);
   return NULL;
}

void sd_tool_free(struct sd_tool *adt)
{
   if (adt && adt->ops) {
      adt->ops->deallocdata(adt);
      free(adt);
   }
}

/**
 *  @brief assemble the Lagrangian function(s) corresponding to the
 *  mathematical programm
 *
 *  @param adt      the AD object
 *  @param ctr      the container object
 *  @param empinfo  the EMP information
 *
 *  @return         the error code
 */
int sd_tool_assemble(struct sd_tool *adt, Container *ctr,
                     EmpInfo *empinfo)
{
   //return ad_tool->ops->assemble(ad_tool, ctr, empinfo);
   return Error_NotImplemented;
}

/**
 *  @brief compute the derivative with respect to one variable
 *
 *  @param adt   the AD object
 *  @param vidx  the variable for the differentiation
 *  @param e     resulting mapping
 *
 *  @return      the error code
 */
int sd_tool_deriv(struct sd_tool *adt, int vidx, Equ *e)
{
   double val;
   unsigned dummyuint;

   /*  TODO(xhub) support QUAD case */
   /* ----------------------------------------------------------------------
    * If the variable is linear, then the derivative is just a constant
    * ---------------------------------------------------------------------- */

   if (adt->lequ) {
      S_CHECK(lequ_find(adt->lequ, vidx, &val, &dummyuint));

      if (isfinite(val)) {
         /*  TODO(xhub) URG: CHECK SIGN! */
         equ_set_cst(e, val);
         return OK;
      }
   }

   /* ----------------------------------------------------------------------
    * The variable is nonlinear, we call the NL codepath
    * ---------------------------------------------------------------------- */

   return adt->ops->deriv(adt, vidx, e);

}
