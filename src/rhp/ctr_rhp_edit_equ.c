#include "checks.h"
#include "cmat.h"
#include "container.h"
#include "ctr_rhp.h"
#include "ctrdat_rhp.h"
#include "ctrdat_rhp_priv.h"
#include "equ_modif.h"
#include "equvar_metadata.h"
#include "equvar_helpers.h"
#include "nltree.h"
#include "lequ.h"
#include "macros.h"
#include "nltree_priv.h"
#include "printout.h"
#include "status.h"


UNUSED static bool rctr_chk_map(Container *ctr, rhp_idx ei, rhp_idx vi_map)
{
   bool valid_vimap = valid_vi(vi_map);
   if (!valid_ei_(ei, rctr_totalm(ctr), __func__)) { return false; }
   RhpContainerData *cdat = (RhpContainerData *)ctr->data;
   
   // TODO: equation might already have been deleted. Maybe base this on rctr_walkallequ
   //if (!cdat->equs[ei]) { return false; }

   if (valid_vimap) {
      if (!valid_vi_(vi_map, rctr_totaln(ctr), __func__)) { return false; }

      // TODO: equation might already have been deleted. Maybe base this on rctr_walkallequ
      if (cdat->equs[ei] && !cmat_chk_varinequ(cdat->last_equ[vi_map], ei)) { return false; }
   }

   EquObjectType object = ctr->equs[ei].object;
   return (valid_vimap && (object == ConeInclusion || object == DefinedMapping)) ||
          (!valid_vimap && object == Mapping);
}


int rctr_equ_addnewvar(Container *ctr, Equ *e, rhp_idx vi, double val)
{
   UNUSED RhpContainerData *cdat = (RhpContainerData *)ctr->data;
   rhp_idx ei = e->idx;

   assert(valid_ei_(ei, cdat->total_m, __func__));
   assert(valid_vi_(vi, cdat->total_n, __func__));
   assert(cdat->equs[ei] || rctr_eq_not_deleted(cdat, ei));

   S_CHECK(equ_add_newlvar(e, vi, val));

   S_CHECK(cmat_equ_add_newlvar(ctr, ei, vi, val));

   return OK;
}

/**
 * @brief Add a variable to an equation, safe version
 *
 * This function updates the container matrix
 * If you know that the variable is new, use rctr_equ_addnewlvar()
 *
 * @ingroup EquSafeEditing
 *
 * @param ctr  the container
 * @param e    the equation
 * @param vi   the variable index
 * @param val  the value
 *
 * @return     the error code
 */
int rctr_equ_addlvar(Container *ctr, Equ *e, rhp_idx vi, double val)
{
   assert(e);

   bool isNL = false;
   S_CHECK(cmat_equ_add_lvar(ctr, e->idx, vi, val, &isNL));

   if (!isNL) {
      if (!e->lequ) {
         A_CHECK(e->lequ, lequ_alloc(1));
      }

      S_CHECK(lequ_add_unique(e->lequ, vi, val));
      assert(isfinite(val));
   } else {
      S_CHECK(nltree_add_var_tree(ctr, e->tree, vi, val));
   }

   return OK;
}


/**
 *  @brief Set an equation to sum_i a_i v_i
 *
 *  @warning this function does not perform any check. If any v_i was already
 *           present in the equation, this results in an inconsistent model
 *
 * @ingroup EquUnsafeEditing 
 *
 *  @param ctr    the container
 *  @param e      the equation to which the variable is added
 *  @param v      the variable to add
 *  @param vals   the value for each scalar variable
 *
 *  @return       The error code
 */
int rctr_equ_addnewlvars(Container *ctr, Equ *e, Avar *v, const double* vals)
{
   S_CHECK(equ_add_newlvars(e, v, vals));
   S_CHECK(cmat_equ_add_newlvars(ctr, e->idx, v, vals));

   return OK;
}

/**
 *  @brief Add  c * sum a_i v_i  to an equation
 *
 *  @warning: no check is made to ensure that the v_i are not already in the
 *  equation. Please use rctr_equ_addlinvars() when this is not the case
 *
 * @ingroup EquUnsafeEditing
 *
 *  @param ctr    the container
 *  @param e      the equation to which the variable is added
 *  @param v      the variable to add
 *  @param vals   the value for each scalar variable
 *  @param coeff  the coefficient to use
 *
 *  @return       The error code
 */
int rctr_equ_addnewlin_coeff(Container *ctr, Equ *e, Avar *v,
                              const double *vals, double coeff)
{
   size_t old_len = e->lequ ? e->lequ->len : 0;

   /* ----------------------------------------------------------------------
    * Inject the values in the linear part, the coeff is dealt with later
    * ---------------------------------------------------------------------- */

   S_CHECK(equ_add_newlvars(e, v, vals));

   Lequ *lequ = e->lequ;
   assert(lequ);

   /*  \TODO(xhub) use BLAS?. Also detec coeff = -1 and just take the opposite */
   if (!(fabs(coeff - 1.) < DBL_EPSILON)) {
      for (size_t i = old_len; i < old_len+v->size; ++i) {
         lequ->coeffs[i] *= coeff;
      }
   }

   S_CHECK(cmat_equ_add_newlvars(ctr, e->idx, v, &lequ->coeffs[old_len]));

   return OK;
}

/**
 *  @brief Add  c * sum a_i v_i  to an equation in the model
 *
 *         This version performs a check whether the v_i are already in the
 *         equation, and ensures that the representation is rightfully updated
 *
 * @ingroup EquSafeEditing
 *
 *  @param ctr    the container
 *  @param e      the equation to which the variable is added
 *  @param v      the variable to add
 *  @param vals   the value for each scalar variable
 *
 *  @return       The error code
 */
int rctr_equ_addlinvars(Container *ctr, Equ *e, Avar *v, const double *vals)
{
   Lequ *lequ = e->lequ;
   assert(lequ);

   /*  TODO(Xhub) this sounds super slow for the quadratic case from Julia:
    *  - the quadratic parts of the equation are added first
    *  - then the linear parts are added, using this function
    */

   /* ----------------------------------------------------------------------
    * Pre-allocate memory to be on the safe side
    * ---------------------------------------------------------------------- */

   S_CHECK(lequ_reserve(lequ, lequ->len + v->size));

   switch (v->type) {
   case EquVar_Compact:
      for (unsigned i = v->start, j = 0; j < v->size; ++i, ++j) {
         bool isNL = false;
         double val = vals[j];
         S_CHECK(cmat_equ_add_lvar(ctr, e->idx, i, val, &isNL));
         if (!isNL) {
            S_CHECK(lequ_add_unique(lequ, i, val));
         } else {
            S_CHECK(nltree_add_var_tree(ctr, e->tree, i, val));
         }
      }
      break;
   case EquVar_List:
      for (size_t i = 0; i < v->size; ++i) {
         bool isNL = false;
         double val = vals[i];
         double vidx = v->list[i];
         S_CHECK(cmat_equ_add_lvar(ctr, e->idx, vidx, val, &isNL));
         if (!isNL) {
            S_CHECK(lequ_add_unique(lequ, vidx, val));
         } else {
            S_CHECK(nltree_add_var_tree(ctr, e->tree, vidx, val));
         }
      }
      break;
   default:
      TO_IMPLEMENT("Block Var");
   }

   return OK;
}

/**
 *  @brief Add  c * sum a_i v_i  to an equation in the model
 *
 *         This version performs a check whether the v_i are already in the
 *         equation, and ensures that the representation is rightfully updated
 *
 * @ingroup EquSafeEditing
 *
 *  @param ctr    the container
 *  @param e      the equation to which the variable is added
 *  @param v      the variable to add
 *  @param vals   the value for each scalar variable
 *
 *  @return       The error code
 */
int rctr_equ_addlinvars_coeff(Container *ctr, Equ *e, Avar *v, const double *vals,
                              double c)
{

   if (fabs(c - 1.) < DBL_EPSILON) { return rctr_equ_addlinvars(ctr, e, v, vals); }

   Lequ *lequ = e->lequ; assert(lequ);

   /*  TODO(Xhub) this sounds super slow for the quadratic case from Julia:
    *  - the quadratic parts of the equation are added first
    *  - then the linear parts are added, using this function
    */

   /* ----------------------------------------------------------------------
    * Pre-allocate memory to be on the safe side
    * ---------------------------------------------------------------------- */

   S_CHECK(lequ_reserve(lequ, lequ->len + v->size));

   switch (v->type) {
   case EquVar_Compact:
      for (unsigned i = v->start, j = 0; j < v->size; ++i, ++j) {
         bool isNL = false;
         double val = c*vals[j];
         S_CHECK(cmat_equ_add_lvar(ctr, e->idx, i, val, &isNL));
         if (!isNL) {
            S_CHECK(lequ_add_unique(lequ, i, val));
         } else {
            S_CHECK(nltree_add_var_tree(ctr, e->tree, i, val));
         }
      }
      break;
   case EquVar_List:
      for (size_t i = 0; i < v->size; ++i) {
         bool isNL = false;
         double val = vals[i];
         double vidx = c*v->list[i];
         S_CHECK(cmat_equ_add_lvar(ctr, e->idx, vidx, val, &isNL));
         if (!isNL) {
            S_CHECK(lequ_add_unique(lequ, vidx, val));
         } else {
            S_CHECK(nltree_add_var_tree(ctr, e->tree, vidx, val));
         }
      }
      break;
   default:
      TO_IMPLEMENT("Block Var");
   }

   return OK;
}


/**
 * @brief Set the equation as constant
 *
 * @param ctr  the container
 * @param ei   the equation
 *
 * @return     the error code
 */
int rctr_set_equascst(Container *ctr, rhp_idx ei)
{
   RhpContainerData *cdat = (RhpContainerData *)ctr->data;
   assert(valid_ei_(ei, cdat->total_m, __func__));

   double cst = equ_get_cst(&ctr->equs[ei]);
   if (isfinite(cst)) {
      A_CHECK(cdat->equs[ei], cmat_cst_equ(ei));
      return OK;
   }

   error("[container] ERROR: equation '%s' is invalid: no variable and "
         "no finite constant value (%e)\n", ctr_printequname(ctr, ei), cst);
   return Error_Inconsistency;
}


/**
 * @brief Fix (if needed) the equation in the container
 *
 * @param ctr  the container
 * @param ei   the equation
 *
 * @return     the error code
 */
int rctr_fix_equ(Container *ctr, rhp_idx ei)
{
   RhpContainerData *cdat = (RhpContainerData *)ctr->data;
   assert(valid_ei_(ei, cdat->total_m, __func__));

   /* ----------------------------------------------------------------------
    * Ensure that the equation is considered as active in the model.
    * If the equation has a constant value, model->eqns[eidx] is NULL and the
    * equation may be considered inactive. Hence, we need to add a dummy
    * model_elt to encode that case.
    * ---------------------------------------------------------------------- */

   if (!cdat->equs[ei]) {
      S_CHECK(rctr_set_equascst(ctr, ei));
   }

   /* ----------------------------------------------------------------------
    * Ensure that if the equation is in a VI relationship, then the associated
    * variable is considered as active
    * TODO(xhub) this is a delicate mix of EMP and model, revisit soon
    * TODO(xhub) look for implicit/shared/explicit
    * ---------------------------------------------------------------------- */

   if (ctr->equmeta && ctr->equmeta[ei].role == EquViFunction) {
      rhp_idx vi = ctr->equmeta[ei].dual;
      assert(valid_vi(vi));

      if (!cdat->vars[vi]) {
         A_CHECK(cdat->vars[vi], cmat_isolated_var_perp_equ(vi, ei));
         cdat_add2free(cdat, cdat->vars[vi]);
         cdat->last_equ[vi] = cdat->vars[vi];

         ctr->varmeta[vi].ppty = VarPerpToViFunction;
         ctr->n++;
      } else {
         assert(!cdat->vars[vi]->placeholder ||
               (valid_vi(cdat->vars[vi]->vi) && valid_ei(cdat->vars[vi]->ei)));
      }
   }

   return OK;
}

static int _equ_add_nl_part_x(Container *ctr, Equ * dst, Equ *src,
                              double coeffp, const rhp_idx *rosetta)
{
   S_CHECK(rctr_getnl(ctr, src));
   NlTree *tree  = src->tree;

   if (tree && tree->root) {
      S_CHECK(rctr_getnl(ctr, dst));
      if (!dst->tree) {
         S_CHECK(nltree_bootstrap(dst, nltree_numnodes(tree), nltree_numchildren(tree)));
      }

      if (rosetta) {
         S_CHECK(rctr_equ_add_nlexpr_full(ctr, dst->tree, tree->root, coeffp, rosetta));
      } else{
         S_CHECK(rctr_equ_add_nlexpr(ctr, dst->idx, tree->root, coeffp));
      }

   }

   return OK;
}
  /* ----------------------------------------------------------------------
   * Add the nonlinear part of src to dst
   * ---------------------------------------------------------------------- */

static int _equ_add_nl_part(Container *ctr, Equ *dst, Equ *src, double coeffp)
{
   return _equ_add_nl_part_x(ctr, dst, src, coeffp, NULL);
}


/* ------------------------------------------------------------------------
 * Add the nonlinear expression of src to dst, possibly with translation
 * ------------------------------------------------------------------------ */
UNUSED static int _equ_add_nl_part_rosetta(Container *ctr, Equ * dst, Equ *src,
                                    const rhp_idx *rosetta)
{
   return _equ_add_nl_part_x(ctr, dst, src, 1., rosetta);
}

/**
 * @brief Add the map f(x) to an equation
 *
 * The source equation either contains f(x) or z =E= f(x), where z is given
 * by the vi_map argument
 *
 * @warning This function does not check whether the linear part of f contains
 * variables already present in the destination equation. If that were true,
 * this results in a bogus equation
 *
 * @ingroup EquUnsafeEditing
 *
 * @param ctr     the container
 * @param edst    the destination equation
 * @param ei      the source equation/map
 * @param vi_map  If valid, the variable defining the map in the equation
 * @param coeff   If finite, the coefficient to apply on the map.
 *
 * @return        The error code
 */
int rctr_equ_add_newmap(Container *ctr, Equ *edst, rhp_idx ei, rhp_idx vi_map, double coeff)
{
   assert(valid_ei_(edst->idx, rctr_totalm(ctr), __func__));
   assert(rctr_chk_map(ctr, ei, vi_map));
   Lequ *lequ = ctr->equs[ei].lequ;

   /* --------------------------------------------------------------------
    * If coeff is not given, it is -coeff(vi_map)
    * -------------------------------------------------------------------- */
   if (!isfinite(coeff)) {
      unsigned pos_dummy;
      S_CHECK(lequ_find(lequ, vi_map, &coeff, &pos_dummy));

      if (pos_dummy == UINT_MAX) {
         error("[container] ERROR: could not find variable '%s' in equation '%s'",
               ctr_printvarname(ctr, vi_map), ctr_printequname(ctr, ei));
         return Error_RuntimeError;
      }

      coeff = -1./coeff;
   }

   /* --------------------------------------------------------------------
    * Deal with the linear part
    * -------------------------------------------------------------------- */

   Lequ *le   = edst->lequ;

   double cst = equ_get_cst(&ctr->equs[ei]);
   equ_add_cst(edst, cst*coeff);

   /* ----------------------------------------------------------------
    * Copy the linear part of F(X)
    *
    * If the coefficient is 1., it is easier
    * ---------------------------------------------------------------- */

   if (fabs(coeff - 1.) < DBL_EPSILON) {
      /* Could use memcpy or BLAS if we know where the argument is */
      for (unsigned l = 0, k = le->len; l < lequ->len; ++l) {
         if (lequ->vis[l] == vi_map || !isfinite(lequ->coeffs[l])) continue;
         assert(!lequ_debug_hasvar(le, lequ->vis[l]));
         le->coeffs[k] = lequ->coeffs[l];
         le->vis[k] = lequ->vis[l];
         k++;
      }
   } else {
      printout(PO_DEBUG, "[PerfWarn] %s :: Suboptimal specification for equation %d\n"
            , __func__, ei);
      if (fabs(coeff) < DBL_EPSILON) {
         error("%s ERROR: the coefficient of variable '%s' in equation '%s' "
               "is too small : %e\n", __func__, ctr_printvarname(ctr, vi_map),
               ctr_printequname(ctr, ei), coeff);
      }

      if (le->max < lequ->len-1 + le->len) {
         S_CHECK(lequ_reserve(le, lequ->len - 1 - le->max + le->len));
      }

      /* \TODO(xhub) option to use BLAS */
      for (unsigned l = 0, k = le->len; l < lequ->len; ++l) {
         /*  Do not use the placeholder variable */
         if (lequ->vis[l] == vi_map || !isfinite(lequ->coeffs[l])) continue;
         assert(!lequ_debug_hasvar(le, lequ->vis[l]));
         le->coeffs[k] = coeff*lequ->coeffs[l];
         le->vis[k] = lequ->vis[l];

         k++;
      }
   }

   le->len += lequ->len - 1;

   /* -----------------------------------------------------------------
    * Now the nonlinear part of F
    * ----------------------------------------------------------------- */

   /* \TODO(xhub) analyze if we really need to copy the tree */
   S_CHECK(_equ_add_nl_part(ctr, edst, &ctr->equs[ei], coeff));

   return OK;
}

/**
 * @brief Augment the content of an equation by another one
 *
 * dst +=  src
 *
 * @ingroup EquSafeEditing
 *
 * @param ctr      the container
 * @param dst      the destination
 * @param src      the equation to copy info the destination
 * @param rosetta  an array to translate variable indices
 *
 * @return         the error code
 */
int rctr_equ_add_equ_x(Container *ctr, Equ * restrict dst, Equ * restrict src,
                       double coeff, const rhp_idx* restrict rosetta)
{
   assert(valid_ei_(dst->idx, rctr_totalm(ctr), __func__));
   /* Do not check src->idx as it could be invalid */
   /* ----------------------------------------------------------------------
    * 3 steps in this function:
    * - add the rhs
    * - add the linear terms
    * - add the nonlinear terms
    *
    * TODO(xhub) add the quadratic terms
    * ---------------------------------------------------------------------- */

   /*  TODO(xhub) CONE SUPPORT */
   assert(dst->object == Mapping || dst->object == EquTypeUnset);

   double cst = equ_get_cst(src);
   equ_add_cst(dst, coeff*cst);

   bool isNL = false;

   /*  TODO(xhub) add a vectorized version */
   size_t lin_len = src->lequ ? src->lequ->len : 0;
   if (lin_len > 0) {

      if (!dst->lequ) {
         A_CHECK(dst->lequ, lequ_alloc(lin_len));
      }

      rhp_idx * restrict svidx = src->lequ->vis;
      double  * restrict svals = src->lequ->coeffs;
      rhp_idx ei_dst = dst->idx;

      for (size_t i = 0; i < lin_len; ++i) {

         rhp_idx vi = rosetta ? rosetta[svidx[i]] : svidx[i];
         double val = coeff * svals[i];
         S_CHECK(cmat_equ_add_lvar(ctr, ei_dst, vi, val, &isNL));

         if (isNL) {
            error("%s :: variable %d is nonlinear in the destination "
                  " equation, but linear in the source", __func__,
                  src->lequ->vis[i]);
            return Error_NotImplemented;
         }

         S_CHECK(lequ_add_unique(dst->lequ, vi, val));
      }
   } 

   return _equ_add_nl_part_x(ctr, dst, src, coeff, rosetta);
}

/**
 * @brief Augment the content of an equation by another one
 *
 * dst +=  src
 *
 * @ingroup EquSafeEditing
 *
 * @param ctr      the container
 * @param dst      the destination
 * @param src      the equation to copy info the destination
 * @param rosetta  an array to translate variable indices
 *
 * @return         the error code
 */
int rctr_equ_add_equ_rosetta(Container *ctr, Equ * restrict dst, Equ * restrict src,
                             const rhp_idx* restrict rosetta)
{
   return rctr_equ_add_equ_x(ctr, dst, src, 1., rosetta);
}

/**
 * @brief Augment the content of an equation by another one
 *
 * dst +=  src
 *
 * @ingroup EquSafeEditing
 *
 * @param ctr      the container
 * @param dst      the destination
 * @param src      the equation to copy info the destination
 * @param rosetta  an array to translate variable indices
 *
 * @return         the error code
 */
int rctr_equ_add_equ_coeff(Container *ctr, Equ * restrict dst, Equ * restrict src,
                           double coeff)
{
   return rctr_equ_add_equ_x(ctr, dst, src, coeff, NULL);
}

/**
 * @brief Augment the content of an equation by another one
 *
 * dst -=  src
 *
 * @ingroup EquSafeEditing
 *
 * @param ctr      the container
 * @param dst      the destination
 * @param src      the equation to copy info the destination
 * @param rosetta  if not NULL, an array to translate variable indices
 *
 * @return         the error code
 */
int rctr_equ_min_equ_rosetta(Container *ctr, Equ *dst, Equ *src, const rhp_idx* rosetta)
{
   return rctr_equ_add_equ_x(ctr, dst, src, -1., rosetta);
}

/**
 * @brief Multiply an equation by a variable and add the result to an equation
 *
 * dst += cst * vi * src
 *
 * @warning This function does not perform any model update! Only for use in fooc computations
 *
 * @ingroup EquSafeEditing
 *
 * @param ctr      the container
 * @param dst      the destination
 * @param src      the equation to copy info the destination
 * @param vi       the variable to multiply
 * @param coeff    the variable to multiply
 *
 * @return     the error code
 */
int rctr_equ_addmulv_equ_coeff(Container *ctr, Equ *dst, Equ *src, rhp_idx vi, double coeff)
{
   /* TODO: we cannot use valid_vi_() because of issues with bilevel problems */
   assert(valid_vi(vi));

   /* ----------------------------------------------------------------------
    * If src is just a constant, then we add vi to the to dst->lequ.
    * Otherwise, we add a bilinear term in the tree
    *
    * TODO(xhub) add the quadratic terms
    * ---------------------------------------------------------------------- */

   double cst = equ_get_cst(src);

   if (!src->tree) {
      S_CHECK(rctr_getnl(ctr, src));
   }

   size_t nb_nodes = src->lequ && src->lequ->len > 0 ? 1 : 0;
   nb_nodes += src->tree && src->tree->root ? 1 : 0;

   if (nb_nodes == 0) {
      if (fabs(cst) > DBL_EPSILON) {
         if (!dst->lequ) {
            A_CHECK(dst->lequ, lequ_alloc(3));
         }

         assert(!lequ_debug_hasvar(dst->lequ, vi));

         /* cmat update in  cmat_sync_lequ */
         S_CHECK(lequ_add(dst->lequ, vi, coeff*cst));
      }
      return OK;
   }

   S_CHECK(rctr_getnl(ctr, dst));

   NlNode **add_node = NULL;

   /* ----------------------------------------------------------------------
    * Get the tree and initialize it if needed
    * ---------------------------------------------------------------------- */

   if (!dst->tree) {
      unsigned est = (src->lequ ? src->lequ->len : 0) + (src->tree ? nltree_numnodes(src->tree) : 0);
      S_CHECK(nltree_bootstrap(dst, est, est));
      add_node = &dst->tree->root;
   } else {
      S_CHECK(nltree_find_add_node(dst->tree, &add_node, ctr->pool, &coeff));
   }

   assert(add_node);
   NlTree *tree = dst->tree;

  /* ----------------------------------------------------------------------
   * TODO: rewrite this by copying the tree
   * ---------------------------------------------------------------------- */

   unsigned offset;
   S_CHECK(nltree_ensure_add_node(tree, add_node, nb_nodes, &offset));

   Lequ *le = src->lequ;
   if (le && le->len > 0) {
      NlNode ** restrict node = &(*add_node)->children[offset++];
  //    S_CHECK(nltree_umin(tree, &node));

      S_CHECK(rctr_nltree_add_bilin(ctr, tree, &node, coeff, vi, IdxNA));
      S_CHECK(rctr_nltree_add_lin_term(ctr, tree, &node, le, IdxNA, 1.));
   }

   /* --------------------------------------------------------------------
    * Deal with the nonlinear part: v * e_NL
    * -------------------------------------------------------------------- */

   if (src->tree && src->tree->root) {
      assert(offset < (*add_node)->children_max);
      NlNode ** restrict node = &(*add_node)->children[offset];
      unsigned old_idx = 0;
      if (!tree->v_list) {
         S_CHECK(nltree_alloc_var_list(tree));
      } else {
         old_idx = tree->v_list->idx;
      }
//      S_CHECK(nltree_umin(tree, &node));

      S_CHECK(rctr_nltree_add_bilin(ctr, tree, &node, coeff, vi, IdxNA));

      /* Fill in v * e_NL  */
      S_CHECK(nlnode_dup(node, src->tree->root, tree));

      /* WARNING: this breaks if we change tree->v_list to be sorted */
      if (tree->v_list->idx > old_idx) {
         unsigned diff = tree->v_list->idx - old_idx;
         Avar v;
         avar_setlist(&v, diff, &tree->v_list->pool[old_idx]);
         S_CHECK(cmat_equ_add_vars(ctr, tree->idx, &v, NULL, true));
      }
   }

   if (fabs(cst) > DBL_EPSILON) {
      NlNode **dummynode = NULL;
      S_CHECK(nltree_add_var(ctr, tree, &dummynode, vi, -cst));
   }

   return OK;
}

/**
 * @brief Multiply an equation by a variable and subtract the content to an equation
 *
 * dst -= vidx * src
 *
 * @warning This function does not perform any model update! Only for use in fooc computations
 *
 * @ingroup EquSafeEditing
 *
 * @param ctr      the container
 * @param dst      the destination
 * @param src      the equation to copy info the destination
 * @param vi       the variable to multiply
 * @param coeff    the variable to multiply
 *
 * @return     the error code
 */
int rctr_equ_submulv_equ_coeff(Container *ctr, Equ *dst, Equ *src, rhp_idx vi, double coeff)
{
   return rctr_equ_addmulv_equ_coeff(ctr, dst, src, vi, -coeff);
}

/**
 * @brief Copy a mapping f(x) at the given node of the nltree of an equation
 *
 * The source equation either contains f(x) or z = f(x).
 * The given node is where the expression should be put.
 *
 * @warning this function assumes that the expression f(x) appears
 * nonlinearly in the destination equation. This implies that
 * the expression is multiplied by an expression with no constant
 * part.
 *
 * @warning the container matrix is not updated
 *
 * @param ctr     The container
 * @param tree    The tree of the destination equation
 * @param node    The node where to copy the mapping
 * @param esrc    The equation holdiong the variable
 * @param vi_map  If valid, the variable denoting the image of the mapping
 * @param coeff   If finite, the coefficient of vi_map in the linear part
 * 
 * @return        the error code
 */
int rctr_nltree_copy_map(Container *ctr, NlTree *tree, NlNode **node, Equ *esrc,
                         rhp_idx vi_map, double coeff)
{
   /* ---------------------------------------------------------------------
    * We have 3 parts to copy:
    * - linear part
    * - constant part
    * - nonlinear part
    * --------------------------------------------------------------------- */

   Lequ *lequ = esrc->lequ;

   if (valid_vi(vi_map) && !isfinite(coeff)) {
      unsigned idx;
      assert(lequ);
      lequ_find(lequ, vi_map, &coeff, &idx);

      if (idx == UINT_MAX) {
         error("ERROR: variable '%s' not found in equation '%s'\n",
               ctr_printvarname(ctr, vi_map),
               ctr_printequname(ctr, esrc->idx));
      }

      assert(isfinite(coeff));
      coeff = -1./coeff;
   }

   assert((valid_vi(vi_map) && isfinite(coeff)) || (!valid_vi(vi_map) && !isfinite(coeff)));

   S_CHECK(nltree_reset_var_list(tree));

   NlNode **addr;
   addr = node;
   /*  Add (sum a_i x_i) */
   if (lequ && lequ->len > 1) {
      S_CHECK(rctr_nltree_add_lin_term(ctr, tree, &addr, lequ, vi_map, coeff));
   }

   /* add constant */
   double cst = equ_get_cst(esrc);
   if (fabs(cst) > DBL_EPSILON) {
      S_CHECK(nltree_add_cst(ctr, tree, &addr, cst*coeff));
   }

   /* --------------------------------------------------------------
    * node now contains a pointer to the node that contains the expression
    * -------------------------------------------------------------- */

   S_CHECK(rctr_getnl(ctr, esrc));

   if (esrc->tree && esrc->tree->root) {

      /*  Get the first empty child */
      if (*addr) {
         S_CHECK(nlnode_next_child(tree, &addr));
      }

      /* Add coeff * ( ... )  */
      /* TODO(xhub) we may end up in a ADD->ADD situation, be careful here */
      S_CHECK(nltree_mul_cst(tree, &addr, ctr->pool, coeff));

      /* \TODO(xhub) optimize w.r.t. umin */
      /* Fill in Fnl_i  */
      S_CHECK(nlnode_dup(addr, esrc->tree->root, tree));
   }

   /* Do a final sanity check: it may happen that the equation consists
    * only of 1 ADD */
   if (*addr && (*addr)->op == NLNODE_ADD) {
      S_CHECK(nltree_check_add(*addr));
   }

   /* keep the model representation consistent */
   Avar v;
   avar_setlist(&v, tree->v_list->idx, tree->v_list->pool);
   S_CHECK(cmat_equ_add_vars(ctr, tree->idx, &v, NULL, true));

   return OK;
}

/**
 * @brief Multiply an equation by a variable and subtract the content to an equation
 *
 * dst -= vidx * src
 *
 * @warning This function does not perform any model update! Only for use in fooc computations
 *
 * @ingroup EquSafeEditing
 *
 * @param ctr   the container
 * @param dst   the destination
 * @param src   the equation to copy info the destination
 * @param vi    the variable to multiply
 *
 * @return     the error code
 */
int rctr_equ_submulv_equ(Container *ctr, Equ *dst, Equ *src, rhp_idx vi)
{
   return rctr_equ_submulv_equ_coeff(ctr, dst, src, vi, 1.);
}
