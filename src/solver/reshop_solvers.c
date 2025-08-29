#include "asprintf.h"
#include "container.h"
/*  TODO(xhub) remove that with ctr and mdl merge */
#include "nltree.h"
#include "fooc.h"
#include "macros.h"
#include "mdl.h"
#include "mdl_gams.h"
#include "mdl_rhp.h"
#include "open_lib.h"
#include "printout.h"
/* for RMDL_SOLVER_GAMS */
#include "reshop.h"
#include "reshop_solvers.h"
#include "rmdl_data.h"
#include "solver_eval.h"
#include "status.h"
#include "tlsdef.h"


tlsvar int (*solver_pathvi)(void *, void *, void *) = NULL;
tlsvar void* plugin_pathvi_handle = NULL;

#if defined(_WIN32) && !defined(__CYGWIN__)
#define WIN32_LEAN_AND_MEAN
#define VC_EXTRALEAN
#include <windows.h>
#include "win-compat.h"

  void cleanup_pathvi(void)
  {
      if (plugin_pathvi_handle)
      {
        FreeLibrary(plugin_pathvi_handle);
        plugin_pathvi_handle = NULL;
      }
  }

#elif defined(__GNUC__) & !defined(__APPLE__)
#include <dlfcn.h>

  __attribute__ ((destructor)) static void cleanup_pathvi(void) 
  {
    if (plugin_pathvi_handle)
    {
       dlclose(plugin_pathvi_handle);
       plugin_pathvi_handle = NULL;
    }
  }
#endif


/**
 * @brief  Solve a generalized equation
 *
 * @param mdl  the model
 *
 * @return     the error code
 */
static int solve_mcp(Model *mdl, struct jacdata *jacdata)
{
   RhpModelData *mdldata = (RhpModelData *)mdl->data;

   S_CHECK(rmdl_export_latex(mdl, __func__));
   S_CHECK(mdl_export_gms(mdl, __func__));

   switch (mdldata->solver) {
   case RMDL_SOLVER_GAMS:
   {
     Model *mdl_gams;
     A_CHECK(mdl_gams, mdl_new(RhpBackendGamsGmo));

     S_CHECK(gmdl_set_gamsdata_from_env(mdl_gams));

     S_CHECK(rhp_process(mdl, mdl_gams));
     S_CHECK(rhp_solve(mdl_gams));
     S_CHECK(rhp_postprocess(mdl_gams));

      mdl_release(mdl_gams);

     return OK;
   }
   case RMDL_SOLVER_UNSET:
   case RMDL_SOLVER_PATH:
   {
      return solver_path(mdl, jacdata);
   }
   default:
      error("%s :: unsupported GE solver %d\n", __func__, mdldata->solver);
      return Error_NotImplemented;
   }

   return OK;
}

/**
 * @brief Solve a mathematical problem as an MCP using RHP as backend
 *
 * @param mdl  the model to solve
 *
 * @return     the error code
 */
int rmdl_solve_asmcp(Model *mdl)
{
   int status = OK;
   assert(mdl->mdl_up);

  /* ----------------------------------------------------------------------
   * We expect mdl->mdl_up to contain the model to solve and mdl to be an 
   * empty shell right now.
   * ---------------------------------------------------------------------- */

   ModelType mdltype;
   S_CHECK(mdl_gettype(mdl->mdl_up, &mdltype));


  switch (mdltype) {
  case MdlType_lp:
  case MdlType_qcp:
  case MdlType_nlp:
  case MdlType_emp:
    S_CHECK(fooc_create_mcp(mdl));
    break;

  case MdlType_mcp:
      TO_IMPLEMENT("Model type MCP in rmdl_solve_asmcp()");
  case MdlType_dnlp:
    error("%s :: ERROR: nonsmooth NLP are not supported\n", __func__);
    return Error_NotImplemented;

  case MdlType_cns:
    error("%s :: ERROR: constraints systems are not supported\n", __func__);
    return Error_NotImplemented;

  case MdlType_mip:
  case MdlType_minlp:
  case MdlType_miqcp:
    error("%s :: ERROR: Model with integer variables are not yet supported\n", __func__);
    return Error_NotImplemented;

  default:
    error("%s :: ERROR: unknown/unsupported container type %s\n", __func__,
          mdltype_name(mdltype));
    return Error_InvalidValue;
  }

   McpInfo *mcpdata;
   A_CHECK(mcpdata, mdl_getmcpinfo(mdl));

   /* TODO: what is the relationship between jacdata.n and mcpdata.n? */
   struct jacdata jacdata;
   memset(&jacdata, 0, sizeof(jacdata));
   jacdata.n_nl = mcpdata->n_nlcons;
   jacdata.n_primal = mcpdata->n_primalvars;
   S_CHECK_EXIT(ge_prep_jacdata(&mdl->ctr, &jacdata));

   S_CHECK_EXIT(solve_mcp(mdl, &jacdata));

_exit:
   jacdata_free(&jacdata);

   return status;
}
