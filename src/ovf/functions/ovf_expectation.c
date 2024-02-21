#include "macros.h"
#include "printout.h"
#include "ovf_expectation.h"
#include "ovf_fn_helpers.h"
#include "ovf_parameter.h"
#include "ovf_generator.h"
#include "ovf_risk_measure_common.h"
#include "status.h"

/* TODO(xhub) analyze the transformation. Also for nonbox constraint, we may
 * have to add a 0 set  */
#define OVF_NAME expectation

static int expectation_gen_A(unsigned n, const void *env, SpMat *mat)
{
   rhpmat_reset(mat);
   A_CHECK(mat->csr, ovf_speye_mat(n, n, 1.));

   rhpmat_set_csr(mat);
   rhpmat_set_sym(mat);
   rhpmat_set_eye(mat);

   return OK;
}

static int expectation_gen_b(unsigned n, const void* env, double **vals)
{
   const struct ovf_param_list *p = (const struct ovf_param_list*) env;
   const struct ovf_param *probs = ovf_find_param("probabilities", p);
   OVF_CHECK_PARAM(probs, Error_OvfMissingParam);

   MALLOC_(*vals, double, n);
   double *lvals = *vals;

   for (size_t i = 0; i < n; ++i) {
      lvals[i] = risk_measure_get_probability(probs, i);
   }

   return OK;
}

static enum cone expectation_gen_set_cones(unsigned n, const void* env, unsigned idx,
                                     void **cone_data)
{
   assert(idx < n);
   *cone_data = NULL;
   return CONE_0;
}

static int expectation_u_shift(unsigned n, const void *env, double **u_shift)
{
   const struct ovf_param_list *p = (const struct ovf_param_list *) env;
   const struct ovf_param *probs = ovf_find_param("probabilities", p);
   OVF_CHECK_PARAM(probs, Error_OvfMissingParam);

   return risk_measure_u_shift(n, probs, u_shift);
}

/**
 *  @brief return the lower bound */
static double expectation_var_lb(const void* env, unsigned indx)
{
   const struct ovf_param_list *p = (const struct ovf_param_list*) env;
   const struct ovf_param *probs = ovf_find_param("probabilities", p);
   OVF_CHECK_PARAM(probs, Error_OvfMissingParam);

   return risk_measure_get_probability(probs, indx);
}

static double expectation_var_ub(const void* env, unsigned indx)
{
   const struct ovf_param_list *p = (const struct ovf_param_list*) env;
   const struct ovf_param *probs = ovf_find_param("probabilities", p);
   OVF_CHECK_PARAM(probs, Error_OvfMissingParam);

   return risk_measure_get_probability(probs, indx);
}

static const struct var_genops expectation_varfill  = {
   .set_type = NULL,
   .get_lb = expectation_var_lb,
   .get_ub = expectation_var_ub,
   .set_level = NULL,
   .set_marginal = NULL,
};

static int expectation_gen_var(Container* ctr,  unsigned n, const void* p)
{
   return ovf_box(ctr,  n, p, &expectation_varfill);
}

const struct ovf_param_def* const OVF_expectation_params[] = {&risk_measure_probabilities};
const unsigned OVF_expectation_params_len = sizeof(OVF_expectation_params)/sizeof(struct ovf_param_def*);

const struct ovf_genops OVF_expectation_datagen = {
   .M = NULL,
   .D = NULL,
   .J = NULL,
   .B = NULL,
   .b = NULL,
   .k = NULL,
   .set_A = expectation_gen_A,
   .set_A_nonbox = expectation_gen_A,
   .set_b = expectation_gen_b,
   .set_b_nonbox = expectation_gen_b,
   .set_b_0 = NULL,
   .set_cones = expectation_gen_set_cones,
   .set_cones_nonbox = expectation_gen_set_cones,
   .size_u = risk_measure_u_size,
   .u_shift = expectation_u_shift,
   .var = expectation_gen_var,
   .var_ppty = &expectation_varfill,
   .name = xstr(OVF_NAME),
};
