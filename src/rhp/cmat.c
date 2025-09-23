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

//#define DEBUG_CMAT

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

/**
 * @brief Check whether a container matrix element is regular
 *
 * @param cme  the container matrix element
 *
 * @return     true if the element is regular
 */
DBGUSED static bool cme_regular_elt(const CMatElt *cme)
{
   return !cme_isplaceholder(cme) && valid_vi(cme->vi) && valid_ei(cme->ei);
}


DBGUSED static bool cmat_chk_equvaridx(RhpContainerData *cdat, rhp_idx ei, rhp_idx vi)
{
   if (!cdat) { return false; }
   if (!valid_ei_(ei, cdat->total_m, __func__)) { return false; }
   if (valid_vi(vi) && !valid_vi_(vi, cdat->total_n, __func__)) { return false; }

   return (cdat->cmat.equs[ei] || rctr_eq_not_deleted(cdat, ei));
}

static CMatElt* cmat_get_equ_cme(CMat *cmat, rhp_idx ei, Container *ctr)
{
   CMatElt * restrict prev_cme = cmat->equs[ei];

   if (prev_cme && cme_isplaceholder(prev_cme)) {

      if (prev_cme->type != CMatEltCstEqu) {
         error("[container/matrix] ERROR: equation '%s' has unexpected placeholder\n",
               ctr_printequname(ctr, ei));
         assert(0);
      }

      return NULL;
   }

   return prev_cme;
}

/* \TODO(xhub) is this still necessary? */
static int push_on_deleted(RhpContainerData *cdat, CMatElt *ce)
{
   assert(ce->ei < cdat->max_m);
   if (!cdat->cmat.deleted_equs) {
      CALLOC_(cdat->cmat.deleted_equs, CMatElt *, cdat->max_m);
   }

   cdat->cmat.deleted_equs[ce->ei] = ce;

   return OK;
}


static int _debug_print_var(Container *ctr, rhp_idx vi)
{
#ifdef DEBUG_CMAT
   const char *varname = ctr_printvarname(ctr, vi);
   printout(PO_INFO, "[cmat] Variable '%s' (%d) information: ", varname, vi);

   RhpContainerData *cdat = (RhpContainerData *)ctr->data;
   const CMatElt *cme = cdat->cmat.vars[vi];
   if (!cme) {
      printout(PO_INFO, "Variable is no longer in the active model\n");
      return OK;
   }

   while (cme) {
      printout(PO_INFO, "Equ '%s'; ", ctr_printequname(ctr, cme->ei));
      cme = cme->next_equ;
   }

   puts("");
#endif

   return OK;
}

NONNULL static int _debug_print_me(Container *ctr, const CMatElt *cme)
{
#ifdef DEBUG_CMAT
   cmat_elt_print(PO_INFO, cme, ctr);
#endif

   return OK;
}

void cmat_elt_print(unsigned mode, const CMatElt *cme, Container *ctr)
{
   printout(mode, "Container matrix element for variable %s in equation %s\n",
            ctr_printvarname(ctr, cme->vi), ctr_printequname(ctr, cme->ei));

   printout(mode, "\t prev_equ = '%20s'; ",
            cme->prev_equ ? ctr_printequname(ctr, cme->prev_equ->ei) : "nil");
   printout(mode, "next_equ = '%20s'; ",
            cme->next_equ ? ctr_printequname(ctr, cme->next_equ->ei) : "nil");
   printout(mode, "next_var = '%20s'\n",
            cme->next_var ? ctr_printvarname(ctr, cme->next_var->vi) : "nil");

}

int cmat_init(CMat *cmat)
{
   cmat->equs = NULL;
   cmat->vars = NULL;
   cmat->last_equ = NULL;
   cmat->deleted_equs = NULL;

   return arenaL_init(&cmat->arena);
}

void cmat_fini(CMat *cmat)
{
   FREE(cmat->equs);
   FREE(cmat->vars);
   FREE(cmat->last_equ);
   FREE(cmat->deleted_equs);

   arenaL_free(&cmat->arena);
}

static CMatElt* cmat_elt_new(Container * restrict ctr, rhp_idx ei, rhp_idx vi, bool isNL,
                             double val)
{
   assert(valid_ei(ei));
   RhpContainerData *cdat = ctr->data; 
   CMat * restrict cmat = &cdat->cmat;
   CMatElt ** restrict cmat_vars = cmat->vars;

   CMatElt* cme;
   AA_CHECK(cme, arenaL_alloc(&cmat->arena, sizeof(CMatElt)));

   cme->value = val;
   cme->next_var = NULL;
   cme->next_equ = NULL;
   cme->ei = ei;
   cme->vi = vi;
   cme->type = isNL ? CMatEltNL : CMatEltLin;

   /* Is this the first time the variable appears? */
   if (!cmat_vars[vi]) {

      cme->prev_equ = NULL;
      cmat->vars[vi] = cme;
      ctr->n++;

      trace_ctr("[container] ADD var '%s' via equ '%s'%s", ctr_printvarname(ctr, vi),
                ctr_printequname(ctr, ei),
                ctr->vars[vi].is_deleted ? ", previously deleted\n" : "\n");

      ctr->vars[vi].is_deleted = false;

      if (ctr->varmeta) {
         /*  TODO(xhub) this should be investigated */
//         varmeta[vi].type = Varmeta_Undefined;
         ctr->varmeta[vi].ppty = VarPptyNone;
      }

   /* If there is no last_equ, then it is was a placeholder var  */
   } else if (!cmat->last_equ[vi]) {

      assert(cme_isplaceholder(cmat_vars[vi]));
      cme->prev_equ = NULL;

      cmat_vars[vi] = cme;

   } else {

      /* We show have a pointer to the equation were the variable previously appeared */
      cme->prev_equ = cmat->last_equ[vi];
      cme->prev_equ->next_equ = cme;
      /*  TODO(xhub) make an option for that */
      bool do_test = false;
#ifndef NDEBUG
      do_test = true;
#endif

      if (do_test && !cmat_chk_varnotinequ(cme->prev_equ, ei)) {
         error("[container/matrix] ERROR: variable %u already appeared in equation %u",
               vi, ei);
         return NULL;
      }
   }

   assert(!ctr->varmeta || !(ctr->varmeta[vi].ppty & VarIsDeleted));

   /* Always update last equation since we add new equation */
   cmat->last_equ[vi] = cme;

   return cme;
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
int cmat_cst_equ(CMat *cmat, rhp_idx ei)
{
   assert(valid_ei(ei));

   if (cmat->equs[ei]) {
      error("[container/matrix] ERROR: equation %u is non-empty. Please file a bug report\n",
            ei);
      return Error_RuntimeError;
   }

   CMatElt *me;
   A_CHECK(me, arenaL_alloc(&cmat->arena, sizeof(CMatElt)));

   me->value       = SNAN;
   me->type        = CMatEltCstEqu;
   me->next_var    = NULL;
   me->next_equ    = NULL;
   me->prev_equ    = NULL;
   me->ei          = ei;
   me->vi          = IdxNA;

   cmat->equs[ei] = me;

   return OK;
}

/**
 * @brief Return a special container matrix element to indicate that a variable is
 * perpendicular to an equation.
 *
 * @warning Only use this when the variable does not appear in the model anymore,
 * but there is still an inclusion relation involving this variable
 *
 * @param cmat  the container matrix
 * @param vi    the variable index
 * @param ei    the equation index
 *
 * @return    the error code
 */
int cmat_isolated_var_perp_equ(CMat *cmat, rhp_idx vi, rhp_idx ei)
{
   assert(valid_vi(vi) && valid_ei(ei));

   CMatElt *me;
   A_CHECK(me, arenaL_alloc(&cmat->arena, sizeof(CMatElt)));

   me->value  = SNAN;
   me->type = CMatEltVarPerp;
   me->next_var = NULL;
   me->next_equ = NULL;
   me->prev_equ = NULL;
   me->ei     = ei;
   me->vi     = vi;

   cmat->vars[vi] = me;
   cmat->last_equ[vi] = me;

   return OK;
}

/**
 * @brief Add a placeholder container matrix element for an objective variable not already
 * present in the container matrix;
 *
 * @warning Only call this function when the variable does not appear in any valid
 * equation
 *
 * @param cmat    the container matrix
 * @param objvar  the objective variable
 *
 * @return        the error code
 */
int cmat_objvar(CMat *cmat, rhp_idx objvar)
{
   assert(valid_vi(objvar));

   if (cmat->vars[objvar]) {
      error("[container/matrix] ERROR: variable %u is already present in the model. ", objvar);
      errormsg("Please file a bug report.\n");
      return Error_RuntimeError;
   }

   CMatElt *me;
   A_CHECK(me, arenaL_alloc_zero(&cmat->arena, sizeof(CMatElt)));

   me->value  = SNAN;
   me->type = CMatEltObjVar;
   me->next_var = NULL;
   me->next_equ = NULL;
   me->prev_equ = NULL;
   me->ei     = IdxNA;
   me->vi     = objvar;

   cmat->vars[objvar] = me;

   return OK;
}

/** @brief Add a new linear variable to an equation in the container matrix
 *
 * @ingroup CMatUnsafeUpdate
 *
 *  @param ctr   the container
 *  @param ei    the equation index
 *  @param vi     list of variables appearing in the equation
 *  @param val  the values of the variable coefficients
 *
 *  @return      the error code
 */
int cmat_equ_add_newlvar(Container *ctr, rhp_idx ei, rhp_idx vi, double val)
{
   RhpContainerData *cdat = (RhpContainerData *)ctr->data;
   assert(cmat_chk_equvaridx(cdat, ei, vi));

   CMatElt *prev_cme = cmat_get_equ_cme(&cdat->cmat, ei, ctr);

   assert(cmat_chk_varnotinequ(cdat->cmat.vars[vi], ei));

   CMatElt * restrict cme;
   A_CHECK(cme, cmat_elt_new(ctr, ei, vi, false, val));

   /*  TODO(xhub) think of placeholders */
   if (prev_cme) {
      assert(cme_regular_elt(prev_cme));
      cme->next_var = prev_cme;
   }

   cdat->cmat.equs[ei] = cme;

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
int cmat_equ_add_newlvars(Container *ctr, rhp_idx ei, const Avar *v, const double *vals)
{
   RhpContainerData *cdat = (RhpContainerData *)ctr->data;
   assert(cmat_chk_equvaridx(cdat, ei, IdxNA));

   CMatElt *prev_cme = cmat_get_equ_cme(&cdat->cmat, ei, ctr);

   /* TODO: just add all elements in front? */
   if (prev_cme) {
      while (prev_cme->next_var) {
         assert(cme_regular_elt(prev_cme));
         prev_cme = prev_cme->next_var;
      };
   }

   /* TODO: this looks wrong: what if the variable was already present*/
   /* Add all the variables from that equation  */
   for (size_t i = 0; i < v->size; ++i) {
      rhp_idx vi = avar_fget(v, i);

      S_CHECK(vi_inbounds(vi, cdat->total_n, __func__));
      assert(cmat_chk_varnotinequ(cdat->cmat.vars[vi], ei));
 
      CMatElt* cme;
      A_CHECK(cme, cmat_elt_new(ctr, ei, vi, false, vals[i]));

      /* If there is a previous variable */
      if (prev_cme) {
         prev_cme->next_var = cme;
      } else /* if not, the eqn is most likely empty */ {
         cdat->cmat.equs[ei] = cme;
      }

      /* Update for the next variable */
      prev_cme = cme;
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

   CMatElt *prev_cme = cmat_get_equ_cme(&cdat->cmat, ei, ctr);

   if (prev_cme) {
      while (prev_cme->next_var) {
         assert(cme_regular_elt(prev_cme));
         prev_cme = prev_cme->next_var;
      }
   }

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

      CMatElt *cme;
      A_CHECK(cme, cmat_elt_new(ctr, ei, vi, false, vals[i]));
      _debug_print_var(ctr, vi);

      if (prev_cme) {
         prev_cme->next_var = cme;
         _debug_print_me(ctr, prev_cme);
      } else {
          cdat->cmat.equs[ei] = cme;
      }

      /* Update for the next variable */
      prev_cme = cme;
   }
   _debug_print_me(ctr, prev_cme);

_end:
   S_CHECK(rctr_fix_equ(ctr, ei));

   return OK;
}
/** 
 *  @brief specialized function to add a linear variable to an equation without
 *  knowing if it is already present in it and in which form.
 *
 *  The main point of this function is to return the type of variable, if it
 *  was already in that equation. Then, the parent function can add this to the
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

   CMatElt *prev_cme = cmat_get_equ_cme(&cdat->cmat, ei, ctr);

   if (prev_cme) {
      do {
         assert(cme_regular_elt(prev_cme));

         /* The variable has been found and we can return  */
         if (prev_cme->vi == vi) {
            prev_cme->value += val;
            *isNL = cme_isNL(prev_cme);
            return OK;
         }

         if (!prev_cme->next_var) { break; }
         prev_cme = prev_cme->next_var;
      }
      while (true);
   }

   CMatElt *me;
   A_CHECK(me, cmat_elt_new(ctr, ei, vi, *isNL, val));

   if (prev_cme) {
      prev_cme->next_var = me;
   } else {
      cdat->cmat.equs[ei] = me;
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
 * @param jac_val  the Jacobian value for the variable
 *
 * @return         the error code
 */
int cmat_equ_add_nlvar(Container *ctr, rhp_idx ei, rhp_idx vi, double jac_val)
{
   RhpContainerData *cdat = (RhpContainerData *)ctr->data;
   assert(cmat_chk_equvaridx(cdat, ei, vi));

   /*  We need to scan from the start of the equation, since the variable
    *  may already be present */
   CMatElt *prev_cme = cmat_get_equ_cme(&cdat->cmat, ei, ctr);

#ifdef DEBUG_CMAT
   printout(PO_INFO, "[cmat] Adding variable '%s' via equation '%s', NL = %s\n",
            ctr_printvarname(ctr, vi), ctr_printequname(ctr, ei), "true");
#endif

   if (prev_cme) {
      do {

         assert(cme_regular_elt(prev_cme));

         if (prev_cme->vi == vi) {

            /* -----------------------------------------------------------
             * This variable is already in this equation
             * We just invalidate the jacobian value and check the
             * non-linear status, before moving to the next variable
             * ---------------------------------------------------------- */

            prev_cme->value += jac_val;

            if (prev_cme->type != CMatEltNL) {

               assert(prev_cme->type == CMatEltLin || prev_cme->type == CMatEltQuad);

               prev_cme->type = CMatEltNL;
               /*  TODO(xhub) figure out whether this is correct
                   prev_e->value = NAN; */

               /* -------------------------------------------------------
                * We need to move the variable from the linear part to the
                * non-linear one
                * ------------------------------------------------------- */
               S_CHECK(equ_switch_var_nl(ctr, &ctr->equs[ei], vi));
            }

            return OK;
         }

         if (!prev_cme->next_var) { break; }
         prev_cme = prev_cme->next_var;

      } while (true);
   }

   CMatElt *cme;
   A_CHECK(cme, cmat_elt_new(ctr, ei, vi, true, jac_val));

   if (prev_cme) {
      prev_cme->next_var = cme;
   } else {
      cdat->cmat.equs[ei] = cme;
   }

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
   
   CMatElt * restrict prev_cme = cmat_get_equ_cme(&cdat->cmat, ei, ctr);

   for (unsigned i = 0, len = v->size; i < len; ++i) {

      /* -------------------------------------------------------------------
       *
       *  We need to scan from the start of the equation, since the variable
       *  may already be present
       *
       * ------------------------------------------------------------------- */

      rhp_idx vi = avar_fget(v, i);
      if (vi == vi_no) continue;

       if (!valid_vi(vi) || (unsigned)vi >= cdat->total_n) {
         error("%s :: index %d >= %zu\n", __func__, vi, cdat->total_n);
         return Error_IndexOutOfRange;
      }

#ifdef DEBUG_CMAT
      DPRINT("[cmat] Adding variable '%s' to the model, NL = %s\n",
             ctr_printvarname(ctr, vi), vi, isNL ? "true" : "false");
#endif

      if (prev_cme) {
         do {
            assert(cme_regular_elt(prev_cme));

            if (prev_cme->vi == vi) {
               /* This variable is already in this equation
                * We just invalidate the jacobian value and check the
                * non-linear status, before moving to the next variable*/
               prev_cme->value += values ? values[i] : SNAN;
               if (prev_cme->type != CMatEltNL && isNL) {
                  assert(prev_cme->type == CMatEltLin || prev_cme->type == CMatEltQuad);
                  prev_cme->type = CMatEltNL;
                  /*  TODO(xhub) figure out whether this is correct
                  prev_e->value = NAN; */
                  /*  We need to move the variable from the linear part to the
                   *  non-linear one*/
                  S_CHECK(equ_switch_var_nl(ctr, &ctr->equs[ei], vi));
               }
               goto _end_loop;
            }

            if (!prev_cme->next_var) { break; }
            prev_cme = prev_cme->next_var;
         }
         while (true);
      }

      double val;
      if (values) { val = values[i]; } else { val = SNAN; }

      CMatElt *e;
      A_CHECK(e, cmat_elt_new(ctr, ei, vi, isNL, val));

      if (prev_cme) {
         prev_cme->next_var = e;
      } else {
          cdat->cmat.equs[ei] = e;
      }

_end_loop: ;

      /* Reset prev_cme as the presence of the next variable in the equation must be searched */
      prev_cme = cdat->cmat.equs[ei];

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

   CMatElt * restrict prev_cme = cmat_get_equ_cme(&cdat->cmat, ei, ctr);

   for (unsigned i = 0, len = v->size; i < len; ++i) {
      /*  We need to scan from the start of the equation, since the variable
       *  may already be present */

      /* XXX this is suboptimal --xhub  */
      rhp_idx vi = avar_fget(v, i);

      S_CHECK(vi_inbounds(vi, cdat->total_n, __func__));

#ifdef DEBUG_CMAT
      printout(PO_INFO, "[cmat] Adding variable '%s' via equation '%s', NL = %s\n",
             ctr_printvarname(ctr, vi), ctr_printequname(ctr, ei), isNL ? "true" : "false");
#endif

      if (prev_cme) {
         do {
            assert(cme_regular_elt(prev_cme));

            if (RHP_UNLIKELY(prev_cme->vi == vi)) {

               /* This variable is already in this equation
                * We just invalidate the Jacobian value and check the
                * non-linear status, before moving to the next variable*/

               prev_cme->value += values ? values[i] : SNAN;

               if (prev_cme->type != CMatEltNL && isNL) {
                  assert(prev_cme->type == CMatEltLin || prev_cme->type == CMatEltQuad);
                  prev_cme->type = CMatEltNL;
                  /*  TODO(xhub) figure out whether this is correct
                  prev_e->value = NAN; */
                  /*  We need to move the variable from the linear part to the
                   *  non-linear one*/
                  S_CHECK(equ_switch_var_nl(ctr, &ctr->equs[ei], vi));
               }

               goto _end_loop;
            }

            if (!prev_cme->next_var) { break; }

            prev_cme = prev_cme->next_var;

         } while (true);
      }

      double val = values ? values[i] : SNAN;

      CMatElt *cme;
      A_CHECK(cme, cmat_elt_new(ctr, ei, vi, isNL, val));

      if (prev_cme) {
         prev_cme->next_var = cme;
      } else {
          cdat->cmat.equs[ei] = cme;
      }

_end_loop: ;

      /* Reset prev_cme as the presence of the next variable in the equation must be searched */
      prev_cme = cdat->cmat.equs[ei];

   }

   return OK;
}

/** @brief remove a variable from an equation in the container matrix
 *
 *  @warning    it is not advised to use this function directly. Use equ_rm_var() instead
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
   CMat * restrict cmat = &cdat->cmat;

   CMatElt *cme = cmat->equs[ei];
   CMatElt *prev_cme = NULL;

   if (!cme) {
      error("[container/matrix] ERROR: equation '%s' is empty!\n",  ctr_printequname(ctr, ei));
      return Error_NullPointer;
   }

   trace_ctr("[container] DEL var '%s' from equ '%s'\n",
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
      if (cme->vi == vi) {

         /* -----------------------------------------------------------------
          * If there is a previous equation where this variable appear, replace
          * the link to this variable to the next one
          * ----------------------------------------------------------------- */

         if (prev_cme) {

            assert(cmat->equs[ei] != cme);
            prev_cme->next_var = cme->next_var;

         } else {

            if (cme->next_var) {
               cmat->equs[ei] = cme->next_var;
            } else {
               /*  EQU2 */
               double cst = equ_get_cst(&ctr->equs[ei]);
               if (ctr->equs[ei].object == Mapping && isfinite(cst)) {
                  S_CHECK(cmat_cst_equ(cmat, ei));
               } else {
                  error("[container] ERROR: the equation '%s' no longer contains any "
                        "variable and has a CST of %e\n", ctr_printequname(ctr, ei), cst);
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

         if (cme->next_equ) {

            cme->next_equ->prev_equ = cme->prev_equ;

         } else {

            assert(cdat->cmat.last_equ[vi] == cme);
            cmat->last_equ[vi] = cme->prev_equ;
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

         if (cme->prev_equ) {

            cme->prev_equ->next_equ = cme->next_equ;

         } else {

            cmat->vars[vi] = cme->next_equ;

            if (!cme->next_equ) {

               assert(!cmat->last_equ[vi]);

               ctr->n--;
               ctr->vars[vi].is_deleted = true;
               if (ctr->varmeta) {
                   ctr->varmeta[vi].ppty |= VarIsDeleted;
               }

            trace_ctr("[container] %14s var '%s' deleted\n", "->", ctr_printvarname(ctr, vi));
            }
         }

         break;
      }

      prev_cme = cme;
      cme = cme->next_var;

      if (!cme) {
         error("[container] ERROR: variable '%s' does not appear in equation '%s'\n",
               ctr_printvarname(ctr, vi), ctr_printequname(ctr, ei));
         return Error_Inconsistency;
      }
   }

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

   if (RHP_UNLIKELY(cdat->cmat.equs[ei])) {
      error("[container] ERROR: cannot fill non-empty equation #%u\n", ei);
      return Error_RuntimeError;
   }

   CMatElt* prev_cme = NULL;

   /* Add all the variables from that equation */
   for (unsigned i = 0, total_n = cdat->total_n, len = v->size; i < len; ++i) {

      rhp_idx vi = avar_fget(v, i);
      S_CHECK(vi_inbounds(vi, total_n, __func__));
 
      CMatElt* cme;
      A_CHECK(cme, cmat_elt_new(ctr, ei, vi, nlflags[i], values[i]));

      /* If there is a previous variable */
      if (prev_cme) {
         assert(cme_regular_elt(prev_cme));
         prev_cme->next_var = cme;
      } else /* if not, the equation is most likely empty */ {
         cdat->cmat.equs[ei] = cme;
      }

      /* Update for the next variable */
      prev_cme = cme;
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
   CMat * restrict cmat = &cdat->cmat;

   CMatElt * restrict cme = cdat->cmat.equs[ei];

   if (!cme) {
      error("[container/matrix] ERROR: cannot remove equation '%s', it is already inactive\n",
            ctr_printequname(ctr, ei));
      return Error_NullPointer;
   }

   /* Clear from the model */
   cdat->cmat.equs[ei] = NULL;

   /* Add the equation to deleted_equs  */
   assert(cme->ei == ei);
   S_CHECK(push_on_deleted(cdat, cme));

   /* Remove the equation from the model*/
   while (cme) {

      /* Update last_equ if necessary */
      if (cme->next_equ) {
         cme->next_equ->prev_equ = cme->prev_equ;
      } else {
         assert(cme == cdat->cmat.last_equ[cme->vi]);
         cmat->last_equ[cme->vi] = cme->prev_equ;
      }

      /* If the variable appeared in a previous equ */
      if (cme->prev_equ) {
         cme->prev_equ->next_equ = cme->next_equ;
      } else { /* We are deleting the first equation where the variable appears */
         /* set model->vars  */
         rhp_idx vi = cme->vi;

         assert(!cmat->vars[vi] || !cme_isplaceholder(cmat->vars[vi]));
         cmat->vars[vi] = cme->next_equ;

         /*  If this is true, the variable disappear from the container */
         if (!cme->next_equ) {

            assert(!cmat->last_equ[vi]);

            ctr->n--;
            ctr->vars[vi].is_deleted = true;
            if (ctr->varmeta) {
               ctr->varmeta[vi].ppty |= VarIsDeleted;
            }

            trace_ctr(" %14s var '%s'\n", "-> DEL", ctr_printvarname(ctr, vi));

         }
      }

      _debug_print_var(ctr, cme->vi);

      /* Update the next variable */
      cme = cme->next_var;
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
   const CMatElt * restrict cme_src = cdat->cmat.equs[ei_src];
   CMatElt * restrict prev_cme = NULL;

   S_CHECK(ei_inbounds(ei_src, ctr_nequs_total(ctr), __func__));
   S_CHECK(ei_inbounds(ei_dst, ctr_nequs_total(ctr), __func__));

   if (cdat->cmat.equs[ei_dst]) {
      error("[container] ERROR: cannot copy '%s' into non-empty equation #%u\n",
            ctr_printequname(ctr, ei_src), ei_dst);
      return Error_RuntimeError;
   }

   /* Add all the variables from that equation  */
   while (cme_src) {
      assert(cme_regular_elt(cme_src));

      CMatElt *cme_dst;
      A_CHECK(cme_dst, cmat_elt_new(ctr, ei_dst, cme_src->vi, cme_isNL(cme_src),
                                    -cme_src->value));

      /* If there is a previous variable */
      if (prev_cme) {
         assert(cme_regular_elt(prev_cme));
         prev_cme->next_var = cme_dst;
      } else /* if not, the eqn is most likely empty */ {
         cdat->cmat.equs[ei_dst] = cme_dst;
      }

      /* Update for the next variable */
      prev_cme = cme_dst;

      /* Go to the next variable in the equation */
      cme_src = cme_src->next_var;
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
 *
 * @return         the error code
 */
int cmat_copy_equ(Container *ctr, rhp_idx ei_src, rhp_idx ei_dst)
{
   RhpContainerData *cdat = ctr->data;

   S_CHECK(ei_inbounds(ei_src, ctr_nequs_total(ctr), __func__));
   S_CHECK(ei_inbounds(ei_dst, ctr_nequs_total(ctr), __func__));

   const CMatElt * restrict cme_src = cdat->cmat.equs[ei_src];
   assert(cme_src);
   CMatElt * restrict prev_cme = NULL;

   if (cdat->cmat.equs[ei_dst]) {
      error("[container] ERROR: cannot copy '%s' into non-empty equation #%u\n",
            ctr_printequname(ctr, ei_src), ei_dst);
      return Error_RuntimeError;
   }

   /* Add all the variables from that equation  */
   while (cme_src) {

      CMatElt *me_dst;
      A_CHECK(me_dst, cmat_elt_new(ctr, ei_dst, cme_src->vi, cme_isNL(cme_src),
                                   cme_src->value));

      /* If there is a previous variable */
      if (prev_cme) {
         assert(cme_regular_elt(prev_cme));
         prev_cme->next_var = me_dst;
      } else /* if not, the equation is most likely empty */ {
         cdat->cmat.equs[ei_dst] = me_dst;
      }

      /* Update for the next variable */
      prev_cme = me_dst;

      /* Go to the next variable in the equation */
      cme_src = cme_src->next_var;
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
   CMat *cmat = &cdat->cmat;

   S_CHECK(ei_inbounds(ei_src, ctr_nequs_total(ctr), __func__));
   S_CHECK(ei_inbounds(ei_dst, ctr_nequs_total(ctr), __func__));

   const CMatElt * restrict cme_src = cmat->equs[ei_src];
   assert(cme_src);
   CMatElt * restrict prev_cme = NULL;

   if (cmat->equs[ei_dst]) {
      error("[container] ERROR: cannot copy '%s' into non-empty equation #%u\n",
            ctr_printequname(ctr, ei_src), ei_dst);
      return Error_RuntimeError;
   }

   /* Add all the variables from that equation  */
   while (cme_src) {

      if (cme_src->vi == vi_no) { cme_src = cme_src->next_var; continue; }

      CMatElt *cme_dst;
      A_CHECK(cme_dst, cmat_elt_new(ctr, ei_dst, cme_src->vi, cme_isNL(cme_src),
                                    cme_src->value));

      /* If there is a previous variable */
      if (prev_cme) {
         assert(cme_regular_elt(prev_cme));
         prev_cme->next_var = cme_dst;
      } else /* if not, the equation is most likely empty */ {
         cmat->equs[ei_dst] = cme_dst;
      }

      /* Update for the next variable */
      prev_cme = cme_dst;

      /* Go to the next variable in the equation */
      cme_src = cme_src->next_var;
   }

   /* It can happen that the new equation is empty. Use a placeholder then */
   if (!cmat->equs[ei_dst]) {
      S_CHECK(cmat_cst_equ(cmat, ei_dst));
   }

   return OK;
}

/**
 * @brief Append equations from a container matrix to another container one
 *
 * @param ctr_dst        the destination container
 * @param ctr_src        the source container
 * @param e              the equation indices to add
 * @param ei_dst_start   the offset at which to add the equation indices
 *
 * @return                the error code
 */
int cmat_append_equs(Container * restrict ctr_dst, const Container * restrict ctr_src,
                     const Aequ * restrict e, rhp_idx ei_dst_start)
{
   RhpContainerData *cdat_dst = ctr_dst->data;
   RhpContainerData *cdat_src = ctr_src->data;

   rhp_idx ei_dst = ei_dst_start;

   unsigned len = e->size;
   cdat_dst->total_m += len;
   ctr_dst->m += len;

   CMatElt * restrict * restrict cmat_dst_equs = cdat_dst->cmat.equs;

   for (unsigned i = 0; i < len; ++i) {

      rhp_idx ei_src = aequ_fget(e, i);
      assert(!valid_ei(ctr_getcurrent_ei(ctr_src, ei_src)) ||
             ei_dst == ctr_getcurrent_ei(ctr_src, ei_src));

      const CMatElt * restrict cme_src = cdat_src->cmat.equs[ei_src];
      /* TODO GITLAB #110 */
      if (!cme_src) { continue; }
      CMatElt * restrict prev_cme = NULL;

      if (cmat_dst_equs[ei_dst]) {
         error("[container] ERROR: cannot copy '%s' into non-empty equation #%u\n",
               ctr_printequname(ctr_src, ei_src), ei_dst);
         return Error_RuntimeError;
      }

      /* Add all the variables from that equation  */
      while (cme_src) {

         rhp_idx vi_dst = ctr_getcurrent_vi(ctr_src, cme_src->vi);
         assert(valid_vi_(vi_dst, cdat_dst->total_n, __func__));


         CMatElt *me_dst;
         A_CHECK(me_dst, cmat_elt_new(ctr_dst, ei_dst, vi_dst, cme_isNL(cme_src), cme_src->value));

         /* If there is a previous variable */
         if (prev_cme) {
            prev_cme->next_var = me_dst;
         } else /* if not, the equation is most likely empty */ {
            cmat_dst_equs[ei_dst] = me_dst;
         }

         /* Update for the next variable */
         prev_cme = me_dst;

         /* Go to the next variable in the equation */
         cme_src = cme_src->next_var;
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

   CMatElt * restrict me = cdat->cmat.equs[ei]; assert(me);

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
   case vlin: return "linear";
   case vquad: return "quadratic";
   case vnl: return "nonlinear";
   default: return "unknown tag ERROR";
   }
}

NONNULL static int cmat_chk_lequ(Container *ctr, RhpContainerData *cdat, rhp_idx ei,
                                 enum vtag *vtags) 
{
   int status = OK;

   Lequ *le = ctr->equs[ei].lequ;

   if (!le) return status;

   rhp_idx * restrict vidx = le->vis;
   double * restrict vals = le->coeffs;
   rhp_idx total_n = cdat->total_n;

   for (size_t j = 0, len = le->len; j < len; ++j) {
      double val = vals[j];
      rhp_idx vi = vidx[j];

      if (!isfinite(val)) {
         if (isnan(val) && vi < total_n && cdat->cmat.vars[vi]) {
            error("[cmat/check] ERROR: variable '%s' has value %E in equation '%s', "
                  "but is marked as active\n", ctr_printvarname(ctr, vi), val,
                  ctr_printequname(ctr, ei));
            status = Error_Inconsistency;

         } else if (!isnan(val)) {
            error("[cmat/check] ERROR: variable '%s' has value %E in equation '%s', "
                  "which is inconsistent,\n", ctr_printvarname(ctr, vi), val,
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

   trace_ctr("[container] Checking reshop container.\n");

   int status = OK;
   RhpContainerData *restrict cdat = ctr->data;
   rhp_idx total_n = cdat->total_n, total_m = cdat->total_m;

   struct ctrmem CTRMEM working_mem = {.ptr = NULL, .ctr = ctr};
   A_CHECK(working_mem.ptr, ctr_getmem_old(ctr, total_n * sizeof(enum vtag)));
   enum vtag *vtags = working_mem.ptr;

   struct equ_meta * restrict emd = ctr->equmeta;

   for (rhp_idx i = 0, len = total_m; i < len; ++i)
   {
      CMatElt *cme = cdat->cmat.equs[i];

      if (cme && cme_isplaceholder(cme)) {
         if (valid_vi(cme->vi) || !valid_ei(cme->ei) || cme->ei != i || cme->next_var || cme->next_equ || cme->prev_equ) {
            error("[cmat/check] ERROR: placeholder for equation %s is invalid: "
                  "vi: %u; ei: %u; next_var: %p; prev_equ: %p; next_equ: %p\n",
                  ctr_printequname(ctr, i), cme->vi, cme->ei, (void*)cme->next_var,
                  (void*)cme->prev_equ, (void*)cme->next_equ);

            status = Error_Inconsistency;
         }
         continue;
      }

      if (!cme) {
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
      if (status != OK) { continue; }

      /*TODO QUAD*/
      assert(cme->type != CMatEltQuad);

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

      while (cme) {

         rhp_idx ei = cme->ei, vi = cme->vi;

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

         if (!cdat->cmat.vars[vi]) {
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
            cmat_elt_print(PO_ERROR, cme, ctr);
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

            if (cme->type != CMatEltLin) {
               error("[cmat/check] ERROR: in equation %s: variable %s is "
                     "marked as nonlinear or quadratic, but could be found "
                     "in the linear equation\n", ctr_printequname(ctr, ei),
                     ctr_printvarname(ctr, vi));
               status = Error_Inconsistency;
            }

            S_CHECK(lequ_find(le, vi, &val, &pos));

            if (fabs(cme->value - val) > DBL_EPSILON) {
               error("[cmat/check] ERROR: in equation %s: linear variable %s "
                     "has coefficient %e in the container matrix, and %e in the "
                     "equation; diff = %e\n", ctr_printequname(ctr, ei),
                     ctr_printvarname(ctr, vi), cme->value, val, cme->value - val);
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

             if (!cme_isNL(cme)) {
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
                   "the container matrix, but isn't found in the equation data\n",
                   ctr_printequname(ctr, ei), ctr_printvarname(ctr, vi));
              cmat_elt_print(PO_ERROR, cme, ctr);
             status = Error_Inconsistency;
             goto _end;

         default: error("[cmat/check] ERROR in equation %s: unexpected tag for "
                        "variable %s\n", ctr_printequname(ctr, ei),
                        ctr_printvarname(ctr, vi));
               return Error_Inconsistency;
         }

_end:
         cme = cme->next_var;
      }

      int offset;
      bool missing_linvar = nlinvar > 0;
      bool missing_nlvar  = nNLvar  > 0;
      if (missing_linvar || missing_nlvar) {
         error("[cmat/check] %nERROR in equation %s: ", &offset, ctr_printequname(ctr, i));
         if (missing_linvar) {
            error("%u linear variables ", nlinvar);
            if (missing_nlvar) { errormsg(" and "); }
            else { errormsg("are absent from the container matrix\n"); }
         }
         if (missing_nlvar) {
            error("%u nonlinear variables are absent from the container matrix\n",
                  nNLvar);
         }

         for (unsigned vi = 0; vi < total_n; ++vi) {
            if (vtags[vi] == vnone || vtags[vi] & vchecked)  { continue; }

            error("%*s variable %s\n", offset+4, vtag2str(vtags[vi]),
                  ctr_printvarname(ctr, vi));
         }

         status = Error_Inconsistency;
      }

   }

   return status;
}
