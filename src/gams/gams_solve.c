#include "fooc.h"
#include "gams_solve.h"
#include "mathprgm.h"
#include "mdl.h"
#include "mdl_rhp.h"
#include "rhp_model.h"
#include "rmdl_gams.h"
#include "var.h"

static int err_mdlbackend_gmo(Model *mdl)
{
   error("[model] ERROR: can't create a GMO for %s model '%.*s' #%u\n",
         mdl_fmtargs(mdl));
   return Error_NotImplemented;
}

static int gmdl_gmo2gmo(Model *mdl, Model *mdl_solver)
{
   assert(mdl->backend == RHP_BACKEND_GAMS_GMO && mdl_solver->backend == RHP_BACKEND_GAMS_GMO);
   TO_IMPLEMENT("not yet implemented");
}

static int mdl_directexportasgmo(Model *mdl, Model *mdl_solver)
{
   assert(mdl_solver->backend == RHP_BACKEND_GAMS_GMO);

   if (mdl->backend == RHP_BACKEND_GAMS_GMO) {
      return gmdl_gmo2gmo(mdl, mdl_solver);
   }

   if (!mdl_is_rhp(mdl)) { return err_mdlbackend_gmo(mdl); }

   return rmdl_exportasgmo(mdl, mdl_solver);
}

/**
 * @brief Solve a model as an MCP using GAMS as backend
 *
 * @param mdl         the original model to solve
 * @param mdl_solver  the model that will be solve
 *
 * @return     the error code
 */
static int mdl_exportasmcp_gmo(Model *mdl, Model *mdl_solver)
{
   assert(mdl_solver->backend == RHP_BACKEND_GAMS_GMO);

   /* TODO: this should not be necessary */
   Model *mdl_rhp_for_fooc;
   if (mdl->backend == RHP_BACKEND_GAMS_GMO) {
      A_CHECK(mdl_rhp_for_fooc, rhp_mdl_new(RHP_BACKEND_RHP));
      S_CHECK(mdl_setname(mdl_rhp_for_fooc, "RHP mdl for FOOC"));

      S_CHECK(rmdl_initfromfullmdl(mdl_rhp_for_fooc, mdl));
      S_CHECK(rmdl_analyze_modeltype(mdl_rhp_for_fooc, NULL));

   } else if (mdl_is_rhp(mdl)) {
      mdl_rhp_for_fooc = mdl;
   } else {
      return err_mdlbackend_gmo(mdl);
   }

   Model *mdl_mcp;
   A_CHECK(mdl_mcp, rhp_mdl_new(RHP_BACKEND_RHP));
   S_CHECK(mdl_setname(mdl_mcp, "MCP"));

   /* TODO: this should not be necessary */
   S_CHECK(rmdl_ensurefops_activedefault(mdl_rhp_for_fooc));
   S_CHECK(rmdl_exportmodel(mdl_rhp_for_fooc, mdl_mcp, NULL));
   S_CHECK(rmdl_analyze_modeltype(mdl_mcp, NULL));

   /* TODO: this is part of GITLAB #67 */
//   S_CHECK(empinfo_initfromupstream(mdl_tmp_fooc));

//   S_CHECK(rmdl_prepare_export(mdl_tmp_rhp, mdl_tmp_fooc));
   // S_CHECK(rmdl_initfromfullmdl(mdl_tmp_fooc, mdl_tmp_rhp));

   McpStats mcpdata;
   S_CHECK(fooc_create_mcp(mdl_mcp, &mcpdata));

   S_CHECK(rmdl_export_latex(mdl_mcp, "mcp"));
   /* TODO GITLAB #71 */
   S_CHECK(rmdl_ensurefops_activedefault(mdl_mcp));
   S_CHECK(rmdl_exportmodel(mdl_mcp, mdl_solver, NULL));

   mdl_release(mdl_mcp);
   mdl_release(mdl_rhp_for_fooc);

   return OK;
}

/**
 * @brief Solve a model as an MCP using GAMS as backend
 *
 * @param mdl         the original model to solve
 * @param mdl_solver  the model that will be solve
 *
 * @return     the error code
 */
static int mdl_exportasmpec_gmo(Model *mdl, Model *mdl_solver)
{
   assert(mdl_solver->backend == RHP_BACKEND_GAMS_GMO);

   EmpDag *empdag = &mdl->empinfo.empdag;
   assert(empdag->roots.len == 1);
   daguid_t root_uid = empdag->roots.list[0];

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

   daguid_t lower_uid = empdag->mps.Carcs[upper_id].list[0];

   /* TODO: this should not be necessary */
   Model *mdl_rhp_for_fooc;
   if (mdl->backend == RHP_BACKEND_GAMS_GMO) {
      A_CHECK(mdl_rhp_for_fooc, rhp_mdl_new(RHP_BACKEND_RHP));
      S_CHECK(mdl_setname(mdl_rhp_for_fooc, "RHP mdl for FOOC"));

      S_CHECK(rmdl_initfromfullmdl(mdl_rhp_for_fooc, mdl));
      S_CHECK(rmdl_analyze_modeltype(mdl_rhp_for_fooc, NULL));

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
   * We prepare the Fops for the lower level part
   * ---------------------------------------------------------------------- */

   
   
//   S_CHECK(fops_subdag(fops_lower, mdl_rhp_for_fooc, lower_uid));

   /* TODO: this should not be necessary */
   S_CHECK(rmdl_ensurefops_activedefault(mdl_rhp_for_fooc));
   S_CHECK(rmdl_exportmodel(mdl_rhp_for_fooc, mdl_mpec, NULL));

   /* TODO: this is part of GITLAB #67 */
//   S_CHECK(empinfo_initfromupstream(mdl_tmp_fooc));

//   S_CHECK(rmdl_prepare_export(mdl_tmp_rhp, mdl_tmp_fooc));
   // S_CHECK(rmdl_initfromfullmdl(mdl_tmp_fooc, mdl_tmp_rhp));

   McpStats mcpdata;
   McpDef mcpdef = {.uid = lower_uid}; //, .fops_vars = fops_perm };
   S_CHECK(fooc_mcp(mdl_mpec, &mcpdef, &mcpdata));
   
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
      aequ_setlist(&e_upper, mpequs.len, mpequs.list);
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

   S_CHECK(mdl_setprobtype(mdl_mpec, MdlProbType_mpec));
   S_CHECK(mdl_setsense(mdl_mpec, sense));

   if (valid_ei(objequ_upper)) {
      rhp_idx objequ_mpec = mdl_rhp_for_fooc->ctr.rosetta_equs[objequ_upper];
      assert(valid_ei(objequ_mpec));
      S_CHECK(rmdl_setobjequ(mdl_mpec, objequ_mpec));
   }

   if (valid_vi(objvar_upper)) {
      rhp_idx objvar_mpec = mdl_rhp_for_fooc->ctr.rosetta_vars[objvar_upper];
      assert(valid_vi(objvar_mpec));
      S_CHECK(rmdl_setobjvar(mdl_mpec, objvar_mpec));

   }

   S_CHECK(rmdl_export_latex(mdl_mpec, "mpec4gams"));
   /* TODO GITLAB #71 */
   S_CHECK(rmdl_ensurefops_activedefault(mdl_mpec));
   S_CHECK(rmdl_exportmodel(mdl_mpec, mdl_solver, NULL));

   return OK;
}

static int mdl_emp_transform_exportasgmo(Model *mdl, Model *mdl_solver)
{
   EmpDag *empdag = &mdl->empinfo.empdag;
   EmpDagType empdag_type = empdag->type;
   assert(empdag_type != EmpDag_Unset);

   switch (empdag_type) {
   case EmpDag_Empty:
      if (mdl->backend == RHP_BACKEND_GAMS_GMO) {
         return gmdl_gmo2gmo(mdl, mdl_solver);
      }

      /* We don't know what to do here */
      error("[GAMS] ERROR in exporting %s model '%.*s' #%u of type EMP to GMO: "
            "empdag type is %s\n", mdl_fmtargs(mdl), empdag_typename(empdag_type));
      return Error_EMPRuntimeError;
   case EmpDag_Single_Opt:
   case EmpDag_Opt:
   {
      S_CHECK(rmdl_reset_modeltype(mdl, NULL));
      return mdl_directexportasgmo(mdl, mdl_solver);
   }
   case EmpDag_Single_Vi:
   case EmpDag_Vi:
   case EmpDag_Mopec:
      return mdl_exportasmcp_gmo(mdl, mdl_solver);
   case EmpDag_Bilevel:
   case EmpDag_Mpec:
      return mdl_exportasmpec_gmo(mdl, mdl_solver);
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

int mdl_exportasgmo(Model *mdl, Model *mdl_solver)
{
   ProbType probtype;
   S_CHECK(mdl_getprobtype(mdl, &probtype));

   switch (probtype) {
   case MdlProbType_lp:         /**< LP    Linear Programm   */
   case MdlProbType_nlp:        /**< NLP   NonLinear Programm     */
   case MdlProbType_qcp:        /**< QCP   Quadratically Constraint Programm */
   case MdlProbType_mip:        /**< MIP   Mixed-Integer Programm */
   case MdlProbType_minlp:      /**< MINLP Mixed-Integer NLP*/
   case MdlProbType_miqcp:      /**< MIQCP Mixed-Integer QCP*/
   case MdlProbType_mcp:
   case MdlProbType_mpec:
     return mdl_directexportasgmo(mdl, mdl_solver);
   case MdlProbType_vi:
      return mdl_exportasmcp_gmo(mdl, mdl_solver);
   case MdlProbType_emp:        /**< EMP   Extended Mathematical Programm */
      return mdl_emp_transform_exportasgmo(mdl, mdl_solver);
   case MdlProbType_cns:        /**< CNS   Constrained Nonlinear System */
      error("%s :: CNS is not yet supported\n", __func__);
      return Error_NotImplemented;
   case MdlProbType_dnlp:       /**< DNLP  Nondifferentiable NLP  */
      error("%s :: nonsmooth NLP are not yet supported\n", __func__);
      return Error_NotImplemented;
   default:
      error("%s :: no solve procedure for a model of type %s\n", __func__,
            probtype_name(probtype));
      return Error_NotImplemented;
   }
}
