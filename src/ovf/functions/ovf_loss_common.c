#include "asnan.h"

#include "rhp_LA.h"
#include "macros.h"
#include "ovf_fn_helpers.h"
#include "ovf_generator.h"
#include "ovf_parameter.h"
#include "ovf_loss_common.h"
#include "printout.h"
#include "status.h"

const struct ovf_param_def loss_kappa = {
   .name = "kappa",
   .help = "Smoothing parameter for loss function.",
   .mandatory = true,
   .allow_data = true,
   .allow_var = false,
   .allow_fun = false,
   .allow_scalar = true,
   .allow_vector = true,
   .allow_matrix = false,
   .get_vector_size = ovf_same_as_argsize,
};

const struct ovf_param_def loss_epsilon = {
   .name = "epsilon",
   .help = "Insensitivity parameter",
   .mandatory = true,
   .allow_data = true,
   .allow_var = false,
   .allow_fun = false,
   .allow_scalar = true,
   .allow_vector = true,
   .allow_matrix = false,
   .get_vector_size = ovf_same_as_argsize,
};

int loss_kappa_gen_M(unsigned n, const void *env, SpMat *mat)
{
   rhpmat_reset(mat);
   const struct ovf_param_list *p = (const struct ovf_param_list *) env;
   const struct ovf_param *kappa = ovf_find_param("kappa", p);

   OVF_CHECK_PARAM(kappa, Error_OvfMissingParam);

   switch (kappa->type) {
   case ARG_TYPE_SCALAR:
      mat->csr = ovf_speye_mat(n, n, kappa->val);
      break;
   case ARG_TYPE_VEC:
   {
      mat->csr = ovf_speye_mat(n, n, 1.);
      for (unsigned i = 0; i < n; ++i) {
         mat->csr->x[i] = kappa->vec[i];
      }
      break;
   }
   default:
       error("%s :: unsupported parameter type %d\n", __func__, kappa->type);
       return Error_InvalidValue;
   }

   if (!mat->csr) return Error_InsufficientMemory;
   rhpmat_set_eye(mat);
   rhpmat_set_csr(mat);
   rhpmat_set_sym(mat);

   return OK;
}

int loss_epsilon_gen_boff(unsigned n, const void* env, double **vals)
{
   const struct ovf_param_list *p = (const struct ovf_param_list *) env;
   const struct ovf_param *epsilon = ovf_find_param("epsilon", p);
   OVF_CHECK_PARAM(epsilon, Error_OvfMissingParam);

   MALLOC_(*vals, double, n);
   switch(epsilon->type) {
   case ARG_TYPE_SCALAR:
   {
      double val = epsilon->val;
      for (unsigned i = 0; i < n; ++i) {
         (*vals)[i] = -val;
      }
      break;
   }
   case ARG_TYPE_VEC:
   {
      for (unsigned i = 0; i < n; ++i) {
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

int loss_kappa_gen_b(unsigned n, const void *env, double **vals)
{
   const struct ovf_param_list *p = (const struct ovf_param_list *) env;
   const struct ovf_param *kappa = ovf_find_param("kappa", p);

   OVF_CHECK_PARAM(kappa, Error_OvfMissingParam);

   MALLOC_(*vals, double, 2*n);

   switch (kappa->type) {
   case ARG_TYPE_SCALAR:
   {
      for (size_t i = 0; i < 2*n; ++i) {
         (*vals)[i] = kappa->val;
      }
      break;
   }
   case ARG_TYPE_VEC:
   {
      for (unsigned i = 0; i < n; ++i) {
         (*vals)[i] = kappa->vec[i];
         (*vals)[i+i] = kappa->vec[i];
      }
      break;
   }
   default:
       error("%s :: unsupported parameter type %d\n", __func__, kappa->type);
       return Error_InvalidValue;
   }

   return OK;
}

double loss_kappa_var_lb(const void *env, unsigned idx)
{
   const struct ovf_param_list *p = (const struct ovf_param_list *) env;
   const struct ovf_param *kappa = ovf_find_param("kappa", p);

   OVF_CHECK_PARAM(kappa, MKSNAN(Error_OvfMissingParam));



   if (idx > kappa->size_vector) {
      error("%s :: index out of range: %d > %d\n", __func__, idx, kappa->size_vector);
      return MKSNAN(Error_IndexOutOfRange);
   }

   switch (kappa->type) {
   case ARG_TYPE_SCALAR:
      return -kappa->val;
   case ARG_TYPE_VEC:
      return -kappa->vec[idx];
   default:
       error("%s :: unsupported parameter type %d\n", __func__, kappa->type);
       return MKSNAN(Error_InvalidValue);
   }
}

double loss_kappa_var_ub(const void *env, unsigned idx)
{
   const struct ovf_param_list *p = (const struct ovf_param_list *) env;
   const struct ovf_param *kappa = ovf_find_param("kappa", p);

   OVF_CHECK_PARAM(kappa, MKSNAN(Error_OvfMissingParam));



   if (idx > kappa->size_vector) {
      error("%s :: index out of range: %d > %d\n", __func__, idx, kappa->size_vector);
      return MKSNAN(Error_IndexOutOfRange);
   }

   switch (kappa->type) {
   case ARG_TYPE_SCALAR:
      return kappa->val;
   case ARG_TYPE_VEC:
      return kappa->vec[idx];
   default:
       error("%s :: unsupported parameter type %d\n", __func__, kappa->type);
       return MKSNAN(Error_InvalidValue);
   }
}
