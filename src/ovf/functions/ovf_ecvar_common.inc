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
   const struct ovf_param *tail = ovf_find_param("tail", p);
   const struct ovf_param *risk_wt = ovf_find_param("risk_wt", p);
   const struct ovf_param *probs = ovf_find_param("probabilities", p);

   OVF_CHECK_PARAM(tail, Error_OvfMissingParam);
   OVF_CHECK_PARAM(risk_wt, Error_OvfMissingParam);
   OVF_CHECK_PARAM(probs, Error_OvfMissingParam);

   MALLOC_(*vals, double, 2*n+1);

   assert(tail->type == ARG_TYPE_SCALAR);
   for (unsigned i = n, j = 0; i < 2*n; ++i, ++j) {
      double pval = risk_measure_get_probability(probs, j);
      double minus_wpval = (risk_wt->val - 1.)*pval;
      (*vals)[j] = minus_wpval;
      (*vals)[i] = risk_wt->val*pval/TAIL_VAL(tail->val) - minus_wpval;
   }

   (*vals)[2*n] = 1.;

   return OK;
}

static int FN(gen_b_0)(unsigned n, const void* env, SpMat *mat,
                    double *u_shift, double **vals)
{
   const struct ovf_param_list *p = (const struct ovf_param_list *) env;
   const struct ovf_param *tail = ovf_find_param("tail", p);
   const struct ovf_param *risk_wt = ovf_find_param("risk_wt", p);
   const struct ovf_param *probs = ovf_find_param("probabilities", p);

   OVF_CHECK_PARAM(tail, Error_OvfMissingParam);
   OVF_CHECK_PARAM(risk_wt, Error_OvfMissingParam);
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

   assert(tail->type == ARG_TYPE_SCALAR);
   for (unsigned i = n, j = 0; i < 2*n; ++i, ++j) {
      double pval = risk_measure_get_probability(probs, j);
      double minus_wpval = (risk_wt->val - 1.)*pval;
      lvals[j] += minus_wpval;
      lvals[i] += (risk_wt->val*pval) / TAIL_VAL(tail->val) - minus_wpval;
   }

   lvals[2*n] += 1.;

   return OK;
}

/**
 *  @brief return the lower bound */
static double FN(var_lb)(const void* env, unsigned indx)
{
   const struct ovf_param_list *p = (const struct ovf_param_list *) env;
   const struct ovf_param *risk_wt = ovf_find_param("risk_wt", p);
   const struct ovf_param *probs = ovf_find_param("probabilities", p);

   OVF_CHECK_PARAM(risk_wt, Error_OvfMissingParam);
   OVF_CHECK_PARAM(probs, Error_OvfMissingParam);

   return (1.-risk_wt->val)*risk_measure_get_probability(probs, indx);
}

static double FN(var_ub)(const void* env, unsigned indx)
{
   const struct ovf_param_list *p = (const struct ovf_param_list*) env;
   const struct ovf_param *tail = ovf_find_param("tail", p);
   const struct ovf_param *risk_wt = ovf_find_param("risk_wt", p);
   const struct ovf_param *prob = ovf_find_param("probabilities", p);

   OVF_CHECK_PARAM(tail, Error_OvfMissingParam);
   OVF_CHECK_PARAM(risk_wt, Error_OvfMissingParam);
   OVF_CHECK_PARAM(prob, Error_OvfMissingParam);

   double pr = risk_measure_get_probability(prob, indx);

   double val = (risk_wt->val/TAIL_VAL(tail->val) + (1.- risk_wt->val))*pr;

   return val > 1. ? 1. : val;
}
