#include "asprintf.h"
#include "container.h"
/*  TODO(xhub) remove that with ctr and mdl merge */
#include "empinfo.h"
#include "nltree.h"
#include "fooc.h"
#include "macros.h"
#include "mdl.h"
#include "ctrdat_rhp.h"
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

#if defined(_WIN32) && defined(_MSC_VER)
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

static int _load_pathvi(void)
{
   if (!solver_pathvi) {
      if (!plugin_pathvi_handle) {
         plugin_pathvi_handle = open_library(DLL_FROM_NAME("reshop_pathvi"), 0);
         if (!plugin_pathvi_handle) {
            error("%s :: Could not find "DLL_FROM_NAME("reshop_pathvi")". "
                     "Some functionalities may not be available\n", __func__);
            return Error_SystemError;
         }
      }

      *(void **) (&solver_pathvi) = get_function_address(plugin_pathvi_handle, "pathvi_solve");
      if (!solver_pathvi) {
         error("%s :: Could not find function named pathvi_solve"
                  " in " DLL_FROM_NAME("reshop_pathvi")". Some functionalities may not "
                  "be available\n", __func__);
         return Error_SystemError;
      }

   }

   return OK;
}


#ifndef NDEBUG
UNUSED static void _output_equs(Model *mdl)
{
   char* dot_print_dir = getenv("RHP_DOT_PRINT_DIR");
   char *fname;

   Container *ctr = &mdl->ctr;

   if (dot_print_dir) {
      for (unsigned i = 0; i < (unsigned)ctr->m; ++i) {
         if (!ctr->equs[i].tree) {
            continue;
         }
         int res = asprintf(&fname, "%s" DIRSEP "equ%d.dot", dot_print_dir, i);
         if (res < 0) {
           error("%s :: asprintf() failed\n", __func__);
           return;
         }
         nltree_print_dot(ctr->equs[i].tree, fname, ctr);
      }
   }
}
#else
#define _output_equs(X) 
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

  _output_equs(mdl);

   switch (mdldata->solver) {
#ifdef TO_BE_DELETED
   case RMDL_SOLVER_PATHVI_GE:
   {
      struct reshop_print rp = { .printout = &printout, .printstr = &printstr };
      S_CHECK(_load_pathvi());
      return solver_pathvi(&mdl->ctr, jacdata, &rp);
   }
#endif
   case RMDL_SOLVER_GAMS:
   {
     Model *mdl_gams;
     A_CHECK(mdl_gams, rhp_mdl_new(RHP_BACKEND_GAMS_GMO));

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
   struct jacdata jacdata;
   memset(&jacdata, 0, sizeof(jacdata));

   McpStats mcpdata;
   S_CHECK(fooc_create_mcp(mdl, &mcpdata));

   /* TODO: what is the relationship between jacdata.n and mcpdata.n? */
   jacdata.n_nl = mcpdata.n_nlcons;
   jacdata.n_primal = mcpdata.n_primalvars;
   S_CHECK_EXIT(ge_prep_jacdata(&mdl->ctr, &jacdata));

   S_CHECK_EXIT(solve_mcp(mdl, &jacdata));

   S_CHECK_EXIT(fooc_postprocess(mdl, mcpdata.n_primalvars));

_exit:
   jacdata_free(&jacdata);

   /*  TODO(xhub) make the code in path generic? */
//   S_CHECK(_report_dual_info_into_primal(mdl));

   return status;
}

