#include "asprintf.h"

#include <assert.h>


#include "filter_ops.h"
#include "fooc.h"
#include "fooc_priv.h"
#include "macros.h"
#include "mathprgm.h"
#include "mdl.h"
#include "mdl_priv.h"
#include "mdl_rhp.h"
#include "mdl_transform.h"
#include "reshop.h"
#include "rhp_fwd.h"
#include "var.h"

/**
 * @brief Analyse the EMP structure to see if we can compute the first order
 * optimality conditions
 *
 * @param mdl  the model
 *
 * @return         the error code
 */
static NONNULL int mdl_analyze_emp_for_fooc(Model *mdl) 
{
   int status = OK;

   const EmpInfo *empinfo_src = &mdl->mdl_up->empinfo;
   const EmpDag *empdag = &empinfo_src->empdag;

   assert(empinfo_hasempdag(empinfo_src));

   const DagUidArray *roots = &empdag->roots;
   unsigned roots_len = roots->len;

   if (roots_len > 1) {
      TO_IMPLEMENT("EMPDAG with multiple roots: need to implement DAG filtering");
   }

  /* ----------------------------------------------------------------------
   * FOOC requires that no VF path are present
   * ---------------------------------------------------------------------- */

   if (empdag->features.hasVFpath) {
      Model *mdl_up = mdl->mdl_up;
      Model *mdl4fooc = rhp_mdl_new(RHP_BACKEND_RHP);
      char *mdlname;
      IO_CALL(asprintf(&mdlname, "Contracted version of model '%s'", mdl_getname(mdl_up)));

      TO_IMPLEMENT("rmdl_contract_along_Vpaths() needs to be finished");
//      S_CHECK(rmdl_contract_along_Vpaths(mdl4fooc, mdl_up));

   }

   daguid_t root = empdag->uid_root;

   if (!valid_uid(root)) {
      error("[fooc] ERROR in %s model '%.*s' #%u, no valid EMPDAG root\n", mdl_fmtargs(mdl));
      return Error_EMPRuntimeError;
   }

   MathPrgm *mp = NULL;
   Mpe *mpe = NULL;
   dagid_t id = uid2id(root);

   if (uidisMP(root)) {
      S_CHECK(empdag_getmpbyid(empdag, id, &mp));
   } else {
      S_CHECK(empdag_getmpebyid(empdag, id, &mpe));
   }

   if (mpe) {
      UIntArray mps;
      S_CHECK(empdag_mpe_getchildren(empdag, mpe->id, &mps));
      unsigned nb_mp = mps.len;
      if (nb_mp == 0) {
         error("%s ERROR: empty equilibrium '%s'\n", __func__,
               empdag_getmpename(empdag, mpe->id));
         return Error_EMPRuntimeError;
      }

      for (unsigned i = 0; i < nb_mp; ++i) {
         mpid_t mpid = uid2id(mps.arr[i]);
         if (!childless_mp(empdag, mpid)) { status = Error_OperationNotAllowed; }
      }

   } else {

      if (!mp) {
         error("[fooc] ERROR in %s model '%.*s' #%u: Empdag root is neither an "
               "MP or an MPE\n", mdl_fmtargs(mdl));
         return Error_RuntimeError;
      }

      if (!childless_mp(empdag, mp->id)) { status = Error_OperationNotAllowed; }
   }

   return status;
}

static int mdl_prepare_fooc(Model *mdl, Model *mdl_mcp)
{
   BackendType backend_src = mdl->backend;
   switch (backend_src) {
   case RHP_BACKEND_RHP:
      S_CHECK(rmdl_prepare_export(mdl, mdl_mcp));
      break;
   case RHP_BACKEND_GAMS_GMO:
      TO_IMPLEMENT("Fooc with GAMS/GMO");
   default:
      return backend_throw_notimplemented_error(backend_src, __func__);
   }

   return OK;
}


static int mdl_create_fooc(Model *mdl, Model *mdl_mcp)
{
   ModelType mdltype;
   S_CHECK(mdl_gettype(mdl, &mdltype));

  if (mdltype == MdlType_mcp) {
    error("[fooc] ERROR in %s model '%.*s' #%u: the problem type is MCP, which "
          "already represents optimality conditions\n", mdl_fmtargs(mdl));
    return Error_UnExpectedData;
  }

  switch (mdltype) {
  case MdlType_lp:
  case MdlType_qcp:
  case MdlType_nlp:
  case MdlType_emp:
    break;

  case MdlType_dnlp:
    error("%s :: ERROR: nonsmooth NLP are not supported\n", __func__);
    return Error_NotImplemented;

  case MdlType_cns:
    error("%s :: ERROR: constraints systems are not supported\n", __func__);
    return Error_NotImplemented;

  case MdlType_mip:
  case MdlType_minlp:
  case MdlType_miqcp:
    error("%s :: ERROR: Model with integer variables are not yet supported\n", __func__);
    return Error_NotImplemented;

  default:
    error("%s :: ERROR: unknown/unsupported container type %s\n", __func__,
          mdltype_name(mdltype));
    return Error_InvalidValue;
  }

   mdl_linkmodels(mdl, mdl_mcp);
   
   S_CHECK(mdl_prepare_fooc(mdl, mdl_mcp));

   return fooc_create_mcp(mdl_mcp);
}

/**
 * @brief Formulate the MCP version of a given model
 *
 * @param mdl         the source model
 * @param mdl_target  the model that will be solve
 *
 * @return     the error code
 */
int mdl_transform_tomcp(Model *mdl, Model **mdl_target)
{
   /* TODO: this should not be necessary */
   Model *mdl_rhp_for_fooc;
   if (mdl->backend == RHP_BACKEND_GAMS_GMO) {
      A_CHECK(mdl_rhp_for_fooc, rhp_mdl_new(RHP_BACKEND_RHP));
      S_CHECK(mdl_setname(mdl_rhp_for_fooc, "RHP mdl for FOOC"));

      S_CHECK(rmdl_initfromfullmdl(mdl_rhp_for_fooc, mdl));

   } else if (mdl_is_rhp(mdl)) {
      mdl_rhp_for_fooc = mdl;
   } else {
      return backend_throw_notimplemented_error(mdl->backend, __func__);
   }

   Model *mdl_mcp;
   A_CHECK(mdl_mcp, rhp_mdl_new(RHP_BACKEND_RHP));
   S_CHECK(mdl_setname(mdl_mcp, "MCP"));

   S_CHECK(mdl_create_fooc(mdl_rhp_for_fooc, mdl_mcp))

   S_CHECK(rmdl_export_latex(mdl_mcp, "mcp"));

   *mdl_target = mdl_mcp;

   mdl_release(mdl_rhp_for_fooc);

   return OK;
}

/**
 * @brief Transform a bilevel/MPEC into a MPMCC model 
 *
 * @param mdl         the original model to solve
 * @param mdl_target  the model that will be solve
 *
 * @return     the error code
 */
static int mdl_transform_tompmcc(Model *mdl, Model **mdl_target)
{
   /* TODO: implement working on a subset with subdag fops */
   EmpDag *empdag = &mdl->empinfo.empdag;
   assert(empdag->roots.len == 1);
   daguid_t root_uid = empdag->roots.arr[0];

   if (!uidisMP(root_uid)) {
      errormsg("[bilevel2MPEC] ERROR: root is required to be an MP");
      return Error_EMPRuntimeError;
   }

   mpid_t upper_id = uid2id(root_uid);
   assert(upper_id < empdag->mps.len);

   MathPrgm *mp_upper = empdag->mps.arr[upper_id];
   RhpSense sense = mp_getsense(mp_upper);

   if (empdag->mps.Carcs[upper_id].len != 1) {
      error("[bilevel2MPEC] ERROR: expecting 1 child, got %u\n",
            empdag->mps.Carcs[upper_id].len);
      return Error_EMPRuntimeError;
   }

   daguid_t lower_uid = empdag->mps.Carcs[upper_id].arr[0];

   /* TODO: this should not be necessary */
   Model *mdl_rhp_for_fooc;
   if (mdl->backend == RHP_BACKEND_GAMS_GMO) {
      A_CHECK(mdl_rhp_for_fooc, rhp_mdl_new(RHP_BACKEND_RHP));
      S_CHECK(mdl_setname(mdl_rhp_for_fooc, "RHP mdl for FOOC"));

      S_CHECK(rmdl_initfromfullmdl(mdl_rhp_for_fooc, mdl));
      S_CHECK(mdl_analyze_modeltype(mdl_rhp_for_fooc, NULL));

   } else if (mdl_is_rhp(mdl)) {
      mdl_rhp_for_fooc = mdl;
   } else {
      error("[model] ERROR: can't create an MPEC GMO for %s model '%.*s' #%u\n",
            mdl_fmtargs(mdl));
      return Error_NotImplemented;
   }

   Model *mdl_mpec;
   A_CHECK(mdl_mpec, rhp_mdl_new(RHP_BACKEND_RHP));
   S_CHECK(mdl_setname(mdl_mpec, "MPEC"));

  /* ----------------------------------------------------------------------
   * We prepare the model, including fops for the lower level part
   * ---------------------------------------------------------------------- */
   
   mdl_linkmodels(mdl_rhp_for_fooc, mdl_mpec);
   S_CHECK(mdl_prepare_fooc(mdl_rhp_for_fooc, mdl_mpec));

   Fops *fops_lower = fops_subdag_activevars_new(mdl_rhp_for_fooc, lower_uid);
   Fops *fops_old = mdl_rhp_for_fooc->ctr.fops;
   mdl_rhp_for_fooc->ctr.fops = fops_lower;

   S_CHECK(fooc_mcp(mdl_mpec));

   mdl_rhp_for_fooc->ctr.fops = fops_old;
   fops_lower->freedata(fops_lower->data);
   
  /* ----------------------------------------------------------------------
   * Now we add the upper level problem. We need to update mp_upper to the
   * one in the temporary fooc model to get the changes there
   * ---------------------------------------------------------------------- */

   mp_upper = mdl_rhp_for_fooc->empinfo.empdag.mps.arr[upper_id];
   IdxArray mpvars = mp_upper->vars;
   IdxArray mpequs = mp_upper->equs;
   Avar v_upper;
   //avar_setlist(&v_upper, mpvars.len, mpvars.list);
   avar_setcompact(&v_upper, 0, IdxNA);

   /* TODO: what happens if v_upper is not in the model? */
   if (mpequs.len > 0) {
      Aequ e_upper;
      aequ_setlist(&e_upper, mpequs.len, mpequs.arr);
      S_CHECK(rmdl_appendequs(mdl_mpec, &e_upper));
   }

   rhp_idx cur_vi = ctr_nvars(&mdl_mpec->ctr);
   rhp_idx cur_ei = ctr_nequs(&mdl_mpec->ctr);

   /* The objective variable of the original problem disappeared when we did the
    * model export to fooc. */
   rhp_idx objequ_upper = mp_getobjequ(mp_upper);
   rhp_idx objvar_upper = mp_getobjvar(mp_upper);
   bool valid_objequ_upper = valid_ei(objequ_upper);
   bool valid_objvar_upper = valid_ei(objvar_upper);

   if (!valid_objequ_upper && !valid_objvar_upper) {
      mp_err_noobjdata(mp_upper);
      return Error_EMPRuntimeError;
   }

   S_CHECK(mdl_settype(mdl_mpec, MdlType_mpec));
   S_CHECK(mdl_setsense(mdl_mpec, sense));

   if (valid_ei(objequ_upper)) {
      rhp_idx objequ_mpec = mdl_rhp_for_fooc->ctr.rosetta_equs[objequ_upper];
      assert(valid_ei(objequ_mpec));
      S_CHECK(rmdl_setobjfun(mdl_mpec, objequ_mpec));
   }

   if (valid_vi(objvar_upper)) {
      rhp_idx objvar_mpec = mdl_rhp_for_fooc->ctr.rosetta_vars[objvar_upper];
      assert(valid_vi(objvar_mpec));
      S_CHECK(rmdl_setobjvar(mdl_mpec, objvar_mpec));

   }

   S_CHECK(rmdl_export_latex(mdl_mpec, "mpec"));
   /* TODO GITLAB #71 */
   *mdl_target = mdl_mpec;

   return OK;
}

/**
 * @brief Transform an EMP model into a regular GAMS model type
 *
 * - If the model is a classical one, then we do nothing
 * - If we have a VI or a MOPEC, then we form the MCP
 * - If we have a bilevel/MPEC, we form a MPMCC
 *
 * @param mdl_src     The source model
 * @param mdl_target  The target model
 *
 * @return            The error code
 */
int mdl_transform_emp_togamsmdltype(Model *mdl_src, Model **mdl_target)
{
   EmpDag *empdag = &mdl_src->empinfo.empdag;
   EmpDagType empdag_type = empdag->type;
   assert(empdag_type != EmpDag_Unset);

   switch (empdag_type) {
   case EmpDag_Empty:
   case EmpDag_Single_Opt:
   case EmpDag_Opt:
      /* This is necessary to ensure that we can release mdl_target */
      *mdl_target = mdl_borrow(mdl_src);
      return OK;
   case EmpDag_Single_Vi:
   case EmpDag_Vi:
   case EmpDag_Mopec:
      S_CHECK(mdl_transform_tomcp(mdl_src, mdl_target));
      break;
   case EmpDag_Bilevel:
   case EmpDag_Mpec:
      S_CHECK(mdl_transform_tompmcc(mdl_src, mdl_target));
      break;
   case EmpDag_Multilevel:
   case EmpDag_MultilevelMopec:
   case EmpDag_Epec:
   case EmpDag_NestedCcf:
   case EmpDag_Complex:
   default:
      TO_IMPLEMENT("non-trivial EMPDAG not supported");
   }

   return OK;
}
