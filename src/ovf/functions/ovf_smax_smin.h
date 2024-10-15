#ifndef OVF_SMAX_SMIN_H
#define OVF_SMAX_SMIN_H

extern const struct ovf_genops OVF_smax_datagen;
extern const struct ovf_param_def* const OVF_smax_params[];
extern const unsigned OVF_smax_params_len;

#define smax_sense RhpMax

extern const struct ovf_genops OVF_smin_datagen;
extern const struct ovf_param_def* const OVF_smin_params[];
extern const unsigned OVF_smin_params_len;

#define smin_sense RhpMin

#endif /* OVF_SMIN_H */
