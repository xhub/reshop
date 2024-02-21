#ifndef OVF_CVAR_COMMON_H
#define OVF_CVAR_COMMON_H

#include "rhp_fwd.h"
#include "cones.h"

int cvar_gen_A(unsigned n, const void *env, SpMat *mat);
int cvar_gen_A_nonbox(unsigned n, const void *env, SpMat *mat);
int cvar_gen_b_nonbox(unsigned n, const void* env, double **vals);
enum cone cvar_gen_set_cones(unsigned n, const void* env, unsigned idx,
                              void **cone_data);
enum cone cvar_gen_set_cones_nonbox(unsigned n, const void* env, unsigned idx,
                                     void **cone_data);
int cvar_u_shift(unsigned n, const void *env, double **u_shift);

#endif
