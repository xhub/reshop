#include "ovf_l1.h"
#include "ovf_parameter.h"
#include "ovf_generator.h"
#include "status.h"
#include "macros.h"
#include "cones.h"

#define OVF_NAME l1
#define SIZE_U(n) n

static int l1_gen_A(unsigned n, const void *env, SpMat *mat)
{

   /* If the matrix is   */
   if (mat->ppty & EMPMAT_CSC) {
      rhpmat_reset(mat);
      rhpmat_set_csr(mat);
      mat->csr = ovf_speye_mat(2*n, n, 1.);
      if (!mat->csr) return Error_InsufficientMemory;
      for (size_t i = 1; i < 2*n; i += 2) {
         mat->csr->x[i] = -1.;
      }

   } else {
      rhpmat_set_csr(mat);
      mat->csr = ovf_speye_mat(n, 2*n, 1.);
      if (!mat->csr) return Error_InsufficientMemory;
      for (size_t i = n; i < 2*n; ++i) {
         mat->csr->x[i] = -1.;
      }
   }

   return OK;
}

static int l1_gen_b(unsigned n, const void* env, double **vals)
{
   MALLOC_(*vals, double, 2*n);

   for (size_t i = 0; i < 2*n; ++i) {
      (*vals)[i] = 1.;
   }

   return OK;
}

static enum cone l1_gen_set_cones(unsigned n, const void* env, unsigned idx,
                            void **cone_data)
{
   *cone_data = NULL;
   return CONE_R_MINUS;
}

static double l1_var_lb(const void* env, unsigned indx)
{
   return -1.;
}

static double l1_var_ub(const void* env, unsigned indx)
{
   return 1.;
}

static const struct var_genops l1_varfill  = {
   .set_type = NULL,
   .get_lb = l1_var_lb,
   .get_ub = l1_var_ub,
   .set_level = NULL,
   .set_marginal = NULL,
};

static int l1_gen_var(Container* ctr,  unsigned n, const void* p)
{
   return ovf_box(ctr,  n, p, &l1_varfill);
}

static size_t l1_size_u(size_t n_args)
{
   return SIZE_U(n_args);
}
const struct ovf_param_def* const OVF_l1_params[] = {NULL};
const unsigned OVF_l1_params_len = 0;

const struct ovf_genops OVF_l1_datagen = {
   .M = NULL,
   .D = NULL,
   .J = NULL,
   .B = NULL,
   .b = NULL,
   .k = NULL,
   .set_A = l1_gen_A,
   .set_A_nonbox = NULL,
   .set_b = l1_gen_b,
   .set_b_nonbox = NULL,
   .set_b_0 = NULL,
   .set_cones = l1_gen_set_cones,
   .set_cones_nonbox = NULL,
   .size_u = l1_size_u,
   .u_shift = NULL,
   .var = l1_gen_var,
   .var_ppty = &l1_varfill,
   .name = xstr(OVF_NAME),
};
