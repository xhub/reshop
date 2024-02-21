#include "asnan.h"
#include "ovf_fn_helpers.h"

#ifndef OVF_NAME
#error "OVF_NAME must be defined before including this file"
#endif

#ifndef TAIL_VAL
#error "TAIL_VAL must be defined before including this file"
#endif

#define PP_CAT_NX(X, Y) X ## _ ## Y

#define PP_CAT(X,Y) PP_CAT_NX(X,Y)

#define FN(X) PP_CAT(OVF_NAME, X)

static int FN(gen_b)(unsigned n, const void* env, double **vals)
{
   const struct ovf_param_list *p = (const struct ovf_param_list *) env;
   const struct ovf_param *probs = ovf_find_param("probabilities", p);
   const struct ovf_param *tail = ovf_find_param("tail", p);

   OVF_CHECK_PARAM(probs, Error_OvfMissingParam);
   OVF_CHECK_PARAM(tail, Error_OvfMissingParam);

   MALLOC_(*vals, double, 2*n+1);

   memset(*vals, 0, n * sizeof(double));

   switch(tail->type) {
   case ARG_TYPE_SCALAR:
   {
      double tailval = TAIL_VAL(tail->val);

      if (probs->type != ARG_TYPE_UNSET) {
         if (probs->type == ARG_TYPE_SCALAR) {
            double val = probs->val/tailval;
            assert(val == 1./(tailval*n));

            for (unsigned i = n; i < 2*n; ++i) {
               (*vals)[i] = val;
            }

         } else if (probs->type == ARG_TYPE_VEC) {
            if (probs->size_vector != n) {
               error("%s ERROR: probability parameter must be of size %u"
                        "but is of size %u\n", __func__, n, probs->size_vector);
               return Error_InvalidValue;
            }

            for (unsigned i = n, j = 0; i < 2*n; ++i, ++j) {
               double val2 = probs->vec[j]/tailval;
               val2 = val2 > 1. ? 1. : val2;
               (*vals)[i] = val2;
            }
         }
      } else {
         double val = 1./(tailval*n);
         val = val > 1. ? 1. : val;

         for (unsigned i = n; i < 2*n; ++i) {
            (*vals)[i] = val;
         }
      }
      break;
   }
   default:
       error("%s ERROR: unsupported parameter type %d\n", __func__, tail->type);
       return Error_InvalidValue;
   }

   (*vals)[2*n] = 1.;

   return OK;
}

static double FN(var_lb)(const void* env, unsigned indx)
{
   return 0.;
}

static double FN(var_ub)(const void* env, unsigned indx)
{
   const struct ovf_param_list *p = (const struct ovf_param_list*) env;
   const struct ovf_param *probs = ovf_find_param("probabilities", p);
   const struct ovf_param *alpha = ovf_find_param("tail", p);

   OVF_CHECK_PARAM(probs, SNAN);
   OVF_CHECK_PARAM(alpha, SNAN);

   double val;
   switch(alpha->type) {
   case ARG_TYPE_SCALAR:
   {
      double valtmp = TAIL_VAL(alpha->val);
      if (probs->type != ARG_TYPE_UNSET) {
         if (probs->type == ARG_TYPE_SCALAR) {
            val = probs->val/valtmp;
         } else if (probs->type == ARG_TYPE_VEC) {
            if (probs->size_vector <= indx) {
            error("%s :: probability parameter must be of size %d"
                     "but is of size %d\n", __func__, indx, probs->size_vector);
            return NAN;
            }
         val = probs->vec[indx]/valtmp;
         } else {
            error("%s :: probability parameter must be a vector or a scalar\n",
                  __func__);
            return NAN;
         }
      } else {
         TO_IMPLEMENT(MACRO_STR(OVF_NAME) "needs the number of events");
//         val = 1./(valtmp*n);
      }
      break;
   }
   default:
       error("%s :: unsupported parameter type %d\n", __func__, alpha->type);
       return NAN;
   }

   val = val > 1. ? 1. : val;

   return val;
}


