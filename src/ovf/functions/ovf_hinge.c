#include "cones.h"
#include "macros.h"
#include "printout.h"
#include "ovf_hinge.h"
#include "ovf_loss_common.h"
#include "ovf_parameter.h"
#include "ovf_generator.h"
#include "status.h"

#define OVF_NAME hinge

#define SIZE_U(n) n

static int hinge_gen_A(unsigned n, const void *env, SpMat *mat)
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

static int hinge_gen_b(unsigned n, const void* env, double **vals)
{
   MALLOC_(*vals, double, 2*SIZE_U(n));

   for (unsigned i = 0; i < SIZE_U(n); ++i) {
      (*vals)[i] = 1.;
   }

   memset(&(*vals)[SIZE_U(n)], 0, SIZE_U(n)*sizeof(double));
   return OK;
}


static enum cone hinge_gen_set_cones(unsigned n, const void* env, unsigned idx,
                                      void **cone_data)
{
   *cone_data = NULL;
   return CONE_R_MINUS;
}

static double hinge_var_lb(const void* env, unsigned indx)
{
   return 0.;
}

static double hinge_var_ub(const void* env, unsigned indx)
{
   return 1.;
}

static const struct var_genops hinge_varfill  = {
   .set_type = NULL,
   .get_lb = hinge_var_lb,
   .get_ub = hinge_var_ub,
   .set_level = NULL,
   .set_marginal = NULL,
};

static int hinge_gen_var(Container* ctr,  unsigned n, const void* p)
{
   return ovf_box(ctr,  n, p, &hinge_varfill);
}

static size_t hinge_size_u(size_t n_args)
{
   return SIZE_U(n_args);
}

const struct ovf_param_def* const OVF_hinge_params[] = {&loss_epsilon};
const unsigned OVF_hinge_params_len = sizeof(OVF_hinge_params)/sizeof(struct ovf_param_def*);

const struct ovf_genops OVF_hinge_datagen = {
   .M = NULL,
   .D = NULL,
   .J = NULL,
   .B = ovf_gen_id_matrix,
   .b = loss_epsilon_gen_boff,
   .k = NULL,
   .set_A = hinge_gen_A,
   .set_A_nonbox = NULL,
   .set_b = hinge_gen_b,
   .set_b_nonbox = NULL,
   .set_b_0 = NULL,
   .set_cones = hinge_gen_set_cones,
   .set_cones_nonbox = NULL,
   .size_u = hinge_size_u,
   .u_shift = NULL,
   .var = hinge_gen_var,
   .var_ppty = &hinge_varfill,
   .name = xstr(OVF_NAME),
};
