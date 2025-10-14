#include "asprintf.h"

#include "ccflib_utils.h"
#include "ctr_rhp.h"
#include "equil_common.h"
#include "mathprgm.h"
#include "mdl.h"
#include "ovf_common.h"
#include "ovf_equil.h"

/* ----------------------------------------------------------------------
   * Instantiate the MP: var + equations BUT NOT any of the <y, Bu+b> term
   * ---------------------------------------------------------------------- */
int mp_ccflib_instantiate(MathPrgm *mp_instance, MathPrgm *mp_ccflib,
                          CcflibInstanceData *instancedat)
{
   char *y_name;
   mpid_t mpid = mp_instance->id;
   /* TODO GAMS NAMING: do better and use MP name */
   IO_PRINT(asprintf(&y_name, "ccflib_y_%u", mpid));

   const OvfOps *ops = instancedat->ops; assert(ops);
   OvfOpsData ovfd =  instancedat->ovfd;
   Model *mdl = mp_instance->mdl;

   S_CHECK(ops->create_uvar(ovfd, &mdl->ctr, y_name, &instancedat->y));
   Avar *y = &instancedat->y;

   S_CHECK(rctr_reserve_equs(&mdl->ctr, 1));

   rhp_idx objequ = IdxNA;
   /* TODO(xhub) improve naming */
   char *ovf_objequ;
   Equ *eobj;
   IO_PRINT(asprintf(&ovf_objequ, "ccfObj(%u)", mpid));
   RhpContainerData *cdat = (RhpContainerData*)mdl->ctr.data;
   S_CHECK(cdat_equname_start(cdat, ovf_objequ));
   S_CHECK(rctr_add_equ_empty(&mdl->ctr, &objequ, &eobj, Mapping, CONE_NONE));
   S_CHECK(cdat_equname_end(cdat));

   /*  Add -k(y) */
   ops->add_k(ovfd, mdl, eobj, y);

   S_CHECK(mp_setobjequ(mp_instance, objequ));

   S_CHECK(mp_addvars(mp_instance, y));

   SpMat A;
   rhpmat_null(&A);
   double *s = NULL;
   rhpmat_null(&instancedat->B);
   instancedat->b = NULL;

   int status = OK;

   S_CHECK_EXIT(ops->get_set_nonbox(ovfd, &A, &s, false));

   /* Last, add the (non-box) constraints on u */
   if (A.ppty) {
      S_CHECK_EXIT(ovf_add_polycons(mdl, ovfd, y, ops, &A, s, mp_instance, "ccflib"));
   }

   unsigned nargs_maps;
   S_CHECK_EXIT(ops->get_nargs(ovfd, &nargs_maps));

   S_CHECK_EXIT(ops->get_affine_transformation(ovfd, &instancedat->B, &instancedat->b));

   if (nargs_maps > 0) {
      CcflibPrimalDualData ccfdat = {.mp_primal = mp_ccflib, .mpid_dual = MpId_NA};
      OvfOpsData ovfd_mp = {.ccfdat = &ccfdat};
      S_CHECK_EXIT(reformulation_equil_compute_inner_product(OvfType_Ccflib, ovfd_mp, mdl,
                                                             &instancedat->B, instancedat->b,
                                                             &objequ, y, NULL));
      mp_instance->probtype = MdlType_nlp; /* HACK */
   } else {
      OvfPpty ovfppty;
      ops->get_ppty(ovfd, &ovfppty);

      mp_instance->probtype = ovfppty.probtype;
   }


_exit:
   rhpmat_free(&A);
   rhpmat_free(&instancedat->B);
   free(s);
   free(instancedat->b);

   return status;
}


