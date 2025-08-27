#include "checks.h"
#include "rhp_LA.h"
#include "reshop.h"

/** @file api_LA.c */

/** @copydoc rhpmat_free
 *  @ingroup publicAPI */
void rhp_mat_free(SpMat *m)
{
   rhpmat_free(m);
}

/** @copydoc rhpmat_triplet
 *  @ingroup publicAPI */
SpMat* rhp_mat_triplet(unsigned n, unsigned m, unsigned nnz, int *rowidx, int *colidx, double *data)
{
   SN_CHECK(chk_arg_nonnull(rowidx, 4, __func__));
   SN_CHECK(chk_arg_nonnull(colidx, 5, __func__));
   SN_CHECK(chk_arg_nonnull(data, 6, __func__));

   return rhpmat_triplet(n, m, nnz, rowidx, colidx, data);
}
