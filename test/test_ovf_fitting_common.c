#include <math.h>
#include <stdint.h>
#include <stdio.h>

#if defined(_WIN32) || defined(_WIN64)
//  #define snprintf _snprintf
//  #define vsnprintf _vsnprintf
  #define strcasecmp _stricmp
  #define strncasecmp _strnicmp
  #define strdup _strdup
#include <string.h>

#else
#include <strings.h>

#endif

#include "reshop.h"

#include "colors.h"
#include "test_ovf_fitting_common.h"

double x[] = { 201, 244, 47, 287, 203, 58, 210, 202, 198, 158, 165, 201, 157, 131, 166, 160, 186, 125, 218, 146};

double y[] = { 592, 401, 583, 402, 495, 173, 479, 504, 510, 416, 393, 442, 317, 311, 400, 337, 423, 334, 533, 344 };

double sigma_y[] = {61, 25, 38, 15, 21, 15, 27, 14, 30, 16, 14, 25, 52, 16, 34, 31, 42, 26, 16, 22};

#ifndef M_PI
#define M_PI		3.14159265358979323846
#endif


#define ADD_V_EQUIL(MP, FN) { \
  struct rhp_ovf_def *ovfdef; \
  RESHOP_CHECK(FN(mdl, MP, loss_fn, &ovfdef)); \
  if (reformulation) { \
    RESHOP_CHECK(rhp_ovf_setreformulation(ovfdef, reformulation)); \
  } \
  RESHOP_CHECK(rhp_ovf_check(mdl, ovfdef)); \
  }




#define RHP_CHK(EXPR) { int status42 = EXPR; if (status42 != RHP_OK) { return status42; } }
#define RESHOP_CHECK(EXPR) { status = EXPR; if (status != RHP_OK) { goto _exit; } }
#define TOL_EPS 1e-10
#define TOL_EPSLAX 1e-4

typedef int (*param_fn)(struct rhp_ovf_def *ovf_def);

static int param_kappa(struct rhp_ovf_def *ovf_def)
{
  return rhp_ovf_param_add_scalar(ovf_def, "kappa", 0.67);
}

static int param_epsilon(struct rhp_ovf_def *ovf_def)
{
  return rhp_ovf_param_add_scalar(ovf_def, "epsilon", 0.77);
}

static int param_lambda(struct rhp_ovf_def *ovf_def)
{
  return rhp_ovf_param_add_scalar(ovf_def, "lambda", 0.999);
}

const param_fn elastic_net_param_fn[] = {param_lambda, NULL};
const param_fn l1_param_fn[] = {NULL};
const param_fn l2_param_fn[] = {NULL};
const param_fn hinge_param_fn[] = {param_epsilon, NULL};
const param_fn huber_param_fn[] = {param_kappa, NULL};
const param_fn hubnik_param_fn[] = {param_epsilon, param_kappa, NULL};
const param_fn soft_hinge_param_fn[] = {param_epsilon, param_kappa, NULL};
const param_fn vapnik_param_fn[] = {param_epsilon, NULL};

const param_fn *loss_fn_param_fn[] = {
  elastic_net_param_fn,
  l1_param_fn,
  l2_param_fn,
  hinge_param_fn,
  huber_param_fn,
  hubnik_param_fn,
  soft_hinge_param_fn,
  vapnik_param_fn,
};

const double loss_fn_sols[][3] = {
  {1.12997878394e+00, 2.06553500649e+02, 2.01600145727e+02},  /*  elastic net */
  {1.91111111111e+00, 7.76666666667e+01, 5.16936399857e+01},  /*  l1          */
  {1.07674752416e+00, 2.13273491978e+02, 1.44981861391e+02},  /*  l2          */
  {NAN, NAN, NAN},                                            /*  hinge       */
  {1.98022727187e+00, 6.62296137062e+01, 3.07609719225e+01},  /*  huber       */
  {1.94963070855e+00, 7.83555769225e+01, 2.33199138466e+01},  /*  hubnik      */
  {NAN, NAN, NAN},                                            /*  soft hinge  */
  {2.00443181818e+00, 6.82929545455e+01, 3.90616984235e+01},  /*  vapnik      */
};

static size_t loss_fn_idx(const char * loss_fn_str)
{
  for (size_t i = 0; i < loss_fns_size; ++i) {
    if (!strcasecmp(loss_fn_str, loss_fns[i])) {
      return i;
    }
  }

  return SIZE_MAX;
}

static int loss_fn_params(struct rhp_ovf_def *ovf_def, const char *loss_fn)
{
  int status = 0;

  size_t loss_idx = loss_fn_idx(loss_fn);
  if (loss_idx == SIZE_MAX) { return 1; }

  const param_fn *fns = loss_fn_param_fn[loss_idx];
  size_t i = 0;
  while (fns[i]) {
    RESHOP_CHECK((*fns[i])(ovf_def));
    i++;
  }

_exit:
  return status;
}

static int ovf_printsolveinfo(struct rhp_mdl *mdl_solver, const char* testname, const char* loss_fn, const char* reformulation)
{
  const char *solvername;
  RHP_CHK(rhp_mdl_getsolvername(mdl_solver, &solvername));
  printf(ANSI_COLOR_BLUE "Solving '%s' test with solver '%s' for loss '%s' and reformulation '%s'\n" ANSI_COLOR_RESET,
         testname, solvername, loss_fn, reformulation);

  return RHP_OK;
}

static int solve_test(struct rhp_mdl *mdl, struct rhp_mdl *mdl_solver)
{

  int status = rhp_process(mdl, mdl_solver);
  if (status != RHP_OK) {
    printf("Transforming the problem failed: status = %s (%d)\n ",
        rhp_status_descr(status), status);
    goto _exit;
  }

  status = rhp_solve(mdl_solver);
  if (status != RHP_OK) {
    printf("Solving the problem failed: status = %s (%d)\n ",
        rhp_status_descr(status), status);
    goto _exit;
  }

  status = rhp_postprocess(mdl_solver);
  if (status != RHP_OK) {
    printf("Reporting the values failed: status = %s (%d)\n ",
        rhp_status_descr(status), status);
    goto _exit;
  }

_exit:
  return status;
}

static int solve_ovf_test(struct rhp_mdl *mdl, struct rhp_mdl *mdl_solver, const char *loss_fn, const char *reformulation)
{
  int status = RHP_OK;

  size_t loss_idx = loss_fn_idx(loss_fn);
  if (loss_idx == SIZE_MAX) { status = 1; goto _exit; }

  RESHOP_CHECK(solve_test(mdl, mdl_solver));

  printf(ANSI_COLOR_BLUE"\nloss: %s; reformulation %s\n", loss_fn, reformulation ? reformulation : "\"\""ANSI_COLOR_RESET);
  for (unsigned i = 0; i < 3; ++i) {
    double val;
    rhp_mdl_getvarval(mdl, i, &val);
    printf("%s: %.11e; ", rhp_mdl_printvarname(mdl, i), val);
    }
  puts("\n");

  const double *sols = loss_fn_sols[loss_idx];
  for (unsigned i = 0; i < 3; ++i) {
    double sol = sols[i], val;
    rhp_mdl_getvarval(mdl, i, &val);

    if (isfinite(sol)) {
      double delta = fabs(val-sols[i]);
      printf("diff_%s: %e; ", rhp_mdl_printvarname(mdl, i), delta);
      if (delta > TOL_EPSLAX) {
        status = 2;
      }
    }
  }
  puts("\n");

_exit:
  if (status != RHP_OK) {
    printf(ANSI_COLOR_RED "Test on OVF function %s with reformulation %s failed\n" ANSI_COLOR_RESET, loss_fn, reformulation);
  }
  return status;
}

int fitting_test(struct rhp_mdl *mdl_solver, const char *loss_fn, const char *reformulation)
{
  int status = 0;

  struct rhp_avar *v_fit = rhp_avar_new();
  struct rhp_avar *v_ovf = rhp_avar_new();
  struct rhp_avar *v_arg_ovf = rhp_avar_new();

  struct rhp_mdl *mdl = rhp_mdl_new(RHP_BACKEND_RHP);

  if (!mdl_solver) { status = 1; goto _exit; }

  size_t loss_idx = loss_fn_idx(loss_fn);
  if (loss_idx == SIZE_MAX) { status = 1; goto _exit; }

  unsigned nb_equ = sizeof x / sizeof(double);
  rhp_set_option_d(mdl, "rtol", TOL_EPS/10.);

  RESHOP_CHECK(ovf_printsolveinfo(mdl_solver, __func__, loss_fn, reformulation));

  RESHOP_CHECK(rhp_mdl_resize(mdl, 3+nb_equ, nb_equ+1));

  RESHOP_CHECK(rhp_add_varsnamed(mdl, 2, v_fit, "x"));
  RESHOP_CHECK(rhp_add_varsnamed(mdl, 1, v_ovf, "ovfvar"));
  RESHOP_CHECK(rhp_add_varsnamed(mdl, nb_equ, v_arg_ovf, "defargovf"));

  rhp_idx ovf_idx, c_idx, d_idx;
  RESHOP_CHECK(rhp_avar_get(v_ovf, 0, &ovf_idx));
  RESHOP_CHECK(rhp_avar_get(v_fit, 0, &c_idx));
  RESHOP_CHECK(rhp_avar_get(v_fit, 1, &d_idx));

  rhp_idx ei;
  RESHOP_CHECK(rhp_add_func(mdl, &ei));

  RESHOP_CHECK(rhp_equ_addnewlvar(mdl, ei, ovf_idx, 1.));

  RESHOP_CHECK(rhp_mdl_setobjequ(mdl, ei));
  RESHOP_CHECK(rhp_mdl_setobjsense(mdl, RHP_MIN));
  RESHOP_CHECK(rhp_mdl_setprobtype(mdl, 1));

   for (unsigned i = 0; i < nb_equ; ++i) {
      rhp_idx vi;
      ei = -1;
      RESHOP_CHECK(rhp_avar_get(v_arg_ovf, i, &vi));
      /*  (y(i) - sum(j, c1(j)*x(i, j)) - d1) */
      RESHOP_CHECK(rhp_add_con(mdl, RHP_CON_EQ, &ei));
      RESHOP_CHECK(rhp_equ_addnewlvar(mdl, ei, vi, -1.));
      RESHOP_CHECK(rhp_equ_addnewlvar(mdl, ei, c_idx, -x[i]/sigma_y[i]));
      RESHOP_CHECK(rhp_equ_addnewlvar(mdl, ei, d_idx, -1./sigma_y[i]));
      RESHOP_CHECK(rhp_mdl_setequrhs(mdl, ei, -y[i]/sigma_y[i]));
   }

  struct rhp_ovf_def *ovf_def;
  RESHOP_CHECK(rhp_ovf_add(mdl, loss_fn, ovf_idx, v_arg_ovf, &ovf_def));

  RESHOP_CHECK(loss_fn_params(ovf_def, loss_fn));

  if (reformulation) {
    RESHOP_CHECK(rhp_ovf_setreformulation(ovf_def, reformulation));
  }

  RESHOP_CHECK(rhp_ovf_check(mdl, ovf_def));

  RESHOP_CHECK(solve_ovf_test(mdl, mdl_solver, loss_fn, reformulation));

_exit:
  rhp_mdl_free(mdl);

  rhp_avar_free(v_fit);
  rhp_avar_free(v_ovf);
  rhp_avar_free(v_arg_ovf);

  return status == RHP_OK ? 0 : 1;
}

static int add_v1(struct rhp_mdl *mdl, struct rhp_mathprgm *mp, const char *loss_fn, struct rhp_ovf_def **ovf)
{

  int status = RHP_OK;
  unsigned nb_equ = sizeof x / sizeof(double);

  struct rhp_avar *v_fit = rhp_avar_new();
  struct rhp_avar *v_ovf = rhp_avar_new();
  struct rhp_avar *v_arg_ovf = rhp_avar_new();
  struct rhp_aequ *e_cons = rhp_aequ_new();

  RESHOP_CHECK(rhp_add_varsnamed(mdl, 2, v_fit, "fit1"))
  RESHOP_CHECK(rhp_add_varsnamed(mdl, 1, v_ovf, "ovf1"));
  RESHOP_CHECK(rhp_add_varsnamed(mdl, nb_equ, v_arg_ovf, "arg_ovf1"));

  rhp_idx ovfvar, cvar, dvar;
  RESHOP_CHECK(rhp_avar_get(v_ovf, 0, &ovfvar));
  RESHOP_CHECK(rhp_avar_get(v_fit, 0, &cvar));
  RESHOP_CHECK(rhp_avar_get(v_fit, 1, &dvar));

  /* Add the objective equation  */
  rhp_idx objequ = -1;
  RESHOP_CHECK(rhp_add_funcnamed(mdl, &objequ, "objequ1"));

  RESHOP_CHECK(rhp_equ_addnewlvar(mdl, objequ, ovfvar, 1.));

  if (mp) {
    RESHOP_CHECK(rhp_mp_settype(mp, RHP_MP_OPT));
    /* Add variant with objvar  */
    RESHOP_CHECK(rhp_mp_setobjequ(mp, objequ));

    RESHOP_CHECK(rhp_mp_addvars(mp, v_fit));
    RESHOP_CHECK(rhp_mp_addvars(mp, v_ovf));
  } else {
    RESHOP_CHECK(rhp_mdl_setobjequ(mdl, objequ));
    RESHOP_CHECK(rhp_mdl_setobjsense(mdl, RHP_MIN));
  }

  RESHOP_CHECK(rhp_add_consnamed(mdl, nb_equ, RHP_CON_EQ, e_cons, "ovf_arg1"));

  for (unsigned i = 0; i < nb_equ; ++i) {
    rhp_idx ui;
    rhp_idx ei;
    RESHOP_CHECK(rhp_avar_get(v_arg_ovf, i, &ui));
    RESHOP_CHECK(rhp_aequ_get(e_cons, i, &ei));
    /*  (y(i) - sum(j, c1(j)*x(i, j)) - d1) */
    RESHOP_CHECK(rhp_equ_addnewlvar(mdl, ei, ui, -1.));
    RESHOP_CHECK(rhp_equ_addnewlvar(mdl, ei, cvar, -x[i]/sigma_y[i]));
    RESHOP_CHECK(rhp_equ_addnewlvar(mdl, ei, dvar, -1./sigma_y[i]));
    RESHOP_CHECK(rhp_mdl_setequrhs(mdl, ei, -y[i]/sigma_y[i]));

  }

  if (mp) {
    RESHOP_CHECK(rhp_mp_finalize(mp));
  }

  RESHOP_CHECK(rhp_ovf_add(mdl, loss_fn, ovfvar, v_arg_ovf, ovf));
  RESHOP_CHECK(loss_fn_params(*ovf, loss_fn));

_exit:
  rhp_avar_free(v_fit);
  rhp_avar_free(v_ovf);
  rhp_avar_free(v_arg_ovf);
  rhp_aequ_free(e_cons);

  return status;
}


static int add_v2(struct rhp_mdl *mdl, struct rhp_mathprgm *mp, const char *loss_fn, struct rhp_ovf_def **ovf)
{

  int status = RHP_OK;
  unsigned nb_equ = sizeof x / sizeof(double);

  struct rhp_avar *v_fit = rhp_avar_new();
  struct rhp_avar *v_ovf = rhp_avar_new();
  struct rhp_avar *v_arg_ovf = rhp_avar_new();
  struct rhp_aequ *e_cons = rhp_aequ_new();

  RESHOP_CHECK(rhp_add_varsnamed(mdl, 2, v_fit, "fit2"))
  RESHOP_CHECK(rhp_add_varsnamed(mdl, 1, v_ovf, "ovf2"));
  RESHOP_CHECK(rhp_add_varsnamed(mdl, nb_equ, v_arg_ovf, "arg_ovf2"));

  rhp_idx ovfvar, cvar, dvar;
  RESHOP_CHECK(rhp_avar_get(v_ovf, 0, &ovfvar));
  RESHOP_CHECK(rhp_avar_get(v_fit, 0, &cvar));
  RESHOP_CHECK(rhp_avar_get(v_fit, 1, &dvar));

  /* Add the objective equation  */
  rhp_idx objequ = -1;
  RESHOP_CHECK(rhp_add_funcnamed(mdl, &objequ, "objequ2"));

  RESHOP_CHECK(rhp_equ_addnewlvar(mdl, objequ, ovfvar, 1.));

  if (mp) {
    RESHOP_CHECK(rhp_mp_settype(mp, RHP_MP_OPT));
    /* Add variant with objvar  */
    RESHOP_CHECK(rhp_mp_setobjequ(mp, objequ));
    RESHOP_CHECK(rhp_mp_addvars(mp, v_fit));
    RESHOP_CHECK(rhp_mp_addvars(mp, v_ovf));
  } else {
    RESHOP_CHECK(rhp_mdl_setobjequ(mdl, objequ));
    RESHOP_CHECK(rhp_mdl_setobjsense(mdl, RHP_MIN));
  }

  RESHOP_CHECK(rhp_add_consnamed(mdl, nb_equ, RHP_CON_EQ, e_cons, "ovf_arg2"));

  for (unsigned i = 0; i < nb_equ; ++i) {
    rhp_idx ui;
    rhp_idx ei = -1;
    RESHOP_CHECK(rhp_avar_get(v_arg_ovf, i, &ui));
    RESHOP_CHECK(rhp_aequ_get(e_cons, i, &ei));

    /* log(exp((y[i] - c2*x[i] - d2) / sigma_y(i))) - u[i] = 0  */
    RESHOP_CHECK(rhp_equ_addnewlvar(mdl, ei, ui, -1.));

    struct rhp_nltree *tree = rhp_mdl_getnltree(mdl, ei);
    struct rhp_nlnode **child = NULL;

    /* Add the log   */
    struct rhp_nlnode **log_node = NULL;
    RESHOP_CHECK(rhp_nltree_getroot(tree, &log_node));
    RESHOP_CHECK(rhp_nltree_call(mdl, tree, &log_node, 11, 1));

    /* Add the exp */
    struct rhp_nlnode **exp_node = NULL;
    RESHOP_CHECK(rhp_nltree_getchild(log_node, &exp_node, 0));
    RESHOP_CHECK(rhp_nltree_call(mdl, tree, &exp_node, 10, 1));

    /*  The DIV */
    struct rhp_nlnode **div_node = NULL;
    RESHOP_CHECK(rhp_nltree_getchild(exp_node, &div_node, 0));
    RESHOP_CHECK(rhp_nltree_arithm(tree, &div_node, 5, 2));

    /* Deal with the denominator now  */
    child = NULL;
    RESHOP_CHECK(rhp_nltree_getchild(div_node, &child, 0));
    RESHOP_CHECK(rhp_nltree_cst(mdl, tree, &child, sigma_y[i]));

    /* The numerator is an ADD (sum) node with 3 terms */
    struct rhp_nlnode **lin_term = NULL;
    RESHOP_CHECK(rhp_nltree_getchild(div_node, &lin_term, 1));
    RESHOP_CHECK(rhp_nltree_arithm(tree, &lin_term, 2, 3));
    child = NULL;

    /* y[i] */
    RESHOP_CHECK(rhp_nltree_getchild(lin_term, &child, 0));
    RESHOP_CHECK(rhp_nltree_cst(mdl, tree, &child, y[i]));
    child = NULL;

    /* deal with - x[i]  * c */
    RESHOP_CHECK(rhp_nltree_getchild(lin_term, &child, 1));
    RESHOP_CHECK(rhp_nltree_var(mdl, tree, &child, cvar, -x[i]));
    child = NULL;

    /* This is the -d2  */
    RESHOP_CHECK(rhp_nltree_getchild(lin_term, &child, 2));
    RESHOP_CHECK(rhp_nltree_var(mdl, tree, &child, dvar, -1.));

  }

  if (mp) {
    RESHOP_CHECK(rhp_mp_finalize(mp));
  }

  RESHOP_CHECK(rhp_ovf_add(mdl, loss_fn, ovfvar, v_arg_ovf, ovf));
  RESHOP_CHECK(loss_fn_params(*ovf, loss_fn));

_exit:
  rhp_avar_free(v_fit);
  rhp_avar_free(v_ovf);
  rhp_avar_free(v_arg_ovf);
  rhp_aequ_free(e_cons);

  return status;
}

static int add_v3(struct rhp_mdl *mdl, struct rhp_mathprgm *mp, const char *loss_fn, struct rhp_ovf_def **ovf)
{

  int status = RHP_OK;
  unsigned nb_equ = sizeof x / sizeof(double);

  struct rhp_avar *v_fit = rhp_avar_new();
  struct rhp_avar *v_ovf = rhp_avar_new();
  struct rhp_avar *v_arg_ovf = rhp_avar_new();
  struct rhp_aequ *e_cons = rhp_aequ_new();

  RESHOP_CHECK(rhp_add_varsnamed(mdl, 2, v_fit, "fit3"))
  RESHOP_CHECK(rhp_add_varsnamed(mdl, 1, v_ovf, "ovf3"));
  RESHOP_CHECK(rhp_add_varsnamed(mdl, nb_equ, v_arg_ovf, "arg_ovf3"));

  rhp_idx ovfvar, cvar, dvar;
  RESHOP_CHECK(rhp_avar_get(v_ovf, 0, &ovfvar));
  RESHOP_CHECK(rhp_avar_get(v_fit, 0, &cvar));
  RESHOP_CHECK(rhp_avar_get(v_fit, 1, &dvar));

  /* Add the objective equation  */
  rhp_idx objequ = -1;
  RESHOP_CHECK(rhp_add_funcnamed(mdl, &objequ, "objequ3"));

  RESHOP_CHECK(rhp_equ_addnewlvar(mdl, objequ, ovfvar, 1.));

  if (mp) {
    RESHOP_CHECK(rhp_mp_settype(mp, RHP_MP_OPT));
    /* Add variant with objvar  */
    RESHOP_CHECK(rhp_mp_setobjequ(mp, objequ));
    RESHOP_CHECK(rhp_mp_addvars(mp, v_fit));
    RESHOP_CHECK(rhp_mp_addvars(mp, v_ovf));
  } else {
    RESHOP_CHECK(rhp_mdl_setobjequ(mdl, objequ));
    RESHOP_CHECK(rhp_mdl_setobjsense(mdl, RHP_MIN));
  }

  RESHOP_CHECK(rhp_add_consnamed(mdl, nb_equ, RHP_CON_EQ, e_cons, "ovf_arg3"));

  for (unsigned i = 0; i < nb_equ; ++i) {
    rhp_idx ui;
    rhp_idx ei = -1;
    RESHOP_CHECK(rhp_avar_get(v_arg_ovf, i, &ui));
    RESHOP_CHECK(rhp_aequ_get(e_cons, i, &ei));
    /*  (y(i) - sum(j, c1(j)*x(i, j)) - d1) */
    RESHOP_CHECK(rhp_equ_addnewlvar(mdl, ei, ui, 1.));
    RESHOP_CHECK(rhp_equ_addnewlvar(mdl, ei, cvar, x[i]/sigma_y[i]));
    RESHOP_CHECK(rhp_equ_addnewlvar(mdl, ei, dvar, 1./sigma_y[i]));
    RESHOP_CHECK(rhp_mdl_setequrhs(mdl, ei, y[i]/sigma_y[i]));

  }

  if (mp) {
    RESHOP_CHECK(rhp_mp_finalize(mp));
  }

  RESHOP_CHECK(rhp_ovf_add(mdl, loss_fn, ovfvar, v_arg_ovf, ovf));
  RESHOP_CHECK(loss_fn_params(*ovf, loss_fn));

_exit:
  rhp_avar_free(v_fit);
  rhp_avar_free(v_ovf);
  rhp_avar_free(v_arg_ovf);
  rhp_aequ_free(e_cons);

  return status;
}

static int add_v4(struct rhp_mdl *mdl, struct rhp_mathprgm *mp, const char *loss_fn, struct rhp_ovf_def **ovf)
{

  int status = RHP_OK;
  unsigned nb_equ = sizeof x / sizeof(double);

  struct rhp_avar *v_fit = rhp_avar_new();
  struct rhp_avar *v_ovf = rhp_avar_new();
  struct rhp_avar *v_arg_ovf = rhp_avar_new();
  struct rhp_aequ *e_cons = rhp_aequ_new();

  RESHOP_CHECK(rhp_add_varsnamed(mdl, 2, v_fit, "fit4"))
  RESHOP_CHECK(rhp_add_varsnamed(mdl, 1, v_ovf, "ovf4"));
  RESHOP_CHECK(rhp_add_varsnamed(mdl, nb_equ, v_arg_ovf, "arg_ovf4"));

  rhp_idx ovfvar, cvar, dvar;
  RESHOP_CHECK(rhp_avar_get(v_ovf, 0, &ovfvar));
  RESHOP_CHECK(rhp_avar_get(v_fit, 0, &cvar));
  RESHOP_CHECK(rhp_avar_get(v_fit, 1, &dvar));

  /* Add the objective equation  */
  rhp_idx objequ = -1;
  RESHOP_CHECK(rhp_add_funcnamed(mdl, &objequ, "objequ4"));

  RESHOP_CHECK(rhp_equ_addnewlvar(mdl, objequ, ovfvar, 1.));

  if (mp) {
    RESHOP_CHECK(rhp_mp_settype(mp, RHP_MP_OPT));
    /* Add variant with objvar  */
    RESHOP_CHECK(rhp_mp_setobjequ(mp, objequ));
    RESHOP_CHECK(rhp_mp_addvars(mp, v_fit));
    RESHOP_CHECK(rhp_mp_addvars(mp, v_ovf));
  } else {
    RESHOP_CHECK(rhp_mdl_setobjequ(mdl, objequ));
    RESHOP_CHECK(rhp_mdl_setobjsense(mdl, RHP_MIN));
  }

  RESHOP_CHECK(rhp_add_consnamed(mdl, nb_equ, RHP_CON_EQ, e_cons, "ovf_arg4"));

  for (unsigned i = 0; i < nb_equ; ++i) {
    rhp_idx ui;
    rhp_idx ei = -1;
    RESHOP_CHECK(rhp_avar_get(v_arg_ovf, i, &ui));
    RESHOP_CHECK(rhp_aequ_get(e_cons, i, &ei));

    /* u[i] - log(exp((y[i] - c2*x[i] - d2) / sigma_y(i))) = 0  */
    RESHOP_CHECK(rhp_equ_addnewlvar(mdl, ei, ui, 1.));

    struct rhp_nltree *tree = rhp_mdl_getnltree(mdl, ei);
    struct rhp_nlnode **child = NULL;
    struct rhp_nlnode **root = NULL;
    RESHOP_CHECK(rhp_nltree_getroot(tree, &root));
    RESHOP_CHECK(rhp_nltree_umin(tree, &root));

    /* Add the log   */
    struct rhp_nlnode **log_node = root;
    RESHOP_CHECK(rhp_nltree_call(mdl, tree, &log_node, 11, 1));

    /* Add the exp */
    struct rhp_nlnode **exp_node = NULL;
    RESHOP_CHECK(rhp_nltree_getchild(log_node, &exp_node, 0));
    RESHOP_CHECK(rhp_nltree_call(mdl, tree, &exp_node, 10, 1));

    /*  The DIV */
    struct rhp_nlnode **div_node = NULL;
    RESHOP_CHECK(rhp_nltree_getchild(exp_node, &div_node, 0));
    RESHOP_CHECK(rhp_nltree_arithm(tree, &div_node, 5, 2));

    /* Deal with the denominator now  */
    child = NULL;
    RESHOP_CHECK(rhp_nltree_getchild(div_node, &child, 0));
    RESHOP_CHECK(rhp_nltree_cst(mdl, tree, &child, sigma_y[i]));

    /* The numerator is an ADD (sum) node with 3 terms */
    struct rhp_nlnode **lin_term = NULL;
    RESHOP_CHECK(rhp_nltree_getchild(div_node, &lin_term, 1));
    RESHOP_CHECK(rhp_nltree_arithm(tree, &lin_term, 2, 3));
    child = NULL;

    /* y[i] */
    RESHOP_CHECK(rhp_nltree_getchild(lin_term, &child, 0));
    RESHOP_CHECK(rhp_nltree_cst(mdl, tree, &child, y[i]));
    child = NULL;

    /* deal with - x[i]  * c */
    RESHOP_CHECK(rhp_nltree_getchild(lin_term, &child, 1));
    RESHOP_CHECK(rhp_nltree_var(mdl, tree, &child, cvar, -x[i]));
    child = NULL;

    /* This is the -d2  */
    RESHOP_CHECK(rhp_nltree_getchild(lin_term, &child, 2));
    RESHOP_CHECK(rhp_nltree_var(mdl, tree, &child, dvar, -1.));

  }

  if (mp) {
    RESHOP_CHECK(rhp_mp_finalize(mp));
  }

  RESHOP_CHECK(rhp_ovf_add(mdl, loss_fn, ovfvar, v_arg_ovf, ovf));
  RESHOP_CHECK(loss_fn_params(*ovf, loss_fn));

_exit:
  rhp_avar_free(v_fit);
  rhp_avar_free(v_ovf);
  rhp_avar_free(v_arg_ovf);
  rhp_aequ_free(e_cons);

  return status;
}

static int add_v5(struct rhp_mdl *mdl, struct rhp_mathprgm *mp, const char *loss_fn, struct rhp_ovf_def **ovf)
{

  int status = RHP_OK;
  unsigned nb_equ = sizeof x / sizeof(double);

  struct rhp_avar *v_fit = rhp_avar_new();
  struct rhp_avar *v_ovf = rhp_avar_new();
  struct rhp_avar *v_arg_ovf = rhp_avar_new();
  struct rhp_aequ *e_cons = rhp_aequ_new();

  RESHOP_CHECK(rhp_add_varsnamed(mdl, 2, v_fit, "fit5"))
  RESHOP_CHECK(rhp_add_varsnamed(mdl, 1, v_ovf, "ovf5"));
  RESHOP_CHECK(rhp_add_varsnamed(mdl, nb_equ, v_arg_ovf, "arg_ovf5"));

  rhp_idx ovfvar, cvar, dvar;
  RESHOP_CHECK(rhp_avar_get(v_ovf, 0, &ovfvar));
  RESHOP_CHECK(rhp_avar_get(v_fit, 0, &cvar));
  RESHOP_CHECK(rhp_avar_get(v_fit, 1, &dvar));

  /* Add the objective equation  */
  rhp_idx objequ = -1;
  RESHOP_CHECK(rhp_add_funcnamed(mdl, &objequ, "objequ5"));

  RESHOP_CHECK(rhp_equ_addnewlvar(mdl, objequ, ovfvar, 1.));

  if (mp) {
    RESHOP_CHECK(rhp_mp_settype(mp, RHP_MP_OPT));
    /* Add variant with objvar  */
    RESHOP_CHECK(rhp_mp_setobjequ(mp, objequ));
    RESHOP_CHECK(rhp_mp_addvars(mp, v_fit));
    RESHOP_CHECK(rhp_mp_addvars(mp, v_ovf));
  } else {
    RESHOP_CHECK(rhp_mdl_setobjequ(mdl, objequ));
    RESHOP_CHECK(rhp_mdl_setobjsense(mdl, RHP_MIN));
  }

  RESHOP_CHECK(rhp_add_consnamed(mdl, nb_equ, RHP_CON_EQ, e_cons, "ovf_arg5"));

  for (unsigned i = 0; i < nb_equ; ++i) {
    rhp_idx ui;
    rhp_idx ei = -1;
    RESHOP_CHECK(rhp_avar_get(v_arg_ovf, i, &ui));
    RESHOP_CHECK(rhp_aequ_get(e_cons, i, &ei));
    /*  (y(i) - sum(j, c1(j)*x(i, j)) - d1) */
    RESHOP_CHECK(rhp_equ_addnewlvar(mdl, ei, ui, -M_PI));
    RESHOP_CHECK(rhp_equ_addnewlvar(mdl, ei, cvar, -M_PI*x[i]/sigma_y[i]));
    RESHOP_CHECK(rhp_equ_addnewlvar(mdl, ei, dvar, -M_PI/sigma_y[i]));
    RESHOP_CHECK(rhp_mdl_setequrhs(mdl, ei, -M_PI*y[i]/sigma_y[i]));

  }

  if (mp) {
    RESHOP_CHECK(rhp_mp_finalize(mp));
  }

  RESHOP_CHECK(rhp_ovf_add(mdl, loss_fn, ovfvar, v_arg_ovf, ovf));
  RESHOP_CHECK(loss_fn_params(*ovf, loss_fn));

_exit:
  rhp_avar_free(v_fit);
  rhp_avar_free(v_ovf);
  rhp_avar_free(v_arg_ovf);
  rhp_aequ_free(e_cons);

  return status;
}

int fitting_v_test(struct rhp_mdl *mdl_solver, const char *loss_fn, const char *reformulation, unsigned ver)
{
  int status = RHP_OK;

  struct rhp_mdl *mdl = rhp_mdl_new(RHP_BACKEND_RHP);
  if (!mdl_solver) { status = 1; goto _exit; }


  size_t loss_idx = loss_fn_idx(loss_fn);
  if (loss_idx == SIZE_MAX) { status = 1; goto _exit; }

  unsigned nb_equ = sizeof x / sizeof(double);
  rhp_set_option_d(mdl, "rtol", TOL_EPS/10.);

  RESHOP_CHECK(ovf_printsolveinfo(mdl_solver, __func__, loss_fn, reformulation));
  printf(ANSI_COLOR_MAGENTA "Testing v%u" ANSI_COLOR_RESET ANSI_CURSOR_SAVE"\n", ver);

  RESHOP_CHECK(rhp_mdl_resize(mdl, 3+nb_equ, nb_equ+1));

  struct rhp_ovf_def *ovfdef;
  switch (ver) {
  case 1:
    RESHOP_CHECK(add_v1(mdl, NULL, loss_fn, &ovfdef));
    break;
  case 2:
    RESHOP_CHECK(add_v2(mdl, NULL, loss_fn, &ovfdef));
    break;
  case 3:
    RESHOP_CHECK(add_v3(mdl, NULL, loss_fn, &ovfdef));
    break;
  case 4:
    RESHOP_CHECK(add_v4(mdl, NULL, loss_fn, &ovfdef));
    break;
  case 5:
    RESHOP_CHECK(add_v5(mdl, NULL, loss_fn, &ovfdef));
    break;
  default:
    fprintf(stderr, "%s :: unsupported version %u\n", __func__, ver);
    status = 3;
    goto _exit;
  }

  if (reformulation) {
    RESHOP_CHECK(rhp_ovf_setreformulation(ovfdef, reformulation));
  }
  RESHOP_CHECK(rhp_ovf_check(mdl, ovfdef));

  RESHOP_CHECK(solve_ovf_test(mdl, mdl_solver, loss_fn, reformulation));

_exit:
  rhp_mdl_free(mdl);

  if (status == RHP_OK) {
    puts(ANSI_CURSOR_RESTORE": " ANSI_COLOR_GREEN "OK\n" ANSI_COLOR_RESET);
  } else {
    printf(ANSI_COLOR_RED "ERROR: formulation v%u of OVF function `%s` and reformulation`%s` failed!\n",
           ver, loss_fn, reformulation);
  }

  return status;
}

int fitting_equil_test(struct rhp_mdl *mdl_solver, const char *loss_fn, const char *reformulation)
{
  int status = RHP_OK;

  struct rhp_mdl *mdl = rhp_mdl_new(RHP_BACKEND_RHP);
  if (!mdl_solver) { status = 1; goto _exit; }

  size_t loss_idx = loss_fn_idx(loss_fn);
  if (loss_idx == SIZE_MAX) { status = 1; goto _exit; }

  unsigned nb_equ = sizeof x / sizeof(double);
  rhp_set_option_d(mdl, "rtol", TOL_EPS/10.);

  RESHOP_CHECK(ovf_printsolveinfo(mdl_solver, __func__, loss_fn, reformulation));

  RESHOP_CHECK(rhp_mdl_resize(mdl, 3+nb_equ, nb_equ+1));

  struct rhp_equilibrium *mpe = rhp_empdag_newmpe(mdl);
  RESHOP_CHECK(rhp_empdag_rootsetmpe(mdl, mpe));

  struct rhp_mathprgm *mp_v1 = rhp_empdag_newmp(mdl, RHP_MIN);
  struct rhp_mathprgm *mp_v2 = rhp_empdag_newmp(mdl, RHP_MIN);
  struct rhp_mathprgm *mp_v3 = rhp_empdag_newmp(mdl, RHP_MIN);
  struct rhp_mathprgm *mp_v4 = rhp_empdag_newmp(mdl, RHP_MIN);
  struct rhp_mathprgm *mp_v5 = rhp_empdag_newmp(mdl, RHP_MIN);

  RESHOP_CHECK(rhp_empdag_mpeaddmp(mdl, mpe, mp_v1));
  RESHOP_CHECK(rhp_empdag_mpeaddmp(mdl, mpe, mp_v2));
  RESHOP_CHECK(rhp_empdag_mpeaddmp(mdl, mpe, mp_v3));
  RESHOP_CHECK(rhp_empdag_mpeaddmp(mdl, mpe, mp_v4));
  RESHOP_CHECK(rhp_empdag_mpeaddmp(mdl, mpe, mp_v5));

  ADD_V_EQUIL(mp_v1, add_v1);
  ADD_V_EQUIL(mp_v2, add_v2);
  ADD_V_EQUIL(mp_v3, add_v3);
  ADD_V_EQUIL(mp_v4, add_v4);
  ADD_V_EQUIL(mp_v5, add_v5);

  status = solve_ovf_test(mdl, mdl_solver, loss_fn, reformulation);

  const double *sols = loss_fn_sols[loss_idx];
  rhp_idx offset = nb_equ+3;
  for (unsigned i = 0; i < 3; ++i) {
    double sol = sols[i], val;
    for (unsigned j = 0; j < 5; ++j) {
      rhp_idx vi = i + offset*j;
      rhp_mdl_getvarval(mdl, vi, &val);
      printf("%s: %.11e; ", rhp_mdl_printvarname(mdl, vi), val);


      if (isfinite(sol)) {
        double delta = fabs(val-sols[i]);
        printf("diff_%s: %e; ", rhp_mdl_printvarname(mdl, vi), delta);
        if (delta > TOL_EPSLAX) {
          status = 2;
        }
      }
    }
    puts("");
  }
  puts("\n");

_exit:
  rhp_mdl_free(mdl);

  return status;
}
