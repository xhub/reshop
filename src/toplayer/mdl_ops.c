#include "ctrdat_rhp.h"
#include "equvar_helpers.h"
#include "filter_ops.h"
#include "mdl.h"
#include "mdl_data.h"
#include "mdl_ops.h"
#include "mdl_priv.h"
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
   ModelType mdltype = mdl_src->commondata.mdltype;

   assert(mdltype != MdlType_none);
   if (mdltype >= mdltypeslen) {
      error("%s ERROR: unknown model type %d\n", __func__, mdltype);
      return Error_InvalidValue;
   }
   
   trace_model("[model] %s model '%.*s' #%u: setting model type to %s "
               "from %s model '%.*s' #%u\n", mdl_fmtargs(mdl),
               mdltype_name(mdltype), mdl_fmtargs(mdl_src));

   return mdl_settype(mdl, mdltype);
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
   bool has_optobj = valid_vi(objvar) || valid_ei(objequ);

   ModelType probtype;
   S_CHECK(mdl_gettype(mdl, &probtype));

   switch (probtype) {
   case MdlType_none:
      error("[model check] ERROR: %s model '%.*s' #%u has no type set", mdl_fmtargs(mdl));
      return Error_InvalidModel;
   case MdlType_lp:
   case MdlType_nlp:
   case MdlType_dnlp:
   case MdlType_mip:
   case MdlType_minlp:
   case MdlType_miqcp:
   case MdlType_qcp:
   case MdlType_mpec:
      if (!has_optobj) {
         error("[model check] ERROR: %s model '%.*s' #%u of type %s has neither"
               " an objective variable or an objective function.\n", mdl_fmtargs(mdl),
               mdltype_name(probtype));
         return Error_InvalidModel;
      }
      break;
   case MdlType_emp:
      if (mdl->empinfo.empdag.mps.len == 0) {
         if (!valid_optsense(sense)) {
            int offset;
            error("[model check] ERROR: %n%s model '%.*s' #%u of type %s has no "
                  "EMPinfo structure and is not an optimization problem.\n",
                  &offset, mdl_fmtargs(mdl), mdltype_name(probtype));
            error("%*sSpecify the EMPinfo structure.\n", offset, "");
            return Error_InvalidModel;

         }
         if (!has_optobj) {
            error("[model check] ERROR: %s model '%.*s' #%u of type %s has neither"
                  " an objective variable or an objective function.\n", mdl_fmtargs(mdl),
                  mdltype_name(probtype));
            return Error_InvalidModel;
         }
      }
      /* TODO where to check the compatibility between empdag and objective stuff */
      break;

   case MdlType_mcp:
   case MdlType_vi:
   case MdlType_cns:
      if (has_optobj) {
         error("[model check] ERROR: %s model '%.*s' #%u of type %s has either an"
               " objective variable or an objective function.\n", mdl_fmtargs(mdl),
               mdltype_name(probtype));
         return Error_InvalidModel;
      }
      break;
   default:
      error("[model check] ERROR: unknown model type %s (#%d)",
            mdltype_name(probtype), probtype);
      return Error_RuntimeError;
   }

   int status = mdl->ops->checkmdl(mdl);
   if (status == OK) {
      mdl_setchecked(mdl);
   }

   return status;
}

/**
 * @brief Check if the given model is solvable
 *
 * - For now, check that the empdag, if present, has a root
 *
 * @param mdl  the model to check
 *
 * @return     the error code
 */
int mdl_solvable(Model *mdl)
{
   ModelType probtype;
   S_CHECK(mdl_gettype(mdl, &probtype));

   if (probtype != MdlType_emp) {
      return OK;
   }

   const EmpDag *empdag = &mdl->empinfo.empdag;
   if (empdag->mps.len == 0 || valid_uid(empdag->uid_root)) {
      return OK;
   }

   error("[empdag] ERROR in %s model '%.*s' #%u: No valid root of the EMPDAG\n",
         mdl_fmtargs(mdl));

   return Error_EMPIncorrectInput;
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
         status = status == OK ? Error_IncompleteModelMetadata : status;
      }
   }

   EquMeta *emeta = mdl->ctr.equmeta;
   for (rhp_idx ei = 0; ei < nvars; ++ei) {
      rhp_idx vi = emeta[ei].dual;
      if (!valid_vi_(vi, nvars, "[mcp check]")) {
         error("[mcp check] ERROR: equation '%s' has an invalid dual variable. "
               "See above for details\n", mdl_printequname(mdl, ei));
         status = status == OK ? Error_IncompleteModelMetadata : status;
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

   ModelType probtype;
   EmpInfo *empinfo = &mdl->empinfo;
   S_CHECK(mdl_gettype(mdl, &probtype));

   if (!mdltype_hasmetadata(probtype) || ((probtype == MdlType_emp) &&
      (empdag_isopt(&empinfo->empdag) || empdag_isempty(&empinfo->empdag)))) {
     return OK;
   }

   Container *ctr = &mdl->ctr;
   if (!ctr->varmeta || !ctr->equmeta) {
      errormsg("[metadata check]  ERROR: varmeta or equmeta is NULL\n");
      return Error_NullPointer;
   }

   /* TODO: GITLAB#70 */
   if (probtype == MdlType_mpec) { return OK; }

   if (probtype == MdlType_mcp) {
      return mdl_chk_mcpmetadata(mdl);
   }

   unsigned num_unattached_vars = 0, num_unattached_equs = 0;
   unsigned num_invalid_mps = 0;

   Fops *fops = ctr->fops, *fops_active, fops_active_dat;

   if (mdl_is_rhp(mdl)) {
      S_CHECK(fops_active_init(&fops_active_dat, ctr));
      fops_active = &fops_active_dat;
   } else if (mdl->backend == RHP_BACKEND_GAMS_GMO) {
      fops_active = NULL;
   } else {
      TO_IMPLEMENT("metadata check for new backend model");
   }

   MathPrgm **mps = empdag->mps.arr;
   unsigned num_mps = empdag->mps.len;

   size_t total_n = ctr_nvars_total(ctr);
   size_t total_m = ctr_nequs_total(ctr);

   int status = OK;

   for (rhp_idx i = 0; i < total_n; ++i) {
      /* If the variable is going to disappear, continue */
      if (fops && !fops->keep_var(fops->data, i)) continue;
      /* If the variable does not appear in the model, continue */
      if (fops_active && !fops_active->keep_var(fops_active->data, i)) continue;

      const VarMeta var_md = ctr->varmeta[i];

      mpid_t mpid = var_md.mp_id;
      if (!valid_mpid(mpid)) {
         error("[empdag] ERROR: variable '%s' is not attached to any MP\n",
               ctr_printvarname(ctr, i));
         num_unattached_vars++;
         status = status == OK ? Error_IncompleteModelMetadata : status;
         continue;
      }

      if (mpid_regularmp(mpid)) {
         if (mpid >= num_mps) {
            error("[empdag] ERROR: variable '%s' belongs to non-existing MP #%u, "
                  "the largest ID is %u\n", ctr_printvarname(ctr, i), mpid,
                  num_mps-1);
            num_invalid_mps++;
            status = status == OK ? Error_RuntimeError : status;
            continue;
         }

         if (!mps[mpid]) {
            error("[empdag] ERROR: variable '%s' belongs to deleted MP #%u\n",
                  ctr_printvarname(ctr, i), mpid);
            num_invalid_mps++;
            status = status == OK ? Error_RuntimeError : status;
            continue;
         }
      }

      switch (var_md.type) {
      case VarUndefined:
         error("[metadata check] Error: Variable '%s' has an undefined type\n", 
               ctr_printvarname(ctr, i));
         status = status == OK ? Error_IncompleteModelMetadata : status;
         break;
      case VarObjective:
      {
         rhp_idx ei_dual = var_md.dual;
         VarPptyType ppty = var_md.ppty;

         if (valid_ei(ei_dual) && ei_dual <= total_m) {
           error("[metadata check] ERROR: %s '%s' has a valid dual equation '%s'"
                 ", this is inconsistent.\n",  varrole_name(var_md.type),
                 ctr_printvarname(ctr, i), ctr_printequname(ctr, ei_dual));
           status = status == OK ? Error_IncompleteModelMetadata : status;
         }

         if (ppty & VarIsDeleted) {
           error("[metadata check] ERROR: %s '%s' has a deleted subtype, "
                 "but is still seen as active by the model.\n",
                 varrole_name(var_md.type), ctr_printvarname(ctr, i));
           status = status == OK ? Error_IncompleteModelMetadata : status;
         }

         /*  If we have an objective equation, then the variable should not have extra properties */
         if ((ppty & VarIsExplicitlyDefined) && (ppty &
             ~(VarIsObjMin | VarIsObjMax | VarIsExplicitlyDefined))) {
           error("[metadata check] ERROR: %s '%s' has an inconsistent subtype.\n",
                 varrole_name(var_md.type), ctr_printvarname(ctr, i));
           status = status == OK ? Error_IncompleteModelMetadata : status;
         }

         /* Check for inconsistent subtypes  */
         if ((ppty & VarIsObjMin) && (ppty & VarIsObjMax)) {
           error("[metadata check] ERROR: %s '%s' has both minimize and maximize subtypes.\n",
                 varrole_name(var_md.type), ctr_printvarname(ctr, i));
           status = status == OK ? Error_IncompleteModelMetadata : status;
         }
         break;
      }
      case VarPrimal:
      {
         if (var_md.ppty & VarIsDeleted) {
           error("[metadata check] ERROR: %s '%s' has a deleted subtype, but "
                 "is still seen as active by the model.\n",
                 varrole_name(var_md.type), ctr_printvarname(ctr, i));
           status = status == OK ? Error_IncompleteModelMetadata : status;
         }

         rhp_idx ei_dual = var_md.dual;

         unsigned vbasictype = vmd_basictype(var_md.ppty);

         if (valid_ei(ei_dual) && ei_dual <= total_m) {
           if (vbasictype == VarPerpToViFunction) { /* This is OK */
           } else if (vbasictype == VarPerpToZeroFunctionVi) {
              /* If we have a SubZeroFunction, no dual equ must be present */
              error("[metadata check] ERROR: %s '%s' is perpendicular to the zero "
                 "function, but has a valid dual equation '%s'.\n",
                 varrole_name(var_md.type), ctr_printvarname(ctr, i),
                 ctr_printequname(ctr, ei_dual));

              status = status == OK ? Error_IncompleteModelMetadata : status;
           } else {
             error("[metadata check] ERROR: %s '%s' has a valid dual equation %s "
                   "but doesn't have the subtype ViFunction. This is inconsistent.\n", 
                   varrole_name(var_md.type), ctr_printvarname(ctr, i),
                   ctr_printequname(ctr, ei_dual));
             status = status == OK ? Error_IncompleteModelMetadata : status;
           }
         }
         break;
      }
      case VarDual:
      {
         if (var_md.ppty & VarIsDeleted) {
           error("[metadata check] ERROR: %s '%s' has a deleted subtype, but "
                 "is still seen as active by the model.\n",
                 varrole_name(var_md.type), ctr_printvarname(ctr, i));
           status = status == OK ? Error_IncompleteModelMetadata : status;
         }

         rhp_idx ei_dual = var_md.dual;

         if (!valid_ei(ei_dual) || ei_dual >= total_m) {
             error("[metadata check] ERROR: %s '%s' has no dual equation, "
                   "this is inconsistent.\n", 
                   varrole_name(var_md.type), ctr_printvarname(ctr, i));
             status = status == OK ? Error_IncompleteModelMetadata : status;
         }
         break;
      }
      case VarDefiningMap:
         /* Doing nothing */
         break;
      default:
         error("[metadata check] ERROR: Invalid type %d for variable '%s'\n",
               var_md.type, ctr_printvarname(ctr, i));
         status = status == OK ? Error_IncompleteModelMetadata : status;
      }



   }

   for (rhp_idx i = 0; i < total_m; ++i) {
      if (fops && !fops->keep_equ(fops->data, i)) continue;
         /* Do not error for equations not in the model */
      if (fops_active && !fops_active->keep_equ(fops_active->data, i)) continue;

      const struct equ_meta *equ_md = &ctr->equmeta[i];

      mpid_t mpid = equ_md->mp_id;
      if (!valid_mpid(equ_md->mp_id)) {
         error("[empdag] ERROR: equation '%s' is not attached to any MP\n",
               ctr_printequname(ctr, i));
         status = status == OK ? Error_IncompleteModelMetadata : status;
         continue;
      }

      if (mpid_regularmp(mpid)) {
         if (mpid >= num_mps) {
            error("[empdag] ERROR: equation '%s' belongs to non-existing MP #%u, "
                  "the largest ID is %u\n", ctr_printequname(ctr, i), mpid,
                  num_mps-1);
            num_invalid_mps++;
            status = status == OK ? Error_RuntimeError : status;
         }

         if (!mps[mpid]) {
            error("[empdag] ERROR: equation '%s' belongs to deleted MP #%u\n",
                  ctr_printequname(ctr, i), mpid);
            num_invalid_mps++;
            status = status == OK ? Error_RuntimeError : status;
         }
      }

      switch (equ_md->role) {
      case EquUndefined:
         error("[checkmetadata] ERROR: Equation '%s' has an undefined type\n",
               ctr_printequname(ctr, i));
         status = status == OK ? Error_IncompleteModelMetadata : status;
         break;
      case EquConstraint:
      {
         /* if there is a dual variable, it must be of type Varmeta_Dual*/
         rhp_idx vi_dual = equ_md->dual;
         if (valid_vi(vi_dual)) {
           if (total_n < vi_dual) {
             error("[metadata check] ERROR: %s '%s' has invalid dual variable index %zu.\n",
                    equrole_name(equ_md->role), ctr_printequname(ctr, i), (size_t)vi_dual);
             status = status == OK ? Error_IncompleteModelMetadata : status;
           } else if (ctr->varmeta[vi_dual].type != VarDual) {
             error("[metadata check] ERROR: %s '%s' has dual variable '%s' of "
                   "type %s, it should be %s.\n",  equrole_name(equ_md->role),
                   ctr_printequname(ctr, i), ctr_printvarname(ctr, vi_dual),
                   varrole_name(ctr->varmeta[vi_dual].type),
                   varrole_name(VarDual));
             error("Hints: it could also be that the equation should be of type %s.\n",
                   equrole_name(EquViFunction));
           status = status == OK ? Error_IncompleteModelMetadata : status;
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
             status = status == OK ? Error_IncompleteModelMetadata : status;

         } else if (ctr->varmeta[vi_dual].type != VarPrimal) {
           error("[metadata check] ERROR: %s '%s' has dual variable '%s' of "
                 "type %s, it should be %s.\n", equrole_name(equ_md->role),
                 ctr_printequname(ctr, i), ctr_printvarname(ctr, vi_dual),
                 varrole_name(ctr->varmeta[vi_dual].type),
                 varrole_name(VarPrimal));
           status = status == OK ? Error_IncompleteModelMetadata : status;
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
          status = status == OK ? Error_IncompleteModelMetadata : status;
        }

        /* An objective equation has no subtype */
        if (equ_md->ppty != EquPptyNone) {
          error("[metadata check] ERROR: %s '%s' has a defined subtype, this is" 
                " inconsistent!\n", equrole_name(equ_md->role),
                ctr_printequname(ctr, i));
          status = status == OK ? Error_IncompleteModelMetadata : status;
        }
        break;
      }
      case EquIsMap:
         /* Do nothing for now */
         break;
      default:
         error("[metadata check] ERROR: invalid equation metadata %d.\n",  equ_md->role);
         status = status == OK ? Error_IncompleteModelMetadata : status;
      }



   }

   if (status == OK) {
      mdl_setmetachecked(mdl);
   } else {
      if (num_unattached_vars > 0) {
         error("[metadata check] ERROR: %u unattached variables\n", num_unattached_vars);
      }
      if (num_unattached_equs > 0) {
         error("[metadata check] ERROR: %u unattached equations\n", num_unattached_equs);
      }
      if (num_invalid_mps > 0) {
         error("[metadata check] ERROR: %u invalid MPs\n", num_invalid_mps);
      }
   }

   if (fops_active) { fops_active->freedata(fops_active->data); }
   return status;
}

int mdl_finalize(Model *mdl)
{
   S_CHECK(empdag_fini(&mdl->empinfo.empdag))

   ModelType probtype;
   S_CHECK(mdl_gettype(mdl, &probtype));

   if (probtype == MdlType_none) {
      if (mdl_is_rhp(mdl)) {
         S_CHECK(mdl_analyze_modeltype(mdl, NULL));
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

/**
 * @brief Export a model to another instance
 *
 * This create a new model instance, with possibly a different backend, that
 * represents the same model instance, possibly filtered.
 *
 * @param mdl      The source model
 * @param mdl_dst  The destination model
 *
 * @return         The error code
 */
int mdl_export(Model *mdl, Model *mdl_dst)
{
   /* ----------------------------------------------------------------------
    * - Finalize the model
    * - Check source model
    * - Check metadata of source model
    * ---------------------------------------------------------------------- */

   S_CHECK(mdl_finalize(mdl));

   S_CHECK(mdl_check(mdl));
   S_CHECK(mdl_checkmetadata(mdl));

   /* TODO: this is part of GITLAB #67 */
   mdl_linkmodels(mdl, mdl_dst);

   S_CHECK(mdl->ops->export(mdl, mdl_dst));

   /* TODO: hopefully we can remove this */
   ModelType probtype;
   mdl_gettype(mdl_dst, &probtype);

   assert(probtype != MdlType_none);
//   if (probtype == MdlProbType_none) {
//      S_CHECK(mdl_copyprobtype(mdl_solver, mdl));
//   }

   return OK;
}

/**
 * @brief Export a model into a solvable form
 *
 * This create a new model instance, with possibly a different backend, that
 * shares the same solution set as the source model. If the source model is
 * a classical optimization problem, then the destination one is another instance.
 * However, if this is not the case, we seek to derive a model which has the same
 * solution set. A bilevel/MPEC is transformed into an MPCC. A Nash problem into
 * an MCP. A VI into an MCP.
 *
 * @param mdl      The model
 * @param mdl_src  The source model
 *
 * @return         The error code
 */
int mdl_copyassolvable(Model *mdl, Model *mdl_src)
{
   /* ----------------------------------------------------------------------
    * - Finalize the model
    * - Check source model
    * - Check metadata of source model
    * ---------------------------------------------------------------------- */

   S_CHECK(mdl_finalize(mdl_src));

   S_CHECK(mdl_check(mdl_src));
   S_CHECK(mdl_checkmetadata(mdl_src));

   S_CHECK(mdl->ops->copyassolvable(mdl, mdl_src));

   /* TODO: hopefully we can remove this */
   ModelType probtype;
   mdl_gettype(mdl, &probtype);

   assert(probtype != MdlType_none);

   return OK;
}


int mdl_reportvalues(Model *mdl, Model *mdl_src)
{
   return mdl->ops->reportvalues(mdl, mdl_src);
}

int mdl_getmodelstat(const Model *mdl, int *modelstat)
{
   return mdl->ops->getmodelstat(mdl, modelstat);
}

int mdl_gettype(const Model *mdl, ModelType *type)
{
   /* TODO: this could just return the type */
   *type = mdl->commondata.mdltype;
   return OK;
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
 * @param mdl    the container
 * @param type   the model type
 *
 * @return       the error code
 */
int mdl_settype(Model *mdl, ModelType type)
{
   if (type >= mdltypeslen) {
      error("%s ERROR: unknown model type %d\n", __func__, type);
      return Error_InvalidValue;
   }

   mdl->commondata.mdltype = type;
   
   trace_model("[model] %s model '%.*s' #%u: setting model type to %s\n",
               mdl_fmtargs(mdl), mdltype_name(type));

   Container *ctr = &mdl->ctr;
   if (mdltype_hasmetadata(type)) {
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
                 " model status %s\n", mdl_fmtargs(mdl),
                 mdl_getsolvestatastxt(mdl), mdl_getmodelstatastxt(mdl));
   return status;
}


