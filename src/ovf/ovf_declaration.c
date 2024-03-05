#include "reshop_config.h"

#include <stdio.h>
#include <string.h>

#include "checks.h"
#include "container_helpers.h"
#include "compat.h"
#include "container.h"
#include "empinfo.h"
#include "equvar_helpers.h"
#include "macros.h"
#include "mathprgm.h"
#include "mdl.h"
#include "ovf_parameter.h"
#include "ovfinfo.h"
#include "printout.h"
#include "reshop.h"
#include "status.h"

static const char * ovf_synonyms_str(const char *name)
{
  size_t i = 0;
  while (ovf_synonyms[i][0]) {
     if (!strcasecmp(name, ovf_synonyms[i][0])) {
        return ovf_synonyms[i][1];
     }
     ++i;
  }

   return NULL;
}

static const char * ovf_synonyms_token(const char *start, unsigned len)
{
  size_t i = 0;
  while (ovf_synonyms[i][0]) {
     if (!strncasecmp(start, ovf_synonyms[i][0], len)) {
        return ovf_synonyms[i][1];
     }
     ++i;
  }

   return NULL;
}

static unsigned ovf_errormsgname(void)
{
   for (size_t i = 0; i < ovf_numbers; ++i) {
      error("%s\n", ovf_names[i]);
   }

   errormsg("List of synonyms (alternative names):\n");
   size_t i = 0;
   while (ovf_synonyms[i][0]) {
      error("%s == %s\n", ovf_synonyms[i][0], ovf_synonyms[i][1]);
      ++i;
   }

   return UINT_MAX;
}

unsigned ovf_findbyname(const char *name)
{
   for (unsigned i = 0; i < ovf_numbers; ++i) {
      if (!strcasecmp(name, ovf_names[i])) {
         return i;
      }
   }

   const char *oldname = name;
   name = ovf_synonyms_str(name);
   if (name) {
      for (unsigned i = 0; i < ovf_numbers; ++i) {
         if (!strcasecmp(name, ovf_names[i])) {
            return i;
         }
      }
   }

   error("Unknown OVF function '%s'. List of implemented functions:\n", oldname);

   return ovf_errormsgname();
}


unsigned ovf_findbytoken(const char *start, unsigned len)
{
   for (unsigned i = 0; i < ovf_numbers; ++i) {
      if (!strncasecmp(start, ovf_names[i], len)) {
         return i;
      }
   }

   const char* name = ovf_synonyms_token(start, len);

   if (name) {
      for (unsigned i = 0; i < ovf_numbers; ++i) {
         if (!strcasecmp(name, ovf_names[i])) {
            return i;
         }
      }
   }

   error("Unknown OVF function '%.*s'. List of implemented functions:\n", len, start);

   return ovf_errormsgname();
}

/**
 *  @brief Find the OVF by name and allocate its data structure
 *
 *  @param      empinfo  the empinfo structure
 *  @param      name     the name of the OVF to find
 *  @param[out] ovfdef   the allocated OVF definition
 *
 *  @return              the error code
 */
int ovf_addbyname(EmpInfo *empinfo, const char *name,
                  OvfDef **ovfdef)
{
   unsigned ovf_idx = ovf_findbyname(name);

   if (ovf_idx == UINT_MAX) return Error_RuntimeError;

   printout(PO_DEBUG, "[OVF] function '%s' detected\n", name);

   A_CHECK(*ovfdef, ovf_def_new_ovfinfo(empinfo->ovf, ovf_idx));

   A_CHECK((*ovfdef)->name, strdup(name));


   return OK;
}

int ovf_params_sync(OvfDef * restrict ovf, OvfParamList * restrict params)
{
   OvfParamList * restrict params_old = &ovf->params;

   if (params->size != params_old->size) {
      error("%s :: Number of parameters differs: old = %u; new = %u\n", __func__,
            params_old->size, params->size);
      return Error_RuntimeError;
   }

   for (unsigned i = 0, len = params->size; i < len; ++i) {
      OvfParam * restrict param_old = &params_old->p[i];
      OvfParam * restrict param_new = &params->p[i];

      param_old->type = param_new->type;
      param_old->size_vector = param_new->size_vector;

      switch (param_new->type) {
      case ARG_TYPE_UNSET: break;
      case ARG_TYPE_SCALAR:
         param_old->val = param_new->val;
         break;
      case ARG_TYPE_VEC:
         param_old->vec = param_new->vec;
         break;
      case ARG_TYPE_MAT:
         param_old->mat = param_new->mat;
         break;
      default:
         error("%s :: unsupported param type %d at index %u", __func__,
               param_new->type, i);
      }

      }

   return OK;
}

/*
 *  @brief Add an OVF variable 
 *
 *  This is mainly for declaring OVF variable via an API.
 *  The last argument returns a pointer to the OVF definitional
 *  structure, which can be used to set parameters
 *
 *  @ingroup publicAPI
 *
 *  @param       mdl       the model
 *  @param       name      name of the OVF function
 *  @param       ovf_vi    index of the OVF variable
 *  @param       v_args    arguments for the OVF
 *  @param[out]  ovf_def   OVF definition structure
 *
 *  @return                the return code
 */
int rhp_ovf_add(Model *mdl, const char* name, rhp_idx ovf_vi,
                Avar *v_args, OvfDef **ovf_def)
{
    S_CHECK(chk_mdl(mdl, __func__));

   Container *ctr = &mdl->ctr;
   EmpInfo *empinfo = &mdl->empinfo;

   if (!name) {
      error("%s ERROR: the OVF name argument is null!\n", __func__);
      return Error_NullPointer;
   }

   if (!v_args) {
      error("%s ERROR: the argument list is null!\n", __func__);
      return Error_NullPointer;
   }

   if (!ovf_def) {
      error("%s ERROR: the OVF destination pointer is null!\n", __func__);
      return Error_NullPointer;
   }

   if (v_args->size == 0) {
      error("%s ERROR: the number of arguments must be greater than 0!\n",
            __func__);
      return Error_InvalidValue;
   }

   size_t total_n = ctr_nvars_total(ctr);
   S_CHECK(vi_inbounds(ovf_vi, total_n, __func__));

   /* ---------------------------------------------------------------------
    * Create the OVF struct if necessary
    * --------------------------------------------------------------------- */

   if (!empinfo->ovf) { S_CHECK(ovfinfo_alloc(empinfo)); }

   /* ---------------------------------------------------------------------
    * Find the OVF named ``name'' and allocate it
    * --------------------------------------------------------------------- */

   S_CHECK(ovf_addbyname(empinfo, name, ovf_def));

   /* ---------------------------------------------------------------------
    * Set the OVF variable and the arguments.
    *
    * Note that we only support variable argument.
    * --------------------------------------------------------------------- */

   /*  For convenience */
   OvfDef *ovfdef_ = *ovf_def;

   ovfdef_->ovf_vidx = ovf_vi;
   avar_extend(ovfdef_->args, v_args);

   for (size_t i = 0, len = avar_size(v_args); i < len; ++i) {

     rhp_idx vi = avar_fget(v_args, i);
     S_CHECK(vi_inbounds(vi, total_n, __func__));

     if (vi == ovf_vi) {
         error("[OVF] ERROR: argument %zu is the same as the OVF variable\n", i);
         return Error_EMPIncorrectInput;
     }
   }

   for (size_t i = 0; i < ovfdef_->params.size; ++i) {
     OvfParam *p = &ovfdef_->params.p[i];
     if (p->def->get_vector_size) {
       p->size_vector = p->def->get_vector_size(v_args->size);
     } else {
       p->size_vector = 0;
     }
   }

   return OK;
}

/**
 *  @brief Check that an OVF is well defined
 *
 *  @param ovf_def  the OVF definition to check
 *
 *  @return         OK if the OVF definition is correct, otherwise the error
 *                  code
 */
int ovf_check(OvfDef *ovf_def)
{
   if (ovf_def->status & OvfChecked) { return OK; }

   unsigned nargs = ovf_argsize(ovf_def);

   if (nargs == 0) {
      error("[OVF] ERROR: OVF '%s' #%u with has no argument, this is not supported\n",
            ovf_getname(ovf_def), ovf_def->idx);;
      return Error_RuntimeError;
   }

   /* -----------------------------------------------------------------------
    * Check if all the mandatory parameters are defined
    * ----------------------------------------------------------------------- */

   for (size_t i = 0; i < ovf_def->params.size; ++i) {
      OvfParam* p = &ovf_def->params.p[i];
      if (p->type == ARG_TYPE_UNSET) {
         if (p->def->mandatory && !p->def->default_val) {
            error("[ovf/check] in the definition of OVF variable #%d of type %s"
                  ", the required parameter %s is unset\n", ovf_def->ovf_vidx,
                  ovf_getname(ovf_def), p->def->name);
         return Error_EMPIncorrectInput;
         }

         S_CHECK(p->def->default_val(p, nargs));
      }
   }

   ovf_def->status |= OvfChecked;

   return OK;
}


/** @brief  find the equations where placeholder variables appear
 *
 *  If an argument appears only once, in a linear fashion, then we can use
 *  the expression in the reformulations.
 *
 *  Call ovf_remove_mappings() to remove the mappings from the model
 *
 *  @param       ctr      the container
 *  @param       args     the given variable arguments
 *  @param[out]  eidx     the given equation arguments
 *  @param[out]  coeffs   coefficients of each placeholder variables in indices
 *
 *  @return               the error code
 */
static int preprocess_ovfargs(Model *mdl, OvfDef *ovf_def, mpid_t mpid)
{

   int status = OK;

   /* ----------------------------------------------------------------------
    * Here we look for the OVF arguments. For each we find the equations where
    * the OVF argument variable appears
    * ---------------------------------------------------------------------- */

   const Avar *args = ovf_def->args;
   unsigned nargs = avar_size(args);

   EquMeta *emd = mdl->ctr.equmeta;
   VarMeta *vmd = mdl->ctr.varmeta;

   if (ovf_def->eis) { 
      rhp_idx *eis = ovf_def->eis;

      if (!vmd | !emd) {
         errormsg("[OVF] ERROR: runtime error. Please file a bug report\n");
         return Error_RuntimeError;
      }

      for (unsigned i = 0; i < nargs; ++i) {
         rhp_idx vi = avar_fget(args, i);
         rhp_idx ei = eis[i];

         if (!valid_ei(ei)) { continue; }

         mpid_t vmpid =  vmd[vi].mp_id;
         if (valid_mpid(vmpid) && vmpid != mpid) {
            error("[OVF] ERROR in OVF %s: variable '%s' is already affected to "
                  "the MP(%s), it should not\n", ovf_getname(ovf_def),
                  mdl_printvarname(mdl, vi), mpid_getname(mdl, vmpid));
            status = Error_EMPIncorrectInput;
            continue;
         }

         mpid_t empid =  emd[ei].mp_id;
         if (valid_mpid(empid) && empid != mpid) {
            error("[OVF] ERROR in OVF %s: equation '%s' is already affected to "
                  "the MP(%s), it should not\n", ovf_getname(ovf_def),
                  mdl_printvarname(mdl, ei), mpid_getname(mdl, empid));
            status = Error_EMPIncorrectInput;
            continue;
         }

         emd[ei].mp_id = mpid;
         emd[ei].role  = EquIsMap;
         vmd[vi].mp_id = mpid;
         vmd[vi].type  = VarDefiningMap;
      }

      return OK;
   }

   MALLOC_(ovf_def->eis, rhp_idx, nargs);
   MALLOC_(ovf_def->coeffs, double, nargs);

   rhp_idx *eidx = ovf_def->eis;
   double *coeffs = ovf_def->coeffs;

   for (unsigned i = 0; i < nargs; ++i) {
      rhp_idx vi = avar_fget(args, i);
      assert(vi < ctr_nvars_total(&mdl->ctr));

      if (vmd) {
         mpid_t vmpid =  vmd[vi].mp_id;
         if (valid_mpid(vmpid)) {
            error("[OVF] ERROR in OVF %s: variable '%s' is already affected to "
                  "the MP(%s), it should not\n", ovf_getname(ovf_def),
                  mdl_printvarname(mdl, vi), mpid_getname(mdl, vmpid));
            status = Error_EMPIncorrectInput;
            continue;
         }
      }

      double jacval;
      int nlflag;
      void *iter = NULL;
      rhp_idx ei;
      S_CHECK(ctr_var_iterequs(&mdl->ctr, vi, &iter, &jacval, &ei, &nlflag));

      if (!iter && !nlflag && (ctr_equ_type(&mdl->ctr, ei) == ConeInclusion)
         && (ctr_equ_cone(&mdl->ctr, ei) == CONE_0)) {
         /* The variable appeared only in an equality equation.Therefore, we
             * can remove the equation and variable and deduce the expression */
         eidx[i] = ei;
         coeffs[i] = jacval;
         assert(isfinite(jacval));

         if (emd) {
            assert(vmd); /* Just to silence a warning */

            mpid_t empid =  emd[ei].mp_id;
            if (valid_mpid(empid)) {
               error("[OVF] ERROR in OVF %s: equation '%s' is already affected to "
                     "the MP(%s), it should not\n", ovf_getname(ovf_def),
                     mdl_printequname(mdl, ei), mpid_getname(mdl, empid));
               status = Error_EMPIncorrectInput;
               continue;
            }
            emd[ei].mp_id = mpid;
            emd[ei].role = EquIsMap;

            vmd[vi].mp_id = mpid;
            vmd[vi].type = VarDefiningMap;
         }

#ifdef DEBUG_OVF
         Lequ *le = ctr->equs[ei].lequ;
         bool once = false;
         for (unsigned j = 0; j < le->len; ++j) {
            if (vi == le->index[j]) {
               assert(jacval == le->value[j]);
               printout(PO_DEBUG, "%s :: jacval = %e; value = %e\n", __func__, jacval, le->value[j]);
               once = true;
            }
         }
         if (!once) {
            assert(0);
         }
#endif


      } else if (nlflag) {
         /* \TODO(xhub) see if we can reuse some of the implicit logic */
         error("[OVF] ERROR: the variable '%s' appears in a non-linear fashion "
               "in the equation '%s'. This is currently not supported.\n",
                  ctr_printvarname(&mdl->ctr, vi), ctr_printequname(&mdl->ctr, ei));
         status = Error_NotImplemented;

      } else { /* TODO(xhub) is this well tested?  */
         eidx[i] = IdxNA;
         coeffs[i] = SNAN;
      }
   }

   return status;
}

int ovf_finalize(Model *mdl, OvfDef *ovf_def)
{
   S_CHECK(ovf_check(ovf_def));

   if (ovf_def->status & OvfFinalized) { return OK; }

   S_CHECK(preprocess_ovfargs(mdl, ovf_def, MpId_OvfDataMask | ovf_def->idx));

   ovf_def->status |= OvfFinalized;

   return OK;
}

/* try to find a better name for this */
int ovf_forcefinalize(Model *mdl, OvfDef *ovf_def)
{
   S_CHECK(ovf_check(ovf_def));

   S_CHECK(preprocess_ovfargs(mdl, ovf_def, MpId_OvfDataMask | ovf_def->idx));

   ovf_def->status |= OvfFinalized;

   return OK;
}

static int preprocess_ccflibargs(Model *mdl, OvfDef *ovf_def, MathPrgm *mp)
{

   int status = OK;

   /* ----------------------------------------------------------------------
    * Here we look for the OVF arguments. For each we find the equations where
    * the OVF argument variable appears
    * ---------------------------------------------------------------------- */

   const Avar *args = ovf_def->args;
   unsigned nargs = avar_size(args);

   EquMeta *emd = mdl->ctr.equmeta;
   VarMeta *vmd = mdl->ctr.varmeta;

   assert(!ovf_def->eis);

   MALLOC_(ovf_def->eis, rhp_idx, nargs);
   MALLOC_(ovf_def->coeffs, double, nargs);

   rhp_idx *eidx = ovf_def->eis;
   double *coeffs = ovf_def->coeffs;

   for (unsigned i = 0; i < nargs; ++i) {
      rhp_idx vi = avar_fget(args, i);
      assert(vi < ctr_nvars_total(&mdl->ctr));

      if (vmd) {
         mpid_t vmpid =  vmd[vi].mp_id;
         if (valid_mpid(vmpid)) {
            error("[OVF] ERROR in OVF %s: variable '%s' is already affected to "
                  "the MP(%s), it should not\n", ovf_getname(ovf_def),
                  mdl_printvarname(mdl, vi), mpid_getname(mdl, vmpid));
            status = Error_EMPIncorrectInput;
            continue;
         }
      }

      double jacval;
      int nlflag;
      void *iter = NULL;
      rhp_idx ei;
      S_CHECK(ctr_var_iterequs(&mdl->ctr, vi, &iter, &jacval, &ei, &nlflag));

      if (!iter && !nlflag && (ctr_equ_type(&mdl->ctr, ei) == ConeInclusion)
         && (ctr_equ_cone(&mdl->ctr, ei) == CONE_0)) {
         /* The variable appeared only in an equality equation.Therefore, we
             * can remove the equation and variable and deduce the expression */
         eidx[i] = ei;
         coeffs[i] = jacval;
         assert(isfinite(jacval));

         mpid_t empid =  emd[ei].mp_id;
         if (valid_mpid(empid)) {
            error("[OVF] ERROR in OVF %s: equation '%s' is already affected to "
                  "the MP(%s), it should not\n", ovf_getname(ovf_def),
                  mdl_printequname(mdl, ei), mpid_getname(mdl, empid));
            status = Error_EMPIncorrectInput;
            continue;
         }

         emd[ei].role = EquIsMap;
         S_CHECK(mp_addequ(mp, ei));

         S_CHECK(mp_addvar(mp, vi));
         vmd[vi].type = VarDefiningMap;

      } else if (nlflag) {
         /* \TODO(xhub) see if we can reuse some of the implicit logic */
         error("[MP/CCFLIB] ERROR: the variable '%s' appears in a non-linear fashion "
               "in the equation '%s'. This is currently not supported.\n",
                  ctr_printvarname(&mdl->ctr, vi), ctr_printequname(&mdl->ctr, ei));
         status = Error_NotImplemented;

      } else if (iter) { /* TODO(xhub) TEST  */

         error("[MP/CCFLIB] ERROR in MP(%s): the defining variable '%s' appears in more "
               "than one equation. This is not allowed in an MP defined by a "
               "CCF.\nThe variables appears in:\n", mp_getname(mp),
               mdl_printvarname(mdl, vi));

         do {
            error("\t%s\n", mdl_printequname(mdl, ei));
            S_CHECK(ctr_var_iterequs(&mdl->ctr, vi, &iter, &jacval, &ei, &nlflag));
         }
         while (iter);

         status = Error_EMPIncorrectInput;
      } else {
         error("[MP/CCFLIB] ERROR in MP(%s): the defining variable '%s' appears in "
               "%s, but the later is not an equality constraint, rather has type "
               "%s and cone %s\n",
               mp_getname(mp), mdl_printvarname(mdl, vi), mdl_printequname(mdl, ei),
               equtype_name(ctr_equ_type(&mdl->ctr, ei)), cone_name(ctr_equ_cone(&mdl->ctr, ei)));
         status = Error_EMPIncorrectInput;
      }
   }

   return status;
}

int ccflib_finalize(Model *mdl, OvfDef *ovf_def, MathPrgm* mp)
{
   S_CHECK(ovf_check(ovf_def));

   if (ovf_def->status & OvfFinalized) { return OK; }

   S_CHECK(preprocess_ccflibargs(mdl, ovf_def, mp));

   ovf_def->status |= OvfFinalized;

   return OK;
}


