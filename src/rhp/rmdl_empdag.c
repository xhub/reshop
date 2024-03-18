#include <assert.h>

#include "rmdl_empdag.h"
#include "empdag.h"
#include "mathprgm.h"
#include "mdl.h"
#include "status.h"

/** @file rmdl_empdag.c
*
* @brief Operations on the empdag of a model
* */

/**
 * @brief Contract the empdag by reducing along arcVFs to a single MP
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

   const EmpDag * empdag_src = &mdl_src->empinfo.empdag;
   unsigned num_mp = empdag_src->mps.len;

   if (num_mp <= 1) {
      error("[model] ERROR with %s model '%.*s' #%u: graph contraction "
            "meaningless with no EMPDAG", mdl_fmtargs(mdl_src));
   }

   unsigned num_objequs = 0, num_nodes = num_mp + empdag_src->mpes.len;
   rhp_idx *objequs = NULL;
   bool *seen_uid = NULL;
   MpIdArray VFdag_starts, VFdag_mpids;
   DagUidArray daguids;
   mpidarray_init(&VFdag_starts);
   mpidarray_init(&VFdag_mpids);
   daguidarray_init(&daguids);

   MALLOC_EXIT(objequs, rhp_idx, num_mp);
   MALLOC_EXIT(seen_uid, bool, num_nodes);
   memset(seen_uid, 0, num_nodes*sizeof(bool));

   S_CHECK_EXIT(mpidarray_reserve(&VFdag_starts, num_mp));
   S_CHECK_EXIT(mpidarray_reserve(&VFdag_mpids, num_mp));
   S_CHECK_EXIT(daguidarray_reserve(&daguids, num_nodes));

  /* ----------------------------------------------------------------------
   * Start of the BFS-like exploration
   * ---------------------------------------------------------------------- */

   assert(valid_uid(empdag_src->uid_root));
   S_CHECK_EXIT(daguidarray_add(&daguids, empdag_src->uid_root));

   const DagMpArray *mps = &empdag_src->mps;
   const DagMpeArray *nashs = &empdag_src->mpes;



   for (unsigned i = 0; i < daguids.len; ++i) {
      Mpe *mpe = NULL;

      daguid_t node = daguids.arr[i];

      /* --------------------------------------------------------------------
       * If we have a Nash node, just add the children
       * -------------------------------------------------------------------- */

      if (uidisMPE(node)) {

         unsigned nidx = num_mp + uid2id(node);

         if (seen_uid[nidx]) { continue; }
         seen_uid[nidx] = true;

         assert(uid2id(node) < nashs->len);
         mpe = empdag_src->mpes.arr[uid2id(node)];

         DagUidArray *c = &empdag_src->mpes.arcs[uid2id(node)];
         memcpy(&daguids.arr[daguids.len], c->arr, c->len * sizeof(daguid_t));
         daguids.len += c->len;
         continue;
      }

      mpid_t mpid = uid2id(node); assert(mpid < mps->len);

      if (seen_uid[mpid]) { continue; }
      seen_uid[mpid] = true;

     /* ----------------------------------------------------------------------
      * We potentially have the start of a VFtree
      * ---------------------------------------------------------------------- */

      MathPrgm *mp = empdag_src->mps.arr[mpid];

      DagUidArray *c = &empdag_src->mps.Carcs[mpid];
      memcpy(&daguids.arr[daguids.len], c->arr, c->len * sizeof(daguid_t));
      daguids.len += c->len;

      VarcArray *varcs = &empdag_src->mps.Varcs[mpid];

      for (unsigned j = 0, len = varcs->len; j < len; ++j) {
         mpidarray_add(&VFdag_mpids, varcs->arr[j].child_id);
      }

      if (VFdag_mpids.len == 0) { continue; }

     /* ---------------------------------------------------------------------
      * Explore the VFdag: add mpid to starting nodes
      * --------------------------------------------------------------------- */
      mpidarray_add(&VFdag_starts, mpid);

      unsigned num_objequs_old = num_objequs;

      for (unsigned j = 0; j < VFdag_mpids.len; ++j) {
         mpid_t mpid_ = VFdag_mpids.arr[j];
         assert(mpid_ < mps->len);


         if (mps->Carcs[mpid_].len > 0) {
            c = &empdag_src->mps.Carcs[mpid_];
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

        DagUidArray *rarcs = &mps->rarcs[mpid_];
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

         MathPrgm *mp_ = mps->arr[mpid_];

         rhp_idx objequ = mp_getobjequ(mp_);

         if (valid_ei(objequ)) {
            objequs[num_objequs++] = objequ;
         }

         varcs = &mps->Varcs[mpid_];

         for (unsigned k = 0, len = varcs->len; k < len; ++k) {
            mpidarray_add(&VFdag_mpids, varcs->arr[k].child_id);
         }
      }

     /* ---------------------------------------------------------------------
      * We were successful. Just reset the VFdag_mpids array
      * --------------------------------------------------------------------- */

      VFdag_mpids.len = 0;
   }

  /* ----------------------------------------------------------------------
   * Now reformulate
   * ---------------------------------------------------------------------- */

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


_exit:
   FREE(objequs);
   mpidarray_empty(&VFdag_starts);
   mpidarray_empty(&daguids);
   mpidarray_empty(&VFdag_mpids);
   return OK;
}
