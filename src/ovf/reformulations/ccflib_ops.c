#include "container.h"
#include "macros.h"
#include "mathprgm.h"
#include "ovf_common.h"
#include "ovfgen_ops.h"
#include "status.h"

static rhp_idx ccflib_varidx(union ovf_ops_data ovfd)
{
   assert(ovfd.mp->type == MpTypeCcflib);
   OvfDef *ovf = ovfd.mp->ccflib.ccf;
   return ovf->ovf_vidx;
}

static int ccflib_getargs(union ovf_ops_data ovfd, Avar **v)
{
   assert(ovfd.mp->type == MpTypeCcflib);
   OvfDef *ovf = ovfd.mp->ccflib.ccf;

   *v = ovf->args;

   return OK;
}

static int ccflib_getmappings(union ovf_ops_data ovfd, rhp_idx **eis)
{
   assert(ovfd.mp->type == MpTypeCcflib);
   OvfDef *ovf = ovfd.mp->ccflib.ccf;

   *eis = ovf->eis;

   return OK;
}

static int ccflib_getcoeffs(union ovf_ops_data ovfd, double **coeffs)
{
   assert(ovfd.mp->type == MpTypeCcflib);
   OvfDef *ovf = ovfd.mp->ccflib.ccf;

   *coeffs = ovf->coeffs;

   return OK;
}

static int ccflib_add_k(union ovf_ops_data ovfd, Model *mdl,
                     Equ *e, Avar *y, unsigned n_args)
{
   OvfDef *ovf = ovfd.mp->ccflib.ccf;

   return ovfgen_add_k(ovf, mdl, e, y, n_args);
}

static int ccflib_create_uvar(union ovf_ops_data ovfd, Container *ctr,
                              char* name, Avar *uvar)
{
   OvfDef *ovf = ovfd.mp->ccflib.ccf;

   return ovfgen_create_uvar(ovf, ctr, name, uvar);
}

static int ccflib_get_D(union ovf_ops_data ovfd, SpMat *D, SpMat *J)
{
   OvfDef *ovf = ovfd.mp->ccflib.ccf;

   return ovfgen_get_D(ovf, D, J);
}

static int ccflib_get_lin_transformation(union ovf_ops_data ovfd, SpMat *B, double** b)
{
   OvfDef *ovf = ovfd.mp->ccflib.ccf;

   return ovfgen_get_lin_transformation(ovf, B, b);
}

static int ccflib_get_M(union ovf_ops_data ovfd, SpMat *M)
{
   OvfDef *ovf = ovfd.mp->ccflib.ccf;

   return ovfgen_get_M(ovf, M);
}

static int ccflib_get_set(union ovf_ops_data ovfd, SpMat *A, double** b, bool trans)
{
   OvfDef *ovf = ovfd.mp->ccflib.ccf;

   return ovfgen_get_set(ovf, A, b, trans);
}

static int ccflib_get_set_nonbox(union ovf_ops_data ovfd, SpMat *A, double** b, bool trans)
{
   OvfDef *ovf = ovfd.mp->ccflib.ccf;

   return ovfgen_get_set_nonbox(ovf, A, b, trans);
}

static int ccflib_get_set_0(union ovf_ops_data ovfd, SpMat *A_0, double **b_0, double **shift_u)
{
   OvfDef *ovf = ovfd.mp->ccflib.ccf;

   return ovfgen_get_set_0(ovf, A_0, b_0, shift_u);
}

static void ccflib_get_ppty(union ovf_ops_data ovfd, struct ovf_ppty *ovf_ppty)
{
   OvfDef *ovf = ovfd.mp->ccflib.ccf;

   ovfgen_get_ppty(ovf, ovf_ppty);
}

static int ccflib_get_cone(union ovf_ops_data ovfd, unsigned idx, enum cone *cone, void **cone_data)
{
   OvfDef *ovf = ovfd.mp->ccflib.ccf;

   return ovfgen_get_cone(ovf, idx, cone, cone_data);
}

static int ccflib_get_cone_nonbox(union ovf_ops_data ovfd, unsigned idx, enum cone *cone, void **cone_data)
{
   OvfDef *ovf = ovfd.mp->ccflib.ccf;

   return ovfgen_get_cone_nonbox(ovf, idx, cone, cone_data);
}

static size_t ccflib_size_u(union ovf_ops_data ovfd, size_t n_args)
{
   OvfDef *ovf = ovfd.mp->ccflib.ccf;

   return ovfgen_size_u(ovf, n_args);
}

static double ccflib_get_var_lb(union ovf_ops_data ovfd, size_t vidx)
{
   OvfDef *ovf = ovfd.mp->ccflib.ccf;

   return ovfgen_get_var_lb(ovf, vidx);
}

static double ccflib_get_var_ub(union ovf_ops_data ovfd, size_t vidx)
{
   OvfDef *ovf = ovfd.mp->ccflib.ccf;

   return ovfgen_get_var_ub(ovf, vidx);
}

static const char* ccflib_get_name(union ovf_ops_data ovfd)
{
   OvfDef *ovf = ovfd.mp->ccflib.ccf;

   return ovf_getname(ovf);
}

static void ccflib_trimmem(union ovf_ops_data ovfd)
{
   OvfDef *ovf = ovfd.mp->ccflib.ccf;

   FREE(ovf->eis);
   FREE(ovf->coeffs);
}

const struct ovf_ops ccflib_ops = {
   .add_k = ccflib_add_k,
   .get_args = ccflib_getargs,
   .get_mappings = ccflib_getmappings,
   .get_coeffs =  ccflib_getcoeffs,
   .get_cone = ccflib_get_cone,
   .get_cone_nonbox = ccflib_get_cone_nonbox,
   .get_D = ccflib_get_D,
   .get_lin_transformation = ccflib_get_lin_transformation,
   .get_M = ccflib_get_M,
   .get_name = ccflib_get_name,
   .get_ovf_vidx = ccflib_varidx,
   .get_set = ccflib_get_set,
   .get_set_nonbox = ccflib_get_set_nonbox,
   .get_set_0 = ccflib_get_set_0,
   .create_uvar = ccflib_create_uvar,
   .get_var_lb = ccflib_get_var_lb,
   .get_var_ub = ccflib_get_var_ub,
   .get_ppty = ccflib_get_ppty,
   .size_u = ccflib_size_u,
   .trimmem = ccflib_trimmem,
};


