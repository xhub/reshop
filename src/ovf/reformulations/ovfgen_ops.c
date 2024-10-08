#include "container.h"
#include "equ_modif.h"
#include "ctrdat_rhp.h"
#include "mdl.h"
#include "ovf_common.h"
#include "ovf_generator.h"
#include "ovfgen_ops.h"
#include "printout.h"
#include "status.h"

int ovfgen_add_k(OvfDef *ovf, Model *mdl, Equ *e, Avar *y)
{
   int status = OK;

   if (ovf->generator->M) {
      SpMat M;
      rhpmat_null(&M);
      status = ovf->generator->M(ovf_argsize(ovf), (void*)&ovf->params, &M);
      if (status == OK) {
         status = rctr_equ_add_quadratic(&mdl->ctr, e, &M, y, -1.0);
      }
      rhpmat_free(&M);
   } else if (ovf->generator->k) {
      TO_IMPLEMENT("The case will a purely non-linear function is not implemented");
   }

   return status;
}

int ovfgen_create_uvar(OvfDef *ovf, Container *ctr, char* name, Avar *uvar)
{
   RhpContainerData *cdat = (RhpContainerData *)ctr->data;

   uvar->start = cdat->total_n;

   cdat_varname_start(cdat, name);

  //TODO OVF: distinguish between the equvar argument and the EMPDAG ones.

   S_CHECK(ovf->generator->var(ctr, ovf_argsize(ovf), (void*)&ovf->params));

   uvar->type = EquVar_Compact;
   uvar->size = cdat->total_n - uvar->start;

   cdat_varname_end(cdat);

   assert(uvar->size > 0);

   return OK;
}

int ovfgen_get_D(OvfDef *ovf, SpMat *D, SpMat *J)
{
   if (!ovf->generator->M && !ovf->generator->D) {
      rhpmat_reset(D);
      rhpmat_reset(J);
      return OK;
   }

   if (!ovf->generator->D || !ovf->generator->J) {
      return Error_NotImplemented;
   }

   S_CHECK(ovf->generator->D(ovf_argsize(ovf), (void*)&ovf->params, D));
   S_CHECK(ovf->generator->J(ovf_argsize(ovf), (void*)&ovf->params, J));

   return OK;
}

int ovfgen_get_lin_transformation(OvfDef *ovf, SpMat *B, double** b)
{
   rhpmat_reset(B);
   if (ovf->generator->B) { S_CHECK(ovf->generator->B(ovf_argsize(ovf), (void*)&ovf->params, B)); }
   if (ovf->generator->b) {
      S_CHECK(ovf->generator->b(ovf_argsize(ovf), (void*)&ovf->params, b));
   }

   return OK;
}

int ovfgen_get_M(OvfDef *ovf, SpMat *M)
{
   if (!ovf->generator->M) {
      return Error_NotImplemented;
   }

   S_CHECK(ovf->generator->M(ovf_argsize(ovf), (void*)&ovf->params, M));

   return OK;
}

int ovfgen_get_set(OvfDef *ovf, SpMat *A, double** b, bool trans)
{
   rhpmat_reset(A);
   /*  \TODO(xhub) sanity check that both set_A and set_b are present at the
    *  same time */
   if (ovf->generator->set_A && ovf->generator->set_b) {
      if (trans) {
         rhpmat_set_csc(A);
      }
      S_CHECK(ovf->generator->set_A(ovf_argsize(ovf), (void*)&ovf->params, A));
      S_CHECK(ovf->generator->set_b(ovf_argsize(ovf), (void*)&ovf->params, b));
   }

   return OK;
}

int ovfgen_get_set_nonbox(OvfDef *ovf, SpMat *A, double** b, bool trans)
{
   rhpmat_reset(A);
   /*  \TODO(xhub) sanity check that both set_A and set_b are present at the
    *  same time */
   if (ovf->generator->set_A_nonbox) {
      if (trans) {
         rhpmat_set_csc(A);
      }
      S_CHECK(ovf->generator->set_A_nonbox(ovf_argsize(ovf), (void*)&ovf->params, A));
      S_CHECK(ovf->generator->set_b_nonbox(ovf_argsize(ovf), (void*)&ovf->params, b));
   }

   return OK;
}

int ovfgen_get_set_0(OvfDef *ovf, SpMat *A_0, double **b_0, double **shift_u)
{
   rhpmat_reset(A_0);
   /*  \TODO(xhub) sanity check that both set_A and set_b are present at the
    *  same time */
   if (ovf->generator->u_shift) {
      if (ovf->generator->set_A && ovf->generator->set_b_0) {
         S_CHECK(ovf->generator->set_A(ovf_argsize(ovf), (void*)&ovf->params, A_0));
         S_CHECK(ovf->generator->u_shift(ovf_argsize(ovf), (void*)&ovf->params, shift_u));
         S_CHECK(ovf->generator->set_b_0(ovf_argsize(ovf), (void*)&ovf->params, A_0, *shift_u, b_0));
      }
   } else if (ovf->generator->set_A && ovf->generator->set_b) {
       S_CHECK(ovf->generator->set_A(ovf_argsize(ovf), (void*)&ovf->params, A_0));
       S_CHECK(ovf->generator->set_b(ovf_argsize(ovf), (void*)&ovf->params, b_0));
   }

   return OK;
}

void ovfgen_get_ppty(OvfDef *ovf, struct ovf_ppty *ovf_ppty)
{
   if (ovf->generator->M || ovf->generator->D) {
      ovf_ppty->quad = true;
   } else {
      ovf_ppty->quad = false;
   }

   ovf_ppty->sense = ovf->sense;
}

int ovfgen_get_cone(OvfDef *ovf, unsigned idx, enum cone *cone, void **cone_data)
{
   (*cone) = ovf->generator->set_cones(ovf_argsize(ovf), (void*)&ovf->params, idx, cone_data);

   return OK;
}

int ovfgen_get_cone_nonbox(OvfDef *ovf, unsigned idx, enum cone *cone, void **cone_data)
{
   (*cone) = ovf->generator->set_cones_nonbox(ovf_argsize(ovf), (void*)&ovf->params, idx, cone_data);

   return OK;
}

size_t ovfgen_size_u(OvfDef *ovf, size_t n_args)
{
   return ovf->generator->size_u(n_args);
}

double ovfgen_get_var_lb(OvfDef *ovf, size_t vidx)
{
   if (ovf->generator->var_ppty->get_lb) {
      return ovf->generator->var_ppty->get_lb((void*)&ovf->params, vidx);
   }

   return -INFINITY;
}

double ovfgen_get_var_ub(OvfDef *ovf, size_t vidx)
{
   if (ovf->generator->var_ppty->get_ub) {
      return ovf->generator->var_ppty->get_ub((void*)&ovf->params, vidx);
   }

   return INFINITY;
}
