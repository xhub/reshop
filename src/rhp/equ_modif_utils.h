#ifndef EQU_MODIF_UTILS_H
#define EQU_MODIF_UTILS_H

#include <stddef.h>

#include "rhp_fwd.h"

#include "rhp_LA.h"

/** @file equ_modif_utils.h
 *
 * @brief Equation modifications
 */

int rctr_nltree_add_quad_coo_abs(Container *ctr, Equ * e, NlNode **addr,
                         size_t nnz, RHP_INT *i, RHP_INT *j, double *x, double coeff) NONNULL;

int rctr_nltree_add_quad_coo_rel(Container *ctr, Equ * e, NlNode **addr,
                         Avar * restrict v_row, Avar * restrict v_col,
                         size_t nnz, unsigned * restrict i, unsigned * restrict j,
                         double * restrict x, double coeff) NONNULL;
#endif
