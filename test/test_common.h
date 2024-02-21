#ifndef RHP_TEST_COMMON_H
#define RHP_TEST_COMMON_H

#include <assert.h>
#include <stdbool.h>
#include <stdio.h>

#include "compat.h"

struct rhp_mdl;

struct sol_vals_test {
  bool xvals;
  bool xduals;
  bool xbas;
  bool evals;
  bool eduals;
  bool ebas;
  bool objval;
};

struct sol_vals {
  double *xvals;
  double *xduals;
  enum rhp_basis_status *xbas;
  double *evals;
  double *eduals;
  enum rhp_basis_status *ebas;
  union {
    double objval;
    double *objvals;
  };
  struct sol_vals_test dotest;
};

struct solver_params {
  struct sol_vals_test *dotest;
};

#define RHP_CHK(EXPR) { status = EXPR; if (status != RHP_OK) { puts(#EXPR " failed"); assert (status == RHP_OK); goto _exit; } }
#define RESHOP_CHECK(EXPR) RHP_CHK(EXPR)
#define RHP_NONNULL(obj) { if (!(obj)) {puts(#obj " is NULL"); goto _exit;} }


void sol_vals_init(struct sol_vals *vals) NONNULL;
void sol_vals_test_init(struct sol_vals_test *valtest) NONNULL;
void sol_vals_test_sync(struct sol_vals *vals, struct sol_vals_test *solver_dotest) NONNULL;

void solve_params_init(struct solver_params *params) NONNULL;

struct rhp_mdl * test_init(void);
void test_fini(void);
int test_solve(struct rhp_mdl *mdl, struct rhp_mdl *mdl_solver, struct sol_vals *vals);

#endif
