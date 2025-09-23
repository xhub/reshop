#ifndef CONTAINER_MATRIX_H
#define CONTAINER_MATRIX_H

#include <assert.h>
#include <stdbool.h>

#include "allocators.h"
#include "compat.h"
#include "rhp_fwd.h"

/** @file cmat.h 
 *
 *  @brief matrix representation of the (algebraic) container content
 */

/** Type of container matrix element */
__extension__ typedef enum ctr_mat_elt_type ENUM_U8 {
   CMatEltLin      = 0,  /**< The variable appears linearly in the equation            */
   CMatEltQuad     = 1,  /**< The variable appears quadratically in the equation       */
   CMatEltNL       = 2,  /**< The variable appears non-linearly in the equation        */
   CMatEltCstEqu   = 3,  /**< The equation is constant                                 */
   CMatEltVarPerp  = 4,  /**< The variable is perpendicular to an equation, but not 
                              present in any valid equation                            */
   CMatEltObjVar   = 5,  /**< The variable is tagged as objective, but not present in
                              any equation (useful for feasibility problem)            */
} CMatEltType;

/** Element of the matrix representation */
typedef struct ctr_mat_elt {
   double value;                /**< value of the jacobian. When the variable is
                                     appears linearly, this value is used as the
                                     coefficient in GAMS                      */
   CMatEltType type;            /**< Type of the container matrix element     */
   struct ctr_mat_elt *next_var;  /**< next variable in equation                */
   struct ctr_mat_elt *next_equ;  /**< next equation for the variable           */
   struct ctr_mat_elt *prev_equ;  /**< previous equation for the variable       */
   rhp_idx ei;                    /**< index of the equation                    */
   rhp_idx vi;                    /**< index of the variable                    */
} CMatElt;

/** Container matrix to store the information equation (row) and variable (col)
 * information */
typedef struct ctr_mat {
   M_ArenaLink arena;      /**< Arena for memory allocation          */
   CMatElt **equs;         /**< list of equations                    */
   CMatElt **vars;         /**< list of variables                    */
   CMatElt **last_equ;     /**< pointer to the last equation where a variable appears  */
   CMatElt **deleted_equs; /**< list of deleted equations            */
} CMat;

int cmat_init(CMat *cmat) NONNULL;
void cmat_fini(CMat *cmat) NONNULL;
int cmat_cst_equ(CMat *cmat, rhp_idx ei) NONNULL;
int cmat_isolated_var_perp_equ(CMat *cmat, rhp_idx vi, rhp_idx ei) NONNULL;
int cmat_objvar(CMat *cmat, rhp_idx objvar) NONNULL;
NONNULL void cmat_elt_print(unsigned mode, const CMatElt *cme, Container *ctr) NONNULL;

int cmat_fill_equ(Container *ctr, rhp_idx ei, const Avar *v,
                  const double *values, const bool* nlflags) NONNULL;

int cmat_add_lvar_equ(Container *ctr, rhp_idx ei, rhp_idx vi, double coeff ) NONNULL;
int cmat_equ_add_newlvars(Container *ctr, rhp_idx ei, const Avar *v, const double *vals ) NONNULL;

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
static inline bool cmat_chk_varnotinequ(const CMatElt *cme, rhp_idx ei)
{
   while (cme) {
      if (cme->ei == ei) { assert(cme->ei != ei); return false; }
      cme = cme->prev_equ;
   }

   return true;
}

/* ----------------------------------------------------------------------
 * The goal of this check is to ensure that a variable was not already
 * present in a given equation. If this is the case, there is a bug in
 * the code.
 * ---------------------------------------------------------------------- */
static inline bool cmat_chk_varinequ(const CMatElt *cme, rhp_idx ei)
{
   while (cme) {
      if (cme->ei == ei) { return true; }

      cme = cme->prev_equ;
   }

   return false;
}

/**
 * @brief Return true if the container matrix element is a placeholder
 *
 * @param cme  the container matrix element
 *
 * @return     the error code
 */
static inline bool cme_isplaceholder(const CMatElt *cme)
{
   CMatEltType type = cme->type;
   return type >= CMatEltCstEqu;
}

/**
 * @brief Return true if the container matrix element is non-linear
 *
 * @param cme  the container matrix element
 *
 * @return     the error code
 */
static inline bool cme_isNL(const CMatElt *cme)
{
   return cme->type == CMatEltNL;
}


#endif /* CONTAINER_MATRIX_H  */
