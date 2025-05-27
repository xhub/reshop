
/*  TODO: temporary kludge to be removed once avar_size is moved out of reshop.h */
#include "reshop.h"

#include "equ.h"
#include "equ_modif_utils.h"
#include "nltree.h"
#include "nltree_priv.h"
#include "macros.h"
#include "var.h"


/**
 * @brief Add a quadratic expression c * (v_i * v_j * x_ij) given as COO
 *
 * @ingroup EquSafeEditing
 *
 * @param ctr    the model
 * @param e      the equation
 * @param addr   the address of the node
 * @param nnz    the number of non-zero
 * @param i      the row indices
 * @param j      the column indices
 * @param x      the data array
 * @param coeff  the coeff in front
 *
 * @return       the error code
 */
int rctr_nltree_add_quad_coo_abs(Container *ctr, Equ * e, NlNode **addr,
                                 size_t nnz, RHP_INT *i, RHP_INT *j, double *x,
                                 double coeff)
{
   /* ------------------------------------------------------------------
    * We already have an add node in addr. The triplet case is easy
    * since we add sum v_ij x_i * x_j where i is a row index, j is a
    * column index 
    *
    * 1. Reserve the space of all the elements
    * 2. Add all the v_ij x_i * x_j terms
    * ------------------------------------------------------------------*/
   /*  TODO(xhub) find a cleaner way to do this */
   unsigned cur_idx;
   NlNode *node;

   S_CHECK(nltree_ensure_add_node(e->tree, addr, nnz, &cur_idx));
   N_CHECK(node, (*addr));

   for (size_t k = 0; k < nnz; ++k, ++cur_idx) {
      addr = &node->children[cur_idx];
      rhp_idx v1     = i[k];
      rhp_idx v2     = j[k];
      /* TODO(xhub) optimize with cblas/others? */
      double val = x[k]*coeff;

      /* ---------------------------------------------------------------
       * If v1 == v2 the term is v_ii x_i^2
       * ---------------------------------------------------------------*/

      if (v1 == v2) {
         S_CHECK(nltree_mul_cst(e->tree, &addr, ctr->pool, val));
         S_CHECK(rctr_nltree_add_sqr(ctr, e->tree, &addr, v1));
         assert(node->children[cur_idx]);
         S_CHECK(nlnode_print_now(node->children[cur_idx]));
      } else {

      /* ---------------------------------------------------------------
       * This term is v_ij x_i x_j
       * ---------------------------------------------------------------*/

         S_CHECK(rctr_nltree_add_bilin(ctr, e->tree, &addr, val, v1, v2));
      }
   }

   return OK;
}

/**
 * @brief Add a quadratic expression c * (v_i * v_j * x_ij) given as COO
 *
 * @warning the COO indices are relative to v_row and v_col
 *
 * @param ctr    the model
 * @param e      the equation
 * @param addr   the address of the node
 * @param v_row  the row variable
 * @param v_col  the column variable
 * @param nnz    the number of quadratic terms
 * @param i      the row indices
 * @param j      the column indices
 * @param x      the data array
 * @param coeff  the coeff in front
 *
 * @return       the error code
 */
int rctr_nltree_add_quad_coo_rel(Container *ctr, Equ * e, NlNode **addr,
                                 Avar * restrict v_row, Avar * restrict v_col,
                                 size_t nnz, unsigned * restrict i, unsigned * restrict j,
                                 double * restrict x, double coeff)
{
   /* ------------------------------------------------------------------
    * We already have an add node in addr. The triplet case is easy
    * since we add sum v_ij x_i * x_j where i is a row index, j is a
    * column index 
    *
    * 1. Reserve the space of all the elements
    * 2. Add all the v_ij x_i * x_j terms
    * ------------------------------------------------------------------*/
   /*  TODO(xhub) find a cleaner way to do this */
   unsigned cur_idx;
   NlNode *node;

   S_CHECK(nltree_ensure_add_node(e->tree, addr, nnz, &cur_idx));
   N_CHECK(node, (*addr));

   for (size_t k = 0; k < nnz; ++k, ++cur_idx) {
      /* TODO(xhub) optimize with cblas/others? */
      double val = x[k]*coeff;

      addr = &node->children[cur_idx];
      rhp_idx v1     = avar_fget(v_row, i[k]);
      rhp_idx v2     = avar_fget(v_col, j[k]);
      if (!valid_vi(v1)) {
         error("%s :: invalid index %u for avar of size %u, err is %s\n",
                            __func__, i[k], avar_size(v_row), badidx2str(v1));
      }

      if (!valid_vi(v2)) {
         error("%s :: invalid index %u for avar of size %u, err is %s\n",
                            __func__, j[k], avar_size(v_col), badidx2str(v2));
      }

      /* ---------------------------------------------------------------
       * If v1 == v2 the term is v_ii x_i^2
       * ---------------------------------------------------------------*/

      if (v1 == v2) {
         S_CHECK(nltree_mul_cst(e->tree, &addr, ctr->pool, val));
         S_CHECK(rctr_nltree_add_sqr(ctr, e->tree, &addr, v1));
         assert(node->children[cur_idx]);
         S_CHECK(nlnode_print_now(node->children[cur_idx]));
      } else {

      /* ---------------------------------------------------------------
       * This term is v_ij x_i x_j
       * ---------------------------------------------------------------*/

         S_CHECK(rctr_nltree_add_bilin(ctr, e->tree, &addr, val, v1, v2));
      }
   }

   return OK;
}
