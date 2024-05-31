#include "reshop_config.h"

#include <assert.h>
#include <string.h>

#include "container.h"
#include "compat.h"
#include "ctr_rhp.h"
#include "equ.h"
#include "nltree.h"
#include "gams_option.h"
#include "hdf5_logger.h"
#include "mdl.h"
#include "mdl_timings.h"
#include "rmdl_options.h" 
#include "open_lib.h"
#include "printout.h"
#include "reshop_solvers.h"
#include "solver_eval.h"
#include "status.h"
#include "timings.h"
#include "tlsdef.h"
#include "var.h"


tlsvar void* libpath_handle = NULL;
tlsvar const char* libpath_fname = NULL;

#undef IMPORT_FUNC
#define IMPORT_FUNC(RTYPE, FN_NAME, ...) tlsvar RTYPE (*FN_NAME)(__VA_ARGS__) = NULL;
#include "path_func.h"


static const char *path_libnames[] = {
   DLL_FROM_NAME("path47"),
   DLL_FROM_NAME("path50"),
   DLL_FROM_NAME("path51")
};

#if defined(_WIN32) && defined(_MSC_VER)
#define WIN32_LEAN_AND_MEAN
#define VC_EXTRALEAN
#include <windows.h>
#include "win-compat.h"

  void cleanup_path(void)
  {
      if (libpath_handle)
      {
        FreeLibrary(libpath_handle);
        libpath_handle = NULL;
      }
  }

#elif defined(__GNUC__) & !defined(__APPLE__)
#include <dlfcn.h>
  static __attribute__ ((destructor)) void cleanup_path(void) 
  {
    if (libpath_handle) {
       dlclose(libpath_handle);
       libpath_handle = NULL;
    }
    if (libpath_fname) {
       FREE(libpath_fname);
    }
  }
#endif

static int load_pathlib(Model *mdl)
{
   if (!libpath_handle) {
      const char* libname, *opt_libpath_fname = NULL;
      bool copy_libpath_fname = false;

      if (!libpath_fname) {
         opt_libpath_fname = optvals(mdl, Options_Pathlib_Name);

         if (opt_libpath_fname && strlen(opt_libpath_fname) > 0) {
            libpath_fname = opt_libpath_fname;
            copy_libpath_fname = true;
         }
      }

      if (libpath_fname) {
         libpath_handle = open_library(libpath_fname, 0);
         if (libpath_handle) {
           libname = copy_libpath_fname ? strdup(libpath_fname) : libpath_fname;
         }
      }

      FREE(opt_libpath_fname);

      unsigned ii = ARRAY_SIZE(path_libnames);
      while (!libpath_handle && ii <= ARRAY_SIZE(path_libnames) && ii > 0) {
         ii--;
         libname = path_libnames[ii];
         libpath_handle = open_library(libname, 0);
      }

      if (!libpath_handle) {
         errormsg("[PATH] ERROR: Could not find the PATH solver. The following names where tried: ");
         for (unsigned i = 0, len = ARRAY_SIZE(path_libnames); i < len; ++i) {
            errormsg(path_libnames[i]); errormsg(" ");
         }
         errormsg("[PATH] Use the option 'pathlib_name' to set the path to the PATH library\n");
         return Error_SystemError;
      }

#define PLUGIN_HANDLE libpath_handle
#define LIBNAME libname

#undef IMPORT_FUNC

#define IMPORT_FUNC(RTYPE, FN_NAME, ...) \
*(void **) (&FN_NAME) = get_function_address(PLUGIN_HANDLE, #FN_NAME); \
if (!FN_NAME) { \
   error("%s :: Could not find function named "#FN_NAME" in %s." \
            "Some functionalities may not " \
            "be available\n", __func__, LIBNAME); \
   return Error_SystemError; }

#include "path_func.h"
   }

   return OK;
}

#include "PATH_SDK/Types.h"

static const char *_path_rc_code_str(int rc)
{
  switch(rc) {
  case MCP_Solved:
    return "The problem was solved";
  case MCP_NoProgress:
    return "A stationary point was found";
  case MCP_MajorIterationLimit:
    return "Major iteration limit met";
  case MCP_MinorIterationLimit:
    return "Cumulative minor iterlim met";
  case MCP_TimeLimit:
    return "Ran out of time";
  case MCP_UserInterrupt:
    return "Control-C, typically";
  case MCP_BoundError:
    return "Problem has a bound error";
  case MCP_DomainError:
    return "Could not find starting point";
  case MCP_Infeasible:
    return "Problem has no solution";
  case MCP_Error:
    return "An error occurred within the code";
  case MCP_Reduced:
    return "Presolve reduced problem";
  case MCP_LicenseError:
    return "License could not be found";
  case MCP_OK:
    return "Presolve did not perform any modifications";
  default:
    return "unknown code";
  }
}

struct _status {
  unsigned char model;
  unsigned char solve;
};

static struct _status _path_rc2status(int rc)
{
  switch(rc) {
  case MCP_Solved:
    return (struct _status){ModelStat_OptimalLocal, SolveStat_Normal};
  case MCP_NoProgress:
    return (struct _status){ModelStat_Feasible, SolveStat_Solver};
  case MCP_MajorIterationLimit:
    return (struct _status){ModelStat_Feasible, SolveStat_Iteration };
  case MCP_MinorIterationLimit:
    return (struct _status){ModelStat_Feasible,SolveStat_Iteration };
  case MCP_TimeLimit:
    return (struct _status){ModelStat_Feasible, SolveStat_Resource };
  case MCP_UserInterrupt:
    return (struct _status){ModelStat_ErrorUnknown, SolveStat_User };
  case MCP_BoundError:
    return (struct _status){ModelStat_InfeasibleLocal, SolveStat_SetupErr };
  case MCP_DomainError:
    return (struct _status){ModelStat_InfeasibleLocal, SolveStat_EvalError };
  case MCP_Infeasible:
    return (struct _status){ModelStat_InfeasibleGlobal, SolveStat_Normal };
  case MCP_Error:
    return (struct _status){ModelStat_ErrorUnknown, SolveStat_SolverErr };
  case MCP_Reduced:
    return (struct _status){ModelStat_ErrorUnknown, SolveStat_SolverErr };
  case MCP_LicenseError:
    return (struct _status){ModelStat_LicenseError, SolveStat_License };
  case MCP_OK:
    return (struct _status){ModelStat_ErrorUnknown, SolveStat_Normal };
  default:
    return (struct _status){ModelStat_ErrorUnknown, SolveStat_InternalErr };
  }
}
#define path_int Int

static inline BasisStatus basis_path_to_rhp(int basis)
{
 /* TODO(xhub) low could be improved by a static table */
    switch (basis) {
    case Basis_LowerBound:
       return BasisLower;
    case Basis_UpperBound:
       return BasisUpper;
    case Basis_Basic:
       return BasisBasic;
    case Basis_SuperBasic:
       return BasisSuperBasic;
    case Basis_Unknown:
       return BasisUnset;
    case Basis_Fixed:
       return BasisFixed;
    default:
       return BasisUnset;
    }

}

static unsigned path_eval_jacobian(Container * restrict ctr,
                                   struct jacdata * restrict jacdata,
                                   double * restrict x,
                                   double * restrict F,
                                   int * restrict col,
                                   int * restrict len,
                                   int * restrict row,
                                   double * restrict vals)
{
   assert(ctr->n == jacdata->n && jacdata->nnz == jacdata->p[ctr->n]);
   /* TODO(Xhub) parallelize  */
   /* TODO(xhub) support constant case AVI with specialized code  */

/* - col[i] is the column start in [1, ..., nnz] in the row/data vector,
 * - len[i] is the number of nonzeros in the column,
 * - row[j] is the row index in [1, ..., n]
 *
 *   with 1-based indices ... */

   for (size_t i = 0; i < jacdata->nnz; ++i) {
      S_CHECK(rctr_evalfuncat(ctr, &jacdata->equs[i], x, &vals[i]));
      row[i] = jacdata->i[i] + 1;

      assert(row[i] >= 1 && row[i] <= jacdata->n && isfinite(vals[i]));
      DPRINT("path_jac: row = %i; val = %e\n", row[i], vals[i]);
   }

   for (size_t i = 0; i < jacdata->n; ++i) {
      col[i] = jacdata->p[i] + 1;
      len[i] = jacdata->p[i+1] - jacdata->p[i];

      assert(len[i] >= 0 && col[i] >= 1 && col[i] <= jacdata->nnz && (i == 0 || col[i] >= col[i-1]));
      DPRINT("path_jac: col = %i; start = %i, len = %i\n", (int)i+1, col[i], len[i]);
   }

   return 0;
}

static void (path_problem_size)(void *id, path_int *size, path_int *nnz)
{
   struct path_env *env = (struct path_env *)id;
   *size = env->ctr->n;
   *nnz = env->jacdata->nnzmax;
}

static void (path_bounds)(void *id, path_int size, double * restrict x, double * restrict l, double * restrict u)
{
   struct path_env *env = (struct path_env *)id;
   Var * restrict vars = env->ctr->vars;
   assert(size == env->ctr->n);

   for (size_t i = 0; i < size; ++i) {
      assert(isfinite(vars[i].value) || isnan(vars[i].value));
      assert(!isnan(vars[i].bnd.lb) && !isnan(vars[i].bnd.ub));

      double val = vars[i].value;
      x[i] = isfinite(val) ? val : 0.;

      if (isfinite(vars[i].bnd.lb)) { l[i] = vars[i].bnd.lb; }
      if (isfinite(vars[i].bnd.ub)) { u[i] = vars[i].bnd.ub; }
   }
}

static path_int (path_function_evaluation)(void *id, path_int n, double *x, double *f)
{
   struct path_env *env = (struct path_env *)id;
   return env->eval_func(env->ctr, x, f);
}

static path_int (path_jacobian_evaluation)(void *id, path_int n, double *x,
                                    path_int wantf, double *f,
                                    path_int *nnz, path_int *col, path_int *len,
                                    path_int *row, double *data)
{
   struct path_env *env = (struct path_env *)id;
   assert(env->ctr->n == n);
   path_int num_err = 0;

   if (wantf) {
      double start = get_thrdtime();
      num_err = env->eval_func(env->ctr, x, f);
      env->timings.func_evals += start - get_thrdtime();
   }
   double start = get_thrdtime();
   num_err += path_eval_jacobian(env->ctr, env->jacdata, x, f, col, len, row, data);
   env->timings.jacobian_evals += start - get_thrdtime();

   *nnz = env->jacdata->nnz;
   assert(env->jacdata->p[n] == *nnz);

   if (env->logh5) {
      logh5_inc_iter(env->logh5);
      struct sp_matrix mat;
      mat.n = n;
      mat.m = n;
      mat.nnz = *nnz;
      mat.nnzmax = env->jacdata->nnzmax;
      mat.i = (RHP_INT*)row;
      mat.p =  (RHP_INT*)col;
      mat.x = data;
      logh5_sparse(env->logh5, &mat, "jacobian");
      logh5_vec_double(env->logh5, n, x, "x");
      logh5_vec_double(env->logh5, n, f, "f");
   }

   return num_err;
}

UNUSED static
path_int (path_hessian_evaluation)(void *id, path_int n, double *x, double *l,
                                   path_int *nnz, path_int *col, path_int *len,
                                   path_int *row, double *data)
{
   /*  To implement. The spec talks about computing the following full matrix */
 /* H = sum_i lambda[i] * nabla^2 F_i(x) */
 return 1;
}

//void (path_start)(void *id);
//void (path_finish)(void *id, double *x);

static void (path_varname)(void *id, path_int vi, char *buffer, path_int buf_size)
{
   struct path_env *env = (struct path_env *)id;
   /* PATH indices are 1-based  */
   ctr_copyvarname(env->ctr, vi-1, buffer, buf_size);
}

static void (path_equname)(void *id, path_int ei, char *buffer, path_int buf_size)
{
   struct path_env *env = (struct path_env *)id;
   /* PATH indices are 1-based  */
   ctr_copyequname(env->ctr, ei-1, buffer, buf_size);
}

static void path_basis(void *id, path_int size, path_int * restrict basX)
{
   struct path_env *env = (struct path_env *)id;
   Var * restrict vars = env->ctr->vars;
   assert(size == env->ctr->n);

   for (size_t i = 0; i < size; ++i) {
    switch (vars[i].basis) {
    case BasisLower:
       basX[i] = Basis_LowerBound;
       break;
    case BasisUpper:
       basX[i] = Basis_UpperBound;
       break;
    case BasisBasic:
       basX[i] = Basis_Basic;
       break;
    case BasisSuperBasic:
       basX[i] = Basis_SuperBasic;
       break;
    case BasisUnset:
       basX[i] = Basis_Unknown;
       break;
    case BasisFixed:
       basX[i] = Basis_Fixed;
    }

   }
}

static void path_presolve_type(void *id, int nnz, int *type)
{
   struct path_env *env = (struct path_env *)id;
   assert(nnz == env->jacdata->nnz);
   Equ * restrict equs = env->jacdata->equs;

   /* ----------------------------------------------------------------------
    * 0 is ???
    * 1 is ???
    *
    * This is obviously wrong because we don't remove yet constant from nltree
    * ---------------------------------------------------------------------- */

   for (size_t i = 0; i < (size_t)nnz; ++i) {
      Equ *e = &equs[i];
      if ((!e->lequ || e->lequ->len == 0) && 
          (!e->tree || !e->tree->root)) {
         type[i] = 0;
      } else {
         type[i] = 1;
      }
   }
}
static void path_presolve_cons(void *id, int n, int *type)
{
   struct path_env *env = (struct path_env *)id;
   assert(n > 0 && n == env->ctr->n);

   /* ----------------------------------------------------------------------
    * The primal variables are tagged with 1, the dual with -1
    * ---------------------------------------------------------------------- */

   size_t n_primal = env->jacdata->n_primal;
   for (size_t i = 0; i < n_primal; ++i) {
      type[i] = 1;
   }

   for (size_t i = n_primal; i < (size_t)n; ++i) {
      type[i] = -1;
   }
}

static void _path_report_sol(Container * restrict ctr, double * restrict x,
                             double * restrict F, int * restrict basis, size_t n_primal)
{
   /* ----------------------------------------------------------------------
    * This function takes the value of the F and injects it into the multiplier
    * value of the corresponding variable
    *
    * F(x,λ) ⟂ x  -> the value of F gives the multiplier for the bound constraint on x
    * F(x)   ⟂ λ  -> the value of F is the value of the constraint
    *             -> the basis information on λ gives the one for the constraint
    *             -> the value of λ is the value of the multiplier for the constraint
    *
    * ---------------------------------------------------------------------- */

   for (size_t i = 0; i < ctr->n; ++i) {
      ctr->vars[i].value = x[i];
      ctr->vars[i].basis = basis_path_to_rhp(basis[i]);
      ctr->equs[i].value = F[i] - ctr->equs[i].p.cst;
   }

   /*  Report multiplier on x (primal value) */
   for (size_t i = 0; i < n_primal; ++i) {
      ctr->vars[i].multiplier = F[i];
   }

   /*  Report multiplier on constraints */
   for (size_t i = n_primal; i < (size_t)ctr->n; ++i) {
      ctr->equs[i].multiplier = x[i];
    switch (ctr->vars[i].basis) {
    case BasisLower:
    case BasisUpper:
      ctr->equs[i].basis = BasisBasic;
      break;
    case BasisBasic:
    case BasisSuperBasic:
        {
      const struct var_bnd *bnd = &ctr->vars[i].bnd;
      /* Seems that if we have an equality constraint, we set the basis status to upper */
      if (bnd->lb == 0.) {
        ctr->equs[i].basis = BasisLower;
      } else {
        ctr->equs[i].basis = BasisUpper;
      }
      break;
        }
    case BasisFixed:
      default:
        ctr->equs[i].basis = BasisUnset;
    }
  }
}

static void flush_dummy(void *data, int mode)
{
   return;
}

static void path_print(void *data, int mode, const char* msg)
{
   if (!O_Output_Subsolver_Log) { return; }

   if (mode & Output_Log) { printstr((O_Output & PO_ALLDEST) | PO_INFO, msg); }
   else if (mode & Output_Status) { printstr((O_Output & PO_ALLDEST) | PO_V, msg); }
   else if (mode & Output_Listing) { printstr((O_Output & PO_ALLDEST) | PO_VV, msg); }
}

#ifdef __GNUC__
_Pragma("GCC diagnostic push")
_Pragma("GCC diagnostic ignored \"-Wpedantic\"")
#endif

static tlsvar void* path_io[] = {
   NULL,                         /*  User data */
   (void*)path_print,
   (void*)flush_dummy,
};

static tlsvar void* mcp_iface[] = {
   NULL,                         /* Interface data  */
   (void*)path_problem_size,                 /*  */
   (void*)path_bounds,
   (void*)path_function_evaluation,
   (void*)path_jacobian_evaluation,
   NULL,                         /* Hessian evaluation */
   NULL,                         /* Start function  */
   NULL,                         /* Finish function  */
   (void*)path_varname,                 /* Variable name  */
   (void*)path_equname,                 /* Constraint name  */
   (void*)path_basis,                   /* Basis information  */
};

static tlsvar void* presolve_iface[] = {
   NULL,                         /* Presolve data  */
   NULL,                         /* Start pre-solve   */
   NULL,                         /* Start post-solve  */
   NULL,                         /* End pre-solve     */
   NULL,                         /* End post-solve    */
   (void*)path_presolve_type,           /* Specify linear or NL elements       */
   (void*)path_presolve_cons            /* Specify primal or dual variables    */
};

#ifdef __GNUC__
_Pragma("GCC diagnostic pop")
#endif

int solver_path(Model * restrict mdl, struct jacdata * restrict jac)
{
   int status = OK;
   bool export_hdf5 = false;

   Information path_info;
   memset(&path_info, 0, sizeof(Information));

   unsigned print_level = O_Output & PO_MASK_LEVEL;
   if (print_level >= PO_VV) {
      path_info.generate_output = Output_Log | Output_Status | Output_Listing;
   } else if (print_level >= PO_V) {
      path_info.generate_output = Output_Log | Output_Status;
   } else if (print_level >= PO_INFO) {
      path_info.generate_output = Output_Log;
   } else {
      path_info.generate_output = 0;
   }

   path_info.use_start = 1;
   path_info.use_basics = 1;

   struct path_env env;
   env.ctr = &mdl->ctr;
   env.jacdata = jac;
   env.eval_func = ge_eval_func;
   env.eval_jacobian = NULL;

   if (export_hdf5) {
      char buf[256];
      snprintf(buf, sizeof buf, "/tmp/debug-%p.h5", (void*)mdl);
      env.logh5 = logh5_init(buf, 100000);
   } else {
      env.logh5 = NULL;
   }

   /* ----------------------------------------------------------------------
    * Path opaque objects
    * ---------------------------------------------------------------------- */
   void *m = NULL;
   void *opt = NULL;

   /* ----------------------------------------------------------------------
    * Put the environment at the right place
    * ---------------------------------------------------------------------- */

   mcp_iface[0] = &env;
   presolve_iface[0] = &env;

   /* ----------------------------------------------------------------------
    * Try to load a PATH library and define the suitable function calls
    * ---------------------------------------------------------------------- */

   S_CHECK_EXIT(load_pathlib(mdl));
#undef IMPORT_FUNC
#define IMPORT_FUNC(RTYPE, FN_NAME, ...) assert(FN_NAME);
#include "path_func.h"

   /* ----------------------------------------------------------------------
    * Interface first, then options
    *
    * WARNING: the order of function calls for the option is super important
    * ---------------------------------------------------------------------- */

   Output_SetInterface(path_io);

   opt = Options_Create();
   Path_AddOptions(opt);
   Options_Default(opt);

   double tol;
   S_CHECK(rmdl_getoption(mdl, "rtol", &tol));
   Options_SetDouble(opt, "convergence_tolerance", tol);

//   Options_SetDouble(opt, "infinity", Infinity);

//   Options_SetBoolean(opt, "output_linear_model", 1);
//   Options_Set(opt, "crash_method pnewton");
//   Options_Set(opt, "crash_perturb yes");
//   Options_Set(opt, "lemke_start_type slack");
//   Options_Set(opt, "lemke_start first");
//   Options_Set(opt, "crash_method none");
//   Options_Set(opt, "crash_perturb no");
   Options_Display(opt);

   m = MCP_Create(mdl->ctr.n, jac->nnzmax);
   if (!m) {
      errormsg("[PATH] ERROR: cannot create MCP object\n");
      status = Error_SolverCreateFailed;
      goto _exit;
   }


   /* ----------------------------------------------------------------------
    * Set the MCP interface, then the presolve one, and set some jacobian
    * properties
    * ---------------------------------------------------------------------- */

   MCP_SetInterface(m, &mcp_iface);
   MCP_SetPresolveInterface(m, &presolve_iface);
   MCP_Jacobian_Structure_Constant(m, 1);
   MCP_Jacobian_Data_Contiguous(m, 1);
   MCP_Jacobian_Diagonal(m, 1);
   //MCP_Jacobian_Diagonal_First(m, 0);

   /* ----------------------------------------------------------------------
    * Call the solver and hope it can solve the problem
    * ---------------------------------------------------------------------- */

   double solve_start = get_walltime();
   int path_rc = Path_Solve(m, &path_info);
   env.timings.path_solve = solve_start - get_walltime();
   struct _status stats = _path_rc2status(path_rc);

   S_CHECK_EXIT(mdl_setmodelstat(mdl, stats.model));
   S_CHECK_EXIT(mdl_setsolvestat(mdl, stats.solve));

   if (path_rc != MCP_Solved) {
      printout(PO_INFO, "PATH return with code %i which most likely means \"%s\".\n"
                         "The model was most likely not solved\n", path_rc,
                         _path_rc_code_str(path_rc));
   }

    /* -------------------------------------------------------------------
     * Get values from solver and put them in the container
     * ------------------------------------------------------------------- */

    double * restrict x = MCP_GetX(m);
    double * restrict F = MCP_GetF(m);
    int * restrict basis = MCP_GetB(m);

    _path_report_sol(&mdl->ctr, x, F, basis, jac->n_primal);

_exit:
   if (env.logh5) {
      logh5_end_iter(env.logh5);
      int h5status = logh5_end(env.logh5);
      status = status != OK ? status : h5status;
   }

   if (m) {
      MCP_Destroy(m);
   }

   if (opt) {
      Options_Destroy(opt);
   }

   return status;
}

int rhp_PATH_setfname(const char* fname)
{
   if (!fname) {
      errormsg("Filename for PATH is NULL!\n");
      return Error_NullPointer;
   }

   FREE(libpath_fname);

   libpath_fname = strdup(fname);
   if (!libpath_fname) {
      errormsg("Allocation for copying the PATH filename failed!\n");
      return Error_SystemError;
   }

   return OK;
}
