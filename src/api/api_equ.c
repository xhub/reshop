#include "checks.h"
#include "container.h"
#include "ctr_rhp.h"
#include "equ_modif.h"
#include "equvar_helpers.h"
#include "macros.h"
#include "mdl.h"
#include "reshop.h"

Aequ* rhp_aequ_new(void)
{
   return aequ_new();
}

Aequ* rhp_aequ_newcompact(unsigned size, rhp_idx start)
{
   return aequ_newcompact(size, start);
}

Aequ* rhp_aequ_newlist(unsigned size, rhp_idx *eis)
{
   SN_CHECK(chk_arg_nonnull(eis, 2, __func__));

   return aequ_newlistborrow(size, eis);
}

Aequ* rhp_aequ_newlistcopy(unsigned size, rhp_idx *eis)
{
   SN_CHECK(chk_arg_nonnull(eis, 2, __func__));

   return aequ_newlistcopy(size, eis);
}

int rhp_aequ_get(const Aequ *e, unsigned i, rhp_idx *ei)
{
   S_CHECK(chk_arg_nonnull(e, 1, __func__));
   S_CHECK(chk_arg_nonnull(ei, 3, __func__));

   return aequ_get(e, i, ei);
}

void rhp_aequ_free(Aequ* e)
{
   aequ_free(e);
}

unsigned rhp_aequ_size(const Aequ *e)
{
   S_CHECK(chk_arg_nonnull(e, 1, __func__));
   return aequ_size(e);
}

const char* rhp_aequ_gettypename(const struct rhp_aequ *e)
{
   SN_CHECK(chk_arg_nonnull(e, 1, __func__));
   return aequvar_typestr(e->type);
}

/**
 * @brief Check if an abstract equation contains a given index
 *
 * @param  e   the equation container
 * @param  ei  the equation index
 *
 * @return     0 if it is not contained, positive number if it is, a negative number for an error
 */
char rhp_aequ_contains(const Aequ *e, rhp_idx ei)
{
  if (chk_aequ(e, __func__) != OK) { return -1; }
  if (!valid_ei(ei)) { return -2; }

  return aequ_contains(e, ei) ? 1 : 0;
}

/** @brief add \f$\alpha <c,x>\f$ to the nonlinear part of the equation
 *
 *  @param mdl    the model
 *  @param ei     the equation to modify
 *  @param vals   the values of c
 *  @param v      the variable x
 *  @param coeff  the coefficient \f$alpha\f$
 *
 *  @return       the error code
 */
int rhp_nltree_addlin(Model *mdl, rhp_idx ei, double* vals,
      Avar *v, double coeff)
{
   S_CHECK(rhp_chk_ctr(&mdl->ctr, __func__));
   S_CHECK(ei_inbounds(ei, rctr_totalm(&mdl->ctr), __func__));
   S_CHECK(chk_arg_nonnull(vals, 3, __func__));
   S_CHECK(chk_arg_nonnull(v, 4, __func__));

   return nltree_addlvars(&mdl->ctr, ei, vals, v, coeff);
}

/** @brief Add a quadratic term \f$ .5 x^TMx\f$ to an equation.
 *
 *  @warning This function does not check if some of the variables were already
 *  present in the linear part of the equation. This has to be fixed --xhub
 *
 *  @param mdl    the model
 *  @param ei     the equation to modify
 *  @param mat    the matrix
 *  @param v      the variable 
 *  @param coeff  the coefficient multiplying the quadratic. Note that is
 *                coefficient is multiplied by .5
 *
 *  @return       the error code
 */
int rhp_nltree_addquad(Model *mdl, rhp_idx ei, SpMat* mat, Avar *v, double coeff)
{
   S_CHECK(chk_mdl(mdl, __func__));
   Container *ctr = &mdl->ctr;
   S_CHECK(rhp_chk_ctr(ctr, __func__));
   S_CHECK(ei_inbounds(ei, rctr_totalm(ctr), __func__));
   S_CHECK(chk_arg_nonnull(mat, 3, __func__));
   S_CHECK(chk_arg_nonnull(v, 4, __func__));

   Equ *e = &ctr->equs[ei];

   return rctr_equ_add_quadratic(ctr, e, mat, v, coeff);
}
