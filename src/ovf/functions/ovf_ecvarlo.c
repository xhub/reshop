#include "macros.h"
#include "ovf_cvar_common.h"
#include "ovf_ecvarlo.h"
#include "ovf_generator.h"
#include "ovf_parameter.h"
#include "ovf_risk_measure_common.h"
#include "printout.h"
#include "status.h"

#define SIZE_U(n) n


#define OVF_NAME ecvarlo
#define TAIL_VAL(X) (X)

#include "ovf_ecvar_common.inc"

static const struct var_genops ecvarlo_varfill  = {
   .set_type = NULL,
   .get_lb = ecvarlo_var_lb,
   .get_ub = ecvarlo_var_ub,
   .get_value = NULL,
   .get_multiplier = NULL,
};

static int ecvarlo_gen_var(Container* ctr, unsigned n, const void* p)
{
   return ovf_box(ctr, n, p, &ecvarlo_varfill);
}

const struct ovf_param_def* const OVF_ecvarlo_params[] = {
   &cvar_tail,
   &ecvar_lambda,
   &risk_measure_probabilities
};

const unsigned OVF_ecvarlo_params_len = sizeof(OVF_ecvarlo_params)/sizeof(struct ovf_param_def*);

const struct ovf_genops OVF_ecvarlo_datagen = {
   .M = NULL,
   .D = NULL,
   .J = NULL,
   .B = NULL,
   .b = NULL,
   .k = NULL,
   .set_A = cvar_gen_A,
   .set_A_nonbox = cvar_gen_A_nonbox,
   .set_b = ecvarlo_gen_b,
   .set_b_nonbox = cvar_gen_b_nonbox,
   .set_b_0 = ecvarlo_gen_b_0,
   .set_cones = cvar_gen_set_cones,
   .set_cones_nonbox = cvar_gen_set_cones_nonbox,
   .size_u = risk_measure_u_size,
   .u_shift = cvar_u_shift,
   .var = ecvarlo_gen_var,
   .var_ppty = &ecvarlo_varfill,
   .name = xstr(OVF_NAME),
};
