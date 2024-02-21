#ifndef EQUIL_COMMON_H
#define EQUIL_COMMON_H

#include "ovfinfo.h"
#include "rhp_fwd.h"


int ovf_add_polycons(Model *mdl, union ovf_ops_data ovfd, Avar *y,
                     const struct ovf_ops *op, SpMat *A, double *s,
                     MathPrgm *mp_ovf, const char *prefix);

#endif

