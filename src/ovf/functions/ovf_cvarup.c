#include "asnan.h"
#include "macros.h"
#include "ovf_functions_common.h"
#include "printout.h"
#include "ovf_cvar_common.h"
#include "ovf_cvarup.h"
#include "ovf_parameter.h"
#include "ovf_generator.h"
#include "ovf_risk_measure_common.h"
#include "status.h"

#define OVF_NAME cvarup
#define TAIL_VAL(X) (1. - X)

#include "ovf_cvar_common.inc"

static const struct var_genops cvarup_varfill  = {
   .set_type = NULL,
   .get_lb = cvarup_var_lb,
   .get_ub = cvarup_var_ub,
   .get_value = NULL,
   .get_multiplier = NULL,
};

static int cvarup_gen_var(Container* ctr,  unsigned n, const void* p)
{
   return ovf_box(ctr,  n, p, &cvarup_varfill);
}

const struct ovf_param_def* const OVF_cvarup_params[] = {&cvar_tail, &risk_measure_probabilities};
const unsigned OVF_cvarup_params_len = sizeof(OVF_cvarup_params)/sizeof(struct ovf_param_def*);

const struct ovf_genops OVF_cvarup_datagen = {
   .M = NULL,
   .D = NULL,
   .J = NULL,
   .B = NULL,
   .b = NULL,
   .k = NULL,
   .set_A = cvar_gen_A,
   .set_A_nonbox = cvar_gen_A_nonbox,
   .set_b = cvarup_gen_b,
   .set_b_nonbox = cvar_gen_b_nonbox,
   .set_b_0 = NULL,
   .set_cones = cvar_gen_set_cones,
   .set_cones_nonbox = cvar_gen_set_cones_nonbox,
   .size_u = u_size_same,
   .u_shift = cvar_u_shift,
   .var = cvarup_gen_var,
   .var_ppty = &cvarup_varfill,
   .name = xstr(OVF_NAME),
};
