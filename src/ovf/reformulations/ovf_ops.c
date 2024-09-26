#include "container.h"
#include "macros.h"
#include "ovf_common.h"
#include "ovfgen_ops.h"
#include "printout.h"
#include "status.h"

static rhp_idx ovfdef_varidx(union ovf_ops_data ovfd)
{
   OvfDef *ovf = ovfd.ovf;
   rhp_idx vi_ovf = ovf->vi_ovf;

   if (!valid_vi(vi_ovf)) {
      error("[OVF] ERROR: the OVF variable is not set! Value = %d\n", vi_ovf);
      return Error_InvalidValue;
   }
   return vi_ovf;
}

static int ovfdef_getarg(union ovf_ops_data ovfd, Avar **v)
{
   OvfDef *ovf = ovfd.ovf;

   *v = ovf->args;

   return OK;
}

static int ovfdef_getnarg(union ovf_ops_data ovfd, unsigned *nargs)
{
   OvfDef *ovf = ovfd.ovf;

   *nargs = ovf->args->size;

   return OK;
}

static int ovfdef_getmappings(union ovf_ops_data ovfd, rhp_idx **eis)
{
   OvfDef *ovf = ovfd.ovf;

   *eis = ovf->eis;

   return OK;
}

static int ovfdef_getcoeffs(union ovf_ops_data ovfd, double **coeffs)
{
   OvfDef *ovf = ovfd.ovf;

   *coeffs = ovf->coeffs;

   return OK;
}

static int ovfdef_add_k(union ovf_ops_data ovfd, Model *mdl,
                     Equ *e, Avar *y, unsigned n_args)
{
   OvfDef *ovf = ovfd.ovf;

   return ovfgen_add_k(ovf, mdl, e, y, n_args);
}

static int ovfdef_create_uvar(union ovf_ops_data ovfd, Container *ctr,
                           char* name, Avar *uvar)
{
   OvfDef *ovf = ovfd.ovf;

   return ovfgen_create_uvar(ovf, ctr, name, uvar);
}

static int ovfdef_get_D(union ovf_ops_data ovfd, SpMat *D, SpMat *J)
{
   OvfDef *ovf = ovfd.ovf;

   return ovfgen_get_D(ovf, D, J);
}

static int ovfdef_get_lin_transformation(union ovf_ops_data ovfd, SpMat *B, double** b)
{
   OvfDef *ovf = ovfd.ovf;

   return ovfgen_get_lin_transformation(ovf, B, b);
}

static int ovfdef_get_M(union ovf_ops_data ovfd, SpMat *M)
{
   OvfDef *ovf = ovfd.ovf;

   return ovfgen_get_M(ovf, M);
}

static int ovfdef_get_mp_and_sense(UNUSED OvfOpsData dat, const Model *mdl,
                                   rhp_idx vi_ovf, MathPrgm **mp_dual,
                                   RhpSense *sense)
{
   return ovf_get_mp_and_sense(mdl, vi_ovf, mp_dual, sense);
}

static int ovfdef_get_set(union ovf_ops_data ovfd, SpMat *A, double** b, bool trans)
{
   OvfDef *ovf = ovfd.ovf;

   return ovfgen_get_set(ovf, A, b, trans);
}

static int ovfdef_get_set_nonbox(union ovf_ops_data ovfd, SpMat *A, double** b, bool trans)
{
   OvfDef *ovf = ovfd.ovf;

   return ovfgen_get_set_nonbox(ovf, A, b, trans);
}

static int ovfdef_get_set_0(union ovf_ops_data ovfd, SpMat *A_0, double **b_0, double **shift_u)
{
   OvfDef *ovf = ovfd.ovf;

   return ovfgen_get_set_0(ovf, A_0, b_0, shift_u);
}

static void ovfdef_get_ppty(union ovf_ops_data ovfd, struct ovf_ppty *ovf_ppty)
{
   OvfDef *ovf = ovfd.ovf;

   ovfgen_get_ppty(ovf, ovf_ppty);
}

static int ovfdef_get_cone(union ovf_ops_data ovfd, unsigned idx, enum cone *cone, void **cone_data)
{
   OvfDef *ovf = ovfd.ovf;

   return ovfgen_get_cone(ovf, idx, cone, cone_data);
}

static int ovfdef_get_cone_nonbox(union ovf_ops_data ovfd, unsigned idx, enum cone *cone, void **cone_data)
{
   OvfDef *ovf = ovfd.ovf;

   return ovfgen_get_cone_nonbox(ovf, idx, cone, cone_data);
}

static int ovfdef_get_equ(UNUSED OvfOpsData ovfd, Model *mdl, void **iterator,
                          UNUSED rhp_idx vi_ovf, UNUSED double *ovf_coeff,
                          rhp_idx *ei_new, UNUSED unsigned n_z)
{
   return ovf_replace_var(mdl, vi_ovf, iterator, ovf_coeff, ei_new, n_z);
}

static size_t ovfdef_size_u(union ovf_ops_data ovfd, size_t n_args)
{
   OvfDef *ovf = ovfd.ovf;

   return ovfgen_size_u(ovf, n_args);
}

static double ovfdef_get_var_lb(union ovf_ops_data ovfd, size_t vidx)
{
   OvfDef *ovf = ovfd.ovf;

   return ovfgen_get_var_lb(ovf, vidx);
}

static double ovfdef_get_var_ub(union ovf_ops_data ovfd, size_t vidx)
{
   OvfDef *ovf = ovfd.ovf;

   return ovfgen_get_var_ub(ovf, vidx);
}

static const char* ovfdef_get_name(union ovf_ops_data ovfd)
{
   OvfDef *ovf = ovfd.ovf;

   return ovf_getname(ovf);
}

static void ovf_trimmem(union ovf_ops_data ovfd)
{
   OvfDef *ovf = ovfd.ovf;

   FREE(ovf->eis);
   FREE(ovf->coeffs);
}

const OvfOps ovfdef_ops = {
   .add_k = ovfdef_add_k,
   .get_args = ovfdef_getarg,
   .get_nargs = ovfdef_getnarg,
   .get_mappings = ovfdef_getmappings,
   .get_coeffs =  ovfdef_getcoeffs,
   .get_cone = ovfdef_get_cone,
   .get_cone_nonbox = ovfdef_get_cone_nonbox,
   .get_D = ovfdef_get_D,
   .get_equ = ovfdef_get_equ,
   .get_lin_transformation = ovfdef_get_lin_transformation,
   .get_M = ovfdef_get_M,
   .get_mp_and_sense = ovfdef_get_mp_and_sense,
   .get_name = ovfdef_get_name,
   .get_ovf_vidx = ovfdef_varidx,
   .get_set = ovfdef_get_set,
   .get_set_nonbox = ovfdef_get_set_nonbox,
   .get_set_0 = ovfdef_get_set_0,
   .create_uvar = ovfdef_create_uvar,
   .get_var_lb = ovfdef_get_var_lb,
   .get_var_ub = ovfdef_get_var_ub,
   .get_ppty = ovfdef_get_ppty,
   .size_u = ovfdef_size_u,
   .trimmem = ovf_trimmem,
};


