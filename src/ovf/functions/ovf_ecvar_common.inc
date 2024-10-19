#include "ovf_fn_helpers.h"
#include "ovf_parameter.h"
#include "ovf_risk_measure_common.h"
#include "printout.h"
#include "rhp_LA.h"

#ifndef OVF_NAME
#error "OVF_NAME must be defined before including this file"
#endif

#ifndef TAIL_VAL
#error "TAIL_VAL must be defined before including this file"
#endif

#define PP_CAT_NX(X, Y) X ## _ ## Y

#define PP_CAT(X,Y) PP_CAT_NX(X,Y)

#define FN(X) PP_CAT(OVF_NAME, X)

static int FN(gen_b)(unsigned n, const void* env, double **vals)
{
   const struct ovf_param_list *p = (const struct ovf_param_list *) env;
   const struct ovf_param *alpha = ovf_find_param("tail", p);
   const struct ovf_param *lambda = ovf_find_param("risk_wt", p);
   const struct ovf_param *probs = ovf_find_param("probabilities", p);

   OVF_CHECK_PARAM(alpha, Error_OvfMissingParam);
   OVF_CHECK_PARAM(lambda, Error_OvfMissingParam);
   OVF_CHECK_PARAM(probs, Error_OvfMissingParam);

   MALLOC_(*vals, double, 2*n+1);

   assert(alpha->type == ARG_TYPE_SCALAR);
   for (unsigned i = n, j = 0; i < 2*n; ++i, ++j) {
      double pval = risk_measure_get_probability(probs, j);
      double minus_wpval = (lambda->val - 1.)*pval;
      (*vals)[j] = minus_wpval;
      (*vals)[i] = lambda->val*pval/TAIL_VAL(alpha->val) - minus_wpval;
   }

   (*vals)[2*n] = 1.;

   return OK;
}

static int FN(gen_b_0)(unsigned n, const void* env, SpMat *mat,
                    double *u_shift, double **vals)
{
   const struct ovf_param_list *p = (const struct ovf_param_list *) env;
   const struct ovf_param *alpha = ovf_find_param("tail", p);
   const struct ovf_param *lambda = ovf_find_param("risk_wt", p);
   const struct ovf_param *probs = ovf_find_param("probabilities", p);

   OVF_CHECK_PARAM(alpha, Error_OvfMissingParam);
   OVF_CHECK_PARAM(lambda, Error_OvfMissingParam);
   OVF_CHECK_PARAM(probs, Error_OvfMissingParam);

   MALLOC_(*vals, double, 2*n+1);

   double *lvals = *vals;

   /* ----------------------------------------------------------------------
    * Au - b in K means that A(u - u_shift) - (b - Au_shift) in K
    * ---------------------------------------------------------------------- */

   /* ----------------------------------------------------------------------
    * Computes -A u_shift and set it in lvals
    * ---------------------------------------------------------------------- */

   S_CHECK(rhpmat_axpy(mat, u_shift, lvals));
   for (unsigned i = 0; i < 2*n+1; ++i) {
      lvals[i] = -lvals[i];
   }

   assert(alpha->type == ARG_TYPE_SCALAR);
   for (unsigned i = n, j = 0; i < 2*n; ++i, ++j) {
      double pval = risk_measure_get_probability(probs, j);
      double minus_wpval = (lambda->val - 1.)*pval;
      lvals[j] += minus_wpval;
      lvals[i] += (lambda->val*pval) / TAIL_VAL(alpha->val) - minus_wpval;
   }

   lvals[2*n] += 1.;

   return OK;
}

/**
 *  @brief return the lower bound */
static double FN(var_lb)(const void* env, unsigned indx)
{
   const struct ovf_param_list *p = (const struct ovf_param_list *) env;
   const struct ovf_param *lambda = ovf_find_param("risk_wt", p);
   const struct ovf_param *probs = ovf_find_param("probabilities", p);

   OVF_CHECK_PARAM(lambda, Error_OvfMissingParam);
   OVF_CHECK_PARAM(probs, Error_OvfMissingParam);

   double m_lambda = 1.- lambda->val;
   return m_lambda*risk_measure_get_probability(probs, indx);
}

static double FN(var_ub)(const void* env, unsigned indx)
{
   const struct ovf_param_list *p = (const struct ovf_param_list*) env;
   const struct ovf_param *alpha = ovf_find_param("tail", p);
   const struct ovf_param *lambda = ovf_find_param("risk_wt", p);
   const struct ovf_param *probs = ovf_find_param("probabilities", p);

   OVF_CHECK_PARAM(alpha, Error_OvfMissingParam);
   OVF_CHECK_PARAM(lambda, Error_OvfMissingParam);
   OVF_CHECK_PARAM(probs, Error_OvfMissingParam);

   double m_lambda = 1.- lambda->val;
   double pval = risk_measure_get_probability(probs, indx);

   return (lambda->val/TAIL_VAL(alpha->val) + m_lambda)*pval;
}
