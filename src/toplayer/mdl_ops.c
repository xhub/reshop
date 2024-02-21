#include "ctrdat_rhp.h"
#include "equvar_helpers.h"
#include "filter_ops.h"
#include "mdl.h"
#include "mdl_data.h"
#include "mdl_ops.h"
#include "ovfinfo.h"
#include "printout.h"

int mdl_copysolveoptions(Model *mdl, const Model *mdl_src)
{
   return mdl->ops->copysolveoptions(mdl, mdl_src);
}

/**
 * @brief Set the model type
 *
 * @ingroup publicAPI
 *
 * @param mdl        the model
 * @param mdl_src  the source model
 *
 * @return           the error code
 */
int mdl_copyprobtype(Model *mdl, const Model *mdl_src)
{
   ProbType probtype;
   S_CHECK(mdl_src->ops->getprobtype(mdl_src, &probtype));

   assert(probtype != MdlProbType_none);
   if (probtype >= probtypeslen) {
      error("%s :: unknown model type %d\n", __func__, probtype);
      return Error_InvalidValue;
   }
   
   trace_stack("[model] %s model '%.*s' #%u: setting model type to %s "
               "from %s model '%.*s' #%u\n", mdl_fmtargs(mdl),
               probtype_name(probtype), mdl_fmtargs(mdl_src));

   return mdl_setprobtype(mdl, probtype);
}

/**
 * @brief Check the model 
 *
 * - Fixed variables have values set to their bounds
 *
 * @param mdl  the model to check
 *
 * @return     the error code
 */
int mdl_check(Model *mdl)
{
   if (mdl_checked(mdl)) {
      return OK;
   }

   if (!mdl_finalized(mdl)) {
      S_CHECK(mdl_finalize(mdl));
   }

   rhp_idx objvar, objequ;
   RhpSense sense;
   S_CHECK(mdl_getobjvar(mdl, &objvar))
   S_CHECK(mdl_getobjequ(mdl, &objequ))
   S_CHECK(mdl_getsense(mdl, &sense))
   bool has_optobj =  valid_vi(objvar) || valid_ei(objequ);

   ProbType probtype;
   S_CHECK(mdl_getprobtype(mdl, &probtype));

   switch (probtype) {
   case MdlProbType_none:
      error("[model check] ERROR: %s model '%.*s' #%u has no type set", mdl_fmtargs(mdl));
      return Error_InvalidModel;
   case MdlProbType_lp:
   case MdlProbType_nlp:
   case MdlProbType_dnlp:
   case MdlProbType_mip:
   case MdlProbType_minlp:
   case MdlProbType_miqcp:
   case MdlProbType_qcp:
   case MdlProbType_mpec:
      if (!has_optobj) {
         error("[model check] ERROR: %s model '%.*s' #%u of type %s has neither"
               " an objective variable or an objective function.\n", mdl_fmtargs(mdl),
               probtype_name(probtype));
         return Error_InvalidModel;
      }
      break;
   case MdlProbType_emp:
      if (mdl->empinfo.empdag.mps.len == 0 && valid_optsense(sense)) {
         if (!has_optobj) {
         error("[model check] ERROR: %s model '%.*s' #%u of type %s has neither"
               " an objective variable or an objective function.\n", mdl_fmtargs(mdl),
               probtype_name(probtype));
         return Error_InvalidModel;
         }
      }
      /* TODO where to check the compatibility between empdag and objective stuff */
      break;

   case MdlProbType_mcp:
   case MdlProbType_vi:
   case MdlProbType_cns:
      if (has_optobj) {
         error("[model check] ERROR: %s model '%.*s' #%u of type %s has either an"
               " objective variable or an objective function.\n", mdl_fmtargs(mdl),
               probtype_name(probtype));
         return Error_InvalidModel;
      }
      break;
   default:
      error("[model check] ERROR: unknown model type %s (#%d)",
            probtype_name(probtype), probtype);
      return Error_RuntimeError;
   }

   int status = mdl->ops->checkmdl(mdl);
   if (status == OK) {
      mdl_setchecked(mdl);
   }

   return status;
}

static int mdl_chk_mcpmetadata(const Model *mdl)
{
   int status = OK;

   unsigned nvars = mdl->ctr.n;
   unsigned nequs = mdl->ctr.m;

   if (nvars != nequs) {
      error("[mcp check] ERROR: %s model '%.*s' #%u of type MCP has %u variables"
            " and %u equations. An MCP model must be square\n", mdl_fmtargs(mdl),
            nvars, nequs);
      return Error_RuntimeError;
   }

   VarMeta *vmeta = mdl->ctr.varmeta;
   for (rhp_idx vi = 0; vi < nvars; ++vi) {
      rhp_idx ei = vmeta[vi].dual;
      if (!valid_ei_(ei, nvars, "[mcp check]")) {
         error("[mcp check] ERROR: variable '%s' has an invalid dual equation. "
               "See above for details\n", mdl_printvarname(mdl, vi));
         status = status == OK ? Error_ModelIncompleteMetadata : status;
      }
   }

   EquMeta *emeta = mdl->ctr.equmeta;
   for (rhp_idx ei = 0; ei < nvars; ++ei) {
      rhp_idx vi = emeta[ei].dual;
      if (!valid_vi_(vi, nvars, "[mcp check]")) {
         error("[mcp check] ERROR: equation '%s' has an invalid dual variable. "
               "See above for details\n", mdl_printequname(mdl, ei));
         status = status == OK ? Error_ModelIncompleteMetadata : status;
      }
   }

   return status;
}

int mdl_checkmetadata(Model *mdl)
{
   EmpDag *empdag = &mdl->empinfo.empdag;
   if (!empdag->finalized) {
      S_CHECK(empdag_fini(empdag))
   }

   OvfInfo *ovfinfo = mdl->empinfo.ovf;
   OvfDef *ovfdef = ovfinfo ? ovfinfo->ovf_def : NULL;

   while (ovfdef) {
      S_CHECK(ovf_finalize(mdl, ovfdef));
      ovfdef = ovfdef->next;
   }

   if (mdl_metachecked(mdl)) {
      return OK;
   }

   if (!mdl_finalized(mdl)) {
      S_CHECK(mdl_finalize(mdl));
   }

   ProbType probtype;
   EmpInfo *empinfo = &mdl->empinfo;
   S_CHECK(mdl_getprobtype(mdl, &probtype));

   if (!probtype_hasmetadata(probtype) || ((probtype == MdlProbType_emp) &&
      (empdag_isopt(&empinfo->empdag) || empdag_isempty(&empinfo->empdag)))) {
     return OK;
   }

   Container *ctr = &mdl->ctr;
   if (!ctr->varmeta || !ctr->equmeta) {
      errormsg("[metadata check]  ERROR: varmeta or equmeta is NULL\n");
      return Error_NullPointer;
   }

   /* TODO: GITLAB#70 */
   if (probtype == MdlProbType_mpec) { return OK; }

   if (probtype == MdlProbType_mcp) {
      return mdl_chk_mcpmetadata(mdl);
   }

   unsigned num_unattached_vars = 0;
   unsigned num_unattached_equs = 0;

   Fops *fops, *fops_active, fops_active_dat;

   if (mdl_is_rhp(mdl)) {
      RhpContainerData *cdat = (RhpContainerData *)ctr->data;
      S_CHECK(fops_active_init(&fops_active_dat, ctr));
      fops = cdat->fops;
      fops_active = &fops_active_dat;
   } else if (mdl->backend == RHP_BACKEND_GAMS_GMO) {
      fops = fops_active = NULL;
   } else {
      TO_IMPLEMENT("metadata check for new backend model");
   }

   size_t total_n = ctr_nvars_total(ctr);
   size_t total_m = ctr_nequs_total(ctr);
   int status = OK;

   for (rhp_idx i = 0; i < total_n; ++i) {
      /* If the variable is going to disappear, continue */
      if (fops && !fops->keep_var(fops->data, i)) continue;
      /* If the variable does not appear in the model, continue */
      if (fops_active && !fops_active->keep_var(fops_active->data, i)) continue;

      const VarMeta var_md = ctr->varmeta[i];

      if (!valid_mpid(var_md.mp_id)) {
         error("[empdag] ERROR: variable '%s' is not attached to any MP\n",
               ctr_printvarname(ctr, i));
         num_unattached_vars++;
         status = status == OK ? Error_ModelIncompleteMetadata : status;
         continue;
      }

      switch (var_md.type) {
      case VarUndefined:
         error("[metadata check] Error: Variable '%s' has an undefined type\n", 
               ctr_printvarname(ctr, i));
         status = status == OK ? Error_ModelIncompleteMetadata : status;
         break;
      case VarObjective:
      {
         rhp_idx ei_dual = var_md.dual;
         enum VarPptyType ppty = var_md.ppty;

         if (valid_ei(ei_dual) && ei_dual <= total_m) {
           error("[metadata check] ERROR: %s '%s' has a valid dual equation '%s'"
                 ", this is inconsistent.\n",  varmetatype_name(var_md.type),
                 ctr_printvarname(ctr, i), ctr_printequname(ctr, ei_dual));
           status = status == OK ? Error_ModelIncompleteMetadata : status;
         }

         if (ppty & VarIsDeleted) {
           error("[metadata check] ERROR: %s '%s' has a deleted subtype, "
                 "but is still seen as active by the model.\n",
                 varmetatype_name(var_md.type), ctr_printvarname(ctr, i));
           status = status == OK ? Error_ModelIncompleteMetadata : status;
         }

         /*  If we have an objective equation, then the variable should not have extra properties */
         if ((ppty & VarIsExplicitlyDefined) && (ppty &
             ~(VarIsObjMin | VarIsObjMax | VarIsExplicitlyDefined))) {
           error("[metadata check] ERROR: %s '%s' has an inconsistent subtype.\n",
                 varmetatype_name(var_md.type), ctr_printvarname(ctr, i));
           status = status == OK ? Error_ModelIncompleteMetadata : status;
         }

         /* Check for inconsistent subtypes  */
         if ((ppty & VarIsObjMin) && (ppty & VarIsObjMax)) {
           error("[metadata check] ERROR: %s '%s' has both minimize and maximize subtypes.\n",
                 varmetatype_name(var_md.type), ctr_printvarname(ctr, i));
           status = status == OK ? Error_ModelIncompleteMetadata : status;
         }
         break;
      }
      case VarPrimal:
      {
         if (var_md.ppty & VarIsDeleted) {
           error("[metadata check] ERROR: %s '%s' has a deleted subtype, but "
                 "is still seen as active by the model.\n",
                 varmetatype_name(var_md.type), ctr_printvarname(ctr, i));
           status = status == OK ? Error_ModelIncompleteMetadata : status;
         }

         rhp_idx ei_dual = var_md.dual;

         unsigned vbasictype = vmd_basictype(var_md.ppty);

         if (valid_ei(ei_dual) && ei_dual <= total_m) {
           if (vbasictype == VarPerpToViFunction) { /* This is OK */
           } else if (vbasictype == VarPerpToZeroFunctionVi) {
              /* If we have a SubZeroFunction, no dual equ must be present */
              error("[metadata check] ERROR: %s '%s' is perpendicular to the zero "
                 "function, but has a valid dual equation '%s'.\n",
                 varmetatype_name(var_md.type), ctr_printvarname(ctr, i),
                 ctr_printequname(ctr, ei_dual));

              status = status == OK ? Error_ModelIncompleteMetadata : status;
           } else {
             error("[metadata check] ERROR: %s '%s' has a valid dual equation %s "
                   "but doesn't have the subtype ViFunction. This is inconsistent.\n", 
                   varmetatype_name(var_md.type), ctr_printvarname(ctr, i),
                   ctr_printequname(ctr, ei_dual));
             status = status == OK ? Error_ModelIncompleteMetadata : status;
           }
         }
         break;
      }
      case VarDual:
      {
         if (var_md.ppty & VarIsDeleted) {
           error("[metadata check] ERROR: %s '%s' has a deleted subtype, but "
                 "is still seen as active by the model.\n",
                 varmetatype_name(var_md.type), ctr_printvarname(ctr, i));
           status = status == OK ? Error_ModelIncompleteMetadata : status;
         }

         rhp_idx ei_dual = var_md.dual;

         if (!valid_ei(ei_dual) || ei_dual >= total_m) {
             error("[metadata check] ERROR: %s '%s' has no dual equation, "
                   "this is inconsistent.\n", 
                   varmetatype_name(var_md.type), ctr_printvarname(ctr, i));
             status = status == OK ? Error_ModelIncompleteMetadata : status;
         }
         break;
      }
      case VarDefiningMap:
         /* Doing nothing */
         break;
      default:
         error("[metadata check] ERROR: Invalid type %d for variable '%s'\n",
               var_md.type, ctr_printvarname(ctr, i));
         status = status == OK ? Error_ModelIncompleteMetadata : status;
      }



   }

   for (rhp_idx i = 0; i < total_m; ++i) {
      if (fops && !fops->keep_equ(fops->data, i)) continue;
         /* Do not error for equations not in the model */
      if (fops_active && !fops_active->keep_equ(fops_active->data, i)) continue;

      const struct equ_meta *equ_md = &ctr->equmeta[i];

      if (!valid_mpid(equ_md->mp_id)) {
         error("[empdag] ERROR: equation '%s' is not attached to any MP\n",
               ctr_printequname(ctr, i));
         status = status == OK ? Error_ModelIncompleteMetadata : status;
         continue;
      }

      switch (equ_md->role) {
      case EquUndefined:
         error("[checkmetadata] ERROR: Equation '%s' has an undefined type\n",
               ctr_printequname(ctr, i));
         status = status == OK ? Error_ModelIncompleteMetadata : status;
         break;
      case EquConstraint:
      {
         /* if there is a dual variable, it must be of type Varmeta_Dual*/
         rhp_idx vi_dual = equ_md->dual;
         if (valid_vi(vi_dual)) {
           if (total_n < vi_dual) {
             error("[metadata check] ERROR: %s '%s' has invalid dual variable index %zu.\n",
                    equrole_name(equ_md->role), ctr_printequname(ctr, i), (size_t)vi_dual);
             status = status == OK ? Error_ModelIncompleteMetadata : status;
           } else if (ctr->varmeta[vi_dual].type != VarDual) {
             error("[metadata check] ERROR: %s '%s' has dual variable '%s' of "
                   "type %s, it should be %s.\n",  equrole_name(equ_md->role),
                   ctr_printequname(ctr, i), ctr_printvarname(ctr, vi_dual),
                   varmetatype_name(ctr->varmeta[vi_dual].type),
                   varmetatype_name(VarDual));
             error("Hints: it could also be that the equation should be of type %s.\n",
                   equrole_name(EquViFunction));
           status = status == OK ? Error_ModelIncompleteMetadata : status;
         }
         }
         break;
      }
      case EquViFunction:
      {
         rhp_idx vi_dual = equ_md->dual;
         if (!valid_vi(vi_dual) || (total_n < vi_dual)) {
             error("[metadata check] ERROR: %s '%s' has invalid dual variable index ",
                   equrole_name(equ_md->role), ctr_printequname(ctr, i));
             if (vi_dual >= IdxMaxValid) {
                error("%s\n", badidx_str(vi_dual));
             } else {
                error("%zu.\n", (size_t)vi_dual);
             }
             status = status == OK ? Error_ModelIncompleteMetadata : status;

         } else if (ctr->varmeta[vi_dual].type != VarPrimal) {
           error("[metadata check] ERROR: %s '%s' has dual variable '%s' of "
                 "type %s, it should be %s.\n", equrole_name(equ_md->role),
                 ctr_printequname(ctr, i), ctr_printvarname(ctr, vi_dual),
                 varmetatype_name(ctr->varmeta[vi_dual].type),
                 varmetatype_name(VarPrimal));
           status = status == OK ? Error_ModelIncompleteMetadata : status;
         }

         break;
      }
              case EquObjective:
      {
         /* If we have an objective, there must be no dual variable */
        rhp_idx vi_dual = equ_md->dual;
        if (valid_vi(vi_dual) && (total_n >= vi_dual)) {
          error("[metadata check] ERROR: %s '%s' has a dual variable, this is "
                "inconsistent!\n", equrole_name(equ_md->role),
                ctr_printequname(ctr, i));
          status = status == OK ? Error_ModelIncompleteMetadata : status;
        }

        /* An objective equation has no subtype */
        if (equ_md->ppty != EquUndefined) {
          error("[metadata check] ERROR: %s '%s' has a defined subtype, this is" 
                " inconsistent!\n", equrole_name(equ_md->role),
                ctr_printequname(ctr, i));
          status = status == OK ? Error_ModelIncompleteMetadata : status;
        }
        break;
      }
      case EquIsMap:
         /* Do nothing for now */
         break;
      default:
         error("[metadata check] ERROR: invalid equation metadata %d.\n",  equ_md->role);
         status = status == OK ? Error_ModelIncompleteMetadata : status;
      }



   }

   if (status == OK) {
      mdl_setmetachecked(mdl);
   }

   if (fops_active) { fops_active->freedata(fops_active->data); }
   return status;
}

int mdl_finalize(Model *mdl)
{
   S_CHECK(empdag_fini(&mdl->empinfo.empdag))

   ProbType probtype;
   S_CHECK(mdl_getprobtype(mdl, &probtype));

   if (probtype == MdlProbType_none) {
      if (mdl_is_rhp(mdl)) {
         S_CHECK(rmdl_analyze_modeltype(mdl, NULL));
      } else {
         error("[process] ERROR: %s model '%.*s' #%u has no type set\n",
               mdl_fmtargs(mdl));
         return Error_InvalidModel;
      }
   }

   mdl_setfinalized(mdl);

   return OK;
}

int mdl_checkobjequvar(const Model *mdl, rhp_idx objvar, rhp_idx objequ)
{
   return mdl->ops->checkobjequvar(mdl, objvar, objequ);
}

/**
 * @brief Copy relevant statistics and numbers from the solver to the user model
 *
 * @param mdl         the user model
 * @param mdl_solver  the solver model
 *
 * @return            the error code
 */
int mdl_copystatsfromsolver(Model *mdl, const Model *mdl_solver)
{
   return mdl->ops->copystatsfromsolver(mdl, mdl_solver);
}

int mdl_exportmodel(Model *mdl, Model *mdl_dst)
{
   /* TODO: this is part of GITLAB #67 */
   mdl_dst->mdl_up = mdl_borrow(mdl);
   mdl_dst->ctr.ctr_up = &mdl->ctr;
   mdl_timings_rel(mdl_dst->timings);
   mdl_dst->timings = mdl_timings_borrow(mdl->timings);

   return mdl->ops->exportmodel(mdl, mdl_dst);
}

int mdl_reportvalues(Model *mdl, Model *mdl_src)
{
   return mdl->ops->reportvalues(mdl, mdl_src);
}

int mdl_getmodelstat(const Model *mdl, int *modelstat)
{
   return mdl->ops->getmodelstat(mdl, modelstat);
}

int mdl_getprobtype(const Model *mdl, ProbType *probtype)
{
   return mdl->ops->getprobtype(mdl, probtype);
}

/**
 * @brief Return the objective row index.
 *
 * @ingroup publicAPI
 *
 * Note that in GAMS the objective row is the last row where the objective
 * variable appears. It may not have to do with the objective equation where
 * the objective variable is defined.
 *
 * @param mdl     the container object
 * @param objequ  the objective row index
 *
 * @return status OK if objective row is found
 *                Error otherwise
 */
int mdl_getobjequ(const Model *mdl, rhp_idx *objequ)
{
   return mdl->ops->getobjequ(mdl, objequ);
}

int mdl_getobjjacval(const Model *mdl, double *objjacval)
{
   return mdl->ops->getobjjacval(mdl, objjacval);
}

int mdl_getsense(const Model *mdl, RhpSense *objsense)
{
   return mdl->ops->getsense(mdl, objsense);
}

int mdl_getobjvar(const Model *mdl, rhp_idx *objvar)
{
   return mdl->ops->getobjvar(mdl, objvar);
}

int mdl_getoption(const Model *mdl, const char *option, void *val)
{
   return mdl->ops->getoption(mdl, option, val);
}
/**
 * @brief Get the solver name
 *
 * @param      mdl         the container
 * @param[out] solvername  the pointer to the string
 *
 * @return                 the name of the solver
 */
int mdl_getsolvername(const Model *mdl, char const ** solvername)
{
   return mdl->ops->getsolvername(mdl, solvername);
}

int mdl_getsolvestat(const Model *mdl, int *solvestat)
{
   return mdl->ops->getsolvestat(mdl, solvestat);
}

int mdl_postprocess(Model *mdl)
{
   return mdl->ops->postprocess(mdl);
}

int mdl_setmodelstat(Model *mdl, int modelstat)
{
   return mdl->ops->setmodelstat(mdl, modelstat);
}

/**
 * @brief Set the model type
 *
 * @ingroup publicAPI
 *
 * @param mdl        the container
 * @param probtype   the model type
 *
 * @return           the error code
 */
int mdl_setprobtype(Model *mdl, ProbType probtype)
{
   if (probtype >= probtypeslen) {
      error("%s :: unknown model type %d\n", __func__, probtype);
      return Error_InvalidValue;
   }
   
   S_CHECK(mdl->ops->setprobtype(mdl, probtype));

   Container *ctr = &mdl->ctr;
   if (probtype_hasmetadata(probtype)) {
      rhp_idx max_n = ctr_nvars_max(ctr);
      if (!ctr->varmeta) {
         MALLOC_(ctr->varmeta, struct var_meta, max_n);
         for (size_t i = 0; i < (size_t)max_n; ++i) {
            varmeta_init(&ctr->varmeta[i]);
         }
      }

      if (!ctr->equmeta) {
         rhp_idx max_m = ctr_nequs_max(ctr);
         MALLOC_(ctr->equmeta, struct equ_meta, max_m);
         for (size_t i = 0; i < (size_t)max_m; ++i) {
            equmeta_init(&ctr->equmeta[i]);
         }
      }
   }

   return OK;
}

int mdl_setsense(Model *mdl, unsigned objsense)
{
   return mdl->ops->setsense(mdl, objsense);
}

int mdl_setobjvar(Model *mdl, rhp_idx vi)
{
   return mdl->ops->setobjvar(mdl, vi);
}

int mdl_setsolvestat(Model *mdl, int solvestat)
{
   return mdl->ops->setsolvestat(mdl, solvestat);
}

/**
 * @brief Set the solver name
 *
 * @param mdl         the container
 * @param solvername  the name of the solver
 *
 * @return            the error code
 */
int mdl_setsolvername(Model *mdl, const char *solvername)
{
   return mdl->ops->setsolvername(mdl, solvername);
}

/**
 * @brief Call the solver for the model
 *
 * @param mdl  model to be solve
 *
 * @return     the error code
 */
int mdl_solve(Model *mdl)
{
   int status = mdl->ops->solve(mdl);

   trace_process("[process] %s model '%.*s' #%u solved with solve status %s and"
                 " model status %s)\n", mdl_fmtargs(mdl),
                 mdl_getsolvestatastxt(mdl), mdl_getmodelstatastxt(mdl));
   return status;
}


