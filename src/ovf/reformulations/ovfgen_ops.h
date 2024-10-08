#ifndef OVFGEN_OPS_H
#define OVFGEN_OPS_H

#include <stdbool.h>

#include "cones.h"
#include "rhp_fwd.h"
struct ovf_ppty;

int ovfgen_add_k(OvfDef *ovf, Model *mdl, Equ *e, Avar *y) NONNULL;
int ovfgen_create_uvar(OvfDef *ovf, Container *ctr, char* name, Avar *uvar) NONNULL;
int ovfgen_get_D(OvfDef *ovf, SpMat *D, SpMat *J) NONNULL;
int ovfgen_get_lin_transformation(OvfDef *ovf, SpMat *B, double** b) NONNULL;
int ovfgen_get_M(OvfDef *ovf, SpMat *M) NONNULL;
int ovfgen_get_set(OvfDef *ovf, SpMat *A, double** b, bool trans) NONNULL;
int ovfgen_get_set_nonbox(OvfDef *ovf, SpMat *A, double** b, bool trans) NONNULL;
int ovfgen_get_set_0(OvfDef *ovf, SpMat *A_0, double **b_0, double **shift_u) NONNULL;
void ovfgen_get_ppty(OvfDef *ovf, struct ovf_ppty *ovf_ppty) NONNULL;
int ovfgen_get_cone(OvfDef *ovf, unsigned idx, enum cone *cone, void **cone_data) NONNULL;
int ovfgen_get_cone_nonbox(OvfDef *ovf, unsigned idx, enum cone *cone, void **cone_data) NONNULL;
size_t ovfgen_size_u(OvfDef *ovf, size_t n_args) NONNULL;
double ovfgen_get_var_lb(OvfDef *ovf, size_t vidx) NONNULL;
double ovfgen_get_var_ub(OvfDef *ovf, size_t vidx) NONNULL;

#endif

