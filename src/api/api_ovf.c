#include "checks.h"
#include "macros.h"
#include "ovf_options.h"
#include "ovf_parameter.h"
#include "ovfinfo.h"
#include "printout.h"
#include "reshop.h"

/**
 *  @brief Find the parameter structure corresponding to the name
 *
 *  @param       ovf_def     the OVF definition
 *  @param       param_name  the parameter name
 *  @param[out]  param       the parameter structure
 *
 *  */
static int _get_param(OvfDef *ovf_def, const char *param_name,
                      struct ovf_param **param)
{
   /* ---------------------------------------------------------------------
    * 1. Get the ovf function based on ``ovf_idx''
    * 2. Find the parameter definition corresponding to ``param_name''
    * --------------------------------------------------------------------- */


   struct ovf_param *p = NULL;
   for (unsigned i = 0; i < ovf_def->params.size; ++i) {
      if (!strcmp(ovf_def->params.p[i].def->name, param_name)) {
         p = &ovf_def->params.p[i];
      }
   }
   if (!p) {
      error("%s :: Could not find parameter named %s for OVF "
                         "function %s. Possible options are:", __func__,
                         param_name, ovf_getname(ovf_def));
      for (unsigned i = 0; i < ovf_def->params.size; ++i) {
         error(" %s", ovf_def->params.p[i].def->name);
      }
       errormsg("\n");
      return Error_InvalidValue;
   }

   *param = p;

   return OK;
}

/**
 *  @brief Add a scalar parameter value to an OVF definition
 *
 *  @ingroup publicAPI 
 *
 *  @param ovf_def     the definition of the OVF
 *  @param param_name  the parameter name
 *  @param val         the value of the parameter
 *
 *  @return            the error code
 */
int rhp_ovf_param_add_scalar(OvfDef* ovf_def, const char *param_name,
                             double val)
{
   /* ---------------------------------------------------------------------
    * 1. Get the ovf function based on ``ovf_idx''
    * 2. Find the parameter definition corresponding to ``param_name''
    * 3. Create and add the parameter
    * --------------------------------------------------------------------- */

  S_CHECK(chk_arg_nonnull(ovf_def, 1, __func__));
  S_CHECK(chk_arg_nonnull(param_name, 2, __func__));

   struct ovf_param *p = NULL;
   S_CHECK(_get_param(ovf_def, param_name, &p));

   if (!p->def->allow_scalar) {
      error("[OVF] ERROR in OVF %s: parameter %s cannot be defined as a scalar\n",
            ovf_def->name, p->def->name);
      /*  TODO(xhub) print usage */
      return Error_Unconsistency;
   }

   p->type = ARG_TYPE_SCALAR;
   p->val = val;

   return OK;
}

/**
 *  @brief Add a vector parameter value to an OVF definition
 *
 *  @ingroup publicAPI
 *
 *  @param ovf_def     the definition of the OVF
 *  @param param_name  the parameter name
 *  @param n_vals      the number of values
 *  @param vals        the values of the parameter
 *
 *  @return            the error code
 */
int rhp_ovf_param_add_vector(OvfDef *ovf_def, const char *param_name,
                             unsigned n_vals, double *vals)
{
   /* ---------------------------------------------------------------------
    * 1. Find the parameter definition corresponding to ``param_name''
    * 2. Create and add the parameter
    * --------------------------------------------------------------------- */

  S_CHECK(chk_arg_nonnull(ovf_def, 1, __func__));
  S_CHECK(chk_arg_nonnull(param_name, 2, __func__));
  S_CHECK(chk_arg_nonnull(vals, 4, __func__));

   struct ovf_param *p = NULL;
   S_CHECK(_get_param(ovf_def, param_name, &p));

   if (!p->def->allow_vector) {
      error("[OVF] ERROR in OVF %s: parameter %s cannot be defined as a vector\n",
            ovf_def->name, p->def->name);
      /*  TODO(xhub) print usage */
      return Error_Unconsistency;
   }

   p->type = ARG_TYPE_VEC;
   MALLOC_(p->vec, double, n_vals);
   memcpy(p->vec, vals, n_vals*sizeof(double));

   return OK;
}

/**
 *  @brief Check that an OVF is well defined
 *
 *  @ingroup publicAPI
 *
 *  @param mdl      the model
 *  @param ovf_def  the OVF definition to check
 *
 *  @return         OK if the OVF definition is correct, otherwise the error
 *                  code
 */
int rhp_ovf_check(Model *mdl, OvfDef *ovf_def)
{
   S_CHECK(chk_mdl(mdl, __func__));
   S_CHECK(chk_arg_nonnull(ovf_def, 2, __func__));

   S_CHECK(ovf_check(ovf_def));

   return OK;
}

/**
 *  @brief Set a reformulation for the given OVF
 *
 *  @ingroup publicAPI
 *
 *  @param ovf_def        the OVF definition to check
 *  @param reformulation  the reformulation
 *
 *  @return               the error code
 */
int rhp_ovf_setreformulation(OvfDef *ovf_def, const char *reformulation)
{
  S_CHECK(chk_arg_nonnull(ovf_def, 1, __func__));
  S_CHECK(chk_arg_nonnull(reformulation, 2, __func__));

   unsigned val;
   if (!optovf_getreformulationmethod(reformulation, &val)) {
      error("[ovf] ERROR: reformulation '%s' is not valid\n", reformulation);
      return Error_InvalidValue;
   }

   ovf_def->reformulation = val;

   printout(PO_INFO, "Setting OVF reformulation to '%s' for OVF with var index %u\n",
            reformulation, ovf_def->ovf_vidx);

   return OK;
}
