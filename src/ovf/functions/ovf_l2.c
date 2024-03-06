#include "ovf_l2.h"
#include "ovf_parameter.h"
#include "ovf_generator.h"
#include "status.h"
#include "macros.h"

#define OVF_NAME l2
#define SIZE_U(n) n

static int l2_gen_M(unsigned n, const void *env, SpMat *mat)
{
   rhpmat_reset(mat);
   mat->csr = ovf_speye_mat(n, n, 1.);
   rhpmat_set_eye(mat);
   if (!mat->csr) return Error_InsufficientMemory;
   rhpmat_set_csr(mat);
   rhpmat_set_sym(mat);

   return OK;
}

static const struct var_genops l2_varfill  = {
   .set_type = NULL,
   .get_lb = NULL,
   .get_ub = NULL,
   .get_value = NULL,
   .get_multiplier = NULL,
};

static int l2_gen_var(Container* ctr,  unsigned n, const void* p)
{
   return ovf_box(ctr,  n, p, &l2_varfill);
}

static size_t l2_size_u(size_t n_args)
{
   return SIZE_U(n_args);
}
const struct ovf_param_def* const OVF_l2_params[] = {NULL};
const unsigned OVF_l2_params_len = 0;

const struct ovf_genops OVF_l2_datagen = {
   .M = l2_gen_M,
   .D = l2_gen_M,
   .J = l2_gen_M,
   .B = NULL,
   .b = NULL,
   .k = NULL,
   .set_A = NULL,
   .set_A_nonbox = NULL,
   .set_b = NULL,
   .set_b_nonbox = NULL,
   .set_b_0 = NULL,
   .set_cones = NULL,
   .set_cones_nonbox = NULL,
   .size_u = l2_size_u,
   .u_shift = NULL,
   .var = l2_gen_var,
   .var_ppty = &l2_varfill,
   .name = xstr(OVF_NAME),
};
