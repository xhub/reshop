#include "macros.h"
#include "ovf_cvar_common.h"
#include "ovf_ecvarup.h"
#include "ovf_fn_helpers.h"
#include "ovf_generator.h"
#include "ovf_parameter.h"
#include "ovf_risk_measure_common.h"
#include "printout.h"
#include "status.h"

int cvar_gen_A(unsigned n, const void *env, SpMat *mat)
{

   /* If the matrix is   */
   if (mat->ppty & EMPMAT_CSC) {
      rhpmat_reset(mat);
      rhpmat_set_csr(mat);
      A_CHECK(mat->csr, rhp_spalloc(2*n+1, n, 3*n, RHP_CS));
      mat->csr->m = n;
      mat->csr->n = 2*n+1;

      for (size_t i = 0, j = 0; i < n; ++i, j += 3) {
         mat->csr->x[j]   = -1.;
         mat->csr->x[j+1] = 1.;
         mat->csr->x[j+2] = 1.;
         mat->csr->p[i]   = j;
         mat->csr->i[j]   = i;
         mat->csr->i[j+1] = i+n;
         mat->csr->i[j+2] = 2*n;
         mat->csr->p[n] = 3*n;
      }
   } else {
      rhpmat_reset(mat);
      rhpmat_set_csr(mat);
      A_CHECK(mat->csr, rhp_spalloc(n, 2*n+1, 3*n, RHP_CS));
      mat->csr->m = 2*n+1;
      mat->csr->n = n;

      for (size_t i = 0, j = n, k = 2*n; i < n; ++i, ++j, ++k) {
         mat->csr->x[i]   = -1.;
         mat->csr->x[j]   = 1.;
         mat->csr->x[k]   = 1.; /*  For the last row */
         /* There is only 1 element for each row  */
         mat->csr->p[i]   = i;
         mat->csr->p[j]   = j;
         /* Everything is for the ith element  */
         mat->csr->i[i]   = i;
         mat->csr->i[j]   = i;
         mat->csr->i[k]   = i;
      }
      mat->csr->p[2*n] = 2*n;
      mat->csr->p[2*n+1] = 3*n;
   }

   return OK;
}

int cvar_gen_A_nonbox(unsigned n, const void *env, SpMat *mat)
{
   if (mat->ppty & EMPMAT_CSC) {
      rhpmat_reset(mat);
      rhpmat_set_csr(mat);
      A_CHECK(mat->csr, rhp_spalloc(1, n, n, RHP_CS));
      mat->csr->m = n;
      mat->csr->n = 1;

      for (unsigned i = 0; i < n; ++i) {
         mat->csr->x[i] = 1.;
         mat->csr->p[i] = i;
         mat->csr->i[i] = 0;
      }
      mat->csr->p[n] = n;
   } else {
      rhpmat_reset(mat);
      rhpmat_set_csr(mat);
      A_CHECK(mat->csr, rhp_spalloc(n, 1, n, RHP_CS));
      mat->csr->m = 1;
      mat->csr->n = n;

      for (unsigned i = 0; i < n; ++i) {
         mat->csr->x[i] = 1.;
         mat->csr->i[i] = i;
      }
      mat->csr->p[0] = 0;
      mat->csr->p[1] = n;
   }

   return OK;
}

int cvar_gen_b_nonbox(unsigned n, const void* env, double **vals)
{
   MALLOC_(*vals, double, 1);
   (*vals)[0] = 1.;

   return OK;
}

enum cone cvar_gen_set_cones(unsigned n, const void* env, unsigned idx,
                              void **cone_data)
{
   assert(idx < 2*n+1);
   *cone_data = NULL;
   if (idx < 2*n) {
      return CONE_R_MINUS;
   }

   return CONE_0;
}

enum cone cvar_gen_set_cones_nonbox(unsigned n, const void* env, unsigned idx,
                                     void **cone_data)
{
   assert(idx == 0);
   *cone_data = NULL;
   return CONE_0;
}

int cvar_u_shift(unsigned n, const void *env, double **u_shift)
{
   const struct ovf_param_list *p = (const struct ovf_param_list *) env;
   const struct ovf_param *probs = ovf_find_param("probabilities", p);
   OVF_CHECK_PARAM(probs, Error_OvfMissingParam);

   return risk_measure_u_shift(n, probs, u_shift);
}


