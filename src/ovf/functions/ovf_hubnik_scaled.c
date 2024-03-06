#include "macros.h"
#include "printout.h"
#include "ovf_fn_helpers.h"
#include "ovf_hubnik_scaled.h"
#include "ovf_loss_common.h"
#include "ovf_parameter.h"
#include "ovf_generator.h"
#include "status.h"

#define OVF_NAME hubnik_scaled
#define SIZE_U(n) 2*n

static int hubnik_scaled_gen_M(unsigned n, const void *env, SpMat *mat)
{
   rhpmat_reset(mat);
   const struct ovf_param_list *p = (const struct ovf_param_list *) env;
   const struct ovf_param *kappa = ovf_find_param("kappa", p);

   OVF_CHECK_PARAM(kappa, Error_OvfMissingParam);

   switch (kappa->type) {
   case ARG_TYPE_SCALAR:
      A_CHECK(mat->csr, ovf_speye_mat(SIZE_U(n), SIZE_U(n), kappa->val));
      break;
   case ARG_TYPE_VEC:
   {
      A_CHECK(mat->csr, ovf_speye_mat(SIZE_U(n), SIZE_U(n), 1.));
      for (unsigned i = 0, j = n; i < n; ++i, ++j) {
         mat->csr->x[i] = kappa->vec[i];
         mat->csr->x[j] = kappa->vec[i];
      }
      break;
   }
   default:
       error("%s :: unsupported parameter type %d\n", __func__, kappa->type);
       return Error_InvalidValue;
   }

   rhpmat_set_eye(mat);
   rhpmat_set_csr(mat);
   rhpmat_set_sym(mat);

   return OK;
}

static int hubnik_scaled_gen_B(unsigned n, const void *env, SpMat *mat)
{
   rhpmat_reset(mat);
   A_CHECK(mat->csr, ovf_speye_mat(n, SIZE_U(n), 1.));
   rhpmat_set_csr(mat);
   /**/
   for (unsigned i = n; i < SIZE_U(n); ++i) {
      mat->csr->x[i] = -1.;
   }

   return OK;
}

static int hubnik_scaled_gen_A(unsigned n, const void *env, SpMat *mat)
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

static int hubnik_scaled_gen_b(unsigned n, const void* env, double **vals)
{
   MALLOC_(*vals, double, 2*SIZE_U(n));
   for (unsigned i = 0; i < SIZE_U(n); ++i) {
      (*vals)[i] = 1.;
   }

   memset(&(*vals)[SIZE_U(n)], 0, SIZE_U(n)*sizeof(double));

   return OK;
}

static enum cone hubnik_scaled_gen_set_cones(unsigned n, const void* env, unsigned idx,
                                       void **cone_data)
{
   *cone_data = NULL;
   return CONE_R_MINUS;
}

static int hubnik_scaled_gen_boff(unsigned n, const void* env, double **vals)
{
   const struct ovf_param_list *p = (const struct ovf_param_list *) env;
   const struct ovf_param *epsilon = ovf_find_param("epsilon", p);
   OVF_CHECK_PARAM(epsilon, Error_OvfMissingParam);

   MALLOC_(*vals, double, SIZE_U(n));

   switch(epsilon->type) {
   case ARG_TYPE_SCALAR:
   {
      double val = epsilon->val;
      for (unsigned i = 0; i < SIZE_U(n); ++i) {
         (*vals)[i] = -val;
      }
      break;
   }
   case ARG_TYPE_VEC:
   {
      for (unsigned i = 0; i < SIZE_U(n); ++i) {
         (*vals)[i] = -epsilon->vec[i];
      }
      break;
   }
   default:
       error("%s :: unsupported parameter type %d\n", __func__, epsilon->type);
       return Error_InvalidValue;
   }

   return OK;
}

static double hubnik_scaled_var_lb(const void* env, unsigned indx)
{
   return 0.;
}

static double hubnik_scaled_var_ub(const void* env, unsigned indx)
{
   return 1.;
}

static const struct var_genops hubnik_scaled_varfill  = {
   .set_type = NULL,
   .get_lb = hubnik_scaled_var_lb,
   .get_ub = hubnik_scaled_var_ub,
   .get_value = NULL,
   .get_multiplier = NULL,
};

static int hubnik_scaled_gen_var(Container* ctr,  unsigned n, const void* p)
{
   return ovf_box(ctr,  SIZE_U(n), p, &hubnik_scaled_varfill);
}

static size_t hubnik_scaled_size_u(size_t n_args)
{
   return SIZE_U(n_args);
}

const struct ovf_param_def* const OVF_hubnik_scaled_params[] = {&loss_epsilon, &loss_kappa};
const unsigned OVF_hubnik_scaled_params_len = sizeof(OVF_hubnik_scaled_params)/sizeof(struct ovf_param_def*);

const struct ovf_genops OVF_hubnik_scaled_datagen = {
   .M = hubnik_scaled_gen_M,
   .D = hubnik_scaled_gen_M,
   .J = hubnik_scaled_gen_M,
   .B = hubnik_scaled_gen_B,
   .b = hubnik_scaled_gen_boff,
   .k = NULL,
   .set_A = hubnik_scaled_gen_A,
   .set_A_nonbox = NULL,
   .set_b = hubnik_scaled_gen_b,
   .set_b_nonbox = NULL,
   .set_b_0 = NULL,
   .set_cones = hubnik_scaled_gen_set_cones,
   .set_cones_nonbox = NULL,
   .size_u = hubnik_scaled_size_u,
   .u_shift = NULL,
   .var = hubnik_scaled_gen_var,
   .var_ppty = &hubnik_scaled_varfill,
   .name = xstr(OVF_NAME),
};
