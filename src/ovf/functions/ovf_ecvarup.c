#include "macros.h"
#include "ovf_cvar_common.h"
#include "ovf_ecvarup.h"
#include "ovf_generator.h"
#include "ovf_parameter.h"
#include "ovf_risk_measure_common.h"
#include "printout.h"
#include "status.h"

#define OVF_NAME ecvarup
#define TAIL_VAL(X) (1. - X)

#include "ovf_ecvar_common.inc"

static const struct var_genops ecvarup_varfill  = {
   .set_type = NULL,
   .get_lb = ecvarup_var_lb,
   .get_ub = ecvarup_var_ub,
   .get_value = NULL,
   .get_multiplier = NULL,
};

static int ecvarup_gen_var(Container* ctr, unsigned n, const void* p)
{
   return ovf_box(ctr, n, p, &ecvarup_varfill);
}

const struct ovf_param_def* const OVF_ecvarup_params[] = {
   &cvar_tail,
   &ecvar_lambda,
   &risk_measure_probabilities
};

const unsigned OVF_ecvarup_params_len = sizeof(OVF_ecvarup_params)/sizeof(struct ovf_param_def*);

const struct ovf_genops OVF_ecvarup_datagen = {
   .M = NULL,
   .D = NULL,
   .J = NULL,
   .B = NULL,
   .b = NULL,
   .k = NULL,
   .set_A = cvar_gen_A,
   .set_A_nonbox = cvar_gen_A_nonbox,
   .set_b = ecvarup_gen_b,
   .set_b_nonbox = cvar_gen_b_nonbox,
   .set_b_0 = ecvarup_gen_b_0,
   .set_cones = cvar_gen_set_cones,
   .set_cones_nonbox = cvar_gen_set_cones_nonbox,
   .size_u = risk_measure_u_size,
   .u_shift = cvar_u_shift,
   .var = ecvarup_gen_var,
   .var_ppty = &ecvarup_varfill,
   .name = xstr(OVF_NAME),
};
