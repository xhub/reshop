#include "asnan.h"

#include <math.h>
#include <stdint.h>
#include <stdio.h>

#if defined(_WIN32)
//  #define snprintf _snprintf
//  #define vsnprintf _vsnprintf
  #define strcasecmp _stricmp
  #define strncasecmp _strnicmp
  #define strdup _strdup
#include <string.h>

#else

#include <string.h>

#endif

#include "reshop.h"

#include "test_common.h"

#include "colors.h"

#define TOL_EPS 1e-10
#define TOL_EPSLAX 1e-4

void sol_vals_init(struct sol_vals *vals)
{
  vals->xvals = NULL;
  vals->xduals = NULL;
  vals->xbas = NULL;
  vals->evals = NULL;
  vals->eduals = NULL;
  vals->ebas = NULL;
  vals->objval = NAN;

  sol_vals_test_init(&vals->dotest);
}

void sol_vals_test_init(struct sol_vals_test *vals)
{
  vals->xvals = true;
  vals->xduals = true;
  vals->xbas = true;
  vals->evals = true;
  vals->eduals = true;
  vals->ebas = true;
  vals->objval = true;
}

void sol_vals_test_sync(struct sol_vals *vals, struct sol_vals_test *solver_dotest)
{
  memcpy(&vals->dotest, solver_dotest, sizeof(struct sol_vals_test));
}


void solve_params_init(struct solver_params *params) 
{
  params->dotest = NULL;
}

static int cmp_var_val(const struct rhp_mdl *mdl, double refval, double val, rhp_idx i, const char* typ)
{
  if (isfinite(refval)) {
    double delta = fabs(val-refval);
    if (delta > TOL_EPSLAX) { printf(ANSI_COLOR_RED); }
    printf("%s.%s: %e: %e vs %e\n", rhp_mdl_printvarname(mdl, i), typ, delta, val, refval);
    if (delta > TOL_EPSLAX) {
      printf(ANSI_COLOR_RESET);
      return 2;
    }
  }

  return 0;
}

static int cmp_equ_val(const struct rhp_mdl *mdl, double refval, double val, rhp_idx i, const char* typ)
{
  if (isfinite(refval)) {
    double delta = fabs(val-refval);
    if (delta > TOL_EPSLAX) { printf(ANSI_COLOR_RED); }
    printf("%s.%s: %e: %e vs %e\n", rhp_mdl_printequname(mdl, i), typ, delta, val, refval);
    if (delta > TOL_EPSLAX) {
      printf(ANSI_COLOR_RESET);
      return 2;
    }
  }

  return 0;
}

static int _cmp_var_bas(const struct rhp_mdl *mdl, enum rhp_basis_status sol, int val, rhp_idx i)
{
  if (sol != RHP_BASIS_UNSET) {
    /* We do not strongly enforce this, too many superbasic issues */
    bool err = (val != sol && val != RHP_BASIS_SUPERBASIC);
    if (err) { printf(ANSI_COLOR_RED); }
    printf("%s.bas: %s vs %s\n", rhp_mdl_printvarname(mdl, i), rhp_basis_str(val), rhp_basis_str(sol));
    if (err) {
      printf(ANSI_COLOR_RESET);
      return 2;
    }
  }

  return 0;
}

static int _cmp_equ_bas(const struct rhp_mdl *mdl, enum rhp_basis_status sol, int val, rhp_idx i)
{
  if (sol != RHP_BASIS_UNSET) {
    bool err = (val != sol && val != RHP_BASIS_SUPERBASIC);
    if (err) printf(ANSI_COLOR_RED);
    printf("%s.bas: %s vs %s\n", rhp_mdl_printequname(mdl, i), rhp_basis_str(val), rhp_basis_str(sol));
    if (err) {
      printf(ANSI_COLOR_RESET);
      return 2;
    }
  }

  return 0;
}

struct rhp_mdl * test_init(void)
{

  struct rhp_mdl *mdl = rhp_mdl_new(RHP_BACKEND_RHP);
  if (!mdl) { return NULL; }

  rhp_set_option_d(mdl, "rtol", TOL_EPS/10.);

  return mdl;
}

void test_fini(void)
{
}

int test_solve(struct rhp_mdl *mdl, struct rhp_mdl *rhp_mdl_solver, struct sol_vals *solvals)
{
  struct rhp_aequ *objequs = rhp_aequ_new();

  unsigned n = rhp_mdl_nvars(mdl);
  unsigned m = rhp_mdl_nequs(mdl);

  int status = rhp_process(mdl, rhp_mdl_solver);
  if (status != RHP_OK) {
    printf("Transforming the problem failed: status = %s (%d)\n ",
        rhp_status_descr(status), status);
    goto _exit;
  }

  status = rhp_solve(rhp_mdl_solver);
  if (status != RHP_OK) {
    printf("Solving the problem failed: status = %s (%d)\n ",
        rhp_status_descr(status), status);
    goto _exit;
  }

  status = rhp_postprocess(rhp_mdl_solver);
  if (status != RHP_OK) {
    printf("Reporting the values failed: status = %s (%d)\n ",
        rhp_status_descr(status), status);
    goto _exit;
  }

  if (rhp_mdl_nvars(mdl) != n) {
    printf("User-visible size changed! current = %u, original = %u\n",
           rhp_mdl_nvars(mdl), n);
    status = 3;
  }

   // TODO: remove the cast once we move to unsigned type

  for (rhp_idx i = 0; i < (rhp_idx)n && i < 10; ++i) {
    double val;
    rhp_mdl_getvarval(mdl, i, &val);
    printf("%s: %e; ", rhp_mdl_printvarname(mdl, i), val);
  }
   if (n >= 10) { puts("..."); } else { puts(""); }

  double *xvals = solvals->xvals;
  double *xduals = solvals->xduals;
  enum rhp_basis_status *xbas = solvals->xbas;
  double *evals = solvals->evals;
  double *eduals = solvals->eduals;
  enum rhp_basis_status *ebas = solvals->ebas;
  struct sol_vals_test *dotest = &solvals->dotest;

   if (xvals && dotest->xvals) {
    for (rhp_idx i = 0; i < (rhp_idx)n; ++i) {
      double refval = xvals[i], val;
      rhp_mdl_getvarval(mdl, i, &val);
      int rc = cmp_var_val(mdl, refval, val, i, "val");
      if (rc != 0) {
        status = rc;
      }
    }
  }

   if (xduals && dotest->xduals) {
    for (rhp_idx i = 0; i < (rhp_idx)n; ++i) {
      double sol = xduals[i], val;
      rhp_mdl_getvarmult(mdl, i, &val);
      int rc = cmp_var_val(mdl, sol, val, i, "dual");
      if (rc != 0) {
        status = rc;
      }
    }
  }

   if (xbas && dotest->xbas) {
    for (rhp_idx i = 0; i < (rhp_idx)n; ++i) {
      enum rhp_basis_status sol = xbas[i];
      int val;

      rhp_mdl_getvarbasis(mdl, i, &val);
      int rc = _cmp_var_bas(mdl, sol, val, i);
      if (rc != 0) {
        status = rc;
      }
    }
  }

  rhp_mdl_getobjequs(mdl, objequs);

   if (evals && dotest->evals) {
    for (rhp_idx i = 0, j = 0; i < (rhp_idx)m; ++i) {
      if (rhp_aequ_contains(objequs, i)) continue;
      double sol = evals[j], val;
      rhp_mdl_getequval(mdl, i, &val);
      int rc = cmp_equ_val(mdl, sol, val, i, "val");
      if (rc != 0) {
        status = rc;
      }
      j++;
    }
  }

   if (eduals && dotest->eduals) {
    for (rhp_idx i = 0, j = 0; i < (rhp_idx)m; ++i) {
      if (rhp_aequ_contains(objequs, i)) continue;
      double sol = eduals[j], val;
      rhp_mdl_getequmult(mdl, i, &val);
      int rc = cmp_equ_val(mdl, sol, val, i, "dual");
      if (rc != 0) {
        status = rc;
      }
      j++;
    }
  }

   if (ebas && dotest->ebas) {
    for (rhp_idx i = 0, j = 0; i < (rhp_idx)m; ++i) {
      if (rhp_aequ_contains(objequs, i)) continue;
      enum rhp_basis_status sol = ebas[j];
      int val;
      RHP_CHK(rhp_mdl_getequbasis(mdl, i, &val));
      int rc = _cmp_equ_bas(mdl, sol, val, i);
      if (rc != 0) {
        status = rc;
      }
      j++;
    }
  }

  unsigned nb_objequs = rhp_aequ_size(objequs);
  if ((nb_objequs == 1) && isfinite(solvals->objval)) {
    double objval;
    rhp_idx objequ;
    rhp_aequ_get(objequs, 0, &objequ);
    RHP_CHK(rhp_mdl_getequval(mdl, objequ, &objval));
    double delta = fabs(solvals->objval - objval);
    if (delta > TOL_EPSLAX) { printf(ANSI_COLOR_RED); }
    printf("objval: %e: %e vs %e\n", delta, objval, solvals->objval);
    if (delta > TOL_EPSLAX) {
      printf(ANSI_COLOR_RESET);
      status = 2;
    }
  } else if (nb_objequs > 1 && solvals->objvals) {
    for (unsigned i = 0; i < nb_objequs; ++i) {
      double objval;
      rhp_idx ei;
      rhp_aequ_get(objequs, i, &ei);
      RHP_CHK(rhp_mdl_getequval(mdl, ei, &objval));
      double delta = fabs(solvals->objvals[i] - objval);
      if (delta > TOL_EPSLAX) { printf(ANSI_COLOR_RED); }
      printf("objval(%u): %e: %e vs %e\n", i, delta, objval, solvals->objvals[i]);
      if (delta > TOL_EPSLAX) {
        printf(ANSI_COLOR_RESET);
      status = 2;
      }
    }
  }



_exit:
  rhp_aequ_free(objequs);
  rhp_mdl_free(mdl);

  if (status == 2) {
    printf("large difference! Stopping\n");
  }

  return status == RHP_OK ? 0 : 1;
}
