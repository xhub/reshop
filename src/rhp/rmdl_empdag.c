#include "lequ.h"
#include "mdl_rhp.h"
#include "nltree_priv.h"
#include "reshop_config.h"

#include <assert.h>

#include "rmdl_empdag.h"
#include "ctr_rhp.h"
#include "empdag.h"
#include "empdag_mp.h"
#include "empdag_mpe.h"
#include "mathprgm.h"
#include "mdl.h"
#include "nltree.h"
#include "rhp_model.h"
#include "rmdl_priv.h"
#include "status.h"

/** @file rmdl_empdag.c
*
*   @brief Operations on EMPDAG of ReSHOP models
* */

#ifndef NDEBUG
#define RMDL_DEBUG(s, ...) trace_empdag("[rmdl] DEBUG: " s, __VA_ARGS__)
#else
#define RMDL_DEBUG(s, ...)
#endif

#ifndef NDEBUG
#define EMPDAG_DEBUG(...) trace_empdag("[empdag] DEBUG: " __VA_ARGS__)
#else
#define EMPDAG_DEBUG(s, ...)
#endif

#if 0
NONNULL static
int ccflib_equil_setup_dual_objequ(DfsData *dfsdat, MathPrgm *mp, const DagMpArray *mps_old, NlNode ***child_nodes)
{
   /* ---------------------------------------------------------------------
    * This function prepares the NlTree of the objequ to copy the expression
    * of the primal child nodes.
    * --------------------------------------------------------------------- */
   const VarcArray *Varcs = &mps_old->Varcs[mp->id];
   unsigned n_arcs = Varcs->len;
   ArcVFData * restrict children = Varcs->arr;
   const MathPrgm *mp_old = mps_old->arr[mp->id];

   unsigned n_primal_children = 0;
   for (unsigned i = 0; i < n_arcs; ++i) {
      assert(children[i].child_id < mps_old->len);

      if (mp_getsense(mps_old->arr[children[i].child_id]) != primal_sense) {
         continue;
      }

      n_primal_children++;
   }

   if (n_primal_children != n_arcs) {
      error("[ccflib/equil] ERROR: dual MP(%s) has a mixture of primal and dual"
            "children. This is not supported yet.\n",
            empdag_getmpname(dfsdat->empdag, mp->id));
      return Error_NotImplemented;
   }

   unsigned n_maps = avar_size(mp_old->ccflib.ccf->args);
   unsigned ccflib_size = n_maps + n_primal_children;

   if (ccflib_size == 0) {
      error("[ccflib/equil] ERROR: CCFLIB MP(%s) has neither children or function "
            "arguments", mp_getname(mp_old));
      return Error_EMPIncorrectInput;
   }

   rhp_idx objequ = mp_getobjequ(mp);
   Model *mdl = dfsdat->mdl;
   NlNode * add_node;
   double coeff = 1.;
   unsigned offset;

  /* ----------------------------------------------------------------------
   * Get an ADD node, and for each EMPDAG child, add       sum(j, Bij vj) * (...)
   * ---------------------------------------------------------------------- */

   S_CHECK(nltree_get_add_node(mdl, objequ, ccflib_size, &add_node, &offset, &coeff));
   NlTree *tree = mdl->ctr.equs[objequ].tree;

   dfsdat->dual_objequ_dat.equ = &mdl->ctr.equs[objequ];
   dfsdat->dual_objequ_dat.tree = tree;
   dfsdat->dual_objequ_dat.mul_term_is_lin = true;

   rhp_idx y_start = mp->vars.arr[0];

   for (unsigned i = 0, j = offset; i < n_arcs; ++i, ++j) {

     /* ----------------------------------------------------------------------
      * Get i-th row of B^T, that is the i-th column of B
      * ---------------------------------------------------------------------- */

      unsigned si, len;
      double sv;
      unsigned *idxs;
      double *vals;

      S_CHECK(rhpmat_col(B, i, &si, &sv, &len, &idxs, &vals));

      NlNode **addr = &add_node->children[j];

      /* ------------------------------------------------------------------
       * If the row of B^T has only 1 elt, just add  " Bji vj * (...) "
       * Otherwise, we add  ( sum(j, Bij vj) * (...) )
       * ------------------------------------------------------------------ */

      if (len == 1) {
         S_CHECK(rctr_nltree_add_bilin(&mdl->ctr, tree, &addr, coeff, idxs[0]+y_start, IdxNA));
      } else {

         S_CHECK(rctr_nltree_add_bilin(&mdl->ctr, tree, &addr, coeff, IdxNA, IdxNA));
         Lequ lequ = {.len = len, .max = len, .coeffs = vals, .vis = (rhp_idx*)idxs};
         S_CHECK(rctr_nltree_add_lin_term(&mdl->ctr, tree, &addr, &lequ, IdxNA, coeff));

      }

      child_nodes[i] = addr;
   }

  /* ----------------------------------------------------------------------
   * For the function part, just add sum(i, y[i] * ( sum(j, B(i, j) * F(j) )))
   * ---------------------------------------------------------------------- */


   return OK;
}
#endif

/** Copy expression types */
__extension__ typedef enum ENUM_U8 {
   CopyExprNone,
   CopyExprInvalid,
   CopyExprSingle,
   CopyExprMultiple,   /**< */
} CopyExprType;

typedef struct copy_expr_single {
   rhp_idx ei;
   double coeff_path;
   NlNode **nlnode_addr;
} CopyExprSingleDat;

typedef struct copy_expr_multiple {
   unsigned len;
   unsigned max;
   CopyExprSingleDat *arr;
} CopyExprMultipleDat;

#define RHP_LOCAL_SCOPE
#define RHP_ARRAY_PREFIX copyexprs
#define RHP_ARRAY_TYPE copy_expr_multiple
#define RHP_ELT_TYPE struct copy_expr_single
#define RHP_ELT_INVALID ((CopyExprSingleDat){.ei = IdxNA, .coeff_path = SNAN, .nlnode_addr = NULL})
#include "array_generic.inc"

typedef struct {
   CopyExprType type;
   ArcVFData rarc;
   union {
      CopyExprSingleDat single;
      CopyExprMultipleDat multiple;
   };
} CopyExprDat;


#define RHP_LOCAL_SCOPE
#define RHP_ARRAY_PREFIX cpy_expr
#define RHP_ELT_TYPE CopyExprDat
#define RHP_ARRAY_TYPE CopyExprDatArray
#define RHP_ELT_INVALID (CopyExprDat){.type = CopyExprInvalid}
#define RHP_DEFINE_STRUCT
#include "array_generic.inc"


/**
 * @brief Initialize the copy expression data from arvVFData
 *
 * @param cpydat  the copy expression to initialize
 *
 * @return        the error code
 */
NONNULL static inline int arcVFdata2copyexprdat(CopyExprDat *cpydat)
{
   assert(cpydat->type == CopyExprNone);
   ArcVFData *rarc = &cpydat->rarc;

   switch (rarc->type) {

   case ArcVFBasic:
      assert(isfinite(rarc->basic_dat.cst));
      cpydat->single.ei = rarc->basic_dat.ei;
      cpydat->single.coeff_path = 1.;
 
      cpydat->type = CopyExprSingle;
      trace_empdag("[empdag/contraction] init data with equation #%u and coeff = %e\n",
                   cpydat->single.ei, cpydat->single.coeff_path);
      break;

   case ArcVFMultipleBasic: {
      unsigned len = rarc->basics_dat.len;
      CopyExprMultipleDat *bdats = &cpydat->multiple;
      S_CHECK(copyexprs_resize(bdats, len));
      ArcVFBasicData *arr = rarc->basics_dat.list;

      for (unsigned i = 0; i < len; ++i) {
            bdats->arr[i] = (CopyExprSingleDat){.ei = arr[i].ei, .coeff_path = arr[i].cst };
            bdats->arr[i].nlnode_addr = NULL;
      }

      cpydat->type = CopyExprMultiple;
      break;
   }

   default:
      return error_runtime();
   }

   return OK;
}

NONNULL
static int copy_expr_arc_parent_basic(Model *mdl, ArcVFBasicData *arcdat, Equ *eobj,
                                      rhp_idx objvar, CopyExprDat *cpydat,
                                      CopyExprDat *cpydat_child)
{
   switch (cpydat->type) {

   case CopyExprNone:
      cpydat_child->single.ei = arcdat->ei;
      cpydat_child->single.nlnode_addr = NULL;
      cpydat_child->single.coeff_path = arcdat->cst;

      trace_empdag("[empdag/contraction] init data with equation '%s' and coeff = %e\n",
                   mdl_printequname(mdl, arcdat->ei), cpydat_child->single.coeff_path);
      return OK;

   case CopyExprSingle: {

      CopyExprSingleDat *cpydat_single = &cpydat->single;
      double coeff_path;
      rhp_idx vi = arcdat->vi, ei = arcdat->ei, ei_dst;
      Equ *e;
      NlNode **nlnode_addr;

      /* If the VF is in the objective of the parent MP, then use the current path equation */
      if (mdl->ctr.equmeta[ei].role == EquObjective) {
        ei_dst = cpydat_single->ei;
        nlnode_addr = cpydat_single->nlnode_addr;
        coeff_path = arcdat->cst * cpydat_single->coeff_path;
     } else {
        ei_dst = ei;
        nlnode_addr = NULL;
        coeff_path = arcdat->cst;
     }

      assert(isfinite(coeff_path));

      S_CHECK(rmdl_get_equation_modifiable(mdl, ei_dst, &e));

      cpydat_child->single.ei = e->idx;

      trace_empdag("[empdag/contraction] copying objective equation '%s' into '%s'\n\tcoeff = %e = %e * %e; nlnode_addr = %p\n",
                   mdl_printequname(mdl, eobj->idx), mdl_printequname2(mdl, e->idx),
                   coeff_path, arcdat->cst, cpydat_single->coeff_path, (void*)nlnode_addr);

      Container *ctr = &mdl->ctr;
      NlTree *dtree = e->tree;

      unsigned offset, s = valid_vi(vi) ? 1 : equ_get_nladd_estimate(eobj);

     /* ----------------------------------------------------------------------
      * If we already have an NLnode, then we just copy the eobj there
      * ---------------------------------------------------------------------- */

      if (nlnode_addr) {

         assert(dtree && dtree->root);

         S_CHECK(nltree_ensure_add_node_inplace(dtree, nlnode_addr, s, &offset));

         NlNode *nlnode = *nlnode_addr;
         if (!nlnode) {
            errormsg("[empdag/contraction] ERROR: couldn't get a nlnode\n");
            return Error_RuntimeError;
         }

         NlNode **addr = &nlnode->children[offset];

        /* ----------------------------------------------------------------------
         * Add either cst * vi * (...) or cst * (...)
         * ---------------------------------------------------------------------- */

         if (valid_vi(vi)) {
            S_CHECK(rctr_nltree_add_bilin(ctr, dtree, &addr, coeff_path, vi, IdxNA));
         } else if (coeff_path != 1.) {
            S_CHECK(rctr_nltree_mul_cst(ctr, dtree, &addr, coeff_path));
         }

         /* Save node as starting point for future */
         cpydat_child->single.nlnode_addr = addr;

         S_CHECK(rctr_nltree_copy_map(ctr, dtree, addr, eobj, objvar, 1.));

         /* The constant is already propagated through the tree */
         cpydat_child->single.coeff_path = 1.;

         return OK;
      }

  /* ----------------------------------------------------------------------
   * In this case, we need to
   * - check whether we have just a constant equation. If yes, we add vi * cst
   * - Otherwise add a nonlinear 
   * ---------------------------------------------------------------------- */

      if (valid_vi(vi)) {

         unsigned lequ_len = eobj->lequ->len;
         double ecst = equ_get_cst(eobj);

         if (valid_vi(objvar)) { lequ_len--; assert(lequ_debug_hasvar(eobj->lequ, objvar)); }

         S_CHECK(rctr_getnl(&mdl->ctr, eobj));

         /* If the equation is just a constant, just add the term vi * cst */
         if (lequ_len == 0 && (!eobj->tree || !eobj->tree->root)) {
            assert(valid_ei_(eobj->idx, mdl_nequs_total(mdl), __func__));
            assert(((RhpContainerData*)mdl->ctr.data)->cmat.equs[eobj->idx]->type == CMatEltCstEqu);
            cpydat_child->single.coeff_path = coeff_path;
            return rctr_equ_addlvar(&mdl->ctr, e, vi, coeff_path*ecst);
         }

         if (!dtree) {
            S_CHECK(nltree_bootstrap(e, lequ_len+1, lequ_len+1));
            dtree = e->tree;
         }

         NlNode *root = dtree->root;
         S_CHECK(nltree_ensure_add_node_inplace(dtree, &root, s, &offset));
         NlNode **addr = &root->children[offset];

         if (valid_vi(vi)) {
            S_CHECK(rctr_nltree_add_bilin(&mdl->ctr, dtree, &addr, coeff_path, vi, IdxNA));
         } else if (coeff_path != 1.) {
            S_CHECK(rctr_nltree_mul_cst(ctr, dtree, &addr, coeff_path));
         }

         S_CHECK(rctr_nltree_copy_map(ctr, dtree, addr, eobj, objvar, 1.));
         cpydat_child->single.nlnode_addr = addr;

         /* The constant is already propagated through the tree */
         cpydat_child->single.coeff_path = 1.;

      } else { /* vi not valid */

         if (valid_vi(objvar)) {

            S_CHECK(rctr_equ_add_map(ctr, e, eobj->idx, objvar, coeff_path));

            /* NOTE: we do not need to propagate the constant from objvar */

         } else {
            S_CHECK(rctr_equ_add_equ_coeff(ctr, e, eobj, coeff_path));
         }

         /* Propagate the constant */
         cpydat_child->single.nlnode_addr = NULL;
         cpydat_child->single.coeff_path = coeff_path;
      }
      break;
   }

   case CopyExprMultiple: {
         TO_IMPLEMENT("multiple copies of the objequ");
   }

   default:
      return error_runtime();
   }

   return OK;
}

static inline int copy_expr_arc_parent(Model *mdl, MathPrgm * restrict mp_child,
                                       CopyExprDat * restrict cpydat,
                                       CopyExprDat * restrict cpydat_child)
{
   ArcVFData *rarc = &cpydat->rarc;

   /* *This is true at the subtree root node */
   if (rarc->type == ArcVFUnset) {
      assert(cpydat->type == CopyExprNone);
      EMPDAG_DEBUG("MP(%s) has no reverse arc\n", mp_getname(mp_child));
      return OK;
   }

   rhp_idx objequ = mp_getobjequ(mp_child), objvar = mp_getobjvar(mp_child);

  /* ----------------------------------------------------------------------
   * If objequ does not exists, which happens when we have a trivial MP with
   * just a sum of valFn, just skip.
   * ---------------------------------------------------------------------- */

   if (objequ == IdxNA) {
      assert(mp_child->mdl->empinfo.empdag.mps.Varcs[mp_child->id].len > 0);
      cpydat_child->single.coeff_path = 1.;
      return OK;
   }

   assert(valid_ei_(objequ, mdl_nequs_total(mdl), __func__));

   /* Set objvar to be evaluated via objequ */
   if (valid_vi(objvar)) {
      S_CHECK(rctr_add_eval_equvar(&mdl->ctr, objequ, objvar));
   }

   Equ *eobj = &mdl->ctr.equs[objequ];
   ArcVFType type = rarc->type;

   switch (type) {

   case ArcVFBasic:

      cpydat_child->type = CopyExprSingle;
      EMPDAG_DEBUG("MP(%s) has a single parent\n", mp_getname(mp_child));
      S_CHECK(copy_expr_arc_parent_basic(mdl, &rarc->basic_dat, eobj, objvar,
                                         cpydat, cpydat_child));
      break;

   case ArcVFMultipleBasic: {

      cpydat_child->type = CopyExprMultiple;
      unsigned len = rarc->basics_dat.len;
      ArcVFBasicData *arcb_arr = rarc->basics_dat.list;

      EMPDAG_DEBUG("MP(%s) has %u parents\n", mp_getname(mp_child), len);

      for (unsigned i = 0; i < len; ++i) {
         S_CHECK(copy_expr_arc_parent_basic(mdl, &arcb_arr[i], eobj, objvar,
                                            cpydat, cpydat_child))
      }
      break;
   }

   default:
      error("[empdag/contraction] ERROR: support for arc type %s not implemented", arcVFType2str(type));
      return Error_NotImplemented;
   }

   S_CHECK(rmdl_equ_rm(mdl, objequ));

   return OK;
}

void vf_contractions_init(VFContractions *contractions)
{
   contractions->need_sorting = true;
   contractions->max_size_subdag = 0;
   contractions->num_newequs = 0;

   mpidarray_init(&contractions->VFdag_starts);
   mpidarray_init(&contractions->VFdag_mpids);
   contractions->mpbig_vars = NULL;
   contractions->mpbig_cons = NULL;
}

void vf_contractions_empty(VFContractions *contractions)
{
   free(contractions->mpbig_vars);
   free(contractions->mpbig_cons);
   mpidarray_empty(&contractions->VFdag_starts);
   mpidarray_empty(&contractions->VFdag_mpids);
}

int rmdl_contract_subtrees(Model *mdl, VFContractions *contractions)
{
  /* ----------------------------------------------------------------------
   * 1. Prepare the reformulation:
   *   - allocate num_newequs
   *
   * 2. Iterate over the subDAG:
   *   - for each starting MP:
   *     x All constraints and decision variables are now attached to the starting MP
   *     x Inject the obj(equ|var), weighted by arc data, into the equations
   *
   * 3. For each child MP, we 
   * ---------------------------------------------------------------------- */

   EmpDag *empdag = &mdl->empinfo.empdag;

   DagMpArray *mps = &empdag->mps;

   MpIdArray *VFdag_starts = &contractions->VFdag_starts;
   MpIdArray *VFdag_mpids = &contractions->VFdag_mpids;
   unsigned num_mp_big = VFdag_starts->len;
   S_CHECK(dagmp_array_reserve(&empdag->mps, num_mp_big));

   unsigned num_newequs = contractions->num_newequs;
   if (num_newequs > 0) {
      S_CHECK(rctr_reserve_equs(&mdl->ctr, num_newequs));
   }

   unsigned *mpbig_vars = contractions->mpbig_vars;
   unsigned *mpbig_cons = contractions->mpbig_cons;

   bool mp_reserve_equvar = mpbig_vars && mpbig_cons;

   mpid_t *vfdag_root_arr = VFdag_starts->arr, mpid_root;

   daguid_t uid_root = empdag->uid_root;

   if (uidisMP(uid_root)) {
      mpid_root = uid2id(uid_root);
   } else {
      mpid_root = MpId_NA;
   }

   CopyExprDatArray cpy_expr;
   cpy_expr_resize(&cpy_expr, contractions->max_size_subdag+1);

   VarMeta *vmeta = mdl->ctr.varmeta;
   EquMeta *emeta = mdl->ctr.equmeta;

   trace_empdag("[empdag/contract] Contracting %u subtrees in %s model '%.*s' #%u\n",
                num_mp_big, mdl_fmtargs(mdl));

   for (unsigned i = 0; i < num_mp_big; ++i) {

      mpid_t mpid_subtree_root = vfdag_root_arr[i];

      trace_empdag("[empdag/contract] Contracting subtree starting at MP(%s)\n",
                   empdag_getmpname(empdag, mpid_subtree_root));

      /* Reset the working arrays */
      VFdag_mpids->len = 0;
      mpidarray_add(VFdag_mpids, mpid_subtree_root);

      cpy_expr.len = 1;
      CopyExprDat *root_cpy_expr = &cpy_expr.arr[0];
      root_cpy_expr->type = CopyExprNone;
      memset(&root_cpy_expr->rarc, 0, sizeof(root_cpy_expr->rarc));
      root_cpy_expr->single.coeff_path = 1.;

      MathPrgm *mp_subtree_root = mps->arr[mpid_subtree_root];
      if (!mp_isopt(mp_subtree_root)) {
         error("[empdag] ERROR: cannot contract with root MP(%s) of type %s\n",
               empdag_getmpname(empdag, mpid_subtree_root), mp_gettypestr(mp_subtree_root));
      }

      mpid_t mpid_big;
      S_CHECK(empdag_addmpnamed(empdag, mp_getsense(mp_subtree_root),
                                empdag_getmpname(empdag, mpid_subtree_root), &mpid_big));


      if (valid_mpid(mpid_root) && mpid_subtree_root == mpid_root) {
         S_CHECK(empdag_setroot(empdag, mpid2uid(mpid_big)));
      }

     /* ----------------------------------------------------------------------
      * Create the big MP 
      * ---------------------------------------------------------------------- */

      MathPrgm *mp_big;
      S_CHECK(empdag_getmpbyid(empdag, mpid_big, &mp_big));

      if (mp_reserve_equvar) {
         S_CHECK(mp_reserve(mp_big, mpbig_vars[i], mpbig_cons[i]));
      }

      IdxArray * restrict vars_bigmp = &mp_big->vars;
      IdxArray * restrict equs_bigmp = &mp_big->equs;
      S_CHECK(rhp_idx_copy(vars_bigmp, &mp_subtree_root->vars));
      S_CHECK(rhp_idx_copy(equs_bigmp, &mp_subtree_root->equs));
      assert(rhp_idx_chksorted(vars_bigmp));
      assert(rhp_idx_chksorted(equs_bigmp));

      /* Most of the opt data is the same, minus the objequ */
      memcpy(&mp_big->opt, &mp_subtree_root->opt, sizeof(struct mp_opt));

      /* Hide mp_root_src */
      mp_hide(mp_subtree_root);

      S_CHECK(daguidarray_rmnofail(&empdag->roots, mpid2uid(mp_subtree_root->id)));

      rhp_idx objvar_big = mp_getobjvar(mp_big);
      rhp_idx objequ_big = mp_getobjequ(mp_big);

      if (valid_vi(objvar_big)) {
         vmeta[objvar_big].mp_id = mpid_big;
      }

      if (valid_ei(objequ_big)) {
         emeta[objequ_big].mp_id = mpid_big;
      }

      if (valid_vi(objvar_big) && valid_ei(objequ_big)) {
         // HACK: investigate is something cleaner can be done ...
         // This is needed since the replacement of the objequ in
         // rmdl_equ_dup_except wil update the MP owning the objequ ...

         S_CHECK(rmdl_mp_objequ2objfun(mdl, mp_big, objvar_big, objequ_big));
      }

      /* TODO: what does this do? */
      S_CHECK(empdag_substitute_mp_parents_arcs(empdag, mpid_subtree_root, mpid_big));

      /* -------------------------------------------------------------------
       * MAIN LOOP for the iteration. 
       * This is not a DFS or BFS iteration. We do not need to be particularily
       * picky about the order as we store the relevant data in cpy_expr.
       * As long as it is in sync with VFdag_mpids, it is all good
       * ------------------------------------------------------------------- */

      for (unsigned j = 0; j < VFdag_mpids->len; ++j) {

         mpid_t mpid_ = VFdag_mpids->arr[j];
         assert(mpid_ < mps->len);

         /* ----------------------------------------------------------------
          * If a MP has been removed, this implies that it has been contracted
          * - We would need to link the contracted 
          * ---------------------------------------------------------------- */
         MathPrgm *mp = mps->arr[mpid_];

         if (!mp) {
            TO_IMPLEMENT("Partial subtree contraction");
         }

         /* Now copy the MP objequ at the right spots */
         CopyExprDat *cpydat = &cpy_expr.arr[j];
         assert(cpydat->rarc.type == ArcVFUnset || mpid_ == cpydat->rarc.mpid_child);

         CopyExprDat cpydat_child_template = {.type = CopyExprNone, .rarc = {0}};
         S_CHECK(copy_expr_arc_parent(mdl, mp, cpydat, &cpydat_child_template));

         /* Need to get the type of arc*/
         VarcArray *varcs = &mps->Varcs[mpid_];
         ArcVFData * restrict varcs_arr = varcs->arr;

         for (unsigned lenk = varcs->len, k = 0; k < lenk; ++k) {

            ArcVFData *arc = &varcs_arr[k];
            cpydat_child_template.rarc = *arc;

            unsigned arc_cons = arcVF_getnumcons(arc, mdl);

            if (arc_cons == UINT_MAX) {
               error("[empdag/contract] ERROR while processing arcVF %u of MP(%s). "
                     "Please open a bug report\n", k, empdag_getmpname(empdag, mpid_));
               return Error_RuntimeError;
            }

            mpid_t mpid_child = arc->mpid_child;
            MathPrgm *mp_child = mps->arr[mpid_child];

            trace_empdag("[empdag/contract] Adding MP(%s) to the list of nodes to visit.\n",
                         empdag_getmpname(empdag, mpid_child));

            mpidarray_add(VFdag_mpids, mpid_child);

            cpy_expr.arr[cpy_expr.len++] = cpydat_child_template;
            assert(cpy_expr.len <= cpy_expr.max);
            assert(cpy_expr.len == VFdag_mpids->len);


        /* --------------------------------------------------------------
         * We need to update the template if we are at the root.
         * -------------------------------------------------------------- */

            if (cpydat_child_template.type == CopyExprNone) {

               CopyExprDat *cpydat_child = &cpy_expr.arr[cpy_expr.len-1];

               if (arcVF_has_abstract_objfunc(arc)) {

                  if (!valid_ei(objequ_big)) {
                     S_CHECK(mp_ensure_objfunc(mp_big, &objequ_big));
                  }

                  arcVF_subei(&cpydat_child->rarc, IdxObjFunc, objequ_big);
               }

               S_CHECK(arcVFdata2copyexprdat(cpydat_child));
            }

            rhp_idx objequ = mp_getobjequ(mp_child);
            rhp_idx objvar = mp_getobjvar(mp_child);

            if (valid_ei(objequ)) {
               S_CHECK(rhp_idx_extend_except_sorted(equs_bigmp, &mp_child->equs, objequ));
            } else {
               S_CHECK(rhp_idx_extend_sorted(equs_bigmp, &mp_child->equs));
            }

            if (valid_vi(objvar)) {
               S_CHECK(rhp_idx_extend_except_sorted(vars_bigmp, &mp_child->vars, objvar));
            } else {
               S_CHECK(rhp_idx_extend_sorted(vars_bigmp, &mp_child->vars));
            }

         }
      }

     /* ----------------------------------------------------------------------
      * Free MP in the contracted subtree
      * ---------------------------------------------------------------------- */

      mpid_t * restrict mpid_arr = VFdag_mpids->arr;
      MathPrgm ** restrict mp_arr = empdag->mps.arr;
      mpid_t mpid_max = empdag->mps.len;

      for (unsigned j = 0, lenj = VFdag_mpids->len; j < lenj; ++j) {
         mpid_t mpid_ = mpid_arr[j];

         if (mpid_ >= mpid_max) {
            error("[empdag] ERROR: MPID %u larger than maximum %u\n", mpid_,
                  mpid_max);
            return Error_RuntimeError;
         }

         mp_free(mp_arr[mpid_]);
         mp_arr[mpid_] = NULL;
      }

     /* ----------------------------------------------------------------------
      * Re-assign the variables and equations in the new big MP
      * ---------------------------------------------------------------------- */

      rhp_idx *vars_bigmp_arr = vars_bigmp->arr;
      for (unsigned j = 0, len = vars_bigmp->len; j < len; ++j) {
         rhp_idx vi = vars_bigmp_arr[j];

         if (vmeta[vi].mp_id == mpid_big) { continue; };

         assert(vmeta[vi].mp_id < mpid_max && !mp_arr[vmeta[vi].mp_id]);

         vmeta[vi].mp_id = mpid_big;
      }

      rhp_idx *equs_bigmp_arr = equs_bigmp->arr;
      for (unsigned j = 0, len = equs_bigmp->len; j < len; ++j) {
         rhp_idx ei = equs_bigmp_arr[j];

         if (emeta[ei].mp_id == mpid_big) { continue; };

         assert(emeta[ei].mp_id < mpid_max && !mp_arr[emeta[ei].mp_id]);

         emeta[ei].mp_id = mpid_big;
      }
   }

   cpy_expr_free(&cpy_expr);

   S_CHECK(dagmp_array_trimmem(mps));

   return OK;
}

static int rmdl_contract_analyze(Model *mdl, VFContractions *contractions)
{
   int status = OK;

   const EmpDag * empdag_src = &mdl->empinfo.empdag;
   unsigned num_mp = empdag_src->mps.len;

  /* ----------------------------------------------------------------------
   * num_newequs is the number of new equations
   * ---------------------------------------------------------------------- */

   unsigned num_objequs = 0, num_nodes = num_mp + empdag_src->nashs.len;
   unsigned num_newequs = 0, max_size_subdag = 0;
   /* XXX: unclear why this is needed */
   rhp_idx *objequs = NULL ;
   bool *seen_uid = NULL;

   DagUidArray daguids; /* This is the */
   daguidarray_init(&daguids);

   MpIdArray * restrict VFdag_starts = &contractions->VFdag_starts;
   MpIdArray * restrict VFdag_mpids = &contractions->VFdag_mpids;

  /* ----------------------------------------------------------------------
   * Allocate the data:
   * - For the DFS exploration
   * - For the new EMPDAG:
   *    x All the nash nodes will be copied
   *    x The number of MPs is no greater than the existing ones, so we can
   *      reserve that quantity
   * ---------------------------------------------------------------------- */

   MALLOC_EXIT(objequs, rhp_idx, num_mp);
   MALLOC_EXIT(seen_uid, bool, num_nodes);
   memset(seen_uid, 0, num_nodes*sizeof(bool));
   MALLOC_EXIT(contractions->mpbig_cons, unsigned, num_mp);
   MALLOC_EXIT(contractions->mpbig_vars, unsigned, num_mp);
   unsigned * restrict mpbig_cons = contractions->mpbig_cons;
   unsigned * restrict mpbig_vars = contractions->mpbig_vars;

   S_CHECK_EXIT(mpidarray_reserve(VFdag_starts, num_mp));
   S_CHECK_EXIT(mpidarray_reserve(VFdag_mpids, num_mp));
   S_CHECK_EXIT(daguidarray_reserve(&daguids, num_nodes));

  /* ----------------------------------------------------------------------
   * Start of the BFS-like exploration
   * ---------------------------------------------------------------------- */

   assert(valid_uid(empdag_src->uid_root));
   S_CHECK_EXIT(daguidarray_add(&daguids, empdag_src->uid_root));

   const DagMpArray *mps_src = &empdag_src->mps;
   DBGUSED const DagNashArray *nashs_src = &empdag_src->nashs;

   for (unsigned i = 0; i < daguids.len; ++i) {

      daguid_t node = daguids.arr[i];

      /* --------------------------------------------------------------------
       * If we have a Nash node, just add the children
       * -------------------------------------------------------------------- */

      if (uidisNash(node)) {

         nashid_t nashid = uid2id(node); assert(nashid < nashs_src->len);
         unsigned nidx = num_mp + nashid;

         if (seen_uid[nidx]) { continue; }
         seen_uid[nidx] = true;

         DagUidArray *c = &nashs_src->arcs[nashid];
         memcpy(&daguids.arr[daguids.len], c->arr, c->len * sizeof(daguid_t));
         daguids.len += c->len;
         continue;
      }

      mpid_t mpid = uid2id(node); assert(mpid < mps_src->len);

      if (seen_uid[mpid]) { continue; }
      seen_uid[mpid] = true;

     /* ----------------------------------------------------------------------
      * We potentially have the start of a VFtree
      * ---------------------------------------------------------------------- */

      DagUidArray *c = &mps_src->Carcs[mpid];
      memcpy(&daguids.arr[daguids.len], c->arr, c->len * sizeof(daguid_t));
      daguids.len += c->len;

      VarcArray * restrict varcs_src = &mps_src->Varcs[mpid];

      // XXX what is the purpose of start_has_objequ
      bool start_has_objequ = false;

      for (unsigned j = 0, len = varcs_src->len; j < len; ++j) {
         if (!start_has_objequ) { 
            start_has_objequ = arcVF_in_objfunc(&varcs_src->arr[j], mdl);
         }

         mpidarray_add(VFdag_mpids, varcs_src->arr[j].mpid_child);
      }

      if (VFdag_mpids->len == 0) { continue; }

     /* ---------------------------------------------------------------------
      * Explore the VFdag: add mpid to starting nodes.
      * We iterate through all the MP in the subdag.
      * - If we find a Carc, we stop and bactrack
      * - If we go over the whole subdag
      * --------------------------------------------------------------------- */
      mpidarray_add(VFdag_starts, mpid);

      unsigned num_objequs_old = num_objequs;
      unsigned num_newequ_subdag = 0;
      unsigned mp_cons = 0, mp_vars = 0;

      for (unsigned j = 0; j < VFdag_mpids->len; ++j) {
         mpid_t mpid_ = VFdag_mpids->arr[j];
         assert(mpid_ < mps_src->len);

         if (mps_src->Carcs[mpid_].len > 0) {
            c = &mps_src->Carcs[mpid_];
            memcpy(&daguids.arr[daguids.len], c->arr, c->len * sizeof(daguid_t));
            daguids.len += c->len;

            /* We stop the VFdag loo and remove the start MPID */
            VFdag_mpids->len = 0;
            num_objequs = num_objequs_old;
            VFdag_starts->len--;
            continue;
         }

         /* -----------------------------------------------------------------
          * TODO: If we have a DAG, some additional checks might be needed
          * Just fail here to be safe.
          *
          * Most probably, it would be sufficient to ensure that any parent
          * of a give node is in the VFdag. 
          * ----------------------------------------------------------------- */

        DagUidArray *rarcs = &mps_src->rarcs[mpid_];
         if (rarcs->len > 1) {
            TO_IMPLEMENT_EXIT("reduction with a VFdag (only VFtree are supported for now)");
         }

         if (seen_uid[mpid_]) {
            error("[model] ERROR: %s model '%.*s' #%u has a true DAG as EMPDAG\n",
                  mdl_fmtargs(mdl));
            status = Error_NotImplemented;
            goto _exit;
         }

         seen_uid[mpid_] = true;

         MathPrgm *mp_ = mps_src->arr[mpid_];

         rhp_idx objequ = mp_getobjequ(mp_);

         if (valid_ei(objequ)) {
            objequs[num_objequs++] = objequ;
         }

         mp_cons += mp_getnumcons(mp_);

         mp_vars += mp_getnumoptvars(mp_);

         varcs_src = &mps_src->Varcs[mpid_];

         for (unsigned k = 0, len = varcs_src->len; k < len; ++k) {
            ArcVFData *arc = &varcs_src->arr[k];

            /* It is just sufficient to know the number of constraints */
            unsigned arc_num_cons = arcVF_getnumcons(arc, mdl);

            if (arc_num_cons == UINT_MAX) {
               error("[empdag/contract] ERROR while procesing arcVF %u of MP(%s). "
                     "Please file a bug report\n", k, empdag_getmpname(empdag_src, mpid_));
               return Error_RuntimeError;
            }

            num_newequ_subdag += arc_num_cons;

            mpidarray_add(VFdag_mpids, arc->mpid_child);

         }
      }

     /* ---------------------------------------------------------------------
      * We were successful. Just reset the VFdag_mpids array
      * The minimum number of equation to add is 1
      * --------------------------------------------------------------------- */

      num_newequs += (start_has_objequ ? 1 : 0 ) + num_newequ_subdag;
      max_size_subdag = VFdag_mpids->len > max_size_subdag ? VFdag_mpids->len : max_size_subdag;
      VFdag_mpids->len = 0;

      unsigned mpbig_idx = VFdag_starts->len-1;
      mpbig_cons[mpbig_idx] = mp_cons;
      mpbig_vars[mpbig_idx] = mp_vars;
   }

  /* ----------------------------------------------------------------------
   * Now reformulate:
   * ---------------------------------------------------------------------- */

   contractions->num_newequs = num_newequs;
   contractions->max_size_subdag = max_size_subdag;

_exit:
   FREE(objequs);
   free(seen_uid);
   mpidarray_empty(&daguids);

   return status;
}

/**
 * @brief Contract the empdag by merging MPs with the same sense along arcVFs to a single MP
 *
 * @param mdl_src  the original model
 * @param mdl_dst  the model with a contracted EMPDAG
 *
 * @return         the error code
 */
int rmdl_contract_along_Vpaths(Model *mdl_src, Model **mdl_dst)
{
   int status = OK;

   unsigned num_mp = mdl_src->empinfo.empdag.mps.len;

   if (num_mp <= 1) {
      error("[model] ERROR with %s model '%.*s' #%u: graph contraction "
            "meaningless with no EMPDAG", mdl_fmtargs(mdl_src));
   }

   char *mdlname;
   IO_PRINT(asprintf(&mdlname, "Contracted version of model '%s'", mdl_getname(mdl_src)));

   //TODO if mdl_Src is not rhp, copy
   S_CHECK(rmdl_get_editable_mdl(mdl_src, mdl_dst, mdlname));

   FREE(mdlname);

   Model *mdl_dst_ = *mdl_dst;

  /* ----------------------------------------------------------------------
   * HACK: Finish this
   * This 
   * ---------------------------------------------------------------------- */

   VFContractions contractions;
   vf_contractions_init(&contractions);
   contractions.need_sorting = false; /* We do full contractions here */

   S_CHECK_EXIT(rmdl_contract_analyze(mdl_dst_, &contractions));

   trace_process("[model] %s model '%.*s' #%u: contracting %u VF subdags\n",
                 mdl_fmtargs(mdl_dst_), contractions.VFdag_starts.len);

   S_CHECK_EXIT(rmdl_contract_subtrees(mdl_dst_, &contractions));

   S_CHECK_EXIT(rmdl_ensurefops_activedefault(mdl_src));
   S_CHECK_EXIT(rmdl_ensurefops_activedefault(mdl_dst_));

   S_CHECK_EXIT(rmdl_export_latex(mdl_dst_, "contracted"));

   mdl_unsetallchecks(mdl_dst_);

_exit:
   vf_contractions_empty(&contractions);
   return status;
}
