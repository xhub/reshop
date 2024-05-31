#include "asprintf.h"
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



static int chk_mpid(const EmpDag *empdag, unsigned mp_id)
{
   if (mp_id >= empdag->mps.len) {
      if (!valid_idx(mp_id)) {
         error("%s :: %s\n", __func__, badidx_str(mp_id));
      } else {
         error("%s ERROR: no mp with index %u, the number of mps is %u in "
               "%s model '%.*s' #%u\n", __func__, mp_id, empdag->mps.len,
               mdl_fmtargs(empdag->mdl));
      }
      return Error_NotFound;
   }

   return OK;
}

static int chk_nashid(const EmpDag *empdag, nashid_t nashid)
{
   if (nashid >= empdag->nashs.len) {
      if (!valid_idx(nashid)) {
         error("%s :: %s\n", __func__, badidx_str(nashid));
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
      return "EdgeVFUnset";
   case ArcVFBasic:
      return "EdgeVFBasic";
   case ArcVFMultipleBasic:
      return "EdgeVFMultipleBasic";
   case ArcVFLequ:
      return "EdgeVFLequ";
   case ArcVFMultipleLequ:
      return "EdgeVFMultipleLequ";
   case ArcVFEqu:
      return "EdgeVFEqu";
   case ArcVFMultipleEqu:
      return "EdgeVFMultipleEqu";
   default:
      return "ERROR unknown edgeVFType";
   }
}

void empdag_init(EmpDag *empdag, Model *mdl)
{
   empdag->type = EmpDag_Unset;
   empdag->finalized = false;
   empdag->features.istree = false;

   memset(&empdag->node_stats, 0, sizeof (empdag->node_stats));
   memset(&empdag->edge_stats, 0, sizeof (empdag->edge_stats));

   daguidarray_init(&empdag->roots);
   mpidarray_init(&empdag->mps2reformulate);
   mpidarray_empty(&empdag->saddle_path_starts);
   dagmp_array_init(&empdag->mps);
   dagnash_array_init(&empdag->nashs);

   empdag->uid_root = EMPDAG_UID_NONE;

   empdag->empdag_next = NULL;
   empdag->empdag_up = NULL;

   empdag->simple_data.sense = RhpNoSense;
   empdag->simple_data.objequ = IdxNA;
   empdag->simple_data.objvar = IdxNA;

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
      unsigned nb_mps = empdag->mps.len;

      switch (empdag->features.rootnode) {
      case EmpDag_RootOpt:
         empdag->type = nb_mps > 1 ? EmpDag_Opt : EmpDag_Single_Opt;
         break;
      case EmpDag_RootVi:
         empdag->type = nb_mps > 1 ? EmpDag_Vi : EmpDag_Single_Vi;
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
      S_CHECK(mp_finalize(mps[i]));
   }

   /* Outside of the empinfo file, need to do this*/
   if (mp_len > 0 && empdag->roots.len == 0) {
      UIntArray roots;
      S_CHECK(empdag_collectroots(empdag, &roots));

      if (roots.len == 1) {
         daguid_t rootuid = roots.arr[0];
         S_CHECK(empdag_setroot(empdag, rootuid));
      } else if (roots.len == 0) {
         errormsg("[empdag] ERROR: EMPDAG has no root. The EMPDAG must have one root\n");
         return Error_EMPIncorrectInput;
      } 
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

   printout(PO_INFO, "\nEmpdag for %s model '%.*s' #%u has type %s\n",
            mdl_fmtargs(empdag->mdl), empdag_typename(empdag->type));

   unsigned n_opt = 0, n_vi = 0, n_ccflib = 0;
   for (unsigned i = 0, len = empdag->mps.len; i < len; ++i) {
      MathPrgm *mp = empdag->mps.arr[i]; assert(mp);
      if (mp_isopt(mp)) { n_opt++; }
      else if (mp_isvi(mp)) { n_vi++; }
      else if (mp_isccflib(mp)) { n_ccflib++; }
      else { return error_runtime(); }
   }
   
   struct lineppty l = {.colw = 40, .mode = PO_INFO, .ident = 2 };

   unsigned n_mps = empdag->mps.len;
   printuint(&l, "MPs", n_mps);
   l.ident += 2;
   printuint(&l, "OPT MPs", n_opt);
   printuint(&l, "VI MPs", n_vi);
   printuint(&l, "CCFLIB MPs", n_ccflib);
   l.ident -= 2;
   printuint(&l, "Nash nodes", empdag->nashs.len);
   printuint(&l, "VF edges", empdag->mps.Varcs ? empdag->mps.Varcs->len : 0);
   printuint(&l, "CTRL edges", empdag->mps.Carcs ? empdag->mps.Carcs->len : 0);
   printuint(&l, "Children of Nash nodes", empdag->nashs.arcs ? empdag->nashs.arcs->len : 0);

   unsigned n_roots = empdag->roots.len;

   if (n_roots == 1) {
      daguid_t root = empdag->roots.arr[0];
      printout(PO_INFO, "\n%*sRoot is %s(%s)", l.ident, "", daguid_type2str(root),
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

   return OK;
}

void empdag_rel(EmpDag *empdag)
{

   daguidarray_empty(&empdag->roots);
   mpidarray_empty(&empdag->mps2reformulate);
   mpidarray_empty(&empdag->saddle_path_starts);

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

   MpType mptype;

   ModelType mdltype;
   S_CHECK(mdl_gettype(mdl, &mdltype));

   if (mdltype_isvi(mdltype)) {
      mptype = MpTypeVi;
   } else if (mdltype == MdlType_cns) {
      error("%s ::  model \"%s\" (#%u): cannot init an empdag from modeltype %s",
            __func__,  mdl_getname(mdl), mdl->id, mdltype_name(mdltype));
      return Error_NotImplemented;
   } else {
      //TODO: if we end up here with mdl_probtype == MdlProbType_emp, it's okay
      //But investigating when this happens (CCF/OVF comes to mind) and making
      //sure that we are in one of these cases would be nice
      mptype = MpTypeOpt;
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

   char *mp_name;
   A_CHECK(mp_name, strdup(mdl->commondata.name ? mdl->commondata.name : "user model"));
   A_CHECK(mp, empdag_newmpnamed(empdag, sense, mp_name));

   S_CHECK(mp_settype(mp, mptype));

   if (valid_ei(objequ)) {
      S_CHECK(mp_setobjequ(mp, objequ));
   }

   if (valid_vi(objvar)) {
      S_CHECK(mp_setobjvar(mp, objvar));
   }

   /*  TODO(xhub) this is a hack */
   mp->probtype = mdltype;

   VarMeta * restrict vmd = mdl->ctr.varmeta;

   for (rhp_idx vi = 0, len = cdat->total_n; vi < len; ++vi) {
      /* The objective variable should not be added (already added via
     * mp_setobjvar. The ovf variable to going to be removed and
     * therefore should not be added.*/
      if (cdat->vars[vi]) {
         if (vi != objvar && !avar_contains(v_no, vi) && !valid_mpid(vmd[vi].mp_id)) {
         S_CHECK(mp_addvar(mp, vi));
         }
      } else {
         vmd[vi].ppty |= VarIsDeleted;
      }
   }

   EquMeta * restrict emd = mdl->ctr.equmeta;

   for (rhp_idx ei = 0, len = cdat->total_m; ei < len; ++ei) {
      if (cdat->equs[ei]) {
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

   empdag->type = empdag_up->type;
   memcpy(&empdag->features, &empdag_up->features, sizeof(EmpDagFeatures));
   memcpy(&empdag->node_stats, &empdag_up->node_stats, sizeof(EmpDagNodeStats));
   memcpy(&empdag->edge_stats, &empdag_up->edge_stats, sizeof(EmpDagEdgeStats));
   empdag->finalized = empdag_up->finalized;
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

int empdag_addmpnamed(EmpDag *empdag, RhpSense sense,
                      const char *name, unsigned *id)
{
   MathPrgm *mp;
   A_CHECK(mp, mp_new(empdag->mps.len, sense, empdag->mdl));

   *id = mp->id;

   return dagmp_array_add(&empdag->mps, mp, name);
}

MathPrgm *empdag_newmpnamed(EmpDag *empdag, RhpSense sense,
                              const char *name)
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

   empdag->finalized = false;

   return dagnash_array_add(&empdag->nashs, nash, name);
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
   S_CHECK(chk_nashid(empdag, nashid));

   empdag->finalized = false;

   return rhp_uint_addsorted(&empdag->roots, nashid2uid(nashid));
}

int empdag_rootsaddmp(EmpDag *empdag, mpid_t mpid)
{
   S_CHECK(chk_mpid(empdag, mpid));

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

   return OK;
}

int empdag_getmpparents(const EmpDag *empdag, const MathPrgm *mp,
                        const UIntArray **parents_uid)
{
   unsigned mp_id = mp->id;
   S_CHECK(chk_mpid(empdag, mp_id));

   *parents_uid = &empdag->mps.rarcs[mp_id];

   return OK;
}

int empdag_getnashparents(const EmpDag *empdag, const Nash *nash,
                         const UIntArray **parents_uid)
{
   nashid_t nashid = nash->id;
   S_CHECK(chk_mpid(empdag, nashid));

   *parents_uid = &empdag->nashs.rarcs[nashid];

   return OK;
}

int empdag_nashaddmpbyid(EmpDag *empdag, nashid_t nashid, mpid_t mpid)
{
   S_CHECK(chk_nashid(empdag, nashid));
   S_CHECK(chk_mpid(empdag, mpid));

   MathPrgm *mp = empdag->mps.arr[mpid];
   if (mp->type == MpTypeUndef) {
      error("[empdag] ERROR: the MP(%s) has an undefined type",
            empdag_getmpname(empdag, mpid));
      return Error_RuntimeError;
   }

   S_CHECK(rhp_uint_adduniq(&empdag->nashs.arcs[nashid], mpid2uid(mpid)));
   S_CHECK(rhp_uint_adduniq(&empdag->mps.rarcs[mpid], nashid2uid(nashid)));

   trace_empdag("[empdag] adding an edge of type %s from Nash(%s) to MP(%s)\n",
                arctype_str(ArcNash), empdag_getnashname(empdag, nashid),
                empdag_getmpname(empdag, mpid));

   empdag->finalized = false;

   return OK;
}

int empdag_mpVFmpbyid(EmpDag *empdag, mpid_t id_parent, const ArcVFData *edgeVF)
{
   unsigned id_child = edgeVF->child_id;
   S_CHECK(chk_mpid(empdag, id_parent));
   S_CHECK(chk_mpid(empdag, id_child));
   assert(id_parent != id_child);
   assert(valid_arcVF(edgeVF));

   S_CHECK(rhp_uint_adduniqnofail(&empdag->mps.rarcs[id_child], edgeVFuid(mpid2uid(id_parent))));

   trace_empdag("[empdag] adding an edge of type %s from MP(%s) to MP(%s)\n",
                arcVFType2str(edgeVF->type), empdag_getmpname(empdag, id_parent),
                empdag_getmpname(empdag, id_child));


   /* We store the details of the VF edge on the parent MP */
   S_CHECK(_edgeVFs_add(&empdag->mps.Varcs[id_parent], *edgeVF));

   empdag->finalized = false;

   return OK;
}

int empdag_mpCTRLmpbyid(EmpDag *empdag, mpid_t id_parent, mpid_t id_child)
{
   S_CHECK(chk_mpid(empdag, id_parent));
   S_CHECK(chk_mpid(empdag, id_child));
   assert(id_parent != id_child);

   S_CHECK(rhp_uint_adduniq(&empdag->mps.Carcs[id_parent], edgeCTRLuid(mpid2uid(id_child))));
   S_CHECK(rhp_uint_adduniq(&empdag->mps.rarcs[id_child], edgeCTRLuid(mpid2uid(id_parent))));

   trace_empdag("[empdag] adding an edge of type %s from MP(%s) to MP(%s)\n",
                arctype_str(ArcCtrl), empdag_getmpname(empdag, id_parent),
                empdag_getmpname(empdag, id_child));

   empdag->finalized = false;

   return OK;
}

int empdag_mpCTRLnashbyid(EmpDag *empdag, mpid_t mp_id, nashid_t nashid)
{
   S_CHECK(chk_mpid(empdag, mp_id));
   S_CHECK(chk_nashid(empdag, nashid));

   S_CHECK(rhp_uint_adduniq(&empdag->mps.Carcs[mp_id], edgeCTRLuid(nashid2uid(nashid))));
   S_CHECK(rhp_uint_adduniq(&empdag->nashs.rarcs[nashid], edgeCTRLuid(mpid2uid(mp_id))));

   trace_empdag("[empdag] adding an edge of type %s from MP(%s) to Nash(%s)\n",
                arctype_str(ArcCtrl), empdag_getmpname(empdag, mp_id),
                empdag_getnashname(empdag, nashid));

   empdag->finalized = false;

   return OK;
}

int empdag_nashaddmpsbyid(EmpDag *empdag, nashid_t nashid,
                         const MpIdArray *arr)
{

   S_CHECK(chk_nashid(empdag, nashid));

   for (unsigned i = 0, len = arr->len; i < len; ++i) {
      unsigned mp_id = arr->arr[i];
      S_CHECK(chk_mpid(empdag, mp_id));
      S_CHECK(rhp_uint_adduniq(&empdag->mps.rarcs[mp_id], nashid));
   }

   S_CHECK(rhp_uint_addset(&empdag->nashs.arcs[nashid], arr));

   empdag->finalized = false;

   return OK;
}

int empdag_addarc(EmpDag *empdag, daguid_t uid_parent, daguid_t uid_child,
                  EmpDagArc *arc)
{

   unsigned id_parent = uid2id(uid_parent);
   unsigned id_child = uid2id(uid_child);

   if (uidisMP(uid_parent)) {
      if (uidisMP(uid_child)) {
         assert(id_parent != id_child);

         if (arc->type == ArcVF) {
            return empdag_mpVFmpbyid(empdag, id_parent, &arc->Varc);
         }
         if (arc->type == ArcCtrl) {
            return empdag_mpCTRLmpbyid(empdag, id_parent, id_child);
         }

         error("[empdag] ERROR: while processing an edge, unknown type %d\n",
               arc->type);
         return Error_RuntimeError;
      }

      assert(arc->type == ArcCtrl);
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

int empdag_getmpbyid(const EmpDag *empdag, unsigned id,
                     MathPrgm **mp)
{
   S_CHECK(chk_mpid(empdag, id));

   *mp = empdag->mps.arr[id];

   return OK;
}

unsigned empdag_getmpcurid(const EmpDag *empdag, MathPrgm *mp)
{
   while (mp->next_id != UINT_MAX) {
      unsigned next_id = mp->next_id; 
      if (chk_mpid(empdag, next_id)) {
         return UINT_MAX;
      }
      MathPrgm *mp_next = empdag->mps.arr[next_id];
      mp = mp_next;
   }

   return mp->id;
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
   S_CHECK(chk_nashid(empdag, id));

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
   for (unsigned i = 0; i < mp_len; ++i) {
      const MathPrgm *mp = mps->arr[i];
      if (!mp) continue;

      if (!(mp->status & MpFinalized)) {
         error("[empdag:check] ERROR: MP(%s) has not been finalized\n",
                  empdag_getmpname(empdag, mp->id));
         status = status_update(status, Error_EMPRuntimeError);
      }

      /* If we have no parent and are not in the root set, we error */
      if ((mps->rarcs[i].len == 0) &&
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
const ArcVFData* empdag_find_edgeVF(const EmpDag *empdag, unsigned mpid_parent,
                                 unsigned mpid_child)
{
   SN_CHECK(chk_mpid(empdag, mpid_parent));
   SN_CHECK(chk_mpid(empdag, mpid_child));

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
 * @param mp_id  the MP ID
 * @param name   the name
 *
 * @return      the error code
 */
int empdag_setmpname(EmpDag *empdag, mpid_t mpid, const char *const name)
{
   S_CHECK(chk_mpid(empdag, mpid));

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
                           DagUidArray *mps)
{
   S_CHECK(chk_nashid(empdag, nashid));

   memcpy(mps, &empdag->nashs.arcs[nashid], sizeof(DagUidArray));

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
   if (chk_mpid(empdag, mp_id) != OK) {
      error("[empdag] ERROR: seeking to delete MP ID #%u, which does not exists",
            mp_id);
      return Error_RuntimeError;
   }

   if (mp_id != empdag->mps.len-1) {
      error("[empdag] ERROR: seeking to delete MP ID #%u, but it is not the "
            "last one\n", mp_id);
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

   return OK;
}

static int empdag_delnash(EmpDag *empdag, nashid_t nashid)
{
   if (chk_nashid(empdag, nashid) != OK) {
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
   char *name;
   nashid_t nashid;
   A_CHECK(name, strdup("equilibrium"));
   S_CHECK(empdag_addnashnamed(empdag, name, &nashid));
   S_CHECK(empdag_nashaddmpbyid(empdag, nashid, mp->id));
   S_CHECK(empdag_setroot(empdag, nashid2uid(nashid)));

   return OK;
}

static int dfs_mplist(const EmpDag *empdag, daguid_t uid, UIntArray *mplist)
{
   daguid_t * restrict children;
   struct VFedges * restrict Varcs;
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
      S_CHECK(dfs_mplist(empdag, mpid2uid(Vlist[i].child_id), mplist));
   }

   return OK;
}

int empdag_subdag_getmplist(const EmpDag *empdag, daguid_t subdag_root,
                            UIntArray *mplist) 
{


   return dfs_mplist(empdag, subdag_root, mplist);
}

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

int empdag_collectroots(EmpDag *empdag, UIntArray *roots)
{
   rhp_uint_init(roots);
   MathPrgm **mps =  empdag->mps.arr;
   UIntArray * restrict rarcs = empdag->mps.rarcs;
   for (unsigned i = 0, n_mps = empdag->mps.len; i < n_mps; ++i) {
      if (rarcs[i].len == 0) {
         S_CHECK(rhp_uint_add(roots, mpid2uid(mps[i]->id)));
      }
   }

   Nash **nashs = empdag->nashs.arr;
   rarcs = empdag->nashs.rarcs;
   for (unsigned i = 0, num_nash = empdag->nashs.len; i < num_nash; ++i) {
      if (rarcs[i].len == 0) {
         S_CHECK(rhp_uint_add(roots, nashid2uid(nashs[i]->id)));
      }
   }

   return OK;
}

unsigned arcVFb_getnumcons(ArcVFData *arc, const Model *mdl)
{
   assert(valid_arcVF(arc) && arc->type == ArcVFBasic);

   rhp_idx ei = arc->basic_dat.ei;
   assert(chk_ei(mdl, ei, __func__) == OK);

   return mdl->ctr.equmeta[ei].role == EquConstraint ? 1 : 0;
}

bool arcVFb_has_objequ(ArcVFData *arc, const Model *mdl)
{
   assert(valid_arcVF(arc) && arc->type == ArcVFBasic);

   rhp_idx ei = arc->basic_dat.ei;
   assert(chk_ei(mdl, ei, __func__) == OK);

   return mdl->ctr.equmeta[ei].role == EquObjective;
}

