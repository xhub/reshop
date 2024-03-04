#include "cmat.h"
#include "container.h"
#include "ctr_rhp.h"
#include "empinfo.h"
#include "gams_solve.h"
#include "equvar_helpers.h"
#include "equvar_metadata.h"
#include "filter_ops.h"
#include "macros.h"
#include "mdl.h"
#include "mdl_ops.h"
#include "ctrdat_rhp.h"
#include "ctrdat_rhp_priv.h"
#include "mdl_rhp.h"
#include "printout.h"
#include "reshop_solvers.h"
#include "rhp_fwd.h"
#include "rmdl_gams.h"
#include "rmdl_data.h"
#include "rmdl_options.h"
#include "str2idx.h"

#ifndef NDEBUG
#define SOLREPORT_DEBUG(str, ...) trace_solreport("[solreport] " str "\n", __VA_ARGS__)
#else
#define SOLREPORT_DEBUG(...)
#endif

static int err_hop_mdl(const Model *mdl, const char *fn)
{
   error("ERROR: %s model '%.*s' #%u is not a simple optimization or VI model, "
         "which is required for calling the function %s\n", mdl_fmtargs(mdl), fn);
   return Error_WrongModelForFunction;
}

static int chk_empdag_simple(EmpDag *empdag, const char *fn)
{
   if (empdag->type == EmpDag_Unset) {
      return empdag_simple_init(empdag);
   }
   if (empdag->type != EmpDag_Empty) {
      return err_hop_mdl(empdag->mdl, fn);
   }
   return OK;
}

static int rmdl_allocdata(Model *mdl)
{
   int status = OK;
   RhpModelData *mdldata = NULL;
   MALLOC_EXIT(mdldata, RhpModelData, 1);
   mdldata->solver = RMDL_SOLVER_UNSET;
   mdldata->probtype = MdlProbType_none;

   A_CHECK_EXIT(mdldata->options, rmdl_set_options());

   mdl->data = mdldata;
   return OK;

_exit:
   FREE(mdldata);
   mdl->data = NULL;
   return status;
}

static void rmdl_deallocdata(Model *mdl)
{
   if (mdl->data) {
      RhpModelData *data = mdl->data;
      FREE(data->options);
      FREE(data);
      mdl->data = NULL;
   }
}

static int rmdl_checkmdl(Model *mdl)
{
   int status = OK;

   Container *ctr = &mdl->ctr;
   RhpContainerData *cdat = (RhpContainerData *)ctr->data;
   size_t total_n = cdat->total_n;
   Fops * restrict fops = ctr->fops;


   for (rhp_idx i = 0; i < total_n; ++i) {
      if (fops && !fops->keep_var(fops->data, (rhp_idx)i)) continue;

      Var * restrict v = &ctr->vars[i];

      /* -------------------------------------------------------------------
       * If the variable is fixed, then set the value to the right one
       * ------------------------------------------------------------------- */
      if (var_isfixed(v)) {
         v->value = v->bnd.lb;
         v->basis = BasisFixed;
      } else if (var_bndinvalid(v)) {
         error("%s :: wrong bound for var '%s' #%u: lb = %e; ub = %e.\n",
               __func__, ctr_printvarname(ctr, i), i, v->bnd.lb, v->bnd.ub);
         status = status == OK ? Error_InvalidValue : status;
      }
   }

   if (status != OK) { return status; }

   bool do_cmat_check = optvalb(mdl, Options_Expensive_Checks);
#ifndef NDEBUG
   do_cmat_check = true;
#endif
   if (do_cmat_check) {
      S_CHECK(cmat_chk_expensive(ctr));
   }

   EmpDag *empdag = &mdl->empinfo.empdag;
   S_CHECK(empdag_fini(empdag));

   ProbType probtype;
   S_CHECK(mdl_getprobtype(mdl, &probtype));


   /* TODO: GITLAB #69 */
   //if (empdag->type == EmpDag_Empty) { return status; }

   if (!probtype_isopt(probtype)) { return status; }

   /* ----------------------------------------------------------------------
    * For a simple OPT model, check that the objective equation and variable have
    * the proper type
    * ---------------------------------------------------------------------- */
   rhp_idx objvar = empdag->simple_data.objvar;
   rhp_idx objequ = empdag->simple_data.objequ;

   if (!valid_ei(objequ)) {
      if (!valid_vi(objvar)) {
         error("[model] ERROR: %s model '%.*s' #%u has no objective variable or "
               "equation\n", mdl_fmtargs(mdl));
      }

      return OK;
   }

   if (valid_vi(objvar)) {
      return rmdl_checkobjequvar(mdl, objvar, objequ);
   }

   unsigned type, cone;
   S_CHECK(ctr_getequtype(&mdl->ctr, objequ, &type, &cone));
   if (type != EQ_MAPPING) {
      error("[model/rhp] ERROR: %s model '%.*s' #%u has an objective equation "
            "with type %s, but it must be %s. ", mdl_fmtargs(mdl), equtype_name(type),
            equtype_name(EQ_MAPPING));
      errormsg("If there is an objective variable, it should be added to ");
      errormsg("the model as well!\n");
      return Error_EMPRuntimeError;
   }

   return OK;
}

static int rmdl_copysolveoptions(Model *mdl, const Model *mdl_src)
{
   BackendType backend = mdl->backend;
   switch (backend) {
   case RHP_BACKEND_GAMS_GMO:
     S_CHECK(rmdl_copysolveoptions_gams(mdl, mdl_src));
     break;
   case RHP_BACKEND_RHP:
   case RHP_BACKEND_JULIA:
   {
      union opt_t val;
      S_CHECK(mdl_getoption(mdl_src, "solver_option_file_number", &val.i));
      rmdl_setoption(mdl, "solver_option_file_number", val);

      mdl_getoption(mdl_src, "keep_files", &val.b);
      rmdl_setoption(mdl, "keep_files", val);

      mdl_getoption(mdl_src, "rtol", &val.d);
      rmdl_setoption(mdl, "rtol", val);
      mdl_getoption(mdl_src, "atol", &val.d);
      rmdl_setoption(mdl, "atol", val);
      break;
   }
   default:
      error("%s :: unsupported container '%s' (%d)", __func__, backend_name(backend),
            backend);
      return Error_InvalidValue;
   }


   return OK;
}

static int rmdl_copystatsfromsolver(Model *mdl, const Model *mdl_solver)
{
   return OK;
}

static int rmdl_getmodelstat(const Model *mdl, int *modelstat)
{
   const RhpModelData *model = (RhpModelData *) mdl->data;
   *modelstat = model->modelstat;

   return OK;
}

static int rmdl_getsolvername(const Model *mdl, const char ** solvername)
{
   const RhpModelData *model = (RhpModelData *) mdl->data;

   if (model->solver < __RMDL_SOLVER_SIZE) {
     *solvername = rmdl_solvernames[model->solver];
   } else {
     *solvername = "INVALID";
     return Error_InvalidValue;
   }

   return OK;
}

static int rmdl_getsolvestat(const Model *mdl, int *solvestat)
{
   const RhpModelData *model = (RhpModelData *) mdl->data;
   *solvestat = model->solvestat;

   return OK;
}

int rmdl_getobjequ(const Model *mdl, rhp_idx *objequ)
{
   const EmpDag *empdag = &mdl->empinfo.empdag;
   assert(empdag_isset(empdag));

   if (empdag->type != EmpDag_Empty) {
      *objequ = IdxNA;
      return OK;
   }

   *objequ = empdag->simple_data.objequ;

   return OK;
}

int rmdl_getsense(const Model *mdl, RhpSense *sense)
{
   const EmpDag *empdag = &mdl->empinfo.empdag;
   assert(empdag_isset(empdag));

   if (empdag->type != EmpDag_Empty) {
      *sense = RhpNoSense;
      return OK;
   }

   *sense = empdag->simple_data.sense;

   return OK;
}

int rmdl_getobjvar(const Model *mdl, rhp_idx *objvar)
{
   const EmpDag *empdag = &mdl->empinfo.empdag;
   assert(empdag_isset(empdag));

   if (empdag->type != EmpDag_Empty) {
      *objvar = IdxNA;
      return OK;
   }

   *objvar = empdag->simple_data.objvar;

   return OK;
}

static int rmdl_getobjjacval(const Model *mdl, double *objjacval)
{
   return Error_NotImplemented;
}

/* Copy variables attributes in a straightforward fashion */
static inline void  _copy_vars(Container *ctr_dst,
                               const Container *ctr_src,
                               unsigned total_n)
{
   Var * restrict vars_dst = ctr_dst->vars;
   const Var * restrict vars_src = ctr_src->vars;

   for (size_t i = 0; i < total_n; ++i) {
      Var * restrict v = &vars_dst[i];
      const Var * restrict v_src = &vars_src[i];
      v->value = v_src->value;
      v->multiplier = v_src->multiplier;
      v->basis = v_src->basis;
      SOLREPORT_DEBUG("VAR %-30s " SOLREPORT_FMT " from downstream var '%s'",
                      ctr_printvarname(ctr_dst, i), solreport_gms_v(v), 
                      ctr_printvarname2(ctr_src, i));
   }
}

/* Copy equations attributes in a straightforward fashion */
static inline void  _copy_equs(Equ * restrict equs_dst,
                               const Equ * restrict equs_src,
                               unsigned total_m)
{
   for (size_t i = 0; i < total_m; ++i) {
      Equ * restrict e = &equs_dst[i];
      const Equ * restrict e_src = &equs_src[i];
      e->value = e_src->value;
      e->multiplier = e_src->multiplier;
      e->basis = e_src->basis;
   }
}

/* Copy variables attributes with filtering */
static inline void  _copy_vars_fops(Container * ctr_dst,
                                    const Container * ctr_src,
                                    unsigned total_n, 
                                    const Fops * restrict fops)
{
   Var * restrict vars_dst = ctr_dst->vars;
   const Var * restrict vars_src = ctr_src->vars;
   const rhp_idx * restrict rosetta_vars = ctr_dst->rosetta_vars;
   void * restrict fops_data = fops->data;

   for (size_t i = 0, j = 0; i < total_n; ++i) {
      Var * restrict v = &vars_dst[i];
      if (!fops->keep_var(fops_data, i)) {
         /* we cannot check for v->is_deleted as this variable might just never
          * appear in the mode (happens with OVF) */
         assert(rosetta_vars && !valid_vi(rosetta_vars[i]));
         v->value = SNAN;
         v->multiplier = SNAN;
         v->basis = BasisUnset;
      } else {
         assert((rosetta_vars && valid_vi(rosetta_vars[i])) && !v->is_deleted);
         const Var * restrict v_src = &vars_src[j];
         v->value = v_src->value;
         v->multiplier = v_src->multiplier;
         v->basis = v_src->basis;
         SOLREPORT_DEBUG("VAR %-30s " SOLREPORT_FMT " from downstream var %s",
                         ctr_printvarname(ctr_dst, i), solreport_gms_v(v), 
                         ctr_printvarname2(ctr_src, j));
         j++;
      }
   }
}

static inline int  _copy_equs_fops(Container * restrict ctr_dst,
                                   const Container * restrict ctr_src,
                                   unsigned total_m, 
                                   const Fops * restrict fops)
{
   void * restrict fops_data = fops->data;
   Equ * restrict equs_dst = ctr_dst->equs;
   Equ * restrict equs_src = ctr_src->equs;
   const  rhp_idx * restrict rosetta_equs = ctr_dst->rosetta_equs;
   assert(rosetta_equs);
   rhp_idx ei_new;

   for (unsigned i = 0; i < total_m; ++i) {
      Equ * restrict e = &equs_dst[i];

      if (!fops->keep_equ(fops_data, i)) {
         assert(!valid_ei(rosetta_equs[i]));

         /* The equation might have been transformed */
         EquInfo equinfo;
         S_CHECK(rctr_get_equation(ctr_dst, i, &equinfo));

         ei_new = equinfo.ei;
         assert(valid_ei(ei_new));
         rhp_idx ei_src = rosetta_equs[ei_new];

         // TODO  if (!ppty[EQU_PPTY_EXPANDED] && valid_ei(ei_new)) {
         if (valid_ei(ei_src)) {
            const Equ * restrict e_src = &equs_src[ei_src];

            SOLREPORT_DEBUG("EQU %-30s " SOLREPORT_FMT " using transformed %s",
                            ctr_printequname(ctr_dst, i), solreport_gms(e_src), 
                            ctr_printequname2(ctr_src, ei_src));

            if (RHP_LIKELY(!equinfo.flipped)) {
               e->value = e_src->value;
               e->multiplier = e_src->multiplier;
               e->basis = e_src->basis;
            } else {
               e->value = -e_src->value;
               e->multiplier = -e_src->multiplier;
               BasisStatus basis = e_src->basis;

               if (basis == BasisLower) {
                  basis = BasisUpper;
               } else if (basis == BasisUpper) {
                  basis = BasisLower;
               }
               e->basis = basis;
            }

         } else {
            goto forgotten_equ;
         }

      } else if ((ei_new = rosetta_equs[i]), valid_ei(ei_new)) {
         /* If the equation was kept, then look up its new index */
         const Equ * restrict e_src = &equs_src[ei_new];

         SOLREPORT_DEBUG("EQU %-30s " SOLREPORT_FMT " using equ %s",
                            ctr_printequname(ctr_dst, i), solreport_gms(e_src), 
                            ctr_printequname2(ctr_src, ei_new));

         e->value = e_src->value;
         e->multiplier = e_src->multiplier;
         e->basis = e_src->basis;

      } else { /* This case is encountered for any objequ while using an MCP/VI solver */
forgotten_equ:
         /* TODO(xhub) determine whether forgotten eqn is right */

         /* If the equation is in func2eval, everything is fine */
         if ((O_Output & PO_TRACE_SOLREPORT) && (!ctr_dst->func2eval ||
            !valid_idx(aequ_findidx(ctr_dst->func2eval, i)))) {
            trace_solreport("[solreport] equ '%s' was forgotten\n",
                     ctr_printequname(ctr_dst, i));
         }
         e->value = SNAN;
         e->multiplier = SNAN;
         e->basis = BasisUnset;
      }
   }

   return OK;
}

static int rmdl_reportvalues_from_rhp(Container * restrict ctr_dst,
                                  const Container * restrict ctr_src)
{
   RhpContainerData *cdat = (RhpContainerData *) ctr_dst->data;
   const Fops *fops = ctr_dst->fops;

   /* ---------------------------------------------------------------------
    * We copy all active variables of ctr_src in the range [0,n) to ctr_dst
    * --------------------------------------------------------------------- */

   if (fops) {
      _copy_vars_fops(ctr_dst, ctr_src, cdat->total_n, fops);
      _copy_equs_fops(ctr_dst, ctr_src, cdat->total_m, fops);
   } else {
      assert(!ctr_dst->rosetta_vars && !ctr_dst->rosetta_equs);
      _copy_vars(ctr_dst, ctr_src, cdat->total_n);
      _copy_equs(ctr_dst->equs, ctr_src->equs, cdat->total_m);
   }

   return OK;

}

static int rmdl_reportvalues(Model *mdl, const Model *mdl_src)
{
   BackendType backend = mdl_src->backend;
   switch (backend) {
   case RHP_BACKEND_GAMS_GMO:
      return rctr_reporvalues_from_gams(&mdl->ctr, &mdl_src->ctr);
   case RHP_BACKEND_RHP:
   case RHP_BACKEND_JULIA:
      return rmdl_reportvalues_from_rhp(&mdl->ctr, &mdl_src->ctr);
   default:
      error("%s :: not implement for container of type %s\n",
                         __func__, backend_name(backend));
      return Error_NotImplemented;
   }
}

int rmdl_setobjsense(Model *mdl, RhpSense objsense)
{
   EmpDag *empdag = &mdl->empinfo.empdag;
   S_CHECK(chk_empdag_simple(empdag, __func__));

   return empdag_simple_setsense(empdag, objsense);
}

int rmdl_setobjequ(Model *mdl, rhp_idx objequ)
{
   if (!valid_ei(objequ)) { S_CHECK(ei_inbounds(objequ, rctr_totalm(&mdl->ctr), __func__)); }

   EmpDag *empdag = &mdl->empinfo.empdag;
   S_CHECK(chk_empdag_simple(empdag, __func__));

   S_CHECK(empdag_simple_setobjequ(empdag, objequ))

   Container *ctr = &mdl->ctr;
   EquObjectType equtype = ctr->equs[objequ].object;

   if (equtype != EQ_MAPPING) {
      error("[%s] ERROR: %s model '%.*s' #%u, the objective equation '%s' is "
            "of the wrong type: %s. Expected type is %s\n", __func__,
            mdl_fmtargs(mdl), mdl_printequname(mdl, objequ), equtype_name(equtype),
            equtype_name(EQ_MAPPING));
      return Error_InvalidArgument;
   }

   if (mdl->ctr.equmeta) {
      EquMeta *emd = &mdl->ctr.equmeta[objequ];
      EquRole equrole = emd->role;
      /* We also allow EquObjective since we might inject equations into a 
       * model (e.g. bilevel as MPEC). Not the cleanest way, Luft nach oben */
      if (equrole != EquUndefined && equrole != EquObjective) {
         equmeta_rolemismatchmsg(&mdl->ctr, objequ, equrole, EquUndefined, __func__);
         return Error_UnExpectedData;
      }

      emd->role = EquObjective;
   }

   return OK;
}

int rmdl_setobjvar(Model *mdl, rhp_idx objvar)
{
   RhpContainerData *cdat = (RhpContainerData *) mdl->ctr.data;
   if (objvar != IdxNA) { S_CHECK(vi_inbounds(objvar, cdat->total_n, __func__)); }

   EmpDag *empdag = &mdl->empinfo.empdag;
   S_CHECK(chk_empdag_simple(empdag, __func__));

   S_CHECK(empdag_simple_setobjvar(empdag, objvar))

   if (objvar == IdxNA) { return OK; }

   if (!cdat->vars[objvar]) {
      cdat->vars[objvar] = cmat_objvar(objvar);
      if (mdl->ctr.varmeta && (mdl->ctr.varmeta[objvar].ppty & VarIsDeleted)) {
        mdl->ctr.varmeta[objvar].ppty &= ~VarIsDeleted;
      }

      mdl->ctr.n++;
   }

   return OK;
}

static int rmdl_setprobtype(Model *mdl, ProbType probtype)
{
   assert(mdl_is_rhp(mdl));
   RhpModelData *mdldata = (RhpModelData *) mdl->data;
   mdldata->probtype = probtype;

   return OK;
}

static int rmdl_postprocess(Model *mdl)
{
   return OK;
}

static int rmdl_setmodelstat(Model *mdl, int modelstat)
{
   RhpModelData *model = (RhpModelData *) mdl->data;
   model->modelstat = modelstat;

   return OK;
}

static int rmdl_setsolvername(Model *mdl, const char *solvername)
{
   assert(mdl_is_rhp(mdl));
   RhpModelData *mdldat = (RhpModelData *) mdl->data;


   size_t solverid = find_str_idx(rmdl_solvernames, solvername);
   if (solverid == SIZE_MAX) {
     error("%s :: unknown solver named ``%s''\n", __func__, solvername);
     return Error_WrongParameterValue;
   }

   mdldat->solver = solverid;

   return OK;
}

static int rmdl_setsolvestat(Model *mdl, int solvestat)
{
   RhpModelData *model = (RhpModelData *) mdl->data;
   model->solvestat = solvestat;

   return OK;
}

/* TODO: this is part of GITLAB #67 */
static int rmdl_exportmodel(Model *mdl, Model *mdl_dst)
{
   if (!mdl_dst->data || !mdl_dst->ctr.data) {
      error("%s :: The destination model is empty\n", __func__);
      return Error_UnExpectedData;
   }

   trace_process("[process] Exporting %s model '%.*s' #%u to %s model '%.*s' #%u\n",
                 mdl_fmtargs(mdl), mdl_fmtargs(mdl_dst));

   switch(mdl_dst->backend) {
   case RHP_BACKEND_GAMS_GMO:
      S_CHECK(mdl_exportasgmo(mdl, mdl_dst));
      break;
   case RHP_BACKEND_RHP:
      /*  Do nothing here */
   /* TODO: this is part of GITLAB #67 */
//      S_CHECK(rmdl_exportmodel_rmdl(ctr, ctr_dst));
      break;
   default:
      error("[model] ERROR: Only GAMS and RHP are supported as a destination "
            "container, not %s\n", backend_name(mdl_dst->backend));
      return Error_NotImplemented;
   }

   /* Copy the options from one solver to the other  */
   S_CHECK(mdl_copysolveoptions(mdl_dst, mdl));


   return OK;
}

static int rmdl_solve(Model *mdl)
{
  RhpModelData *cdat = (RhpModelData *) mdl->data;

   const char *subsolver_log = mygetenv("RHP_OUTPUT_SUBSOLVER_LOG");
   if (subsolver_log) {
      O_Output_Subsolver_Log = 1;
   }
   myfreeenvval(subsolver_log);

  switch (cdat->probtype) {
  case MdlProbType_lp:         /**< LP    Linear Programm   */
  case MdlProbType_nlp:        /**< NLP   NonLinear Programm     */
  case MdlProbType_qcp:        /**< QCP   Quadratically Constraint Programm */
  case MdlProbType_emp:        /**< EMP   Extended Mathematical Programm */
  case MdlProbType_mcp:
  case MdlProbType_vi:
    return rmdl_solve_asmcp(mdl);
  case MdlProbType_dnlp:       /**< DNLP  Nondifferentiable NLP  */
    error("%s :: nonsmooth NLP are not yet supported\n", __func__);
    return Error_NotImplemented;
  case MdlProbType_mip:        /**< MIP   Mixed-Integer Programm */
  case MdlProbType_minlp:      /**< MINLP Mixed-Integer NLP*/
  case MdlProbType_miqcp:      /**< MIQCP Mixed-Integer QCP*/
    error("%s :: integer model are not yet supported\n", __func__);
    return Error_NotImplemented;
  case MdlProbType_cns:        /**< CNS   Constrained Nonlinear System */
  default:
    error("%s :: no internal solver for a model of type %s\n",
                       __func__, probtype_name(cdat->probtype));
    return Error_NotImplemented;
  }

}

const ModelOps mdl_ops_rhp = {
   .allocdata      = rmdl_allocdata,
   .deallocdata    = rmdl_deallocdata,
   .checkmdl       = rmdl_checkmdl,
   .checkobjequvar = rmdl_checkobjequvar,
   .copysolveoptions = rmdl_copysolveoptions,
   .copystatsfromsolver = rmdl_copystatsfromsolver,
   .exportmodel    = rmdl_exportmodel,
   .getmodelstat   = rmdl_getmodelstat,
   .getobjequ      = rmdl_getobjequ,
   .getobjjacval   = rmdl_getobjjacval,
   .getsense    = rmdl_getsense,
   .getobjvar      = rmdl_getobjvar,
   .getoption      = rmdl_getoption,
   .getprobtype    = rmdl_getprobtype,
   .getsolvername  = rmdl_getsolvername,
   .getsolvestat   = rmdl_getsolvestat,
   .postprocess    = rmdl_postprocess,
   .reportvalues   = rmdl_reportvalues,
   .setmodelstat   = rmdl_setmodelstat,
   .setsense    = rmdl_setobjsense,
   .setprobtype   = rmdl_setprobtype,
   .setobjvar      = rmdl_setobjvar,
   .setsolvername   = rmdl_setsolvername,
   .setsolvestat   = rmdl_setsolvestat,
   .solve          = rmdl_solve,
};
