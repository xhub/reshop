#ifndef OVF_RISK_MEASURE_COMMON_H
#define OVF_RISK_MEASURE_COMMON_H

#include <stddef.h>

extern const struct ovf_param_def cvar_tail;
extern const struct ovf_param_def ecvar_lambda;
extern const struct ovf_param_def risk_measure_probabilities;

double risk_measure_get_probability(const struct ovf_param *probs, size_t i);
int risk_measure_probabilities_default(struct ovf_param* p, size_t dim);
int risk_measure_u_shift(unsigned n, const struct ovf_param *prob, double **u_shift);
size_t risk_measure_u_size(size_t n_args);

#endif /* OVF_RISK_MEASURE_COMMON_H */
