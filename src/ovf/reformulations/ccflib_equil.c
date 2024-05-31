#include "asprintf.h"

#include "ccflib_reformulations.h"
#include "cmat.h"
#include "ctr_rhp.h"
#include "empdag.h"
#include "empinfo.h"
#include "equ_modif.h"
#include "equil_common.h"
#include "nltree.h"
#include "nltree_priv.h"
#include "macros.h"
#include "mathprgm.h"
#include "mathprgm_priv.h"
#include "mdl.h"
#include "ctrdat_rhp.h"
#include "ovf_common.h"
#include "printout.h"
#include "rhp_LA.h"
#include "rmdl_priv.h"
#include "status.h"

typedef struct {
   unsigned size;
   NlNode *nodes[] __counted_by(size);
} DualObjEquNlNodes;

typedef struct {
   bool mul_term_is_lin;
   Equ *equ;
   NlTree *tree;
   NlNode **node;
} DualObjEquData;

typedef struct {
   size_t size;
   void *mem;
} DfsWorkspace;

typedef struct {
   RhpSense path_sense;
   mpid_t mpid_primal;           /* mpid of the active primal node */
   mpid_t mpid_dual;             /* mpid of the active dual node */
   nashid_t mpeid;                /* ID of the MPE node, now at the root */
   rhp_idx vi_dual;
   ArcVFData edgeVFprimal;
   ArcVFData edgeVFdual;
   DualObjEquData dual_objequ_dat;
   DfsWorkspace wksp;
   EmpDag *empdag;
   Model *mdl;
} DfsData;

typedef struct {
   Avar y;                     /**< Variable of the active dual MP */
   union ovf_ops_data ovfd;
   const struct ovf_ops *ops;
} DualData;

static int ccflib_equil_dfs_dual(dagid_t mpid, DfsData *dfsdat, DagMpArray *mps,
                                 const DagMpArray *mps_old);
static int ccflib_equil_dfs_primal(dagid_t mpid, DfsData *dfsdat, DagMpArray *mps,
                                   const DagMpArray *mps_old);



#if defined(RHP_DEV_MODE) && false
#include "rhp_dot_exports.h"
#define DEBUG_DISPLAY_OBJEQU(mdl, mp) { \
   rhp_idx eiobj = mp_getobjequ(mp); assert(valid_ei(eiobj)); \
   view_equ_as_png(mdl, eiobj); \
}
#else

#define DEBUG_DISPLAY_OBJEQU(mdl, mp) 

#endif

#ifdef UNUSED_AS_OF_20240320
static void ws_init(DfsWorkspace *ws)
{
   ws->size = 0;
   ws->mem = NULL;
}

static void ws_fini(DfsWorkspace *ws)
{
   FREE(ws->mem);
}

static void* ws_getmem(DfsWorkspace *ws, size_t size)
{
   if (size > ws->size) {
      REALLOC_NULL(ws->mem, uint8_t, size);
      ws->size = size;
   }

   return ws->mem;
}
#endif

/**
 * @brief Instanciate the CCFLIB node whose children are EMPDAG nodes
 *
 * @param empdag   the EMPDAG
 * @param mpid     the ID of the MP to instanciate
 * @param dfsdat   the DFS data
 * @param dualdat  the dual data
 *
 * @return         the error code
 */
static int mp_ccflib_instantiate(EmpDag *empdag, unsigned mpid, DfsData *dfsdat, DualData *dualdat)
{
  /* ----------------------------------------------------------------------
   * IMPORTANT: we must guarantee that the objequ only has the -k(y) term
   * ---------------------------------------------------------------------- */

   Model *mdl = empdag->mdl;
   RhpContainerData *cdat = (RhpContainerData*)mdl->ctr.data;

   const DagMpArray *mps_old = &empdag->empdag_up->mps;
   MathPrgm *mp_ovf_old = empdag->empdag_up->mps.arr[mpid];

   unsigned n_children = mps_old->Varcs[mpid].len;
   OvfDef *ovf_def = mp_ovf_old->ccflib.ccf;
   union ovf_ops_data ovfd = {.ovf = ovf_def};
   const struct ovf_ops *ops = &ovfdef_ops;

   ovf_def->num_empdag_children = n_children;

   /* Change the MP to an OPT one */
   MathPrgm *mp_ovf = empdag->mps.arr[mpid];
   mp_ovf->sense = ovf_def->sense;
   mp_ovf->type = MpTypeOpt;
   mpopt_init(&mp_ovf->opt);

   /* ---------------------------------------------------------------------
    * If the dual subdag already has elements, add the child here.
    * Otherwise, add it to the Nash problem
    * --------------------------------------------------------------------- */

   if (mpid_regularmp(dfsdat->mpid_dual)) {
      dfsdat->edgeVFdual.child_id = mp_ovf->id;
      S_CHECK(empdag_mpVFmpbyid(empdag, dfsdat->mpid_dual, &dfsdat->edgeVFdual));
   } else {
      S_CHECK(empdag_nashaddmpbyid(empdag, dfsdat->mpeid, mp_ovf->id));
   }

  /* ----------------------------------------------------------------------
   * Instantiate the MP: var + equations BUT NOT any of the <y, Bu+b> term
   * ---------------------------------------------------------------------- */

   char *y_name;
   /* TODO GAMS NAMING: do better and use MP name */
   IO_CALL(asprintf(&y_name, "ccflib_y_%u", mpid));

   Avar y;
   S_CHECK(ops->create_uvar(ovfd, &mdl->ctr, y_name, &y));


   S_CHECK(rctr_reserve_equs(&mdl->ctr, 1));

   rhp_idx objequ = IdxNA;
   /* TODO(xhub) improve naming */
   char *ovf_objequ;
   Equ *eobj;
   IO_CALL(asprintf(&ovf_objequ, "ccfObj(%u)", mpid));
   S_CHECK(cdat_equname_start(cdat, ovf_objequ));
   S_CHECK(rctr_add_equ_empty(&mdl->ctr, &objequ, &eobj, ConeInclusion, CONE_0));
   S_CHECK(cdat_equname_end(cdat));

   /*  Add -k(y) */
   ops->add_k(ovfd, mdl, eobj, &y, n_children);

   /*  TODO(xhub) URG remove this HACK */
   mp_ovf->probtype = eobj->tree ? MdlType_nlp : MdlType_lp;

   S_CHECK(mp_settype(mp_ovf, RHP_MP_OPT));

   S_CHECK(mp_setobjequ(mp_ovf, objequ));

   S_CHECK(mp_addvars(mp_ovf, &y));

   SpMat A;
   rhpmat_null(&A);

   double *s = NULL;
   S_CHECK(ops->get_set_nonbox(ovfd, &A, &s, false));

   /* Last, add the (non-box) constraints on u */
   if (A.ppty) {
      S_CHECK(ovf_add_polycons(mdl, ovfd, &y, ops, &A, s, mp_ovf, "ccflib"));
   }

   dualdat->y = y;
   dualdat->ops = ops;
   dualdat->ovfd = ovfd;

   return OK;

}

NONNULL static inline
int primal_edgeVFb_add_by(Model *mdl, EdgeVFBasicData *edgeVFb_dat,  double *b, Avar *y)
{
   rhp_idx ei = edgeVFb_dat->ei;
   assert(valid_ei_(ei, rctr_totalm(&mdl->ctr), __func__));

   if (valid_vi(edgeVFb_dat->vi)) {
      TO_IMPLEMENT("bilinear case of primal_edgeVFb_add_by()")
   }

   double cst = edgeVFb_dat->cst;
   Container *ctr = &mdl->ctr;
   return rctr_equ_addlinvars_coeff(ctr, &ctr->equs[ei], y, b, cst);
}

/**
 * @brief Add w * <b, y> to add the equations where the dual MP VF appears
 *
 * @param mdl     the model
 * @param edgeVF  the edge VF
 * @param b       the b vector
 * @param y       the variables of the dual MP
 *
 * @return        the error code
 */
NONNULL static inline
int primal_add_by(Model *mdl, ArcVFData *edgeVF, double *b, Avar *y)
{
   /* ---------------------------------------------------------------------
    * This adds the term <b,y> to the equation where the VF of the MP associated
    * with y appears
    * --------------------------------------------------------------------- */

   switch (edgeVF->type) {
   case ArcVFBasic:
      return primal_edgeVFb_add_by(mdl, &edgeVF->basic_dat, b, y);
   default: TO_IMPLEMENT("primal_add_by for non-trivial edge type");
   }
}

NONNULL static inline
int primal_add_minus_ky(Model *mdl, ArcVFData *edgeVF, rhp_idx ky_idx)
{
  /* ----------------------------------------------------------------------
   * This copies -k(y), contained in the equation ky_idx, multiplied by the
   * edge weight.
   * ---------------------------------------------------------------------- */

   Equ *e_ky = rmdl_getequ(mdl, ky_idx);

   double coeff;
   rhp_idx vi, ei_dst;

   switch (edgeVF->type) {
   case ArcVFBasic:
      vi = edgeVF->basic_dat.vi;
      ei_dst = edgeVF->basic_dat.ei;
      coeff = edgeVF->basic_dat.cst;
      break;
   default: TO_IMPLEMENT("primal_add_minus_ky not implement for given edgeVF");
   }

   Equ *e_dst = rmdl_getequ(mdl, ei_dst);

   if (valid_vi(vi)) {
      // This is a hack: we flip the sign of coeff
      S_CHECK(rctr_equ_submulv_equ_coeff(&mdl->ctr, e_dst, e_ky, vi, -coeff));
   } else {
      S_CHECK(rctr_equ_add_equ_coeff(&mdl->ctr, e_dst, e_ky, coeff));
   }
 
   return OK;
}

/**
 * @brief Add <b, y> to the objequ of a dual MP
 *
 * @param mp   the dual MP
 * @param b    the b vector
 * @param y    the variables of the dual MP
 *
 * @return     the error code
 */
NONNULL static inline
int dual_objequ_add_by(MathPrgm *mp, double *b, Avar *y)
{
   rhp_idx objequ = mp_getobjequ(mp);
   assert(valid_ei(objequ));
   Equ *e = &mp->mdl->ctr.equs[objequ];

   return rctr_equ_addlinvars(&mp->mdl->ctr, e, y, b);
}

NONNULL static
int ccflib_equil_setup_dual_objequ(DfsData *dfsdat, MathPrgm *mp, SpMat *B,
                                   const DagMpArray *mps_old, NlNode ***child_nodes)
{
   /* ---------------------------------------------------------------------
    * This function prepares the NlTree of the objequ to copy the expression
    * of the primal child nodes.
    * --------------------------------------------------------------------- */
   RhpSense dual_sense = mp_getsense(mp);
   RhpSense primal_sense = dual_sense == RhpMax ? RhpMin : RhpMax;

   const struct VFedges *edgesVFs = &mps_old->Varcs[mp->id];
   unsigned n_arcs = edgesVFs->len;
   ArcVFData * restrict children = edgesVFs->arr;
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

static int copy_objequ_as_nlnode(DualObjEquData *dualobjequdat, Model *mdl,
                                  Equ *objequ, rhp_idx objvar)
{
   /* ---------------------------------------------------------------------
    * This function adds the term
    * (Bᵀy)ᵢ Π (edge_weight) (objequ(MP))
    *
    * The node field should point to (Bᵀy)ᵢ Π (edge_weight)
    *
    * TODO: this might induce some variables to be wrongly tagged as NL.
    * For the quadratic case, it will also create issue.
    * --------------------------------------------------------------------- */

   if (dualobjequdat->mul_term_is_lin && objequ->lequ->len == 0 &&
       (!objequ->tree || objequ->tree->root)) {
         error("[ccflib/equil] ERROR: constant objequ '%s' is not yet "
               "supported\n", ctr_printequname(&mdl->ctr, objequ->idx));
         return Error_NotImplemented;
   }

   // TODO: to support for primal -> primal node path, an idea is to ensure 
   // an ADD node so that the objequ of grand-children can be copied here as well
   S_CHECK(rctr_nltree_copy_map(&mdl->ctr, dualobjequdat->tree, dualobjequdat->node,
                                objequ, objvar, NAN));

   return OK;
}

static int ccflib_equil_dfs_primal(dagid_t mpid_primal, DfsData *dfsdat, DagMpArray *mps,
                                   const DagMpArray *mps_old)
{
   /* ---------------------------------------------------------------------
    * Each processed MP has its Varcs (children on VF edge) reset.
    * We iterate on the old ones.
    * The Carcs are left untouched
    * --------------------------------------------------------------------- */

   MathPrgm *mp = mps->arr[mpid_primal];
   EmpDag *empdag = dfsdat->empdag;

   DEBUG_DISPLAY_OBJEQU(dfsdat->mdl, mp)

   /* ---------------------------------------------------------------------
    * If we have an active dual node, we add the objective function expression.
    * to the objective function of the active dual node
    *
    * It is better to do this in child than the dual parent, as this seemingly
    * allows for 
    * --------------------------------------------------------------------- */

   // TODO: Get some info about the chidlren beforehand. This would help with
   // supporting primal -> primal subpath
 
   if (dfsdat->dual_objequ_dat.node) {
      rhp_idx objequ = mp_getobjequ(mp);
      if (!valid_ei(objequ)) {
         error("[reformulation:ccflib] ERROR: invalid objequ for MP '%s'",
               empdag_getmpname(empdag, mpid_primal));
         return Error_RuntimeError;
      }

      Equ *e = &empdag->mdl->ctr.equs[objequ];
      rhp_idx objvar = mp_getobjvar(mp);

      S_CHECK(copy_objequ_as_nlnode(&dfsdat->dual_objequ_dat, empdag->mdl, e, objvar));
   }

  /* ----------------------------------------------------------------------
   * Iterate over the children. 
   * ---------------------------------------------------------------------- */

   const ArcVFData *edgeVFs_primal = mps_old->Varcs[mpid_primal].arr;
   struct VFedges *children = &mps->Varcs[mpid_primal];
   children->len = 0;
   RhpSense path_sense = dfsdat->path_sense;

   UIntArray *rarcs = mps->rarcs;
   //ArcVFData edgeVFdual_bck = dfsdat->edgeVFdual;
   //ArcVFData edgeVFprimal_bck = dfsdat->edgeVFprimal;
   mpid_t mpid_dual_bck = dfsdat->mpid_dual;

   for (unsigned i = 0, len = mps_old->Varcs[mpid_primal].len; i < len; ++i) {
      const ArcVFData *edgeVF_primal = &edgeVFs_primal[i];
      unsigned child_id = edgeVF_primal->child_id;

      MathPrgm *mp_child = mps->arr[child_id];
      RhpSense child_sense = mp_getsense(mp_child);
      assert(child_sense == RhpMin || child_sense == RhpMax);

      /* ---------------------------------------------------------------------
       * Update/restore the primal MPID
       * --------------------------------------------------------------------- */
      dfsdat->mpid_primal = mpid_primal;
      dfsdat->mpid_dual = mpid_dual_bck;

      if (child_sense == path_sense) {

         /* ---------------------------------------------------------------------
          * The child is also primal
          *
          * If we have an active dual node, the objective function expression
          * contribution will be addedby inspecting the next dual child
          *
          * We need to multiply the incoming primal edgeVF by the current one
          * to get the right value
          * --------------------------------------------------------------------- */
         S_CHECK(empdag_mpVFmpbyid(dfsdat->empdag, mpid_primal, edgeVF_primal));

         if (valid_mpid(dfsdat->mpid_dual)) {
            TO_IMPLEMENT("Primal -> primal");
         }

         //S_CHECK(edgeVF_mul_edgeVF(&dfsdat->edgeVFprimal, edgeVF_primal));

         //S_CHECK(ccflib_equil_dfs_primal(child_id, dfsdat, mps, mps_old));

         //S_CHECK(edgeVF_copy(&dfsdat->edgeVFprimal, &edgeVFprimal_bck));

      } else {
         /* ---------------------------------------------------------------------
          * We have an adversarial MP. We need to:
          * - Set the primal VF edge to the current one (TODO: true in general?)
          * - 
          * --------------------------------------------------------------------- */
         S_CHECK(rhp_uint_rm(&rarcs[child_id], mpid_primal));

         S_CHECK(arcVF_copy(&dfsdat->edgeVFprimal, edgeVF_primal));

         // TODO: what is this for???
         //EdgeVF edgeVFdual_child;
         //S_CHECK(edgeVF_copy(&edgeVFdual_child, &edgeVFdual_bck));
         //S_CHECK(edgeVF_mul_edgeVF(&edgeVFdual_child, edgeVF_primal));

         S_CHECK(ccflib_equil_dfs_dual(child_id, dfsdat, mps, mps_old));

      }
   }

      /* ---------------------------------------------------------------------
       * Now we must be ready to reformulate, buckle up!
       * We go over the the children of that node. If they are of the right sense:
       * 1) we add them as children of the previous MP.
       * 2) If there is an active dual MP, copy the objective function expression
       *    to the active dual MP at the right spot.
       * 
       * If we have an adversarial MP, we need to do the following:
       * 1) Generate the full problem dual problem and attach it to the dual subdag
       * 2) For its objective equation, we add
       * --------------------------------------------------------------------- */


   DEBUG_DISPLAY_OBJEQU(dfsdat->mdl, mp)

   return OK;
}

static int ccflib_equil_dfs_dual(dagid_t mpid_dual, DfsData *dfsdat, DagMpArray *mps,
                                 const DagMpArray *mps_old)
{
   int status = OK;
   rhp_idx *workY = NULL;

   /* ---------------------------------------------------------------------
    * We have a dual MP. We perform the following action:
    * - instantiate the node
    * - attach the node to the active dual node, with active dual weight
    * 
    * - Compute/make (B^T y) accessible.
    * - Compute <b, y> and add the term to the objective function of the
    *   active primal node
    * - update the data structure to set the current node as the active dual
    *   node
    * --------------------------------------------------------------------- */

   Model *mdl = dfsdat->mdl;
   assert(mpid_dual < mps->len);
   assert(dfsdat->mpid_primal < mps->len);

   MathPrgm *mp_dual = mps->arr[mpid_dual];

   DualData dualdat;
   S_CHECK(mp_ccflib_instantiate(dfsdat->empdag, mpid_dual, dfsdat, &dualdat));

   /* Add - w * k(y) to the primal MP equation(s) */
   rhp_idx objequ_dual = mp_getobjequ(mp_dual);
   S_CHECK(primal_add_minus_ky(mdl, &dfsdat->edgeVFprimal, objequ_dual));

   SpMat B;
   rhpmat_null(&B);
   double *b = NULL;
   S_CHECK(dualdat.ops->get_lin_transformation(dualdat.ovfd, &B, &b));

   /* EMPDAG: reset VF children of dual node */
   unsigned n_arcs = mps_old->Varcs[mpid_dual].len;
   const ArcVFData *edgeVFs_old = mps_old->Varcs[mpid_dual].arr;
   struct VFedges *VFchildren = &mps->Varcs[mpid_dual];
   VFchildren->len = 0;

   RhpSense path_sense = dfsdat->path_sense;

   NlNode ***child_nlnodes;
   MALLOC_(child_nlnodes, NlNode**, n_arcs);
   S_CHECK_EXIT(ccflib_equil_setup_dual_objequ(dfsdat, mp_dual, &B, mps_old, child_nlnodes));
   rhp_idx objei_dual = dfsdat->dual_objequ_dat.tree->idx;
   assert(valid_ei(objei_dual));

   /* workspace similar to y */
   MALLOC_(workY, rhp_idx, dualdat.y.size);

   UIntArray *rarcs = mps->rarcs;

   /* save our dual information */
   ArcVFData edgeVFprimal2dual = dfsdat->edgeVFprimal;
   mpid_t mpid_primal_bck = dfsdat->mpid_primal;

   DualObjEquData dual_objequ_dat_bck = dfsdat->dual_objequ_dat;

   for (unsigned i = 0; i < n_arcs; ++i) {
      const ArcVFData *edgeVF_old = &edgeVFs_old[i];
      unsigned child_id = edgeVF_old->child_id;

      MathPrgm *mp_child = mps->arr[child_id];
      RhpSense child_sense = mp_getsense(mp_child);
      assert(child_sense == RhpMin || child_sense == RhpMax);

      /* TODO: add restrict keyword */
      unsigned si, len, *idxs;
      double sv, *vals;


      /* Get i-th row of B^T, that is the i-th column of B */
      S_CHECK_EXIT(rhpmat_col(&B, i, &si, &sv, &len, &idxs, &vals));
      assert(len < dualdat.y.size);

      /* Update idxs to the variable space */
      for (unsigned j = 0; j < len; ++j) {
         workY[j] = avar_fget(&dualdat.y, idxs[j]);
      }

      /* ---------------------------------------------------------------------
       * We reset the edge to the current child 
       * --------------------------------------------------------------------- */

      arcVFb_init(&dfsdat->edgeVFdual, objei_dual);
      arcVF_mul_lequ(&dfsdat->edgeVFdual, len, workY, vals);
 
      /* ---------------------------------------------------------------------
      * Since we use DFS, we need to restore the our dual information every time
       * Update/restore the dual MPID
       * --------------------------------------------------------------------- */
      dfsdat->mpid_dual = mpid_dual;
      dfsdat->mpid_primal = mpid_primal_bck;
      dfsdat->dual_objequ_dat = dual_objequ_dat_bck;

      if (child_sense == path_sense) {

         /* ---------------------------------------------------------------
          * - we copy its objective function, times (B_i)^T, into the objequ
          * --------------------------------------------------------------- */

         dfsdat->dual_objequ_dat.node = child_nlnodes[i];

         /* ---------------------------------------------------------------
          * Substitute the edgeVF between primal parent and dual with a
          * primal parent to child MPs with weights being multiplied
          * --------------------------------------------------------------- */

         S_CHECK_EXIT(rhp_uint_rm(&rarcs[child_id], mpid_dual));

         ArcVFData edge_primal_parent2child;
         S_CHECK_EXIT(arcVF_copy(&edge_primal_parent2child, &edgeVFprimal2dual));
         edge_primal_parent2child.child_id = child_id;
         S_CHECK_EXIT(arcVF_mul_lequ(&edge_primal_parent2child, len, workY, vals));

         S_CHECK_EXIT(empdag_mpVFmpbyid(dfsdat->empdag, dfsdat->mpid_primal, &edge_primal_parent2child));


         /* finally iterate on the primal child MP */
         S_CHECK_EXIT(ccflib_equil_dfs_primal(child_id, dfsdat, mps, mps_old));

      } else {
         /* ---------------------------------------------------------------------
          * We have another dual problem:
          * - 
          * S_CHECK_EXIT(edgeVF_mul_edgeVF(&dfsdat->edgeVFprimal, edgeVF_old));
          * S_CHECK_EXIT(edgeVF_mul_lequ(&dfsdat->edgeVFprimal, len, workY, vals));
          * S_CHECK_EXIT(ccflib_equil_dfs_dual(child_id, dfsdat, mps, mps_old));
          * --------------------------------------------------------------------- */
         TO_IMPLEMENT("DUAL after DUAL");

      }

   }

   S_CHECK_EXIT(cmat_sync_lequ(&mdl->ctr, dfsdat->dual_objequ_dat.equ));

   /* Add <b, y> to the equation of the primal parent node and the objequ of
    * the dual node.
    * 
    * WARNING: this has to be done after everything else to ensure that the 
    * correctness of the container matrix */
   if (b) {
      Avar *y = &dualdat.y;

      S_CHECK_EXIT(primal_add_by(mdl, &dfsdat->edgeVFprimal, b, y));
      S_CHECK_EXIT(dual_objequ_add_by(mp_dual, b, y));
   }


_exit:


   FREE(workY);

   DEBUG_DISPLAY_OBJEQU(dfsdat->mdl, mp_dual)

   return status;
}


int ccflib_equil(Model *mdl)
{
   EmpDag *empdag = &mdl->empinfo.empdag;
   const EmpDag *empdag_up = empdag->empdag_up;
   mpid_t *saddle_path_start_mps = empdag_up->saddle_path_starts.arr;

  /* ----------------------------------------------------------------------
   * Iterate over the saddle paths and reformulate
   * ---------------------------------------------------------------------- */

   for (unsigned i = 0, len = empdag_up->saddle_path_starts.len; i < len; ++i) {

      mpid_t mpid = saddle_path_start_mps[i];
      assert(mpid < empdag_up->mps.len);

      DfsData dfsdat = {.mpid_primal = MpId_NA, .mpid_dual = MpId_NA, 
                        .vi_dual = IdxNA, .empdag = empdag, .mdl = mdl};

      dfsdat.dual_objequ_dat.tree = NULL;
      dfsdat.dual_objequ_dat.node = NULL;
      arcVF_empty(&dfsdat.edgeVFprimal);
      arcVFb_init(&dfsdat.edgeVFdual, IdxNA);

      dfsdat.path_sense = mp_getsense(empdag_up->mps.arr[mpid]);

      UIntArray *primal_parents = &empdag_up->mps.rarcs[mpid];

      if (primal_parents->len > 1) { /* TODO: TEST */
         error("[CCFLIB:equilibrium]: ERROR MP(%s) has %u parents, we can only "
               "deal with at most 1\n", empdag_getmpname(empdag, mpid),
               primal_parents->len);
         return Error_EMPRuntimeError;
      }

      /* ----------------------------------------------------------------------
       * If we have no parent, then the root node is the Nash
       * ---------------------------------------------------------------------- */

     if (primal_parents->len == 0) {
         Nash *equil;
         A_CHECK(equil, empdag_newnashnamed(empdag, strdup("CCF equilibrium reformulation")));

         /* ----------------------------------------------------------------------
          * If MP was a root, then put the MPE as root at its place.
          * Ditto for the EMPDAG root.
          * ---------------------------------------------------------------------- */

         daguid_t mpid_uid = mpid2uid(mpid);
         unsigned root_idx = rhp_uint_find(&empdag->roots, mpid_uid);

         if (root_idx < UINT_MAX) {
            empdag->roots.arr[root_idx] = nashid2uid(equil->id);
         }

         if (empdag->uid_root == mpid_uid) {
            assert(root_idx < UINT_MAX);
            empdag->uid_root = nashid2uid(equil->id);
         }

         S_CHECK(empdag_nashaddmpbyid(empdag, equil->id, mpid));
         dfsdat.mpeid = equil->id;
 
      } else if (uidisNash(primal_parents->arr[0])) {
         dfsdat.mpeid = uid2id(primal_parents->arr[0]);

      } else {

         /* ----------------------------------------------------------------------
          * We have one parent, but not a Nash node. Hence, we create a Nash node,
          * add the MP as its child, and replace the latter in its parent Carcs. 
          * ---------------------------------------------------------------------- */

         Nash *equil;
         A_CHECK(equil, empdag_newnashnamed(empdag, strdup("CCF equilibrium reformulation")));

         S_CHECK(empdag_nashaddmpbyid(empdag, equil->id, mpid));
         dfsdat.mpeid = equil->id;

         daguid_t uid = primal_parents->arr[0];

         if (uidisNash(uid)) {
            error("[CCFLIB:equilibrium]: ERROR MP(%s) has Nash(%s) as parent. "
                  "This is not supported!\n", empdag_getmpname(empdag, mpid),
                  empdag_getnashname(empdag, uid2id(uid)));
            return Error_EMPRuntimeError;
         }

         mpid_t mpid_parent = uid2id(uid);

         UIntArray * restrict Carcs_parent = &empdag->mps.Carcs[mpid_parent];
         unsigned idx = rhp_uint_find(Carcs_parent, mpid2uid(mpid));

         if (idx == UINT_MAX) {
            int offset;
            error("[CCFLIB:equilibrium]: %nERROR MP(%s) has MP(%s) as parent, "
                  "but cannot find the CTRL edge between the 2.\n", &offset,
                  empdag_getmpname(empdag, mpid), empdag_getname(empdag, uid));

            const ArcVFData *edge = empdag_find_edgeVF(empdag, mpid_parent, mpid);

            if (edge) {
               error("%*sFound a VF edge between the 2. Please file a bug report\n",
                     offset, "");
            } else {
               error("%*sInconsistent EMPDAG. Please file a bug report\n", offset, "");
            }
 
               return Error_RuntimeError;
            }

            Carcs_parent->arr[idx] = nashid2uid(equil->id);

         }

      S_CHECK(ccflib_equil_dfs_primal(saddle_path_start_mps[i], &dfsdat, &empdag->mps,
                                      &empdag_up->mps));
   }

   return OK;
}

