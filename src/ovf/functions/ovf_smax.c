#include "asnan.h"
#include "macros.h"
#include "ovf_functions_common.h"
#include "printout.h"
#include "ovf_smax.h"
#include "ovf_parameter.h"
#include "ovf_generator.h"
#include "status.h"

static int unit_simplex_gen_A(unsigned n, const void *env, SpMat *mat)
{

   /* If the matrix is   */
   if (mat->ppty & EMPMAT_CSC) {
      rhpmat_reset(mat);
      rhpmat_set_csr(mat);
      A_CHECK(mat->csr, rhp_spalloc(n+1, n, 2*n, RHP_CS));

      mat->csr->m = n;
      mat->csr->n = n+1;

      for (RHP_INT i = 0, j = 0; i < n; ++i, j += 2) {
         mat->csr->x[j]   = 1.;
         mat->csr->x[j+1] = 1.;
         mat->csr->p[i]   = j;
         mat->csr->i[j]   = i;
         mat->csr->i[j+1] = n;
      }
      mat->csr->p[n] = 2*n;
   } else {

      rhpmat_reset(mat);
      rhpmat_set_csr(mat);
      A_CHECK(mat->csr, rhp_spalloc(n, n+1, 2*n, RHP_CS));
      mat->csr->m = n+1;
      mat->csr->n = n;

      for (RHP_INT i = 0, j = n; i < n; ++i, ++j) {
         mat->csr->x[i]   = 1.;
         mat->csr->x[j]   = 1.;
         /* There is only 1 element for each row  */
         mat->csr->p[i]   = i;
         /* Everything is for the ith element  */
         mat->csr->i[i]   = i;
         mat->csr->i[j]   = i;
      }
      mat->csr->p[n] = n;
      mat->csr->p[n+1] = 2*n;
   }

   return OK;
}

static int unit_simplex_gen_A_nonbox(unsigned n, const void *env, SpMat *mat)
{

   /* If the matrix is   */
   if (mat->ppty & EMPMAT_CSC) {
      rhpmat_reset(mat);
      rhpmat_set_csr(mat);
      A_CHECK(mat->csr, rhp_spalloc(1, n, n, RHP_CS));

      mat->csr->m = n;
      mat->csr->n = 1;

      for (RHP_INT i = 0; i < n; ++i) {
         mat->csr->x[i]   = 1.;
         mat->csr->p[i]   = i;
         mat->csr->i[i]   = 0;
      }
      mat->csr->p[n] = n;
   } else {

      rhpmat_reset(mat);
      rhpmat_set_csr(mat);
      A_CHECK(mat->csr, rhp_spalloc(n, n+1, n, RHP_CS));
      mat->csr->m = 1;
      mat->csr->n = n;

      for (RHP_INT i = 0, j = n; i < n; ++i, ++j) {
         mat->csr->x[i]   = 1.;
         /* Everything is for the ith element  */
         mat->csr->i[i]   = i;
      }
      mat->csr->p[0] = 0;
      mat->csr->p[1] = n;
   }

   return OK;
}

static int unit_simplex_gen_b(unsigned n, const void* env, double **vals)
{
   CALLOC_(*vals, double, n+1);

   (*vals)[n] = 1;

   return OK;
}

static int unit_simplex_gen_b_nonbox(unsigned n, const void* env, double **vals)
{
   CALLOC_(*vals, double, 1);

   (*vals)[0] = 1;

   return OK;
}

static enum cone unit_simplex_gen_set_cones(unsigned n, const void* env, unsigned idx,
                            void **cone_data)
{
   *cone_data = NULL;

   if (idx < n)  { return CONE_R_PLUS; }
   if (idx == n) { return CONE_0; }

   return CONE_NONE;
}

static enum cone unit_simplex_gen_set_cones_nonbox(unsigned n, const void* env, unsigned idx,
                            void **cone_data)
{
   *cone_data = NULL;

   if (idx == 0) { return CONE_0; }

   return CONE_NONE;
}

static double unit_simplex_var_lb(const void* env, unsigned indx)
{
   return 0;
}

static double unit_simplex_var_ub(const void* env, unsigned indx)
{
   return INFINITY;
}

static const struct var_genops unit_simplex_varfill  = {
   .set_type = NULL,
   .get_lb = unit_simplex_var_lb,
   .get_ub = unit_simplex_var_ub,
   .get_value = NULL,
   .get_multiplier = NULL,
};

static int unit_simplex_gen_var(Container* ctr,  unsigned n, const void* p)
{
   return ovf_box(ctr,  n, p, &unit_simplex_varfill);
}

const struct ovf_param_def* const OVF_smax_params[] = {NULL};
const unsigned OVF_smax_params_len = 0;

const struct ovf_param_def* const OVF_smin_params[] = {NULL};
const unsigned OVF_smin_params_len = 0;

const struct ovf_genops OVF_smax_datagen = {
   .M = NULL,
   .D = NULL,
   .J = NULL,
   .B = NULL,
   .b = NULL,
   .k = NULL,
   .set_A = unit_simplex_gen_A,
   .set_A_nonbox = unit_simplex_gen_A_nonbox,
   .set_b = unit_simplex_gen_b,
   .set_b_nonbox = unit_simplex_gen_b_nonbox,
   .set_b_0 = NULL,
   .set_cones = unit_simplex_gen_set_cones,
   .set_cones_nonbox = unit_simplex_gen_set_cones_nonbox,
   .size_u = u_size_same,
   .u_shift = NULL,
   .var = unit_simplex_gen_var,
   .var_ppty = &unit_simplex_varfill,
   .name = "smax",
};

const struct ovf_genops OVF_smin_datagen = {
   .M = NULL,
   .D = NULL,
   .J = NULL,
   .B = NULL,
   .b = NULL,
   .k = NULL,
   .set_A = unit_simplex_gen_A,
   .set_A_nonbox = unit_simplex_gen_A_nonbox,
   .set_b = unit_simplex_gen_b,
   .set_b_nonbox = unit_simplex_gen_b_nonbox,
   .set_b_0 = NULL,
   .set_cones = unit_simplex_gen_set_cones,
   .set_cones_nonbox = unit_simplex_gen_set_cones_nonbox,
   .size_u = u_size_same,
   .u_shift = NULL,
   .var = unit_simplex_gen_var,
   .var_ppty = &unit_simplex_varfill,
   .name = "smin",
};
