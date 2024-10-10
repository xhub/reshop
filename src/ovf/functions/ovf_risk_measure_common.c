#include "asnan.h"
#include "macros.h"
#include "ovf_generator.h"
#include "ovf_parameter.h"
#include "ovf_risk_measure_common.h"

const struct ovf_param_def cvar_tail = {
   .name = "tail",
   .help = "Tail length ",
   .mandatory = true,
   .allow_data = true,
   .allow_var = false,
   .allow_fun = false,
   .allow_scalar = true,
   .allow_vector = false,
   .allow_matrix = false,
   .get_vector_size = NULL,
//   .check_vector_size = ovf_same_as_m,
};

const struct ovf_param_def ecvar_lambda = {
   .name = "cvar_weight",
   .help = "Level ",
   .mandatory = true,
   .allow_data = true,
   .allow_var = false,
   .allow_fun = false,
   .allow_scalar = true,
   .allow_vector = false,
   .allow_matrix = false,
   .get_vector_size = NULL,
//   .check_vector_size = ovf_same_as_m,
};

const struct ovf_param_def risk_measure_probabilities = {
   .name = "probabilities",
   .help = "Bounds on the variable u. If a scalar, -alpha <= u <= alpha. \
            If a vector, than previous inequality is understood componentwise",
   .mandatory = false,
   .allow_data = true,
   .allow_var = false,
   .allow_fun = false,
   .allow_scalar = true,
   .allow_vector = true,
   .allow_matrix = false,
   .get_vector_size = ovf_same_as_argsize,
   .default_val = risk_measure_probabilities_default,
};


/**
 *  @brief set the default probability if no probability vector is defined
 *
 *  @param p    the
 *  @param dim  the number of events
 */
int risk_measure_probabilities_default(struct ovf_param* p, size_t dim)
{
   p->type = ARG_TYPE_SCALAR;
   p->size_vector = dim;
   p->val = 1./dim;

   return OK;
}

double risk_measure_get_probability(const struct ovf_param *probs, size_t i)
{
   double val = SNAN;

   if (probs->type != ARG_TYPE_UNSET) {
      if (probs->type == ARG_TYPE_SCALAR) {
         val = probs->val;
      } else if (probs->type == ARG_TYPE_VEC) {
         val = probs->vec[i];
      }
   }

   return val;
}

int risk_measure_u_shift(unsigned n, const struct ovf_param *prob, double **u_shift)
{

   MALLOC_(*u_shift, double, n);
   double *lu_shift = *u_shift;

   for (unsigned i = 0; i < n; ++i) {
      lu_shift[i] = risk_measure_get_probability(prob, i);
   }

   return OK;
}


