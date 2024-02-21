#ifndef SOLVER_EVAL_H
#define SOLVER_EVAL_H

#include "rhp_fwd.h"
#include "rhp_LA.h"

/** @file solver_eval.h
 *
 *  @brief solver evaluations related
 */

/*
 *  The jacobian has this structure:
 *
 *  | ∇ₓf   ∇ₓg^NL  -A^T |
 *  |                     |
 *  | g^NL                |
 *  |                     |
 *  | A                   |
 */

/**
 * @brief jacobian evaluation struct
 */
struct jacdata {
   RHP_INT n;              /**< size of the matrix */
   RHP_INT n_primal;       /**< number of primal variable */
   RHP_INT n_nl;           /**< Number of NL constraints */
   RHP_INT nnz;
   RHP_INT nnzmax;
   RHP_INT *i;             /**< row indices */
   RHP_INT *p;             /**< pointers */
   double *x;              /**< data */
   Equ *equs;       /**< array of equations to evaluate the jacobian matrix */
//   RHP_INT *last_NL;       /**< Index of the last non-constant element in a primal variable column */
//   struct sp_matrix *A;    /**< constant constraint matrix A*/
//   struct sp_matrix *nAt;  /**< -A^T */
//   RHP_INT *diag_idx;      /**< diagonal indices in the jacobian matrix */
};


int ge_eval_func(Container *ctr, double * restrict x, double * restrict F) NONNULL;
int ge_eval_jacobian(Container *ctr, struct jacdata *jacdata, double * restrict x, double * restrict F, int * restrict p, int * restrict i, double * restrict vals) NONNULL;
int ge_prep_jacdata(Container *ctr, struct jacdata *jacdata) NONNULL;

void jacdata_free(struct jacdata *jacdata) NONNULL;

#endif /* SOLVER_EVAL_H */
