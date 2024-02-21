#include "asnan.h"
#include "macros.h"
#include "printout.h"
#include "ovf_elastic_net.h"
#include "ovf_fn_helpers.h"
#include "ovf_parameter.h"
#include "ovf_generator.h"
#include "status.h"

#define OVF_NAME elastic_net
#define SIZE_U(n) 2*n

static const struct ovf_param_def elastic_net_lambda = {
   .name = "lambda",
   .help = "Coefficient on the L2 part of the OVF",
   .mandatory = true,
   .allow_data = true,
   .allow_var = false,
   .allow_fun = false,
   .allow_scalar = true,
   .allow_vector = true,
   .allow_matrix = false,
   .get_vector_size = ovf_same_as_argsize,
};

static int elastic_net_gen_M(unsigned n, const void *env, SpMat *mat)
{
   rhpmat_reset(mat);
   CALLOC_(mat->block, struct block_spmat, 1);
   mat->block->m = SIZE_U(n);
   mat->block->n = SIZE_U(n);
   mat->block->number = 2;

   MALLOC_(mat->block->blocks, struct sp_matrix *, 2);
   MALLOC_(mat->block->row_starts, unsigned, 2);

   A_CHECK(mat->block->blocks[0], ovf_speye_mat(n, n, 1.));
   A_CHECK(mat->block->blocks[1], ovf_speye_mat(n, n, 0.));

   mat->block->row_starts[0] = 0;
   mat->block->row_starts[1] = n;

   rhpmat_set_eye(mat);
   rhpmat_set_block(mat);
   rhpmat_set_sym(mat);

   return OK;
}

static int elastic_net_gen_B(unsigned n, const void *env, SpMat *mat)
{
   rhpmat_reset(mat);
   A_CHECK(mat->csr, ovf_speye_mat(n, SIZE_U(n), 1.));
   rhpmat_set_csr(mat);
   /**/
   return OK;
}

static int elastic_net_gen_A(unsigned n, const void *env, SpMat *mat)
{

   /* If the matrix is   */
   if (mat->ppty & EMPMAT_CSC) {
      rhpmat_reset(mat);
      rhpmat_set_csr(mat);
      A_CHECK(mat->csr, rhp_spalloc(SIZE_U(n), SIZE_U(n), SIZE_U(n), RHP_CS));
      MALLOC_(mat->csr->x, double, SIZE_U(n));
      mat->csr->n = SIZE_U(n);
      mat->csr->m = SIZE_U(n);
      memset(mat->csr->p, 0, n*sizeof(double));

      for (size_t i = 0, j = 0; i < 2*n; i += 2, ++j) {
         mat->csr->x[i] = 1.;
         mat->csr->x[i+1] = -1.;
         /* \TODO(xhub) investigate why it segfault when mat->csr->p[n] = i; */
         mat->csr->p[j+n] = i;
         mat->csr->i[i] = j;
         mat->csr->i[i+1] = j+n;
      }
      mat->csr->p[SIZE_U(n)] = SIZE_U(n);

   } else {
      TO_IMPLEMENT("CSR mat")
      return Error_InvalidValue;
   }

   return OK;
}

static int elastic_net_gen_b(unsigned n, const void* env, double **vals)
{
   const struct ovf_param_list *p = (const struct ovf_param_list*) env;
   const struct ovf_param *lambda = ovf_find_param("lambda", p);

   OVF_CHECK_PARAM(lambda, Error_OvfMissingParam);

   MALLOC_(*vals, double, SIZE_U(n));

   switch(lambda->type) {
   case ARG_TYPE_SCALAR:
   {
      double val = lambda->val;
      for (unsigned i = 0; i < SIZE_U(n); ++i) {
         (*vals)[i] = val;
      }
      break;
   }
   case ARG_TYPE_VEC:
   {
      /*  \TODO(xhub) memcpy */
      for (unsigned i = 0; i < SIZE_U(n); ++i) {
         (*vals)[i] = lambda->vec[i];
      }
      break;
   }
   default:
       error("%s :: unsupported parameter type %d\n", __func__, lambda->type);
       return Error_InvalidValue;
   }

   return OK;
}

static enum cone elastic_net_gen_set_cones(unsigned n, const void* env, unsigned idx,
                                      void **cone_data)
{
   *cone_data = NULL;

   if (idx < n) return CONE_R;
   else         return CONE_R_MINUS;
}

static double elastic_net_var_lb(const void* env, unsigned indx)
{
   const struct ovf_param_list* p = (const struct ovf_param_list*) env;
   const struct ovf_param *lambda = ovf_find_param("lambda", p);

   OVF_CHECK_PARAM(lambda, SNAN);

   double val;

   unsigned n = lambda->size_vector;
   if (indx < n) return -INFINITY;

   switch(lambda->type) {
   case ARG_TYPE_SCALAR:
      val = -lambda->val;
      break;
   case ARG_TYPE_VEC:
      /* \TODO(xhub) indx or indx%n? */
      val = -lambda->vec[indx];
      break;
   default:
      error("%s :: unsupported parameter type %d\n", __func__, lambda->type);
      val = NAN;
   }

   return val;
}

static double elastic_net_var_ub(const void* env, unsigned indx)
{
   const struct ovf_param_list* p = (const struct ovf_param_list*) env;
   const struct ovf_param *lambda = ovf_find_param("lambda", p);

   OVF_CHECK_PARAM(lambda, SNAN);

   double val;

   unsigned n = lambda->size_vector;
   if (indx < n) return INFINITY;

   switch(lambda->type) {
   case ARG_TYPE_SCALAR:
      val = lambda->val;
      break;
   case ARG_TYPE_VEC:
      /* \TODO(xhub) indx or indx%n? */
      val = lambda->vec[indx];
      break;
   default:
      error("%s :: unsupported parameter type %d\n", __func__, lambda->type);
      val = NAN;
   }

   return val;
}

static const struct var_genops elastic_net_varfill  = {
   .set_type = NULL,
   .get_lb = elastic_net_var_lb,
   .get_ub = elastic_net_var_ub,
   .set_level = NULL,
   .set_marginal = NULL,
};

static int elastic_net_gen_var(Container* ctr,  unsigned n, const void* p)
{
   return ovf_box(ctr,  SIZE_U(n), p, &elastic_net_varfill);
}

static size_t elastic_net_size_u(size_t n_args)
{
   return SIZE_U(n_args);
}

const struct ovf_param_def* const OVF_elastic_net_params[] = {&elastic_net_lambda};
const unsigned OVF_elastic_net_params_len = sizeof(OVF_elastic_net_params)/sizeof(struct ovf_param_def*);

const struct ovf_genops OVF_elastic_net_datagen = {
   .M = elastic_net_gen_M,
   .D = elastic_net_gen_M,
   .J = elastic_net_gen_M,
   .B = elastic_net_gen_B,
   .b = NULL,
   .k = NULL,
   .set_A = elastic_net_gen_A,
   .set_A_nonbox = NULL,
   .set_b = elastic_net_gen_b,
   .set_b_nonbox = NULL,
   .set_b_0 = NULL,
   .set_cones = elastic_net_gen_set_cones,
   .set_cones_nonbox = NULL,
   .size_u = elastic_net_size_u,
   .u_shift = NULL,
   .var = elastic_net_gen_var,
   .var_ppty = &elastic_net_varfill,
   .name = xstr(OVF_NAME),
};
