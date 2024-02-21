#ifndef OVF_LOSS_COMMON_H
#define OVF_LOSS_COMMON_H

#include "rhp_fwd.h"

extern const struct ovf_param_def loss_kappa;
extern const struct ovf_param_def loss_epsilon;

int loss_kappa_gen_b(unsigned n, const void *env, double **vals);
int loss_kappa_gen_M(unsigned n, const void *env, SpMat *mat);
int loss_epsilon_gen_boff(unsigned n, const void* env, double **vals);
double loss_kappa_var_lb(const void *env, unsigned idx);
double loss_kappa_var_ub(const void *env, unsigned idx);

#endif /* OVF_LOSS_COMMON_H  */
