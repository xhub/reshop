#include "checks.h"
#include "container.h"
#include "equ.h"
#include "equvar_helpers.h"
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


int chk_aequ(const Aequ *e, const char *fn)
{
   if (!e) {
      error("%s ERROR: the given equation object is NULL!\n", fn);
      return Error_NullPointer;
   }

   if (e->type == EquVar_Unset) {
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

   return OK;
}

int chk_ei(const Model *mdl, rhp_idx ei, const char *fn)
{
   return ei_inbounds(ei, ctr_nequs_total(&mdl->ctr), __func__);
}

int chk_vi(const Model *mdl, rhp_idx vi, const char *fn)
{
   return vi_inbounds(vi, ctr_nvars_total(&mdl->ctr), __func__);
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

   return OK;
}

int chk_mpe(const Mpe *mpe, const char *fn)
{
   if (!mpe) {
      error("%s ERROR: the Equilibrium object is NULL!\n", fn);
      return Error_NullPointer;
   }

   return OK;
}
