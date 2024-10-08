#include "asprintf.h"

#include "container.h"
#include "empdag.h"
#include "macros.h"
#include "mathprgm.h"
#include "mdl.h"
#include "ovf_common.h"
#include "ovfgen_ops.h"
#include "status.h"

static rhp_idx ccflib_varidx(OvfOpsData ovfd)
{
   assert(ovfd.ccfdat->mp->type == MpTypeCcflib);
   OvfDef *ovf = ovfd.ccfdat->mp->ccflib.ccf;

   return ovf->vi_ovf;
}

static int ccflib_getnargs(OvfOpsData ovfd, unsigned *nargs)
{
   assert(ovfd.ccfdat->mp->type == MpTypeCcflib);
   OvfDef *ovf = ovfd.ccfdat->mp->ccflib.ccf;

   *nargs = ovf->args->size;

   return OK;
}

static int ccflib_getargs(OvfOpsData ovfd, Avar **v)
{
   assert(ovfd.ccfdat->mp->type == MpTypeCcflib);
   OvfDef *ovf = ovfd.ccfdat->mp->ccflib.ccf;

   *v = ovf->args;

   return OK;
}

static int ccflib_getmappings(OvfOpsData ovfd, rhp_idx **eis)
{
   assert(ovfd.ccfdat->mp->type == MpTypeCcflib);
   OvfDef *ovf = ovfd.ccfdat->mp->ccflib.ccf;

   *eis = ovf->eis;

   return OK;
}

static int ccflib_getcoeffs(OvfOpsData ovfd, double **coeffs)
{
   assert(ovfd.ccfdat->mp->type == MpTypeCcflib);
   OvfDef *ovf = ovfd.ccfdat->mp->ccflib.ccf;

   *coeffs = ovf->coeffs;

   return OK;
}

static int ccflib_add_k(OvfOpsData ovfd, Model *mdl, Equ *e, Avar *y)
{
   OvfDef *ovf = ovfd.ccfdat->mp->ccflib.ccf;

   return ovfgen_add_k(ovf, mdl, e, y);
}

static int ccflib_create_uvar(OvfOpsData ovfd, Container *ctr,
                              char* name, Avar *uvar)
{
   OvfDef *ovf = ovfd.ccfdat->mp->ccflib.ccf;

   return ovfgen_create_uvar(ovf, ctr, name, uvar);
}

static int ccflib_get_D(OvfOpsData ovfd, SpMat *D, SpMat *J)
{
   OvfDef *ovf = ovfd.ccfdat->mp->ccflib.ccf;

   return ovfgen_get_D(ovf, D, J);
}

static int ccflib_get_lin_transformation(OvfOpsData ovfd, SpMat *B, double** b)
{
   OvfDef *ovf = ovfd.ccfdat->mp->ccflib.ccf;

   return ovfgen_get_lin_transformation(ovf, B, b);
}

static int ccflib_get_M(OvfOpsData ovfd, SpMat *M)
{
   OvfDef *ovf = ovfd.ccfdat->mp->ccflib.ccf;

   return ovfgen_get_M(ovf, M);
}

static int ccflib_get_mp_and_sense(UNUSED OvfOpsData dat, Model *mdl,
                                   UNUSED rhp_idx vi_ovf, MathPrgm **mp_dual,
                                   RhpSense *sense)
{
   MathPrgm *mp_;
   char *name_dual = NULL;
   int status = OK;

   mpid_t mpid_dual = dat.ccfdat->mpid_dual;

   /* HACK sense is set to just pass a later check */
   if (mpid_dual == MpId_NA) {
      MathPrgm *mp_primal = dat.ccfdat->mp;

      if (!mp_primal) {
         errormsg("[ccflib] ERROR: primal MP is NULL!\n");
         return Error_NullPointer;
      }

      if (mp_primal->type != MpTypeCcflib) {
         TO_IMPLEMENT("Dualization for general MP");
      }

      const char *name_primal = mp_getname(mp_primal);
      IO_CALL(asprintf(&name_dual, "%s_dual", name_primal));

      OvfDef * ovfdef = mp_primal->ccflib.ccf;
      RhpSense sense_ = ovfdef->sense == RhpMax ? RhpMin : RhpMax;
      *sense = sense_;

      EmpDag *empdag = &mdl->empinfo.empdag;
      A_CHECK_EXIT(mp_, empdag_newmpnamed(empdag, sense_, name_dual));
      free(name_dual);
      *mp_dual = mp_;

      mpid_dual = mp_->id;
      dat.ccfdat->mpid_dual = mpid_dual;
      S_CHECK(empdag_substitute_mp_arcs(empdag, mp_primal->id, mpid_dual));
      mp_hide(mp_primal);

   } else {
      S_CHECK(empdag_getmpbyid(&mdl->empinfo.empdag, mpid_dual, mp_dual));
      mp_ = *mp_dual;

      *sense = mp_->sense;
   }


   return OK;

_exit:
   free(name_dual);
   return status;
}

static int ccflib_get_set(OvfOpsData ovfd, SpMat *A, double** b, bool trans)
{
   OvfDef *ovf = ovfd.ccfdat->mp->ccflib.ccf;

   return ovfgen_get_set(ovf, A, b, trans);
}

static int ccflib_get_set_nonbox(OvfOpsData ovfd, SpMat *A, double** b, bool trans)
{
   OvfDef *ovf = ovfd.ccfdat->mp->ccflib.ccf;

   return ovfgen_get_set_nonbox(ovf, A, b, trans);
}

static int ccflib_get_set_0(OvfOpsData ovfd, SpMat *A_0, double **b_0, double **shift_u)
{
   OvfDef *ovf = ovfd.ccfdat->mp->ccflib.ccf;

   return ovfgen_get_set_0(ovf, A_0, b_0, shift_u);
}

static void ccflib_get_ppty(OvfOpsData ovfd, struct ovf_ppty *ovf_ppty)
{
   OvfDef *ovf = ovfd.ccfdat->mp->ccflib.ccf;

   ovfgen_get_ppty(ovf, ovf_ppty);
}

static int ccflib_get_cone(OvfOpsData ovfd, unsigned idx, enum cone *cone, void **cone_data)
{
   OvfDef *ovf = ovfd.ccfdat->mp->ccflib.ccf;

   return ovfgen_get_cone(ovf, idx, cone, cone_data);
}

static int ccflib_get_cone_nonbox(OvfOpsData ovfd, unsigned idx, enum cone *cone, void **cone_data)
{
   OvfDef *ovf = ovfd.ccfdat->mp->ccflib.ccf;

   return ovfgen_get_cone_nonbox(ovf, idx, cone, cone_data);
}

static size_t ccflib_size_u(OvfOpsData ovfd, size_t n_args)
{
   OvfDef *ovf = ovfd.ccfdat->mp->ccflib.ccf;

   return ovfgen_size_u(ovf, n_args);
}

static double ccflib_get_var_lb(OvfOpsData ovfd, size_t vidx)
{
   OvfDef *ovf = ovfd.ccfdat->mp->ccflib.ccf;

   return ovfgen_get_var_lb(ovf, vidx);
}

static double ccflib_get_var_ub(OvfOpsData ovfd, size_t vidx)
{
   OvfDef *ovf = ovfd.ccfdat->mp->ccflib.ccf;

   return ovfgen_get_var_ub(ovf, vidx);
}

static const char* ccflib_get_name(OvfOpsData ovfd)
{
   OvfDef *ovf = ovfd.ccfdat->mp->ccflib.ccf;

   return ovf_getname(ovf);
}

static void ccflib_trimmem(OvfOpsData ovfd)
{
   OvfDef *ovf = ovfd.ccfdat->mp->ccflib.ccf;

   FREE(ovf->eis);
   FREE(ovf->coeffs);
}

/* TODO: find better name */
static int ccflib_get_equ(OvfOpsData ovfd, Model *mdl, void **iterator,
                          UNUSED rhp_idx vi_ovf, double *ovf_coeff,
                          rhp_idx *ei_new, UNUSED unsigned n_z)
{

   *iterator = NULL;
   *ovf_coeff = 1.;

   mpid_t mpid = ovfd.ccfdat->mpid_dual;
   MathPrgm *mp_dual;

   S_CHECK(empdag_getmpbyid(&mdl->empinfo.empdag, mpid, &mp_dual));

   return mp_ensure_objfunc(mp_dual, ei_new);
}

const struct ovf_ops ccflib_ops = {
   .add_k = ccflib_add_k,
   .get_args = ccflib_getargs,
   .get_nargs = ccflib_getnargs,
   .get_mappings = ccflib_getmappings,
   .get_coeffs =  ccflib_getcoeffs,
   .get_cone = ccflib_get_cone,
   .get_cone_nonbox = ccflib_get_cone_nonbox,
   .get_D = ccflib_get_D,
   .get_equ = ccflib_get_equ,
   .get_lin_transformation = ccflib_get_lin_transformation,
   .get_M = ccflib_get_M,
   .get_mp_and_sense = ccflib_get_mp_and_sense,
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


