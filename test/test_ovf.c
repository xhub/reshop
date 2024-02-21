
#include "reshop.h"
#include "test_common.h"
#include "test_ovf.h"
#include <errno.h>
#include <math.h>
#include <stdlib.h>
#include <string.h>

static double mean(const double *c, unsigned n)
{
   double res = 0;

   for (unsigned i = 0; i < n; ++i) {
      res += c[i];
   }

   return res/n;
}

static double* gen_gaussrand(unsigned n)
{
    unsigned m = n + n % 2;
    double* values = (double*)calloc(m,sizeof(double));

    if ( values )
    {
        for (unsigned i = 0; i < m; i += 2 )
    
        {
            double x,y,rsq,f;
            do {
                x = 2.0 * rand() / (double)RAND_MAX - 1.0;
                y = 2.0 * rand() / (double)RAND_MAX - 1.0;
                rsq = x * x + y * y;
            }while( rsq >= 1. || rsq == 0. );
            f = sqrt( -2.0 * log(rsq) / rsq );
            values[i]   = x * f;
            values[i+1] = y * f;
        }
    }
    return values;
}

int linear_quantile_regression(struct rhp_mdl *mdl, struct rhp_mdl *mdl_solver,
                               const char *formulation, unsigned s, const double xsols[3])
{
   int status = 0;
   double alpha = .5, *normals = NULL, *eta = NULL, *xsi = NULL;
   struct rhp_avar *sumPosPartArg = NULL;
   struct rhp_aequ *ovfargs = NULL;

   srand(1000);
   
   normals = gen_gaussrand(s);
   xsi = malloc(sizeof(double) * s);
   eta = malloc(sizeof(double) * s);
   if (!normals || !xsi || !eta) { status = ENOMEM; goto _exit; }


   for (unsigned i = 0; i < s; ++i) {
      xsi[i] = rand()/(double)RAND_MAX;
      eta[i] = 1. + .5*xsi[i] + normals[i];
   }

   rhp_idx objequ, c, gamma, sumPosPart;
   RESHOP_CHECK(rhp_add_varnamed(mdl, &c, "c"));
   RESHOP_CHECK(rhp_add_varnamed(mdl, &gamma, "gamma"));
   RESHOP_CHECK(rhp_add_varnamed(mdl, &sumPosPart, "sumPosPart"));
   sumPosPartArg = rhp_avar_new();
   RESHOP_CHECK(rhp_add_varsnamed(mdl, s, sumPosPartArg, "sumPosPartArg"));

   RESHOP_CHECK(rhp_add_func(mdl, &objequ));

   // objfun: 1/((1-alpha)*s) * sumPosPart + gamma - mean(eta) + c*mean(xsi)
   RESHOP_CHECK(rhp_equ_addnewlvar(mdl, objequ, sumPosPart, 1/((1-alpha)*s)));
   RESHOP_CHECK(rhp_equ_addnewlvar(mdl, objequ, gamma, 1));
   RESHOP_CHECK(rhp_equ_addnewlvar(mdl, objequ, c, mean(xsi, s)));
   RESHOP_CHECK(rhp_equ_setcst(mdl, objequ, -mean(eta, s)));

   RESHOP_CHECK(rhp_mdl_setobjequ(mdl, objequ));
   RESHOP_CHECK(rhp_mdl_setobjsense(mdl, RHP_MIN));
 
   ovfargs = rhp_aequ_new();
   RESHOP_CHECK(rhp_add_consnamed(mdl, s, RHP_CON_EQ, ovfargs, "defsumPosPartArg"));

   // sumPosPartArg(i) =E= eta(i) - gamma - c*xsi[i];
   // That is
   // gamma + c*xsi[i] + sumPosPartArg(i) =E= eta[i];
   for (unsigned i = 0; i < s; ++i) {
      rhp_idx ei, vi;
      RESHOP_CHECK(rhp_aequ_get(ovfargs, i, &ei));
      RESHOP_CHECK(rhp_avar_get(sumPosPartArg, i, &vi));
      RESHOP_CHECK(rhp_equ_addnewlvar(mdl, ei, vi, 1.));

      RESHOP_CHECK(rhp_equ_addnewlvar(mdl, ei, gamma, 1.));
      RESHOP_CHECK(rhp_equ_addnewlvar(mdl, ei, c, xsi[i]));
      RESHOP_CHECK(rhp_mdl_setequrhs(mdl, ei, eta[i]));
   }

   /* Define the CCF */
   struct rhp_ovf_def *ovf_def;
   RESHOP_CHECK(rhp_ovf_add(mdl, "sum_pos_part", sumPosPart, sumPosPartArg, &ovf_def));
   RESHOP_CHECK(rhp_ovf_setreformulation(ovf_def, formulation));
   RESHOP_CHECK(rhp_ovf_check(mdl, ovf_def));

   struct sol_vals solvals;
   sol_vals_init(&solvals);

   double *xvals = malloc(sizeof(double)*(3+s));
   memcpy(xvals, xsols, 3*sizeof(double));
   double *xvalsArg = &xvals[3];

   for (unsigned i = 0; i < s; ++i) {
      xvalsArg[i] = NAN;
   }

   solvals.xvals = xvals;

   status = test_solve(mdl, mdl_solver, &solvals);

   free(xvals);

_exit:
   rhp_avar_free(sumPosPartArg);
   rhp_aequ_free(ovfargs);

   free(normals);
   free(eta);
   free(xsi);

   return status;
}

#ifdef __linux__

const double xsols10000[] = { 5.397905e-01, 9.712997e-01, 4.058599543e+03 };
const double xsols1000[] = { 3.333662e-01, 1.091484e+00, 4.13350743e+02 };
const double xsols990[] = { 4.212558e-01, 1.044548e+00, 4.106294e+02 };
const double xsols10[] = {2.343535e+00, 1.169047e-01, 3.761607e+00};

#elif defined(__APPLE__)

const double xsols1000[] = { 6.228774e-01, 9.082586e-01, 4.120466e+02 };
const double xsols990[] = { 3.576578e-01, 1.047069e+00, 4.058557e+02 };
const double xsols10[] = {1.350988e+00, 6.528552e-01, 3.490099e+00};

#elif defined(_WIN32)

const double xsols10000[] = { 5.397905e-01, 9.712997e-01, 4.058599543e+03 };
const double xsols1000[] = { 4.577045e-01, 1.036909e+00, 3.714202e+02 };
const double xsols990[] = { 6.890444e-01, 9.222773e-01, 3.668514e+02 };
const double xsols10[] = {-1.074875e+00, 1.620756e+00, 2.956183e+00};

#else

#error Need to define the solution for this operating system (due to different rand sequence)

#endif

int test_linear_quantile_regression_equilibrium(struct rhp_mdl *mdl, struct rhp_mdl *mdl_solver)
{
    /* TODO(xhub) fix PATH */
   if (rhp_mdl_getbackend(mdl_solver) != RHP_BACKEND_GAMS_GMO) {
      rhp_mdl_free(mdl);
      return 0;
   }
  unsigned s = 990; /* to fit into community license */
   return linear_quantile_regression(mdl, mdl_solver, "equilibrium", s, xsols990);
}

int test_linear_quantile_regression_fenchel(struct rhp_mdl *mdl, struct rhp_mdl *mdl_solver)
{
   /* TODO(xhub) fix PATH */
   if (rhp_mdl_getbackend(mdl_solver) != RHP_BACKEND_GAMS_GMO) {
      rhp_mdl_free(mdl);
      return 0;
   }
   unsigned s = 1000;
   return linear_quantile_regression(mdl, mdl_solver, "fenchel", s, xsols1000);
}

#ifdef HAS_VREPR
int test_s_linear_quantile_regression_conjugate(struct rhp_mdl *mdl, struct rhp_mdl *mdl_solver)
{
   unsigned s = 10;
   return linear_quantile_regression(mdl, mdl_solver, "conjugate", s, xsols10);
}
#endif

int test_s_linear_quantile_regression_equilibrium(struct rhp_mdl *mdl, struct rhp_mdl *mdl_solver)
{
   unsigned s = 10;
   return linear_quantile_regression(mdl, mdl_solver, "equilibrium", s, xsols10);
}

int test_s_linear_quantile_regression_fenchel(struct rhp_mdl *mdl, struct rhp_mdl *mdl_solver)
{
   unsigned s = 10;
   return linear_quantile_regression(mdl, mdl_solver, "fenchel", s, xsols10);
}


