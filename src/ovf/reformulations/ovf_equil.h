#ifndef OVF_EQUIL_H
#define OVF_EQUIL_H

#include "ovfinfo.h"

int reformulation_equil_compute_inner_product(enum OVF_TYPE type, union ovf_ops_data ovfd, Model *mdl,
                                              const SpMat *B, const double *b, Equ **e,
                                              Avar *uvar, const char *ovf_name);
int reformulation_equil_setup_dual_mp(MathPrgm* mp_ovf, Equ *eobj, rhp_idx vi_ovf,
                                      Model *mdl, enum OVF_TYPE type, union ovf_ops_data ovfd,
                                      Avar *uvar, unsigned n_args) NONNULL;
int ovf_equil(Model *mdl, enum OVF_TYPE type, union ovf_ops_data ovfd);

#endif
