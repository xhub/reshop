#include "reshop_config.h"
#include "asprintf.h"

#include <limits.h>
#include <string.h>
#include <sys/stat.h>

#include "checks.h"
#include "container.h"
#include "ctr_rhp.h"
#include "empdag_data.h"
#include "empinfo.h"
#include "equvar_helpers.h"
#include "equvar_metadata.h"
#include "mathprgm.h"
#include "mdl_ops.h"
#include "mdl_rhp.h"
#include "mdl.h"
#include "printout.h"
#include "reshop.h"
#include "tlsdef.h"
#include "toplayer_utils.h"
#include "var.h"
#include "win-compat.h"

#define DEFINE_STR() \
DEFSTR(ModelStat_Unset,"Not available") \
DEFSTR(ModelStat_OptimalGlobal,"OptimalGlobal") \
DEFSTR(ModelStat_OptimalLocal,"OptimalLocal") \
DEFSTR(ModelStat_Unbounded,"Unbounded") \
DEFSTR(ModelStat_InfeasibleGlobal,"InfeasibleGlobal") \
DEFSTR(ModelStat_InfeasibleLocal,"InfeasibleLocal") \
DEFSTR(ModelStat_InfeasibleIntermed,"InfeasibleIntermed") \
DEFSTR(ModelStat_Feasible,"Feasible") \
DEFSTR(ModelStat_Integer,"Integer") \
DEFSTR(ModelStat_NonIntegerIntermed,"NonIntegerIntermed") \
DEFSTR(ModelStat_IntegerInfeasible,"IntegerInfeasible") \
DEFSTR(ModelStat_LicenseError,"LicenseError") \
DEFSTR(ModelStat_ErrorUnknown,"ErrorUnknown") \
DEFSTR(ModelStat_ErrorNoSolution,"ErrorNoSolution") \
DEFSTR(ModelStat_NoSolutionReturned,"NoSolutionReturned") \
DEFSTR(ModelStat_SolvedUnique,"SolvedUnique") \
DEFSTR(ModelStat_Solved,"Solved") \
DEFSTR(ModelStat_SolvedSingular,"SolvedSingular") \
DEFSTR(ModelStat_UnboundedNoSolution,"UnboundedNoSolution") \
DEFSTR(ModelStat_InfeasibleNoSolution,"InfeasibleNoSolution") \

#define DEFSTR(id, str) char id[sizeof(str)];

typedef struct {
   union {
      struct {
      DEFINE_STR()
   };
   char dummystr[1];
   };
} ModelStatNames;
#undef DEFSTR

#define DEFSTR(id, str) .id = str,

static const ModelStatNames modelstatnames = {
DEFINE_STR()
};
#undef DEFSTR

#define DEFSTR(id, str) offsetof(ModelStatNames, id),
static const unsigned modelstatnames_offsets[] = {
DEFINE_STR()
};

#undef DEFSTR
#undef DEFINE_STR

static const unsigned modelstatlen = sizeof(modelstatnames_offsets)/sizeof(modelstatnames_offsets[0]);


const char* mdl_modelstattxt(const Model *mdl, int modelstat)
{
   if (modelstat >= modelstatlen) return "ERROR unknown model stat";

   return modelstatnames.dummystr + modelstatnames_offsets[modelstat];
}

#define DEFINE_STR() \
DEFSTR(SolveStat_NA,"Not available")  \
DEFSTR(SolveStat_Normal,"Normal")  \
DEFSTR(SolveStat_Iteration,"Iteration")  \
DEFSTR(SolveStat_Resource,"Resource")  \
DEFSTR(SolveStat_Solver,"Solver")  \
DEFSTR(SolveStat_EvalError,"EvalError")  \
DEFSTR(SolveStat_Capability,"Capability")  \
DEFSTR(SolveStat_License,"License")  \
DEFSTR(SolveStat_User,"User")  \
DEFSTR(SolveStat_SetupErr,"SetupErr")  \
DEFSTR(SolveStat_SolverErr,"SolverErr")  \
DEFSTR(SolveStat_InternalErr,"InternalErr")  \
DEFSTR(SolveStat_Skipped,"Skipped")  \
DEFSTR(SolveStat_SystemErr,"SystemErr")

#define DEFSTR(id, str) char id[sizeof(str)];

typedef struct {
   union {
      struct {
      DEFINE_STR()
   };
   char dummystr[1];
   };
} SolveStatNames;
#undef DEFSTR

#define DEFSTR(id, str) .id = str,

static const SolveStatNames solvestatnames = {
DEFINE_STR()
};
#undef DEFSTR

#define DEFSTR(id, str) offsetof(SolveStatNames, id),
static const unsigned solvestatnames_offsets[] = {
DEFINE_STR()
};

static const unsigned solvestatlen = sizeof(solvestatnames_offsets)/sizeof(solvestatnames_offsets[0]);


const char* mdl_solvestattxt(const Model *mdl, int solvestat)
{
   if (solvestat >= solvestatlen) return "ERROR unknown solve stat";

   return solvestatnames.dummystr + solvestatnames_offsets[solvestat];
}

static void mdl_free(Model *mdl)
{
   if (!mdl) return;

   empinfo_dealloc(&mdl->empinfo);
   FREE(mdl->commondata.name);

   if (mdl->commondata.own_exports_dir_parent) {
      if (mdl->commondata.delete_exports_dir) {
         rmfn(mdl->commondata.exports_dir_parent);
      }

      FREE(mdl->commondata.exports_dir_parent);
   }
   mdl_timings_rel(mdl->timings);

   mdl->ops->deallocdata(mdl);
   ctr_dealloc(&mdl->ctr);

   if (mdl->mdl_up) {
      mdl_release(mdl->mdl_up);
      mdl->mdl_up = NULL;
   }

   free(mdl);
}

static void mdl_commondata_init(Model *mdl)
{
   MdlCommonData *dat = &mdl->commondata;
   dat->name = NULL;
   dat->exports_dir = NULL;
   dat->exports_dir_parent = NULL;
   dat->own_exports_dir_parent = false;
   dat->exports_dir_parent = false;
   dat->mdltype = MdlType_none;
}

Model* mdl_new(BackendType backend)
{
   static tlsvar unsigned g_mdl_id = 0;
   Model *mdl;
   CALLOC_NULL(mdl, Model, 1);

   mdl->id = g_mdl_id++;
   mdl->backend = backend;
   mdl->refcnt = 1;

   mdl_commondata_init(mdl);

   switch (backend) {
   case RHP_BACKEND_GAMS_GMO:
      mdl->ops = &mdl_ops_gams;
      break;
   case RHP_BACKEND_JULIA:
   case RHP_BACKEND_RHP:
      mdl->ops = &mdl_ops_rhp;
      break;
   default:
      error("%s :: unsupported backend '%s'", __func__, backend_name(backend));
      goto _exit;
   }

   SN_CHECK_EXIT(mdl->ops->allocdata(mdl));

   SN_CHECK_EXIT(ctr_alloc(&mdl->ctr, backend));

   SN_CHECK_EXIT(empinfo_alloc(&mdl->empinfo, mdl));

   SN_CHECK_EXIT(mdl_timings_alloc(mdl));

   return mdl;
_exit:
   mdl_free(mdl);
   return NULL;
}

Model *mdl_borrow(Model *mdl)
{
   /* To be changed to use mutex. */
   if (!mdl) {
      error("%s ERROR: mdl is NULL\n", __func__);
      return NULL;
   }

   if (mdl->refcnt == UINT_MAX) {
      error("[model] ERROR: %s model '%.*s' #%u has reached max refcnt value\n",
            mdl_fmtargs(mdl));
      return NULL;
   }

   mdl->refcnt++;
   printout(PO_TRACE_REFCNT, "[refcnt] %s model '%.*s' #%u: %1u -> %1u\n",
            mdl_fmtargs(mdl), mdl->refcnt-1, mdl->refcnt);

   return mdl;
}

void mdl_release(Model *mdl)
{
   if (mdl && (mdl)->refcnt > 0) {
      (mdl)->refcnt--;

      printout(PO_TRACE_REFCNT, "[refcnt] %s model %.*s #%u: %1u -> %1u",
               mdl_fmtargs(mdl), mdl->refcnt+1, mdl->refcnt);

      if (mdl->refcnt == 0) {
         printstr(PO_TRACE_REFCNT, ", FREEING.\n");
         mdl_free(mdl);
      } else {
         printstr(PO_TRACE_REFCNT, "\n");
      }

   } else if (mdl) {
      error("[ERROR] %s model '%*.s' #%u: refcnt is 0.\n", mdl_fmtargs(mdl));
  }
}

unsigned mdl_getid(const Model *mdl)
{
  return !mdl ? UINT_MAX : mdl->id;
}

int mdl_setname(Model *mdl, const char *name)
{
   FREE(mdl->commondata.name);
   A_CHECK(mdl->commondata.name, strdup(name));
   return OK;
}

static const char default_mdlname[] = "noname";

const char* mdl_getname(const Model *mdl)
{
  return !mdl ? "ERROR NULL mdl" : (mdl->commondata.name ? mdl->commondata.name : (mdl->mdl_up ? mdl_getname(mdl->mdl_up) : default_mdlname));
}

int mdl_getnamelen(const Model *mdl)
{
  return mdl->commondata.name ? (int)strlen(mdl->commondata.name) : (mdl->mdl_up ? mdl_getnamelen(mdl->mdl_up) : (int)strlen(default_mdlname));
}

rhp_idx mdl_getcurrentvi(const Model * const mdl, rhp_idx vi)
{
   if (mdl->mdl_up) {
      return ctr_getcurrent_vi(&mdl->mdl_up->ctr, vi);
   }

   return vi;
}

rhp_idx mdl_getcurrentei(const Model * const mdl, rhp_idx ei)
{
   if (mdl->mdl_up) {
      return ctr_getcurrent_ei(&mdl->mdl_up->ctr, ei);
   }

   return ei;
}

MathPrgm* mdl_getmpforvar(const Model *mdl, rhp_idx vi)
{
   SN_CHECK(vi_inbounds(vi, ctr_nvars_total(&mdl->ctr), __func__));

   if (!mdl->ctr.varmeta) return NULL;

   const EmpDag *empdag = &mdl->empinfo.empdag;

   unsigned mp_id = mdl->ctr.varmeta[vi].mp_id;
   if (!mpid_regularmp(mp_id)) return NULL;

   return empdag_getmpfast(empdag, mp_id);
}

MathPrgm* mdl_getmpforequ(const Model *mdl, rhp_idx ei)
{
   SN_CHECK(ei_inbounds(ei, ctr_nequs_total(&mdl->ctr), __func__));

   if (!mdl->ctr.equmeta) return NULL;

   const EmpDag *empdag = &mdl->empinfo.empdag;

   unsigned mp_id = mdl->ctr.equmeta[ei].mp_id;
   if (!mpid_regularmp(mp_id)) return NULL;

   return empdag_getmpfast(empdag, mp_id);
}

const char *mdl_getprobtypetxt(enum mdl_type probtype)
{
   switch (probtype) {
   case MdlType_none:
      return "none";
   case MdlType_lp:
      return "lp";
   case MdlType_nlp:
      return "nlp";
   case MdlType_dnlp:
      return "dnlp";
   case MdlType_mip:
      return "mip";
   case MdlType_minlp:
      return "minlp";
   case MdlType_qcp:
      return "qcp";
   case MdlType_mcp:
      return "mcp";
   case MdlType_emp:
      return "emp";
   default:
      error("%s :: unknown problem type %d\n", __func__, probtype);
      return "";
   }
}

int mdl_getobjequs(const Model *mdl, Aequ *objs)
{
  S_CHECK(chk_mdl(mdl, __func__));
  S_CHECK(chk_aequ_nonnull(objs, __func__));

  const Container *ctr = &mdl->ctr;
  if (ctr->equmeta) {
    rhp_idx size = 0;
    rhp_idx max = 10;
    rhp_idx *list;
    MALLOC_(list, rhp_idx, max);

    for (rhp_idx i = 0, len = ctr_nequs_total(ctr); i < len; ++i) {
      if (ctr->equmeta[i].role == EquObjective &&
        !(ctr->equmeta[i].ppty & EquPptyIsDeleted)) {

        if (size >= max) {
          max *=2;
          REALLOC_(list, rhp_idx, max);
        }

        list[size++] = i;
      }
    }

    aequ_setandownlist(objs, size, list);
  } else {
    rhp_idx ei;
    S_CHECK(mdl_getobjequ(mdl, &ei));

      if (valid_ei(ei)) {
         aequ_setcompact(objs, 1, ei);
      } else {
         aequ_reset(objs);
      }
  }

  return OK;
}

const char *mdl_getsolvestatastxt(const Model *mdl)
{
   int solvestat;
   SN_CHECK(mdl_getsolvestat(mdl, &solvestat));

   return mdl_solvestattxt(mdl, solvestat);

}

const char *mdl_getmodelstatastxt(const Model *mdl)
{
   int modelstat;
   SN_CHECK(mdl_getmodelstat(mdl, &modelstat));

   return mdl_modelstattxt(mdl, modelstat);

}

int mdl_setdualvars(Model *mdl, Avar *v, Aequ *e)
{
   /* TODO(xhub) when part of the general interface, this must be changed */
   assert(mdl->ctr.equmeta && mdl->ctr.varmeta);

   unsigned size = v->size;
   if (size != e->size) {
      error("[model] ERROR while adding dualvar information: the variable has "
            "size %u and the equation has size %u. They must be equal\n", size,
            e->size);
   }

   rhp_idx objvar;
   S_CHECK(mdl_getobjvar(mdl, &objvar));

   unsigned total_n = ctr_nvars_total(&mdl->ctr);
   unsigned total_m = ctr_nequs_total(&mdl->ctr);

   EquMeta *emeta = mdl->ctr.equmeta;
   VarMeta *vmeta = mdl->ctr.varmeta;
   for (unsigned i = 0; i < size; ++i) {

      rhp_idx vi = avar_fget(v, i);
      rhp_idx ei = aequ_fget(e, i);

      S_CHECK(vi_inbounds(vi, total_n, __func__));
      S_CHECK(ei_inbounds(ei, total_m, __func__));

      if (vi == objvar) {
         error("[model] ERROR: objective variable '%s' cannot be a dualvar",
               mdl_printvarname(mdl, vi));
         return Error_RuntimeError;
      }

      if (vmeta[vi].type != VarPrimal) {
//         error("[model] ERROR while setting variable '%s' as dualvar: it's type is %s, expecting %s",
//               );
      }
      vmeta[vi].type = VarDual;
      vmeta[vi].ppty = VarIsDualVar;
      vmeta[vi].dual = ei;

      emeta[ei].ppty |= EquPptyHasDualVar;
      emeta[ei].dual = vi;
   }
   
   return OK;
}

int mdl_ensure_exportdir(Model *mdl)
{
   if (mdl->commondata.exports_dir) { return OK; }

   if (!mdl->commondata.exports_dir_parent) {
#if defined(_POSIX_C_SOURCE) && _POSIX_C_SOURCE >= 200809L
   char *exports_dir_template;
   A_CHECK(exports_dir_template, strdup("/tmp/reshop_exports_XXXXXX"));
   char *res = mkdtemp(exports_dir_template);

   if (!res) {
      perror("mkdtemp");
      return Error_SystemError;
   }

   mdl->commondata.exports_dir_parent = exports_dir_template;
   mdl->commondata.own_exports_dir_parent = true;

#elif defined(_WIN32)

   A_CHECK(mdl->commondata.exports_dir_parent, win_gettmpdir());
   mdl->commondata.own_exports_dir_parent = true;
   mdl->commondata.delete_exports_dir = true;

#else

      printout(PO_INFO, "[model] %s model '%.*s' #%u has no valid export directory."
               "Set RHP_EXPORT_DIR to provide one.\n", mdl_fmtargs(mdl));
      return Error_NotFound;
#endif
   }

   IO_CALL(asprintf(&mdl->commondata.exports_dir, "%s" DIRSEP "%u-%s",
                    mdl->commondata.exports_dir_parent, mdl->id,
                    mdl->commondata.name));

   if (mkdir(mdl->commondata.exports_dir, S_IRWXU)) {
      perror("mkdir");
      error("%s ERROR: Could not create directory '%s'\n", __func__,
            mdl->commondata.exports_dir);
      return Error_SystemError;
   }

   return OK;
}

McpInfo* mdl_getmcpinfo(Model *mdl)
{
   if (mdl->commondata.mdltype != MdlType_mcp) {
      return NULL;
   }

   return &mdl->info.mcp;
}


int mdl_solreport(Model *mdl_dst, Model *mdl_src)
{

   int mstat, sstat;

   trace_solreport("[solreport] %s model '%.*s' #%u: reporting values from %s "
                   "model '%.*s' #%u\n", mdl_fmtargs(mdl_dst), mdl_fmtargs(mdl_src));

   S_CHECK(mdl_reportvalues(mdl_dst, mdl_src));
   S_CHECK(ctr_evalequvar(&mdl_dst->ctr));

   /* ------------------------------------------------------------------
       * Get the solve and model status
       * ------------------------------------------------------------------ */

   S_CHECK(mdl_getmodelstat(mdl_src, &mstat));
   S_CHECK(mdl_setmodelstat(mdl_dst, mstat));
   S_CHECK(mdl_getsolvestat(mdl_src, &sstat));
   S_CHECK(mdl_setsolvestat(mdl_dst, sstat));

   if (mdl_is_rhp(mdl_dst)) {
      /* deleted equations and equations in func2eval */
      S_CHECK(rctr_evalfuncs(&mdl_dst->ctr));
      /* if required, set the objective function value to the objvar one */
      S_CHECK(rmdl_fix_objequ_value(mdl_dst));
   }

   return OK;
}

/**
 * @brief Set the model type from scratch
 *
 * In the case of a trivial EMPDAG, this changes the problem type from EMP
 * to the appropriate type.
 *
 * @param mdl   the model
 *
 * @return      the error code
 */
int mdl_recompute_modeltype(Model *mdl)
{
   ModelType mdltype;
   S_CHECK(mdl_gettype(mdl, &mdltype));
   S_CHECK(mdl_settype(mdl, MdlType_none));

   S_CHECK(mdl_analyze_modeltype(mdl));

   ModelType mdltype_new;
   S_CHECK(mdl_gettype(mdl, &mdltype_new));

   if (mdltype == MdlType_emp && mdltype_new != MdlType_emp) {

      EmpDag *empdag = &mdl->empinfo.empdag;

      if (empdag_singleprob(empdag)) {

         daguid_t uid_root = empdag->uid_root;

         assert(uidisMP(uid_root));

         mpid_t mpid_root = uid2id(uid_root);

         DagMpArray *mps = &mdl->empinfo.empdag.mps;
         if (mpid_root >= mps->len) {
            error("[empdag] ERROR: MP root has index #%u, but the number of MP "
                  "is %u\n", mpid_root, mps->len);
         }

         MathPrgm *mp = mps->arr[mpid_root];
         if (!mp) { return error_runtime(); }

         rhp_idx objvar = mp_getobjvar(mp);
         rhp_idx objequ = mp_getobjequ(mp);
         RhpSense sense = mp_getsense(mp);

         empdag->type = EmpDag_Empty;
         S_CHECK(mdl_setsense(mdl, sense));

         if (mdltype_isopt(mdltype_new)) {
            S_CHECK(mdl_setobjvar(mdl, objvar));

            if (mdl_is_rhp(mdl)) {
               S_CHECK(rmdl_setobjfun(mdl, objequ));
            }

         } else if (mdltype_isvi(mdltype_new)) {
            empdag->type = EmpDag_Empty;
            assert(!valid_vi(objvar) && !valid_ei(objequ));
         } else {
            return error_runtime();
         }


         trace_process("[model] %s model '%.*s' #%u has now type %s with "
                       "sense %s, objvar = %s, objequ = %s\n", mdl_fmtargs(mdl),
                       mdltype_name(mdltype_new), sense2str(sense),
                       mdl_printvarname(mdl, objvar), mdl_printequname(mdl, objequ));

         return OK;

      }

      if (empdag_isempty(empdag)) {
         return OK;
      }

      return error_runtime();
   }

   if (mdltype == mdltype_new ||  (mdltype_isopt(mdltype) && mdltype_isopt(mdltype_new))) {
      return OK;
   }

   TO_IMPLEMENT("unsupported reset modeltype");
}

/**
 * @brief Analyze the model to set the modeltype (LP, NLP, MIP, ...)
 *
 * @param mdl   the model to analyze
 *
 * @return      the error code
 */
int mdl_analyze_modeltype(Model *mdl)
{
   ModelType mdltype;
   S_CHECK(mdl_gettype(mdl, &mdltype));

   if (mdltype != MdlType_none) {
      if (empinfo_hasempdag(&mdl->empinfo) && empinfo_is_hop(&mdl->empinfo) && mdltype != MdlType_emp) {
        error("[model] ERROR: High-Order Problem data, but the model type is %s rather than %s.\n",
              mdltype_name(mdltype), mdltype_name(MdlType_emp));
        return Error_EMPIncorrectInput;
      }
      return OK;
   }

   Container *ctr = &mdl->ctr;
   S_CHECK(ctr_equvarcounts(ctr));

   const EmpInfo *empinfo = &mdl->empinfo;
   if (empinfo_hasempdag(empinfo)) {
      if (empinfo_is_hop(empinfo)) {
         S_CHECK(mdl_settype(mdl, MdlType_emp));
         return OK;
      }

      if (empinfo_is_vi(empinfo)) {
         S_CHECK(mdl_settype(mdl, MdlType_vi));
         return OK;
      }

      assert(empinfo_is_opt(empinfo));
   }

   S_CHECK(ctr_equvarcounts(ctr));

   bool has_quad = ctr->equvarstats.equs.exprtypes[EquExprQuadratic] > 0;
   bool has_nl = ctr->equvarstats.equs.exprtypes[EquExprNonLinear] > 0;
   unsigned *vartypes = ctr->equvarstats.vartypes;

   bool is_integral = vartypes[VAR_I] || vartypes[VAR_B] || vartypes[VAR_SI];

  /* ----------------------------------------------------------------------
   * TODO: we would fail on MCP/MPEC
   * ---------------------------------------------------------------------- */

   if (has_nl) {
      mdltype = is_integral ? MdlType_minlp : MdlType_nlp;
   } else if (has_quad) {
      mdltype = is_integral ? MdlType_miqcp : MdlType_qcp;
   } else {
      mdltype = is_integral ? MdlType_mip   : MdlType_lp;
   }

   S_CHECK(mdl_settype(mdl, mdltype));

   return OK;
}


