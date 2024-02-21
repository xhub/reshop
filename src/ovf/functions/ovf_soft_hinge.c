#include "macros.h"
#include "printout.h"
#include "ovf_generator.h"
#include "ovf_loss_common.h"
#include "ovf_parameter.h"
#include "ovf_soft_hinge.h"
#include "status.h"

#define OVF_NAME soft_hinge
#define SIZE_U(n) n

static int soft_hinge_gen_A(unsigned n, const void *env, SpMat *mat)
{

   /* If the matrix is   */
   if (mat->ppty & EMPMAT_CSC) {
      rhpmat_reset(mat);
      rhpmat_set_csr(mat);
      mat->csr = ovf_speye_mat(2*SIZE_U(n), SIZE_U(n), 1.);
      if (!mat->csr) return Error_InsufficientMemory;
      for (size_t i = 1; i < 2*SIZE_U(n); i += 2) {
         mat->csr->x[i] = -1.;
      }
   } else {
      return Error_InvalidValue;
   }

   return OK;
}

static enum cone soft_hinge_gen_set_cones(unsigned n, const void* env, unsigned idx,
                                void **cone_data)
{
   *cone_data = NULL;
   return CONE_R_MINUS;
}

static double soft_hinge_var_lb(const void* env, unsigned indx)
{
   return 0.;
}

static const struct var_genops soft_hinge_varfill  = {
   .set_type = NULL,
   .get_lb = soft_hinge_var_lb,
   .get_ub = loss_kappa_var_ub,
   .set_level = NULL,
   .set_marginal = NULL,
};

static int soft_hinge_gen_var(Container* ctr,  unsigned n, const void* p)
{
   return ovf_box(ctr,  n, p, &soft_hinge_varfill);
}

static size_t soft_hinge_size_u(size_t n_args)
{
   return SIZE_U(n_args);
}
const struct ovf_param_def* const OVF_soft_hinge_params[] = {&loss_epsilon, &loss_kappa};
const unsigned OVF_soft_hinge_params_len = sizeof(OVF_soft_hinge_params)/sizeof(struct ovf_param_def*);

const struct ovf_genops OVF_soft_hinge_datagen = {
   .M = ovf_gen_id_matrix,
   .D = ovf_gen_id_matrix,
   .J = ovf_gen_id_matrix,
   .B = ovf_gen_id_matrix,
   .b = loss_epsilon_gen_boff,
   .k = NULL,
   .set_A = soft_hinge_gen_A,
   .set_A_nonbox = NULL,
   .set_b = loss_kappa_gen_b,
   .set_b_nonbox = NULL,
   .set_b_0 = NULL,
   .set_cones = soft_hinge_gen_set_cones,
   .set_cones_nonbox = NULL,
   .size_u = soft_hinge_size_u,
   .u_shift = NULL,
   .var = soft_hinge_gen_var,
   .var_ppty = &soft_hinge_varfill,
   .name = xstr(OVF_NAME),
};
