#include <assert.h>
#include <float.h>
#include <math.h>
#include <stdbool.h>
#include <string.h>

#include "cmat.h"
#include "checks.h"
#include "container.h"
#include "ctr_rhp.h"
#include "rhp_LA.h"
#include "equ_modif.h"
#include "equ_modif_utils.h"
#include "nltree.h"
#include "nltree_priv.h"
#include "equ.h"
#include "equvar_helpers.h"
#include "lequ.h"
#include "macros.h"
#include "ctrdat_rhp.h"
#include "mdl_rhp.h"
#include "printout.h"



/** @brief add \f$\alpha <c,x>\f$ to the nonlinear part of the equation
 *
 *  @param ctr    the container object
 *  @param ei     the equation to modify
 *  @param vals   the values of c
 *  @param v      the variable x
 *  @param coeff  the coefficient \f$alpha\f$
 *
 *  @return       the error code
 */
int nltree_addlvars(Container *ctr, rhp_idx ei, double* vals, Avar *v, double coeff)
{
   assert(rhp_chk_ctr(ctr, __func__));
   assert(ei_inbounds(ei, rctr_totalm(ctr), __func__));
   assert(chk_arg_nonnull(vals, 3, __func__));
   assert(chk_arg_nonnull(v, 4, __func__));

   Equ *e = &ctr->equs[ei];
   NlTree *tree = e->tree;
   if (!tree) {
      unsigned size = avar_size(v);
      S_CHECK(nltree_bootstrap(e, 2*size, 2*size));
      tree = e->tree;
   }

   double lcoeff = coeff;
   NlNode **addr, *node;
   S_CHECK(nltree_find_add_node(tree, &addr, ctr->pool, &lcoeff));
   unsigned offset;
   S_CHECK(nltree_ensure_add_node(tree, addr, v->size, &offset));

   /* TODO(xhub) use this code where is it needed, not here */
#if 0
   /*  Avoid ADD -> ADD situation*/
   NlNode **caddr = &(*addr)->children[offset];
   bool new_node;
   S_CHECK(nltree_mul_cst_x(ctr, tree, &caddr, lcoeff, &new_node));
   if (new_node) {
      addr = caddr;
      S_CHECK(nltree_reserve_add_node(tree, addr, v->size, &offset));
   } else {
      unsigned dummy_offset;
      S_CHECK(nltree_reserve_add_node(tree, addr, v->size-1, &dummy_offset));
   }
#endif

   /* Now the ADD node is ready */
   node = *addr;

   for (unsigned i = 0; i < v->size; ++i, ++offset) {
      rhp_idx vidx = avar_fget(v, i);
      addr = &node->children[offset];
      S_CHECK(rctr_nltree_var(ctr, tree, &addr, vidx, lcoeff*vals[i]));
   }

   /* Make sure that we don't have just one child for the node  */
   S_CHECK(nltree_check_add(node));

   return OK;
}


/** @brief Add a quadratic term \f$ .5 x^TMx\f$ to an equation.
 *
 * @ingroup EquSafeEditing
 *
 *  @param ctr    the container object
 *  @param e      the equation to modify
 *  @param mat    the matrix
 *  @param v      the variable 
 *  @param coeff  the coefficient multiplying the quadratic. Note that is
 *                coefficient is multiplied by .5
 *
 *  @return       the error code
 */
int rctr_equ_add_quadratic(Container *ctr, Equ *e, SpMat* mat, Avar *v, double coeff)
{
   double val;
   unsigned *sizes = NULL;

   if (!rhpmat_nonnull(mat)) {
      error("%s :: the given matrix is empty!\n", __func__);
      return Error_UnExpectedData;
   }

   if (!rhpmat_is_square(mat)) {
      error("%s :: the matrix is not square!\n", __func__);
      return Error_UnExpectedData;
   }

   /* TODO(Xhub) is this a hack? */
   if (!e->tree) {
      /* TODO(xhub) Tune with nnz */
      RHP_INT nnz = rhpmat_nnz(mat);
      if (nnz == RHP_INTMAX) {
         return error_runtime();
      }

      S_CHECK(nltree_bootstrap(e, nnz, nnz));
      /* TODO(Xhub) check whether we can really do that
       * e->is_quad = true; */
   }

   NlNode *node = NULL;
   NlNode **addr = &e->tree->root;

   /* Prep the tree and get the address of the node in addr  */
   double lcoeff = coeff;
   S_CHECK(nltree_find_add_node(e->tree, &addr, ctr->pool, &lcoeff));

   NlNode *check_node = *addr;

   /* TODO(xhub) document that coeff  */
   coeff = .5*lcoeff;

   struct ctrmem CTRMEM working_mem = {.ptr = NULL, .ctr = ctr};

   if (mat->ppty & EMPMAT_EYE) {
      bool is_diag = false;
      double *vals = NULL;
      unsigned nb_blocks = 0;

      if (mat->ppty & EMPMAT_BLOCK) {
         assert(mat->block);
         nb_blocks = mat->block->number;
         A_CHECK(working_mem.ptr, ctr_getmem_old(ctr, sizeof(double) * nb_blocks));
         vals = (double*)working_mem.ptr;

         for (unsigned i = 0; i < nb_blocks; ++i) {
            struct sp_matrix *bcsr = mat->block->blocks[i];
            assert(bcsr);
            if (bcsr->nnzmax > 1) {
               error("%s :: unsupported non-identity matrix: nnz ="
                     " %d\n", __func__, bcsr->nnzmax);
               return Error_UnExpectedData;
            }
            vals[i] = bcsr->nnzmax > 0 ? bcsr->x[0] : 1.;
         }
         sizes = mat->block->row_starts;
         is_diag = true;
      } else {
         assert(mat->csr);
         switch (mat->csr->nnzmax) {
            case 0:
               val = 1.;
               vals = &val;
               break;
            case 1:
               vals = mat->csr->x;
               break;
            default:
               error("%s :: unsupported non-identity matrix: nnz = %d\n",
                     __func__, mat->csr->nnzmax);
               return Error_NotImplemented;
         }
         nb_blocks = 1;
         is_diag = true;
      }

      /* ---------------------------------------------------------------------
       * The diagonal case is the easiest one: just add n sqr nodes
       * ---------------------------------------------------------------------*/
      if (is_diag) {
         assert(v);
         NlNode* topnode = *addr;
         /* ------------------------------------------------------------------
          * If we have a block matrix, we use a topnode to do the splitting
          * between the blocks of the matrix
          * TODO(xhub) this doesn't look very good.
          * ------------------------------------------------------------------*/
         if (*addr) {
            assert(topnode->op == NlNode_Add);
            /*  TODO(xhub) this part needs quite a bit of crappy logic to
             *  work */
            // S_CHECK(nlnode_reserve(e->tree, topnode, nb_blocks));
            assert(topnode == *addr);
         } else {
            A_CHECK(topnode, nlnode_alloc(e->tree, 0));
            nlnode_default(topnode, NlNode_Add);
            *addr = topnode;
         }

         /* ------------------------------------------------------------------
          * Iterate over the blocks in the matrix
          * ------------------------------------------------------------------*/

         for (unsigned i = 0; i < nb_blocks; ++i) {
            unsigned offset;
            /* Do not multiply when the value is 0. */
            if (fabs(vals[i]) < DBL_EPSILON) {
               /*  TODO(xhub) make sure this is not necessary
                *addr = NULL; */
               continue;
            }

            offset = topnode->children_max;
            S_CHECK(nlnode_reserve(e->tree, topnode, 1));

            /* ---------------------------------------------------------------
             * Multiply by the coefficient and create the ADD node for sum x_i^2
             * ---------------------------------------------------------------*/
            S_CHECK(nltree_mul_cst_add_node(e->tree, &addr, ctr->pool, vals[i]*coeff,
                                            v->size, &offset));

            node = *addr;
            unsigned end, offset_var;
            if (i == nb_blocks-1) {
               end = v->size;
            } else {
               assert(sizes);
               end = sizes[i+1];
            }
            if (nb_blocks > 1) {
               assert(sizes);
               offset_var = sizes[i];
            } else {
               offset_var = 0;
            }

            switch (v->type) {
            case EquVar_Compact:
               for (unsigned k = offset, j = v->start + offset_var; k < end + offset; ++k, ++j) {
                  addr = &node->children[k];
                  S_CHECK(rctr_nltree_add_sqr(ctr, e->tree, &addr, j));
                  S_CHECK(nlnode_print_now(node->children[k]));
               }
               break;
            case EquVar_List:
               assert(v->list);
               for (unsigned k = offset_var;  k < end; ++k, ++offset) {
                  addr = &node->children[offset];
                  S_CHECK(rctr_nltree_add_sqr(ctr, e->tree, &addr, v->list[k]));
                  S_CHECK(nlnode_print_now(node->children[offset]));
               }
               break;
            default:
               TO_IMPLEMENT("Block Var");
            }
            /*  \TODO(xhub) fix this hack */
            for (unsigned k = v->size; k < node->children_max; ++k) {
               node->children[k] = NULL;
            }
         }

      } else {
         error("%s :: non unique diag case to be implemented\n",
               __func__);
         return Error_NotImplemented;
      }

   } else {
      /* ---------------------------------------------------------------------
       * Only the triplet (or COO, coordinate) format is supported right now
       * TODO(xhub) 
       * ---------------------------------------------------------------------*/
      switch (rhpmat_get_type(mat)) {
      case EMPMAT_COO:
      {
         struct sp_matrix *m = mat->triplet;
         assert(m);
         S_CHECK(rctr_nltree_add_quad_coo_abs(ctr, e, addr, m->nnz, m->i, m->p, m->x, coeff));
         break;
      }
      case EMPMAT_CSR:
      case EMPMAT_CSC:
      default:
         error("%s :: only diagonal and triplet matrices are supported\n", __func__);
         return Error_NotImplemented;
      }
   }

   S_CHECK(nltree_check_add(check_node));

   return OK;
}

/** 
 *  @brief Add the inner product \f$ \langle u, B F(x) + b\rangle \f$ or 
 *         \f$ \langle u, B v + b\rangle \f$
 *
 * @warning the node where to copy must be an ADD node and have enough children
 * space to receive the expression in a continuous fashion
 *
 * @ingroup EquSafeEditing
 *
 *  @param ctr      the container object
 *  @param tree     the nltree
 *  @param addnode  the ADD node where to copy the expression
 *  @param uvar     the variable u
 *  @param B        the linear part of the transformation
 *  @param b        the constant part of the transformation
 *  @param coeffs   the 
 *  @param args     the mapping variables in the equation
 *  @param eqn      the equations
 *
 *  @return               the error code
 */
int rctr_nltree_cpy_dot_prod_var_map(Container *ctr, NlTree *tree, NlNode *addnode,
                                     Avar *uvar, const SpMat *B, const double *b,
                                     double *coeffs, Avar * restrict args, Aequ* eqn)
{
   assert(addnode->op == NlNode_Add);

   /* TODO: this relies on u always being nonlinear. This could fail. Also the
    * quadratic case is not handled well*/
   /* TODO QUAD support */

   /* Needed because of nlnode_copy */
   S_CHECK(nltree_reset_var_list(tree));

   unsigned offset = 0;
   while (addnode->children[offset]) {
      offset++;
      assert (offset < addnode->children_max);
   }

   for (unsigned i = 0, idx_c = offset; i < uvar->size; ++i) {

      if (idx_c >= addnode->children_max) {
         error("%s :: parent node is not big enough\n", __func__);
         return Error_SizeTooSmall;
      }

      /* Number of component of B_i  */
      unsigned *row_idx, row_len, row_single_idx;
      double *row_vals = NULL, single_val;
      S_CHECK(rhpmat_row_needs_update(B, i, &row_single_idx, &single_val, &row_len,
               &row_idx, &row_vals));

      if (row_len == 0) {
         error("[Warn] %s :: row %d is empty\n", __func__, i);
         continue;
      }

      /* Now args_indx_len contains the number of nnz in the i-th row of B */
      rhp_idx ui = avar_fget(uvar, i);

      /* -------------------------------------------------------------------
       * Add < u_i, B_i*(...) + b_i >
       * ------------------------------------------------------------------- */

      NlNode **addr = &addnode->children[idx_c];
      assert(!*addr);
      double bi = b ? b[i] : NAN;

      S_CHECK(rctr_nltree_add_bilin(ctr, tree, &addr, 1., ui, IdxNA));

      /* Add the term <u_i, b_i> if necessary */

      if (b && fabs(bi) > DBL_EPSILON) {
         S_CHECK(nltree_add_cst(ctr, tree, &addr, bi));
      }

      rhp_idx ei;
      S_CHECK(aequ_get(eqn, row_idx[0], &ei));

      /* -------------------------------------------------------------------
       *  Case: < u_i, B_i*arg_i + b_i >. It remains to add B_i*v_i to the nltree
       * ------------------------------------------------------------------- */

      if (row_len == 1 && !valid_ei(ei)) {
         unsigned arg_idx = row_idx[0];
         rhp_idx vi;
         S_CHECK(avar_get(args, arg_idx, &vi));
         assert(valid_vi(vi));

         /* -----------------------------------------------------------------
          * We don't use coeffs here since it is NAN
          * ----------------------------------------------------------------- */

         double coeff;
         if (row_vals) { coeff = row_vals[0]; }
         else { coeff = 1.; }

         S_CHECK(rctr_nltree_var(ctr, tree, &addr, vi, coeff));

         nlnode_print_now(addnode->children[idx_c]);

         /* Make sure that we don't have just one child for the node  */
         if (addnode->children[idx_c]->op == NlNode_Add) {
            S_CHECK(nltree_check_add(addnode->children[idx_c]));
         }

         idx_c++;

         continue;
      }

      nlnode_print_now(addnode->children[idx_c]);

      /* TODO: document */
      if (*addr) {
         assert((*addr)->op == NlNode_Add || (*addr)->op == __OPCODE_LEN);
         if ((*addr)->op == __OPCODE_LEN) {
            nlnode_default(*addr, NlNode_Add);
         }
         S_CHECK(nlnode_reserve(tree, *addr, 2*row_len));
      } else {
         A_CHECK(*addr, nlnode_alloc_fixed_init(tree, row_len));
         nlnode_default(*addr, NlNode_Add);
      }

      NlNode *add_node = *addr;

      for (unsigned j = 0; j < row_len; ++j) {
         /* We need to encode var_idx * coeff * var  */
         unsigned arg_idx = row_idx[j];
         S_CHECK(aequ_get(eqn, arg_idx, &ei));

         rhp_idx vi;
         S_CHECK(avar_get(args, arg_idx, &vi));

         assert(valid_ei(ei) || valid_vi(vi));

         /* TODO: document */
         if (row_len > 1) {
            addr = &add_node->children[j];
         }

         /* -----------------------------------------------------------------
          * If the second term is a variable, we are done quickly
          * If not, we have to copy the expression from the second term
          * ----------------------------------------------------------------- */

         if (!valid_ei(ei)) {
            S_CHECK(nltree_add_var(ctr, tree, &addr, vi, row_vals ? row_vals[j] : 1.));
         } else {
            double mcoeff;
            if (row_vals) { mcoeff = -row_vals[j]/coeffs[arg_idx]; }
            else { mcoeff = -1./coeffs[arg_idx]; }

            /*  TODO(Xhub) try to be a bit smarter here*/
            if (fabs(mcoeff) < DBL_EPSILON) {
               error("%s :: coefficient for index %d is too small\n",
                     __func__, j);
               return Error_UnExpectedData;
            }

            /* Save this node since we need to check it afterwards  */
            NlNode *l_add_node = *addr;
            assert(l_add_node->op == NlNode_Add);

            S_CHECK(rctr_nltree_copy_map_old(ctr, tree, addr, &ctr->equs[ei], vi, mcoeff));

            /* TODO: it is unclear that this still works is needed */
            if (l_add_node != *addr) {
               S_CHECK(nltree_check_add(l_add_node));
            }
         }
      }

      /* Make sure that we don't have just one child for the node  */
      if (add_node->op == NlNode_Add) {
         S_CHECK(nltree_check_add(add_node));
      }

      /* If we make it here, we should increase the counter  */
      idx_c++;
   }

   /* Make sure that we don't have just one child for the node  */
   S_CHECK(nltree_check_add(addnode));

   /* ----------------------------------------------------------------------
    * Update the container matrix
    * ---------------------------------------------------------------------- */

   if (tree->v_list) {
      Avar _v;
      avar_setlist(&_v, tree->v_list->idx, tree->v_list->pool);

      S_CHECK(cmat_equ_add_vars(ctr, tree->idx, &_v, NULL, true));
   }

   return OK;
}

/** 
 *  @brief Add the inner product \f$ \langle c, B F(x) + b\rangle \f$
 *
 *  @param  ctr      the container
 *  @param  e        the destination equation
 *  @param  c        the vector of constants
 *  @param  n_c      the size of the inner product arguments
 *  @param  B        the B matrix in the affine transformation
 *  @param  b        the c constant part in the affine transformation
 *  @param  coeffs   ???
 *  @param  v        ???
 *  @param  eis      ???
 *  @param  coeff    ???
 *
 *  @return               the error code
 */
int rctr_equ_add_dot_prod_cst(Container *ctr, Equ *e, const double *c, unsigned n_c,
                         SpMat *B, double *b, double *coeffs, Avar *v, rhp_idx *eis,
                         double coeff)
{
   NlNode **addr = NULL, *add_node = NULL;
   double dummyval = 1.;

   NlTree *tree = NULL;
   unsigned offset = UINT_MAX;

   /* ----------------------------------------------------------------------
    * Go through each component of u
    * ---------------------------------------------------------------------- */

   for (unsigned i = 0; i < n_c; ++i) {

      double ci = c[i];
      if (fabs(ci) < DBL_EPSILON) { continue; }
      ci *= coeff;

      double bi = b ? b[i] : NAN;

      unsigned *args_idx;
      /* Number of component of B_i  */
      unsigned args_idx_len;
      unsigned single_idx;
      double *lcoeffs = NULL;
      double single_val;
      S_CHECK(rhpmat_row_needs_update(B, i, &single_idx, &single_val, &args_idx_len,
               &args_idx, &lcoeffs));

      if (args_idx_len == 0) {
         error("[Warn] %s :: row %d is empty\n", __func__, i);
         continue;
      }

      /* -------------------------------------------------------------------
       * We can always add to the constant
       * ------------------------------------------------------------------- */

      /*  TODO(xhub) CONE_SUPPORT */
      if (b && fabs(bi) > DBL_EPSILON) {
         equ_add_cst(e, ci*bi);
      }

      /*  Trivial case: <u_i, B_i*v_i + b_i> */
      if (args_idx_len == 1 && !valid_ei(eis[args_idx[0]])) {
         rhp_idx arg_idx = args_idx[0], vidx;
         S_CHECK(avar_get(v, arg_idx, &vidx));

         assert(arg_idx >= 0);
         assert(valid_vi(vidx));
         /* Add the term <u_i, b_i> if necessary */

         /* ----------------------------------------------------------------
          * We don't use coeffs here since it is NAN
          * ---------------------------------------------------------------- */

         double mcoeff;
         if (lcoeffs) { mcoeff = ci*lcoeffs[0]; }
         else { mcoeff = ci; }

         S_CHECK(rctr_equ_addlvar(ctr, e, vidx, mcoeff));

         continue;
      }

      for (unsigned j = 0; j < args_idx_len; ++j) {
         /* We need to encode var_idx * coeff * var  */

         unsigned arg_idx = args_idx[j];
         rhp_idx eqidx = eis[arg_idx];
         rhp_idx vidx;
         S_CHECK(avar_get(v, arg_idx, &vidx));

         bool isvar = !valid_ei(eqidx);
         assert(!isvar || valid_vi(vidx));

         Equ *eq = &ctr->equs[eqidx];

         /* ---------------------------------------------------------------- *
          * If the second term is a variable, we are done quickly            *
          * If not, we have to copy the expression from the second term      *
          * ---------------------------------------------------------------- */

         if (isvar) {
            S_CHECK(rctr_equ_addlvar(ctr, e, vidx, lcoeffs ? ci*lcoeffs[j] : ci));
         } else {
            double mcoeff;
            if (lcoeffs) { mcoeff = -ci*lcoeffs[j]/coeffs[arg_idx]; }
            else { mcoeff = -ci/coeffs[arg_idx]; }

            /*  TODO(Xhub) try to be a bit smarter here*/
            if (fabs(mcoeff) < DBL_EPSILON) {
               error("%s :: coefficient for index %d is too small\n",
                     __func__, j);
               return Error_UnExpectedData;
            }

            /* ------------------------------------------------------------- *
             * Add the linear part of the equation: B_i (sum a_i x_i)        *
             * ------------------------------------------------------------- */

            Lequ *lequ = eq->lequ;
            for (unsigned k = 0; k < lequ->len; ++k) {
               if (lequ->vis[k] != vidx && isfinite(lequ->coeffs[k])) {
                  S_CHECK(rctr_equ_addlvar(ctr, e, lequ->vis[k],
                                                       mcoeff*lequ->coeffs[k]));
               }
            }

            /*  TODO(xhub) CONE_SUPPORT */
            double cst = equ_get_cst(eq);
            if (fabs(cst) > DBL_EPSILON) {
               equ_add_cst(e, cst*mcoeff);
            }

            /* --------------------------------------------------------------
             * deal with the nonlinear part
             * -------------------------------------------------------------- */

            S_CHECK(rctr_getnl(ctr, eq));

            if (eq->tree && eq->tree->root) {

               /* ---------------------------------------------------------- *
                * We do a lazy init here                                     *
                * ---------------------------------------------------------- */

               if (!addr) {
                  if (!e->tree) {
                     /*  TODO(xhub) see the constant */
                     S_CHECK(nltree_bootstrap(e, 2*n_c, 2*n_c));
                  }

                  tree = e->tree;

                  /*  Needed because of nlnode_copy */
                  S_CHECK(nltree_reset_var_list(tree));

                  S_CHECK(nltree_find_add_node(tree, &addr, ctr->pool, &dummyval));
                  S_CHECK(nlnode_child_getoffset(tree, *addr, &offset));

                  S_CHECK(nlnode_reserve(tree, *addr, n_c));
               }

               NlNode **laddr;
               /* ---------------------------------------------------------- *
                * We do a lazy init here                                     *
                * ---------------------------------------------------------- */

               if (args_idx_len > 1) {
                  if (!add_node) {
                     A_CHECK((*addr)->children[offset],
                                 nlnode_alloc_fixed_init(tree, args_idx_len));
                     add_node = (*addr)->children[offset];
                  }

                  laddr = &add_node->children[j];
               } else {
                  laddr = &(*addr)->children[offset];
               }
               /* Add B_i * ( ... )  */
               /* TODO(xhub) we may end up in a ADD->ADD situation, be careful
                * here*/
               S_CHECK(nltree_mul_cst(tree, &laddr, ctr->pool, mcoeff/dummyval));

               /* ----------------------------------------------------------
                * Copy the expression tree
                * ---------------------------------------------------------- */

               S_CHECK(nlnode_dup(laddr, eq->tree->root, tree));

            }
         }
      }
      /* Do a final sanity check: it may happen that the equation consists only
       * of 1 child */
      if (add_node) {
         S_CHECK(nltree_check_add(add_node));
         add_node = NULL;
      }
      if (addr && offset < UINT_MAX) {
         offset++;
      }
   }

   if (addr) {
      assert(*addr);
      S_CHECK(nltree_check_add(*addr));
   }

   /* ----------------------------------------------------------------------
    * Keep the model representation consistent
    * ---------------------------------------------------------------------- */

   if (tree && tree->v_list) {
      Avar _v;
      avar_setlist(&_v, tree->v_list->idx, tree->v_list->pool);

      S_CHECK(cmat_equ_add_vars(ctr, tree->idx, &_v, NULL, true));
   }

   return OK;
}

/** 
 *  @brief Add the inner product \f$ \langle c, B F(x) + b\rangle \f$
 *
 *  @param ctr      ???
 *  @param tree     ???
 *  @param node     ???
 *  @param offset   ???
 *  @param c        ???
 *  @param n_c      ???
 *  @param B        ???
 *  @param b        ???
 *  @param coeffs   ???
 *  @param var_idx  ???
 *  @param eis  ???
 *
 *  @return               the error code
 */
int equ_add_dot_prod_cst_x(Container *ctr, NlTree *tree, NlNode *node, unsigned offset,
                           const double *c, unsigned n_c, SpMat *B,
                           double *b, double *coeffs, rhp_idx *var_idx, rhp_idx *eis)
{
   assert(c);
   assert(n_c > 0);

   /* Needed because of nlnode_copy */
   S_CHECK(nltree_reset_var_list(tree));

   for (unsigned i = 0, idx_c = offset; i < n_c; ++i) {

      double ci = c[i];

      if (fabs(ci) < DBL_EPSILON) { continue; }

      unsigned *args_idx;
      /* Number of component of B_i  */
      unsigned args_idx_len;
      unsigned single_idx;
      double *lcoeffs = NULL;
      double single_val;
      S_CHECK(rhpmat_row_needs_update(B, i, &single_idx, &single_val, &args_idx_len,
               &args_idx, &lcoeffs));

      if (args_idx_len == 0) {
         error("[Warn] %s :: row %d is empty\n", __func__, i);
         continue;
      }

      if (idx_c >= node->children_max) {
         error("%s :: parent node is not big enough\n", __func__);
         return Error_SizeTooSmall;
      }

      /* Add <u_i, B_i*(...) + b_i>  */
      NlNode **addr = &node->children[idx_c];
      double bi = b ? b[i] : NAN;

      /*  Trivial case: <u_i, B_i*v_i + b_i> */
      if (args_idx_len == 1 && !valid_ei(eis[args_idx[0]])) {
         int arg_idx = args_idx[0];
         int vidx = var_idx[arg_idx];

         assert(arg_idx >= 0);
         assert(var_idx[arg_idx] >= 0);

         /* Add the term <u_i, b_i> if necessary */

         if (b && fabs(bi) > DBL_EPSILON) {
            S_CHECK(nltree_add_cst(ctr, tree, &addr, ci*bi));
         }

         /* -----------------------------------------------------------------
          * We don't use coeffs here since it is NAN
          * ----------------------------------------------------------------- */

         double coeff;
         if (lcoeffs) { coeff = ci*lcoeffs[0]; }
         else { coeff = ci; }

         S_CHECK(rctr_nltree_var(ctr, tree, &addr, vidx, coeff));

         nlnode_print_now(node->children[idx_c]);
         idx_c++;
         continue;
      }

      /* Add the term <u_i, b_i> if necessary */
      if (b && fabs(bi) > DBL_EPSILON) {
         S_CHECK(nltree_add_cst(ctr, tree, &addr, ci*bi));
      }

      A_CHECK(*addr, nlnode_alloc_fixed_init(tree, args_idx_len));
      nlnode_default(*addr, NlNode_Add);

      NlNode *add_node = *addr;

      for (unsigned j = 0; j < args_idx_len; ++j) {
         /* We need to encode var_idx * coeff * var  */

         int arg_idx = args_idx[j];
         int eqidx = eis[arg_idx];
         int vidx = var_idx[arg_idx];
         bool isvar = !valid_ei(eqidx);
         assert(!isvar || valid_vi(vidx));

         Equ *e = &ctr->equs[eqidx];
         if (args_idx_len > 1) {
            addr = &add_node->children[j];
         }

         /* -----------------------------------------------------------------
          * If the second term is a variable, we are done quickly
          * If not, we have to copy the expression from the second term
          * ----------------------------------------------------------------- */

         if (isvar) {
            S_CHECK(nltree_add_var(ctr, tree, &addr, vidx, lcoeffs ? ci*lcoeffs[j] : ci));
         } else {
            double coeff;
            if (lcoeffs) { coeff = ci*lcoeffs[j]/coeffs[arg_idx]; }
            else { coeff = ci/coeffs[arg_idx]; }

            /*  TODO(Xhub) try to be a bit smarter here*/
            if (fabs(coeff) < DBL_EPSILON) {
               error("%s :: coefficient for index %d is too small\n",
                     __func__, j);
               return Error_UnExpectedData;
            }

            /*  Add B_i (sum a_i x_i) */
            Lequ *lequ = e->lequ;
            if (lequ && lequ->len > 1) {
               S_CHECK(rctr_nltree_add_lin_term(ctr, tree, &addr, lequ, vidx,
                        -coeff));
            }

            double cst = equ_get_cst(e);
            if (fabs(cst) > DBL_EPSILON) {
               /*  TODO(xhub) URG CHK SIGN */
               S_CHECK(nltree_add_cst(ctr, tree, &addr, -cst*coeff));
            }

            /* --------------------------------------------------------------
             * node now contains a pointer to the node that contains the expression
             * -------------------------------------------------------------- */

            S_CHECK(rctr_getnl(ctr, e));

            if (e->tree && e->tree->root) {

               /*  Get the first empty child */
               S_CHECK(nlnode_next_child(tree, &addr));

               /* Add B_i * ( ... )  */
               /* TODO(xhub) we may end up in a ADD->ADD situation, be careful
                * here
                * TODO(Xhub) clarify -coeff*/
               S_CHECK(nltree_mul_cst(tree, &addr, ctr->pool, -coeff));

               /* \TODO(xhub) optimize w.r.t. umin */
               /* Fill in Fnl_i  */
               S_CHECK(nlnode_dup(addr, e->tree->root, tree));
            }

            /* Do a final sanity check: it may happen that the equation consists
             * only of 1 */
            if (*addr && (*addr)->op == NlNode_Add) {
               S_CHECK(nltree_check_add(*addr));
            }
         }
      }

      /* Make sure that we don't have just one child for the node  */
      S_CHECK(nltree_check_add(add_node));

      /* If we make it here, we should increase the counter  */
      idx_c++;
   }

   /* Make sure that we don't have just one child for the node  */
   S_CHECK(nltree_check_add(node));

   /* ----------------------------------------------------------------------
    * Keep the model representation consistent
    * ---------------------------------------------------------------------- */

   if (tree->v_list) {
      Avar v;
      avar_setlist(&v, tree->v_list->idx, tree->v_list->pool);

      S_CHECK(cmat_equ_add_vars(ctr, tree->idx, &v, NULL, true));
   }

   return OK;
}


/**
 * @brief Copy  \f$ <Bi, F(x)> + b\f$ into an equation
 *
 * @param ctr            the container
 * @param edst           the equation to modify
 * @param coeffs         ???
 * @param row_len        the number of arguments
 * @param row_idx        ???
 * @param ei_maps        ???
 * @param v              ???
 * @param lcoeffs        ???
 * @param cst            ???
 *
 * @return               the error code
 */
int rctr_equ_add_maps(Container *ctr, Equ *edst, double *coeffs,
                      unsigned row_len, rhp_idx *row_idx, rhp_idx *ei_maps,
                      Avar * restrict v, double *lcoeffs, double cst)
{
   Lequ *le = edst->lequ;

   for (unsigned j = 0; j < row_len; ++j) {


      unsigned ridx = row_idx[j]; assert(ridx < avar_size(v));

      rhp_idx ei = ei_maps[ridx];
      rhp_idx vi;
      S_CHECK(avar_get(v, ridx, &vi));

      /* -----------------------------------------------------------------
       * If the argument is a variable, just add it to the equation and go
       * to the next argument
       * ----------------------------------------------------------------- */

      if (!valid_ei(ei)) {
         S_CHECK(lequ_add_unique(le, vi, cst));
         continue;
      }

      /* -----------------------------------------------------------------
       * The argument refers to an equation that we will copy into the new
       * one
       * ----------------------------------------------------------------- */

      /* -----------------------------------------------------------------
       * Compute the coefficient that will multiply the function part
       * ----------------------------------------------------------------- */

      double coeff = coeffs[ridx];
      double coeffp;
      if (lcoeffs) { coeffp = cst*lcoeffs[j]/coeff; }
      else { coeffp = cst/coeff; }

      S_CHECK(rctr_equ_add_newmap(ctr, edst, ei, vi, -coeffp));
   }

   return OK;
}

/**
 *  @brief Switch a variable to the NL part of the equation
 *
 *  @param ctr   the container
 *  @param e     the equation
 *  @param vi    the variable index
 *
 *  @return      the error code
 */
int equ_switch_var_nl(Container *ctr, Equ *e, rhp_idx vi)
{
   double val;
   unsigned pos;
   S_CHECK(lequ_find(e->lequ, vi, &val, &pos));
   if (!isfinite(val)) {
      error("%s :: the variable %s is marked as linear in equation %s, but "
            "can't be found\n", __func__, ctr_printvarname(ctr, vi),
            ctr_printequname(ctr, e->idx));
      return Error_Inconsistency;
   }

   lequ_delete(e->lequ, pos);

   /* This is a bit weird  */
   S_CHECK(nltree_add_var_tree(ctr, e->tree, vi, val));

   return OK;
}

/** @brief remove (safely) a variable from an equation
 *
 *  @param ctr   the container
 *  @param e     the equation
 *  @param vi    the variable index
 *
 *  @return      the error code
 */
// HACK : this should be a rctr function
int equ_rm_var(Container *ctr, Equ *e, rhp_idx vi)
{
   double dummy = NAN;
   unsigned pos = UINT_MAX;

   if (e->lequ) {
      S_CHECK(lequ_find(e->lequ, vi, &dummy, &pos));
   }

   if (!isfinite(dummy)) {
      TO_IMPLEMENT("Only linear variable can be removed for now");
   }

   lequ_delete(e->lequ, pos);

   // HACK: what about the MP
   S_CHECK(cmat_equ_rm_var(ctr, e->idx, vi));

   assert(e->lequ && !lequ_debug_hasvar(e->lequ, vi));

   return OK;
}
