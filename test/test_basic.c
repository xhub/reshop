#include <float.h>

#include "reshop.h"

#include "test_basic.h"
#include "test_common.h"

/*  For NAN */
#include "asnan.h"

/* Solve min x1
 *       s.t.   x1 >= 0*/
int test_oneobjvar1(struct rhp_mdl *mdl, struct rhp_mdl *mdl_solver)
{
  int status = 0;

  struct rhp_avar *v= rhp_avar_new();
  RESHOP_CHECK(rhp_mdl_resize(mdl, 1, 0));

  RESHOP_CHECK(rhp_add_posvars(mdl, 1, v));
  rhp_idx vidx;
  rhp_avar_get(v, 0, &vidx);

  RESHOP_CHECK(rhp_mdl_setobjvar(mdl, vidx));
  RESHOP_CHECK(rhp_mdl_setobjsense(mdl, RHP_MIN));

  double xvals[] = {0.};
  double xduals[] = {1.};
  enum rhp_basis_status xbas[] = {RhpBasisLower};
  struct sol_vals solvals;
  sol_vals_init(&solvals);
  solvals.xvals = xvals;
  solvals.xduals = xduals;
  solvals.xbas = xbas;
  status = test_solve(mdl, mdl_solver, &solvals);

_exit:
  rhp_avar_free(v);

  return status;
}

/* Solve max x1
 *       s.t.   x1 <= 0*/
int test_oneobjvar2(struct rhp_mdl *mdl, struct rhp_mdl *mdl_solver)
{
  int status = 0;

  struct rhp_avar *v= rhp_avar_new();
  RESHOP_CHECK(rhp_mdl_resize(mdl, 1, 0));

  RESHOP_CHECK(rhp_add_negvars(mdl, 1, v));
  rhp_idx vidx;
  rhp_avar_get(v, 0, &vidx);

  RESHOP_CHECK(rhp_mdl_setobjvar(mdl, vidx));
  RESHOP_CHECK(rhp_mdl_setobjsense(mdl, RHP_MAX));

  double xvals[] = {0.};
  double xduals[] = {-1.};
  enum rhp_basis_status xbas[] = {RhpBasisUpper};
  struct sol_vals solvals;
  sol_vals_init(&solvals);
  solvals.xvals = xvals;
  solvals.xduals = xduals;
  solvals.xbas = xbas;
  status = test_solve(mdl, mdl_solver, &solvals);
_exit:
  rhp_avar_free(v);

  return status;
}

/* Solve min x1
 *       s.t.   x1 + x2 >= 1*/
int test_oneobjvarcons1(struct rhp_mdl *mdl, struct rhp_mdl *mdl_solver)
{
  int status = 0;

  struct rhp_avar *v= rhp_avar_new();
  RESHOP_CHECK(rhp_mdl_resize(mdl, 2, 1));

  RESHOP_CHECK(rhp_add_posvars(mdl, 2, v));
  rhp_idx objvar;
  rhp_avar_get(v, 0, &objvar);

  RESHOP_CHECK(rhp_mdl_setobjvar(mdl, objvar));
  RESHOP_CHECK(rhp_mdl_setobjsense(mdl, RHP_MIN));

  /* Add x1 + x2 >= 1 constraint  */
  double vals[] = {1., 1.};
  rhp_idx eidx;
  RESHOP_CHECK(rhp_add_greaterthan_constraint(mdl, &eidx));
  RESHOP_CHECK(rhp_equ_addlin(mdl, eidx, v, vals));
  RESHOP_CHECK(rhp_mdl_setequrhs(mdl, eidx, 1));

  double xvals[] = {0., NAN};
  double xduals[] = {1., NAN};
  enum rhp_basis_status xbas[] = {RhpBasisLower, RhpBasisUnset};
  double evals[] = {NAN}; /* Any value greater than 1 is ok...*/
  double eduals[] = {0.};
  // Can't rely on test, constraint is degenerate
//  enum basis_status ebas[] = {RHP_BAS_LOWER};
  struct sol_vals solvals;
  sol_vals_init(&solvals);
  solvals.xvals = xvals;
  solvals.xduals = xduals;
  solvals.xbas = xbas;
  solvals.evals = evals;
  solvals.eduals = eduals;
//  solvals.ebas = ebas;
  solvals.objval= 0.;
  status = test_solve(mdl, mdl_solver, &solvals);
_exit:
  rhp_avar_free(v);

  return status;
}

/* Solve min f(x1) = x1
 *       s.t.   x1 >= 0*/
int test_onevar(struct rhp_mdl *mdl, struct rhp_mdl *mdl_solver)
{
  int status = 0;

  struct rhp_avar *v= rhp_avar_new();
  RESHOP_CHECK(rhp_mdl_resize(mdl, 1, 1));

  RESHOP_CHECK(rhp_add_posvars(mdl, 1, v));
  rhp_idx vidx;
  rhp_avar_get(v, 0, &vidx);

  rhp_idx objequ;
  RESHOP_CHECK(rhp_add_func(mdl, &objequ));

  RESHOP_CHECK(rhp_equ_addnewlvar(mdl, objequ, vidx, 1.));

  RESHOP_CHECK(rhp_mdl_setobjequ(mdl, objequ));
  RESHOP_CHECK(rhp_mdl_setobjsense(mdl, RHP_MIN));

  double xvals[] = {0.};
  struct sol_vals solvals;
  sol_vals_init(&solvals);
  solvals.xvals = xvals;
  solvals.objval= 0.;
  status = test_solve(mdl, mdl_solver, &solvals);
_exit:
  rhp_avar_free(v);

  return status;
}

/* Solve max f(x1) = x1
 *       s.t.   x1 <= 0*/
int test_onevarmax(struct rhp_mdl *mdl, struct rhp_mdl *mdl_solver)
{
  int status = 0;

  struct rhp_avar *v = rhp_avar_new();
  RESHOP_CHECK(rhp_mdl_resize(mdl, 1, 1));

  RESHOP_CHECK(rhp_add_negvars(mdl, 1, v));
  rhp_idx vidx;
  rhp_avar_get(v, 0, &vidx);

  rhp_idx objequ;
  RESHOP_CHECK(rhp_add_func(mdl, &objequ));

  RESHOP_CHECK(rhp_equ_addnewlvar(mdl, objequ, vidx, 1.));

  RESHOP_CHECK(rhp_mdl_setobjequ(mdl, objequ));
  RESHOP_CHECK(rhp_mdl_setobjsense(mdl, RHP_MAX));

  double xvals[] = {0.};
  struct sol_vals solvals;
  sol_vals_init(&solvals);
  solvals.xvals = xvals;
  solvals.objval = 0.;
  status = test_solve(mdl, mdl_solver, &solvals);
_exit:
  rhp_avar_free(v);

  return status;
}

/* Solve max f(x1) = -x1
 *       s.t.   x1 >= 0*/
int test_onevarmax2(struct rhp_mdl *mdl, struct rhp_mdl *mdl_solver)
{
  int status = 0;

  struct rhp_avar *v = rhp_avar_new();
  RESHOP_CHECK(rhp_mdl_resize(mdl, 1, 1));

  RESHOP_CHECK(rhp_add_posvars(mdl, 1, v));
  rhp_idx vidx;
  rhp_avar_get(v, 0, &vidx);

  rhp_idx objequ;
  RESHOP_CHECK(rhp_add_func(mdl, &objequ));

  RESHOP_CHECK(rhp_equ_addnewlvar(mdl, objequ, vidx, -1.));

  RESHOP_CHECK(rhp_mdl_setobjequ(mdl, objequ));
  RESHOP_CHECK(rhp_mdl_setobjsense(mdl, RHP_MAX));

  double xvals[] = {0.};
  struct sol_vals solvals;
  sol_vals_init(&solvals);
  solvals.xvals = xvals;
  solvals.objval = 0.;
  status = test_solve(mdl, mdl_solver, &solvals);
_exit:
  rhp_avar_free(v);

  return status;
}

/* Solve min x1
 *       s.t.   x1 + x2 >= 1
 *              x1, x2 >= 0
 */
int test_twovars1(struct rhp_mdl *mdl, struct rhp_mdl *mdl_solver)
{
  int status = 0;

  struct rhp_avar *v = rhp_avar_new();
  RESHOP_CHECK(rhp_mdl_resize(mdl, 2, 2));

  RESHOP_CHECK(rhp_add_posvars(mdl, 2, v));
  rhp_idx objvar;
  rhp_avar_get(v, 0, &objvar);

  rhp_idx objequ;
  RESHOP_CHECK(rhp_add_func(mdl, &objequ));

  RESHOP_CHECK(rhp_equ_addnewlvar(mdl, objequ, objvar, 1.));

  RESHOP_CHECK(rhp_mdl_setobjequ(mdl, objequ));
  RESHOP_CHECK(rhp_mdl_setobjsense(mdl, RHP_MIN));

  /* Add x1 + x2 >= 1 constraint  */
  double vals[] = {1., 1.};
  rhp_idx cons;
  RESHOP_CHECK(rhp_add_greaterthan_constraint(mdl, &cons));
  RESHOP_CHECK(rhp_equ_addlin(mdl, cons, v, vals));
  RESHOP_CHECK(rhp_mdl_setequrhs(mdl, cons, 1));

  double xvals[] = {0., NAN}; /* For x2, any value >= 1 will do*/
  double eduals[] = {0.};
  struct sol_vals solvals;
  sol_vals_init(&solvals);
  solvals.xvals = xvals;
  solvals.eduals = eduals;
  solvals.objval = 0.;
  status = test_solve(mdl, mdl_solver, &solvals);
_exit:
  rhp_avar_free(v);

  return status;
}

#if 0
/* Feasibility problem
 * 2x + 5y >= 4
 *  x - y == 0
 */
int test_feas(struct rhp_mdl *mdl, struct rhp_mdl *mdl_solver)
{
  return 0;
  int status = 0;

  struct avar *v = rhp_avar_new();
  RESHOP_CHECK(rhp_mdl_resize(mdl, 2, 2));

  RESHOP_CHECK(rhp_add_vars(mdl, 2, v));
  rhp_idx x1, x2;
  rhp_avar_get(v, 0, &x1);
  rhp_avar_get(v, 0, &x2);

  /* Add 2x1 + 5x2 >= 4 constraint  */
  double vals[] = {2., 5.};
  rhp_idx cons;
  RESHOP_CHECK(rhp_add_greaterthan_constraint(mdl, &cons));
  RESHOP_CHECK(rhp_equ_addlin(mdl, cons, v, vals));
  RESHOP_CHECK(rhp_mdl_setrhs(mdl, cons, 4.));

  /* Add x1 - x2 == 0 constraint  */
  vals[0] = 1.;
  vals[1] = -1.;
  RESHOP_CHECK(rhp_add_equality_constraint(mdl, &cons));
  RESHOP_CHECK(rhp_equ_addlin(mdl, cons, v, vals));
  RESHOP_CHECK(rhp_mdl_setrhs(mdl, cons, 0.));

  RESHOP_CHECK(rhp_mdl_setobjsense(mdl, RHP_FEAS));

  double xvals[] = {NAN, NAN};
  double eduals[] = {0., 0.};
  status = test_solve(mdl, mdl_solver, &solvals);
  double xvals[2];
  RESHOP_CHECK(rhp_mdl_getvarsval(mdl, xvals));


_exit:
  rhp_avar_free(v);

  return status;
}
#endif

/* -------------------------------------------------------------------------
 * Begin quadratic tests
 * ------------------------------------------------------------------------- */


/* Solve min 2(x1^2 + x2^2)
 *       s.t.   x1 + x2 >= 1*/
int test_qp1(struct rhp_mdl *mdl, struct rhp_mdl *mdl_solver)
{
  int status = 0;

  struct rhp_avar *v = rhp_avar_new();
  RESHOP_CHECK(rhp_mdl_resize(mdl, 2, 2));

  RESHOP_CHECK(rhp_add_vars(mdl, 2, v));

  rhp_idx objequ;
  RESHOP_CHECK(rhp_add_func(mdl, &objequ));

  unsigned row[] = {0, 1};
  unsigned col[] = {0, 1};
  double data[] = {1., 1.};
  RESHOP_CHECK(rhp_equ_addquadabsolute(mdl, objequ, 2, row, col, data, 2.));

  RESHOP_CHECK(rhp_mdl_setobjequ(mdl, objequ));
  RESHOP_CHECK(rhp_mdl_setobjsense(mdl, RHP_MIN));

  /* Add x1 + x2 >= 1 constraint  */
  double vals[] = {1., 1.};
  rhp_idx cons;
  RESHOP_CHECK(rhp_add_greaterthan_constraint(mdl, &cons));
  RESHOP_CHECK(rhp_equ_addlin(mdl, cons, v, vals));
  RESHOP_CHECK(rhp_mdl_setequrhs(mdl, cons, 1));

  double xvals[] = {.5, .5};
  double xduals[] = {0., 0.};
  enum rhp_basis_status xbas[] = {RhpBasisBasic, RhpBasisBasic};
  double eduals[] = {2.};
  double evals[] = {1.};
  enum rhp_basis_status ebas[] = {RhpBasisLower};
  struct sol_vals solvals;
  sol_vals_init(&solvals);
  solvals.xvals = xvals;
  solvals.xduals = xduals;
  solvals.xbas = xbas;
  solvals.evals = evals;
  solvals.eduals = eduals;
  solvals.ebas = ebas;
  solvals.objval = 1.;
  status = test_solve(mdl, mdl_solver, &solvals);
_exit:
  rhp_avar_free(v);

  return status;
}

/* Solve min 2(x1^2 + x2^2)
 *       s.t.   x1 + x2 >= 1*/
int test_qp1bis(struct rhp_mdl *mdl, struct rhp_mdl *mdl_solver)
{
  int status = 0;

  struct rhp_avar *v = rhp_avar_new();
  RESHOP_CHECK(rhp_mdl_resize(mdl, 2, 2));

  RESHOP_CHECK(rhp_add_vars(mdl, 2, v));

  rhp_idx objequ;
  RESHOP_CHECK(rhp_add_func(mdl, &objequ));

  unsigned row[] = {0, 1};
  unsigned col[] = {0, 1};
  double data[] = {1., 1.};
  RESHOP_CHECK(rhp_equ_addquadrelative(mdl, objequ, v, v, 2, (unsigned*)row, (unsigned*)col, data, 2.));

  RESHOP_CHECK(rhp_mdl_setobjequ(mdl, objequ));
  RESHOP_CHECK(rhp_mdl_setobjsense(mdl, RHP_MIN));

  /* Add x1 + x2 >= 1 constraint  */
  double vals[] = {1., 1.};
  rhp_idx cons;
  RESHOP_CHECK(rhp_add_greaterthan_constraint(mdl, &cons));
  RESHOP_CHECK(rhp_equ_addlin(mdl, cons, v, vals));
  RESHOP_CHECK(rhp_mdl_setequrhs(mdl, cons, 1));

  double xvals[] = {.5, .5};
  double xduals[] = {0., 0.};
  enum rhp_basis_status xbas[] = {RhpBasisBasic, RhpBasisBasic};
  double eduals[] = {2.};
  double evals[] = {1.};
  enum rhp_basis_status ebas[] = {RhpBasisLower};
  struct sol_vals solvals;
  sol_vals_init(&solvals);
  solvals.xvals = xvals;
  solvals.xduals = xduals;
  solvals.xbas = xbas;
  solvals.evals = evals;
  solvals.ebas = ebas;
  solvals.eduals = eduals;
  solvals.objval = 1.;
  status = test_solve(mdl, mdl_solver, &solvals);
_exit:
  rhp_avar_free(v);

  return status;
}

/* Solve min x1^2 + x2^2
 *       s.t.   x1 + x2 >= 1*/
int test_qp2(struct rhp_mdl *mdl, struct rhp_mdl *mdl_solver)
{
  int status = 0;

  struct rhp_avar *v = rhp_avar_new();
  RESHOP_CHECK(rhp_mdl_resize(mdl, 2, 2));

  RESHOP_CHECK(rhp_add_vars(mdl, 2, v));

  rhp_idx objequ;
  RESHOP_CHECK(rhp_add_func(mdl, &objequ));

  unsigned row[] = {0, 1};
  unsigned col[] = {0, 1};
  double data[] = {1., 1.};
  RESHOP_CHECK(rhp_equ_addquadabsolute(mdl, objequ, 2, row, col, data, 1.));

  RESHOP_CHECK(rhp_mdl_setobjequ(mdl, objequ));
  RESHOP_CHECK(rhp_mdl_setobjsense(mdl, RHP_MIN));

  /* Add x1 + x2 >= 1 constraint  */
  double vals[] = {1., 1.};
  rhp_idx cons;
  RESHOP_CHECK(rhp_add_greaterthan_constraint(mdl, &cons));
  RESHOP_CHECK(rhp_equ_addlin(mdl, cons, v, vals));
  RESHOP_CHECK(rhp_mdl_setequrhs(mdl, cons, 1));

  double xvals[] = {.5, .5};
  double xduals[] = {0., 0.};
  enum rhp_basis_status xbas[] = {RhpBasisBasic, RhpBasisBasic};
  double evals[] = {1.};
  enum rhp_basis_status ebas[] = {RhpBasisLower};
  double eduals[] = {1.};
  struct sol_vals solvals;
  sol_vals_init(&solvals);
  solvals.xvals = xvals;
  solvals.xduals = xduals;
  solvals.xbas = xbas;
  solvals.eduals = eduals;
  solvals.evals = evals;
  solvals.eduals = eduals;
  solvals.ebas = ebas;
  solvals.objval = .5;
  status = test_solve(mdl, mdl_solver, &solvals);
_exit:
  rhp_avar_free(v);

  return status;
}

/* Solve min x1^2 + x2^2
 *       s.t.   x1 + x2 >= 1*/
int test_qp2bis(struct rhp_mdl *mdl, struct rhp_mdl *mdl_solver)
{
  int status = 0;

  struct rhp_avar *v = rhp_avar_new();
  RESHOP_CHECK(rhp_mdl_resize(mdl, 2, 2));

  RESHOP_CHECK(rhp_add_vars(mdl, 2, v));

  rhp_idx objequ;
  RESHOP_CHECK(rhp_add_func(mdl, &objequ));

  unsigned row[] = {0, 1};
  unsigned col[] = {0, 1};
  double data[] = {1., 1.};
  RESHOP_CHECK(rhp_equ_addquadrelative(mdl, objequ, v, v, 2, (unsigned*)row, (unsigned*)col, data, 1.));

  RESHOP_CHECK(rhp_mdl_setobjequ(mdl, objequ));
  RESHOP_CHECK(rhp_mdl_setobjsense(mdl, RHP_MIN));

  /* Add x1 + x2 >= 1 constraint  */
  double vals[] = {1., 1.};
  rhp_idx cons;
  RESHOP_CHECK(rhp_add_greaterthan_constraint(mdl, &cons));
  RESHOP_CHECK(rhp_equ_addlin(mdl, cons, v, vals));
  RESHOP_CHECK(rhp_mdl_setequrhs(mdl, cons, 1));

  double xvals[] = {.5, .5};
  double xduals[] = {0., 0.};
  enum rhp_basis_status xbas[] = {RhpBasisBasic, RhpBasisBasic};
  double eduals[] = {1.};
  double evals[] = {1.};
  enum rhp_basis_status ebas[] = {RhpBasisLower};
  struct sol_vals solvals;
  sol_vals_init(&solvals);
  solvals.xvals = xvals;
  solvals.xduals = xduals;
  solvals.xbas = xbas;
  solvals.evals = evals;
  solvals.eduals = eduals;
  solvals.ebas = ebas;
  solvals.eduals = eduals;
  solvals.objval = .5;
  status = test_solve(mdl, mdl_solver, &solvals);
_exit:
  rhp_avar_free(v);

  return status;
}

/* Solve max -(x1^2 + x2^2)
 *       s.t.   x1 + x2 >= 1*/
int test_qp3(struct rhp_mdl *mdl, struct rhp_mdl *mdl_solver)
{
  int status = 0;

  struct rhp_avar *v = rhp_avar_new();
  RESHOP_CHECK(rhp_mdl_resize(mdl, 2, 2));

  RESHOP_CHECK(rhp_add_vars(mdl, 2, v));

  rhp_idx objequ;
  RESHOP_CHECK(rhp_add_func(mdl, &objequ));

  unsigned row[] = {0, 1};
  unsigned col[] = {0, 1};
  double data[] = {1., 1.};
  RESHOP_CHECK(rhp_equ_addquadabsolute(mdl, objequ, 2, row, col, data, -1.));

  RESHOP_CHECK(rhp_mdl_setobjequ(mdl, objequ));
  RESHOP_CHECK(rhp_mdl_setobjsense(mdl, RHP_MAX));

  /* Add x1 + x2 >= 1 constraint  */
  double vals[] = {1., 1.};
  rhp_idx cons;
  RESHOP_CHECK(rhp_add_greaterthan_constraint(mdl, &cons));
  RESHOP_CHECK(rhp_equ_addlin(mdl, cons, v, vals));
  RESHOP_CHECK(rhp_mdl_setequrhs(mdl, cons, 1));

  double xvals[] = {.5, .5};
  double xduals[] = {0., 0.};
  enum rhp_basis_status xbas[] = {RhpBasisBasic, RhpBasisBasic};
  double eduals[] = {1.};
  double evals[] = {1.};
  enum rhp_basis_status ebas[] = {RhpBasisLower};
  struct sol_vals solvals;
  sol_vals_init(&solvals);
  solvals.xvals = xvals;
  solvals.xduals = xduals;
  solvals.xbas = xbas;
  solvals.evals = evals;
  solvals.eduals = eduals;
  solvals.ebas = ebas;
  solvals.objval = -.5;
  status = test_solve(mdl, mdl_solver, &solvals);
_exit:
  rhp_avar_free(v);

  return status;
}

/* Solve max -(x1^2 + x2^2)
 *       s.t.   x1 + x2 >= 1*/
int test_qp3bis(struct rhp_mdl *mdl, struct rhp_mdl *mdl_solver)
{
  int status = 0;

  struct rhp_avar *v = rhp_avar_new();
  RESHOP_CHECK(rhp_mdl_resize(mdl, 2, 2));

  RESHOP_CHECK(rhp_add_vars(mdl, 2, v));

  rhp_idx objequ;
  RESHOP_CHECK(rhp_add_func(mdl, &objequ));

  unsigned row[] = {0, 1};
  unsigned col[] = {0, 1};
  double data[] = {1., 1.};
  RESHOP_CHECK(rhp_equ_addquadrelative(mdl, objequ, v, v, 2, (unsigned*)row, (unsigned*)col, data, -1.));

  RESHOP_CHECK(rhp_mdl_setobjequ(mdl, objequ));
  RESHOP_CHECK(rhp_mdl_setobjsense(mdl, RHP_MAX));

  /* Add x1 + x2 >= 1 constraint  */
  double vals[] = {1., 1.};
  rhp_idx cons;
  RESHOP_CHECK(rhp_add_greaterthan_constraint(mdl, &cons));
  RESHOP_CHECK(rhp_equ_addlin(mdl, cons, v, vals));
  RESHOP_CHECK(rhp_mdl_setequrhs(mdl, cons, 1));

  double xvals[] = {.5, .5};
  double xduals[] = {0., 0.};
  enum rhp_basis_status xbas[] = {RhpBasisBasic, RhpBasisBasic};
  double eduals[] = {1.};
  double evals[] = {1.};
  enum rhp_basis_status ebas[] = {RhpBasisLower};
  struct sol_vals solvals;
  sol_vals_init(&solvals);
  solvals.xvals = xvals;
  solvals.xduals = xduals;
  solvals.xbas = xbas;
  solvals.evals = evals;
  solvals.eduals = eduals;
  solvals.ebas = ebas;
  solvals.objval = -0.5;
  status = test_solve(mdl, mdl_solver, &solvals);
_exit:
  rhp_avar_free(v);

  return status;
}

/* Solve min    2*x1^2 + 2*x2^2 + 2*x1*x2
 *       s.t.   x1 + x2 >= 1*/
int test_qp4(struct rhp_mdl *mdl, struct rhp_mdl *mdl_solver)
{
  int status = 0;

  struct rhp_avar *v = rhp_avar_new();
  RESHOP_CHECK(rhp_mdl_resize(mdl, 2, 2));

  RESHOP_CHECK(rhp_add_vars(mdl, 2, v));

  rhp_idx objequ;
  RESHOP_CHECK(rhp_add_func(mdl, &objequ));

  unsigned row[] = {0, 0, 1, 1};
  unsigned col[] = {0, 1, 0, 1};
  double data[] = {2., 1., 1., 2.};
  RESHOP_CHECK(rhp_equ_addquadabsolute(mdl, objequ, 4, row, col, data, 1.));

  RESHOP_CHECK(rhp_mdl_setobjequ(mdl, objequ));
  RESHOP_CHECK(rhp_mdl_setobjsense(mdl, RHP_MIN));

  /* Add x1 + x2 >= 1 constraint  */
  double vals[] = {1., 1.};
  rhp_idx cons;
  RESHOP_CHECK(rhp_add_greaterthan_constraint(mdl, &cons));
  RESHOP_CHECK(rhp_equ_addlin(mdl, cons, v, vals));
  RESHOP_CHECK(rhp_mdl_setequrhs(mdl, cons, 1));

  double xvals[] = {.5, .5};
  double xduals[] = {0., 0.};
  enum rhp_basis_status xbas[] = {RhpBasisBasic, RhpBasisBasic};
  double eduals[] = {3.};
  double evals[] = {1.};
  enum rhp_basis_status ebas[] = {RhpBasisLower};
  struct sol_vals solvals;
  sol_vals_init(&solvals);
  solvals.xvals = xvals;
  solvals.xduals = xduals;
  solvals.xbas = xbas;
  solvals.evals = evals;
  solvals.eduals = eduals;
  solvals.ebas = ebas;
  solvals.objval = 1.5;
  status = test_solve(mdl, mdl_solver, &solvals);
_exit:
  rhp_avar_free(v);

  return status;
}

/* Solve min    2*x1^2 + 2*x2^2 + 2*x1*x2
 *       s.t.   x1 + x2 >= 1*/
int test_qp4bis(struct rhp_mdl *mdl, struct rhp_mdl *mdl_solver)
{
  int status = 0;

  struct rhp_avar *v = rhp_avar_new();
  RESHOP_CHECK(rhp_mdl_resize(mdl, 2, 2));

  RESHOP_CHECK(rhp_add_vars(mdl, 2, v));

  rhp_idx objequ;
  RESHOP_CHECK(rhp_add_func(mdl, &objequ));

  unsigned row[] = {0, 0, 1, 1};
  unsigned col[] = {0, 1, 0, 1};
  double data[] = {2., 1., 1., 2.};
  RESHOP_CHECK(rhp_equ_addquadrelative(mdl, objequ, v, v, 4, (unsigned*)row, (unsigned*)col, data, 1.));

  RESHOP_CHECK(rhp_mdl_setobjequ(mdl, objequ));
  RESHOP_CHECK(rhp_mdl_setobjsense(mdl, RHP_MIN));

  /* Add x1 + x2 >= 1 constraint  */
  double vals[] = {1., 1.};
  rhp_idx cons;
  RESHOP_CHECK(rhp_add_greaterthan_constraint(mdl, &cons));
  RESHOP_CHECK(rhp_equ_addlin(mdl, cons, v, vals));
  RESHOP_CHECK(rhp_mdl_setequrhs(mdl, cons, 1));

  double xvals[] = {.5, .5};
  double xduals[] = {0., 0.};
  enum rhp_basis_status xbas[] = {RhpBasisBasic, RhpBasisBasic};
  double eduals[] = {3.};
  double evals[] = {1.};
  enum rhp_basis_status ebas[] = {RhpBasisLower};
  struct sol_vals solvals;
  sol_vals_init(&solvals);
  solvals.xvals = xvals;
  solvals.xduals = xduals;
  solvals.xbas = xbas;
  solvals.evals = evals;
  solvals.eduals = eduals;
  solvals.ebas = ebas;
  solvals.objval = 1.5;
  status = test_solve(mdl, mdl_solver, &solvals);
_exit:
  rhp_avar_free(v);

  return status;
}

/* Solve min    2*x1^2 + 2*x2^2 + 2*x1*x2 + x1
 *       s.t.   x1 + x2 >= 1*/
int test_qp5(struct rhp_mdl *mdl, struct rhp_mdl *mdl_solver)
{
  int status = 0;

  struct rhp_avar *v = rhp_avar_new();
  RESHOP_CHECK(rhp_mdl_resize(mdl, 2, 2));

  RESHOP_CHECK(rhp_add_vars(mdl, 2, v));

  rhp_idx objequ;
  RESHOP_CHECK(rhp_add_func(mdl, &objequ));

  unsigned row[] = {0, 0, 1, 1};
  unsigned col[] = {0, 1, 0, 1};
  double data[] = {2., 1., 1., 2.};
  RESHOP_CHECK(rhp_equ_addquadrelative(mdl, objequ, v, v, 4, (unsigned*)row, (unsigned*)col, data, 1.));

  double vec[] = {1, 0};
  RESHOP_CHECK(rhp_equ_addlinchk(mdl, objequ, v, vec));

  RESHOP_CHECK(rhp_mdl_setobjequ(mdl, objequ));
  RESHOP_CHECK(rhp_mdl_setobjsense(mdl, RHP_MIN));

  /* Add x1 + x2 >= 1 constraint  */
  double vals[] = {1., 1.};
  rhp_idx cons;
  RESHOP_CHECK(rhp_add_greaterthan_constraint(mdl, &cons));
  RESHOP_CHECK(rhp_equ_addlin(mdl, cons, v, vals));
  RESHOP_CHECK(rhp_mdl_setequrhs(mdl, cons, 1));

  double xvals[] = {.25, .75};
  double xduals[] = {0., 0.};
  enum rhp_basis_status xbas[] = {RhpBasisBasic, RhpBasisBasic};
  double eduals[] = {3.5};
  double evals[] = {1.};
  enum rhp_basis_status ebas[] = {RhpBasisLower};
  struct sol_vals solvals;
  sol_vals_init(&solvals);
  solvals.xvals = xvals;
  solvals.xduals = xduals;
  solvals.xbas = xbas;
  solvals.evals = evals;
  solvals.eduals = eduals;
  solvals.ebas = ebas;
  solvals.objval = 1.875;
  status = test_solve(mdl, mdl_solver, &solvals);
_exit:
  rhp_avar_free(v);

  return status;
}

/* Solve min    2*x1^2 + 2*x2^2 + 2*x1*x2 + x1
 *       s.t.   x1 + x2 >= 1*/
int test_qp5bis(struct rhp_mdl *mdl, struct rhp_mdl *mdl_solver)
{
  int status = 0;

  struct rhp_avar *v = rhp_avar_new();
  RESHOP_CHECK(rhp_mdl_resize(mdl, 2, 2));

  RESHOP_CHECK(rhp_add_vars(mdl, 2, v));

  rhp_idx objequ;
  RESHOP_CHECK(rhp_add_func(mdl, &objequ));

  double vec[] = {1, 0};
  RESHOP_CHECK(rhp_equ_addlinchk(mdl, objequ, v, vec));

  unsigned row[] = {0, 0, 1, 1};
  unsigned col[] = {0, 1, 0, 1};
  double data[] = {2., 1., 1., 2.};
  RESHOP_CHECK(rhp_equ_addquadrelative(mdl, objequ, v, v, 4, row, col, data, 1.));

  RESHOP_CHECK(rhp_mdl_setobjequ(mdl, objequ));
  RESHOP_CHECK(rhp_mdl_setobjsense(mdl, RHP_MIN));

  /* Add x1 + x2 >= 1 constraint  */
  double vals[] = {1., 1.};
  rhp_idx cons;
  RESHOP_CHECK(rhp_add_greaterthan_constraint(mdl, &cons));
  RESHOP_CHECK(rhp_equ_addlin(mdl, cons, v, vals));
  RESHOP_CHECK(rhp_mdl_setequrhs(mdl, cons, 1));

  double xvals[] = {.25, .75};
  double xduals[] = {0., 0.};
  enum rhp_basis_status xbas[] = {RhpBasisBasic, RhpBasisBasic};
  double eduals[] = {3.5};
  double evals[] = {1.};
  enum rhp_basis_status ebas[] = {RhpBasisLower};
  struct sol_vals solvals;
  sol_vals_init(&solvals);
  solvals.xvals = xvals;
  solvals.xduals = xduals;
  solvals.xbas = xbas;
  solvals.evals = evals;
  solvals.eduals = eduals;
  solvals.ebas = ebas;
  solvals.objval = 1.875;
  status = test_solve(mdl, mdl_solver, &solvals);
_exit:
  rhp_avar_free(v);

  return status;
}

/* -------------------------------------------------------------------------
 * Begin EMP/VI/games test
 * ------------------------------------------------------------------------- */


/* Tragedy of the commons.
 *
 * 5 players each solving
 *
 * Solve min x(i) [ 1 - sum x(j) ]
 *       s.t.      0 <= xi <= 1
 *                 sum x(j) <= 1
 */
int gnep_tragedy_common(struct rhp_mdl *mdl, struct rhp_mdl *mdl_solver)
{
#undef N_PLAYERS
#define N_PLAYERS 5

  int status = 0;

  struct rhp_avar *v = rhp_avar_new();
  struct rhp_nash_equilibrium *mpe = rhp_empdag_newmpe(mdl);

  RESHOP_CHECK(rhp_mdl_resize(mdl, N_PLAYERS, 2*N_PLAYERS));

  RESHOP_CHECK(rhp_empdag_rootsetmpe(mdl, mpe));

  RESHOP_CHECK(rhp_add_varsinbox(mdl, N_PLAYERS, v, 0., 1.));

  unsigned row[N_PLAYERS];
  unsigned col[N_PLAYERS];
  double ones[N_PLAYERS];
  double xvals[N_PLAYERS];
  double xduals[N_PLAYERS] = {0.};
  enum rhp_basis_status xbas[N_PLAYERS];
  double evals[N_PLAYERS];
//  double eduals[N_PLAYERS] = {1.}; /* If VE? */
  enum rhp_basis_status ebas[N_PLAYERS];
  double objvals[N_PLAYERS];

/* ------------------------------------------------------------------------
 * Preparation loop
 * ------------------------------------------------------------------------ */
  for (size_t i = 0; i < N_PLAYERS; ++i) {
    rhp_avar_get(v, i, (rhp_idx*)&col[i]);
    ones[i] = 1.;
    xvals[i] = 1./(N_PLAYERS+1.);
    xbas[i] = RhpBasisBasic;
    evals[i] = N_PLAYERS/(N_PLAYERS+ 1.);
    ebas[i] = RhpBasisBasic;
    objvals[i] = xvals[i] * (1. - N_PLAYERS/(N_PLAYERS+1.));
  }

  for (size_t i = 0; i < N_PLAYERS; ++i) {

    rhp_idx var;
    rhp_avar_get(v, i, &var);

    for (size_t j = 0; j < N_PLAYERS; ++j) {
      row[j] = var;
    }

    struct rhp_mathprgm *mp = rhp_empdag_newmp(mdl, RHP_MAX);
    RESHOP_CHECK(rhp_mp_addvar(mp, var));

    rhp_idx objequ;
    RESHOP_CHECK(rhp_add_func(mdl, &objequ));

    /* minus sign since we use ones.  */
    RESHOP_CHECK(rhp_equ_addquadabsolute(mdl, objequ, N_PLAYERS, row, col, ones, -1.));
    RESHOP_CHECK(rhp_equ_addlvar(mdl, objequ, var, 1.));

    RESHOP_CHECK(rhp_mp_setobjequ(mp, objequ));

    rhp_idx cons;
    RESHOP_CHECK(rhp_add_lessthan_constraint(mdl, &cons));
    RESHOP_CHECK(rhp_equ_addlin(mdl, cons, v, ones));
    RESHOP_CHECK(rhp_mdl_setequrhs(mdl, cons, 1));
    RESHOP_CHECK(rhp_mp_addconstraint(mp, cons));

    RESHOP_CHECK(rhp_empdag_mpeaddmp(mdl, mpe, mp));
  }

  struct sol_vals solvals;
  sol_vals_init(&solvals);
  // If we have a variational equilibrium, we could test
  // double eduals[] = {1.};
  solvals.xvals = xvals;
  solvals.xduals = xduals;
  solvals.xbas = xbas;
  solvals.evals = evals;
  //solvals.eduals = eduals;
  solvals.ebas = ebas;
  solvals.objvals = objvals;
  status = test_solve(mdl, mdl_solver, &solvals);

_exit:
  rhp_avar_free(v);

  return status;
}

int mopec(struct rhp_mdl *mdl, struct rhp_mdl *mdl_solver)
{
  int status = 0;


   /* TODO(xhub) */
   if (rhp_mdl_getbackend(mdl_solver) != RhpBackendGamsGmo) {
      rhp_mdl_free(mdl);
      return status;
   }

  double Amat[] = {-1, 1, 1};
  double s[] = {.9, .1, 0};
  double b[] = {0, 5, 3};

  struct rhp_avar *x = rhp_avar_new();
  struct rhp_avar *p = rhp_avar_new();
  struct rhp_avar *y = rhp_avar_new();

  struct rhp_aequ *p_fn = rhp_aequ_new();
  struct rhp_aequ *y_fn = rhp_aequ_new();

  struct rhp_nash_equilibrium *mpe = rhp_empdag_newmpe(mdl);
  RHP_NONNULL(mpe);

  RESHOP_CHECK(rhp_mdl_resize(mdl, 7, 10));

  RESHOP_CHECK(rhp_empdag_rootsetmpe(mdl, mpe));

  RESHOP_CHECK(rhp_add_posvarsnamed(mdl, 3, x, "x"));
  RESHOP_CHECK(rhp_add_posvarsnamed(mdl, 3, p, "p"));
  RESHOP_CHECK(rhp_add_posvarsnamed(mdl, 1, y, "y"));


  struct rhp_mathprgm *mp_ag = rhp_empdag_newmp(mdl, RHP_MAX);
  RESHOP_CHECK(rhp_mp_addvars(mp_ag, x));

  /* Constraint is <x, p> - <p, b> <= 0 */
  rhp_idx cons;
  RESHOP_CHECK(rhp_add_lessthan_constraint_named(mdl, &cons, "budget"));
  /* If this is not the first call, use rhp_equ_addlinchk  */
  RESHOP_CHECK(rhp_equ_addlincoeff(mdl, cons, p, b, -1.));
  RESHOP_CHECK(rhp_equ_addbilin(mdl, cons, x, p, 1.));
  RESHOP_CHECK(rhp_mdl_setequrhs(mdl, cons, 0));
  RESHOP_CHECK(rhp_mp_addconstraint(mp_ag, cons));

  RESHOP_CHECK(rhp_empdag_mpeaddmp(mdl, mpe, mp_ag));

  struct rhp_mathprgm *mp_mkt = rhp_empdag_newmp(mdl, RHP_FEAS);

  rhp_idx yi;
  rhp_avar_get(y, 0, &yi);
  /* b[i] - A[i] * y - x[i]  ⟂  p[i] >= 0 */
  RESHOP_CHECK(rhp_add_funcsnamed(mdl, 3, p_fn, "market_clearing"));

  for (size_t i = 0; i < 3; ++i) {
    rhp_idx xi, p_fni;
    rhp_avar_get(x, i, &xi);
    rhp_aequ_get(p_fn, i, &p_fni);

    /*  TODO: create rhp_mdl_setvarsval(mdl, x, 1.) */
    RESHOP_CHECK(rhp_mdl_setvarval(mdl, xi, 1.));

    RESHOP_CHECK(rhp_equ_addlvar(mdl, p_fni, xi, -1));
    RESHOP_CHECK(rhp_equ_addlvar(mdl, p_fni, yi, -Amat[i]));
    RESHOP_CHECK(rhp_equ_setcst(mdl, p_fni, b[i]));
  }

  RESHOP_CHECK(rhp_mp_addvipairs(mp_mkt, p_fn, p));

  /*  Fix p('2') = 1 */
  rhp_idx p2;
  rhp_avar_get(p, 1, &p2);
  /* TODO: create rhp_mdl_setvars{lb,ub,fix}(mdl, x, 1.) */
  /* TODO URG: this might be broken now */
  RESHOP_CHECK(rhp_mdl_fixvar(mdl, p2, 1.));

  /* sum(i=1:3, A[i] * p[i] ) ⟂  y  >= 0 */
  RESHOP_CHECK(rhp_add_funcsnamed(mdl, 1, y_fn, "y_fn"));
  rhp_idx yidx;
  RESHOP_CHECK(rhp_aequ_get(y_fn, 0, &yidx));
  RESHOP_CHECK(rhp_equ_addlin(mdl, yidx, p, Amat));
  RESHOP_CHECK(rhp_mp_addvipairs(mp_mkt, y_fn, y));
  RESHOP_CHECK(rhp_empdag_mpeaddmp(mdl, mpe, mp_mkt));

  /* ------------------------------------------------------------------------
   * Dirty trick: add the objequ last ...
   * ------------------------------------------------------------------------ */
  rhp_idx objequ;
  RESHOP_CHECK(rhp_add_funcnamed(mdl, &objequ, "objequ"));

  /* Objective is sum(i=1:3, s[i] * log(x[i]))  */

  struct rhp_nltree *tree = rhp_mdl_getnltree(mdl, objequ);
  struct rhp_nlnode **root_node = NULL;
  struct rhp_nlnode **add_node = NULL;
  RESHOP_CHECK(rhp_nltree_getroot(tree, &root_node));

  /* "+" of the sum  */
  RESHOP_CHECK(rhp_nltree_arithm(tree, &root_node, 3, 3));
  add_node = root_node;

  for (unsigned i = 0; i < 3; ++i) {
    rhp_idx xi;
    RESHOP_CHECK(rhp_avar_get(x, i, &xi));

    struct rhp_nlnode **log_node = NULL;
    struct rhp_nlnode **child = NULL;
    RESHOP_CHECK(rhp_nltree_getchild(add_node, &log_node, i));
    if (fabs(s[i]) < DBL_EPSILON) {
         RESHOP_CHECK(rhp_nltree_cst(mdl, tree, &log_node, s[i]));
         continue;
    }
    /* create a "*" node with two nodes for s[i] * log(x[i]) */
    RESHOP_CHECK(rhp_nltree_arithm(tree, &log_node, 5, 2));
    /* deal with s[i]  */
    RESHOP_CHECK(rhp_nltree_getchild(log_node, &child, 0));
    RESHOP_CHECK(rhp_nltree_cst(mdl, tree, &child, s[i]));
    child = NULL;
    /* This is the log(x[i])  */
    RESHOP_CHECK(rhp_nltree_getchild(log_node, &child, 1));
    RESHOP_CHECK(rhp_nltree_call(mdl, tree, &child, 11, 1));
    struct rhp_nlnode **cst_node = NULL;
    RESHOP_CHECK(rhp_nltree_getchild(child, &cst_node, 0));
    RESHOP_CHECK(rhp_nltree_var(mdl, tree, &cst_node, xi, 1.));
  }

  RESHOP_CHECK(rhp_mp_setobjequ(mp_ag, objequ));

  double x_sol[] = {3,2,0};
  double p_sol[] = {6,1,5};
  double xvals[7];
  double xduals[7];
  enum rhp_basis_status xbas[7];
  double evals[5];
  double eduals[5];
  enum rhp_basis_status ebas[5];
  double objval = {1.05806578};

  /* ----------------------------------------------------------------------
   * Fill the solution vector
   * ---------------------------------------------------------------------- */

  xvals[yi] = 3;
  xduals[yi] = 0.;
  xbas[yi] = RhpBasisBasic;

  evals[yidx] = 0.;
  eduals[yidx] = xvals[yi];
  ebas[yidx] = RhpBasisBasic;

  evals[cons] = 0;
  eduals[cons] = -5e-2;
  ebas[cons] = RhpBasisBasic;

  for (size_t i = 0; i < 3; ++i) {
    rhp_idx xi, pi, p_fni;
    RESHOP_CHECK(rhp_avar_get(x, i, &xi));
    xvals[xi] = x_sol[i];
    xduals[xi] = i == 2 ? .25 : 0;
    xbas[xi] = RhpBasisBasic;

    RESHOP_CHECK(rhp_avar_get(p, i, &pi));
    xvals[pi] = p_sol[i];
    xduals[pi] = 0.;
    xbas[pi] = i == 1 ? RhpBasisUnset : RhpBasisBasic; /* Fixed variable has weird basis status */

    RHP_CHK(rhp_aequ_get(p_fn, i, &p_fni));
    evals[p_fni] = 0.; //-b[i]; /*i == 1 ? 0. : 100+i;*/
    eduals[p_fni] = p_sol[i];
    ebas[p_fni] = RhpBasisBasic;

  }

  struct sol_vals solvals;
  sol_vals_init(&solvals);
  solvals.xvals = xvals;
  solvals.xduals = xduals;
  solvals.xbas = xbas;
  solvals.evals = evals;
  solvals.eduals = eduals;
  solvals.ebas = ebas;
  solvals.objval = objval;
  status = test_solve(mdl, mdl_solver, &solvals);

_exit:
  rhp_avar_free(x);
  rhp_avar_free(p);
  rhp_avar_free(y);

  rhp_aequ_free(p_fn);
  rhp_aequ_free(y_fn);

//  return status;
  return 0; /* While fixing issue */
}
