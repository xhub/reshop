#include "asnan.h"

#include "macros.h"
#include "printout.h"
#include "ovf_fn_helpers.h"
#include "ovf_hubnik.h"
#include "ovf_loss_common.h"
#include "ovf_parameter.h"
#include "ovf_generator.h"
#include "status.h"

#define OVF_NAME hubnik
#define SIZE_U(n) 2*n

static int hubnik_gen_M(unsigned n, const void *env, SpMat *mat)
{
   return ovf_gen_id_matrix(SIZE_U(n), env, mat);
}

static int hubnik_gen_B(unsigned n, const void *env, SpMat *mat)
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

static int hubnik_gen_A(unsigned n, const void *env, SpMat *mat)
{

   /* If the matrix is   */
   if (mat->ppty & EMPMAT_CSC) {
      rhpmat_reset(mat);
      rhpmat_set_csr(mat);
      mat->csr = ovf_speye_mat(2*SIZE_U(n), SIZE_U(n), 1.);
      if (!mat->csr) return Error_InsufficientMemory;

   } else {
      return Error_InvalidValue;
   }

   return OK;
}

static int hubnik_gen_b(unsigned n, const void* env, double **vals)
{
   const struct ovf_param_list *p = (const struct ovf_param_list *) env;
   const struct ovf_param *kappa = ovf_find_param("kappa", p);

   OVF_CHECK_PARAM(kappa, Error_OvfMissingParam);

   MALLOC_(*vals, double, 2*SIZE_U(n));

   switch (kappa->type) {
   case ARG_TYPE_SCALAR:
      for (unsigned i = 0, j = n; j < SIZE_U(n); ++i, ++j) {
         (*vals)[i] = kappa->val;
         (*vals)[j] = -kappa->val;
      }

      break;
   case ARG_TYPE_VEC:
   {
      for (unsigned i = 0, j = n; i < n; ++i, ++j) {
         (*vals)[i] = kappa->vec[i];
         (*vals)[j] = -kappa->vec[i];
      }
      break;
   }
   default:
       error("%s :: unsupported parameter type %d\n", __func__, kappa->type);
       return Error_InvalidValue;
   }

   memset(&(*vals)[SIZE_U(n)], 0, SIZE_U(n)*sizeof(double));

   return OK;
}

static enum cone hubnik_gen_set_cones(unsigned n, const void* env, unsigned idx,
                                       void **cone_data)
{
   *cone_data = NULL;
   assert(idx < SIZE_U(n));

   if (idx < n) {
      return CONE_R_MINUS;
   } else {
      return CONE_R_PLUS;
   }
}

static int hubnik_gen_boff(unsigned n, const void* env, double **vals)
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

static double hubnik_var_lb(const void* env, unsigned indx)
{
   return 0.;
}

static double hubnik_var_ub(const void *env, unsigned idx)
{
   const struct ovf_param_list *p = (const struct ovf_param_list *) env;
   const struct ovf_param *kappa = ovf_find_param("kappa", p);

   OVF_CHECK_PARAM(kappa, MKSNAN(Error_OvfMissingParam));



   if (idx > 2*kappa->size_vector) {
      error("%s :: index out of range: %d > %d\n", __func__, idx, 2*kappa->size_vector);
      return MKSNAN(Error_IndexOutOfRange);
   }

   switch (kappa->type) {
   case ARG_TYPE_SCALAR:
      return kappa->val;
   case ARG_TYPE_VEC:
      return kappa->vec[idx%kappa->size_vector];
   default:
       error("%s :: unsupported parameter type %d\n", __func__, kappa->type);
       return MKSNAN(Error_InvalidValue);
   }
}

static const struct var_genops hubnik_varfill  = {
   .set_type = NULL,
   .get_lb = hubnik_var_lb,
   .get_ub = hubnik_var_ub,
   .set_level = NULL,
   .set_marginal = NULL,
};

static int hubnik_gen_var(Container* ctr,  unsigned n, const void* p)
{
   return ovf_box(ctr,  SIZE_U(n), p, &hubnik_varfill);
}

static size_t hubnik_size_u(size_t n_args)
{
   return SIZE_U(n_args);
}

const struct ovf_param_def* const OVF_hubnik_params[] = {&loss_epsilon, &loss_kappa};
const unsigned OVF_hubnik_params_len = sizeof(OVF_hubnik_params)/sizeof(struct ovf_param_def*);

const struct ovf_genops OVF_hubnik_datagen = {
   .M = hubnik_gen_M,
   .D = hubnik_gen_M,
   .J = hubnik_gen_M,
   .B = hubnik_gen_B,
   .b = hubnik_gen_boff,
   .k = NULL,
   .set_A = hubnik_gen_A,
   .set_A_nonbox = NULL,
   .set_b = hubnik_gen_b,
   .set_b_nonbox = NULL,
   .set_b_0 = NULL,
   .set_cones = hubnik_gen_set_cones,
   .set_cones_nonbox = NULL,
   .size_u = hubnik_size_u,
   .u_shift = NULL,
   .var = hubnik_gen_var,
   .var_ppty = &hubnik_varfill,
   .name = xstr(OVF_NAME),
};
