#ifndef EQU_MODIF_H
#define EQU_MODIF_H

#include "equ.h"
#include "lequ.h"
#include "macros.h"
#include "rhp_fwd.h"
#include "var.h"

//int equ_add_inner_prod(Container *ctr, Equ *e, double v,
//                       Equ *ein);
int rctr_equ_add_dot_prod_cst(Container *ctr, Equ *e, const double *c, unsigned n_c,
                              SpMat *B, double *b, double *coeffs, Avar *v,
                              rhp_idx *eis, double coeff);
int equ_add_dot_prod_cst_x(Container *ctr, NlTree *tree, NlNode *node, unsigned offset,
                           const double *c, unsigned n_c, SpMat *B, double *b,
                           double *coeffs, rhp_idx *var_idx, rhp_idx *eis);
int rctr_nltree_cpy_dot_prod_var_map(Container *ctr, NlTree *tree, NlNode *node, 
                                Avar *uvar, const SpMat *B, const double *b, double *coeffs, Avar *v,
                                Aequ* eqn);
int rctr_equ_add_quadratic(Container *ctr, Equ *e, SpMat *mat, Avar *v, double coeff);
int rctr_equ_add_maps(Container *ctr, Equ *e, double *coeffs,
                      unsigned args_indx_len, rhp_idx *arg_idx, rhp_idx *equ_idx,
                      Avar *v, double *lcoeffs, double cst);
int add_dot_prod_term_cst(Container *ctr, Equ *e,
                          NlNode **node, double cst, unsigned args_idx_len,
                          int *args_idx, double *coeffs, double *lcoeffs, int *var_idx,
                          int *equ_idx, double r_cst);

int equ_rm_var(Container *ctr, Equ *e, rhp_idx vi);
int equ_switch_var_nl(Container *ctr, Equ *e, rhp_idx vi);


int nltree_addlvars(Container *ctr, rhp_idx ei, double* vals, Avar *v, double coeff) NONNULL;

/**
 *  @brief Set an equation to sum_i a_i v_i
 *
 *  @warning this function does not perform any check. If any v_i was already
 *           present in the equation, this results in an inconsistent equation
 *           Use rctr_equ_addlinvars() in that case
 *
 *  @param e      the equation to which the variable is added
 *  @param v      the variable to add
 *  @param vals   the value for each scalar variable
 *
 *  @return       The error code
 */
static inline NONNULL int equ_add_newlvars(Equ *e, Avar *v, const double* vals)
{
   if (!e->lequ) {
      A_CHECK(e->lequ, lequ_new(v->size));
   }
   Lequ *lequ = e->lequ;

   return lequ_adds(lequ, v, vals);
}

/**
 *  @brief add a new variable to an equation (expert version)
 *
 *  @warning the variable should not already be in the equation!
 *  Use rctr_equ_addvar() in that case.
 *
 *  @param e     the equation
 *  @param vi    the variable index
 *  @param val   the coefficient for the variable
 *
 *  @return      the error code
 */
static inline NONNULL int equ_add_newlvar(Equ *e, rhp_idx vi, double val)
{
   assert(isfinite(val));
   if (!e->lequ) {
      A_CHECK(e->lequ, lequ_new(1));
   }

   S_CHECK(lequ_add(e->lequ, vi, val));

   return OK;
}

#endif /* EQU_MODIF_H */
