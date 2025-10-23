#include "asprintf.h"
#include "lequ.h"
#include "reshop_config.h"

#include <limits.h>
#include <string.h>

#include "empdag_alg.h"
#include "mdl_data.h"
#include "ovfinfo.h"
#include "container.h"
#include "empdag.h"
#include "empdag_common.h"
#include "empdag_uid.h"
#include "empinfo.h"
#include "macros.h"
#include "mathprgm.h"
#include "ctrdat_rhp.h"
#include "mdl.h"
#include "printout.h"
#include "print_utils.h"
#include "string_utils.h"

#include "empdag_mp.h"
#include "empdag_mpe.h"
#include "toplayer_utils.h"


/* FIXME: On 2024.02.29, removed these from header. Could be converted to
 * static function if the need for theses does not appear soon
 */
int empdag_rootsaddnash(EmpDag *empdag, nashid_t nashid) NONNULL;
int empdag_rootsaddmp(EmpDag *empdag, mpid_t mpid) NONNULL;
int empdag_rootsadd(EmpDag *empdag, daguid_t uid) NONNULL;



static int chk_mpid_(const EmpDag *empdag, mpid_t mpid)
{
   if (mpid >= empdag->mps.len) {
      if (!mpid_regularmp(mpid)) {
         error("%s :: %s\n", __func__, mpid_specialvalue(mpid));
      } else {
         error("[empdag] ERROR: no MP with index %u, the number of mps is %u in "
               "%s model '%.*s' #%u\n", mpid, empdag->mps.len,
               mdl_fmtargs(empdag->mdl));
      }

      return Error_NotFound;
   }

   return OK;
}

static int chk_nashid_(const EmpDag *empdag, nashid_t nashid)
{
   if (nashid >= empdag->nashs.len) {
      if (!valid_idx(nashid)) {
         error("%s :: %s\n", __func__, badidx2str(nashid));
      } else {
         error("%s :: no Nash with index %d, the number of Nash nodes is %u "
                  "in %s model '%.*s' #%u\n", __func__, nashid, empdag->nashs.len,
                  mdl_fmtargs(empdag->mdl));
      }
      return Error_NotFound;
   }

   return OK;
}

const char *empdag_typename(enum empdag_type type)
{
   switch (type) {
      case EmpDag_Empty:
         return "empty DAG";
      case EmpDag_Unset:
         return "unset";
      case EmpDag_Single_Opt:
         return "single optimization problem";
      case EmpDag_Single_Vi:
         return "single VI";
      case EmpDag_Opt:
         return "optimization problem";
      case EmpDag_Vi:
         return "VI";
      case EmpDag_Mopec:
         return "MOPEC";
      case EmpDag_Bilevel:
         return "bilevel";
      case EmpDag_Multilevel:
         return "multilevel";
      case EmpDag_Mpec:
         return "MPEC";
      case EmpDag_Epec:
         return "EPEC";
      case EmpDag_Complex:
         return "complex";
      default:
         return "unknown (this is an error)";
   }
}

const char* arcVFType2str(ArcVFType type)
{
   switch(type) {
   case ArcVFUnset:
      return "ArcVFUnset";
   case ArcVFBasic:
      return "ArcVFBasic";
   case ArcVFMultipleBasic:
      return "ArcVFMultipleBasic";
   case ArcVFLequ:
      return "ArcVFLequ";
   case ArcVFMultipleLequ:
      return "ArcVFMultipleLequ";
   case ArcVFEqu:
      return "ArcVFEqu";
   case ArcVFMultipleEqu:
      return "ArcVFMultipleEqu";
   default:
      return "ERROR unknown arcVFType";
   }
}

void empdag_init(EmpDag *empdag, Model *mdl)
{
   empdag->type = EmpDag_Unset;
   empdag->stage = EmpDagStage_Unset;
   empdag->finalized = false;
   empdag->arcs_fully_resolved = true;  /* Set by false at the start of the empinterp */
   empdag->features.istree = false;

   memset(&empdag->node_stats, 0, sizeof (empdag->node_stats));
   memset(&empdag->arc_stats, 0, sizeof (empdag->arc_stats));

   daguidarray_init(&empdag->roots);
   mpidarray_init(&empdag->minimaxi.mps2reformulate);
   mpidarray_init(&empdag->minimaxi.saddle_path_starts);
   mpidarray_init(&empdag->mps2instantiate);
   mpidarray_init(&empdag->mps_newly_created);

   dagmp_array_init(&empdag->mps);
   dagnash_array_init(&empdag->nashs);

   empdag->uid_root = EMPDAG_UID_NONE;

   empdag->empdag_next = NULL;
   empdag->empdag_up = NULL;

   empdag->simple_data.sense = RhpNoSense;
   empdag->simple_data.objequ = IdxNA;
   empdag->simple_data.objvar = IdxNA;

   mpidarray_init(&empdag->transformations.fenchel_dual_nodal);
   mpidarray_init(&empdag->transformations.fenchel_dual_subdag);
   mpidarray_init(&empdag->transformations.epi_dual_nodal);
   mpidarray_init(&empdag->transformations.epi_dual_subdag);
   mpidarray_init(&empdag->transformations.fooc.src);
   mpidarray_init(&empdag->transformations.fooc.vi);
   mpidarray_init(&empdag->objfn.src);
   mpidarray_init(&empdag->objfn.dst);
   mpidarray_init(&empdag->objfn_maps.mps);

   empdag->objfn_maps.lequs = NULL;

   empdag->mdl = mdl;
}

/* TODO UNUSED */
int empdag_next(EmpDag *empdag)
{
   if (empdag->empdag_next) {
      error("%s ERROR: EMPDAG has already a derived DAG", __func__);
      return Error_RuntimeError;
   }

   MALLOC_(empdag->empdag_next, EmpDag, 1);
   empdag_init(empdag->empdag_next, empdag->mdl);

   return OK;
}

static int empdag_infertype(EmpDag *empdag)
{
   /* ---------------------------------------------------------------------
    * Based on the features, we can infer the kind of DAG.
    *
    * - If there are no MPs, it is an empty DAG
    * - If we have only simple constraints, then the root node determines the
    *   type
    * - With otherwise the type of non-conical constraints determines the type
    * --------------------------------------------------------------------- */

   if (empdag->mps.len == 0) {
      empdag->type = EmpDag_Empty;
      return OK;
   }

   if (empdag->features.vicons != EmpDag_Vi_SimpleConstraint) {
      empdag->type = EmpDag_Complex;
      return OK;
   }

   if (empdag->features.constraint == EmpDag_SimpleConstraint) {
      unsigned num_node_active = empdag->node_stats.num_active;

      switch (empdag->features.rootnode) {
      case EmpDag_RootOpt:
         empdag->type = num_node_active > 1 ? EmpDag_Opt : EmpDag_Single_Opt;
         break;
      case EmpDag_RootVi:
         empdag->type = num_node_active > 1 ? EmpDag_Vi : EmpDag_Single_Vi;
         break;
      case EmpDag_RootEquil:
         empdag->type = EmpDag_Mopec;
         break;
      default:
         goto err_rootnode;
      } 

      return OK;
   }

   if (empdag->features.constraint & EmpDag_MultiLevelConstraint) {
      if (empdag->features.rootnode == EmpDag_RootEquil) {
         empdag->type = EmpDag_MultilevelMopec;
      } else {
         empdag->type = EmpDag_Multilevel;
      }

      return OK;
   }

   if (empdag->features.constraint == EmpDag_OptSolMapConstraint) {
      assert(empdag->features.rootnode != EmpDag_RootVi);

      switch (empdag->features.rootnode) {
      case EmpDag_RootOpt:
         empdag->type = EmpDag_Bilevel;
         break;
      case EmpDag_RootEquil:
         assert (empdag->features.vicons == EmpDag_Vi_SimpleConstraint);
         empdag->type = EmpDag_Epec;
         break;
      default:
         goto err_rootnode;
      } 

      return OK;
   }

   if (empdag->features.constraint & EmpDag_EquilConstraint || 
       empdag->features.constraint & EmpDag_ViConstraint) {
      assert(empdag->features.rootnode != EmpDag_RootVi);

      switch (empdag->features.rootnode) {
      case EmpDag_RootOpt:
         empdag->type = EmpDag_Mpec;
         break;
      case EmpDag_RootEquil:
         assert (empdag->features.vicons == EmpDag_Vi_SimpleConstraint);
         empdag->type = EmpDag_Epec;
         break;
      default:
         goto err_rootnode;
      } 

      return OK;
   }

   empdag->type = EmpDag_Complex;
 
   trace_empdag("[empdag] the EMPDAG of %s model '%.*s' #%u has type %s\n",
                mdl_fmtargs(empdag->mdl), empdag_typename(empdag->type));

   return OK;

err_rootnode:
   error("[empdag] ERROR: Invalid root node type %u\n", empdag->features.rootnode);
   return Error_RuntimeError;
}

int empdag_fini(EmpDag *empdag)
{
   if (empdag->finalized) { return OK; }

   /* Reset Empdag type */
   empdag->type = EmpDag_Unset;

  /* ----------------------------------------------------------------------
   * First Part: finalize MPs and identify EMPDAG root, if needed
   * ---------------------------------------------------------------------- */

   unsigned mp_len = empdag->mps.len;

   MathPrgm **mps = empdag->mps.arr;
   for (unsigned i = 0; i < mp_len; ++i) {
      if (!mps[i]) { continue; }

      S_CHECK(mp_finalize(mps[i]));
   }

   /* Outside of the empinfo file, need to do this*/
   if (mp_len) {
      S_CHECK(empdag_infer_roots(empdag));
   } else {
      //S_CHECK(empdag_checkroot());
   }

  /* ----------------------------------------------------------------------
   * Second Part: check EMPDAG, infer the type
   * ---------------------------------------------------------------------- */

   S_CHECK(empdag_check(empdag));

   S_CHECK(empdag_infertype(empdag));

   empdag->finalized = true;

   if (mp_len == 0) { return OK; }

   pr_info("\nEMPDAG for %s model '%.*s' #%u has type %s\n", mdl_fmtargs(empdag->mdl),
           empdag_typename(empdag->type));

   unsigned n_opt = 0, n_vi = 0, n_ccflib = 0, n_hidden = 0, n_dual = 0, n_fooc = 0, n_mps = 0;

   for (unsigned i = 0, len = empdag->mps.len; i < len; ++i) {
      MathPrgm *mp = empdag->mps.arr[i];

      if (!mp) { continue; }

      n_mps++;

      if (mp_ishidden(mp)) { n_hidden++; }
      else if (mp_isopt(mp)) { n_opt++; }
      else if (mp_isvi(mp)) { n_vi++; }
      else if (mp_isccflib(mp)) { n_ccflib++; }
      else if (mp_isdual(mp)) { n_dual++; }
      else if (mp_isfooc(mp)) { n_fooc++; }
      else { return error_runtime(); }
   }

   struct lineppty l = {.colw = 40, .mode = PO_INFO, .ident = 2 };

   printuint(&l, "MPs", n_mps);
   l.ident += 2;
   printuint(&l, "OPT MPs", n_opt);
   printuint(&l, "VI MPs", n_vi);
   printuint(&l, "CCFLIB MPs", n_ccflib);
   printuint(&l, "hidden MPs", n_hidden);
   printuint(&l, "dual MPs", n_dual);
   printuint(&l, "FOOC MPs", n_fooc);
   l.ident -= 2;
   printuint(&l, "Nash nodes", empdag->nashs.len);
   printuint(&l, "VF arcs", empdag->mps.Varcs ? empdag->mps.Varcs->len : 0);
   printuint(&l, "CTRL arcs", empdag->mps.Carcs ? empdag->mps.Carcs->len : 0);
   printuint(&l, "Children of Nash nodes", empdag->nashs.arcs ? empdag->nashs.arcs->len : 0);

   unsigned n_roots = empdag->roots.len;

   if (n_roots == 1) {
      daguid_t root = empdag->roots.arr[0];
      printout(PO_INFO, "\n%*sRoot is %s(%s)\n", l.ident, "", daguid_type2str(root),
                empdag_getname(empdag, root));
   } else if (n_roots > 1) {
      printout(PO_INFO, "\n%*sRoots are:\n", l.ident, "");
      l.ident += 2;

      for (unsigned i = 0, len = empdag->roots.len; i < len; ++i) {
         daguid_t root =  empdag->roots.arr[i];
         printout(PO_INFO, "%*sRoot is %s(%s)\n", l.ident, "", daguid_type2str(root),
                  empdag_getname(empdag, root));
      }

      l.ident -= 2;
   }

   printstr(PO_INFO, "\n");

   return empdag_export(empdag->mdl);
}

void empdag_rel(EmpDag *empdag)
{

   daguidarray_empty(&empdag->roots);
   mpidarray_empty(&empdag->minimaxi.mps2reformulate);
   mpidarray_empty(&empdag->minimaxi.saddle_path_starts);
   mpidarray_empty(&empdag->mps2instantiate);
   mpidarray_empty(&empdag->mps_newly_created);

   mpidarray_empty(&empdag->transformations.fenchel_dual_nodal);
   mpidarray_empty(&empdag->transformations.fenchel_dual_subdag);
   mpidarray_empty(&empdag->transformations.epi_dual_nodal);
   mpidarray_empty(&empdag->transformations.epi_dual_subdag);
   mpidarray_empty(&empdag->transformations.fooc.src);
   mpidarray_empty(&empdag->transformations.fooc.vi);
   mpidarray_empty(&empdag->objfn.src);
   mpidarray_empty(&empdag->objfn.dst);

   unsigned len = empdag->objfn_maps.mps.len;
   mpidarray_empty(&empdag->objfn_maps.mps);

   if (empdag->objfn_maps.lequs) {
      for (unsigned i = 0; i < len; ++i) {
         lequ_free(empdag->objfn_maps.lequs[i]);
      }
   }

   free((void*)empdag->objfn_maps.lequs);

   /****************************************************************************
   * Free MP and Nash data structures
   ****************************************************************************/

   dagmp_array_free(&empdag->mps);
   dagnash_array_free(&empdag->nashs);

   if (empdag->empdag_next) {
      empdag_rel(empdag->empdag_next);
      FREE(empdag->empdag_next);
   }

}

/**
 * @brief Initialize the DAG from a simple model
 *
 * @param mdl   the model
 * @param v_no  variables that should not be included in the MP
 *
 * @return      the error code
 */
int empdag_initDAGfrommodel(Model *mdl, const Avar *v_no)
{
   assert(mdl_is_rhp(mdl));
   RhpContainerData *cdat = (RhpContainerData *)mdl->ctr.data;
   EmpDag *empdag = &mdl->empinfo.empdag;

   if (empdag_exists(empdag)) {
      error("[empdag] ERROR: the %s model '%.*s' #%u already has an empdag",
            mdl_fmtargs(mdl));
   } else if (empdag->type != EmpDag_Empty && empdag->type != EmpDag_Unset) {
      error("[empdag] ERROR: cannot initialize full EMPDAG from existing DAG type"
            " %s, must be a simple DAG type\n", empdag_typename(empdag->type));
      return Error_EMPRuntimeError;
   }

   ModelType mdltype;
   S_CHECK(mdl_gettype(mdl, &mdltype));

   if (mdltype == MdlType_cns) {
      error("%s ::  model \"%s\" (#%u): cannot init an empdag from modeltype %s",
            __func__,  mdl_getname(mdl), mdl->id, mdltype_name(mdltype));
      return Error_NotImplemented;
   }

  /* ----------------------------------------------------------------------
   * If we have OVF, we need to set their MP first
   * ---------------------------------------------------------------------- */
   OvfInfo *ovfinfo = mdl->empinfo.ovf;
   OvfDef *ovfdef = ovfinfo ? ovfinfo->ovf_def : NULL;

   while (ovfdef) {
      S_CHECK(ovf_forcefinalize(mdl, ovfdef));
      ovfdef = ovfdef->next;
   }

   RhpSense sense = empdag->simple_data.sense;
   rhp_idx objequ = empdag->simple_data.objequ;
   rhp_idx objvar = empdag->simple_data.objvar;
   assert(valid_sense(sense));
   MathPrgm *mp;

   char *mp_name = mdl->commondata.name ? mdl->commondata.name : "user model";
   A_CHECK(mp, empdag_newmpnamed(empdag, sense, mp_name));

   if (valid_ei(objequ)) {
      S_CHECK(mp_setobjequ(mp, objequ));
   }

   if (valid_vi(objvar)) {
      S_CHECK(mp_setobjvar(mp, objvar));
   }

   /*  TODO(xhub) this is a hack */
   mp->probtype = mdltype;

   VarMeta * restrict vmd = mdl->ctr.varmeta;

   CMatElt **vars = cdat->cmat.vars;
   for (rhp_idx vi = 0, len = cdat->total_n; vi < len; ++vi) {
      /* The objective variable should not be added (already added via
     * mp_setobjvar. The ovf variable to going to be removed and
     * therefore should not be added.*/
      if (vars[vi]) {
         if (vi != objvar && !avar_contains(v_no, vi) && !valid_mpid(vmd[vi].mp_id)) {
            S_CHECK(mp_addvar(mp, vi));
         }
      } else {
         vmd[vi].ppty |= VarIsDeleted;
      }
   }

   EquMeta * restrict emd = mdl->ctr.equmeta;

   CMatElt **equs = cdat->cmat.equs;
   for (rhp_idx ei = 0, len = cdat->total_m; ei < len; ++ei) {
      if (equs[ei]) {
         if (ei != objequ && !valid_mpid(emd[ei].mp_id)) {
            S_CHECK(mp_addconstraint(mp, ei));
         }
      } else {
         emd[ei].ppty |= EquPptyIsDeleted;
      }
   }

   S_CHECK(mp_finalize(mp));

   return OK;
}

void empdag_reset_type(EmpDag* empdag)
{
   empdag->type = EmpDag_Unset;
}

int empdag_initfrommodel(EmpDag * restrict empdag, const Model *mdl_up)
{
   rhp_idx objvar, objequ;
   RhpSense sense;

   S_CHECK(mdl_getobjvar(mdl_up, &objvar));
   S_CHECK(mdl_getobjequ(mdl_up, &objequ));
   S_CHECK(mdl_getsense(mdl_up, &sense));

   empdag->simple_data.sense = sense;
   empdag->simple_data.objequ = objequ;
   empdag->simple_data.objvar = objvar;

 
   if ((sense == RhpMin || sense == RhpMax) && !valid_vi(objvar) && !valid_ei(objequ)) {
      error("[empdag] ERROR: cannot initialized from %s optimization model '%.*s' #%u"
            " with no valid objvar or objequ\n", mdl_fmtargs(mdl_up));
      return Error_RuntimeError;
   }

   trace_empdag("[empdag] empdag of %s model '%.*s' #%u initialized with sense: "
                "%s; objvar: '%s'; objequ: '%s' from %s model '%.*s' #%u\n",
                mdl_fmtargs(empdag->mdl), sense2str(sense),
                mdl_printvarname(empdag->mdl, objvar),
                mdl_printequname(empdag->mdl, objequ), mdl_fmtargs(mdl_up));

   return OK;
}

/**
 * @brief Duplicate an EMPDAG
 *
 * @param empdag     the new EMPDAG
 * @param empdag_up  the original EMPDAG
 * @param mdl        the new model
 *
 * @return           the error code
 */
int empdag_dup(EmpDag * restrict empdag, const EmpDag * restrict empdag_up, Model *mdl)
{
   if (empdag_up->type == EmpDag_Unset) {
      errormsg("[emdpag] ERROR: initializing from an unset EMPDAG\n");
      return Error_EMPRuntimeError;
   }
   assert(empdag_up->type != EmpDag_Empty && empdag_up->type != EmpDag_Unset);
   assert(empdag->type == EmpDag_Unset);

   empdag->type = empdag_up->type;
   memcpy(&empdag->features, &empdag_up->features, sizeof(EmpDagFeatures));
   memcpy(&empdag->node_stats, &empdag_up->node_stats, sizeof(EmpDagNodeStats));
   memcpy(&empdag->arc_stats, &empdag_up->arc_stats, sizeof(EmpDagEdgeStats));

   empdag->finalized = false;
   empdag->uid_root = empdag_up->uid_root;

   /* Do not borrow to avoid circular reference */
   empdag->mdl = mdl;

   empdag->empdag_up = empdag_up;

   trace_empdag("[empdag] empdag of %s model '%.*s' #%u initialized from empdag of "
                "%s model '%.*s' #%u with type %s\n", mdl_fmtargs(mdl), 
                mdl_fmtargs(empdag_up->mdl), empdag_typename(empdag_up->type));

   if (empdag_up->type == EmpDag_Empty) {
      memcpy(&empdag->simple_data, &empdag_up->simple_data, sizeof(empdag->simple_data));
      return OK;
   }

   assert(empdag_up->mps.len > 0);

   /* TODO(xhub) should this be done elsewhere? */
   S_CHECK(mdl_settype(mdl, MdlType_emp));

   S_CHECK(dagmp_array_copy(&empdag->mps, &empdag_up->mps, empdag->mdl));
   S_CHECK(dagnash_array_copy(&empdag->nashs, &empdag_up->nashs, empdag->mdl));

   S_CHECK(daguidarray_copy(&empdag->roots, &empdag_up->roots));

   /* HACK: remove this */
   S_CHECK(mpidarray_copy(&empdag->transformations.fooc.vi, &empdag_up->transformations.fooc.vi));
   S_CHECK(mpidarray_copy(&empdag->transformations.fooc.src, &empdag_up->transformations.fooc.src));

   return OK;
}

int empdag_addmp(EmpDag *empdag, RhpSense sense, unsigned *id)
{
   MathPrgm *mp;
   A_CHECK(mp, mp_new(empdag->mps.len, sense, empdag->mdl));

   S_CHECK(dagmp_array_add(&empdag->mps, mp, NULL));

   *id = mp->id;

   return OK;
}

MathPrgm *empdag_newmp(EmpDag *empdag, RhpSense sense)
{
   unsigned mp_id;
   SN_CHECK(empdag_addmp(empdag, sense, &mp_id));
   return empdag->mps.arr[mp_id];
}

int empdag_addmpnamed(EmpDag *empdag, RhpSense sense, const char *name, unsigned *id)
{
   MathPrgm *mp;
   A_CHECK(mp, mp_new(empdag->mps.len, sense, empdag->mdl));

   *id = mp->id;

   char *name_cpy = name ? strdup(name) : NULL;

   assert((name_cpy && name) || (!name_cpy && !name));

   return dagmp_array_add(&empdag->mps, mp, name_cpy);
}

MathPrgm *empdag_newmpnamed(EmpDag *empdag, RhpSense sense, const char *name)
{
   unsigned mp_id;
   SN_CHECK(empdag_addmpnamed(empdag, sense, name, &mp_id));
   return empdag->mps.arr[mp_id];
}

int empdag_addnash(EmpDag *empdag, unsigned *id)
{
   Nash *nash;
   A_CHECK(nash, nash_new(empdag->nashs.len, empdag->mdl));

   S_CHECK(dagnash_array_add(&empdag->nashs, nash, NULL));

   *id = nash->id;

   empdag->finalized = false;

   return OK;
}

int empdag_addnashnamed(EmpDag *empdag, const char *name, unsigned *id)
{
   Nash *nash;
   A_CHECK(nash, nash_new(empdag->nashs.len, empdag->mdl));

   *id = nash->id;

   char *name_cpy = name ? strdup(name) : NULL;

   assert((name_cpy && name) || (!name_cpy && !name));

   empdag->finalized = false;

   return dagnash_array_add(&empdag->nashs, nash, name_cpy);
}

Nash* empdag_newnash(EmpDag *empdag)
{
   nashid_t nash_id;
   SN_CHECK(empdag_addnash(empdag, &nash_id));

   empdag->finalized = false;

   return empdag->nashs.arr[nash_id];
}

Nash* empdag_newnashnamed(EmpDag *empdag, char* name)
{
   nashid_t nashid;
   SN_CHECK(empdag_addnashnamed(empdag, name, &nashid));

   empdag->finalized = false;

   return empdag->nashs.arr[nashid];
}

int empdag_rootsaddnash(EmpDag *empdag, nashid_t nashid)
{
   S_CHECK(chk_nashid_(empdag, nashid));

   empdag->finalized = false;

   return rhp_uint_addsorted(&empdag->roots, nashid2uid(nashid));
}

int empdag_rootsaddmp(EmpDag *empdag, mpid_t mpid)
{
   S_CHECK(chk_mpid_(empdag, mpid));

   empdag->finalized = false;

   return rhp_uint_addsorted(&empdag->roots, mpid2uid(mpid));
}

int empdag_rootsadd(EmpDag *empdag, daguid_t uid)
{
   if (uidisMP(uid)) {
      return empdag_rootsaddmp(empdag, uid2id(uid));
   }

   return empdag_rootsaddnash(empdag, uid2id(uid));
}

int empdag_setroot(EmpDag *empdag, daguid_t uid)
{
   S_CHECK(empdag_rootsadd(empdag, uid));

   empdag->uid_root = uid;

   trace_empinterp("[empinterp] setting %s(%s) as EMPDAG root\n",
                   daguid_type2str(uid), empdag_getname(empdag, uid));

   return OK;
}

int empdag_getmpparents(const EmpDag *empdag, const MathPrgm *mp,
                        const DagUidArray **parents_uid)
{
   unsigned mp_id = mp->id;
   S_CHECK(chk_mpid_(empdag, mp_id));

   *parents_uid = &empdag->mps.rarcs[mp_id];

   return OK;
}

int empdag_getnashparents(const EmpDag *empdag, const Nash *nash,
                         const UIntArray **parents_uid)
{
   nashid_t nashid = nash->id;
   S_CHECK(chk_mpid_(empdag, nashid));

   *parents_uid = &empdag->nashs.rarcs[nashid];

   return OK;
}

int empdag_nashaddmpbyid(EmpDag *empdag, nashid_t nashid, mpid_t mpid)
{
   S_CHECK(chk_nashid_(empdag, nashid));
   S_CHECK(chk_mpid_(empdag, mpid));

   MathPrgm *mp = empdag->mps.arr[mpid];
   MpType type = mp->type;
   if (type == MpTypeUndef) {
      error("[empdag] ERROR: the MP(%s) has an undefined type",
            empdag_getmpname(empdag, mpid));
      return Error_RuntimeError;
   }

   if (type == MpTypeCcflib) {
      S_CHECK(empdag_mp_needs_instantiation(empdag, mpid));
   }

   S_CHECK(daguidarray_adduniqsorted(&empdag->nashs.arcs[nashid], mpid2uid(mpid)));
   S_CHECK(daguidarray_adduniqsorted(&empdag->mps.rarcs[mpid], nashid2uid(nashid)));

   trace_empdag("[empdag] adding an edge of type %s from Nash(%s) to MP(%s)\n",
                linktype2str(LinkArcNash), empdag_getnashname(empdag, nashid),
                empdag_getmpname(empdag, mpid));

   empdag->finalized = false;

   return OK;
}

int empdag_mpVFmpbyid(EmpDag *empdag, mpid_t mpid_parent, const ArcVFData *arc)
{
   mpid_t mpid_child = arc->mpid_child;
   S_CHECK(chk_mpid_(empdag, mpid_parent));
   S_CHECK(chk_mpid_(empdag, mpid_child));

   assert(mpid_parent != mpid_child); assert(chk_Varc(arc, empdag->mdl));

   S_CHECK(rhp_uint_adduniqnofail(&empdag->mps.rarcs[mpid_child], rarcVFuid(mpid2uid(mpid_parent))));

   trace_empdag("[empdag] adding an edge of type %s from MP(%s) to MP(%s)\n",
                arcVFType2str(arc->type), empdag_getmpname(empdag, mpid_parent),
                empdag_getmpname(empdag, mpid_child));


   /* We store the details of the VF edge on the parent MP */
   S_CHECK(_edgeVFs_add(&empdag->mps.Varcs[mpid_parent], *arc));

   empdag->finalized = false;

   return OK;
}

int empdag_mpCTRLmpbyid(EmpDag *empdag, mpid_t mpid_parent, mpid_t mpid_child)
{
   S_CHECK(chk_mpid_(empdag, mpid_parent));
   S_CHECK(chk_mpid_(empdag, mpid_child));
   assert(mpid_parent != mpid_child);

   S_CHECK(rhp_uint_adduniqsorted(&empdag->mps.Carcs[mpid_parent], mpid2uid(mpid_child)));
   S_CHECK(rhp_uint_adduniqsorted(&empdag->mps.rarcs[mpid_child], rarcCTRLuid(mpid2uid(mpid_parent))));

   trace_empdag("[empdag] adding an edge of type %s from MP(%s) to MP(%s)\n",
                linktype2str(LinkArcCtrl), empdag_getmpname(empdag, mpid_parent),
                empdag_getmpname(empdag, mpid_child));

   empdag->finalized = false;

   return OK;
}

int empdag_mpCTRLnashbyid(EmpDag *empdag, mpid_t mp_id, nashid_t nashid)
{
   S_CHECK(chk_mpid_(empdag, mp_id));
   S_CHECK(chk_nashid_(empdag, nashid));

   S_CHECK(rhp_uint_adduniqsorted(&empdag->mps.Carcs[mp_id], nashid2uid(nashid)));
   S_CHECK(rhp_uint_adduniqsorted(&empdag->nashs.rarcs[nashid], rarcCTRLuid(mpid2uid(mp_id))));

   trace_empdag("[empdag] adding an edge of type %s from MP(%s) to Nash(%s)\n",
                linktype2str(LinkArcCtrl), empdag_getmpname(empdag, mp_id),
                empdag_getnashname(empdag, nashid));

   empdag->finalized = false;

   return OK;
}
/* TODO: empdag->nashs.arcs contains daguid_t not mpid_t */
//int empdag_nashaddmpsbyid(EmpDag *empdag, nashid_t nashid,
//                         const MpIdArray *arr)
//{
//
//   S_CHECK(chk_nashid(empdag, nashid));
//
//   for (unsigned i = 0, len = arr->len; i < len; ++i) {
//      unsigned mp_id = arr->arr[i];
//      S_CHECK(chk_mpid(empdag, mp_id));
//      S_CHECK(rhp_uint_adduniqsorted(&empdag->mps.rarcs[mp_id], nashid));
//   }
//
//   S_CHECK(rhp_uint_addset(&empdag->nashs.arcs[nashid], arr));
//
//   empdag->finalized = false;
//
//   return OK;
//}

int empdag_addarc(EmpDag *empdag, daguid_t uid_parent, daguid_t uid_child,
                  EmpDagLink *arc)
{

   unsigned id_parent = uid2id(uid_parent);
   unsigned id_child = uid2id(uid_child);

   if (uidisMP(uid_parent)) {
      if (uidisMP(uid_child)) {
         assert(id_parent != id_child);

         if (arc->type == LinkArcVF) {
            return empdag_mpVFmpbyid(empdag, id_parent, &arc->Varc);
         }
         if (arc->type == LinkArcCtrl) {
            return empdag_mpCTRLmpbyid(empdag, id_parent, id_child);
         }

         error("[empdag] ERROR: while processing an edge, unknown type %d\n",
               arc->type);
         return Error_RuntimeError;
      }

      assert(arc->type == LinkArcCtrl);
      return empdag_mpCTRLnashbyid(empdag, id_parent, id_child);
   }

   /* ---------------------------------------------------------------------
    * 
    * --------------------------------------------------------------------- */
   if (!uidisMP(uid_child)) {
      errormsg("[empdag] ERROR while processing edge: a Nash parent node can "
               "only a child of type MP.\n");
      return Error_RuntimeError;
   }

   return empdag_nashaddmpbyid(empdag, id_parent, id_child);
}

int empdag_getmpbyname(const EmpDag *empdag, const char * const name,
                       MathPrgm **mp)
{
   unsigned idx = _findnamecase(empdag->mps.names, empdag->mps.len, name);
   if (idx == UINT_MAX) {
      error("%s :: Could not find EMPDAG mp named %s\n", __func__,
               name);
      *mp = NULL;
      return Error_NotFound;
   }

   *mp = empdag->mps.arr[idx];

   return OK;
}

int empdag_getmpidbyname(const EmpDag *empdag, const char * const name,
                         unsigned *id)
{
   unsigned idx = _findnamecase(empdag->mps.names, empdag->mps.len, name);
   *id = idx;

   if (idx == UINT_MAX) {
      error("%s :: Could not find EMPDAG MP named %s\n", __func__,
               name);
      return Error_NotFound;
   }

   return OK;
}

int empdag_getmpbyid(const EmpDag *empdag, mpid_t id, MathPrgm **mp)
{
   S_CHECK(chk_mpid_(empdag, id));

   *mp = empdag->mps.arr[id];
   assert(*mp);

   return OK;
}

int empdag_getnashbyname(const EmpDag *empdag, const char * const name,
                        Nash **nash)
{
   unsigned idx = _findnamecase(empdag->nashs.names, empdag->nashs.len, name);
   if (idx == UINT_MAX) {
      error("%s :: Could not find Nash node named %s\n", __func__,
               name);
      *nash = NULL;
      return Error_NotFound;
   }

   *nash = empdag->nashs.arr[idx];

   return OK;
}

int empdag_getnashidbyname(const EmpDag *empdag, const char * const name,
                          unsigned *id)
{
   unsigned idx = _findnamecase(empdag->nashs.names, empdag->nashs.len, name);
   *id = idx;
   if (idx == UINT_MAX) {
      error("%s :: Could not find Nash node named %s\n", __func__,
               name);
      return Error_NotFound;
   }

   return OK;
}

int empdag_getnashbyid(const EmpDag *empdag, unsigned id, Nash **nash)
{
   S_CHECK(chk_nashid_(empdag, id));

   *nash = empdag->nashs.arr[id];

   return OK;
}

int empdag_mpCTRLmpbyname(EmpDag *empdag, const char *parent, const char *child)
{
   unsigned id_parent, id_child;
   S_CHECK(empdag_getmpidbyname(empdag, parent, &id_parent));
   S_CHECK(empdag_getmpidbyname(empdag, child, &id_child));

   return empdag_mpCTRLmpbyid(empdag, id_parent, id_child);
}

int empdag_mpCTRLnashbyname(EmpDag *empdag, const char *parent, const char *child)
{
   unsigned id_parent, id_child;
   S_CHECK(empdag_getmpidbyname(empdag, parent, &id_parent));
   S_CHECK(empdag_getnashidbyname(empdag, child, &id_child));

   return empdag_mpCTRLnashbyid(empdag, id_parent, id_child);
}

int empdag_check(EmpDag *empdag)
{
   int status = OK;

   const DagUidArray *roots = &empdag->roots;
   const DagMpArray *mps = &empdag->mps;
   const DagNashArray *nashs = &empdag->nashs;

   unsigned mp_len = empdag_num_mp(empdag);
   unsigned nash_len = empdag_num_nash(empdag);
   unsigned roots_len = empdag->roots.len;

   empdag->node_stats.num_mps = mp_len;
   empdag->node_stats.num_nashs = nash_len;

   if (roots_len == 0) {
      if (mp_len > 0) {
         error("[empdag:check] ERROR in %s model '%.*s' #%u: There are %u MPs, "
               "but no root in the EMPDAG\n", mdl_fmtargs(empdag->mdl), mp_len);
         return Error_EMPRuntimeError;
      }

      return OK;
   }

   if (roots_len == 1 && !valid_uid(empdag->uid_root)) {
      error("[empdag:check] ERROR in %s model '%.*s' #%u: there is 1 root, but "
            "it as not been tagged as the root of the empdag\n",
            mdl_fmtargs(empdag->mdl));
      return Error_EMPRuntimeError;
   }

   /* Check if all MP node:
    * - are finalized
    * - have parents or are roots */

   MathPrgm ** const mps_arr = mps->arr;
   for (unsigned i = 0; i < mp_len; ++i) {
      const MathPrgm *mp = mps_arr[i];
      if (!mp) continue;

      if (!(mp->status & MpFinalized)) {
         error("[empdag:check] ERROR: MP(%s) has not been finalized\n",
                  empdag_getmpname(empdag, mp->id));
         status = status_update(status, Error_EMPRuntimeError);
      }

      /* If we have no parent and are not in the root set, we error */
      if ((mps->rarcs[i].len == 0) && !mp_ishidden(mp) &&
         (rhp_uint_findsorted(roots, mpid2uid(i)) == UINT_MAX)) {
         error("[empdag:check] ERROR: MP(%s) is not in the EMPDAG\n",
                  empdag_getmpname(empdag, mp->id));
         status = status_update(status, Error_EMPRuntimeError);
      }
   }

   /* Check if all Nash node have parents or are roots */
   for (unsigned i = 0; i < nash_len; ++i) {
      const Nash *nash = nashs->arr[i];
      if (!nash) continue;

      /* If we have no parent and are not in the root set, we error */
      if ((nashs->rarcs[i].len == 0) &&
         (rhp_uint_findsorted(roots, nashid2uid(i)) == UINT_MAX)) {
         error("[empdag:check]  Nash(%s) is not in the EMPDAG\n",
                 empdag_getnashname(empdag, nash->id));
         status = status_update(status, Error_EMPRuntimeError);
      }
   }

   int status2 = empdag_analysis(empdag);

   return status == OK ? status2 : status;
}

/**
 * @brief Find the VF edge between two MPs
 *
 * @param empdag       the empdag
 * @param mpid_parent  the parent MP
 * @param mpid_child   the child MP
 *
 * @return             NULL if there is no such edge, otherwise the edge
 */
const ArcVFData* empdag_find_Varc(const EmpDag *empdag, unsigned mpid_parent,
                                 unsigned mpid_child)
{
   SN_CHECK(chk_mpid_(empdag, mpid_parent));
   SN_CHECK(chk_mpid_(empdag, mpid_child));

   struct VFedges *Varcs = &empdag->mps.Varcs[mpid_parent];

   if (Varcs->len == 0) return NULL;

   unsigned edgeVF_idx = _edgeVFs_find(Varcs, mpid_child);

   if (edgeVF_idx == UINT_MAX) { return NULL; }
   
   return &Varcs->arr[edgeVF_idx];
}

/**
 * @brief Set the name of an MP
 *
 * This function copies the string in name
 *
 * @param empdag the EMPDAG 
 * @param mpid  the MP ID
 * @param name   the name
 *
 * @return      the error code
 */
int empdag_setmpname(EmpDag *empdag, mpid_t mpid, const char *const name)
{
   S_CHECK(chk_mpid_(empdag, mpid));

   if (empdag->mps.names[mpid]) {
      error("%s :: MP already has name \"%s\"\n", __func__, empdag->mps.names[mpid]);
      return Error_UnExpectedData;
   }

   empdag->mps.names[mpid] = strdup(name);
   if (!empdag->mps.names[mpid]) {
      error("%s :: couldn't allocate memory for %s!\n", __func__, name);
      return Error_SystemError;
   }

   return OK;
}

int empdag_nash_getchildren(const EmpDag *empdag, nashid_t nashid,
                           DagUidArray **mps)
{
   S_CHECK(chk_nashid_(empdag, nashid));

   *mps = &empdag->nashs.arcs[nashid];

   return OK;
}

const char* empdag_printid(const EmpDag *empdag)
{
   return empdag->mdl->commondata.name;
}

int empdag_reserve_mp(EmpDag *empdag, unsigned reserve)
{
   return dagmp_array_reserve(&empdag->mps, reserve);
}

static int empdag_mpdelete(EmpDag *empdag, unsigned mp_id)
{
   if (chk_mpid_(empdag, mp_id) != OK) {
      error("[empdag] ERROR: seeking to delete MP ID #%u, which does not exists", mp_id);
      return Error_RuntimeError;
   }

   if (mp_id != empdag->mps.len-1) {
      error("[empdag] ERROR: seeking to delete MP ID #%u, but it is not the last one\n",
            mp_id);
      return Error_RuntimeError;
   }

   unsigned n_parents = empdag->mps.rarcs[mp_id].len;
   if (n_parents > 0) {
      error("[empdag] ERROR: seeking to delete MP ID #%u, but it has %u parents.\n",
            mp_id, n_parents);
   }

   unsigned n_children = empdag->mps.Carcs[mp_id].len;
   if (n_children > 0) {
      error("[empdag] ERROR: seeking to delete MP ID #%u, but it has %u children.\n",
            mp_id, n_children);
   }

   empdag->mps.len--;
   mp_free(empdag->mps.arr[mp_id]);
   free((char*)empdag->mps.names[mp_id]);

   return OK;
}

static int empdag_delnash(EmpDag *empdag, nashid_t nashid)
{
   if (chk_nashid_(empdag, nashid) != OK) {
      error("[empdag] ERROR: seeking to delete Nash ID #%u, which does not exists",
            nashid);
      return Error_RuntimeError;
   }

   if (nashid != empdag->nashs.len-1) {
      error("[empdag] ERROR: seeking to delete Nash ID #%u, but it is not the "
            "last one\n", nashid);
      return Error_RuntimeError;
   }

   unsigned n_parents = empdag->nashs.rarcs[nashid].len;
   if (n_parents > 0) {
      error("[empdag] ERROR: seeking to delete Nash ID #%u, but it has %u parents.\n",
            nashid, n_parents);
   }

   unsigned n_children = empdag->nashs.arcs[nashid].len;
   if (n_children > 0) {
      error("[empdag] ERROR: seeking to delete Nash ID #%u, but it has %u children.\n",
            nashid, n_children);
   }

   empdag->nashs.len--;

   return OK;
}

int empdag_delete(EmpDag *empdag, daguid_t uid)
{

   if (uidisMP(uid)) {
      return empdag_mpdelete(empdag, uid2id(uid));
   }

   return empdag_delnash(empdag, uid2id(uid));
}

int empdag_simple_init(EmpDag *empdag)
{
   EmpDagType type = empdag->type;
   if (type != EmpDag_Unset) {
      const Model *mdl = empdag->mdl;
      error("[empdag] ERROR: EMPDAG for model %.*s #%u already has type %s, expecting %s",
            mdl_getnamelen(mdl), mdl_getname(mdl), mdl->id, empdag_typename(type),
            empdag_typename(EmpDag_Unset));
      return Error_EMPRuntimeError;
   }

   empdag->type = EmpDag_Empty;

   return OK;
}

int empdag_simple_setsense(EmpDag *empdag, RhpSense sense)
{
   assert(empdag->type == EmpDag_Empty);

   switch (sense) {
   case RhpMin:
   case RhpMax:
   case RhpFeasibility:
      empdag->simple_data.sense = sense;
      return OK;
   default:
      error("%s ERROR: unsupported sense %s\n", __func__, sense2str(sense));
      return Error_InvalidValue;
   }
}

int empdag_simple_setobjvar(EmpDag *empdag, rhp_idx objvar)
{
   assert(empdag->type == EmpDag_Empty);
   empdag->simple_data.objvar = objvar;

   return OK;
}

int empdag_simple_setobjequ(EmpDag *empdag, rhp_idx objequ)
{
   assert(empdag->type == EmpDag_Empty);
   assert(valid_ei(objequ));
   empdag->simple_data.objequ = objequ;

   return OK;
}

int empdag_single_MP_to_Nash(EmpDag* empdag)
{
   unsigned n_mps = empdag->mps.len;
   if (n_mps != 1) {
      error("[empdag] ERROR: EMPDAG should have 1 MP, found %u\n", n_mps);
      return Error_EMPRuntimeError;
   }

   mdl_settype(empdag->mdl, MdlType_emp);
   
   MathPrgm *mp = empdag->mps.arr[0];
   nashid_t nashid;
   S_CHECK(empdag_addnashnamed(empdag, "equilibrium", &nashid));
   S_CHECK(empdag_nashaddmpbyid(empdag, nashid, mp->id));
   S_CHECK(empdag_setroot(empdag, nashid2uid(nashid)));

   return OK;
}

static int dfs_mplist(const EmpDag *empdag, daguid_t uid, UIntArray *mplist)
{
   daguid_t * restrict children;
   VarcArray * restrict Varcs;
   unsigned num_children;

   if (uidisMP(uid)) {
      mpid_t id = uid2id(uid);
      S_CHECK(rhp_uint_addsorted(mplist, id));
      UIntArray *Carcs = &empdag->mps.Carcs[id];
      children = Carcs->arr;
      num_children = Carcs->len;

      Varcs = &empdag->mps.Varcs[id];

   } else {

      nashid_t id = uid2id(uid);
      UIntArray *nash_children = &empdag->nashs.arcs[id];
      children = nash_children->arr;
      num_children = nash_children->len;

      assert(num_children > 0);

      Varcs = NULL;
   }


   for (unsigned i = 0; i < num_children; ++i) {
      S_CHECK(dfs_mplist(empdag, children[i], mplist));
   }

   if (!Varcs) { return OK; }

   struct rhp_empdag_arcVF *Vlist = Varcs->arr;
   for (unsigned i = 0, len = Varcs->len; i < len; ++i) {
      S_CHECK(dfs_mplist(empdag, mpid2uid(Vlist[i].mpid_child), mplist));
   }

   return OK;
}

int empdag_subdag_getmplist(const EmpDag *empdag, daguid_t subdag_root,
                            UIntArray *mplist) 
{


   return dfs_mplist(empdag, subdag_root, mplist);
}

//FIXME: looks unused!!
int empdag_finalize(Model *mdl)
{
   EmpDag *empdag = &mdl->empinfo.empdag;
   DagMpArray *mps = &empdag->mps;

   for (unsigned i = 0, len = mps->len; i < len; ++i) {
      MathPrgm *mp = mps->arr[i];

      if (!mp || mp->status & MpFinalized) { continue; }

      S_CHECK(mp_finalize(mp));
   }

   return OK;
}

int empdag_check_hidable_roots(EmpDag *empdag)
{
   MathPrgm * restrict * mps = empdag->mps.arr;
   UIntArray * restrict rarcs = empdag->mps.rarcs;

   for (unsigned i = 0, n_mps = empdag->mps.len; i < n_mps; ++i) {
      MathPrgm *mp = mps[i];

      if (!mp || mp_ishidden(mp)) { continue; }

      if (rarcs[i].len == 0) {
         if (mp_ishidable(mp)) {
            mp_hide(mp);
         } 
      }
   }

   return OK;
}

int empdag_infer_roots(EmpDag *empdag)
{
   DagUidArray *roots = &empdag->roots;
   daguidarray_empty(roots);
   MathPrgm * restrict * mps =  empdag->mps.arr;
   UIntArray * restrict rarcs = empdag->mps.rarcs;

   for (unsigned i = 0, n_mps = empdag->mps.len; i < n_mps; ++i) {
      MathPrgm *mp = mps[i];

      if (!mp || mp_ishidden(mp)) { continue; }

      if (rarcs[i].len == 0) {
         if (mp_ishidable(mp)) {
            mp_hide(mp);
         } else {
            assert(mp->id == i);
            S_CHECK(daguidarray_add(roots, mpid2uid(mp->id)));
         }
      }
   }

   Nash * restrict *nashs = empdag->nashs.arr;
   rarcs = empdag->nashs.rarcs;

   for (unsigned i = 0, num_nash = empdag->nashs.len; i < num_nash; ++i) {
      if (rarcs[i].len == 0) {
         S_CHECK(daguidarray_add(roots, nashid2uid(nashs[i]->id)));
      }
   }

   unsigned n_roots = roots->len;
   if (n_roots == 1) {
      empdag->uid_root = roots->arr[0];
   } else if (n_roots == 0) {
      errormsg("[empdag] ERROR: EMPDAG has no root. The EMPDAG must have one root\n");
      return Error_EMPIncorrectInput;
   }

   return OK;
}

NONNULL static inline bool chk_Varc_basic(const ArcVFBasicData *arc, const Model *mdl)
{
   rhp_idx ei = arc->ei;

   /* check that the equation index is valid */
   if (ei != IdxObjFunc && ei != IdxCcflib && !chk_ei_(ei, ctr_nequs_total(&mdl->ctr))) {
      if (valid_ei(ei)) {
         error("[empdag] ERROR: invalid arcVF equation index %u not in [0,%u)\n",
               ei, ctr_nequs_total(&mdl->ctr));
      } else {
         error("[empdag] ERROR: invalid arcVF equation index '%s'\n",
               badidx2str(ei));
      }

      return false;
   }

   return true;
}

bool chk_Varc(const ArcVFData *arc, const Model *mdl)
{
   if (!valid_Varc(arc)) { return false; }

   switch (arc->type) {
   case ArcVFBasic: return chk_Varc_basic(&arc->basic_dat, mdl);
   case ArcVFMultipleBasic: {
      const ArcVFMultipleBasicData *dat = &arc->basics_dat;
      for (unsigned i = 0, len = dat->len; i < len; ++i) {
         if (!chk_Varc_basic(&dat->list[i], mdl)) { return false; }
      }
      return true;
   }

   default:
      error("%s: Unsupported arcVF type %u", __func__, arc->type);
   }
   return true;
}

unsigned arcVFb_getnumcons(const ArcVFBasicData *arc, const Model *mdl)
{
   rhp_idx ei = arc->ei;

   if (ei == IdxObjFunc) { return 0; }

   assert(chk_ei(mdl, ei, __func__) == OK);

   return mdl->ctr.equmeta[ei].role == EquConstraint ? 1 : 0;
}

bool arcVFb_in_objfunc(const ArcVFBasicData *arc, const Model *mdl)
{
   rhp_idx ei = arc->ei;

   if (ei == IdxObjFunc) { return true; }

   assert(chk_ei(mdl, ei, __func__) == OK);

   return mdl->ctr.equmeta[ei].role == EquObjective;
}

int arcVFb_subei(ArcVFData *arc, rhp_idx ei_old, rhp_idx ei_new)
{
   assert(valid_Varc(arc) && arc->type == ArcVFBasic);

   if (arc->basic_dat.ei == ei_old) {
      arc->basic_dat.ei = ei_new;
   }

   return OK;
}

bool arcVFb_has_abstract_objfunc(const ArcVFData *arc)
{
   assert(valid_Varc(arc) && arc->type == ArcVFBasic);

   return arc->basic_dat.ei == IdxObjFunc;
}

int empdag_substitute_mp_parents_arcs(EmpDag* empdag, mpid_t mpid_old, mpid_t mpid_new)
{
   DagMpArray *mps = &empdag->mps;
   DagNashArray *nashs = &empdag->nashs;

   /* Delete the arcs from the old MP and substitute with the new one */
   mps->rarcs[mpid_new] = mps->rarcs[mpid_old];
   daguidarray_init(&mps->rarcs[mpid_old]);

   DagUidArray *rarcs = &mps->rarcs[mpid_new];
   daguid_t *rarcs_arr = rarcs->arr;

   for (unsigned j = 0, lenj = rarcs->len; j < lenj; ++j) {

      daguid_t uid = rarcs_arr[j];
      if(uidisMP(uid)) {

         mpid_t mpid_parent = uid2id(uid);

         if (rarcTypeVF(uid)) {

            VarcArray *varcs = &mps->Varcs[mpid_parent];
            ArcVFData *varcs_arr = varcs->arr;

            for (unsigned k = 0, lenk = varcs->len; k < lenk; ++k) {

               if (varcs_arr[k].mpid_child == mpid_old) {
                  varcs_arr[k].mpid_child = mpid_new;
                  break;
               }
            }

         } else {

            DagUidArray *carcs_dst = &mps->Carcs[mpid_parent];
            S_CHECK(daguidarray_rmsorted(carcs_dst, mpid2uid(mpid_old)));
            S_CHECK(daguidarray_adduniqsorted(carcs_dst, mpid2uid(mpid_new)));

         }
      } else { /* Parent is Nash */
         nashid_t nashid = uid2id(uid);

         DagUidArray *nash_arcs = &nashs->arcs[nashid];

         S_CHECK(daguidarray_rmsorted(nash_arcs, mpid2uid(mpid_old)));
         S_CHECK(daguidarray_adduniqsorted(nash_arcs, mpid2uid(mpid_new)));
      }
   }

   empdag->finalized = false;

   return OK;
}

int empdag_substitute_mp_child_arcs(EmpDag* empdag, mpid_t mpid_old, mpid_t mpid_new)
{
   DagMpArray *mps = &empdag->mps;
   DagNashArray *nashs = &empdag->nashs;

   /* Delete the arcs from the old MP and substitute with the new one */
   VarcArray *Varcs = &mps->Varcs[mpid_old];
   ArcVFData *Varcs_arr = Varcs->arr;

   S_CHECK(_edgeVFs_copy(&mps->Varcs[mpid_new], Varcs));

   daguid_t uid_old = mpid2uid(mpid_old);
   daguid_t VFuid_old = rarcVFuid(uid_old);
   daguid_t uid_new = mpid2uid(mpid_new);
   daguid_t VFuid_new = rarcVFuid(uid_new);


   for (unsigned i = 0, len = Varcs->len; i < len; ++i) {

      mpid_t mpid_child = Varcs_arr[i].mpid_child;
      assert(chk_mpid_(empdag, mpid_child));

      DagUidArray *rarcs_child = &mps->rarcs[mpid_child];

      S_CHECK(daguidarray_rmsorted(rarcs_child, VFuid_old));
      S_CHECK(daguidarray_adduniqsorted(rarcs_child, VFuid_new));
   }

   DagUidArray * restrict Carcs = &mps->Carcs[mpid_old];
   daguid_t * restrict Carcs_arr = Carcs->arr;
   S_CHECK(daguidarray_copy(&mps->Carcs[mpid_new], Carcs));

   daguid_t Cuid_old = rarcCTRLuid(uid_old);
   daguid_t Cuid_new = rarcCTRLuid(uid_new);

   for (unsigned i = 0, len = Carcs->len; i < len; ++i) {

      daguid_t uid_child = Carcs_arr[i];
      DagUidArray *rarcs_child;

      if (uidisMP(uid_child)) {
         mpid_t mpid_child = uid2id(uid_child);
         assert(chk_mpid_(empdag, mpid_child));

         rarcs_child = &mps->rarcs[mpid_child];

      } else {
         nashid_t nashid_child = uid2id(uid_child);
         assert(chk_nashid_(empdag, nashid_child));

         rarcs_child = &nashs->rarcs[nashid_child];
      }

      S_CHECK(daguidarray_rmsorted(rarcs_child, Cuid_old));
      S_CHECK(daguidarray_adduniqsorted(rarcs_child, Cuid_new));
   }

   empdag->finalized = false;

   return OK;
}

int empdag_substitute_mp_arcs(EmpDag* empdag, mpid_t mpid_old, mpid_t mpid_new)
{
   S_CHECK(empdag_substitute_mp_parents_arcs(empdag, mpid_old, mpid_new));
   return empdag_substitute_mp_child_arcs(empdag, mpid_old, mpid_new);
}

bool arcVFb_chk_equ(const ArcVFBasicData *arc, mpid_t mpid, const Container *ctr)
{
   rhp_idx ei = arc->ei;

   if (!valid_ei(ei)) {
      /* Only the magic value IdxObjFunc or IdxCcflib is valid */
      if (ei == IdxObjFunc || ei == IdxCcflib ) { return true; }

      error("[empdag] ERROR: magic value '%s' used in a VF arc\n", badidx2str(ei));
      return false;
   }

   bool res = ctr_chk_equ_ownership(ctr, ei, mpid);

   if (!res) {
      error("[empdag] ERROR: Equation '%s' does not belong to parent MP\n",
            ctr_printequname(ctr, ei));
   }

   return res;
}
