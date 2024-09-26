#ifndef CONTAINER_MATRIX_H
#define CONTAINER_MATRIX_H

#include <assert.h>
#include <stdbool.h>

#include "compat.h"
#include "rhp_fwd.h"

/** @file cmat.h 
 *
 *  @brief matrix representation of the (algebraic) container content
 */

/**
 * Element of the matrix representation
 */
typedef struct ctr_mat_elt {
   double value;                /**< value of the jacobian. When the variable is
                                     appears linearly, this value is used as the
                                     coefficient in GAMS                      */
   bool isNL;                   /**< true if the variable appears in a nonlinear
                                     (not quadratic) fashion in this equation */
   bool isQuad;                 /**< flag if the the variable appears in a
                                     quadratic fashion in this equation (TODO)*/
   bool placeholder;            /**< True if this object is a placeholder to
                                     keep the element active in the model     */
   struct ctr_mat_elt *next_var;  /**< next variable in equation                */
   struct ctr_mat_elt *next_equ;  /**< next equation for the variable           */
   struct ctr_mat_elt *prev_equ;  /**< previous equation for the variable       */
   rhp_idx ei;                    /**< index of the equation                    */
   rhp_idx vi;                    /**< index of the variable                    */
} CMatElt;

CMatElt * cmat_cst_equ(rhp_idx ei);
CMatElt * cmat_isolated_var_perp_equ(rhp_idx vi, rhp_idx ei);
CMatElt * cmat_objvar(rhp_idx objvar);
NONNULL void cmat_elt_print(unsigned mode, CMatElt *ce, Container *ctr);

int cmat_fill_equ(Container *ctr, rhp_idx ei, const Avar *v,
                  const double *values, const bool* nlflags) NONNULL;

int cmat_add_lvar_equ(Container *ctr, rhp_idx ei, rhp_idx vi, double coeff ) NONNULL;
int cmat_equ_add_newlvars(Container *ctr, int ei, const Avar *v, const double *vals ) NONNULL;

int cmat_equ_add_nlvar(Container *ctr, rhp_idx ei, rhp_idx vi, double jac_val) NONNULL;
int cmat_equ_add_lvar(Container *ctr, rhp_idx ei, rhp_idx vi, double val, bool *isNL) ACCESS_ATTR(read_write, 5) NONNULL;
int cmat_equ_add_vars_excpt(Container *ctr, rhp_idx ei, Avar *v,
                               rhp_idx vi_no, double* values, bool isNL) NONNULL_AT(1, 3);
int cmat_equ_add_vars(Container *ctr, rhp_idx ei, Avar *v,
                              double* values, bool isNL ) NONNULL_AT(1, 3);

NONNULL
int cmat_equ_add_newlvar(Container *ctr, rhp_idx ei, rhp_idx vi, double val);

int cmat_cpy_equ_flipped(Container *ctr, rhp_idx ei_src, rhp_idx ei_dst) NONNULL;
NONNULL int cmat_copy_equ(Container *ctr, rhp_idx ei_src, rhp_idx ei_dst);
NONNULL
int cmat_copy_equ_except(Container *ctr, rhp_idx ei_src, rhp_idx ei_dst, rhp_idx vi_no);
int cmat_append_equs(Container * restrict ctr_dst,
                     const Container * restrict ctr_src,
                     const Aequ * restrict e,
                     rhp_idx ei_dst_start) NONNULL;

int cmat_scal(Container *ctr, rhp_idx ei, double coeff) NONNULL;
int cmat_equ_rm_var(Container *ctr, rhp_idx ei, rhp_idx vi ) NONNULL;

int cmat_sync_lequ(Container *ctr, Equ * e ) NONNULL;
int cmat_rm_equ(Container *ctr, rhp_idx ei) NONNULL;

int cmat_chk_expensive(Container *ctr) NONNULL;


/* ----------------------------------------------------------------------
 * The goal of this check is to ensure that a variable was not already
 * present in a given equation. If this is the case, there is a bug in
 * the code.
 * ---------------------------------------------------------------------- */
static inline bool cmat_chk_varnotinequ(CMatElt *me, rhp_idx ei)
{
   while (me) {
      if (me->ei == ei) { assert(me->ei != ei); return false; }
      me = me->prev_equ;
   }

   return true;
}

/* ----------------------------------------------------------------------
 * The goal of this check is to ensure that a variable was not already
 * present in a given equation. If this is the case, there is a bug in
 * the code.
 * ---------------------------------------------------------------------- */
static inline bool cmat_chk_varinequ(CMatElt *me, rhp_idx ei)
{
   while (me) {
      if (me->ei == ei) { return true; }
      me = me->prev_equ;
   }

   return false;
}
#endif /* CONTAINER_MATRIX_H  */
