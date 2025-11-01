#include <stdbool.h>

#include "cmat.h"
#include "cones.h"
#include "consts.h"
#include "container.h"
#include "equ_modif.h"
#include "equil_common.h"
#include "equvar_helpers.h"
#include "mdl_timings.h"
#include "nltree.h"
#include "nltree_priv.h"
#include "macros.h"
#include "mathprgm.h"
#include "mdl.h"
#include "ctrdat_rhp.h"
#include "printout.h"
#include "ovf_common.h"
#include "ovf_equil.h"
#include "rhp_LA.h"
#include "timings.h"

int reformulation_equil_compute_inner_product(enum OVF_TYPE type, union ovf_ops_data ovfd, Model *mdl,
                                              const SpMat *B, const double *b, rhp_idx *objequ, Avar *uvar,
                                              const char *ovf_name)
{
   rhp_idx *equ_idx = NULL;
   double *coeffs = NULL;

   const struct ovf_ops *op;
   switch (type) {
   case OvfType_Ovf:
      op = &ovfdef_ops;
      break;
   case OvfType_Ccflib:
      op = &ccflib_ops;
      break;
   default:
      return Error_NotImplemented;
   }

   S_CHECK(op->get_mappings(ovfd, &equ_idx));
   S_CHECK(op->get_coeffs(ovfd, &coeffs));

   Avar *args;
   unsigned num_empdag_children;
   S_CHECK(op->get_args(ovfd, &args, &num_empdag_children));

   if (num_empdag_children > 0) {
      errormsg("[equilibrium] ERROR: not implemented for mixture of EMPDAG children and "
               "algebraic argument\n");
      return Error_NotImplemented;
   }


   S_CHECK(ovf_process_indices(mdl, args, equ_idx));

   Container *ctr = &mdl->ctr;
   RhpContainerData *cdat = (RhpContainerData *)ctr->data;

   Equ *eobj;
   rhp_idx objequ_ = *objequ;
   if (!valid_ei(objequ_)) {
      S_CHECK(rctr_reserve_equs(ctr, 1));

      char *ovf_objequ_name;
      NEWNAME2(ovf_objequ_name, ovf_name, strlen(ovf_name), "_objequ");
      S_CHECK(cdat_equname_start(cdat, ovf_objequ_name));

      S_CHECK(rctr_add_equ_empty(ctr, objequ, &eobj, Mapping, CONE_NONE));
      objequ_ = *objequ;
      S_CHECK(cdat_equname_end(cdat));
   } else {
      eobj = &mdl->ctr.equs[objequ_];
   }

   unsigned nargs = avar_size(args);

   if (!eobj->tree) {
      S_CHECK(nltree_bootstrap(eobj, 3*nargs, 2*nargs + 1)); /* TODO(xhub) tune that*/
      eobj->tree->root->children[nargs] = NULL;
   }

   /* TODO(Xhub) this should be in a dedicated function  */
   Aequ aeqn = { .type = EquVar_List, .size = avar_size(args), .list = equ_idx };
   S_CHECK(rctr_nltree_cpy_dot_prod_var_map(&mdl->ctr, eobj->tree, eobj->tree->root, uvar, B, b,
                                            coeffs, args, &aeqn));

   return OK;

}

int reformulation_equil_setup_dual_mp(MathPrgm* mp_ovf, rhp_idx objequ, rhp_idx vi_ovf,
                                      Model *mdl, enum OVF_TYPE type, union ovf_ops_data ovfd,
                                      Avar *uvar, unsigned n_args)
{
   /* ----------------------------------------------------------------------
    * Creation of the maximization MP
    *
    * - set min/max
    * - add the ovf variable to the objective equation
    * - set the new equation as objective equation
    * - set the ovf variable as objective variable
    * - add u to the MP
    * ---------------------------------------------------------------------- */

   assert(valid_ei_(objequ, mdl_nequs_total(mdl), __func__));
   int status = OK;

   const struct ovf_ops *op;
   switch (type) {
   case OvfType_Ovf:
      op = &ovfdef_ops;
      break;
   case OvfType_Ccflib:
      op = &ccflib_ops;
      break;
   default:
      return Error_NotImplemented;
   }

   /*  Add -k(y) */
   Equ *eobj = &mdl->ctr.equs[objequ];
   op->add_k(ovfd, mdl, eobj, uvar);

   /*  TODO(xhub) URG remove this HACK */
   mp_ovf->probtype = eobj->tree ? MdlType_nlp : MdlType_lp;

   /* Add objvar to the equation */
   S_CHECK(equ_add_newlvar(eobj, vi_ovf, -1.));

   Container *ctr = &mdl->ctr;
   S_CHECK(cmat_sync_lequ(ctr, eobj));

   S_CHECK(mp_setobjvar(mp_ovf, vi_ovf));

   rhp_idx ei_obj = eobj->idx;
   S_CHECK(mp_setobjequ(mp_ovf, ei_obj));

   S_CHECK(mp_addvars(mp_ovf, uvar));

   /* TODO(xhub) not sure this should be here ...  */
   S_CHECK(rctr_add_eval_equvar(ctr, ei_obj, vi_ovf));

   SpMat A;
   rhpmat_null(&A);
   double *s = NULL;

   S_CHECK_EXIT(op->get_set_nonbox(ovfd, &A, &s, false));

   /* Last, add the (non-box) constraints on u */
   if (A.ppty) {
      S_CHECK_EXIT(ovf_add_polycons(mdl, ovfd, uvar, op, &A, s, mp_ovf, "ovf"));
   }

   S_CHECK_EXIT(mp_finalize(mp_ovf));

_exit:

   rhpmat_free(&A);
   free(s);

   return status;
}

int ovf_equil(Model *mdl, enum OVF_TYPE type, union ovf_ops_data ovfd)
{
   double start = get_thrdtime();

   int status = OK;

   Container *ctr = &mdl->ctr;

   S_CHECK(mdl_settype(mdl, MdlType_emp));

   const struct ovf_ops *op;
   switch (type) {
   case OvfType_Ovf:
      op = &ovfdef_ops;
      break;
   case OvfType_Ccflib:
      op = &ccflib_ops;
      break;
   default:
      return Error_NotImplemented;
   }

   rhp_idx vi_ovf = op->get_ovf_vidx(ovfd);
   S_CHECK(vi_inbounds(vi_ovf, ctr_nvars_total(ctr), __func__));

   char ovf_name[SHORT_STRING];
   S_CHECK(ctr_copyvarname(ctr, vi_ovf, ovf_name, SHORT_STRING));
   unsigned ovf_namelen = strlen(ovf_name);

   /* ---------------------------------------------------------------------
    * Test compatibility between OVF and problem type
    *
    * Warning, this has to be before ovf_equil_init
    * --------------------------------------------------------------------- */

   struct ovf_ppty ovf_ppty;
   op->get_ppty(ovfd, &ovf_ppty);

   MathPrgm *mp, *mp_ovf;
   RhpSense sense;

   S_CHECK(ovf_get_mp_and_sense(mdl, vi_ovf, &mp, &sense));

   S_CHECK(ovf_compat_types(op->get_name(ovfd), ovf_name, sense, ovf_ppty.sense));

   struct ovf_basic_data ovf_data = {.sense = ovf_ppty.sense, .name = ovf_name, .idx = vi_ovf};
   S_CHECK(ovf_equil_init(mdl, &ovf_data, &mp_ovf));


   SpMat B;
   rhpmat_null(&B);

   double *b = NULL;

   /* ---------------------------------------------------------------------
    * 1. Create/Get equilibirum 
    * 2. Replace the rho variable by <y, B*F(x) + b> 
    * 3. Add agent
    * --------------------------------------------------------------------- */

   /* Objective functions: modify the first one and create the second one */

   /* What kind of F and k?  */
   /* Check whether we have B or b  */
   S_CHECK(op->get_affine_transformation(ovfd, &B, &b));

   unsigned n_u;
   /* Get the indices of F(x)  */
   unsigned nargs;
   S_CHECK(op->get_nargs(ovfd, &nargs));

   if (B.ppty) {
      unsigned nargs2;
      S_CHECK(rhpmat_get_size(&B, &nargs2, &n_u));

      if (nargs2 != nargs) {
         error("%s :: incompatible size: the number of arguments (%d) and the "
               "number of columns in B (%d) should be the same\n", __func__,
               nargs, nargs2);
         return Error_Inconsistency;
      }
   } else {
      n_u = nargs;
   }

   assert(n_u > 0);

   char *u_name;
   NEWNAME(u_name, ovf_name, ovf_namelen, "_u");

   Avar uvar;
   op->create_uvar(ovfd, ctr, u_name, &uvar);

   /* ----------------------------------------------------------------------
    *  Build <y, G(F(x))> - k(y)
    *
    *  - k(y) only need to be added to the Dual Player
    *  - <y, G(F(x))> needs to be added to both objective function
    *
    *  1. Build parent node
    *  1.1 get the tree from the parent node
    * ---------------------------------------------------------------------- */

   /* This has to be called AFTER ovf_equil_init, otherwise the "base" model
    * might be empty */
   /* ----------------------------------------------------------------------
    * Create <y, G(F(x))>
    * ---------------------------------------------------------------------- */
   rhp_idx objequ = IdxNA;
   S_CHECK_EXIT(reformulation_equil_compute_inner_product(type, ovfd, mdl, &B, b, &objequ, &uvar, ovf_name));

   Equ *eobj = &mdl->ctr.equs[objequ];
   NlNode *dot_prod_parent = eobj->tree->root;
   assert(dot_prod_parent);

   /* Make sure that we have a nice ADD node  */
   /* \TODO(xhub) this should not be here ... */
//   S_CHECK_EXIT(nltree_check_add(dot_prod_parent));

   /* Now copy dot_prod_parent into the objective function of the first one */
   void *iter = NULL;
   unsigned counter = 0;

   do {
      rhp_idx ei_new;
      double ovfvar_coeff;

      if (counter > 0) {
         error("[OVF/equil] OVF variable '%s' appears in more than one equation. "
               "Sharing an OVF variable is not yet supported\n",
               ctr_printvarname(ctr, vi_ovf));
         return Error_NotImplemented;
      }

      S_CHECK_EXIT(ovf_replace_var(mdl, vi_ovf, &iter, &ovfvar_coeff, &ei_new, 0));

      if (!ctr->equs[ei_new].tree) {
         S_CHECK_EXIT(nltree_bootstrap(&ctr->equs[ei_new], n_u, 3*n_u));
      }

      S_CHECK_EXIT(rctr_equ_add_nlexpr(ctr, ei_new, dot_prod_parent, ovfvar_coeff));

      counter++;

   } while(iter);

   /* The first agent is now completed, move to the second one */

   S_CHECK(reformulation_equil_setup_dual_mp(mp_ovf, objequ, vi_ovf, mdl, type, ovfd, &uvar, nargs))

_exit:
   op->trimmem(ovfd);

   rhpmat_free(&B);
   FREE(b);

   simple_timing_add(&mdl->timings->reformulation.CCF.equilibrium, get_thrdtime() - start);

   return status;
}
