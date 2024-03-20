#include "reshop_config.h"
#include "asnan.h"

#include <math.h>

#include "ctr_rhp.h"
#include "ctrdat_rhp.h"
#include "ctrdat_rhp_priv.h"
#include "cmat.h"
#include "container.h"
#include "equ_modif.h"
#include "equvar_helpers.h"
#include "macros.h"
#include "nltree.h"
#include "nltree_priv.h"
#include "printout.h"


/** @file cmat.c 
 *
 * @brief Container matrix (stored as list) functions
 */

/**
 *
 * @defgroup CMatUnsafeUpdate Unsafe container matrix update
 *
 * These functions add a variable to an equation without performing any checks.
 * This will result in a inconsistent container matrix. Use at your own risk.
 *
 */



DBGUSED static bool cmat_chk_equvaridx(RhpContainerData *cdat, rhp_idx ei, rhp_idx vi)
{
   if (!cdat) { return false; }
   if (!valid_ei_(ei, cdat->total_m, __func__)) { return false; }
   if (valid_vi(vi) && !valid_vi_(vi, cdat->total_n, __func__)) { return false; }

   return (cdat->equs[ei] || rctr_eq_not_deleted(cdat, ei));
}

/* \TODO(xhub) is this still necessary? */
static int push_on_deleted(RhpContainerData *cdat, CMatElt *ce)
{
   assert(ce->ei < cdat->max_m);
   if (!cdat->deleted_equs) {
      CALLOC_(cdat->deleted_equs, CMatElt *, cdat->max_m);
   }

   cdat->deleted_equs[ce->ei] = ce;

   return OK;
}


static int _debug_print_var(Container *ctr, rhp_idx vidx)
{
#ifdef DEBUG_MR
   char *varname = ctr_printvarname(ctr, vidx);
   printout(PO_INFO, "%s :: Variable %s (%d) information: ", __func__, varname, vidx);

   struct model_repr *model = (struct model_repr *)ctr->data;
   assert(model);
   const struct model_elt *me = model->vars[vidx];
   if (!me) {
      printout(PO_INFO, "Variable is no longer in the active model\n");
      return OK;
   }

   while (me) {
      printout(PO_INFO, "Equ %s (#%d); ", ctr_printequname(ctr, me->eidx), me->eidx);
      me = me->next_equ;
   }
   puts("");
#endif

   return OK;
}

static int _debug_print_me(Container *ctr, const CMatElt *me)
{
#ifdef DEBUG_MR
   cmat_elt_print(PO_INFO, me, ctr);
   printout(PO_INFO, "%s :: Model element for variable %s (%d) in equation %s (%d)\n",
           __func__, ctr_printvarname(ctr, me->vidx), me->vidx,
           ctr_printequname(ctr, me->eidx), me->eidx);

   printout(PO_INFO, "\t prev_equ = %s (%d); ",
         me->prev_equ ? ctr_printequname(ctr, me->prev_equ->eidx) : "nil",
         me->prev_equ ? me->prev_equ->eidx: IdxNA);
   printout(PO_INFO, "next_equ = %s (%d); ",
         me->next_equ ? ctr_printequname(ctr, me->next_equ->eidx) : "nil",
         me->next_equ ? me->next_equ->eidx: IdxNA);
   printout(PO_INFO, "next_var = %s (#%d)\n",
         me->next_var ? ctr_printvarname(ctr, me->next_var->vidx) : "nil",
         me->next_var ? me->next_var->vidx : IdxNA);
#endif

   return OK;
}

void cmat_elt_print(unsigned mode, CMatElt *ce, Container *ctr)
{
   printout(mode, "Container matrix element for variable %s in equation %s\n",
            ctr_printvarname(ctr, ce->vi), ctr_printequname(ctr, ce->ei));

   printout(mode, "\t prev_equ = %s; ",
            ce->prev_equ ? ctr_printequname(ctr, ce->prev_equ->ei) : "nil");
   printout(mode, "next_equ = %s; ",
            ce->next_equ ? ctr_printequname(ctr, ce->next_equ->ei) : "nil");
   printout(mode, "next_var = %s\n",
            ce->next_var ? ctr_printvarname(ctr, ce->next_var->vi) : "nil");

}

static CMatElt* cmat_elt_new(Container *ctr, rhp_idx ei, rhp_idx vi, bool isNL,
                             double val)
{
   assert(valid_ei(ei));
   RhpContainerData *cdat = ctr->data; 

   CMatElt* me;
   MALLOC_NULL(me, CMatElt, 1);
   me->value = val;
   me->next_var = NULL;
   me->next_equ = NULL;
   me->ei = ei;
   me->vi = vi;
   me->isNL = isNL;
   me->isQuad = false;
   me->placeholder = false;

   /* Is this the first time the variable appears? */
   if (!cdat->vars[vi]) {

      me->prev_equ = NULL;
      cdat->vars[vi] = me;
      ctr->n++;

      trace_ctr("[container] ADD var '%s' via equ '%s'%s",
                ctr_printvarname(ctr, vi), ctr_printequname(ctr, ei),
                ctr->vars[vi].is_deleted ? ", previously deleted\n" : "\n");

      ctr->vars[vi].is_deleted = false;

      if (ctr->varmeta) {
         /*  TODO(xhub) this should be investigated */
//         varmeta[vi].type = Varmeta_Undefined;
         ctr->varmeta[vi].ppty = VarPptyNone;
      }

   /* If there is no last_equ, then it is was a placeholder var  */
   } else if (!cdat->last_equ[vi]) {

      assert(cdat->vars[vi]->placeholder);
      me->prev_equ = NULL;

      FREE(cdat->vars[vi]);
      cdat->vars[vi] = me;

   } else {
      /* We show have a pointer to the equation were the variable previously appeared */
      me->prev_equ = cdat->last_equ[vi];
      me->prev_equ->next_equ = me;
      /*  TODO(xhub) make an option for that */
      bool do_test = false;
#ifndef NDEBUG
      do_test = true;
#endif
      if (do_test && !cmat_chk_varnotinequ(me->prev_equ, ei)) {
         error("%s :: variable %u already appeared in equation %u",
                  __func__, vi, ei);
         return NULL;
      }
   }

   assert(!ctr->varmeta || !(ctr->varmeta[vi].ppty & VarIsDeleted));

   /* Always update last equation since we add new equation */
   cdat->last_equ[vi] = me;

   return me;
}

/**
 * @brief container matrix element for a constant equation
 *
 * Some equation may just be constant and are active. This special element
 * ensures that the equation is not thrown away during the model compression
 *
 * @param ei   the equation index
 *
 * @return     the error code
 */
struct ctr_mat_elt* cmat_cst_equ(rhp_idx ei)
{
   assert(valid_ei(ei));

   struct ctr_mat_elt *me;
   MALLOC_NULL(me, struct ctr_mat_elt, 1);

   me->value  = SNAN;
   me->isNL   = false;
   me->isQuad = false;
   me->placeholder = true;
   me->next_var = NULL;
   me->next_equ = NULL;
   me->prev_equ = NULL;
   me->ei     = ei;
   me->vi     = IdxNA;

   return me;
}

/**
 * @brief Return a special model element to indicate that a variable is
 * perpendicular to an equation.
 *
 * @warning Only when the variable does not appear in the model anymore,
 * but there is still an inclusion relation involving this variable
 *
 * @param vi  the variable index
 * @param ei  the equation index
 *
 * @return    the error code
 */
struct ctr_mat_elt* cmat_isolated_var_perp_equ(rhp_idx vi, rhp_idx ei)
{
   /*  TODO(xhub) Do not use malloc, but an arena in the model.
    *  Right now this requires the use of mem2free in order not to memleaks as
    *  this ME will not appear in any equation, and will not be freed in
    *  model_dealloc */
   assert(valid_vi(vi));

   struct ctr_mat_elt *me;
   MALLOC_NULL(me, struct ctr_mat_elt, 1);

   me->value  = SNAN;
   me->isNL   = false;
   me->isQuad = false;
   me->placeholder = true;
   me->next_var = NULL;
   me->next_equ = NULL;
   me->prev_equ = NULL;
   me->ei     = ei;
   me->vi     = vi;

   return me;
}

struct ctr_mat_elt* cmat_objvar(rhp_idx objvar)
{
   assert(valid_vi(objvar));

   struct ctr_mat_elt *me;
   MALLOC_NULL(me, struct ctr_mat_elt, 1);

   me->value  = SNAN;
   me->isNL   = false;
   me->isQuad = false;
   me->placeholder = true;
   me->next_var = NULL;
   me->next_equ = NULL;
   me->prev_equ = NULL;
   me->ei     = IdxNA;
   me->vi     = objvar;

   return me;
}

/** @brief Add new linear variables to an equation in the container matrix
 *
 * @ingroup CMatUnsafeUpdate
 *
 *  @param ctr   the container
 *  @param ei    the equation index
 *  @param v     list of variables appearing in the equation
 *  @param vals  the values of the variable coefficients
 *
 *  @return      the error code
 */
int cmat_equ_add_newlvar(Container *ctr, rhp_idx ei, rhp_idx vi, double val)
{
   RhpContainerData *cdat = (RhpContainerData *)ctr->data;
   assert(cmat_chk_equvaridx(cdat, ei, vi));

   CMatElt * restrict prev_e = cdat->equs[ei];
   assert(cmat_chk_varnotinequ(cdat->vars[vi], ei));

   CMatElt * restrict me;
   A_CHECK(me, cmat_elt_new(ctr, ei, vi, false, val));

   /*  TODO(xhub) think of placeholders */
   if (prev_e) {
      me->next_var = prev_e;
      cdat->equs[ei] = me;
   } else {
      cdat->equs[ei] = me;
   }

   return OK;
}

/** @brief Add new linear variables to an equation in the container matrix
 *
 * @ingroup CMatUnsafeUpdate
 *
 *  @param ctr   the container
 *  @param ei    the equation index
 *  @param v     list of variables appearing in the equation
 *  @param vals  the values of the variable coefficients
 *
 *  @return      the error code
 */
int cmat_equ_add_newlvars(Container *ctr, rhp_idx ei, const Avar *v,
                          const double *vals)
{
   RhpContainerData *cdat = (RhpContainerData *)ctr->data;
   assert(cmat_chk_equvaridx(cdat, ei, IdxNA));

   CMatElt* prev_e = cdat->equs[ei];
   /* TODO: just add all elements in front? */
   if (prev_e) { while (prev_e->next_var) { prev_e = prev_e->next_var; }; }

   /* TODO: this looks wrong: what if the variable was already present*/
   /* Add all the variables from that equation  */
   for (size_t i = 0; i < v->size; ++i) {
      rhp_idx vi = avar_fget(v, i);

      S_CHECK(vi_inbounds(vi, cdat->total_n, __func__));
      assert(cmat_chk_varnotinequ(cdat->vars[vi], ei));
 
      CMatElt* me;
      A_CHECK(me, cmat_elt_new(ctr, ei, vi, false, vals[i]));

      /* If there is a previous variable */
      if (prev_e) {
         prev_e->next_var = me;
      } else /* if not, the eqn is most likely empty */ {
         cdat->equs[ei] = me;
      }

      /* Update for the next variable */
      prev_e = me;
   }

   return OK;

}

/**
 * @brief  Finish adding the all the linear and constant part of the equation
 *         to the container matrix
 *
 * This should be used only if the equation is built from scratch, by using
 * equ_add_newvar(), or equ_add_linvars().
 * Then, this function finalizes the insertion of the equation.
 *
 * If one wishes to modify an existing equation, it is advised to use
 * model_equ_addnewvar(), model_equ_addlinvars()
 *
 * @ingroup CMatUnsafeUpdate
 *
 * @param ctr  the container
 * @param e    the equation
 *
 * @return     the error code
 */
int cmat_sync_lequ(Container *ctr, Equ *e)
{
   RhpContainerData *cdat = (RhpContainerData *)ctr->data;
   rhp_idx ei = e->idx;
   assert(cmat_chk_equvaridx(cdat, ei, IdxNA));

   Lequ *lequ = e->lequ;

   if (!lequ) { goto _end; }

   CMatElt *prev_e = cdat->equs[ei];
   if (prev_e) { while (prev_e->next_var) { prev_e = prev_e->next_var; } }

   /* ----------------------------------------------------------------------
    * Walk over the linear part and add the linear variables in the container matrix
    * ---------------------------------------------------------------------- */

   rhp_idx * restrict vis = lequ->vis;
   double  * restrict vals  = lequ->coeffs;
   unsigned len = lequ->len;

   size_t total_n = cdat->total_n;
   for (unsigned i = 0; i < len; ++i) {
      rhp_idx vi = vis[i];
      S_CHECK(vi_inbounds(vi, total_n, __func__));

      CMatElt *me;
      A_CHECK(me, cmat_elt_new(ctr, ei, vi, false, vals[i]));
      _debug_print_var(ctr, vi);

      if (prev_e) {
         prev_e->next_var = me;
         _debug_print_me(ctr, prev_e);
      } else {
          cdat->equs[ei] = me;
      }

      /* Update for the next variable */
      prev_e = me;
   }
   _debug_print_me(ctr, prev_e);

_end:
   S_CHECK(rctr_fix_equ(ctr, ei));

   return OK;
}
/** 
 *  @brief specialized function to add a linear variable to an equation without
 *  knowing if it is already present in it and in which form.
 *
 *  The main point of this function is to return the type of variable, if it
 *  was already in that equation. Then the parent function can add this to the
 *  linear or expression tree.
 *
 *  @param         ctr    the container
 *  @param         ei     the equation index
 *  @param         vi     the variable index
 *  @param         val    the coefficient of the variable
 *  @param[in,out] isNL   the nonlinear flag
 *
 *  @return              the error code
 */
int cmat_equ_add_lvar(Container *ctr, rhp_idx ei, rhp_idx vi, double val, bool *isNL)
{
   RhpContainerData *cdat = (RhpContainerData *)ctr->data;
   assert(cmat_chk_equvaridx(cdat, ei, vi));

   CMatElt *prev_e = cdat->equs[ei];

   if (prev_e) {
      do {
         /* The variable has been found and we can return  */
         if (prev_e->vi == vi) {
            prev_e->value += val;
            *isNL = prev_e->isNL;
            return OK;
         }

         if (!prev_e->next_var) { break; }
         prev_e = prev_e->next_var;
      }
      while (true);
   }

   CMatElt *me;
   A_CHECK(me, cmat_elt_new(ctr, ei, vi, *isNL, val));

   if (prev_e) {
      prev_e->next_var = me;
   } else {
      cdat->equs[ei] = me;
   }

   return OK;
}

/**
 * @brief Add a nonlinear variable to an equation
 *
 * If the variable is already is already present, it is flipped to be nonlinear
 *
 * @param ctr      the model
 * @param ei       the equation index
 * @param vi       the variable index
 * @param jac_val  the jacobian value for the variable
 *
 * @return         the error code
 */
int cmat_equ_add_nlvar(Container *ctr, rhp_idx ei, rhp_idx vi, double jac_val)
{
   RhpContainerData *cdat = (RhpContainerData *)ctr->data;
   assert(cmat_chk_equvaridx(cdat, ei, vi));

   /*  We need to scan from the start of the equation, since the variable
    *  may already be present */
   CMatElt *prev_e = cdat->equs[ei];

#ifdef DEBUG_MR
   DPRINT("%s :: adding variable %s (#%d) to the model, NL = %s\n",
         __func__, ctr_printvarname(ctr, vi), vi, "true");
#endif
   if (prev_e) {
      do {
         if (prev_e->vi == vi) {

            /* -----------------------------------------------------------
             * This variable is already in this equation
             * We just invalidate the jacobian value and check the
             * non-linear status, before moving to the next variable
             * ---------------------------------------------------------- */

            prev_e->value += jac_val;

            if (!prev_e->isNL) {
               prev_e->isNL = true;
               /*  TODO(xhub) figure out whether this is correct
                   prev_e->value = NAN; */

               /* -------------------------------------------------------
                * We need to move the variable from the linear part to the
                * non-linear one
                * ------------------------------------------------------- */
               S_CHECK(equ_switch_var_nl(ctr, &ctr->equs[ei], vi));
            }
            goto _end_loop;
         }

         if (!prev_e->next_var) { break; }
         prev_e = prev_e->next_var;

      } while (true);
   }

   CMatElt *e;
   A_CHECK(e, cmat_elt_new(ctr, ei, vi, true, jac_val));

   if (prev_e) {
      prev_e->next_var = e;
   } else {
      cdat->equs[ei] = e;
   }
   /* ------------------------------------------------------------------
    * Move to the next variable 
    * ------------------------------------------------------------------ */
_end_loop: ;

   return OK;
}

/**
 * @brief In the container matrix, add variables to an equation, except one
 *
 * If isNL is true, mark these variables as nonlinear
 *
 * The except one is handy for the case where it was the one defining a mapping
 * from an equality constraint
 *
 * @param ctr     the container
 * @param ei      the equation
 * @param v       the variables to add
 * @param vi_no   if valid, index of the variable in v not to add
 * @param values  the values
 * @param isNL    if true, the variables appear nonlinearly in the equation
 * 
 * @return        the error code
 */
int cmat_equ_add_vars_excpt(Container *ctr, rhp_idx ei, Avar *v, rhp_idx vi_no,
                            double* values, bool isNL)
{
   RhpContainerData *cdat = (RhpContainerData *)ctr->data;
   assert(cmat_chk_equvaridx(cdat, ei, IdxNA));

   for (unsigned i = 0; i < v->size; ++i) {

      /* -------------------------------------------------------------------
       *
       *  We need to scan from the start of the equation, since the variable
       *  may already be present
       *
       * ------------------------------------------------------------------- */

      CMatElt *prev_e = cdat->equs[ei];

      rhp_idx vi = avar_fget(v, i);
      if (vi == vi_no) continue;

       if (!valid_vi(vi) || (unsigned)vi >= cdat->total_n) {
         return Error_IndexOutOfRange;
      }

#ifdef DEBUG_MR
      DPRINT("%s :: adding variable %s (%d) to the model, NL = %s\n",
            __func__, ctr_printvarname(ctr, vidx), vidx, isNL ? "true" : "false");
#endif

      if (prev_e) {
         do {
            if (prev_e->vi == vi) {
               /* This variable is already in this equation
                * We just invalidate the jacobian value and check the
                * non-linear status, before moving to the next variable*/
               prev_e->value += values ? values[i] : SNAN;
               if (!prev_e->isNL && isNL) {
                  prev_e->isNL = isNL;
                  /*  TODO(xhub) figure out whether this is correct
                  prev_e->value = NAN; */
                  /*  We need to move the variable from the linear part to the
                   *  non-linear one*/
                  S_CHECK(equ_switch_var_nl(ctr, &ctr->equs[ei], vi));
               }
               goto _end_loop;
            }
            if (!prev_e->next_var) { break; }
            prev_e = prev_e->next_var;
         }
         while (true);
      }

      double val;
      if (values) { val = values[i]; } else { val = SNAN; }

      CMatElt *e;
      A_CHECK(e, cmat_elt_new(ctr, ei, vi, isNL, val));

      if (prev_e) {
         prev_e->next_var = e;
      } else {
          cdat->equs[ei] = e;
      }
_end_loop: ;
   }

   return OK;
}

/**
 * @brief In the container matrix, add variables to an equation, except one
 *
 * If isNL is true, mark these variables as nonlinear
 *
 * @param ctr     the container
 * @param ei      the equation
 * @param v       the variables to add
 * @param values  the values
 * @param isNL    if true, the variables appear nonlinearly in the equation
 * 
 * @return        the error code
 */
int cmat_equ_add_vars(Container *ctr, rhp_idx ei, Avar *v, double* values, bool isNL)
{
   RhpContainerData *cdat = (RhpContainerData *)ctr->data;
   assert(cmat_chk_equvaridx(cdat, ei, IdxNA));

   for (unsigned i = 0; i < v->size; ++i) {
      /*  We need to scan from the start of the equation, since the variable
       *  may already be present */
      CMatElt *prev_e = cdat->equs[ei];

      /* XXX this is suboptimal --xhub  */
      rhp_idx vi = avar_fget(v, i);

      S_CHECK(vi_inbounds(vi, cdat->total_n, __func__));

#ifdef DEBUG_MR
      DPRINT("%s :: adding variable %s (%d) to the model, NL = %s\n",
            __func__, ctr_printvarname(ctr, vidx), vidx, isNL ? "true" : "false");
#endif

      if (prev_e) {
         do {
            if (prev_e->vi == vi) {

               /* This variable is already in this equation
                * We just invalidate the jacobian value and check the
                * non-linear status, before moving to the next variable*/

               prev_e->value += values ? values[i] : SNAN;

               if (!prev_e->isNL && isNL) {
                  prev_e->isNL = isNL;
                  /*  TODO(xhub) figure out whether this is correct
                  prev_e->value = NAN; */
                  /*  We need to move the variable from the linear part to the
                   *  non-linear one*/
                  S_CHECK(equ_switch_var_nl(ctr, &ctr->equs[ei], vi));
               }

               goto _end_loop;
            }

            if (!prev_e->next_var) { break; }

            prev_e = prev_e->next_var;

         } while (true);
      }

      double val = values ? values[i] : SNAN;

      CMatElt *e = cmat_elt_new(ctr, ei, vi, isNL, val);

      if (!e) return Error_InsufficientMemory;

      if (prev_e) {
         prev_e->next_var = e;
      } else {
          cdat->equs[ei] = e;
      }

_end_loop: ;
   }

   return OK;
}

/** @brief remove a variable from an equation in the container matrix
 *
 *  @warning    it is not advised to use this function directly. Use
 *              equ_rm_var() instead
 *
 *  @param ctr  the container
 *  @param ei   the equation
 *  @param vi   the variable
 *
 *  @return     the error code
 */
int cmat_equ_rm_var(Container *ctr, rhp_idx ei, rhp_idx vi)
{
   RhpContainerData *cdat = (RhpContainerData *)ctr->data;
   assert(cmat_chk_equvaridx(cdat, ei, vi));

   CMatElt *e = cdat->equs[ei];
   CMatElt *prev_e = NULL;

   if (!e) {
      error("%s :: equation %s is empty!\n", __func__, ctr_printequname(ctr, ei));
      return Error_NullPointer;
   }

   trace_ctr("[container] DEL var %s from equ %s\n",
             ctr_printvarname(ctr, vi), ctr_printequname(ctr, ei));

   /* -----------------------------------------------------------------------
    * 1. First find the element that correspond to the (pair variable, equation)
    *
    * 2. Update the following data: note that we update either a model element
    *    or a field in the model
    *
    *   a) next_var in the previous horizontal element
    *         OR
    *      model->eqns[eidx] if it is the first
    *
    *   b) prev_equ in the next vertical element
    *         OR
    *      model->last_equ[vidx] if it was the last one
    *
    *   c) next_equ in the previous vertical element
    *         OR
    *      model->vars[vidx] if this variable appeared first in that equation
    *
    */
   while (true) {
      if (e->vi == vi) {

         /* -----------------------------------------------------------------
          * If there is a previous equation where this variable appear, replace
          * the link to this variable to the next one
          * ----------------------------------------------------------------- */

         if (prev_e) {
            assert(cdat->equs[ei] != e);
            prev_e->next_var = e->next_var;
         } else {
            if (e->next_var) {
               cdat->equs[ei] = e->next_var;
            } else {
               /*  EQU2 */
               double cst = equ_get_cst(&ctr->equs[ei]);
               if (ctr->equs[ei].object == Mapping && isfinite(cst)) {
                  A_CHECK(cdat->equs[ei], cmat_cst_equ(ei));
               } else {
                  error("[container] ERROR: the equation %s no longer contains "
                        "any variable and has a CST of %e",
                        ctr_printequname(ctr, ei), cst);
                  return Error_Inconsistency;
               }
            }
         }

         /* -----------------------------------------------------------------
          * Update the backpointer for the next equation. If the next equation
          * does not exists, update the last equation field for the variable
          *
          * WARNING: this has to be done before the field e->prev_equ->next_equ
          * is updated.
          * ----------------------------------------------------------------- */

         if (e->next_equ) {
            e->next_equ->prev_equ = e->prev_equ;
         } else {
            assert(cdat->last_equ[vi] == e);
            cdat->last_equ[vi] = e->prev_equ;
/* \TODO(xhub) check that it is valid to not do the following
            if (e->prev_equ) {
               model->last_equ[vidx] = e->prev_equ;
            } else {
               model->last_equ[vidx] = e->prev_equ;
               char *varname = ctr_printvarname(ctr, vidx, name, 255);
               error("%s :: variable %s (%d) is going to disappear from the "
                      "complete model. This is not allowed\n", __func__, name,
                      vidx);
               return Error_Unconsistency;
            }
*/
         }

         /* -----------------------------------------------------------------
          * Update the link to the next equation for this variable
          *
          * If there is no previous equation where this variable appears AND
          * there is no next equation, then the variable must disappear.
          * ----------------------------------------------------------------- */

         if (e->prev_equ) {
            e->prev_equ->next_equ = e->next_equ;
         } else {

            cdat->vars[vi] = e->next_equ;

            if (!e->next_equ) {
               assert(!cdat->last_equ[vi]);

               ctr->n--;
               ctr->vars[vi].is_deleted = true;
               if (ctr->varmeta) {
                   ctr->varmeta[vi].ppty |= VarIsDeleted;
               }

            trace_ctr("[container] %14s var '%s' deleted\n",
                      "->", ctr_printvarname(ctr, vi));
            }
         }

         break;
      }
      prev_e = e;
      e = e->next_var;

      if (!e) {
         error("[container] ERROR: variable '%s' does not appear in equation "
               "'%s'\n", ctr_printvarname(ctr, vi), ctr_printequname(ctr, ei));
         return Error_Inconsistency;
      }
   }

   FREE(e);

   return OK;
}

/** @brief Fill the equation row in the matrix representation of the container
 *
 *  This function just updates the container matrix, not the equation itself.
 *  The variables appearing the equation must have been added to the model
 *  representation before. The equation must have been added to the model 
 *
 *  @param ctr      the container
 *  @param ei       the index of the equation to be updated
 *  @param v        variables appearing in the equation
 *  @param values value of the jacobian at the current point for each variable
 *  @param nlflags  array of booleans to indicate whether the variable appears
 *                  linearly or not in the equation
 *
 *  @return         the error code
 */
int cmat_fill_equ(Container *ctr, rhp_idx ei, const Avar * restrict v,
                  const double * restrict values, const bool * restrict nlflags)
{
   RhpContainerData *cdat = (RhpContainerData *)ctr->data;
   assert(cmat_chk_equvaridx(cdat, ei, IdxNA));
   assert(ctr->equs[ei].idx == ei);

   S_CHECK(cdat_equ_init(cdat, ei));

   CMatElt* prev_e = NULL;

   /* Add all the variables from that equation */
   for (unsigned i = 0, total_n = cdat->total_n, len = v->size; i < len; ++i) {
      rhp_idx vi = avar_fget(v, i);

      S_CHECK(vi_inbounds(vi, total_n, __func__));
 
      CMatElt* e;
      A_CHECK(e, cmat_elt_new(ctr, ei, vi, nlflags[i], values[i]));

      /* If there is a previous variable */
      if (prev_e) {
         prev_e->next_var = e;
      } else /* if not, the eqn is most likely empty */ {
         cdat->equs[ei] = e;
      }

      /* Update for the next variable */
      prev_e = e;
   }

   return OK;

}


/**
 * @brief remove an equation from the container matrix
 *
 * @param ctr  the container
 * @param ei   the equation index to be removed
 *
 * @return     the error code
 */
int cmat_rm_equ(Container *ctr, rhp_idx ei)
{
   RhpContainerData *cdat = (RhpContainerData *)ctr->data;
   assert(cmat_chk_equvaridx(cdat, ei, IdxNA));

   CMatElt * restrict me = cdat->equs[ei];

   if (!me) {
      error("[container] ERROR: cannot remove equation '%s', it is already inactive\n",
            ctr_printequname(ctr, ei));
      return Error_NullPointer;
   }

   /* Clear from the model */
   cdat->equs[ei] = NULL;

   /* Add the equation to deleted_equs  */
   assert(me->ei == ei);
   S_CHECK(push_on_deleted(cdat, me));

   /* Remove the equation from the model*/
   while (me) {

      /* Update last_equ if necessary */
      if (me->next_equ) {
         me->next_equ->prev_equ = me->prev_equ;
      } else {
         assert(me == cdat->last_equ[me->vi]);
         cdat->last_equ[me->vi] = me->prev_equ;
      }

      /* If the variable appeared in a previous equ */
      if (me->prev_equ) {
         me->prev_equ->next_equ = me->next_equ;
      } else { /* We are deleting the first equation where the variable appears */
         /* set model->vars  */
         rhp_idx vi = me->vi;
         cdat->vars[vi] = me->next_equ;

         /*  If this is true, the variable disappear from the container */
         if (!me->next_equ) {
            assert(!cdat->last_equ[vi]);

            ctr->n--;
            ctr->vars[vi].is_deleted = true;
            if (ctr->varmeta) {
               ctr->varmeta[vi].ppty |= VarIsDeleted;
            }

            trace_ctr(" %14s var '%s'\n", "-> DEL", ctr_printvarname(ctr, vi));

         }
      }

      _debug_print_var(ctr, me->vi);

      /* Update the next variable */
      me = me->next_var;
   }

   return OK;
}

/**
 * @brief Create a flipped copy of an equation in the container matrix
 *
 * @param ctr      the container
 * @param ei_src   the equation to copy
 * @param ei_dst   the index for the flipped copy
 *
 * @return         the error code
 */
int cmat_cpy_equ_flipped(Container *ctr, rhp_idx ei_src, rhp_idx ei_dst)
{
   RhpContainerData *cdat = ctr->data;
   const CMatElt * restrict me_src = cdat->equs[ei_src];
   CMatElt * restrict prev_e = NULL;

   S_CHECK(ei_inbounds(ei_src, ctr_nequs_total(ctr), __func__));
   S_CHECK(ei_inbounds(ei_dst, ctr_nequs_total(ctr), __func__));

   if (cdat->equs[ei_dst]) {
      error("[container] ERROR: the equation #%u is not empty, cannot flip '%s' "
            "into it!\n", ei_dst, ctr_printequname(ctr, ei_src));
      return Error_RuntimeError;
   }

   /* Add all the variables from that equation  */
   while (me_src) {

      CMatElt *me_dst;
      A_CHECK(me_dst, cmat_elt_new(ctr, ei_dst, me_src->vi, me_src->isNL, -me_src->value));

      /* If there is a previous variable */
      if (prev_e) {
         prev_e->next_var = me_dst;
      } else /* if not, the eqn is most likely empty */ {
         cdat->equs[ei_dst] = me_dst;
      }

      /* Update for the next variable */
      prev_e = me_dst;

      /* Go to the next variable in the equation */
      me_src = me_src->next_var;
   }

   return OK;
}

/**
 * @brief Create a copy of an equation in the container matrix
 *
 * If specified, one variable is not copied
 *
 * @param ctr      the container
 * @param ei_src   the equation to copy
 * @param ei_dst   the index for the flipped copy
 * @param vi_no    if valid, index of the variable not to copy
 *
 * @return         the error code
 */
int cmat_copy_equ_except(Container *ctr, rhp_idx ei_src, rhp_idx ei_dst, rhp_idx vi_no)
{
   RhpContainerData *cdat = ctr->data;
   assert(cmat_chk_equvaridx(cdat, ei_src, vi_no));

   S_CHECK(ei_inbounds(ei_src, ctr_nequs_total(ctr), __func__));
   S_CHECK(ei_inbounds(ei_dst, ctr_nequs_total(ctr), __func__));

   const CMatElt * restrict me_src = cdat->equs[ei_src];
   assert(me_src);
   CMatElt * restrict prev_e = NULL;

   if (cdat->equs[ei_dst]) {
      error("[container] ERROR: the equation #%u is not empty, cannot copy '%s' "
            "into it!\n", ei_dst, ctr_printequname(ctr, ei_src));
      return Error_RuntimeError;
   }

   /* Add all the variables from that equation  */
   while (me_src) {

      if (me_src->vi == vi_no) { me_src = me_src->next_var; continue; }

      CMatElt *me_dst;
      A_CHECK(me_dst, cmat_elt_new(ctr, ei_dst, me_src->vi, me_src->isNL, me_src->value));

      /* If there is a previous variable */
      if (prev_e) {
         prev_e->next_var = me_dst;
      } else /* if not, the eqn is most likely empty */ {
         cdat->equs[ei_dst] = me_dst;
      }

      /* Update for the next variable */
      prev_e = me_dst;

      /* Go to the next variable in the equation */
      me_src = me_src->next_var;
   }

   return OK;
}

/**
 * @brief Append equations from a container matrix to another container one
 *
 * @param ctr_dst 
 * @param ctr_src 
 * @param e 
 * @param ei_dst_start 
 * @return 
 */
int cmat_append_equs(Container * restrict ctr_dst,
                     const Container * restrict ctr_src,
                     const Aequ * restrict e,
                     rhp_idx ei_dst_start)
{
   RhpContainerData *cdat_dst = ctr_dst->data;
   RhpContainerData *cdat_src = ctr_src->data;

   rhp_idx ei_dst = ei_dst_start;

   unsigned len = e->size;
   cdat_dst->total_m += len;
   ctr_dst->m += len;

   for (unsigned i = 0; i < len; ++i) {

      rhp_idx ei_src = aequ_fget(e, i);
      assert(!valid_ei(ctr_getcurrent_ei(ctr_src, ei_src)) ||
             ei_dst == ctr_getcurrent_ei(ctr_src, ei_src));

      const CMatElt * restrict me_src = cdat_src->equs[ei_src];
      /* TODO GITLAB #110 */
      if (!me_src) { continue; }
      CMatElt * restrict prev_e = NULL;

      /* Add all the variables from that equation  */
      while (me_src) {

         rhp_idx vi_dst = ctr_getcurrent_vi(ctr_src, me_src->vi);
         assert(valid_vi_(vi_dst, cdat_dst->total_n, __func__));


         CMatElt *me_dst;
         A_CHECK(me_dst, cmat_elt_new(ctr_dst, ei_dst, vi_dst, me_src->isNL, me_src->value));

         /* If there is a previous variable */
         if (prev_e) {
            prev_e->next_var = me_dst;
         } else /* if not, the eqn is most likely empty */ {
            cdat_dst->equs[ei_dst] = me_dst;
         }

         /* Update for the next variable */
         prev_e = me_dst;

         /* Go to the next variable in the equation */
         me_src = me_src->next_var;
      }

      ei_dst++;

   }

   return OK;
}

/**
 * @brief Scale an equation in the container matrix
 *
 * @param ctr     the container
 * @param ei      the equation index
 * @param coeff   the coefficient
 *
 * @return        the error code
 */
int cmat_scal(Container *ctr, rhp_idx ei, double coeff)
{
   RhpContainerData *cdat = ctr->data;
   assert(cmat_chk_equvaridx(cdat, ei, IdxNA));

   CMatElt * restrict me = cdat->equs[ei]; assert(me);

   /* Add all the variables from that equation  */
   while (me) {

      me->value *= coeff;

      /* Go to the next variable in the equation */
      me = me->next_var;
   }

   return OK;
}

enum vtag {
   vnone    = 0,
   vlin     = 1,
   vquad    = 2,
   vnl      = 3,
   vchecked = 16,
};

static const char *vtag2str(enum vtag tag)
{
   switch (tag & 0xf) {
   case vnone: return "none";
   case vlin: return "lin";
   case vquad: return "quad";
   case vnl: return "NL";
   default: return "unkonwn tag ERROR";
   }
}

NONNULL static int cmat_chk_lequ(Container *ctr, RhpContainerData *cdat,
                                 rhp_idx ei, enum vtag *vtags) 
{
   int status = OK;

   Lequ *le = ctr->equs[ei].lequ;

   if (!le) return status;

   rhp_idx * restrict vidx = le->vis;
   double * restrict vals = le->coeffs;
   rhp_idx total_n = cdat->total_n;

   for (size_t j = 0, len = le->len; j < len; ++j) {
      double v = vals[j];
      rhp_idx vi = vidx[j];

      if (!isfinite(v)) {
         if (isnan(v) && vi < total_n && cdat->vars[vi]) {
            error("[cmat/check] ERROR: variable %s appears with value %E in "
                  "equation %s, but is marked as active\n",
                  ctr_printvarname(ctr, vi), v,
                  ctr_printequname(ctr, ei));
            status = Error_Inconsistency;

         } else if (!isnan(v)) {
            error("[cmat/check] ERROR: variable %s appears with value %E in "
                  "equation %s, which is inconsistent\n",
                  ctr_printvarname(ctr, vi), v,
                  ctr_printequname(ctr, ei));
            status = Error_Inconsistency;

         }
      } else {

         S_CHECK(vi_inbounds(vi, total_n, "[cmat/check]"));

         if (vtags[vi] != vnone) {
            error("[cmat/check] ERROR: variable %s already appeared in equation %s, "
                  "as %s\n", ctr_printvarname(ctr, vidx[j]),
                  ctr_printequname(ctr, ei), vtag2str(vtags[vi]));
            status = Error_Inconsistency;
         }

         vtags[vi] = vlin;
      }
   }

   return status;
}

int cmat_chk_expensive(Container *ctr)
{
   assert(ctr_is_rhp(ctr));

   int status = OK;
   RhpContainerData *restrict cdat = ctr->data;
   rhp_idx total_n = cdat->total_n, total_m = cdat->total_m;

   struct ctrmem CTRMEM working_mem = {.ptr = NULL, .ctr = ctr};
   A_CHECK(working_mem.ptr, ctr_getmem(ctr, total_n * sizeof(enum vtag)));
   enum vtag *vtags = working_mem.ptr;

   struct equ_meta * restrict emd = ctr->equmeta;

   for (rhp_idx i = 0, len = total_m; i < len; ++i)
   {
      CMatElt *ce = cdat->equs[i];

      if (ce && ce->placeholder) {
         if (valid_vi(ce->vi) || !valid_ei(ce->ei) || ce->ei != i || ce->next_var || ce->next_equ || ce->prev_equ) {
            error("[cmat/check] ERROR: placeholder for equation %s is invalid: "
                  "vi: %u; ei: %u; next_var: %p; prev_equ: %p; next_equ: %p\n",
                  ctr_printequname(ctr, i), ce->vi, ce->ei, (void*)ce->next_var,
                  (void*)ce->prev_equ, (void*)ce->next_equ);

            status = Error_Inconsistency;
         }
         continue;
      }

      if (!ce) {
         if (emd && !(emd[i].ppty & EquPptyIsDeleted)) {
            error("[cmat/check] ERROR: equation %s is absent from the container "
                  "matrix but not marked as such in the metadata\n",
                  ctr_printequname(ctr, i));
            status = Error_Inconsistency;
         }
         continue;
      }

      memset(vtags, 0, sizeof(enum vtag)*total_n);
      Lequ *le = ctr->equs[i].lequ;

      unsigned nlinvar = le ? le->len : 0;

        status = cmat_chk_lequ(ctr, cdat, i, vtags);
        if (status != OK) { continue;}

      /*TODO QUAD*/
      assert(!ce->isQuad);
      
      /* TODO thin container: change this */
      S_CHECK(rctr_getnl(ctr, &ctr->equs[i]));
      NlTree *nltree = ctr->equs[i].tree;

      /* ----------------------------------------------------------------
       * go over the tree and get all the variables
       * ---------------------------------------------------------------- */

      rhp_idx nNLvar = 0;

      if (nltree && nltree->root) {
         S_CHECK(nltree_getallvars(nltree));
         struct vlist * restrict vlist = nltree->v_list;
         nNLvar = vlist->idx;
         for (rhp_idx j = 0; j < nNLvar; ++j) {
            rhp_idx vi = vlist->pool[j];

            S_CHECK(vi_inbounds(vi, total_n, "[cmat/check]"));

            if (vtags[vi] != vnone) {
               error("[cmat/check] ERROR in equation %s: variable %s was tagged "
                     "as %s before %s", ctr_printequname(ctr, i), ctr_printvarname(ctr, vi),
                     vtag2str(vtags[vi]), vtag2str(vnl));
            }

            vtags[vi] = vnl;
         } 
      }

      while (ce) {

         rhp_idx ei = ce->ei, vi = ce->vi;

         if (ei != i) {
            error("[cmat/check] ERROR: inconsistency between the container matrix "
                  "and the equation number: %u vs %u\n", ei, i);
            status = Error_Inconsistency;
            goto _end;
         }

         if (!valid_vi_(vi, total_n, "[cmat/check]")) {
            error("[cmat/check] ERROR: equation %s has invalid vi\n",
                  ctr_printequname(ctr, i));
            status = Error_Inconsistency;
            goto _end;
         }

         if (!cdat->vars[vi]) {
            error("[cmat/check] ERROR: variable %s in equation %s is absent "
                  "from the container matrix\n", ctr_printvarname(ctr, vi),
                  ctr_printequname(ctr, i));
            status = Error_Inconsistency;
            goto _end;
         }

         unsigned pos;
         double val;

         if (vtags[vi] & vchecked) {
            error("[cmat/check] ERROR: variable %s in equation %s is already "
                  "marked as checked, but appeared again in the container "
                  "matrix. Tag is %s and current container matrix element is:\n",
                  ctr_printvarname(ctr, vi), ctr_printvarname(ctr, ei),
                  vtag2str(vtags[vi]));
            cmat_elt_print(PO_ERROR, ce, ctr);
            status = Error_Inconsistency;
            goto _end;
         }

         switch (vtags[vi]) {
         case vlin: {
            if (nlinvar == 0) {
               error("[cmat/check] ERROR: in equation %s: variable %s is tagged "
                     "as linear, but the remaining number of linear variable is 0\n",
                     ctr_printequname(ctr, ei), ctr_printvarname(ctr, vi));
               goto _end;
            }

            nlinvar--;
            vtags[vi] |= vchecked;

            if (!le) { 
               error("[cmat/check] ERROR: in equation %s: variable %s is tagged "
                     "as linear, but there is no linear part\n",
                     ctr_printequname(ctr, ei), ctr_printvarname(ctr, vi));
                status = Error_Inconsistency;
                goto _end;
            }

            if (ce->isNL || ce->isQuad) {
               error("[cmat/check] ERROR: in equation %s: variable %s is "
                     "marked as nonlinear or quadratic, but could be found "
                     "in the linear equation\n", ctr_printequname(ctr, ei),
                     ctr_printvarname(ctr, vi));
               status = Error_Inconsistency;
            }

            S_CHECK(lequ_find(le, vi, &val, &pos));

            if (fabs(ce->value - val) > DBL_EPSILON) {
               error("[cmat/check] ERROR: in equation %s: linear variable %s "
                     "has coefficient %e in the container matrix, and %e in the "
                     "equation; diff = %e\n", ctr_printequname(ctr, ei),
                     ctr_printvarname(ctr, vi), ce->value, val, ce->value - val);
               status = Error_Inconsistency;
            }

            break;
         }
         case vquad: TO_IMPLEMENT("quadratic part of equations");
         case vnl:
         {
   
            if (nNLvar == 0) {
               error("[cmat/check] ERROR: in equation %s: variable %s is tagged "
                     "as nonlinear, but the remaining number of nonlinear variable is 0\n",
                     ctr_printequname(ctr, ei), ctr_printvarname(ctr, vi));
               goto _end;
            }

            nNLvar--;
            vtags[vi] |= vchecked;

             if (!ce->isNL) {
                error("[cmat/check] ERROR in equation %s: variable %s is found "
                      "in the nonlinear expression tree, but is not marked as such"
                      " in the container matrix\n", ctr_printequname(ctr, ei),
                      ctr_printvarname(ctr, vi));
                status = Error_Inconsistency;
                goto _end;
             }
             break;
         }
         case vnone:
             error("[cmat/check] ERROR in equation %s: variable %s is found in "
                   "the container matrix, but isn't found in the equation data",
                   ctr_printequname(ctr, ei), ctr_printvarname(ctr, vi));
              cmat_elt_print(PO_ERROR, ce, ctr);
             status = Error_Inconsistency;
             goto _end;

         default: error("[cmat/check] ERROR in equation %s: unexpected tag for "
                        "variable %s\n", ctr_printequname(ctr, ei),
                        ctr_printvarname(ctr, vi));
               return Error_Inconsistency;
         }

_end:
         ce = ce->next_var;
      }

      int offset;
      if (nlinvar > 0 || nNLvar > 0) {
         error("[cmat/check] %nERROR in equation %s: %u linear variables and %u "
               "nonlinear variables are absent from the container matrix\n",
               &offset, ctr_printequname(ctr, i), nlinvar, nNLvar);

         for (unsigned vi = 0; vi < total_n; ++vi) {
            if (vtags[vi] == vnone || vtags[vi] == vchecked)  {continue; }

            error("%*s variable %s\n", offset+4, vtag2str(vtags[vi]),
                  ctr_printvarname(ctr, vi));
         }

         status = Error_Inconsistency;
      }

   }

   return status;
}
