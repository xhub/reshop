#include "ovf_sum_pos_part.h"
#include "ovf_parameter.h"
#include "ovf_generator.h"
#include "printout.h"
#include "status.h"
#include "macros.h"
#include "cones.h"

#define OVF_NAME sum_pos_part
#define SIZE_U(n) n

/* Set is [  I ] - 1 <= 0
 *        [ -I ]     <= 0 */
static int sum_pos_part_gen_A(unsigned n, const void *env, SpMat *mat)
{
   /* If the matrix is   */
   if (mat->ppty & EMPMAT_CSC) {
      TO_IMPLEMENT("CSC matrix");
      //rhpmat_reset(mat);
      //rhpmat_set_csr(mat);
      //mat->csr = ovf_speye_mat(2*n, n, 1.);
      //if (!mat->csr) return Error_InsufficientMemory;
      //for (size_t i = 1; i < 2*n; i += 2) {
      //   mat->csr->x[i] = -1.;
      //}

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


static int sum_pos_part_gen_b(unsigned n, const void* env, double **vals)
{
   double *lvals;
   MALLOC_(lvals, double, 2*n);
   *vals = lvals;

   for (size_t i = 0; i < n; ++i) {
      lvals[i] = 1.;
   }
   memset(&lvals[n], 0, n * sizeof(double));

   return OK;
}

static enum cone sum_pos_part_gen_set_cones(unsigned n, const void* env, unsigned idx,
                            void **cone_data)
{
   *cone_data = NULL;
   return CONE_R_MINUS;
}

static double sum_pos_part_var_lb(const void* env, unsigned indx)
{
   return 0;
}

static double sum_pos_part_var_ub(const void* env, unsigned indx)
{
   return 1.;
}

static const struct var_genops sum_pos_part_varfill  = {
   .set_type = NULL,
   .get_lb = sum_pos_part_var_lb,
   .get_ub = sum_pos_part_var_ub,
   .set_level = NULL,
   .set_marginal = NULL,
};

static int sum_pos_part_gen_var(Container* ctr,  unsigned n, const void* p)
{
   return ovf_box(ctr, n, p, &sum_pos_part_varfill);
}

static size_t sum_pos_part_size_u(size_t n_args)
{
   return SIZE_U(n_args);
}

const struct ovf_param_def* const OVF_sum_pos_part_params[] = {NULL};
const unsigned OVF_sum_pos_part_params_len = 0;

const struct ovf_genops OVF_sum_pos_part_datagen = {
   .M = NULL,
   .D = NULL,
   .J = NULL,
   .B = NULL,
   .b = NULL,
   .k = NULL,
   .set_A = sum_pos_part_gen_A,
   .set_A_nonbox = NULL,
   .set_b = sum_pos_part_gen_b,
   .set_b_nonbox = NULL,
   .set_b_0 = NULL,
   .set_cones = sum_pos_part_gen_set_cones,
   .set_cones_nonbox = NULL,
   .size_u = sum_pos_part_size_u,
   .u_shift = NULL,
   .var = sum_pos_part_gen_var,
   .var_ppty = &sum_pos_part_varfill,
   .name = xstr(OVF_NAME),
};
