#include "checks.h"
#include "container.h"
#include "equ.h"
#include "equvar_helpers.h"
#include "mathprgm.h"
#include "mdl.h"
#include "printout.h"
#include "var.h"


int chk_arg_nonnull(const void *arg, unsigned nb, const char *fn)
{
   if (!arg) {
     error("%s ERROR: argument #%u is NULL\n", fn, nb);
     return Error_NullPointer;
   }

   return OK;
}

int chk_mdl(const Model *mdl, const char *fn)
{
   if (!mdl) {
      error("%s ERROR: the model object is NULL!\n", fn);
      return Error_NullPointer;
   }

   return OK;
}

int chk_gmdl(const Model *mdl, const char *fn)
{
   if (!mdl) {
      error("%s :: the given model object is NULL!\n", fn);
      return Error_NullPointer;
   }

   if (mdl->backend != RHP_BACKEND_GAMS_GMO) {
      error("%s ERROR: %s model '%*s' #%u has wrong backend: expected %s\n",
            fn, mdl_fmtargs(mdl), backend_name(RHP_BACKEND_GAMS_GMO));
      return Error_InvalidValue;
   }

   return OK;
}

int chk_rmdl(const Model *mdl, const char *fn)
{
   if (!mdl) {
      error("%s :: the given model object is NULL!\n", fn);
      return Error_NullPointer;
   }

   if (!mdl_is_rhp(mdl)) {
      error("%s ERROR: %s model '%*s' #%u has wrong backend: expected %s, %s or %s\n",
            fn, mdl_fmtargs(mdl), backend_name(RHP_BACKEND_RHP),
            backend_name(RHP_BACKEND_JULIA), backend_name(RHP_BACKEND_AMPL));
      return Error_InvalidValue;
   }

   return OK;
}

int chk_rmdldag(const Model *mdl, const char *fn)
{

   S_CHECK(chk_rmdl(mdl, fn));

   if (mdl->empinfo.empdag.mps.len == 0) {
      error("%s ERROR: the %s model '%.*s' #%u has an empty EMPDAG\n", fn, mdl_fmtargs(mdl));
      return Error_InvalidValue;
   }

   return OK;
}

int chk_aequ(const Aequ *e, const char *fn)
{
   if (!e) {
      error("%s ERROR: the given equation object is NULL!\n", fn);
      return Error_NullPointer;
   }

   if (e->size > 0 && e->type == EquVar_Unset) {
      error("%s ERROR: the equation object doesn't have a type set!\n", fn);
      return Error_InvalidValue;
   }

   return OK;
}

int chk_aequ_nonnull(const Aequ *e, const char *fn)
{
   if (!e) {
      error("%s ERROR: the given equation object is NULL!\n", fn);
      return Error_NullPointer;
   }

   return OK;
}

int chk_avar(const Avar *v, const char *fn)
{
   if (!v) {
      error("%s ERROR: the given variable object is NULL!\n", fn);
      return Error_NullPointer;
   }

   if (v->type == EquVar_Unset) {
      error("%s ERROR: the variable object doesn't have a type set!\n", fn);
      return Error_InvalidValue;
   }

   if (v->type > EquVar_Unset) {
      error("%s ERROR: the variable object has an invalid type\n", fn);
      return Error_InvalidValue;
   }

   return OK;
}

int chk_ei(const Model *mdl, rhp_idx ei, const char *fn)
{
   return ei_inbounds(ei, ctr_nequs_total(&mdl->ctr), fn);
}

int chk_vi(const Model *mdl, rhp_idx vi, const char *fn)
{
   return vi_inbounds(vi, ctr_nvars_total(&mdl->ctr), fn);
}

int chk_var_isnotconic(const Var *v, const Container *ctr,
                       const char *fn)
{
   if (v->is_conic) {
      error("%s ERROR: variable '%s' is conic!\n", fn, ctr_printvarname(ctr, v->idx));
      return Error_InvalidArgument;
   }

   return OK;
}

int chk_mp(const MathPrgm *mp, const char *fn)
{
   if (!mp) {
      error("%s ERROR: the mathprgm object is NULL!\n", fn);
      return Error_NullPointer;
   }

   return mp_isvalid(mp) ? OK : Error_RuntimeError;
}

int chk_nash(const Nash *nash, const char *fn)
{
   if (!nash) {
      error("%s ERROR: the Nash node is NULL!\n", fn);
      return Error_NullPointer;
   }

   return OK;
}

int chk_uint2int(unsigned v, const char *fn)
{
   if (v > INT_MAX) {
      error("%s ERROR: unsigned int value %u greater than %d\n", fn, v, INT_MAX);
      return Error_SizeTooSmall;
   }

   return OK;
}
