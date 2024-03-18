#include "checks.h"
#include "cmat.h"
#include "container.h"
#include "ctr_rhp.h"
#include "ctr_rhp_add_vars.h"
#include "ctrdat_gams.h"
#include "empinfo.h"
#include "gams_solve.h"
#include "equvar_helpers.h"
#include "equvar_metadata.h"
#include "filter_ops.h"
#include "lequ.h"
#include "macros.h"
#include "mathprgm.h"
#include "mdl.h"
#include "mdl_ops.h"
#include "ctrdat_rhp.h"
#include "ctrdat_rhp_priv.h"
#include "mdl_rhp.h"
#include "nltree.h"
#include "pool.h"
#include "printout.h"
#include "reshop_solvers.h"
#include "rhp_alg.h"
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

static int rmdl_export(Model *mdl, Model *mdl_dst);

static int rmdl_allocdata(Model *mdl)
{
   int status = OK;
   RhpModelData *mdldata = NULL;
   MALLOC_EXIT(mdldata, RhpModelData, 1);
   mdldata->solver = RMDL_SOLVER_UNSET;

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

   ModelType probtype;
   S_CHECK(mdl_gettype(mdl, &probtype));


   /* TODO: GITLAB #69 */
   //if (empdag->type == EmpDag_Empty) { return status; }

   if (!mdltype_isopt(probtype)) { return status; }

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
   if (type != Mapping) {
      error("[model/rhp] ERROR: %s model '%.*s' #%u has an objective equation "
            "with type %s, but it must be %s. ", mdl_fmtargs(mdl), equtype_name(type),
            equtype_name(Mapping));
      errormsg("If there is an objective variable, it should be added to ");
      errormsg("the model as well!\n");
      return Error_EMPRuntimeError;
   }

   return OK;
}

static int rmdl_copyassolvable(Model *mdl, Model *mdl_src)
{
   return rmdl_export(mdl_src, mdl);
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

/* Copy variables attributes with filtering */
static inline void  copy_vars_rosetta(Container * ctr_dst,
                                      const Container * ctr_src)
{
   Var * restrict vars_dst = ctr_dst->vars;
   const Var * restrict vars_src = ctr_src->vars;
   const rhp_idx * restrict rosetta_vars = ctr_dst->rosetta_vars; assert(rosetta_vars);

   for (size_t i = 0, len = ctr_nvars_total(ctr_dst); i < len; ++i) {
      Var * restrict v = &vars_dst[i];
      rhp_idx vi_src = rosetta_vars[i];

      if (!valid_vi(vi_src)) {
         v->value = SNAN;
         v->multiplier = SNAN;
         v->basis = BasisUnset;
      } else {
         assert(!v->is_deleted);
         const Var * restrict v_src = &vars_src[vi_src];
         v->value = v_src->value;
         v->multiplier = v_src->multiplier;
         v->basis = v_src->basis;
         SOLREPORT_DEBUG("VAR %-30s " SOLREPORT_FMT " from downstream var %s",
                         ctr_printvarname(ctr_dst, i), solreport_gms_v(v), 
                         ctr_printvarname2(ctr_src, vi_src));
      }
   }
}

static inline int  copy_equs_rosetta(Container * restrict ctr_dst,
                                     const Container * restrict ctr_src)
{
   Equ * restrict equs_dst = ctr_dst->equs;
   const Equ * restrict equs_src = ctr_src->equs;
   const  rhp_idx * restrict rosetta_equs = ctr_dst->rosetta_equs; assert(rosetta_equs);

   for (unsigned i = 0, len = ctr_nequs_total(ctr_dst); i < len; ++i) {
      Equ * restrict e = &equs_dst[i];
      rhp_idx ei_src = rosetta_equs[i];

      if (valid_ei(ei_src)) {
         assert(valid_ei_(ei_src, ctr_nequs_total(ctr_src), __func__));

         const Equ * restrict e_src = &equs_src[ei_src];

         SOLREPORT_DEBUG("EQU %-30s " SOLREPORT_FMT " using equ %s",
                            ctr_printequname(ctr_dst, i), solreport_gms(e_src), 
                            ctr_printequname2(ctr_src, ei_src));

         e->value = e_src->value;
         e->multiplier = e_src->multiplier;
         e->basis = e_src->basis;

         continue;
      }

  /* ----------------------------------------------------------------------
   * Equations was deleted. Check if it was transformed
   * ---------------------------------------------------------------------- */

      EquInfo equinfo;
      S_CHECK(rctr_get_equation(ctr_dst, i, &equinfo));

      assert(valid_ei_(equinfo.ei, len, __func__));
      ei_src = rosetta_equs[equinfo.ei];

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

      } else { /* This case is encountered for any objequ while using an MCP/VI solver */

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
      return OK;
   }

   if (ctr_dst->rosetta_equs){
      copy_equs_rosetta(ctr_dst, ctr_src);
   }  else {
      _copy_equs(ctr_dst->equs, ctr_src->equs, cdat->total_m);
   }

   if (ctr_dst->rosetta_vars){
      copy_vars_rosetta(ctr_dst, ctr_src);
   }  else {
      _copy_vars(ctr_dst, ctr_src, cdat->total_n);
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

/**
 * @brief Set the objective function of the model
 *
 * @param mdl The model
 * @param ei  The objective function index
 *
 * @return    The error code
 */
int rmdl_setobjfun(Model *mdl, rhp_idx ei)
{
   S_CHECK(chk_ei(mdl, ei, __func__));

   EmpDag *empdag = &mdl->empinfo.empdag;
   S_CHECK(chk_empdag_simple(empdag, __func__));

   S_CHECK(empdag_simple_setobjequ(empdag, ei))

   Container *ctr = &mdl->ctr;
   EquObjectType equtype = ctr->equs[ei].object;

   if (equtype != Mapping) {
      error("[%s] ERROR: %s model '%.*s' #%u, the objective equation '%s' is "
            "of the wrong type: %s. Expected type is %s\n", __func__,
            mdl_fmtargs(mdl), mdl_printequname(mdl, ei), equtype_name(equtype),
            equtype_name(Mapping));
      return Error_InvalidArgument;
   }

   if (mdl->ctr.equmeta) {
      EquMeta *emd = &mdl->ctr.equmeta[ei];
      EquRole equrole = emd->role;
      /* We also allow EquObjective since we might inject equations into a 
       * model (e.g. bilevel as MPEC). Not the cleanest way, Luft nach oben */
      if (equrole != EquUndefined && equrole != EquObjective) {
         equmeta_rolemismatchmsg(&mdl->ctr, ei, equrole, EquUndefined, __func__);
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

/**
 * @brief Make sure that the model has an new objective function
 *
 * If the objective equation already existed, the objective variable
 * should be eliminated from the new objective function
 *
 * If the objective equation did not already existed, then create it and
 * add the objective variable to it
 *
 * @param          mdl     the model
 * @param          fops    the filter ops
 * @param          objvar  the objective variable
 * @param[in,out]  objequ  on input, the current objequ, on output the new objequ
 * @param[out]     e_obj   on output, a pointer to the equation
 *
 * @return                 the error code
 */
static NONNULL_AT(1,4,5)
int ensure_newobjfun(Model *mdl, Fops *fops, rhp_idx objvar, rhp_idx *objequ,
                     Equ **e_obj)
{
   rhp_idx lobjequ = *objequ;
   S_CHECK(rctr_reserve_equs(&mdl->ctr, 1));

  /* ------------------------------------------------------------------------
   * If objequ points to a valid equation, then we check that the objvar
   * is present. Otherwise, we add a new equation
   * ------------------------------------------------------------------------ */

   if (!valid_ei(lobjequ) || (fops && !fops->keep_equ(fops->data, lobjequ))) {
      S_CHECK(rctr_add_equ_empty(&mdl->ctr, &lobjequ, e_obj, Mapping, CONE_NONE));
      assert(valid_ei(lobjequ));
      *objequ = lobjequ;
      S_CHECK(rmdl_setobjfun(mdl, lobjequ));
      return rctr_equ_addnewvar(&mdl->ctr, *e_obj, objvar, 1.);

   }

   Lequ *le = mdl->ctr.equs[lobjequ].lequ;
   double objvar_coeff;
   unsigned dummyint;
   S_CHECK(lequ_find(le, objvar, &objvar_coeff, &dummyint));

   if (!isfinite(objvar_coeff)) {
      error("%s :: objvar '%s' could not be found in equation '%s'\n",
            __func__, ctr_printvarname(&mdl->ctr, objvar), 
            ctr_printequname(&mdl->ctr, lobjequ));
      return Error_IndexOutOfRange;
   }

   S_CHECK(rmdl_dup_equ(mdl, objequ, 0, objvar));
   lobjequ = *objequ;
   Equ *e = &mdl->ctr.equs[lobjequ];
   *e_obj = e;

   double coeff_inv = -1./objvar_coeff;
   S_CHECK(lequ_scal(e->lequ, coeff_inv));
   if (e->tree && e->tree->root) {
      S_CHECK(nltree_scal(&mdl->ctr, e->tree, coeff_inv));
   }
   S_CHECK(cmat_scal(&mdl->ctr, e->idx, coeff_inv));

   double cst = equ_get_cst(e);
   equ_set_cst(e, cst*coeff_inv);

   return OK;
}

/**
 * @brief Create the objective variable and add it to the objective equation
 *
 * @param          ctr     the model
 * @param          fops    the filter ops
 * @param[out]     objvar  on output the objective variable
 * @param[in,out]  objequ  on input, the current objequ, on output a valid objequ
 *
 * @return                 the error code
 */

static NONNULL_AT(1,3,4)
int create_objvar_in_objequ(Container *ctr, Fops *fops, rhp_idx *objvar, rhp_idx *objequ)
{
   /* ----------------------------------------------------------------------
    * Ensure that an objective variable exists and add an objective equation
    * if it is missing.
    *
    * There are instance where there is not even an objective function, like
    * when are given a feasibility problem. In that case, we also have to
    * add a dummy objective equation.
    *
    * If the objequ index was valid, we force the evaluation of the objequ
    * based on this objvar
    * ---------------------------------------------------------------------- */

   Equ *e_obj;
   rhp_idx lobjequ = *objequ;
   if (!valid_ei(lobjequ) || (fops && !fops->keep_equ(fops->data, lobjequ))) {
      S_CHECK(rctr_reserve_equs(ctr, 1));
      S_CHECK(rctr_add_equ_empty(ctr, objequ, &e_obj, ConeInclusion, CONE_0));
   } else {
      e_obj = &ctr->equs[lobjequ];
      if (e_obj->object == Mapping) {
         e_obj->object = ConeInclusion;
         e_obj->cone = CONE_0;
      } else if (e_obj->object != ConeInclusion) {
         error("[model] ERROR: the objective equation has type %s, which is "
               "unsupported. Please file a bug report\n",
               equtype_name(e_obj->object));
         return Error_RuntimeError;
      }
   }

   S_CHECK(rctr_reserve_vars(ctr, 1));
   Avar v;
   S_CHECK(rctr_add_free_vars(ctr, 1, &v));

   rhp_idx lobjvar = v.start;
   S_CHECK(rctr_equ_addnewvar(ctr, e_obj, lobjvar, -1.));
   ctr->vars[lobjvar].value = 0.; /* TODO: delete or initialize via evaluation */
   *objvar = lobjvar;

   return OK;
}

/* ----------------------------------------------------------------------
 * TODO(xhub) DOC internal fn
 * ---------------------------------------------------------------------- */
static NONNULL_AT(1,2,3)
int objvar_gamschk(Model *mdl, rhp_idx * restrict objvar,
                   rhp_idx * restrict objequ, Fops *fops)
{
   RhpContainerData *cdat = mdl->ctr.data;

   /* ----------------------------------------------------------------------
    * If the objective variable is not valid, we add an objective variable
    * and then 
    * ---------------------------------------------------------------------- */
   if (!valid_vi(*objvar) || (fops && !fops->keep_var(fops->data, *objvar))) {

      S_CHECK(create_objvar_in_objequ(&mdl->ctr, fops, objvar, objequ));

      cdat->pp.remove_objvars++;

   } else { /* valid objvar index, but if it is a free variable, we need to replace it */
      Container *ctr = &mdl->ctr;
      Var *v = &ctr->vars[*objvar];
      if (v->type != VAR_X || v->is_conic || v->bnd.lb != -INFINITY ||
         v->bnd.ub != INFINITY) {

         Equ *e_obj;
         if (!valid_ei(*objequ) || (fops && !fops->keep_equ(fops->data, *objequ))) {
            S_CHECK(rctr_reserve_equs(ctr, 1));
            S_CHECK(rctr_add_equ_empty(ctr, objequ, &e_obj, Mapping, CONE_NONE));
            S_CHECK(rctr_equ_addnewvar(ctr, e_obj, *objvar, 1.));
         } else {
            e_obj = &mdl->ctr.equs[*objequ];
            assert(e_obj->cone == CONE_NONE && e_obj->object == Mapping);
            assert(!ctr->equmeta || ctr->equmeta[e_obj->idx].role == EquObjective);
         }

         S_CHECK(rctr_reserve_vars(ctr, 1));
         Avar vv;
         S_CHECK(rctr_add_free_vars(ctr, 1, &vv));

         *objvar = vv.start;
         S_CHECK(rctr_equ_addnewvar(ctr, e_obj, *objvar, -1.));
         mdl->ctr.vars[*objvar].value = 0.;

         cdat->pp.remove_objvars++;
      }
   }

   return rmdl_checkobjequvar(mdl, *objvar, *objequ);
}

static NONNULL_AT(1) int ensure_objvar_exists(Model *mdl, Fops *fops)
{
   rhp_idx objvar, objequ;
   EmpDag *empdag = &mdl->empinfo.empdag;
   RhpContainerData *cdat = mdl->ctr.data;

   assert(empdag->type == EmpDag_Empty);
   objvar = empdag->simple_data.objvar;
   objequ = empdag->simple_data.objequ;

   rhp_idx objvar_bck = objvar, objequ_bck = objequ;
   bool update_objequ = !valid_ei(objequ);

   S_CHECK(objvar_gamschk(mdl, &objvar, &objequ, fops));

   if (update_objequ) {
      trace_process("[process] %s model %.*s #%u: objequ is now %s\n",
                    mdl_fmtargs(mdl), mdl_printequname(mdl, objequ));
      S_CHECK(rmdl_setobjfun(mdl, objequ));
   } else {
      assert(objequ_bck == objequ);
   }

   if (objvar_bck != objvar) {
      assert(mdl->empinfo.empdag.type == EmpDag_Empty);

      trace_process("[process] %s model %.*s #%u: adding objvar %s to objequ %s\n",
                    mdl_fmtargs(mdl), mdl_printvarname(mdl, objvar),
                    mdl_printequname(mdl, objequ));

      empdag->simple_data.objvar = objvar;
      cdat->objequ_val_eq_objvar = true;
   }

   return OK;
}

#if 0
static int objvarmp_gamschk(Model *mdl, MathPrgm *mp, Fops *fops)
{
   rhp_idx objvar, objequ;
   EmpDag *empdag = &mdl->empinfo.empdag;
   RhpContainerData *ctrdat = mdl->ctr.data;

   if (!mp) {
      assert(empdag->type == EmpDag_Empty);
      objvar = empdag->simple_data.objvar;
      objequ = empdag->simple_data.objequ;
   } else {
      objvar = mp_getobjvar(mp);
      objequ = mp_getobjequ(mp);
   }

   rhp_idx objvar_bck = objvar, objequ_bck = objequ;
   bool update_objequ = !valid_ei(objequ);

   S_CHECK(objvar_gamschk(mdl, &objvar, &objequ, fops));

   if (update_objequ) {
      if (mp) {
         S_CHECK(mp_setobjequ(mp, objequ));
      } else {
         trace_process("[process] %s model %.*s #%u: objequ is now %s\n",
                       mdl_fmtargs(mdl), mdl_printequname(mdl, objequ));
         S_CHECK(rmdl_setobjequ(mdl, objequ));
      }
   } else {
      assert(objequ_bck == objequ);
   }

   if (objvar_bck != objvar) {
      if (mp) {
         S_CHECK(mp_setobjvar(mp, objvar));
         S_CHECK(mp_objvarval2objequval(mp));
      } else {
         assert(mdl->empinfo.empdag.type == EmpDag_Empty);

         trace_process("[process] %s model %.*s #%u: adding objvar %s to objequ %s\n",
                       mdl_fmtargs(mdl), mdl_printvarname(mdl, objvar),
                       mdl_printequname(mdl, objequ));

         empdag->simple_data.objvar = objvar;
         ctrdat->objequ_val_eq_objvar = true;
      }
   }

   return OK;
}
#endif

static int rctr_convert_metadata_togams(Container *ctr, Container *ctr_gms)
{
   /* ----------------------------------------------------------------------
    * The SOS variables need special care: copy the group ID  
    * ---------------------------------------------------------------------- */

   RhpContainerData *cdat = (RhpContainerData *)ctr->data;
   GmsContainerData *gms = (GmsContainerData *)ctr_gms->data;

   assert(ctr_gms->status & CtrEquVarInherited && !(ctr_gms->status & CtrMetaDataAvailable));

   /* TODO fops switch */
   rhp_idx * restrict rosetta_vars = ctr->rosetta_vars;
   if (cdat->sos1.len > 0) {

      S_CHECK(chk_uint2int(cdat->sos1.len, __func__));
      CALLOC_(gms->sos_group, int, ctr_nvars(ctr_gms));

      for (int i = 0, len = (int)cdat->sos1.len; i < len; i++) {
         Avar *v = &cdat->sos1.groups[i].v;

         for (unsigned j = 0, size = v->size; j < size; ++j) {
            rhp_idx vi = avar_fget(v, j);
            vi = rosetta_vars ? rosetta_vars[vi] : vi;
            gms->sos_group[vi] = i+1;
         }
      }
   }

   if (cdat->sos2.len > 0) {
      if (!gms->sos_group) {
         CALLOC_(gms->sos_group, int, ctr_nvars(ctr_gms));
      }

      S_CHECK(chk_uint2int(cdat->sos2.len, __func__));
      for (int i = 0, len = (int)cdat->sos2.len; i < len; i++) {

         Avar *v = &cdat->sos2.groups[i].v;
         for (unsigned j = 0, size = v->size; j < size; ++j) {
            rhp_idx vi = avar_fget(v, j);
            vi = rosetta_vars ? rosetta_vars[vi] : vi;
            gms->sos_group[vi] = i+1;
         }
      }
   }

   ctr->status |= CtrMetaDataAvailable;
   return OK;
}

static NONNULL int rmdl_prepare_ctrexport_gams(Model *mdl_src, Model *mdl_dst)
{
   Container *ctr_src = &mdl_src->ctr;
   Container *ctr_dst = &mdl_dst->ctr;
   RhpContainerData *cdat = (RhpContainerData *)ctr_src->data;

   S_CHECK(ctr_prepare_export(ctr_src, ctr_dst));

   S_CHECK(rctr_convert_metadata_togams(ctr_src, ctr_dst));

   mdl_dst->status |= MdlContainerInstantiable;

   return OK;
}

static NONNULL_AT(1)
int check_var_is_really_deleted(Container *ctr, Fops *fops, rhp_idx vi)
{
   if (fops && fops->keep_var(fops->data, vi)) {
      error("%s :: variable '%s' #%u should be inactive but is not marked as such\n",
            __func__, ctr_printvarname(ctr, vi), vi);
      return Error_Inconsistency;
   }

   return OK;
}

static NONNULL
int rmdl_prepare_ctrexport_rhp(Model *mdl, Model *mdl_dst)
{
   Container *ctr_src = &mdl->ctr;
   EmpInfo *empinfo = &mdl->empinfo;

   /* ----------------------------------------------------------------------
    * We need to remove objective variables and also fixed variables
    * 
    * TODO(xhub) URG : we allow ourself to modify an equation in place,
    * without copying it, which quite bad
    * ---------------------------------------------------------------------- */

   /* TODO GITLAB #71 */
   Fops *fops = ctr_src->fops;

   EmpDag *empdag = &empinfo->empdag;
   if (empdag->type == EmpDag_Empty) {

      rhp_idx objvar;
      rmdl_getobjvar(mdl, &objvar);

      if (valid_vi(objvar)) {

         Equ *e_obj;
         rhp_idx objequ_old;
         rmdl_getobjequ(mdl, &objequ_old);

         rhp_idx objfun = objequ_old;
         S_CHECK(ensure_newobjfun(mdl, fops, objvar, &objfun, &e_obj));
         trace_process("[process] %s model %.*s #%u: objvar '%s' removed; "
                       "objective function is now '%s'\n", mdl_fmtargs(mdl),
                       mdl_printvarname(mdl, objvar), mdl_printequname(mdl, objfun));

         if (valid_ei(objequ_old)) {

            S_CHECK(check_var_is_really_deleted(ctr_src, fops, objvar));
            S_CHECK(rctr_add_eval_equvar(ctr_src, objequ_old, objvar));
         } /* TODO ensure that objvar values are set  */

         rmdl_setobjvar(mdl, IdxNA);
      }
   } else {
      unsigned mp_len = empdag_getmplen(empdag);
      assert(mp_len > 0);
      for (unsigned i = 0; i < mp_len; ++i) {
         MathPrgm *mp = empdag_getmpfast(empdag, i);
         if (!mp) continue;

         rhp_idx objvar = mp_getobjvar(mp);
         rhp_idx objequ = mp_getobjequ(mp);

         if (valid_vi(objvar) && valid_ei(objequ)) {
            Equ *e_obj;
            rhp_idx objequ_old = objequ;

            S_CHECK(ensure_newobjfun(mdl, fops, objvar, &objequ, &e_obj));

            assert(objequ_old != objequ);
            S_CHECK(rctr_add_eval_equvar(ctr_src, objequ_old, objvar));
            S_CHECK(check_var_is_really_deleted(ctr_src, fops, objvar));
            /* TODO ensure that objvar values are set  */

            S_CHECK(mp_setobjvar(mp, IdxNA));
            S_CHECK(mp_setobjequ(mp, objequ));
            trace_process("[process] MP(%s): objvar '%s' removed; objequ is now '%s'\n",
                          empdag_getmpname(empdag, i), mdl_printvarname(mdl, objvar),
                          mdl_printequname(mdl, objequ));
         }
      }
   }

   /* TODO GITLAB #90 */
  /* -----------------------------------------------------------------------
   * We go over all the variables identify fixed variables
   * ----------------------------------------------------------------------- */
//  S_CHECK(ctr_fixedvars(ctr));

//  S_CHECK(rmdl_remove_fixedvars(mdl));

   S_CHECK(ctr_prepare_export(ctr_src, &mdl_dst->ctr));

   ctr_src->status |= CtrMetaDataAvailable;

   return OK;
}

/** 
 *  @brief Prepare a ReSHOP model for the export into another model
 *
 *
 *  @param mdl_src   the original model
 *  @param mdl_dst   the destination model
 *
 *  @return          the error code
 */
int rmdl_prepare_export(Model * restrict mdl_src, Model * restrict mdl_dst)
{
   Container *ctr_src = &mdl_src->ctr;
   Container *ctr_dst = &mdl_dst->ctr;

   assert(!(mdl_dst->status & MdlInstantiable));

   trace_process("[process] %s model %.*s #%u: exporting to %s model %.*s #%u\n",
                 mdl_fmtargs(mdl_src), mdl_fmtargs(mdl_dst));

   /* Just make sure we increase the stage to make sure objvar are evaluated properly */
   S_CHECK(rmdl_incstage(mdl_src));

  /* ----------------------------------------------------------------------
   * If there are any deleted/inactive equations or variables, set the fops
   * ATTENTION: this needs to be set before preparing the container export
   * ---------------------------------------------------------------------- */

   RhpContainerData *cdat = (RhpContainerData *)ctr_src->data;
   if (cdat->total_m != ctr_src->m || cdat->total_n != ctr_src->n) {
      S_CHECK(rmdl_ensurefops_activedefault(mdl_src));
   }

   /* ----------------------------------------------------------------------
    * If the destination container is
    * - a GAMS GMO object, we may need to add objective variable(s)
    * - a RHP, we need to remove any objective variables(s) and change
    *   the equations.
    * ---------------------------------------------------------------------- */

   switch (ctr_dst->backend) {
   case RHP_BACKEND_GAMS_GMO:
      S_CHECK(rmdl_prepare_ctrexport_gams(mdl_src, mdl_dst));
      break;
   case RHP_BACKEND_RHP:
      S_CHECK(rmdl_prepare_ctrexport_rhp(mdl_src, mdl_dst));
      break;
   default:
      error("%s ERROR: unsupported destination model type %d\n", __func__,
            ctr_dst->backend);
      return Error_NotImplemented;
   }

   /* HACK: we need this again as we might have delete equations/variables */
   if (cdat->total_m != ctr_src->m || cdat->total_n != ctr_src->n) {
      S_CHECK(rmdl_ensurefops_activedefault(mdl_src));
   }

   /* ----------------------------------------------------------------------
    * There may be no pool in the original container.
    * We have to be careful in the following to ensure that later on it exists
    * when we want to use it.
    * ---------------------------------------------------------------------- */

   trace_model("[export] %s model '%.*s' #%u with %u vars and %u equs, is ready "
               "to receive export from %s model '%.*s' #%u\n", mdl_fmtargs(mdl_dst),
               ctr_dst->n, ctr_dst->m, mdl_fmtargs(mdl_src));

   if (ctr_dst->status & CtrInstantiable) {
      mdl_dst->status |= MdlContainerInstantiable;
   } else {
      error("[model] ERROR: while preparing the export in %s model '%.*s' #%u: "
            "container is not ready after %s\n", mdl_fmtargs(mdl_dst), __func__);
      return Error_RuntimeError;
   }

   return OK;
}

static NONNULL int rmdl_export_setmodeltype(Model *mdl, Model *mdl_dst)
{
   Fops *fops = mdl->ctr.fops;
   if (!fops) {
      S_CHECK(mdl_copyprobtype(mdl_dst, mdl));
   }

   return OK;
}

/* TODO: this is part of GITLAB #67 */
static int rmdl_export(Model *mdl, Model *mdl_dst)
{
   assert(mdl_dst->ctr.n == 0 && mdl_dst->ctr.m == 0);

   S_CHECK(rmdl_prepare_export(mdl, mdl_dst));

  /* ----------------------------------------------------------------------
   * Get the modeltype
   * ---------------------------------------------------------------------- */
   S_CHECK(rmdl_export_setmodeltype(mdl, mdl_dst));

   switch(mdl_dst->backend) {
   case RHP_BACKEND_GAMS_GMO:
      S_CHECK(rmdl_exportasgmo(mdl, mdl_dst));
      break;
   case RHP_BACKEND_RHP: {
      /*  Do nothing here */
   /* TODO: this is part of GITLAB #67 */
//      S_CHECK(rmdl_exportmodel_rmdl(ctr, ctr_dst));
   ModelType probtype;
   mdl_gettype(mdl_dst, &probtype);

   if (probtype == MdlType_none) {
      S_CHECK(mdl_copyprobtype(mdl_dst, mdl));
   }
      }
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

   ModelType type = mdl->commondata.mdltype;

  switch (type) {
  case MdlType_lp:         /**< LP    Linear Programm   */
  case MdlType_nlp:        /**< NLP   NonLinear Programm     */
  case MdlType_qcp:        /**< QCP   Quadratically Constraint Programm */
  case MdlType_emp:        /**< EMP   Extended Mathematical Programm */
  case MdlType_mcp:
  case MdlType_vi:
    return rmdl_solve_asmcp(mdl);
  case MdlType_dnlp:       /**< DNLP  Nondifferentiable NLP  */
    error("%s :: nonsmooth NLP are not yet supported\n", __func__);
    return Error_NotImplemented;
  case MdlType_mip:        /**< MIP   Mixed-Integer Programm */
  case MdlType_minlp:      /**< MINLP Mixed-Integer NLP*/
  case MdlType_miqcp:      /**< MIQCP Mixed-Integer QCP*/
    error("%s :: integer model are not yet supported\n", __func__);
    return Error_NotImplemented;
  case MdlType_cns:        /**< CNS   Constrained Nonlinear System */
  default:
    error("%s :: no internal solver for a model of type %s\n", __func__,
          mdltype_name(type));
    return Error_NotImplemented;
  }

}

const ModelOps mdl_ops_rhp = {
   .allocdata           = rmdl_allocdata,
   .deallocdata         = rmdl_deallocdata,
   .checkmdl            = rmdl_checkmdl,
   .checkobjequvar      = rmdl_checkobjequvar,
   .copyassolvable      = rmdl_copyassolvable,
   .copysolveoptions    = rmdl_copysolveoptions,
   .copystatsfromsolver = rmdl_copystatsfromsolver,
   .export              = rmdl_export,
   .getmodelstat        = rmdl_getmodelstat,
   .getobjequ           = rmdl_getobjequ,
   .getobjjacval        = rmdl_getobjjacval,
   .getsense            = rmdl_getsense,
   .getobjvar           = rmdl_getobjvar,
   .getoption           = rmdl_getoption,
   .getsolvername       = rmdl_getsolvername,
   .getsolvestat        = rmdl_getsolvestat,
   .postprocess         = rmdl_postprocess,
   .reportvalues        = rmdl_reportvalues,
   .setmodelstat        = rmdl_setmodelstat,
   .setsense            = rmdl_setobjsense,
   .setobjvar           = rmdl_setobjvar,
   .setsolvername       = rmdl_setsolvername,
   .setsolvestat        = rmdl_setsolvestat,
   .solve               = rmdl_solve,
};
