#include "macros.h"
#include "printout.h"
#include "ovf_fn_helpers.h"
#include "ovf_generator.h"
#include "ovf_parameter.h"
#include "ovf_vapnik.h"
#include "status.h"

#define OVF_NAME vapnik
#define SIZE_U(n) 2*n

const static struct ovf_param_def loss_epsilon = {
   .name = "epsilon",
   .help = "Bounds on the variable u. If a scalar, -kappa <= u <= kappa. \
            If a vector, than previous inequality is understood componentwise",
   .mandatory = true,
   .allow_data = true,
   .allow_var = false,
   .allow_fun = false,
   .allow_scalar = true,
   .allow_vector = true,
   .allow_matrix = false,
   .get_vector_size = ovf_same_as_argsize,
//   .check_vector_size = ovf_same_as_m,
};

static int vapnik_gen_B(unsigned n, const void *env, SpMat *mat)
{
   rhpmat_reset(mat);
   mat->csr = ovf_speye_mat(n, SIZE_U(n), 1.);
   if (!mat->csr) return Error_InsufficientMemory;
   rhpmat_set_csr(mat);
   /**/
   for (unsigned i = n; i < SIZE_U(n); ++i) {
      mat->csr->x[i] = -1.;
   }

   return OK;
}

static int vapnik_gen_boff(unsigned n, const void* env, double **vals)
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

static int vapnik_gen_A(unsigned n, const void *env, SpMat *mat)
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
      rhpmat_set_csr(mat);
      mat->csr = ovf_speye_mat(SIZE_U(n), 2*SIZE_U(n), 1.);
      if (!mat->csr) return Error_InsufficientMemory;
      for (size_t i = n; i < 2*n; ++i) {
         mat->csr->x[i] = -1.;
      }
   }

   return OK;
}

static int vapnik_gen_b(unsigned n, const void* env, double **vals)
{
   MALLOC_(*vals, double, 2*SIZE_U(n));
   for (unsigned i = 0; i < SIZE_U(n); ++i) {
      (*vals)[i] = 1.;
   }
   memset(&(*vals)[SIZE_U(n)], 0, SIZE_U(n)*sizeof(double));
   return OK;
}


static enum cone vapnik_gen_set_cones(unsigned n, const void* env, unsigned idx,
                                void **cone_data)
{
   *cone_data = NULL;
   return CONE_R_MINUS;
}

static double vapnik_var_lb(const void* env, unsigned indx)
{
   return 0.;
}

static double vapnik_var_ub(const void* env, unsigned indx)
{
   return 1.;
}

static const struct var_genops vapnik_varfill  = {
   .set_type = NULL,
   .get_lb = vapnik_var_lb,
   .get_ub = vapnik_var_ub,
   .set_level = NULL,
   .set_marginal = NULL,
};

static int vapnik_gen_var(Container* ctr,  unsigned n, const void* p)
{
   return ovf_box(ctr,  SIZE_U(n), p, &vapnik_varfill);
}

static size_t vapnik_size_u(size_t n_args)
{
   return SIZE_U(n_args);
}

const struct ovf_param_def* const OVF_vapnik_params[] = {&loss_epsilon};
const unsigned OVF_vapnik_params_len = sizeof(OVF_vapnik_params)/sizeof(struct ovf_param_def*);

const struct ovf_genops OVF_vapnik_datagen = {
   .M = NULL,
   .D = NULL,
   .J = NULL,
   .B = vapnik_gen_B,
   .b = vapnik_gen_boff,
   .k = NULL,
   .set_A = vapnik_gen_A,
   .set_A_nonbox = NULL,
   .set_b = vapnik_gen_b,
   .set_b_nonbox = NULL,
   .set_b_0 = NULL,
   .set_cones = vapnik_gen_set_cones,
   .set_cones_nonbox = NULL,
   .size_u = vapnik_size_u,
   .u_shift = NULL,
   .var = vapnik_gen_var,
   .var_ppty = &vapnik_varfill,
   .name = xstr(OVF_NAME),
};
