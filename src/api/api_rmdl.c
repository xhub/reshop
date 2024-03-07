#include "reshop_config.h"

#include <math.h>
#include <string.h>

#include "checks.h"
#include "compat.h"
#include "container.h"
#include "consts.h"
#include "ctr_rhp.h"
#include "equ.h"
#include "equ_modif.h"
#include "equ_modif_utils.h"
#include "nltree.h"
#include "equvar_helpers.h"
#include "macros.h"
#include "mdl.h"
#include "mdl_rhp.h"
#include "ctrdat_rhp.h"
#include "ctr_rhp_add_vars.h"
#include "rmdl_options.h"
#include "printout.h"
#include "reshop.h"
#include "var.h"

#define TOL_BOUNDS_FIXED_VAR              100*DBL_EPSILON

static int chk_ei_ptr(const rhp_idx *ei, const char* fn)
{
   if (!ei) {
      error("%s ERROR: pointer to ei is NULL!", fn);
      return Error_NullPointer;
   }

   return OK;
}

/** @file rmdl_api.c */

/**
 * @brief Add a quadratic equation to the model
 *
 * The matrix is in COO/triplet format
 *
 * @warning the matrix indices are variable indices.
 * For matrix indices relative to a variable, see rhp_add_equ_quad()
 *
 * @ingroup publicAPI
 *
 * @param mdl    the container
 * @param ei     the equation index
 * @param nnz    the number of quadratic terms
 * @param i      the row indices
 * @param j      the column indices
 * @param x      the quadratic coefficients
 * @param coeff  the general coefficient
 *
 * @return       the error code
 */
int rhp_equ_addquadabsolute(Model *mdl, rhp_idx ei, size_t nnz, unsigned *i,
                            unsigned *j, double *x, double coeff)
{
   S_CHECK(chk_rmdl(mdl, __func__));
   S_CHECK(ei_inbounds(ei, ctr_nequs(&mdl->ctr), __func__));
   S_CHECK(chk_arg_nonnull(i, 4, __func__));
   S_CHECK(chk_arg_nonnull(j, 5, __func__));
   S_CHECK(chk_arg_nonnull(x, 6, __func__));

   if (nnz == 0) {
      error("%s ERROR: Number of nnz is 0, empty sparse matrix given\n", __func__);
      return Error_RuntimeError;
   }

   Equ *e = &mdl->ctr.equs[ei];

   /* TODO(Xhub) is this a hack? */
   if (!e->tree) {
      /* TODO(xhub) Tune that */
      S_CHECK(nltree_bootstrap(e, 3*nnz, 3*nnz));
      e->is_quad = true;
   }

   NlNode *node = NULL;
   NlNode **addr = &e->tree->root;

   /* Prep the tree and get the address of the node in addr  */
   double lcoeff = coeff;
   S_CHECK(nltree_find_add_node(e->tree, &addr, mdl->ctr.pool, &lcoeff));

   NlNode *check_node = *addr;

   S_CHECK(rctr_nltree_add_quad_coo_abs(&mdl->ctr, e, addr, nnz, i, j, x, lcoeff));

   S_CHECK(nltree_check_add(check_node));

   return OK;
}

/**
 * @brief Add a quadratic equation to the model
 *
 * The matrix is in COO/triplet format
 *
 * @warning the matrix indices are relative to the variables.
 * For matrix indices given in absolute indices, see rhp_add_equ_quad_absolute()
 *
 * @ingroup publicAPI
 *
 * @param mdl    the container
 * @param ei     the equation index
 * @param v_row  the abstract variable for the row indices
 * @param v_col  the abstract variable for the col indices
 * @param nnz    the number of quadratic terms
 * @param i      the row indices
 * @param j      the column indices
 * @param x      the quadratic coefficients
 * @param coeff  the general coefficient
 *
 * @return       the error code
 */
int rhp_equ_addquadrelative(Model *mdl, rhp_idx ei, Avar *v_row,
                            Avar *v_col, size_t nnz, unsigned *i, unsigned *j,
                            double *x, double coeff)
{
   S_CHECK(chk_rmdl(mdl, __func__));
   S_CHECK(ei_inbounds(ei, ctr_nequs(&mdl->ctr), __func__));
   S_CHECK(chk_arg_nonnull(v_row, 3, __func__));
   S_CHECK(chk_arg_nonnull(v_col, 4, __func__));
   S_CHECK(chk_arg_nonnull(i, 6, __func__));
   S_CHECK(chk_arg_nonnull(j, 7, __func__));
   S_CHECK(chk_arg_nonnull(x, 8, __func__));

   if (nnz == 0) {
      error("%s ERROR: Number of nnz is 0, empty sparse matrix given\n", __func__);
      return Error_RuntimeError;
   }

   Equ *e = &mdl->ctr.equs[ei];

   /* TODO(Xhub) is this a hack? */
   if (!e->tree) {
      /* TODO(xhub) Tune that */
      S_CHECK(nltree_bootstrap(e, 3*nnz, 3*nnz));
      e->is_quad = true;
   }

   NlNode *node = NULL;
   NlNode **addr = &e->tree->root;

   /* Prep the tree and get the address of the node in addr  */
   double lcoeff = coeff;
   S_CHECK(nltree_find_add_node(e->tree, &addr, mdl->ctr.pool, &lcoeff));

   NlNode *check_node = *addr;

   S_CHECK(rctr_nltree_add_quad_coo_rel(&mdl->ctr, e, addr, v_row, v_col, nnz, i, j, x, lcoeff));

   S_CHECK(nltree_check_add(check_node));

   return OK;
}

/**
 * @brief Add a bilinear term to an equation
 *
 * Add \f$ c \langle v1, v2 \rangle \f$
 *
 * @ingroup publicAPI
 *
 * @param mdl    the container
 * @param eidx   the equation index
 * @param v1     the first variable
 * @param v2     the second variable
 * @param coeff  the general coefficient
 *
 * @return       the error code
 */
int rhp_equ_addbilin(Model *mdl, rhp_idx ei, Avar *v1, Avar *v2, double coeff)
{
   S_CHECK(chk_rmdl(mdl, __func__));
   S_CHECK(ei_inbounds(ei, ctr_nequs(&mdl->ctr), __func__));
   S_CHECK(chk_arg_nonnull(v1, 3, __func__));
   S_CHECK(chk_arg_nonnull(v2, 4, __func__));

   Equ *e = &mdl->ctr.equs[ei];

   unsigned size = avar_size(v1), size2 = avar_size(v2);
   if (size != size2) {
      error("%s ERROR: the two variables have different sizes: %u vs %u. They "
            "must be identical!\n", __func__, size, size2);
      return Error_InvalidArgument;
   }

   if (!e->tree) {
      S_CHECK(nltree_bootstrap(e, 3*size, 3*size));
      e->is_quad = true;
   }

   NlNode *node = NULL;
   NlNode **addr = &e->tree->root;

   /* Prep the tree and get the address of the node in addr  */
   double lcoeff = coeff;
   S_CHECK(nltree_find_add_node(e->tree, &addr, mdl->ctr.pool, &lcoeff));

   NlNode *check_node = *addr;

   /* ----------------------------------------------------------------------
    * First multiply by the constant
    * ---------------------------------------------------------------------- */

   unsigned child_idx;
   S_CHECK(nlnode_findfreechild(e->tree, *addr, 1, &child_idx));
   S_CHECK(nltree_mul_cst_add_node(e->tree, &addr, mdl->ctr.pool, lcoeff, v1->size, &child_idx));

   /* ----------------------------------------------------------------------
    * Add the bilinear part
    * ---------------------------------------------------------------------- */

   N_CHECK(node, *addr);
   S_CHECK(rctr_nltree_add_bilin_vars(&mdl->ctr, e->tree, node, child_idx, v1, v2));

   S_CHECK(nltree_check_add(check_node));

   return OK;
}

/**
 * @brief Add one free variable
 *
 * @ingroup publicAPI
 *
 * @param       mdl   the container
 * @param[out]  vi    the resulting variable index
 *
 * @return     the error code
 */
int rhp_add_var(Model *mdl, rhp_idx *vi)
{
   S_CHECK(chk_rmdl(mdl, __func__));
   S_CHECK(chk_arg_nonnull(vi, 2, __func__));
   S_CHECK(rctr_reserve_vars(&mdl->ctr, 1));

   /*  This should be the best fit */
   Avar vout;
   S_CHECK(rctr_add_free_vars(&mdl->ctr, 1, &vout));

   return avar_get(&vout, 0, vi);
}

/**
 * @brief Add one free variable
 *
 * @ingroup publicAPI
 *
 * @param       mdl   the container
 * @param[out]  vi    the resulting variable index
 *
 * @return     the error code
 */
int rhp_add_varnamed(Model *mdl, rhp_idx *vi, const char *name)
{
   S_CHECK(chk_rmdl(mdl, __func__));
   S_CHECK(chk_arg_nonnull(vi, 2, __func__));
   S_CHECK(rctr_reserve_vars(&mdl->ctr, 1));

   /*  This should be the best fit */

   char *vname;
   A_CHECK(vname, strdup(name));

   Avar vout;
   RhpContainerData *cdat = (RhpContainerData*)mdl->ctr.data;
   S_CHECK(cdat_varname_start(cdat, vname));
   S_CHECK(rctr_add_free_vars(&mdl->ctr, 1, &vout));
   S_CHECK(cdat_varname_end(cdat));

   return avar_get(&vout, 0, vi);
}
/**
 * @brief Add (free) variables and return the variable info
 *
 * @ingroup publicAPI
 *
 * @param       mdl   the container
 * @param       size  the size of the variable 
 * @param[out]  vout  the resulting variables
 *
 * @return     the error code
 */
int rhp_add_vars(Model *mdl, unsigned size, Avar *vout)
{
   S_CHECK(chk_rmdl(mdl, __func__));
   S_CHECK(chk_arg_nonnull(vout, 3, __func__));
   S_CHECK(rctr_reserve_vars(&mdl->ctr, size));

   /*  This should be the best fit */
   S_CHECK(rctr_add_free_vars(&mdl->ctr, size, vout));

    return OK;
}

/**
 * @brief Add positive variables and return the variable info
 *
 * @ingroup publicAPI
 *
 * @param       mdl   the container
 * @param       size  the size of the variable 
 * @param[out]  vout  the resulting variables
 *
 * @return     the error code
 */
int rhp_add_posvars(Model *mdl, unsigned size, Avar *vout)
{
   S_CHECK(chk_rmdl(mdl, __func__));
   S_CHECK(chk_arg_nonnull(vout, 3, __func__));
   S_CHECK(rctr_reserve_vars(&mdl->ctr, size));

   /*  This should be the best fit */
   S_CHECK(rctr_add_pos_vars(&mdl->ctr, size, vout));

    return OK;
}

/**
 * @brief Add negative variables and return the variable info
 *
 * @ingroup publicAPI
 *
 * @param       mdl   the container
 * @param       size  the size of the variable 
 * @param[out]  vout  the resulting variables
 *
 * @return     the error code
 */
int rhp_add_negvars(Model *mdl, unsigned size, Avar *vout)
{
   S_CHECK(chk_rmdl(mdl, __func__));
   S_CHECK(chk_arg_nonnull(vout, 3, __func__));
   S_CHECK(rctr_reserve_vars(&mdl->ctr, size));

   /*  This should be the best fit */
   S_CHECK(rctr_add_neg_vars(&mdl->ctr, size, vout));

    return OK;
}

/**
 * @brief Add a variable with a given name
 *
 * A copy of the string parameter is performed
 *
 * @param       mdl   the model 
 * @param       size  the size of the variable 
 * @param[out]  vout  the variable container
 * @param       name  the name of the variable 

 * @return      the error code
 */
int rhp_add_varsnamed(Model *mdl, unsigned size, Avar *vout, const char* name)
{
   S_CHECK(chk_rmdl(mdl, __func__));
   S_CHECK(chk_arg_nonnull(vout, 3, __func__));
   RhpContainerData *cdat = (RhpContainerData*)mdl->ctr.data;

   char *vname;
   A_CHECK(vname, strdup(name));

   S_CHECK(cdat_varname_start(cdat, vname));
   S_CHECK(rhp_add_vars(mdl, size, vout));
   S_CHECK(cdat_varname_end(cdat));

   return OK;
}

/**
 * @brief Add a positive variable with a given name
 *
 * A copy of the string parameter is performed
 *
 * @param       mdl   the model 
 * @param       size  the size of the variable 
 * @param[out]  vout  the variable container
 * @param       name  the name of the variable 

 * @return      the error code
 */
int rhp_add_posvarsnamed(Model *mdl, unsigned size, Avar *vout, const char* name)
{
   S_CHECK(chk_rmdl(mdl, __func__));
   S_CHECK(chk_arg_nonnull(vout, 3, __func__));
   RhpContainerData *cdat = (RhpContainerData*)mdl->ctr.data;

   char *vname;
   A_CHECK(vname, strdup(name));

   S_CHECK(cdat_varname_start(cdat, vname));
   S_CHECK(rhp_add_posvars(mdl, size, vout));
   S_CHECK(cdat_varname_end(cdat));

   return OK;
}

/**
 * @brief Add a negative variable with a given name
 *
 * A copy of the string parameter is performed
 *
 * @param       mdl   the model 
 * @param       size  the size of the variable 
 * @param[out]  vout  the variable container
 * @param       name  the name of the variable 

 * @return      the error code
 */
int rhp_add_negvarsnamed(Model *mdl, unsigned size, Avar *vout, const char* name)
{
   S_CHECK(chk_rmdl(mdl, __func__));
   S_CHECK(chk_arg_nonnull(vout, 3, __func__));
   RhpContainerData *ctrdat = (RhpContainerData*)mdl->ctr.data;

   char *vname;
   A_CHECK(vname, strdup(name));

   S_CHECK(cdat_varname_start(ctrdat, vname));
   S_CHECK(rhp_add_negvars(mdl, size, vout));
   S_CHECK(cdat_varname_end(ctrdat));

   return OK;
}

/**
 * @brief Add box-constrainted variables 
 *
 * @param       mdl    the model
 * @param       size   the number of variables
 * @param[out]  vout   the 
 * @param       lb     the common lower bound
 * @param       ub     the common upper bound
 *
 * @return             the error code
 */
int rhp_add_varsinbox(Model *mdl, unsigned size, Avar *vout, double lb, double ub)
{
   S_CHECK(chk_rmdl(mdl, __func__));
   S_CHECK(chk_arg_nonnull(vout, 3, __func__));

   return rctr_add_box_vars_id(&mdl->ctr, size, vout, lb, ub);
}

/**
 * @brief Add box-constrainted variables 
 *
 * @ingroup publicAPI
 *
 * @param       mdl    the model
 * @param       size   the number of variables
 * @param[out]  vout   the variable
 * @param       name   the variable name
 * @param       lb     the common lower bound
 * @param       ub     the common upper bound
 *
 * @return             the error code
 */
int rhp_add_varsinboxnamed(Model *mdl, unsigned size, Avar *vout, const char *name, double lb, double ub)
{
   S_CHECK(chk_rmdl(mdl, __func__));
   S_CHECK(chk_arg_nonnull(vout, 3, __func__));
   S_CHECK(chk_arg_nonnull(name, 4, __func__));

   RhpContainerData *ctrdat = (RhpContainerData*)mdl->ctr.data;

   char *vname;
   A_CHECK(vname, strdup(name));

   S_CHECK(cdat_varname_start(ctrdat, vname));
   S_CHECK(rctr_add_box_vars_id(&mdl->ctr, size, vout, lb, ub));
   return cdat_varname_end(ctrdat);
}

/**
 * @brief Add box-constrainted variables 
 *
 * @ingroup publicAPI
 *
 * @param       mdl    the model
 * @param       size   the number of variables
 * @param[out]  vout   the 
 * @param       lb     the optional lower bound; if not set, then -infinity
 * @param       ub     the optional upper bound; if not set, then +infinity
 *
 * @return             the error code
 */
int rhp_add_varsinboxes(Model *mdl, unsigned size, Avar *vout, double *lb, double *ub)
{
   S_CHECK(chk_rmdl(mdl, __func__));
   S_CHECK(chk_arg_nonnull(vout, 3, __func__));

   return rctr_add_box_vars(&mdl->ctr, size, vout, lb, ub);
}

/**
 * @brief Add box-constrainted variables 
 *
 * @ingroup publicAPI
 *
 * @param       mdl    the model
 * @param       size   the number of variables
 * @param[out]  vout   the variable
 * @param       name   the variable name
 * @param       lb     the common lower bound (optional: if not set, then -infinity)
 * @param       ub     the common upper bound (optional: if not set, then +infinity)
 *
 * @return             the error code
 */
int rhp_add_varsinboxesnamed(Model *mdl, unsigned size, Avar *vout, const char *name, double *lb, double *ub)
{
   S_CHECK(chk_rmdl(mdl, __func__));
   S_CHECK(chk_arg_nonnull(vout, 3, __func__));
   S_CHECK(chk_arg_nonnull(name, 4, __func__));

   RhpContainerData *ctrdat = (RhpContainerData*)mdl->ctr.data;

   char *vname;
   A_CHECK(vname, strdup(name));

   S_CHECK(cdat_varname_start(ctrdat, vname));
   S_CHECK(rctr_add_box_vars(&mdl->ctr, size, vout, lb, ub));
   return cdat_varname_end(ctrdat);
}

/**
 * @brief Add a linear expression < a , x > to an equation
 *
 * @warning Here the variables must be new to the equation.
 * If this is unclear, please use rhp_equ_addlinchk()
 *
 * @ingroup publicAPI
 *
 * @param mdl   the model
 * @param ei    the equation index
 * @param v     the abstract variable
 * @param vals  the coefficients of the variables
 *
 * @return      the error code
 */
int rhp_equ_addlin(Model *mdl, rhp_idx ei, Avar *v, const double *vals)
{
   S_CHECK(chk_rmdl(mdl, __func__));

    S_CHECK(ei_inbounds(ei,((RhpContainerData*)mdl->ctr.data)->total_m, __func__)); 

   if (!v) {
      error("%s ERROR: the 3rd argument is NULL!\n", __func__);
      return Error_NullPointer;
   }

   if (!vals) {
      error("%s ERROR: the 4th argument is NULL!\n", __func__);
      return Error_NullPointer;
   }

   return rctr_equ_addnewlvars(&mdl->ctr, &mdl->ctr.equs[ei], v, vals);
}

/**
 * @brief Add a linear expression c < a , x > to an equation
 *
 * @warning Here the variables must be new to the equation.
 * If this is unclear, please use rhp_equ_addlinchk()
 *
 * @ingroup publicAPI
 *
 * @param mdl   the model
 * @param ei    the equation index
 * @param v     the abstract variable
 * @param vals  the coefficients of the variables
 * @param coeff  the coefficient for the expression
 *
 * @return      the error code
 */
int rhp_equ_addlincoeff(Model *mdl, rhp_idx ei, Avar *v, const double *vals, double coeff)
{
   S_CHECK(chk_rmdl(mdl, __func__));

    S_CHECK(ei_inbounds(ei,((RhpContainerData*)mdl->ctr.data)->total_m, __func__)); 

   if (!v) {
      error("%s ERROR: the 3rd argument is NULL!\n", __func__);
      return Error_NullPointer;
   }

   if (!vals) {
      error("%s ERROR: the 4th argument is NULL!\n", __func__);
      return Error_NullPointer;
   }

   return rctr_equ_addnewlin_coeff(&mdl->ctr, &mdl->ctr.equs[ei], v, vals, coeff);
}

/**
 * @brief Add a linear expression < c , x > to an equation
 *
 * @ingroup publicAPI
 *
 * @param mdl   the model
 * @param ei    the equation index
 * @param v     the abstract variable
 * @param vals  the coefficients of the variables
 *
 * @return      the error code
 */
int rhp_equ_addlinchk(Model *mdl, rhp_idx ei, Avar *v,
                      const double *vals)
{
   S_CHECK(chk_rmdl(mdl, __func__));

   S_CHECK(ei_inbounds(ei,((RhpContainerData*)mdl->ctr.data)->total_m, __func__)); 

   if (!v) {
      error("%s ERROR: the 3rd argument is NULL!\n", __func__);
      return Error_NullPointer;
   }

   if (!vals) {
      error("%s ERROR: the 4th argument is NULL!\n", __func__);
      return Error_NullPointer;
   }

   return rctr_equ_addlinvars(&mdl->ctr, &mdl->ctr.equs[ei], v, vals);
}

/**
 *  @brief add a variable to an equation
 *
 *  @ingroup publicAPI
 *
 *  @param  mdl   the container
 *  @param  eidx  the equation index
 *  @param  vidx  the variable index
 *  @param  val   the coefficient for the variable
 *
 *  @return       the error code
 */
int rhp_equ_addlvar(Model *mdl, rhp_idx eidx, rhp_idx vidx, double val)
{
   S_CHECK(chk_rmdl(mdl, __func__));

   S_CHECK(ei_inbounds(eidx,((RhpContainerData*)mdl->ctr.data)->total_m, __func__));
   S_CHECK(vi_inbounds(vidx, ((RhpContainerData*)mdl->ctr.data)->total_n, __func__));

   return rctr_equ_addlvar(&mdl->ctr, &mdl->ctr.equs[eidx], vidx, val);
}

/**
 *  @brief add a variable to an equation
 *
 *  @ingroup publicAPI
 *
 *  @param  mdl   the container
 *  @param  ei    the equation
 *  @param  vi    the variable index
 *  @param  val   the coefficient for the variable
 *
 *  @return       the error code
 */
int rhp_equ_addnewlvar(Model *mdl, rhp_idx ei, rhp_idx vi, double val)
{
   S_CHECK(chk_rmdl(mdl, __func__));

   S_CHECK(ei_inbounds(ei,((RhpContainerData*)mdl->ctr.data)->total_m, __func__));
   S_CHECK(vi_inbounds(vi, ((RhpContainerData*)mdl->ctr.data)->total_n, __func__));

   return rctr_equ_addnewvar(&mdl->ctr, &mdl->ctr.equs[ei], vi, val);
}

/**
 * @brief Set the constant of a mapping
 *
 * @ingroup publicAPI
 *
 * @param mdl  the model
 * @param ei   the equation index
 * @param val  the value
 *
 * @return     the error code
 */
int rhp_equ_setcst(Model *mdl, rhp_idx ei, double val)
{
   S_CHECK(chk_rmdl(mdl, __func__));
   S_CHECK(ei_inbounds(ei,((RhpContainerData*)mdl->ctr.data)->total_m, __func__)); 

   if (mdl->ctr.equs[ei].object != Mapping) {
      error("%s ERROR: equation %s has the wrong type: expecting %s, got %s\n",
            __func__, ctr_printequname(&mdl->ctr, ei), equtype_name(Mapping),
            equtype_name(mdl->ctr.equs[ei].object));
      return Error_UnExpectedData;
   }

   equ_set_cst(&mdl->ctr.equs[ei], val);
   return OK;
}

/**
 * @brief Get the constant of a mapping
 *
 * @ingroup publicAPI
 *
 * @param mdl  the model
 * @param ei   the equation index
 * @param val  the value
 *
 * @return     the error code
 */
int rhp_equ_getcst(Model *mdl, rhp_idx ei, double *val)
{
   S_CHECK(chk_rmdl(mdl, __func__));
   S_CHECK(ei_inbounds(ei,((RhpContainerData*)mdl->ctr.data)->total_m, __func__)); 

   if (mdl->ctr.equs[ei].object != Mapping) {
      error("%s ERROR: equation %s has the wrong type: expecting a %s,"
                         " got a %s\n", __func__, ctr_printequname(&mdl->ctr, ei),
                         equtype_name(Mapping), equtype_name(mdl->ctr.equs[ei].object));
      return Error_UnExpectedData;
   }

   *val = mdl->ctr.equs[ei].p.cst;
   return OK;
}

/**
 * @brief Get the linear part of an equation, if any
 *
 * @param mdl        the container
 * @param ei         the equation index
 * @param[out] len   the length of the arrays
 * @param[out] idxs  the array of variable indices (NULL if no linear part)
 * @param[out] vals  the array of coefficients (NULL if no linear part)
 *
 * @return           the error code
 */
int rhp_equ_getlin(Model *mdl, rhp_idx ei, unsigned *len, rhp_idx **idxs, double **vals)
{
   S_CHECK(chk_rmdl(mdl, __func__));
   S_CHECK(ei_inbounds(ei,((RhpContainerData*)mdl->ctr.data)->total_m, __func__)); 

   if (!idxs) {
      error("%s ERROR: the 3rd argument is NULL!\n", __func__);
      return Error_NullPointer;
   }

   if (!vals) {
      error("%s ERROR: the 4th argument is NULL!\n", __func__);
      return Error_NullPointer;
   }

   Lequ *le = mdl->ctr.equs[ei].lequ;
   if (!le) {
      *len = 0;
      *idxs = NULL;
      *vals = NULL;
   } else {
      *len = le->len;
      *idxs = le->vis;
      *vals = le->coeffs;
   }

   return OK;
}
/**
 * @brief Get the number of "less-than" (or "<=' ) linear equations
 *
 * @param mdl the model
 *
 * @return    the error code
 */
size_t rhp_get_nb_lequ_le(Model *mdl)
{
   if (chk_rmdl(mdl, __func__) != OK) {
      return RHP_INVALID_IDX;
   }

   size_t cnt = 0;
   for (size_t i = 0; i < ctr_nequs(&mdl->ctr); ++i) {
      if (!mdl->ctr.equs[i].tree &&
          mdl->ctr.equs[i].object == ConeInclusion &&
          mdl->ctr.equs[i].cone == CONE_R_MINUS) cnt ++;
   }

   return cnt;
}

size_t rhp_get_nb_lequ_ge(Model *mdl)
{
   if (chk_rmdl(mdl, __func__) != OK) {
      return RHP_INVALID_IDX;
   }

   size_t cnt = 0;
   for (size_t i = 0; i < ctr_nequs(&mdl->ctr); ++i) {
      if (!mdl->ctr.equs[i].tree &&
          mdl->ctr.equs[i].object == ConeInclusion &&
          mdl->ctr.equs[i].cone == CONE_R_PLUS) cnt++;
   }

   return cnt;
}

size_t rhp_get_nb_lequ_eq(Model *mdl)
{
   if (chk_rmdl(mdl, __func__) != OK) {
      return RHP_INVALID_IDX;
   }

   size_t cnt = 0;
   for (size_t i = 0; i < ctr_nequs(&mdl->ctr); ++i) {
      if (!mdl->ctr.equs[i].tree &&
          mdl->ctr.equs[i].object == ConeInclusion &&
          mdl->ctr.equs[i].cone == CONE_0) cnt++;
   }

   return cnt;
}

size_t rhp_get_nb_var_bin(Model *mdl)
{
   if (chk_rmdl(mdl, __func__) != OK) {
      return RHP_INVALID_IDX;
   }

   size_t cnt = 0;
   for (size_t i = 0; i < rctr_totaln(&mdl->ctr); ++i) {
      if (mdl->ctr.vars[i].type == VAR_B) cnt++;
   }
   return cnt;
}

size_t rhp_get_nb_var_int(Model *mdl)
{
   if (chk_rmdl(mdl, __func__) != OK) {
      return RHP_INVALID_IDX;
   }

   size_t cnt = 0;
   for (size_t i = 0; i < rctr_totaln(&mdl->ctr); ++i) {
      if (mdl->ctr.vars[i].type == VAR_I) cnt++;
   }
   return cnt;
}

size_t rhp_get_nb_var_lb(Model *mdl)
{
   if (chk_rmdl(mdl, __func__) != OK) {
      return RHP_INVALID_IDX;
   }

   size_t cnt = 0;
   for (size_t i = 0; i < rctr_totaln(&mdl->ctr); ++i) {
      Var *var = &mdl->ctr.vars[i];
      if (var->type == VAR_X && isfinite(var->bnd.lb)) cnt++;
   }
   return cnt;
}

size_t rhp_get_nb_var_ub(Model *mdl)
{
   if (chk_rmdl(mdl, __func__) != OK) {
      return RHP_INVALID_IDX;
   }

   size_t cnt = 0;
   for (size_t i = 0; i < rctr_totaln(&mdl->ctr); ++i) {
      Var *var = &mdl->ctr.vars[i];
      if (var->type == VAR_X && isfinite(var->bnd.ub)) cnt++;
   }
   return cnt;
}

size_t rhp_get_nb_var_interval(Model *mdl)
{
   if (chk_rmdl(mdl, __func__) != OK) {
      return RHP_INVALID_IDX;
   }

   size_t cnt = 0;
   for (size_t i = 0; i < rctr_totaln(&mdl->ctr); ++i) {
      Var *var = &mdl->ctr.vars[i];
      if (var->type == VAR_X && isfinite(var->bnd.lb) && isfinite(var->bnd.ub)) cnt++;
   }
   return cnt;
}

size_t rhp_get_nb_var_fx(Model *mdl)
{
   if (chk_rmdl(mdl, __func__) != OK) {
      return RHP_INVALID_IDX;
   }

   size_t cnt = 0;
   for (size_t i = 0; i < rctr_totaln(&mdl->ctr); ++i) {
      Var *var = &mdl->ctr.vars[i];
      if (var->type == VAR_X && isfinite(var->bnd.lb) && isfinite(var->bnd.ub) &&
          fabs(var->bnd.ub-var->bnd.lb) < TOL_BOUNDS_FIXED_VAR) cnt++;
   }
   return cnt;
}

/**
 * @brief Get the number of SOS1 variables
 *
 * @ingroup publicAPI
 *
 * @param mdl  the model
 *
 * @return     on success the number of SOS1 variable
 *             on failure, RHP_INVALID_IDX
 */
size_t rhp_get_nb_var_sos1(Model *mdl)
{
   if (chk_rmdl(mdl, __func__) != OK) {
      return RHP_INVALID_IDX;
   }
   RhpContainerData *cdat = (RhpContainerData *)mdl->ctr.data;

   return cdat->sos1.len;
}

/**
 * @brief Get the number of SOS2 variables
 *
 * @ingroup publicAPI
 *
 * @param mdl  the model
 *
 * @return     on success the number of SOS2 variable
 *             on failure, RHP_INVALID_IDX
 */
size_t rhp_get_nb_var_sos2(Model *mdl)
{
   if (chk_rmdl(mdl, __func__) != OK) {
      return RHP_INVALID_IDX;
   }
   RhpContainerData *cdat = (RhpContainerData *)mdl->ctr.data;

   return cdat->sos2.len;
}

/**
 * @brief Get the information associated with a SOS1 variable
 *
 * @ingroup publicAPI
 *
 * @param      mdl   the model
 * @param      vi  the variable index
 * @param[out] grps  the group
 *
 * @return      the error code
 */
int rhp_get_var_sos1(Model *mdl, rhp_idx vi, unsigned **grps)
{
   S_CHECK(chk_rmdl(mdl, __func__));
   return rctr_get_var_sos1(&mdl->ctr, vi, grps);
}

/**
 * @brief Get the information associated with a SOS2 variable
 *
 * @ingroup publicAPI
 *
 * @param mdl   the model
 * @param vidx  the variable index
 * @param grps  the group
 *
 * @return      the error code
 */
int rhp_get_var_sos2(Model *mdl, int vidx, unsigned **grps)
{
   S_CHECK(chk_rmdl(mdl, __func__));
   return rctr_get_var_sos2(&mdl->ctr, vidx, grps);
}

int rhp_set_var_sos1(Model *mdl, Avar *v, double *weights)
{
   S_CHECK(chk_rmdl(mdl, __func__));
   S_CHECK(chk_avar(v, __func__));
   RhpContainerData *cdat = (RhpContainerData *)mdl->ctr.data;

   if (cdat->sos1.len >= cdat->sos1.max) {
      unsigned old_max = cdat->sos1.max;
      cdat->sos1.max = MAX(2*cdat->sos1.max, cdat->sos1.max + 2);
      REALLOC_(cdat->sos1.groups, struct sos_grp, cdat->sos1.max);
      for (unsigned i = old_max; i < cdat->sos1.max; ++i) {
         avar_init(&cdat->sos1.groups[i].v);
      }
   }

   S_CHECK(avar_copy(&cdat->sos1.groups[cdat->sos1.len].v, v));
   if (weights) {
      MALLOC_(cdat->sos1.groups[cdat->sos1.len].w, double, v->size);
      memcpy(cdat->sos1.groups[cdat->sos1.len].w, weights, v->size*sizeof(double));
   } else {
      cdat->sos1.groups[cdat->sos1.len].w = 0;
   }

   cdat->sos1.len++;

   return OK;
}

int rhp_set_var_sos2(Model *mdl, Avar *v, double *weights)
{
   S_CHECK(chk_rmdl(mdl, __func__));
   S_CHECK(chk_avar(v, __func__));
   RhpContainerData *cdat = (RhpContainerData *)mdl->ctr.data;

   if (cdat->sos2.len >= cdat->sos2.max) {
      unsigned old_max = cdat->sos1.max;
      cdat->sos2.max = MAX(2*cdat->sos2.max, cdat->sos2.max + 2);
      REALLOC_(cdat->sos2.groups, struct sos_grp, cdat->sos2.max);
      for (unsigned i = old_max; i < cdat->sos1.max; ++i) {
         avar_init(&cdat->sos2.groups[i].v);
      }
   }

   S_CHECK(avar_copy(&cdat->sos2.groups[cdat->sos2.len].v, v));
   if (weights) {
      MALLOC_(cdat->sos2.groups[cdat->sos2.len].w, double, v->size);
      memcpy(cdat->sos2.groups[cdat->sos2.len].w, weights, v->size*sizeof(double));
   } else {
      cdat->sos2.groups[cdat->sos2.len].w = 0;
   }

   cdat->sos2.len++;

   return OK;
}

/**
 * @brief Set a boolean option
 *
 * @ingroup publicAPI
 *
 * @param mdl      the model
 * @param optname  the name of the option
 * @param opt      the value of the boolean
 *
 * @return         the error code
 */
int rhp_set_option_b(const Model *mdl, const char *optname, unsigned char opt)
{
   S_CHECK(chk_rmdl(mdl, __func__));

   union opt_t val;
   val.b = (bool)opt;
   return rmdl_setoption(mdl, optname, val);
}

/**
 * @brief Set an option of type real
 *
 * @ingroup publicAPI
 *
 * @param mdl      the model
 * @param optname  the name of the option
 * @param optval   the value
 *
 * @return         the error code
 */
int rhp_set_option_d(const Model *mdl, const char *optname, double optval)
{
   S_CHECK(chk_rmdl(mdl, __func__));

   union opt_t val;
   val.d = optval;
   return rmdl_setoption(mdl, optname, val);
}

/**
 * @brief Set an option of type integer
 *
 * @ingroup publicAPI
 *
 * @param mdl      the model
 * @param optname  the name of the option
 * @param optval   the value
 *
 * @return         the error code
 */
int rhp_set_option_i(const Model *mdl, const char *optname, int optval)
{
   S_CHECK(chk_rmdl(mdl, __func__));

   union opt_t val;
   val.i = optval;
   return rmdl_setoption(mdl, optname, val);
}

/**
 * @brief Set an option of type string
 *
 * @ingroup publicAPI
 *
 * @param mdl      the model
 * @param optname  the name of the option
 * @param optstr   the value
 *
 * @return         the error code
 */
int rhp_set_option_s(const Model *mdl, const char *optname, char *optstr)
{
   S_CHECK(chk_rmdl(mdl, __func__));

   union opt_t val;
   val.s = optstr;
   return rmdl_setoption(mdl, optname, val);
}

/**
 * @brief Return 1 if the index points to a valid variable, 0 otherwise
 *
 * @ingroup publicAPI
 *
 * @param mdl   the model
 * @param vi    the variable index
 *
 * @return      1 is the index points to a valid variable, 0 otherwise
 */
int rhp_is_var_valid(Model *mdl, rhp_idx vi)
{
   if (chk_rmdl(mdl, __func__)) {
      return 0;
   }

   RhpContainerData *cdat = (RhpContainerData *)mdl->ctr.data;
   if (!valid_vi(vi) || vi >= (rhp_idx)cdat->total_n) return 0;

   return mdl->ctr.vars[vi].is_deleted ? 0 : 1;
}

/**
 * @brief Return 1 if the index points valid equation, 0 otherwise
 *
 * @ingroup publicAPI
 *
 * @param mdl   the model
 * @param ei  the equation index
 *
 * @return      1 is the index points to a valid equation, 0 otherwise
 */
int rhp_is_equ_valid(Model *mdl, rhp_idx ei)
{
   if (chk_rmdl(mdl, __func__)) {
      return 0;
   }

   RhpContainerData *cdat = (RhpContainerData *)mdl->ctr.data;
    if (!valid_ei(ei) || ei >= (int)cdat->total_m) return 0;
   return cdat->equs[ei] ? 1 : 0;
}

/**
 * @brief Add an equation to the model
 *
 * @ingroup publicAPI
 *
 * @param      mdl   the model
 * @param[out] ei  the equation index
 *
 * @return           the error code
 */
int rhp_add_equation(Model *mdl, rhp_idx *ei)
{
   S_CHECK(chk_rmdl(mdl, __func__));
   S_CHECK(chk_ei_ptr(ei, __func__));
   return rctr_add_equ_empty(&mdl->ctr, ei, NULL, EquTypeUnset, CONE_NONE);
}

/**
 * @brief Add equations to the model
 *
 * @ingroup publicAPI
 *
 * @param      mdl   the model
 * @param      size    number of equations to add
 * @param[out] e     the equation
 *
 * @return           the error code
 */
int rhp_add_equations(Model *mdl, unsigned size, Aequ *e)
{
   S_CHECK(chk_rmdl(mdl, __func__));
   S_CHECK(chk_aequ_nonnull(e, __func__));

   aequ_setcompact(e, size, rctr_totalm(&mdl->ctr));

   for (size_t i = 0; i < size; ++i) {
      rhp_idx ei;
      S_CHECK(rctr_add_equ_empty(&mdl->ctr, &ei, NULL, EquTypeUnset, CONE_NONE));
      assert(ei == e->start + i);
   }

   return OK;
}

/**
 * @brief Add a constraint to the model
 *
 * @ingroup publicAPI
 *
 * @param      mdl   the model
 * @param      cone  the cone of the inclusion
 * @param[out] ei    the equation index
 *
 * @return           the error code
 */
int rhp_add_con(Model *mdl, unsigned cone, rhp_idx *ei)
{
   S_CHECK(chk_rmdl(mdl, __func__));
   S_CHECK(chk_ei_ptr(ei, __func__));

   S_CHECK(rctr_reserve_equs(&mdl->ctr, 1));

   return rctr_add_equ_empty(&mdl->ctr, ei, NULL, ConeInclusion, cone);
}
/**
 * @brief Add a constraint with a given name
 *
 * @ingroup publicAPI
 *
 * A copy of the string parameter is performed
 *
 * @param       mdl   the model
 * @param       cone  the size of the constraint 
 * @param[out]  ei    the constraint index
 * @param       name  the name of the constraint 

 * @return      the error code
 */
int rhp_add_connamed(Model *mdl, unsigned cone, rhp_idx *ei, const char* name)
{
   S_CHECK(chk_rmdl(mdl, __func__));
   RhpContainerData *cdat = (RhpContainerData*)mdl->ctr.data;

   char *ename;
   A_CHECK(ename, strdup(name));

   S_CHECK(cdat_equname_start(cdat, ename));
   S_CHECK(rhp_add_con(mdl, cone, ei));
   S_CHECK(cdat_equname_end(cdat));

   return OK;
}


/**
 * @brief Add a constraint to the model
 *
 * @ingroup publicAPI
 *
 * @param      mdl   the model
 * @param      size    the number of mapping to create
 * @param      type  the type of the inclusion
 * @param[out] e     the equation container
 *
 * @return           the error code
 */
int rhp_add_cons(Model *mdl, unsigned size, unsigned type, Aequ *eout)
{
   S_CHECK(chk_rmdl(mdl, __func__));
   S_CHECK(chk_aequ_nonnull(eout, __func__));

   S_CHECK(rctr_reserve_equs(&mdl->ctr, size));

   aequ_setcompact(eout, size, rctr_totalm(&mdl->ctr));

   for (size_t i = 0; i < size; ++i) {
      rhp_idx ei;
      S_CHECK(rctr_add_equ_empty(&mdl->ctr, &ei, NULL, ConeInclusion, type));
      assert(ei == eout->start + i);
   }

   return OK;
}

/**
 * @brief Add a named constraint
 *
 * @ingroup publicAPI
 *
 * @param      mdl   the model
 * @param      size  the size of the constraint 
 * @param      type  the type of constraint
 * @param[out] e     the constraint container
 * @param      name  the name of the constraint
 * @return           the error code
 */
int rhp_add_consnamed(Model *mdl, unsigned size, unsigned type,
                      Aequ *e, const char *name)
{
   S_CHECK(chk_rmdl(mdl, __func__));
   RhpContainerData *cdat = (RhpContainerData*)mdl->ctr.data;

   char *ename;
   A_CHECK(ename, strdup(name));

   S_CHECK(cdat_equname_start(cdat, ename));
   S_CHECK(rhp_add_cons(mdl, size, type, e));
   S_CHECK(cdat_equname_end(cdat));

   return OK;
}


/**
 * @brief Add a function (or mapping) to the model
 *
 * This is may be use for objective variables, but not for equation/inclusion
 *
 * @ingroup publicAPI
 *
 * @param      mdl  the model
 * @param[out] ei   the equation index
 *
 * @return           the error code
 */
int rhp_add_func(Model *mdl, rhp_idx *ei)
{
   S_CHECK(chk_rmdl(mdl, __func__));
   S_CHECK(chk_ei_ptr(ei, __func__));
   return rctr_add_equ_empty(&mdl->ctr, ei, NULL, Mapping, CONE_NONE);
}

/**
 * @brief Add a named function (or mapping) 
 *
 * This is may be use for objective variables, but not for equation/inclusion
 *
 * @ingroup publicAPI
 *
 * @param       mdl   the model
 * @param[out]  ei    the function index
 * @param       name  the name of the function 
 *
 * @return            the error code 
 */
int rhp_add_funcnamed(Model *mdl, rhp_idx *ei, const char *name)
{
   S_CHECK(chk_rmdl(mdl, __func__));
   RhpContainerData *cdat = (RhpContainerData*)mdl->ctr.data;

   char *ename;
   A_CHECK(ename, strdup(name));

   S_CHECK(cdat_equname_start(cdat, ename));
   S_CHECK(rhp_add_func(mdl, ei));
   S_CHECK(cdat_equname_end(cdat));

   return OK;
}

/**
 * @brief Add a function (or mapping) to the model
 *
 * This is may be use for objective functions, but not for equation/inclusion
 *
 * @ingroup publicAPI
 *
 * @param      mdl  the model
 * @param      size   the number of mapping to create
 * @param[out] e    the equation index
 *
 * @return           the error code
 */
int rhp_add_funcs(Model *mdl, unsigned size, Aequ *e)
{
   S_CHECK(chk_rmdl(mdl, __func__));
   S_CHECK(chk_aequ_nonnull(e, __func__));

   aequ_setcompact(e, size, rctr_totalm(&mdl->ctr));

   for (size_t i = 0; i < size; ++i) {
      rhp_idx ei;
      S_CHECK(rctr_add_equ_empty(&mdl->ctr, &ei, NULL, Mapping, CONE_NONE));
      assert(ei == e->start + i);
   }

   return OK;
}

/**
 * @brief Add a named function (or mapping) 
 *
 * This is may be use for objective variables, but not for equation/inclusion
 *
 * @ingroup publicAPI
 *
 * @param       mdl   the model
 * @param       size  the size of mapping
 * @param[out]  e     the container for indices
 * @param       name  the name of the function 
 * @return            the error code 
 */
int rhp_add_funcsnamed(Model *mdl, unsigned size, Aequ *e, const char *name)
{
   S_CHECK(chk_rmdl(mdl, __func__));
   RhpContainerData *cdat = (RhpContainerData*)mdl->ctr.data;

   char *ename;
   A_CHECK(ename, strdup(name));

   S_CHECK(cdat_equname_start(cdat, ename));
   S_CHECK(rhp_add_funcs(mdl, size, e));
   S_CHECK(cdat_equname_end(cdat));

   return OK;
}

/**
 * @brief Add an equality constraint
 *
 * @ingroup publicAPI
 *
 * @param      mdl   the model
 * @param[out] ei  the equation index
 *
 * @return           the error code
 */
int rhp_add_equality_constraint(Model *mdl, rhp_idx *ei)
{
   S_CHECK(chk_rmdl(mdl, __func__));
   S_CHECK(chk_ei_ptr(ei, __func__));
   return rctr_add_equ_empty(&mdl->ctr, ei, NULL, ConeInclusion, CONE_0);
}

/**
 * @brief Add an exponential cone constraint
 *
 * @ingroup publicAPI
 *
 * @param      mdl   the model
 * @param[out] ei  the equation index
 *
 * @return           the error code
 */
int rhp_add_exp_constraint(Model *mdl, rhp_idx *ei)
{
   S_CHECK(chk_rmdl(mdl, __func__));
   S_CHECK(chk_ei_ptr(ei, __func__));
   return rctr_add_equ_empty(&mdl->ctr, ei, NULL, ConeInclusion, CONE_EXP);
}

/**
 * @brief Add an greater-than constraint
 *
 * @ingroup publicAPI
 *
 * @param      mdl   the model
 * @param[out] ei  the equation index
 *
 * @return           the error code
 */
int rhp_add_greaterthan_constraint(Model *mdl, rhp_idx *ei)
{
   S_CHECK(chk_rmdl(mdl, __func__));
   S_CHECK(chk_ei_ptr(ei, __func__));
   return rctr_add_equ_empty(&mdl->ctr, ei, NULL, ConeInclusion, CONE_R_PLUS);
}

/**
 * @brief Add a less-than constraint
 *
 * @ingroup publicAPI
 *
 * @param      mdl   the model
 * @param[out] ei  the equation index
 *
 * @return           the error code
 */
int rhp_add_lessthan_constraint(Model *mdl, rhp_idx *ei)
{
   S_CHECK(chk_rmdl(mdl, __func__));
   S_CHECK(chk_ei_ptr(ei, __func__));
   return rctr_add_equ_empty(&mdl->ctr, ei, NULL, ConeInclusion, CONE_R_MINUS);
}

/**
 * @brief Add a power cone constraint
 *
 * @ingroup publicAPI
 *
 * @param      mdl   the model
 * @param[out] ei    the equation index
 *
 * @return           the error code
 */
int rhp_add_power_constraint(Model *mdl, rhp_idx *ei)
{
   S_CHECK(chk_rmdl(mdl, __func__));
   S_CHECK(chk_ei_ptr(ei, __func__));
   return rctr_add_equ_empty(&mdl->ctr, ei, NULL, ConeInclusion, CONE_POWER);
}

/**
 * @brief Add a second-order cone (SOC) constraint
 *
 * @ingroup publicAPI
 *
 * @param      mdl   the model
 * @param[out] ei    the equation index
 *
 * @return           the error code
 */
int rhp_add_soc_constraint(Model *mdl, rhp_idx *ei)
{
   S_CHECK(chk_rmdl(mdl, __func__));
   S_CHECK(chk_ei_ptr(ei, __func__));
   return rctr_add_equ_empty(&mdl->ctr, ei, NULL, ConeInclusion, CONE_SOC);
}

/**
 * @brief Delete an equation
 *
 * @ingroup publicAPI
 *
 * @param mdl  the model
 * @param ei   the equation to remove
 *
 * @return      the error code
 */
int rhp_delete_equ(Model *mdl, rhp_idx ei)
{
   S_CHECK(chk_rmdl(mdl, __func__));
   return rmdl_equ_rm(mdl, ei);
}

/**
 * @brief Delete a variable from the model
 *
 * @ingroup publicAPI
 *
 * @param mdl   the model
 * @param vi  the variable to delete
 *
 * @return      the error code
 */
int rhp_delete_var(Model *mdl, rhp_idx vi)
{
   S_CHECK(chk_rmdl(mdl, __func__));
   S_CHECK(chk_vi(mdl, vi, __func__));
   return rctr_delete_var(&mdl->ctr, vi);
}

/**
 * @brief Write out the model in a latex form
 *
 * @TODO document how to use the file
 *
 * @ingroup publicAPI
 *
 * @param mdl       the model
 * @param filename  the filename to store the latex
 *
 * @return          the error code
 */
int rhp_mdl_latex(Model *mdl, const char *filename)
{
   S_CHECK(rhp_chk_ctr(&mdl->ctr, __func__));
   if (!filename) {
      error("%s ERROR: error filename is NULL\n", __func__);
      return Error_NullPointer;
   }

   return rmdl_export_latex(mdl, filename);
}

/**
 * @brief Reserve memory for additional equations
 *
 * @ingroup publicAPI
 *
 * @param  mdl   the model
 * @param  size  the number of equation   
 *
 * @return       the error code
 */
int rhp_mdl_reserve_equs(struct rhp_mdl *mdl, unsigned size)
{
   S_CHECK(chk_rmdl(mdl, __func__));

   return rctr_reserve_equs(&mdl->ctr, size);
}

/**
 * @brief Reserve memory for additional variables
 *
 * @ingroup publicAPI
 *
 * @param  mdl   the model
 * @param  size  the number of variables   
 *
 * @return       the error code
 */
int rhp_mdl_reserve_vars(struct rhp_mdl *mdl, unsigned size)
{
   S_CHECK(chk_rmdl(mdl, __func__));

   return rctr_reserve_vars(&mdl->ctr, size);
}

/**
 * @brief Get the expression tree
 *
 * @ingroup publicAPI
 *
 * @param  ctr   the container object
 * @param  eidx  the equation index
 *
 * @return       If success, a pointer to the expression tree
 *               Otherwise, NULL
 */
NlTree* rhp_mdl_getnltree(const Model *mdl, rhp_idx ei)
{
   SN_CHECK(chk_rmdl(mdl, __func__));
   SN_CHECK(ei_inbounds(ei, rctr_totalm(&mdl->ctr), __func__));

   Equ *e = &mdl->ctr.equs[ei];
   if (!e->tree) {
      e->tree = nltree_alloc(0);
      if (!e->tree) {
         error("%s ERROR: call to nltree_alloc() failed!\n", __func__);
         return NULL;
      }
      e->tree->idx = ei;
   }

   return e->tree;
}

/**
 * @brief Set a given equation as the objective equation
 *
 * @warning this function should not be used when there also
 *          is an objective variable
 *
 * @param ctr     the container
 * @param objeqn  the objective equation index
 *
 * @return        the error code
 */
int rhp_mdl_setobjequ(Model *mdl, rhp_idx objequ)
{
   S_CHECK(chk_rmdl(mdl, __func__));

   return rmdl_setobjfun(mdl, objequ);
}


