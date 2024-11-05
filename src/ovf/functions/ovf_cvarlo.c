#include "ovf_functions_common.h"
#include "ovf_cvar_common.h"
#include "ovf_cvarlo.h"
#include "ovf_parameter.h"
#include "ovf_generator.h"
#include "ovf_risk_measure_common.h"

#define OVF_NAME cvarlo
#define TAIL_VAL(X) (X)

#include "ovf_cvar_common.inc"

static const struct var_genops cvarlo_varfill  = {
   .set_type = NULL,
   .get_lb = cvarlo_var_lb,
   .get_ub = cvarlo_var_ub,
   .get_value = NULL,
   .get_multiplier = NULL,
};

static int cvarlo_gen_var(Container* ctr,  unsigned n, const void* p)
{
   return ovf_box(ctr,  n, p, &cvarlo_varfill);
}

const struct ovf_param_def* const OVF_cvarlo_params[] = {&cvar_tail, &risk_measure_probabilities};
const unsigned OVF_cvarlo_params_len = sizeof(OVF_cvarlo_params)/sizeof(struct ovf_param_def*);

const struct ovf_genops OVF_cvarlo_datagen = {
   .M = NULL,
   .D = NULL,
   .J = NULL,
   .B = NULL,
   .b = NULL,
   .k = NULL,
   .set_A = cvar_gen_A,
   .set_A_nonbox = cvar_gen_A_nonbox,
   .set_b = cvarlo_gen_b,
   .set_b_nonbox = cvar_gen_b_nonbox,
   .set_b_0 = NULL,
   .set_cones = cvar_gen_set_cones,
   .set_cones_nonbox = cvar_gen_set_cones_nonbox,
   .size_u = u_size_same,
   .u_shift = cvar_u_shift,
   .var = cvarlo_gen_var,
   .var_ppty = &cvarlo_varfill,
   .name = xstr(OVF_NAME),
};
