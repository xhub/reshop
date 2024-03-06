#include "checks.h"
#include "container.h"
#include "ctr_rhp.h"
#include "ctr_rhp_add_vars.h"
#include "equvar_metadata.h"
#include "equvar_helpers.h"
#include "macros.h"
#include "mdl.h"
#include "ctrdat_rhp.h"
#include "ctrdat_rhp_priv.h"
#include "printout.h"
#include "status.h"

/**
 * @brief Add multiplier (or init variable as multiplier) to a constraint
 *
 * @param          ctr         the container
 * @param          mcone       the cone type
 * @param          mcone_data  the cone information
 * @param[in,out]  vi          On input, if valid, then the variable is initialized
 *                             with the data. If not valid, then a variable is added
 *                             to the model.
 *                             On output, it contains a valid variable index
 *
 * @return                     the error code
 */
static int add_multiplier_common_(Container *ctr, enum cone mcone,
                                  void* mcone_data, rhp_idx *vi)
{
   rhp_idx lvi = *vi;
   /* For now we don't want to add zero multipliers */
   if (mcone == CONE_0) {
      if (valid_vi(lvi)) {
         error("%s :: requested multiplier %d is zero!\n", __func__, lvi);
         return Error_Inconsistency;
      }
      *vi = IdxInvalid;

      return OK;
   }

   RhpContainerData *cdat = (RhpContainerData *)ctr->data;

   if (!valid_vi(lvi)) {
      S_CHECK(chk_ctrdat_space(cdat, 1, __func__));

     lvi = cdat->total_n++;
      assert(lvi < cdat->max_n);
     *vi = lvi;
   } else {
     /*  TODO(xhub) it is the caller responsibility to adjust total_n */
     S_CHECK(vi_inbounds(lvi, cdat->max_n, __func__));
   }

   var_init(&ctr->vars[lvi], lvi, VAR_X);
   if (ctr->varmeta) {
     varmeta_init(&ctr->varmeta[lvi]);
   }

   switch (mcone) {
   case CONE_R_PLUS:
      ctr->vars[lvi].bnd.lb = 0.;
      ctr->vars[lvi].bnd.ub = INFINITY;
      break;
   case CONE_R_MINUS:
      ctr->vars[lvi].bnd.lb = -INFINITY;
      ctr->vars[lvi].bnd.ub = 0.;
      break;
   case CONE_R:
      ctr->vars[lvi].bnd.lb = -INFINITY;
      ctr->vars[lvi].bnd.ub = INFINITY;
      break;
   case CONE_0:
      ctr->vars[lvi].bnd.lb = 0.;
      ctr->vars[lvi].bnd.ub = 0.;
      break;
  case CONE_SOC:
  case CONE_EXP:
  case CONE_POWER:
  default:
     error("%s :: unsupported cone %s (%d)", __func__,
              cone_name(mcone), mcone);
     return Error_NotImplemented;
   }

   return OK;
}

/**
 * @brief Add positive variables
 *
 * @ingroup publicAPI
 *
 * @param       mdl  the model
 * @param       nb   the number of variables to add
 * @param[out]  v    the abstract variable
 *
 * @return           the error code
 */
int rctr_add_pos_vars(Container * restrict ctr, unsigned nb, Avar *v)
{
   RhpContainerData *cdat = (RhpContainerData *)ctr->data;

   S_CHECK(chk_ctrdat_space(cdat, nb, __func__));

   for (unsigned i = cdat->total_n; i < cdat->total_n + nb; ++i) {

      Var *vv = &ctr->vars[i];
      var_init(vv, i, VAR_X);
      if (ctr->varmeta) {
        varmeta_init(&ctr->varmeta[i]);
      }
      vv->bnd.lb = 0;
      vv->bnd.ub = INFINITY;
   }

   if (v) {
      v->type = EquVar_Compact;
      v->own = false;
      v->start = cdat->total_n;
      v->size = nb;
   }

   cdat->total_n += nb;

   return OK;
}

/**
 * @brief Add negative variables
 *
 * @ingroup publicAPI
 *
 * @param       mdl  the model
 * @param       nb   the number of variables to add
 * @param[out]  v    the abstract variable
 *
 * @return           the error code
 */
int rctr_add_neg_vars(Container *ctr, unsigned nb, Avar *v)
{
   RhpContainerData *cdat = (RhpContainerData *)ctr->data;

   S_CHECK(chk_ctrdat_space(cdat, nb, __func__));

   for (unsigned i = cdat->total_n; i < cdat->total_n + nb; ++i) {

      Var *vv = &ctr->vars[i];
      var_init(vv, i, VAR_X);
      if (ctr->varmeta) {
        varmeta_init(&ctr->varmeta[i]);
      }
      vv->bnd.lb = -INFINITY;
      vv->bnd.ub = 0;
   }

   if (v) {
      v->type = EquVar_Compact;
      v->own = false;
      v->start = cdat->total_n;
      v->size = nb;
   }

   cdat->total_n += nb;

   return OK;
}

/**
 * @brief Add free (or unconstrainted) variables
 *
 * @ingroup publicAPI
 *
 * @param       mdl  the model
 * @param       nb   the number of variables to add
 * @param[out]  v    the abstract variable
 *
 * @return           the error code
 */
int rctr_add_free_vars(Container *ctr, unsigned nb, Avar *v)
{
   RhpContainerData *cdat = (RhpContainerData *)ctr->data;

   S_CHECK(chk_ctrdat_space(cdat, nb, __func__));

   for (unsigned i = cdat->total_n; i < cdat->total_n + nb; ++i) {

      Var *vv = &ctr->vars[i];
      var_init(vv, i, VAR_X);
      if (ctr->varmeta) {
        varmeta_init(&ctr->varmeta[i]);
      }
      vv->bnd.lb = -INFINITY;
      vv->bnd.ub = INFINITY;
   }

   if (v) {
      v->type = EquVar_Compact;
      v->own = false;
      v->start = cdat->total_n;
      v->size = nb;
   }

   cdat->total_n += nb;

   return OK;
}

/**
 * @brief Add box constraint variable 
 *
 * @param       mdl  the model
 * @param       nb   the number of constraints to add
 * @param[out]  v    the abstract variable
 * @param       lb   (optional) the lower bounds
 * @param       ub   (optional) the upper bounds
 *
 * @return           the error code
 */
int rctr_add_box_vars(Container * restrict ctr, unsigned nb,
                      Avar * restrict v, double * restrict lb,
                      double * restrict ub)
{
   RhpContainerData *cdat = (RhpContainerData *)ctr->data;

   S_CHECK(chk_ctrdat_space(cdat, nb, __func__));

   for (unsigned i = cdat->total_n; i < cdat->total_n + nb; ++i) {
      var_init(&ctr->vars[i], i, VAR_X);
      if (ctr->varmeta) {
        varmeta_init(&ctr->varmeta[i]);
      }

      if (lb) {
        ctr->vars[i].bnd.lb = lb[i];
      } else {
        ctr->vars[i].bnd.lb = -INFINITY;
      }

      if (ub) {
        ctr->vars[i].bnd.ub = ub[i];
      } else {
        ctr->vars[i].bnd.ub = INFINITY;
      }
   }

   if (v) {
      v->type = EquVar_Compact;
      v->own = false;
      v->start = cdat->total_n;
      v->size = nb;
   }

   cdat->total_n += nb;

   return OK;
}

/**
 * @brief Add box constraint variables with identical bounds 
 *
 * @ingroup publicAPI
 *
 * @param       ctr  the container
 * @param       nb   the number of constraints to add
 * @param[out]  v    the abstract variable
 * @param       lb   the lower bound
 * @param       ub   the upper bound
 *
 * @return           the error code
 */
int rctr_add_box_vars_id(Container * restrict ctr, unsigned nb,
                          Avar * restrict v, double lb,
                          double ub)
{
   RhpContainerData *cdat = (RhpContainerData *)ctr->data;

   S_CHECK(chk_ctrdat_space(cdat, nb, __func__));

   for (unsigned i = cdat->total_n; i < cdat->total_n + nb; ++i) {
      var_init(&ctr->vars[i], i, VAR_X);
      if (ctr->varmeta) {
        varmeta_init(&ctr->varmeta[i]);
      }
      ctr->vars[i].bnd.lb = lb;
      ctr->vars[i].bnd.ub = ub;
   }

   if (v) {
      v->type = EquVar_Compact;
      v->own = false;
      v->start = cdat->total_n;
      v->size = nb;
   }

   cdat->total_n += nb;

   return OK;
}

int rctr_add_box_vars_ops(Container* ctr, unsigned nb_vars, const void* env,
                           const struct var_genops* varfill)
{
   RhpContainerData *cdat = (RhpContainerData *)ctr->data;

   S_CHECK(chk_ctrdat_space(cdat, nb_vars, __func__));

   for (unsigned i = cdat->total_n, indx = 0; i < cdat->total_n + nb_vars; ++i, ++indx) {
      var_init(&ctr->vars[i], i, VAR_X);
      if (ctr->varmeta) {
        varmeta_init(&ctr->varmeta[i]);
      }

      if (varfill->set_type) {
         ctr->vars[i].type = varfill->set_type(env, indx);
      }

      if (varfill->get_lb) {
         ctr->vars[i].bnd.lb = varfill->get_lb(env, indx);
      } else {
         ctr->vars[i].bnd.lb = -INFINITY;
      }

      if (varfill->get_ub) {
         ctr->vars[i].bnd.ub = varfill->get_ub(env, indx);
      } else {
         ctr->vars[i].bnd.ub = INFINITY;
      }

      if (varfill->get_value) {
         ctr->vars[i].value = varfill->get_value(env, indx);
      }

      if (varfill->get_multiplier) {
         ctr->vars[i].multiplier = varfill->get_multiplier(env, indx);
      }

   }

   cdat->total_n += nb_vars;

   return OK;
}

/**
 *  @brief Add multipliers to a model
 *
 *  From a constraint \f$g(x)\in K\f$, the associated multipliers belongs to
 *  the polar cone \f$K^{\circ}\f$.
 *
 *  @warning if vidx is a valid index on input, the total_n number is not changed
 *  It is the caller responsibility to increase that number
 *
 *  @param ctr         the container object
 *  @param cone        the type of cone
 *  @param cone_data   the necessary data to define the cone
 *  @param[in,out] vi  on input, the desired index, on output the index of the
 *                     multiplier variable
 *
 *  @return            the error code
 */
int rctr_add_multiplier_polar(Container *ctr, enum cone cone, void* cone_data,
                               rhp_idx *vi)
{
   enum cone mcone;
   void *mcone_data;

   /* TODO(xhub) make 2 fns model_add_multiplier_polar and model_add_multiplier_dual
    * to remove that boolean; then all the code is in model_add_multiplier_common*/
   S_CHECK(cone_polar(cone, cone_data, &mcone, &mcone_data));
//      S_CHECK(cone_dual(cone, cone_data, &mcone, &mcone_data));
   return add_multiplier_common_(ctr, mcone, mcone_data, vi);
}

/**
 *  @brief Add multipliers to a model
 *
 *  From a constraint \f$g(x)\in K\f$, the associated multipliers belongs to
 *  the dual cone \f$K^{D}\f$.
 *
 *  @warning if vidx is a valid index on input, the total_n number is not changed
 *  It is the caller responsibility to increase that number
 *
 *  @param ctr         the container object
 *  @param cone        the type of cone
 *  @param cone_data   the necessary data to define the cone
 *  @param[in,out] vi  on input, the desired index, on output the index of the
 *                     multiplier variable
 *
 *  @return            the error code
 */
int rctr_add_multiplier_dual(Container *ctr, enum cone cone, void* cone_data,
                              rhp_idx *vi)
{
   enum cone mcone;
   void *mcone_data;

   S_CHECK(cone_dual(cone, cone_data, &mcone, &mcone_data));
   return add_multiplier_common_(ctr, mcone, mcone_data, vi);
}

int rctr_var_setasdualmultiplier(Container *ctr, Equ *e, rhp_idx vi_mult)
{
   rhp_idx dummy = vi_mult;
   S_CHECK(rctr_add_multiplier_dual(ctr, e->cone, e->p.cone_data, &dummy));

   return dummy != vi_mult ? Error_RuntimeError : OK;
}

/**
 * @brief Copy a variable into a new model
 *
 * @param ctr   the new model
 * @param vsrc  the source variable
 *
 * @return      the error code
 */
int rctr_copyvar(Container *ctr, const Var * restrict vsrc)
{
   RhpContainerData *cdat = (RhpContainerData *)ctr->data;

   S_CHECK(chk_ctrdat_space(cdat, 1, __func__));

   rhp_idx vi = cdat->total_n;
   Var * restrict vdst = &ctr->vars[vi];
   vdst->idx = vi;
   vdst->type = vsrc->type;
   vdst->basis = vsrc->basis;
   vdst->is_conic = vsrc->is_conic;

   /* TODO(xhub) CONE SUPPORT */
   vdst->bnd.lb = vsrc->bnd.lb;
   vdst->bnd.ub = vsrc->bnd.ub;

   vdst->value = vsrc->value;
   vdst->multiplier = vsrc->multiplier;

   cdat->total_n++;

   return OK;
}

