#include "macros.h"
#include "printout.h"
#include "ovf_loss_common.h"
#include "ovf_huber.h"
#include "ovf_parameter.h"
#include "ovf_generator.h"
#include "status.h"

#define OVF_NAME huber
#define SIZE_U(n) n

static int huber_gen_A(unsigned n, const void *env, SpMat *mat)
{

   /* If the matrix is   */
   if (mat->ppty & EMPMAT_CSC) {
      rhpmat_reset(mat);
      rhpmat_set_csr(mat);
      mat->csr = ovf_speye_mat(2*n, n, 1.);
      if (!mat->csr) return Error_InsufficientMemory;

   } else {
      rhpmat_reset(mat);
      rhpmat_set_csr(mat);
      mat->csr = ovf_speye_mat(n, 2*n, 1.);
      if (!mat->csr) return Error_InsufficientMemory;
   }

   return OK;
}

static enum cone huber_gen_set_cones(unsigned n, const void* env, unsigned idx,
                                      void **cone_data)
{
   assert(idx < 2*n);
   *cone_data = NULL;
   if (idx < n) {
      return CONE_R_MINUS;
   } else {
      return CONE_R_PLUS;
   }
}

static const struct var_genops huber_varfill  = {
   .set_type = NULL,
   .get_lb = loss_kappa_var_lb,
   .get_ub = loss_kappa_var_ub,
   .set_level = NULL,
   .set_marginal = NULL,
};

static int huber_gen_var(Container* ctr, 
                         unsigned n, const void* p) {
   return ovf_box(ctr,  n, p, &huber_varfill);
}

static size_t huber_size_u(size_t n_args)
{
   return SIZE_U(n_args);
}
const struct ovf_param_def* const OVF_huber_params[] = {&loss_kappa};
const unsigned OVF_huber_params_len = sizeof(OVF_huber_params)/sizeof(struct ovf_param_def*);

const struct ovf_genops OVF_huber_datagen = {
   .b = NULL,
   .k = NULL,
   .B = ovf_gen_id_matrix,
   .D = ovf_gen_id_matrix,
   .J = ovf_gen_id_matrix,
   .M = ovf_gen_id_matrix,
   .set_A = huber_gen_A,
   .set_A_nonbox = NULL,
   .set_b = loss_kappa_gen_b,
   .set_b_nonbox = NULL,
   .set_b_0 = NULL,
   .set_cones = huber_gen_set_cones,
   .set_cones_nonbox = NULL,
   .size_u = huber_size_u,
   .u_shift = NULL,
   .var = huber_gen_var,
   .var_ppty = &huber_varfill,
   .name = xstr(OVF_NAME),
};
