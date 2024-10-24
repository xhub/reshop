#include "asprintf.h"

#include "ccflib_reformulations.h"
#include "ccflib_utils.h"
#include "cmat.h"
#include "ctr_rhp.h"
#include "empdag.h"
#include "empinfo.h"
#include "mdl_rhp.h"
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
   size_t size;
   void *mem;
} DfsWorkspace;

typedef struct {
   RhpSense path_sense;
   mpid_t mpid_primal;           /* mpid of the active primal node */
   mpid_t mpid_dual;             /* mpid of the active dual node */
   nashid_t nashid;              /* ID of the MPE node, now at the root */
   rhp_idx vi_dual;
   ArcVFData arcVFprimal;
   ArcVFData arcVFdual;
   CopyExprData dual_objequ_dat;
   DfsWorkspace wksp;
   EmpDag *empdag;
   Model *mdl;
} DfsData;

static int ccflib_equil_dfs_dual(dagid_t mpid, DfsData *dfsdat, DagMpArray *mps,
                                 const DagMpArray *mps_old);
static int ccflib_equil_dfs_primal(dagid_t mpid, DfsData *dfsdat, DagMpArray *mps,
                                   const DagMpArray *mps_old);



#if defined(RHP_DEV_MODE) && 0
#include "rhp_dot_exports.h"
#define DEBUG_DISPLAY_OBJEQU(mdl, mp) { \
   rhp_idx eiobj = mp_getobjequ(mp); if (valid_ei(eiobj)) {\
   view_equ_as_png(mdl, eiobj); } \
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
static int ccflib_instantiate_mp(EmpDag *empdag, mpid_t mpid, DfsData *dfsdat,
                                 CcflibInstanceData *dualdat)
{
  /* ----------------------------------------------------------------------
   * IMPORTANT: we must guarantee that the objequ only has the -k(y) term
   * ---------------------------------------------------------------------- */

   const DagMpArray *mps_old = &empdag->empdag_up->mps;
   MathPrgm *mp_ovf_old = mps_old->arr[mpid];

   trace_process("[ccflib/equil] Instantiating MP(%s)\n", mps_old->names[mpid]);

   unsigned n_children = mps_old->Varcs[mpid].len;
   OvfDef *ovf_def = mp_ovf_old->ccflib.ccf;
   OvfOpsData ovfd = {.ovf = ovf_def};
   const OvfOps *ops = &ovfdef_ops;

   ovf_def->num_empdag_children = n_children;

   /* Change the MP to an OPT one */
   MathPrgm *mp_ovf = empdag->mps.arr[mpid];
   if (mp_ovf->type == MpTypeCcflib) {
     ovfdef_free(mp_ovf->ccflib.ccf); 
   } else {
      error("[ccflib/equil] Unsupported type '%s' for MP(%s). Expected type %s\n",
            mptype2str(mp_ovf->type), mp_getname(mp_ovf), mptype2str(MpTypeCcflib));
      return Error_NotImplemented;
   }

   mp_ovf->sense = ovf_def->sense;
   mp_ovf->type = MpTypeOpt;
   mpopt_init(&mp_ovf->opt);

   /* ---------------------------------------------------------------------
    * If the dual subdag already has elements, add the child here.
    * Otherwise, add it to the Nash problem
    * --------------------------------------------------------------------- */

   if (mpid_regularmp(dfsdat->mpid_dual)) {
      dfsdat->arcVFdual.mpid_child = mp_ovf->id;
      S_CHECK(empdag_mpVFmpbyid(empdag, dfsdat->mpid_dual, &dfsdat->arcVFdual));
   } else {
      S_CHECK(empdag_nashaddmpbyid(empdag, dfsdat->nashid, mp_ovf->id));
   }

   dualdat->ops = ops;
   dualdat->ovfd = ovfd;

   return mp_ccflib_instantiate(mp_ovf, mp_ovf_old, dualdat);
}

NONNULL static inline
int primal_arcVFb_add_by(Model *mdl, ArcVFBasicData *arcVFb_dat,  double *b, Avar *y)
{
   rhp_idx ei = arcVFb_dat->ei;
   assert(valid_ei_(ei, rctr_totalm(&mdl->ctr), __func__));

   if (valid_vi(arcVFb_dat->vi)) {
      TO_IMPLEMENT("bilinear case of primal_edgeVFb_add_by()")
   }

   double cst = arcVFb_dat->cst;
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
int primal_add_by(Model *mdl, ArcVFData *arc, double *b, Avar *y)
{
   /* ---------------------------------------------------------------------
    * This adds the term <b,y> to the equation where the VF of the MP associated
    * with y appears
    * --------------------------------------------------------------------- */

   switch (arc->type) {
   case ArcVFBasic:
      return primal_arcVFb_add_by(mdl, &arc->basic_dat, b, y);
   default: TO_IMPLEMENT("primal_add_by for non-trivial edge type");
   }
}

typedef struct {
   rhp_idx ei_old;
   rhp_idx ei_new;
} EiRosetta;

NONNULL static inline
int primal_check_Varc_ei(Model *mdl, ArcVFData *arc, mpid_t mpid, EiRosetta *ei_rosetta)
{
   rhp_idx ei_dst;
   switch (arc->type) {
   case ArcVFBasic:
      ei_dst = arc->basic_dat.ei;
      break;
   default: TO_IMPLEMENT("Varc type not supported");
   }

   if (ei_rosetta->ei_old == ei_dst) {
      assert(ei_dst != IdxNA); assert(mdl_valid_ei_(mdl, ei_rosetta->ei_new, __func__));
      arc->basic_dat.ei = ei_rosetta->ei_new;
      return OK;
   }

  /* ----------------------------------------------------------------------
   * If no objective equation / variable was given, then we instanciate an
   * objective function. We need to be careful to update all references.
   * ---------------------------------------------------------------------- */

   if (ei_dst == IdxObjFunc) {
      MathPrgm *mp;
      S_CHECK(empdag_getmpbyid(&mdl->empinfo.empdag, mpid, &mp));

      assert(mp->type == MpTypeOpt); assert(!valid_ei(mp->opt.objequ));

      Container *ctr = &mdl->ctr;
      RhpContainerData *cdat = (RhpContainerData*)ctr->data;
      S_CHECK(rctr_reserve_equs(ctr, 1));

      char *mp_objequ_name;
      IO_CALL(asprintf(&mp_objequ_name, "MP_objequ(%u)", mpid));
      S_CHECK(cdat_equname_start(cdat, mp_objequ_name));

      S_CHECK(rctr_add_equ_empty(ctr, &ei_dst, NULL, Mapping, CONE_NONE));
      S_CHECK(cdat_equname_end(cdat));

      mp_setobjequ(mp, ei_dst);

      rhp_idx objvar = mp_getobjvar(mp);

      if (valid_vi(objvar)) {
         Equ *e_dst = &ctr->equs[ei_dst];
         S_CHECK(rctr_equ_addnewvar(ctr, e_dst, objvar, 1.));
         mp_setobjvar(mp, IdxNA);
      }

      arc->basic_dat.ei = ei_dst;

      ei_rosetta->ei_old = IdxObjFunc;
      ei_rosetta->ei_new = ei_dst;

      return OK;
   }

  /* ----------------------------------------------------------------------
   * Check that the equation is not a defined mapping. In that case, we
   * transform it into an mapping
   * ---------------------------------------------------------------------- */

   Equ *edst = &mdl->ctr.equs[ei_dst];

   EquObjectType equtype = edst->object;

   switch (equtype) {
   case Mapping:
   case ConeInclusion:
      break;
   case DefinedMapping: {
      Equ *e_map = edst;
      MathPrgm *mp;
      S_CHECK(empdag_getmpbyid(&mdl->empinfo.empdag, mpid, &mp));
      rhp_idx objvar = mp_getobjvar(mp);
      rhp_idx objequ = mp_getobjequ(mp);

      if (objequ != ei_dst) {
         TO_IMPLEMENT("Defined mapping (not objective equation) involved");
      }
      S_CHECK(rmdl_equ_defmap2map(mdl, &e_map, objvar));
      rhp_idx ei_new = e_map->idx;
      ei_rosetta->ei_old = ei_dst;
      ei_rosetta->ei_new = ei_new;
      arc->basic_dat.ei = ei_new;

      }
   break;
   default:
      errbug("[ccflib/equil] ERROR: cannot copy into equation '%s' of type %s\n",
             mdl_printequname(mdl, ei_dst), equtype_name(equtype));
      return Error_BugPleaseReport;
   }


   return OK;
}

NONNULL static inline
int primal_add_minus_ky(Model *mdl, ArcVFData *arcVFdat, rhp_idx ky_idx)
{
  /* ----------------------------------------------------------------------
   * This copies -k(y), contained in the equation ky_idx, multiplied by the
   * edge weight.
   * ---------------------------------------------------------------------- */

   Equ *e_ky = rmdl_getequ(mdl, ky_idx);

   double coeff;
   rhp_idx vi, ei_dst;

   switch (arcVFdat->type) {
   case ArcVFBasic:
      vi = arcVFdat->basic_dat.vi;
      ei_dst = arcVFdat->basic_dat.ei;
      coeff = arcVFdat->basic_dat.cst;
      break;
   default: TO_IMPLEMENT("primal_add_minus_ky not implement for given arcVF");
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

   const VarcArray *Varcs = &mps_old->Varcs[mp->id];
   unsigned n_arcs = Varcs->len;
   ArcVFData * restrict children = Varcs->arr;
   const MathPrgm *mp_old = mps_old->arr[mp->id];

   unsigned n_primal_children = 0;
   for (unsigned i = 0; i < n_arcs; ++i) {
      assert(children[i].mpid_child < mps_old->len);

      if (mp_getsense(mps_old->arr[children[i].mpid_child]) != primal_sense) {
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

   dfsdat->dual_objequ_dat.equ = &mdl->ctr.equs[objequ];
   dfsdat->dual_objequ_dat.node = NULL;

   NlTree *tree = dfsdat->dual_objequ_dat.equ->tree;

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

static int copy_objequ_as_nlnode(CopyExprData *dualobjequdat, Model *mdl,
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

   if (!dualobjequdat->node && objequ->lequ->len == 0 &&
       (!objequ->tree || objequ->tree->root)) {
         error("[ccflib/equil] ERROR: constant objequ '%s' is not yet "
               "supported\n", ctr_printequname(&mdl->ctr, objequ->idx));
         return Error_NotImplemented;
   }

   // TODO: to support for primal -> primal node path, an idea is to ensure 
   // an ADD node so that the objequ of grand-children can be copied here as well
   S_CHECK(rctr_nltree_copy_map(&mdl->ctr, dualobjequdat->equ->tree, dualobjequdat->node,
                                objequ, objvar, NAN));

   return OK;
}

static int ccflib_equil_dfs_primal(mpid_t mpid_primal, DfsData *dfsdat, DagMpArray *mps,
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

   trace_process("[ccflib/equil:primal] processing primal MP(%s)\n",
                    empdag_getmpname(empdag, mpid_primal));

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

   /* Reset the Varcs in the new EMPDAG */
   mps->Varcs[mpid_primal].len = 0;

  /* ----------------------------------------------------------------------
   * Iterate over the children. 
   * ---------------------------------------------------------------------- */
   const ArcVFData *Varcs_primal_old = mps_old->Varcs[mpid_primal].arr;

   RhpSense path_sense = dfsdat->path_sense;

   UIntArray *rarcs = mps->rarcs;
   //ArcVFData edgeVFdual_bck = dfsdat->edgeVFdual;
   //ArcVFData edgeVFprimal_bck = dfsdat->edgeVFprimal;
   mpid_t mpid_dual_bck = dfsdat->mpid_dual;
   EiRosetta ei_rosetta = {.ei_new = IdxNA, .ei_old = IdxNA};
   Model *mdl = dfsdat->mdl;

   unsigned narcs = mps_old->Varcs[mpid_primal].len;
   for (unsigned i = 0; i < narcs; ++i) {
      ArcVFData Varc_primal;
      S_CHECK(arcVF_copy(&Varc_primal, &Varcs_primal_old[i]));
      S_CHECK(primal_check_Varc_ei(mdl, &Varc_primal, mpid_primal, &ei_rosetta));


      mpid_t mpid_child = Varc_primal.mpid_child;
      MathPrgm *mp_child = mps->arr[mpid_child];
      RhpSense child_sense = mp_getsense(mp_child);
      assert(child_sense == RhpMin || child_sense == RhpMax);

      trace_process("[ccflib/equil:primal] tackling child MP(%s)\n", mps->names[mpid_child]);

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
         S_CHECK(empdag_mpVFmpbyid(dfsdat->empdag, mpid_primal, &Varc_primal));

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
         S_CHECK(rhp_uint_rm(&rarcs[mpid_child], mpid_primal));

         S_CHECK(arcVF_copy(&dfsdat->arcVFprimal, &Varc_primal));

         // TODO: what is this for???
         //EdgeVF edgeVFdual_child;
         //S_CHECK(edgeVF_copy(&edgeVFdual_child, &edgeVFdual_bck));
         //S_CHECK(edgeVF_mul_edgeVF(&edgeVFdual_child, edgeVF_primal));

         S_CHECK(ccflib_equil_dfs_dual(mpid_child, dfsdat, mps, mps_old));

      }
   }

   if (narcs == 0) {
      trace_process("[ccflib/equil:dual] MP(%s) has no child\n", mps->names[mpid_primal]);
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

static int ccflib_equil_dfs_dual(mpid_t mpid_dual, DfsData *dfsdat, DagMpArray *mps,
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

   CcflibInstanceData dualdat;
   S_CHECK(ccflib_instantiate_mp(dfsdat->empdag, mpid_dual, dfsdat, &dualdat));

   /* Add - w * k(y) to the primal MP equation(s) */
   rhp_idx objequ_dual = mp_getobjequ(mp_dual);

   S_CHECK(primal_add_minus_ky(mdl, &dfsdat->arcVFprimal, objequ_dual));

   /* EMPDAG: reset VF children of dual node */
   unsigned n_arcs = mps_old->Varcs[mpid_dual].len;
   const ArcVFData *arcVFs_old = mps_old->Varcs[mpid_dual].arr;
   VarcArray *Varcs = &mps->Varcs[mpid_dual];
   Varcs->len = 0;

   RhpSense path_sense = dfsdat->path_sense;

   NlNode ***child_nlnodes;
   MathPrgm *mp_ccflib =  mps_old->arr[mpid_dual];
   OvfDef *ovf_def = mp_ccflib->ccflib.ccf;
   unsigned nargs_maps = ovf_def->args->size;

   /* For now, we have an exclusive treatment: either we have
    * VF arguments or variables */
   if (n_arcs > 0 && nargs_maps > 0) {
      error("[ccflib/equil:dual] ERROR: Mixture of %u MP children and %u "
            "variable arguments, this is not supported yet\n",
            n_arcs, nargs_maps);
      return Error_NotImplemented;
   }

   unsigned n_args = n_arcs + nargs_maps;
   MALLOC_(child_nlnodes, NlNode**, n_args);
   S_CHECK_EXIT(ccflib_equil_setup_dual_objequ(dfsdat, mp_dual, &dualdat.B, mps_old, child_nlnodes));
   rhp_idx objei_dual = dfsdat->dual_objequ_dat.equ->idx;
   assert(valid_ei(objei_dual));

   /* workspace similar to y */
   MALLOC_(workY, rhp_idx, dualdat.y.size);

   UIntArray *rarcs = mps->rarcs;

   /* save our dual information */
   ArcVFData Varc_primal2dual = dfsdat->arcVFprimal;
   mpid_t mpid_primal_bck = dfsdat->mpid_primal;

   CopyExprData dual_objequ_dat_bck = dfsdat->dual_objequ_dat;

   for (unsigned i = 0; i < n_arcs; ++i) {
      const ArcVFData *arcVF_old = &arcVFs_old[i];
      mpid_t mpid_child = arcVF_old->mpid_child;

      MathPrgm *mp_child = mps->arr[mpid_child];
      RhpSense child_sense = mp_getsense(mp_child);
      assert(child_sense == RhpMin || child_sense == RhpMax);

      trace_process("[ccflib/equil:dual] tackling child MP(%s)\n", mps->names[mpid_child]);

      /* TODO: add restrict keyword */
      unsigned si, len, *idxs;
      double sv, *vals;


      /* Get i-th row of B^T, that is the i-th column of B */
      S_CHECK_EXIT(rhpmat_col(&dualdat.B, i, &si, &sv, &len, &idxs, &vals));
      assert(len < dualdat.y.size);

      /* Update idxs to the variable space */
      for (unsigned j = 0; j < len; ++j) {
         workY[j] = avar_fget(&dualdat.y, idxs[j]);
      }

      /* ---------------------------------------------------------------------
       * We reset the arcVF to the current child 
       * --------------------------------------------------------------------- */

      arcVFb_init(&dfsdat->arcVFdual, objei_dual);
      arcVF_mul_lequ(&dfsdat->arcVFdual, len, workY, vals);
 
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

         S_CHECK_EXIT(daguidarray_rm(&rarcs[mpid_child], mpid_dual));

         ArcVFData Varc_primal_parent2child;
         S_CHECK_EXIT(arcVF_copy(&Varc_primal_parent2child, &Varc_primal2dual));
         Varc_primal_parent2child.mpid_child = mpid_child;
         S_CHECK_EXIT(arcVF_mul_lequ(&Varc_primal_parent2child, len, workY, vals));

         S_CHECK_EXIT(empdag_mpVFmpbyid(dfsdat->empdag, dfsdat->mpid_primal, &Varc_primal_parent2child));


         /* finally iterate on the primal child MP */
         S_CHECK_EXIT(ccflib_equil_dfs_primal(mpid_child, dfsdat, mps, mps_old));

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

   if (n_arcs == 0) {
      trace_process("[ccflib/equil:dual] MP(%s) has no child\n", mps->names[mpid_dual]);
   }

   if (nargs_maps > 0) {
      trace_process("[ccflib/equil:dual] MP(%s) has %u mapping arguments\n", mps->names[mpid_dual],
                    nargs_maps);


     /* ----------------------------------------------------------------------
      * This 
      * ---------------------------------------------------------------------- */

      //OvfOpsData ovfd = {.mp = mp_ccflib};



//      S_CHECK_EXIT(reformulation_equil_setup_dual_mp(mp_dual, eobj, vi_ovf, mdl,
//                                                     OVFTYPE_CCFLIB, ovfd,
//                                                     uvar, n_args));
   }


   S_CHECK_EXIT(cmat_sync_lequ(&mdl->ctr, dfsdat->dual_objequ_dat.equ));

   /* Add <b, y> to the equation of the primal parent node and the objequ of
    * the dual node.
    * 
    * WARNING: this has to be done after everything else to ensure that the 
    * correctness of the container matrix */
   if (dualdat.b) {
      Avar *y = &dualdat.y;

      S_CHECK_EXIT(primal_add_by(mdl, &dfsdat->arcVFprimal, dualdat.b, y));
      S_CHECK_EXIT(dual_objequ_add_by(mp_dual, dualdat.b, y));
   }


_exit:

   free((void*)child_nlnodes);

   free(workY);

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

      trace_process("[ccflib/equil] processing saddle path starting at MP(%s)\n",
                    empdag_getmpname(empdag, mpid));
      DfsData dfsdat = {.mpid_primal = MpId_NA, .mpid_dual = MpId_NA, 
                        .vi_dual = IdxNA, .empdag = empdag, .mdl = mdl};

      dfsdat.dual_objequ_dat.equ = NULL;
      dfsdat.dual_objequ_dat.node = NULL;
      arcVF_empty(&dfsdat.arcVFprimal);
      arcVFb_init(&dfsdat.arcVFdual, IdxNA);

      dfsdat.path_sense = mp_getsense(empdag_up->mps.arr[mpid]);

      UIntArray *primal_parents = &empdag_up->mps.rarcs[mpid];

      if (primal_parents->len > 1) { /* TODO: TEST */
         error("[ccflib/equil] ERROR MP(%s) has %u parents, we can only "
               "deal with at most 1\n", empdag_getmpname(empdag, mpid),
               primal_parents->len);
         return Error_EMPRuntimeError;
      }

      /* ----------------------------------------------------------------------
       * If we have no parent, then the root node is the Nash
       * ---------------------------------------------------------------------- */

     if (primal_parents->len == 0) {
         Nash *equil;
         A_CHECK(equil, empdag_newnashnamed(empdag, "CCF equilibrium reformulation"));

         /* ----------------------------------------------------------------------
          * If MP was a root, then put the MPE as root at its place.
          * Ditto for the EMPDAG root.
          * ---------------------------------------------------------------------- */

         daguid_t mpid_uid = mpid2uid(mpid);
         daguid_t root_idx = daguidarray_find(&empdag->roots, mpid_uid);

         if (root_idx < UINT_MAX) {
            empdag->roots.arr[root_idx] = nashid2uid(equil->id);
         }

         if (empdag->uid_root == mpid_uid) {
            assert(root_idx < UINT_MAX);
            empdag->uid_root = nashid2uid(equil->id);
         }

         S_CHECK(empdag_nashaddmpbyid(empdag, equil->id, mpid));
         dfsdat.nashid = equil->id;
 
      } else if (uidisNash(primal_parents->arr[0])) {

         dfsdat.nashid = uid2id(primal_parents->arr[0]);

      } else {

         /* ----------------------------------------------------------------------
          * We have one parent, but not a Nash node. Hence, we create a Nash node,
          * add the MP as its child, and replace the latter in its parent Carcs. 
          * ---------------------------------------------------------------------- */

         Nash *equil;
         A_CHECK(equil, empdag_newnashnamed(empdag, "CCF equilibrium reformulation"));

         S_CHECK(empdag_nashaddmpbyid(empdag, equil->id, mpid));
         dfsdat.nashid = equil->id;

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
                  "but cannot find the CTRL arc between the 2.\n", &offset,
                  empdag_getmpname(empdag, mpid), empdag_getname(empdag, uid));

            const ArcVFData *arc = empdag_find_Varc(empdag, mpid_parent, mpid);

            if (arc) {
               error("%*sFound a VF arc between the 2. Please file a bug report\n",
                     offset, "");
            } else {
               error("%*sInconsistent EMPDAG. Please file a bug report\n", offset, "");
            }
 
               return Error_BugPleaseReport;
            }

            Carcs_parent->arr[idx] = nashid2uid(equil->id);

         }

      S_CHECK(ccflib_equil_dfs_primal(saddle_path_start_mps[i], &dfsdat, &empdag->mps,
                                      &empdag_up->mps));
   }

   return OK;
}

