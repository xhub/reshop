#include "checks.h"
#include "compat.h"
#include "ctrdat_gams.h"
#include "macros.h"
#include "mdl.h"
#include "mdl_gams.h"
#include "reshop-gams.h"
#include "reshop.h"
#include "sys_utils.h"
#include "var.h"

/**
 * @brief Set the model name
 *
 * @ingroup publicAPI
 *
 * @param mdl   the model
 * @param name  the name
 *
 * @return      the error code
 */
int rhp_mdl_setname(Model *mdl, const char *name)
{
   S_CHECK(chk_mdl(mdl, __func__));
   S_CHECK(chk_arg_nonnull(name, 2, __func__));
 
   return mdl_setname(mdl, name);
}

/**
 * @brief Fix a variable to a given value
 *
 * @ingroup publicAPI
 *
 * @param mdl the model
 * @param vi  the variable index
 * @param val the value
 *
 * @return    the error code
 */
int rhp_mdl_fixvar(Model *mdl, rhp_idx vi, double val)
{
  S_CHECK(chk_mdl(mdl, __func__));
  S_CHECK(chk_vi(mdl, vi, __func__));

  Var *v = &mdl->ctr.vars[vi];

  v->basis = BasisFixed;
  v->value = val;
  v->bnd.lb = v->bnd.ub = val;

  return OK;
}

/**
 * @brief Get the backend of the model
 *
 * @ingroup publicAPI
 *
 * @param mdl the model
 *
 * @return    the backend
 */
enum rhp_backendtype rhp_mdl_getbackend(const Model *mdl)
{
   S_CHECK(chk_mdl(mdl, __func__));
   return mdl->backend;
}

/**
 * @brief Get the backend name of the model
 *
 * @ingroup publicAPI
 *
 * @param mdl the model
 *
 * @return    the backend name
 */
const char* rhp_mdl_getbackendname(const Model *mdl)
{
   SN_CHECK(chk_mdl(mdl, __func__));
   return backend2str(mdl->backend);
}

/**
 * @brief Get the name of the model
 *
 * @ingroup publicAPI
 *
 * @param mdl the model
 *
 * @return    the model name
 */
const char* rhp_mdl_getname(const struct rhp_mdl *mdl)
{
   SN_CHECK(chk_mdl(mdl, __func__));
   const char *name = mdl->commondata.name;

   return name ? name : "";
}

/**
 * @brief Get the ID of the model
 *
 * @ingroup publicAPI
 *
 * @param mdl the model
 *
 * @return    the model ID
 */
unsigned rhp_mdl_getid(const struct rhp_mdl *mdl)
{
   if (!mdl) { return IdxError; }
   return mdl->id;
}

/**
 * @brief Instantiate the solver model 
 *
 * @ingroup publicAPI
 *
 * By default the model returned is of the same type as the argument. However,
 * a few mechanism may change that (in order of priority)
 * - the solver_stack option is set in the model passed to the function
 * - the environment variable RHP_SOLVER_BACKEND is set (and valid)
 *
 * Note that no association is made between the model given in argument and the
 * model returned.
 *
 * @warning the caller must call rhp_mdl_free on the returned object
 *
 * @param mdl  the source model 
 *
 * @return     the solver model
 */
Model *rhp_newsolvermdl(Model *mdl)
{
   BackendType mdl_type = mdl->backend;

   /*  TODO(xhub) the solver_stack option */

   const char *backend_env = mygetenv("RHP_SOLVER_BACKEND");
   if (backend_env) {
      unsigned idx = backend_idx(backend_env);
      mdl_type = idx < UINT_MAX ? idx : mdl_type;
   }
   myfreeenvval(backend_env);

   return mdl_new(mdl_type);
}/**
 * @brief Load a model from a ReSHOP control file
 *
 * This function differs from rhp_mdl_newfromcntr just in that it uses the regular
 * logging operations and not the GAMS one from the control file.
 *
 * @warning This assumes that the model is a reshop model.
 * It will try to load the empinfo and reshop options
 *
 * @ingroup publicAPI
 *
 * @param       cntrfile  the GAMS control file 
 * @param[out]  mdlout    the model
 *
 * @return           the error code
 */
int rhp_mdl_newfromcntr(const char *cntrfile, Model **mdlout)
{
   int status = OK;
   char buffer[2048];

   *mdlout = NULL;
   S_CHECK(chk_arg_nonnull(cntrfile, 1, __func__));
   
   FILE* fptr = fopen(cntrfile, RHP_READ_TEXT);
   if (!fptr) {
      error("[GAMS] ERROR: couldn't open control file '%s'\n", cntrfile);
      return Error_RuntimeError;
   }

   /* GAMSDIR seems to be on the 29th line */
   for (unsigned i = 0; i < 29; ++i) {
      if (!fgets(buffer, sizeof buffer, fptr)) {
         error("[GAMS] ERROR: failed to get %u-th line of control file '%s'\n",
               i, cntrfile);
         IO_CALL(fclose(fptr));
         return Error_RuntimeError;
      }
   }

   IO_CALL(fclose(fptr));

   size_t len = strlen(buffer);
   if (len <= 1) {
      error("[GAMS] ERROR: bogus gamsdir '%s' from control file '%s'\n", buffer,
            cntrfile);
      return Error_RuntimeError;
   }

   /* fgets keeps the newline character in, remove it */
   trim_newline(buffer, len);

   Model *mdl;
   A_CHECK(mdl, mdl_new(RhpBackendGamsGmo));

   S_CHECK_EXIT(rhp_gms_setgamsdir(mdl, buffer));

   S_CHECK_EXIT(rhp_gms_loadlibs(buffer));

   S_CHECK_EXIT(rhp_gms_setgamscntr(mdl, cntrfile));

   S_CHECK_EXIT(gcdat_loadmdl(mdl->ctr.data, mdl->data));

   /* Print the reshop banner, after setting the printops */
   rhp_print_banner();

   *mdlout = mdl;

   S_CHECK(gmdl_loadrhpoptions(mdl));

   S_CHECK(rhp_gms_fillmdl(mdl));

   S_CHECK(rhp_gms_readempinfo(mdl, NULL));

   S_CHECK(mdl_check(mdl));

   return OK;

_exit:
   mdl_release(mdl);
   return status;
}

