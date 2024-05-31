#include "reshop_config.h"

#include <assert.h>

#include "rmdl_empdag.h"
#include "ctr_rhp.h"
#include "empdag.h"
#include "empdag_mp.h"
#include "empdag_mpe.h"
#include "mathprgm.h"
#include "mdl.h"
#include "status.h"

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

/** @file rmdl_empdag.c
*
*   @brief Operations on EMPDAG of ReSHOP models
* */

/**
 * @brief Contract the empdag by merging MPs with the same sense along arcVFs to a single MP
 *
 * @param mdl      the model with a contracted EMPDAG
 * @param mdl_src  the original model
 *
 * @return         the error code
 */
int rmdl_contract_along_Vpaths(Model *mdl, const Model *mdl_src)
{
   assert(mdl_is_rhp(mdl));
   int status = OK;

  /* ----------------------------------------------------------------------
   * This 
   * ---------------------------------------------------------------------- */

   const EmpDag * empdag_src = &mdl_src->empinfo.empdag;
   EmpDag * empdag_dst = &mdl->empinfo.empdag;
   unsigned num_mp = empdag_src->mps.len;

   if (num_mp <= 1) {
      error("[model] ERROR with %s model '%.*s' #%u: graph contraction "
            "meaningless with no EMPDAG", mdl_fmtargs(mdl_src));
   }

   unsigned num_objequs = 0, num_nodes = num_mp + empdag_src->nashs.len;
   unsigned num_linkcons = 0;
   /* XXX: unclear why this is needed */
   rhp_idx *objequs = NULL ;
   mpid_t *rosetta_mpid = NULL;
   bool *seen_uid = NULL;
   MpIdArray VFdag_starts, VFdag_mpids;
   DagUidArray daguids;
   mpidarray_init(&VFdag_starts);
   mpidarray_init(&VFdag_mpids);
   daguidarray_init(&daguids);

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

   S_CHECK_EXIT(mpidarray_reserve(&VFdag_starts, num_mp));
   S_CHECK_EXIT(mpidarray_reserve(&VFdag_mpids, num_mp));
   S_CHECK_EXIT(daguidarray_reserve(&daguids, num_nodes));

   /* Prepare new EMPDAG: roots and MPE are unchanged. Reserve enough for MPS */
   S_CHECK_EXIT(dagmp_array_reserve(&empdag_dst->mps, num_mp));
   S_CHECK_EXIT(dagnash_array_copy(&empdag_dst->nashs, &empdag_src->nashs, mdl));



  /* ----------------------------------------------------------------------
   * Start of the BFS-like exploration
   * ---------------------------------------------------------------------- */

   assert(valid_uid(empdag_src->uid_root));
   S_CHECK_EXIT(daguidarray_add(&daguids, empdag_src->uid_root));

   DagMpArray *mps_dst = &empdag_dst->mps;
   const DagMpArray *mps_src = &empdag_src->mps;
   DBGUSED const DagNashArray *nashs_src = &empdag_src->nashs;

   for (unsigned i = 0; i < daguids.len; ++i) {

      daguid_t node = daguids.arr[i];

      /* --------------------------------------------------------------------
       * If we have a Nash node, just add the children
       * -------------------------------------------------------------------- */

      if (uidisNash(node)) {

         unsigned nidx = num_mp + uid2id(node);

         if (seen_uid[nidx]) { continue; }
         seen_uid[nidx] = true;

         assert(uid2id(node) < nashs_src->len);

         DagUidArray *c = &empdag_src->nashs.arcs[uid2id(node)];
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

      VarcArray *varcs_src = &mps_src->Varcs[mpid];
      bool start_has_objequ = false;

      for (unsigned j = 0, len = varcs_src->len; j < len; ++j) {
         if (!start_has_objequ) { 
            start_has_objequ = arcVF_has_objequ(&varcs_src->arr[j], mdl_src);
         }

         mpidarray_add(&VFdag_mpids, varcs_src->arr[j].child_id);
      }

      if (VFdag_mpids.len == 0) { continue; }

     /* ---------------------------------------------------------------------
      * Explore the VFdag: add mpid to starting nodes.
      * We iterate through all the MP in the subdag.
      * - If we find a Carc, we stop and bactrack
      * - If we go over the whole 
      * --------------------------------------------------------------------- */
      mpidarray_add(&VFdag_starts, mpid);

      unsigned num_objequs_old = num_objequs;
      unsigned num_linkcons_subdag = 0;


      for (unsigned j = 0; j < VFdag_mpids.len; ++j) {
         mpid_t mpid_ = VFdag_mpids.arr[j];
         assert(mpid_ < mps_src->len);

         if (mps_src->Carcs[mpid_].len > 0) {
            c = &mps_src->Carcs[mpid_];
            memcpy(&daguids.arr[daguids.len], c->arr, c->len * sizeof(daguid_t));
            daguids.len += c->len;

            /* We stop the VFdag loo and remove the start MPID */
            VFdag_mpids.len = 0;
            num_objequs = num_objequs_old;
            VFdag_starts.len--;
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
                  mdl_fmtargs(mdl_src));
            status = Error_NotImplemented;
            goto _exit;
         }

         seen_uid[mpid_] = true;

         MathPrgm *mp_ = mps_src->arr[mpid_];

         rhp_idx objequ = mp_getobjequ(mp_);

         if (valid_ei(objequ)) {
            objequs[num_objequs++] = objequ;
         }

         /* Need to get the type of arc*/
         varcs_src = &mps_src->Varcs[mpid_];

         for (unsigned k = 0, len = varcs_src->len; k < len; ++k) {
            ArcVFData *arc = &varcs_src->arr[k];

            unsigned num_cons = arcVF_getnumcons(arc, mdl_src);

            if (num_cons == UINT_MAX) {
               error("[empdag/contract] ERROR while procesing arcVF %u of MP(%s). "
                     "Please open a bug report\n", k, empdag_getmpname(empdag_src, mpid_));
               return Error_RuntimeError;
            }

            num_linkcons_subdag += num_cons;

            mpidarray_add(&VFdag_mpids, arc->child_id);
         }
      }

     /* ---------------------------------------------------------------------
      * We were successful. Just reset the VFdag_mpids array
      * --------------------------------------------------------------------- */

      num_linkcons += num_linkcons_subdag;
      VFdag_mpids.len = 0;
   }

  /* ----------------------------------------------------------------------
   * Now reformulate:
   * 1. Prepare the reformulation:
   *   - allocate num_linkcons + objequ of starting MP is involved? 1 : 0 
   *
   * 2. Iterate via DFS through the subDAG:
   *   - for each starting MP:
   *     x All constraints and decision variables are now attached to the starting MP
   *     x Inject the obj(equ|var), weighted by arc data, into the equations
   *
   * 3. For each child MP, we 
   * ---------------------------------------------------------------------- */

   S_CHECK_EXIT(rctr_reserve_equs(&mdl->ctr, num_linkcons + num_objequs));

   /* Reset DAGuids */
   daguids.len = 0;
   S_CHECK_EXIT(daguidarray_add(&daguids, empdag_src->uid_root));

   /* Allocate rosetta src/dst for MPs*/
   unsigned num_mp_dst = mps_dst->len;

   MALLOC_EXIT(rosetta_mpid, mpid_t, num_mp_dst);

   for (unsigned i = 0; i < num_mp_dst; ++i) {

   }

   mpid_t *root_arr = VFdag_starts.arr;
   mpid_t *child_arr = VFdag_starts.arr;

   for (unsigned i = 0, len = VFdag_starts.len; i < len; ++i) {
      mpid_t rootid = root_arr[i];

      VFdag_mpids.len = 0;
      mpidarray_add(&VFdag_mpids, rootid);

      for (unsigned j = 0; j < VFdag_mpids.len; ++j) {
         mpid_t mpid = child_arr[j];
         assert(mpid < mps_src->len);

         MathPrgm *mp = mps_src->arr[mpid];

         rhp_idx objequ = mp_getobjequ(mp);

         if (valid_ei(objequ)) {
            objequs[num_objequs++] = objequ;
         }

         /* Need to get the type of arc*/
         VarcArray *varcs_src = &mps_src->Varcs[mpid];

         for (unsigned k = 0, lenv = varcs_src->len; k < lenv; ++k) {
            ArcVFData *arc = &varcs_src->arr[k];

            unsigned num_cons = arcVF_getnumcons(arc, mdl_src);

            if (num_cons == UINT_MAX) {
               error("[empdag/contract] ERROR while procesing arcVF %u of MP(%s). "
                     "Please open a bug report\n", k, empdag_getmpname(empdag_src, mpid));
               return Error_RuntimeError;
            }

            mpidarray_add(&VFdag_mpids, arc->child_id);
         }
      }
   }
      
   #if 0
   EmpDag *empdag = &mdl->empinfo.empdag;

   typedef enum {
      ArcWeightScalar,
      ArcWeightVariable,
      ArcWeightLequ,
      ArcWeightQequ,
      ArcWeightEqu,
   } ArcWeightType;
   typedef struct {
      ArcWeightType type;
      
   } ArcWeight;

   for (unsigned i = 0, len = VFdag_starts.len; i < len; ++i) {
      mpid_t mpid = VFdag_starts.arr[i];

      MathPrgm* mp = mps->arr[mpid];
   
      VFdag_mpids.len = 1;
      VFdag_mpids.arr[0] = mpid2uid(mpid);

      for (unsigned j = 0; j < VFdag_mpids.len; ++j) {
         mpid_t mpid_j = VFdag_mpids.arr[j];


      }

   }
   #endif

   S_CHECK_EXIT(dagmp_array_trimmem(mps_dst));

_exit:
   FREE(objequs);
   FREE(rosetta_mpid);
   mpidarray_empty(&VFdag_starts);
   mpidarray_empty(&daguids);
   mpidarray_empty(&VFdag_mpids);

   return status;
}
