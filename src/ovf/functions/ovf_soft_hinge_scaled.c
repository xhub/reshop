#include "macros.h"
#include "printout.h"
#include "ovf_generator.h"
#include "ovf_loss_common.h"
#include "ovf_parameter.h"
#include "ovf_soft_hinge_scaled.h"
#include "status.h"

#define OVF_NAME soft_hinge_scaled
#define SIZE_U(n) n

static int soft_hinge_scaled_gen_A(unsigned n, const void *env, SpMat *mat)
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

static int soft_hinge_scaled_gen_b(unsigned n, const void* env, double **vals)
{
   MALLOC_(*vals, double, 2*SIZE_U(n));

   for (unsigned i = 0; i < SIZE_U(n); ++i) {
      (*vals)[i] = 1.;
   }

   memset(&(*vals)[SIZE_U(n)], 0, SIZE_U(n)*sizeof(double));

  return OK;
}


static enum cone soft_hinge_scaled_gen_set_cones(unsigned n, const void* env, unsigned idx,
                                void **cone_data)
{
   *cone_data = NULL;
   return CONE_R_MINUS;
}

static double soft_hinge_scaled_var_lb(const void* env, unsigned indx)
{
   return 0.;
}

static double soft_hinge_scaled_var_ub(const void* env, unsigned indx)
{
   return 1.;
}

static const struct var_genops soft_hinge_scaled_varfill  = {
   .set_type = NULL,
   .get_lb = soft_hinge_scaled_var_lb,
   .get_ub = soft_hinge_scaled_var_ub,
   .set_level = NULL,
   .set_marginal = NULL,
};

static int soft_hinge_scaled_gen_var(Container* ctr,  unsigned n, const void* p)
{
   return ovf_box(ctr,  n, p, &soft_hinge_scaled_varfill);
}

static size_t soft_hinge_scaled_size_u(size_t n_args)
{
   return SIZE_U(n_args);
}
const struct ovf_param_def* const OVF_soft_hinge_scaled_params[] = {&loss_epsilon, &loss_kappa};
const unsigned OVF_soft_hinge_scaled_params_len = sizeof(OVF_soft_hinge_scaled_params)/sizeof(struct ovf_param_def*);

const struct ovf_genops OVF_soft_hinge_scaled_datagen = {
   .M = loss_kappa_gen_M,
   .D = ovf_gen_id_matrix,
   .J = loss_kappa_gen_M,
   .B = ovf_gen_id_matrix,
   .b = loss_epsilon_gen_boff,
   .k = NULL,
   .set_A = soft_hinge_scaled_gen_A,
   .set_A_nonbox = NULL,
   .set_b = soft_hinge_scaled_gen_b,
   .set_b_nonbox = NULL,
   .set_b_0 = NULL,
   .set_cones = soft_hinge_scaled_gen_set_cones,
   .set_cones_nonbox = NULL,
   .size_u = soft_hinge_scaled_size_u,
   .u_shift = NULL,
   .var = soft_hinge_scaled_gen_var,
   .var_ppty = &soft_hinge_scaled_varfill,
   .name = xstr(OVF_NAME),
};
