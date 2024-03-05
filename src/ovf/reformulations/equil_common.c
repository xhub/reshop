#include "asprintf.h"

#include "equil_common.h"
#include "cmat.h"
#include "cones.h"
#include "ctr_rhp.h"
#include "ctrdat_rhp.h"
#include "lequ.h"
#include "macros.h"
#include "mathprgm.h"
#include "mdl.h"
#include "ovf_common.h"
#include "printout.h"
#include "rhp_LA.h"
#include "rhp_fwd.h"
#include "var.h"


/**
 * @brief Add the polyhedral constraints \f$ Ay + s \in K\f$ of an OVF to an MP
 *
 * This forms part of the set Y of the OVF
 *
 * @param  mdl     the model
 * @param  ovfd    the abstract OVF data
 * @param  y       the variable of the OVF
 * @param  op      the operations for the OVF
 * @param  A       the matrix A
 * @param  s       the s vector
 * @param  mp_ovf  the MP for the OVF/CCFLIB
 * @param  prefix  the prefix for the constraint name
 *
 * @return        the error code
 */
int ovf_add_polycons(Model *mdl, union ovf_ops_data ovfd, Avar *y,
                     const struct ovf_ops *op, SpMat *A, double *s,
                     MathPrgm *mp_ovf, const char *prefix)
{
   assert(mdl_is_rhp(mdl));
   unsigned n, m;
   S_CHECK(rhpmat_get_size(A, &n, &m));

   Container *ctr = &mdl->ctr;
   RhpContainerData *cdat = (RhpContainerData *)ctr->data;

   char *equname;
   /* TODO NAMING */
   IO_CALL(asprintf(&equname, "%s_setY_%u", prefix, mp_ovf->id));
   S_CHECK(cdat_equname_start(cdat, equname));


   for (unsigned i = 0; i < m; ++i) {
      unsigned *row_idx, single_row_idx, row_len;
      double *vals = NULL, single_val;

      S_CHECK(rhpmat_row(A, i, &single_row_idx, &single_val, &row_len,
                         &row_idx, &vals));

      if (row_len == 0) {
         printout(PO_INFO, "[Warn] %s :: row %u is empty\n", __func__, i);
         continue;
      } 
      if (row_len == 1) {
         printout(PO_DEBUG, "[Warn] %s :: row %d has only one element\n",
                  __func__, i);
      }

      enum cone cone;
      void *cone_data;
      S_CHECK(op->get_cone_nonbox(ovfd, i, &cone, &cone_data));

      if (!cone_polyhedral(cone)) {
         errormsg("[reformulation:equilibrium] ERROR: non-polyhedral set is not "
                  "yet supported\n");
         return Error_NotImplemented;
      }

      rhp_idx ei_new;
      Equ *e;
      S_CHECK(rctr_add_equ_empty(ctr, &ei_new, &e, ConeInclusion, cone));
      S_CHECK(lequ_reserve(e->lequ, row_len));

      Avar subset_var;
      S_CHECK(avar_subset(y, (rhp_idx*)row_idx, row_len, &subset_var));

      S_CHECK(lequ_adds(e->lequ, &subset_var, vals));
      equ_set_cst(e, -s[i]);

      S_CHECK(cmat_sync_lequ(ctr, e));
      S_CHECK(mp_addconstraint(mp_ovf, ei_new));

      /*  TODO(xhub) improve with a buffer */
      avar_empty(&subset_var);
   }

   S_CHECK(cdat_equname_end(cdat));

   return OK;
}


