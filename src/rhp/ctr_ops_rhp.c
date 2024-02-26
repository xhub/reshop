#include "reshop_config.h"
#include "asnan.h"

#include <fcntl.h>
#include <float.h>
#include <math.h>
#include <stdbool.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>

#include "checks.h"
#include "cmat.h"
#include "container.h"
#include "container_helpers.h"
#include "container_ops.h"
#include "ctr_rhp.h"
#include "ctrdat_rhp.h"
#include "ctrdat_rhp_priv.h"
#include "equ.h"
#include "nltree.h"
#include "equvar_metadata.h"
#include "equvar_helpers.h"
#include "internal_model_common.h"
#include "lequ.h"
#include "macros.h"
#include "pool.h"
#include "printout.h"
#include "status.h"

#include "itostr.h"
//#define EXPORT_MODEL_DEBUG

#ifndef NDEBUG
#define RCTR_DEBUG(str, ...) trace_fooc("[rctr] DEBUG: " str "\n", __VA_ARGS__)
#else
#define RCTR_DEBUG(...)
#endif


/* ------------------------------------------------------------------------
 * Locally defined functions used in other static functions.
 * ------------------------------------------------------------------------ */

static int rctr_notimpl_(Container *ctr)
{
   error("%s :: missing function!\n", __func__);
   return Error_NotImplemented;
}

static int rctr_notimpl2_(const Container *ctr, double* unused)
{
   error("%s :: missing function!\n", __func__);
   return Error_NotImplemented;
}

static int rctr_notimpl3_(const Container *ctr, int idx, int *value)
{
  error("%s :: missing function!\n", __func__);
  return Error_NotImplemented;
}

static int rctr_notimpl3char_(const Container *ctr, const char *name, int *value)
{
  error("%s :: missing function!\n", __func__);
  return Error_NotImplemented;
}


/*  \TODO(xhub) this should return the status */
/* \TODO(xhub) implement an estimate of the number of equation needed */
static int rctr_allocdata(Container *ctr)
{
   if (!ctr->pool) {
      A_CHECK(ctr->pool, pool_new_gams());
   }

   return cdat_alloc(ctr, ctr->n, ctr->m);
}

static void rctr_deallocdata(Container *ctr)
{
   struct ctrdata_rhp *model = (struct ctrdata_rhp *) ctr->data;

   cdat_dealloc(ctr, model);
   ctr->data = NULL;
}


/**
 *  @brief comparison function for sorting the equations to evaluate
 *
 *  @param a  the first equation
 *  @param b  the second equation
 *
 *  @return   > 0 if a > b
 */
static int compar_equvar_pair(const void *a, const void *b)
{
   struct equvar_pair *evp1 = (struct equvar_pair *)a;
   struct equvar_pair *evp2 = (struct equvar_pair *)b;

   return evp1->cost - evp2->cost;
}

/**
 *  @brief Sort the equations to evaluate variables
 *
 *         The idea is to first evaluate the equation that can directly
 *         determine variables, and then report those values in for 
 *
 *  @param ctr  the container
 *
 *  @return     the error code
 */
static int cdat_sort_eval_equvar_(Container *ctr)
{
   RhpContainerData * restrict cdat = (RhpContainerData *)ctr->data;

   rhp_idx  * restrict eis  = NULL;
   rhp_idx  * restrict vis  = NULL;
   unsigned * restrict toeval = NULL;
   unsigned len_v = 0;
   const size_t total_n = cdat->total_n;
   struct ctrmem CTRMEM working_mem = {.ptr = NULL, .ctr = ctr};

   assert(total_n > 0);

   for (unsigned i = 0; i <= cdat->current_stage; ++i) {
      struct equvar_eval * restrict equvar_evals = &cdat->equvar_evals[i];
      struct equvar_pair * restrict pairs = equvar_evals->pairs;
      const unsigned len = equvar_evals->len;

      /* -------------------------------------------------------------------
       * Do we have something to sort?
       * ------------------------------------------------------------------- */

      if (len == 0) {
         continue;
      }

      /* -------------------------------------------------------------------
       * Ensure that we have enough space
       * ------------------------------------------------------------------- */

      if (len_v < len) {
         len_v = len;

         if (working_mem.ptr) {
            ctr_relmem(ctr);
         }

         A_CHECK(working_mem.ptr, ctr_getmem(ctr, sizeof(unsigned) * (len_v + len_v + total_n)));
         toeval = (unsigned*)working_mem.ptr;
         eis = (rhp_idx*)&toeval[total_n];
         vis = &eis[len_v];
      }

      assert(toeval && eis && vis); 
      memset(toeval, 0, total_n * sizeof(unsigned));

      /* -------------------------------------------------------------------
       * Init data (cost and more)
       * ------------------------------------------------------------------- */

      RCTR_DEBUG("Sorting %u equvar pairs", len);

      for (unsigned j = 0; j < len; ++j) {
         rhp_idx vi = pairs[j].var;
         rhp_idx ei = pairs[j].equ;
         assert(valid_vi(vi) && vi < total_n);
         eis[j] = ei;
         vis[j] = vi;
         toeval[vi] = j+1;
         pairs[j].cost = 1;
         RCTR_DEBUG("(%s, %s)", ctr_printvarname(ctr, vis[j]), ctr_printequname(ctr, eis[j]));
      }

      /* -------------------------------------------------------------------
       * Loop until there is no more variables to eval or we cannot resolve the
       * dependencies (number of variable left is stagnant)
       * ------------------------------------------------------------------- */

      unsigned old_len_v;
      do {
         size_t j = 0;
         for (unsigned k = 0; k < len_v; ++k) {
            void* jacptr = NULL;;
            rhp_idx ei = eis[k];
            rhp_idx vi = vis[k];

            assert(toeval[vi] > 0);
            struct equvar_pair *pair = &pairs[toeval[vi]-1];
            assert(pair->equ == ei && pair->var == vi);
            bool keep_var = false;

            /* -------------------------------------------------------------
             * Loop over all the variables in the equation, except the one
             * defined by this equation.
             *
             * If in that equation, a variable has to be evaluated,
             * then we add the cost of the variable, and we keep the variable.
             *
             * model_walkallequ is used instead of ctr_getrowjacinfo, since the
             * latter does not take into account the deleted equations
             * ------------------------------------------------------------- */

            do {

               double jacval;
               bool nlflag;
               rhp_idx vid;
               S_CHECK(rctr_walkallequ(ctr, ei, &jacptr, &jacval, &vid, &nlflag));

               /* ----------------------------------------------------------
                * If a variable present in the equation needs to be also
                * evaluated, we save the couple (equ,var) for the next round.
                *
                * This also means that vi can't be evaluated using that
                * expression. Hence, keep_var is set to true.
                *
                * In nay case, the cost of evaluating vidx increases by the
                * cost of the other variable in the equation.
                * ---------------------------------------------------------- */

               if (vi != vid) {

                  if (toeval[vid] > 0) {
                     RCTR_DEBUG("VAR %s appears in %s", ctr_printvarname(ctr, vid),
                                ctr_printequname(ctr, ei));

                     if (!keep_var) {
                        eis[j] = ei;
                        vis[j] = vi;
                        j++;
                        keep_var = true;
                     }

                     pair->cost += pairs[toeval[vid]-1].cost;
                     RCTR_DEBUG("(%s,%s) has cost %u", ctr_printvarname(ctr, vi),
                                ctr_printequname(ctr, ei), pair->cost);
                  }
               }

            } while(jacptr);

            /* -------------------------------------------------------------
             * If a variable to evaluate is present in the equation, store this
             * information
             * ------------------------------------------------------------- */

            if (!keep_var) {
               RCTR_DEBUG("deleting VAR %s from evaluation loop",
                          ctr_printvarname(ctr, vi));
               toeval[vi] = 0;
            }

         }

         old_len_v = len_v;
         len_v = j;

      } while (len_v > 0 || len_v < old_len_v);

      if (len_v > 0 && len_v == old_len_v) {
         error("%s :: stalling: there must be a dependency between the variables"
               "that have to be evaluated\n", __func__);
         continue;
      }

#ifndef NDEBUG
      RCTR_DEBUG("Final costs %s", "");
      for (unsigned j = 0; j < len; ++j) {
         rhp_idx vi = pairs[j].var;
         rhp_idx ei = pairs[j].equ;
         unsigned cost = pairs[j].cost;
         RCTR_DEBUG("(%s, %s) has cost %u", ctr_printvarname(ctr, vi),
                    ctr_printequname(ctr, ei), cost);
      }
#endif

      /* -------------------------------------------------------------------
       * Perform the sorting using the cost
       * ------------------------------------------------------------------- */

      qsort(pairs, len, sizeof(pairs[0]), &compar_equvar_pair);

#ifndef NDEBUG
      RCTR_DEBUG("Final sorting order%s", "");
      for (unsigned j = 0; j < len; ++j) {
         rhp_idx vi = pairs[j].var;
         rhp_idx ei = pairs[j].equ;
         unsigned cost = pairs[j].cost;
         RCTR_DEBUG("(%s, %s) with cost = %u", ctr_printvarname(ctr, vi),
                    ctr_printequname(ctr, ei), cost);
      }
#endif
   }

   return OK;

}

static int rctr_evalequvar(Container *ctr)
{
   RhpContainerData *cdat = (RhpContainerData *)ctr->data;

   /* ----------------------------------------------------------------------
    * This function has the following phases:
    *
    * 1. Figure out the order to evaluate the equations
    * 2. Perform the evaluation, starting from the latest stage back to the
    * first one
    *
    * WARNING: a prerequisite to calling this function is to report the values!
    * ---------------------------------------------------------------------- */

   S_CHECK(cdat_sort_eval_equvar_(ctr));

   rhp_idx nvars_total = cdat->total_n;
   rhp_idx nequs_total = cdat->total_m;

   for (unsigned cur_stage = cdat->current_stage, k = cur_stage; k <= cur_stage; --k) {
      struct equvar_eval * restrict equvar_evals = &cdat->equvar_evals[k];

      unsigned len = equvar_evals->len;
      trace_solreport("[solreport] stage %u, evaluating %u variables\n", k, len);
               //mdl_getnamelen2(ctr), mdl_getname2(ctr),

      for (unsigned j = 0; j < len; ++j) {
         rhp_idx vi = equvar_evals->pairs[j].var;
         rhp_idx ei = equvar_evals->pairs[j].equ;
         S_CHECK(vi_inbounds(vi, nvars_total, __func__));
         S_CHECK(ei_inbounds(ei, nequs_total, __func__));

         Var * restrict v = &ctr->vars[vi];
         Equ * restrict e = &ctr->equs[ei];
         Lequ * restrict lequ = e->lequ;
         Var * restrict vars = ctr->vars;
         /* Multiplying is usually faster than dividing */
         double coeff_inv = NAN;

         v->value = 0.;

         for (unsigned l = 0; l < lequ->len; ++l) {
            rhp_idx idx = lequ->vis[l];
            if (idx != vi) {
               if (isfinite(lequ->coeffs[l])) {
                  assert(isfinite(vars[idx].value));
                  v->value += lequ->coeffs[l] * vars[idx].value;
               }
            } else {
               assert(fabs(lequ->coeffs[l]) > DBL_EPSILON);
               coeff_inv = 1./lequ->coeffs[l];
            }
         }

         if (!isfinite(coeff_inv)) {
            error("%s ERROR: the coefficient on the variable '%s' in equation "
                  "'%s' is not finite: %e\n\n", __func__,
                  ctr_printvarname(ctr, vi), ctr_printequname(ctr, ei), coeff_inv);
            lequ_print(lequ, PO_ERROR);
            return Error_InvalidValue;
         }

         double nlval;
         S_CHECK(rctr_getnl(ctr, e))

         if (e->tree && e->tree->root) {
            /* I sense some umin fishyness --xhub */
            S_CHECK(nltree_eval(ctr, e->tree, &nlval));
            assert(isfinite(nlval));

            DPRINT("nlval = %e; prev level %e; new level %e\n", nlval, v->level,
               - nlval - v->level);

            v->value = - nlval - v->value;
         } else {
            v->value = - v->value;
         }

         double cst = equ_get_cst(e);
         DPRINT("cst = %e; prev level %e; coeff_inv = %e\n", cst, v->level,
               coeff_inv);

         v->value = (v->value - cst)*coeff_inv;

         DPRINT("EVAL: variable %s (%d), equation %s (%d) : %e\n",
                 ctr_printvarname(ctr, vidx), vidx,
                 ctr_printequname(ctr, eidx), eidx, v->level);

         if (!isfinite(v->value)) {
            error("%s ERROR: evaluation of variable '%s' via equation '%s' "
                  "yields %e\n", __func__, ctr_printvarname(ctr, vi),
                  ctr_printequname(ctr, ei), v->value);
            equ_print(e);
         } else if (O_Output & PO_TRACE_SOLREPORT) {
            printout(PO_TRACE_SOLREPORT, "[solreport] variable %s set to %e\n",
                      ctr_printvarname(ctr, vi), v->value);
         }
      }
   }

   return OK;
}

static int rctr_getequsbasis(const Container *ctr, Aequ *e, int *basis)
{
   assert(e && basis);
   Equ *equs = ctr->equs;
   size_t total_m = rctr_totalm(ctr);
   for (size_t i = 0; i < e->size; ++i) {
      rhp_idx ei = aequ_fget(e, i);
      assert(valid_ei(ei));
      S_CHECK(ei_inbounds(ei, total_m, __func__));
      basis[i] = equs[ei].basis;
   }
   return OK;
}

static int rctr_getequsmult(const Container *ctr, Aequ *e, double *mult)
{
   assert(e && mult);
   Equ *equs = ctr->equs;
   size_t total_m = rctr_totalm(ctr);

   for (size_t i = 0; i < e->size; ++i) {
      rhp_idx ei = aequ_fget(e, i);
      assert(valid_ei(ei));
      S_CHECK(ei_inbounds(ei, total_m, __func__));
      mult[i] = equs[ei].multiplier;
   }
   return OK;
}

static int rctr_getequsval(const Container *ctr, Aequ *e, double *vals)
{
   assert(e && vals);
   Equ *equs = ctr->equs;
   size_t total_m = rctr_totalm(ctr);

   for (size_t i = 0; i < e->size; ++i) {
      rhp_idx ei = aequ_fget(e, i);
      assert(valid_ei(ei));
      ei_inbounds(ei, total_m, __func__);
      vals[i] = equs[ei].value;
   }

   return OK;
}

static int rctr_getvarsbasis(const Container *ctr, Avar *v, int *basis)
{
   assert(v && basis);
   Var *vars = ctr->vars;
   size_t total_n = rctr_totaln(ctr);
   for (size_t i = 0; i < v->size; ++i) {
      rhp_idx vi = avar_fget(v, i);
      assert(valid_vi(vi));
      vi_inbounds(vi, total_n, __func__);
      basis[i] = vars[vi].basis;
   }

   return OK;
}

static int rctr_getvarsmult(const Container *ctr, Avar *v, double *mult)
{
   assert(v && mult);
   Var *vars = ctr->vars;
   size_t total_n = rctr_totaln(ctr);
   for (size_t i = 0; i < v->size; ++i) {
      rhp_idx vi = avar_fget(v, i);
      assert(valid_vi(vi));
      vi_inbounds(vi, total_n, __func__);
      mult[i] = vars[vi].multiplier;
   }
   return OK;
}

static int rctr_getvarsval(const Container *ctr, Avar *v, double *vals)
{
   assert(v && vals);
   Var *vars = ctr->vars;
   size_t total_n = rctr_totaln(ctr);
   for (size_t i = 0; i < v->size; ++i) {
      rhp_idx vi = avar_fget(v, i);
      assert(valid_vi(vi));
      vi_inbounds(vi, total_n, __func__);
      vals[i] = vars[vi].value;
   }
   return OK;
}

static int rctr_getcoljacinfo(const Container *ctr, int colidx,
                              void **jacptr, double *jacval, int *rowidx,
                              int *nlflag)
{
   const struct ctrdata_rhp *model = (struct ctrdata_rhp *) ctr->data;

   struct ctr_mat_elt *e = (struct ctr_mat_elt*)*jacptr;
   assert(!e || (valid_vi(e->vi) && e->vi == colidx &&
         "In rmdl_getcoljacinfo :: unconsistency between jacptr and colidx"));
   if (!valid_vi(colidx) || model->total_n < (unsigned)colidx) {
      return Error_IndexOutOfRange;
   }

   if (!e) {
      e = model->vars[colidx];
      if (!e) {
        error("%s :: variable %d (%s) is not in the model\n",
                 __func__, colidx, ctr_printvarname(ctr, colidx));
         return Error_NullPointer;
      }
   }

   *jacptr = e->next_equ;
   *jacval = e->value;
   *rowidx = e->ei;
   *nlflag = (int)e->isNL;

   return OK;
}

/**
 * @brief  Get a variable name
 *
 * @param       l    the list
 * @param       idx  the index
 * @param[out]  str  the string to print in
 * @param       len  the length of the string
 *
 * @return           the error code
 */
static int _vector_name_get(const struct vnames_list *l, rhp_idx idx, char *str, unsigned len)
{
  for (size_t j = 0; j < l->len; ++j) {
    rhp_idx s = l->start[j];
    rhp_idx e = l->end[j];

    if ((idx >= s) && (idx <= e)) {

      if (s == e) {
        strncpy(str, l->names[j], len);
      } else {
        snprintf(str, len, "%s(%u)", l->names[j], idx - l->start[j] + 1);
      }

      return OK;
    }
  }

  return Error_IndexOutOfRange;
}

static int rctr_copyvarname_v(const Container *ctr, int vi, char *str, unsigned len)
{
  const struct ctrdata_rhp *ctrdat = (struct ctrdata_rhp *) ctr->data;

  rhp_idx vi_up = cdat_var_inherited(ctrdat, vi);
  if (valid_vi(vi_up)) {
    return ctr_copyvarname(ctr->ctr_up, vi_up, str, len);

  }

   if ((unsigned)vi < ctrdat->total_n) {

    const struct vnames *vnames = &ctrdat->var_names.v;

    if (vnames && vi < vnames->start) {
      goto _notfound;
/*      error("%s :: variable index %u less than vname start %u\n",
          __func__, vi, vnames->start);
      return Error_Unconsistency;*/
    }

    while(vnames) {

      if (vi <= vnames->end) { /* end is an index */

        switch (vnames->type) {
        case VNAMES_REGULAR:
        {
          int rc = _vector_name_get(vnames->list, vi, str, len);
          if (rc == OK) {
            return OK;
          }
          goto _notfound;
        }
        case VNAMES_MULTIPLIERS:
        {
          /* We assume that the multiplier and their equation have the same index */
          strncpy(str, "mult_of_", len);
          size_t cur_len = strlen(str);
          if (!ctr->varmeta) {
            errormsg("%s :: while querying multiplier name, varmeta is NULL");
            return Error_NullPointer;
          }
          
          rhp_idx ei = ctr->varmeta[vi].dual;
          assert(valid_ei(ei));
          return ctr_copyequname(ctr, ei, &str[cur_len], len-cur_len);
        }
         case VNAMES_FOOC_LOOKUP: {
            error("%s :: unsupported VNAME type FOOC_LOOKUP", __func__);
            return Error_RuntimeError;
         }
        case VNAMES_LAGRANGIAN_GRADIENT:
        {
          error("%s :: unsupported VNAME type GRADIENT", __func__);
          return Error_RuntimeError;
        }
        case VNAMES_UNSET:
          break;
        default:
          error("%s :: unknown vname type %d", __func__, vnames->type);
        }

      } 

      vnames = vnames->next;
    }

_notfound:
    str[0] = 'x';
    unsignedtostr((unsigned)vi+1, sizeof(unsigned), &str[1], len-1, 10);

    return OK;
  }

   error("%s :: index %d >= %zu\n", __func__, vi, ctrdat->total_n);
   strncpy(str, "out_of_range", len);
   return Error_IndexOutOfRange;

}

static int rctr_copyequname_v(const Container *ctr, int ei, char *str, unsigned len)
{
   const RhpContainerData *cdat = (RhpContainerData *) ctr->data;

   rhp_idx ei_up = cdat_equ_inherited(cdat, ei);
   if (!valid_ei(ei_up)) {
      ei_up = cdat_equname_inherited(cdat, ei);
   }

   if (valid_ei(ei_up)) {
      return ctr_copyequname(ctr->ctr_up, ei_up, str, len);
   }

   if ((unsigned)ei >= cdat->total_m) {

      error("%s :: index %u >= %zu\n", __func__, ei, cdat->total_m);
      strncpy(str, "out_of_range", len);
      return Error_IndexOutOfRange;
   }

   const struct vnames *vnames = &cdat->equ_names.v;

   if (vnames && ei < vnames->start) {
      goto _notfound;
   }

   while(vnames) {

      if (ei <= vnames->end) { /* end is an index */

         switch (vnames->type) {
            case VNAMES_REGULAR:
               {
                  int rc = _vector_name_get(vnames->list, ei, str, len);
                  if (rc == OK) {
                     return OK;
                  }
                  goto _notfound;
               }
            case VNAMES_MULTIPLIERS:
               {
                  error("%s :: unsupported VNAME type MULTIPLIERS", __func__);
                  return Error_RuntimeError;
               }
            case VNAMES_FOOC_LOOKUP:
               /* The lookup for equation is relative to the start */
               return vnames_lookup_copyname(vnames->fooc_lookup, ei-vnames->start,
                                             str, len);
            case VNAMES_LAGRANGIAN_GRADIENT:
               {
                  str[0] = 'd';
                  str[1] = 'L';
                  str[2] = 'd';
                  str[3] = '_';
                  size_t cur_len = 4;
                  assert(len > cur_len);
                  
                  if (!ctr->equmeta) {
                    errormsg("%s :: while querying Lagrangian name, equmeta is NULL");
                    return Error_NullPointer;
                  }
                  rhp_idx vi = ctr->equmeta[ei].dual; assert(valid_vi(vi));
                  return ctr_copyvarname(ctr, vi, &str[cur_len], len-cur_len);
               }
            case VNAMES_UNSET:
               break;
            default:
               error("%s :: unknown vname type %d", __func__, vnames->type);
         }

      }

      vnames = vnames->next;
   }

_notfound:
   str[0] = 'e';
   unsignedtostr((unsigned)ei+1, sizeof(unsigned), &str[1], len-1, 10);

   return OK;

}

static int rctr_copyvarname_s(const Container *ctr, int vi, char *str, unsigned len)
{
   const struct ctrdata_rhp *model = (struct ctrdata_rhp *) ctr->data;

   rhp_idx vi_up;
   if ((vi_up = cdat_var_inherited(model, vi)) && valid_vi(vi_up)) {
      return ctr_copyvarname(ctr->ctr_up, vi_up, str, len);
   } else if ((unsigned)vi < model->total_n) {
      if ((unsigned)vi < model->var_names.s.max && model->var_names.s.names[vi]) {
         strncpy(str, model->var_names.s.names[vi], len);
         return OK;
      } /* else is the fallback */
   } else {
      error("%s :: index %d >= %zu\n", __func__, vi,
               model->total_n);
      strncpy(str, "out_of_range", len);
      return Error_IndexOutOfRange;
   }

   /* ----------------------------------------------------------------------
    * This is the fallback
    *
    * JuMP does not expect a name for anonymous variable.
    * On the other hand, GAMS is really unhappy if there is no name
    * Therefore, we do not have a fallback, unless we are in reshop_solve()
    * ---------------------------------------------------------------------- */

   if (ctr_neednames(ctr)) {
     str[0] = 'x';
     unsignedtostr((unsigned)vi+1, sizeof(unsigned), &str[1], len-1, 10);
   } else {
     str[0] = '\0';
   }

   return OK;
}

static int rctr_setvarname_s(Container* ctr, rhp_idx vidx, const char *name)
{
   S_CHECK(rhp_chk_ctr(ctr, __func__));

   if (!name) {
      error("%s :: the variable name is NULL\n", __func__);
      return Error_NullPointer;
   }

   if (ctr->backend != RHP_BACKEND_JULIA) {
      error("%s :: the container must be of Julia type, got %s (%d)\n"
               , __func__, backend_name(ctr->backend), ctr->backend);
      return Error_Unconsistency;
   }

   struct ctrdata_rhp* model = (struct ctrdata_rhp*)ctr->data;

   S_CHECK(vi_inbounds(vidx, model->total_n, __func__));

   /* ----------------------------------------------------------------------
    * We perform the allocation in a lazy way: we try to only allocate the
    * smallest amount
    * ---------------------------------------------------------------------- */

   if (vidx >= model->var_names.s.max) {
      unsigned old_len = model->var_names.s.max;
      model->var_names.s.max = vidx+1;
      REALLOC_(model->var_names.s.names, const char *, model->var_names.s.max);
      /* Here we assume that NULL is 0  */
      memset(&model->var_names.s.names[old_len], 0, (vidx-old_len+1) * sizeof(char *));
   }

   /* In case we change the name  */
   FREE(model->var_names.s.names[vidx]);

   size_t len_v = strlen(name);
   if (len_v > 0) {
     char *cpy;
     MALLOC_(cpy, char, len_v + 1);
     strcpy(cpy, name);
     model->var_names.s.names[vidx] = cpy;
   }

   return OK;
}

/**
 * @brief Return the name of a variable
 *
 * @ingroup publicAPI
 *
 * @param      ctr   the container
 * @param      eidx  the equation index
 * @param[out] name  on output contains the pointer to name
 *
 * @return           the error code
 */
static int rctr_getequname_s(const Container *ctr, rhp_idx eidx, const char **name)
{
   S_CHECK(rhp_chk_ctr(ctr, __func__));

   if (ctr->backend != RHP_BACKEND_JULIA) {
      error("%s :: the container must be of Julia type, got %s (%d)\n"
               , __func__, backend_name(ctr->backend), ctr->backend);
      return Error_WrongModelForFunction;
   }

   const struct ctrdata_rhp *model = (struct ctrdata_rhp *) ctr->data;

   S_CHECK(ei_inbounds(eidx, model->total_m, __func__));

   if (eidx >= model->equ_names.s.max) {
      printout(PO_INFO, "%s :: equation index %d has no name\n", __func__, eidx);
      return Error_IndexOutOfRange;
   }

   *name = model->equ_names.s.names[eidx];

   return OK;
}

static int rctr_getvarname_s(const Container *ctr, rhp_idx vidx, const char **name)
{
   S_CHECK(rhp_chk_ctr(ctr, __func__));

   if (ctr->backend != RHP_BACKEND_JULIA) {
      error("%s :: the container must be of Julia type, got %s (%d)\n"
               , __func__, backend_name(ctr->backend), ctr->backend);
      return Error_WrongModelForFunction ;
   }

   const struct ctrdata_rhp *model = (struct ctrdata_rhp *) ctr->data;

   S_CHECK(vi_inbounds(vidx, model->total_n, __func__));

   if (vidx >= model->var_names.s.max) {
      printout(PO_INFO, "%s :: variable index %d has no name\n", __func__, vidx);
      return Error_IndexOutOfRange;
   }

   *name = model->var_names.s.names[vidx];

   return OK;
}

static int rctr_getvarbyname_s(const Container *ctr, const char* name, rhp_idx *vi)
{
   const struct ctrdata_rhp *model = (struct ctrdata_rhp *) ctr->data;

   *vi = IdxNotFound;

   if (ctr->ctr_up) {
      S_CHECK(ctr_getvarbyname(ctr->ctr_up, name, vi));
      if (valid_vi(*vi)) {
        return OK;
      }
   }

   const char ** const names = model->var_names.s.names;
   if (!names) {
     return OK;
   }

   for (size_t i = 0; i < model->total_n; ++i) {
      if (names[i] && !strcmp(name, names[i])) {
         if (!valid_vi(*vi)) {
            (*vi) = i;
         } else {
             (*vi) = IdxDuplicate;
             return Error_DuplicateValue;
         }
      }
   }

   return OK;
}

static int rctr_copyequname_s(const Container *ctr, int ei, char *str, unsigned len)
{
   const struct ctrdata_rhp *model = (struct ctrdata_rhp *) ctr->data;

   rhp_idx ei_up;
   if ((ei_up = cdat_equ_inherited(model, ei)) && valid_ei(ei_up)) {
      return ctr_copyequname(ctr->ctr_up, ei_up, str, len);
   } else if ((unsigned)ei < model->total_m) {
      if ((unsigned)ei < model->equ_names.s.max) {
         strncpy(str, model->equ_names.s.names[ei], len);
         return OK;
      } /* else is the fallback */
   } else {
      error("%s :: index %d >= %zu\n", __func__, ei,
               model->total_m);
      strncpy(str, "out_of_range", len);
      return Error_IndexOutOfRange;
   }

   /* ----------------------------------------------------------------------
    * This is the fallback
    *
    * JuMP does not expect a name for anonymous equation.
    * On the other hand, GAMS is really unhappy if there is no name
    * Therefore, we do not have a fallback, unless we are in reshop_solve()
    * ---------------------------------------------------------------------- */

   if (ctr_neednames(ctr)) {
     str[0] = 'e';
     unsignedtostr((unsigned)ei+1, sizeof(unsigned), &str[1], len-1, 10);
   } else {
     str[0] = '\0';
   }

   return OK;
}

static int rctr_getequbyname_s(const Container *ctr, const char* name, rhp_idx *ei)
{
   assert(ctr && name && ei);

   const RhpContainerData *ctrdat = (RhpContainerData *) ctr->data;

   *ei = IdxNotFound;

   const Container *ctr_up = ctr->ctr_up;
   if (ctr_up) {
      S_CHECK(ctr_getequbyname(ctr_up, name, ei));
      if (valid_ei(*ei)) {
        return OK;
      }
   }

   const char ** const names = ctrdat->equ_names.s.names;
   if (!names) {
     return OK;
   }

   for (size_t i = 0; i < ctrdat->total_m; ++i) {
      if (names[i] && !strcmp(name, names[i])) {
         if (!valid_ei(*ei)) {
            (*ei) = i;
         } else {
             (*ei) = IdxDuplicate;
             return Error_DuplicateValue;
         }
      }
   }

   return OK;
}

static int rctr_resize(Container *ctr, unsigned n, unsigned m)
{
   RhpContainerData *cdat = (RhpContainerData *) ctr->data;

   return cdat_resize(cdat, n, m);
}

static int rctr_setvarbounds(Container *ctr, rhp_idx vi, double lb, double ub)
{
   assert(valid_vi_(vi, ctr_nvars_total(ctr), __func__));

   Var * restrict v = &ctr->vars[vi];
   S_CHECK(chk_var_isnotconic(v, ctr, __func__));

   v->bnd.lb = lb;
   v->bnd.ub = ub;

   return OK;
}

static int rctr_setvarlb(Container *ctr, rhp_idx vi, double lb)
{
   assert(valid_vi_(vi, ctr_nvars_total(ctr), __func__));

   Var * restrict v = &ctr->vars[vi];
   S_CHECK(chk_var_isnotconic(v, ctr, __func__));

   v->bnd.lb = lb;

   return OK;
}

static int rctr_setvarval(Container *ctr, rhp_idx vi, double val)
{
   assert(valid_vi_(vi, ctr_nvars_total(ctr), __func__));

   ctr->vars[vi].value = val;
   return OK;
}

static int rctr_setvarmult(Container *ctr, rhp_idx vi, double multiplier)
{
   S_CHECK(vi_inbounds(vi, ctr_nvars_total(ctr), __func__));

   ctr->vars[vi].multiplier = multiplier;
   return OK;
}

static int rctr_setvartype(Container *ctr, rhp_idx vidx, unsigned type)
{
   S_CHECK(vi_inbounds(vidx, ctr_nvars_total(ctr), __func__));

   if (!var_validtype(type)) {
      error("%s :: invalid type %d\n", __func__, type);
      return Error_InvalidValue;
   }
   ctr->vars[vidx].type = type;
   var_update_bnd(&ctr->vars[vidx], type);

   return OK;
}

static int rctr_setvarub(Container *ctr, rhp_idx vi, double ub)
{
   S_CHECK(vi_inbounds(vi, ctr_nvars_total(ctr), __func__));

   Var * restrict v = &ctr->vars[vi];
   S_CHECK(chk_var_isnotconic(v, ctr, __func__));

   v->bnd.ub = ub;

   return OK;
}

static int rctr_getequbasis(const Container *ctr, int i, int *bstat)
{
   if (!valid_ei(i) || (size_t)i >= ((struct ctrdata_rhp *)ctr->data)->total_m) {
      error("%s :: invalid index %d\n", __func__, i);
      return Error_IndexOutOfRange;
   }

   (*bstat) = ctr->equs[i].basis;

   return OK;
}

static int rctr_getequval(const Container *ctr, rhp_idx ei, double *level)
{
   if (!valid_ei(ei) || (size_t)ei >= ((struct ctrdata_rhp *)ctr->data)->total_m) {
      error("%s :: invalid index %d\n", __func__, ei);
      return Error_IndexOutOfRange;
   }

   *level = ctr->equs[ei].value;

   return OK;
}

static int rctr_getequmult(const Container *ctr, rhp_idx ei, double *multiplier)
{
   if (!valid_ei(ei) || (size_t)ei >= ((struct ctrdata_rhp *)ctr->data)->total_m) {
      error("%s :: invalid index %d\n", __func__, ei);
      return Error_IndexOutOfRange;
   }

   *multiplier = ctr->equs[ei].multiplier;

   return OK;
}

static int rctr_getallequsmult(const Container *ctr, double *mult)
{
   assert(mult);
   Equ *equs = ctr->equs;
   size_t n = ctr_nvars(ctr);
   for (size_t i = 0; i < n; ++i) {
      mult[i] = equs[i].multiplier;
   }
   return OK;
}

static int rctr_getequperp(const Container *ctr, rhp_idx ei, rhp_idx *vi)
{
  if (!ctr->equmeta) {
    return Error_ModelIncompleteMetadata;
  }

  S_CHECK(ei_inbounds(ei,((struct ctrdata_rhp*)ctr->data)->total_m, __func__));
  *vi = ctr->equmeta[ei].dual;

  return OK;
}

static int rctr_getallequsval(const Container *ctr, double *vals)
{
   assert(vals);
   Equ *equs = ctr->equs;
   size_t n = ctr_nvars(ctr);
   for (size_t i = 0; i < n; ++i) {
      vals[i] = equs[i].value;
   }
   return OK;
}

static int rctr_getequtype(const Container *ctr, rhp_idx ei, unsigned *type, unsigned *cone)
{
   S_CHECK(ei_inbounds(ei, ((struct ctrdata_rhp *)ctr->data)->total_m, __func__));

   *type = ctr->equs[ei].object;
   *cone = ctr->equs[ei].cone;

   return OK;
}

static int rctr_getspecialfloats(const Container *ctr, double *pinf, double *minf, double* nan)
{
  *pinf = INFINITY;
  *minf = -INFINITY;
  *nan = NAN;

  return OK;
}

static int rctr_getcst(const Container *ctr, rhp_idx eidx, double *val)
{
    if (!valid_ei(eidx) || (size_t)eidx >= ((struct ctrdata_rhp *)ctr->data)->total_m) {
      error("%s :: invalid index %d\n", __func__, eidx);
      return Error_IndexOutOfRange;
   }

   *val = equ_get_cst(&ctr->equs[eidx]);

   return OK;
}

static int rctr_getvarbounds(const Container *ctr, rhp_idx vi, double *lb, double *ub)
{
   if (!valid_vi(vi) || (size_t)vi >= ((struct ctrdata_rhp *)ctr->data)->total_n) {
      error("%s :: invalid index %d\n", __func__, vi);
      return Error_IndexOutOfRange;
   }

   *lb = ctr->vars[vi].bnd.lb;
   *ub = ctr->vars[vi].bnd.ub;

   return OK;
}

static int rctr_getvarbasis(const Container *ctr, rhp_idx vi, int *bstat)
{
   if (!valid_vi(vi) || (size_t)vi >= ((struct ctrdata_rhp *)ctr->data)->total_n) {
      error("%s :: invalid index %d\n", __func__, vi);
      return Error_IndexOutOfRange;
   }

   (*bstat) = ctr->vars[vi].basis;

   return OK;
}

static int rctr_getallvarsmult(const Container *ctr, double *mult)
{
   assert(mult);
   Var *vars = ctr->vars;
   size_t n = ctr_nvars(ctr);
   for (size_t i = 0; i < n; ++i) {
      mult[i] = vars[i].multiplier;
   }
   return OK;
}

static int rctr_getallvarsval(const Container *ctr, double *vals)
{
   assert(vals);
   Var *vars = ctr->vars;
   size_t n = ctr_nvars(ctr);
   for (size_t i = 0; i < n; ++i) {
      vals[i] = vars[i].value;
   }
   return OK;
}

static int rctr_getvartype(const Container *ctr, int vi, unsigned *type)
{
    if (!valid_vi(vi) || (size_t)vi >= ((struct ctrdata_rhp *)ctr->data)->total_n) {
      error("%s :: invalid index %d\n", __func__, vi);
      return Error_IndexOutOfRange;
   }

   (*type) = ctr->vars[vi].type;

   return OK;
}

static int rctr_getvarlb(const Container *ctr, rhp_idx vi, double *lb)
{
   assert(valid_vi_(vi, ctr_nvars_total(ctr), __func__));

   Var * restrict v = &ctr->vars[vi];
   S_CHECK(chk_var_isnotconic(v, ctr, __func__));

   *lb = v->bnd.lb;

   return OK;
}

static int rctr_getvarub(const Container *ctr, rhp_idx vi, double *ub)
{
   assert(valid_vi_(vi, ctr_nvars_total(ctr), __func__));

   Var * restrict v = &ctr->vars[vi];
   S_CHECK(chk_var_isnotconic(v, ctr, __func__));

   *ub = v->bnd.ub;

   return OK;
}

static int rctr_getvarval(const Container *ctr, rhp_idx vi, double *level)
{
   if (!valid_vi(vi) || (size_t)vi >= ((struct ctrdata_rhp *)ctr->data)->total_n) {
      error("%s :: invalid index %d\n", __func__, vi);
      return Error_IndexOutOfRange;
   }
   *level = ctr->vars[vi].value;
   return OK;
}

static int rctr_getvarperp(const Container *ctr, rhp_idx vi, rhp_idx *ei)
{
  if (!ctr->varmeta) {
    return Error_ModelIncompleteMetadata;
  }

  S_CHECK(vi_inbounds(vi,((struct ctrdata_rhp*)ctr->data)->total_n, __func__));
  *ei = ctr->varmeta[vi].dual;

  return OK;
}

static int rctr_getvarmult(const Container *ctr, rhp_idx vi, double *multiplier)
{
   if (!valid_vi(vi) || (size_t)vi >= ((struct ctrdata_rhp *)ctr->data)->total_n) {
      error("%s :: invalid index %d\n", __func__, vi);
      return Error_IndexOutOfRange;
   }

   *multiplier = ctr->vars[vi].multiplier;
   return OK;
}

static int rctr_setequval(Container *ctr, rhp_idx ei, double level)
{
   if (!valid_ei(ei) || (size_t)ei >= ((struct ctrdata_rhp *)ctr->data)->total_m) {
      error("%s :: invalid index %d\n", __func__, ei);
      return Error_IndexOutOfRange;
   }

   ctr->equs[ei].value = level;
   return OK;
}

static int rctr_setequname_s(Container* ctr, rhp_idx ei, const char *name)
{
   S_CHECK(rhp_chk_ctr(ctr, __func__));


   BackendType backend = ctr->backend;
   if (backend != RHP_BACKEND_JULIA) {
      error("%s :: the container must be of Julia type, got %s (%d)\n"
               , __func__, backend_name(backend), backend);
      return Error_Unconsistency;
   }

   RhpContainerData* model = (RhpContainerData*)ctr->data;

   S_CHECK(ei_inbounds(ei, model->total_m, __func__));

   /* ----------------------------------------------------------------------
    * We perform the allocation in a lazy way: we try to only allocate the
    * smallest amount
    * ---------------------------------------------------------------------- */

   if (ei >= model->equ_names.s.max) {
      unsigned old_len = model->equ_names.s.max;
      model->equ_names.s.max = ei+1;
      REALLOC_(model->equ_names.s.names, const char *, model->equ_names.s.max);
      /* Here we assume that NULL is 0  */
      memset(&model->equ_names.s.names[old_len], 0, (ei-old_len+1) * sizeof(char *));
   }

   /* In case we change the name  */
   FREE(model->equ_names.s.names[ei]);

   size_t len_v = strlen(name);
   if (len_v > 0) {
     char *cpy;
     MALLOC_(cpy, char, len_v + 1);
     strcpy(cpy, name);
     model->equ_names.s.names[ei] = cpy;
   }

   return OK;
}

static int rctr_setequmult(Container *ctr, rhp_idx ei, double multiplier)
{
   if (!valid_ei(ei) || (size_t)ei >= ((struct ctrdata_rhp *)ctr->data)->total_m) {
      error("%s :: invalid index %d\n", __func__, ei);
      return Error_IndexOutOfRange;
   }

   ctr->equs[ei].multiplier = multiplier;
   return OK;
}

static int rctr_setequtype(Container *ctr, rhp_idx eidx, unsigned type, unsigned cone)
{
    if (!valid_ei(eidx) || (size_t)eidx >= ((struct ctrdata_rhp *)ctr->data)->total_m) {
      error("%s :: invalid index %d\n", __func__, eidx);
      return Error_IndexOutOfRange;
   }

   if (type >= EQ_UNSUPPORTED) {
      error("%s :: invalid equation type value %u\n", __func__, type);
   }

   if (cone >= CONE_LEN) {
      error("%s :: invalid cone value %u\n", __func__, cone);
      return Error_InvalidValue;
   }

   ctr->equs[eidx].object = type;
   ctr->equs[eidx].cone = cone;

   return OK;
}

static int _rmdl_setequvarperp(Container *ctr, rhp_idx ei, rhp_idx vi)
{
   struct ctrdata_rhp *model = (struct ctrdata_rhp *) ctr->data;
   S_CHECK(vi_inbounds(vi, model->total_n, __func__));

   if (valid_ei(ei)) {
      S_CHECK(ei_inbounds(ei, model->total_m, __func__));
   }

   return rctr_setequvarperp(ctr, ei, vi);
}

static int rctr_setequcst(Container *ctr, rhp_idx ei, double val)
{
    if (!valid_ei(ei) || (size_t)ei >= ((struct ctrdata_rhp *)ctr->data)->total_m) {
      error("%s :: invalid index %d\n", __func__, ei);
      return Error_IndexOutOfRange;
   }

   equ_set_cst(&ctr->equs[ei], val);

   return OK;
}


static int rctr_isequNL(const Container *ctr, int ei, bool *isNL)
{
   const struct ctrdata_rhp *model = (struct ctrdata_rhp *) ctr->data;
   assert(ctr);

   rhp_idx ei_up;
   if ((ei_up = cdat_equ_inherited(model, ei)) && valid_ei(ei_up)) {
      return ctr_isequNL(ctr->ctr_up, ei_up, isNL);
   }

   if (ctr->equs[ei].tree) {
     *isNL = true;
   } else {
     *isNL = false;
   }

   return OK;
}

const struct container_ops ctr_ops_rhp = {
   .allocdata      = rctr_allocdata,
   .deallocdata    = rctr_deallocdata,
   .copyequname    = rctr_copyequname_v,
   .copyvarname    = rctr_copyvarname_v,
   .evalequvar     = rctr_evalequvar,
   .evalfunc       = NULL,
   .evalgrad       = NULL,
   .evalgradobj    = NULL,
   .getequsbasis   = rctr_getequsbasis,
   .getequsmult    = rctr_getequsmult,
   .getequsval     = rctr_getequsval,
   .getvarsbasis   = rctr_getvarsbasis,
   .getvarsmult    = rctr_getvarsmult,
   .getvarsval     = rctr_getvarsval,
   .getequbyname   = rctr_notimpl3char_,
   .getequval     = rctr_getequval,
   .getequmult     = rctr_getequmult,
   .getallequsmult    = rctr_getallequsmult,
   .getequperp     = rctr_getequperp,
   .getallequsval     = rctr_getallequsval,
   .getequbasis  = rctr_getequbasis,
   .getequname     = rctr_getequname_s,
   .getequtype     = rctr_getequtype,
   .getcoljacinfo  = rctr_getcoljacinfo,
   .getrowjacinfo  = rctr_walkequ,
   .getequcst         = rctr_getcst,
   .getspecialfloats = rctr_getspecialfloats,
   .getvarbounds   = rctr_getvarbounds,
   .getvarbyname   = rctr_notimpl3char_,
   .getvarlb       = rctr_getvarlb,
   .getvarub       = rctr_getvarub,
   .getvarval     = rctr_getvarval,
   .getvarmult     = rctr_getvarmult,
   .getvarperp     = rctr_getvarperp,
   .getallvarsmult    = rctr_getallvarsmult,
   .getvarbasis  = rctr_getvarbasis,
   .getallvarsval     = rctr_getallvarsval,
   .getvartype     = rctr_getvartype,
   .isequNL        = rctr_isequNL,
   .resize         = rctr_resize,
   .setequval     = rctr_setequval,
   .setequmult     = rctr_setequmult,
   .setequname     = rctr_setequname_s,
   .setequtype     = rctr_setequtype,
   .setequvarperp  = rctr_setequvarperp,
   .setequcst         = rctr_setequcst,
   .setvarlb       = rctr_setvarlb,
   .setvarbounds   = rctr_setvarbounds,
   .setvarval     = rctr_setvarval,
   .setvarmult     = rctr_setvarmult,
   .setvarname     = rctr_setvarname_s,
   .setvartype     = rctr_setvartype,
   .setvarub       = rctr_setvarub,
};

static int rctr_copyvarname(const Container *ctr, int i, char *str, unsigned len)
{
   if (ctr->backend == RHP_BACKEND_RHP) {
      return rctr_copyvarname_v(ctr, i, str, len);
   }
   return rctr_copyvarname_s(ctr, i, str, len);
}

static int rctr_copyequname(const Container *ctr, int i, char *str, unsigned len)
{
   if (ctr->backend == RHP_BACKEND_RHP) {
      return rctr_copyequname_v(ctr, i, str, len);
   }
   return rctr_copyequname_s(ctr, i, str, len);
}


const struct container_ops ctr_ops_julia = {
   .allocdata      = rctr_allocdata,
   .deallocdata    = rctr_deallocdata,
   .copyequname    = rctr_copyequname_s,
   .copyvarname    = rctr_copyvarname_s,
   .evalfunc       = NULL,
   .evalgrad       = NULL,
   .evalgradobj    = NULL,
   .evalequvar     = rctr_evalequvar,
   .getequsbasis   = rctr_getequsbasis,
   .getequsmult    = rctr_getequsmult,
   .getequsval     = rctr_getequsval,
   .getvarsbasis   = rctr_getvarsbasis,
   .getvarsmult    = rctr_getvarsmult,
   .getvarsval     = rctr_getvarsval,
   .getequbyname   = rctr_getequbyname_s,
   .getequval      = rctr_getequval,
   .getequmult     = rctr_getequmult,
   .getequperp     = rctr_getequperp,
   .getallequsmult = rctr_getallequsmult,
   .getequbasis    = rctr_getequbasis,
   .getallequsval  = rctr_getallequsval,
   .getequtype     = rctr_getequtype,
   .getcoljacinfo  = rctr_getcoljacinfo,
   .getrowjacinfo  = rctr_walkequ,
   .getequcst         = rctr_getcst,
   .getspecialfloats = rctr_getspecialfloats,
   .getvarbounds   = rctr_getvarbounds,
   .getvarbyname   = rctr_getvarbyname_s,
   .getvarlb       = rctr_getvarlb,
   .getvarub       = rctr_getvarub,
   .getvarval      = rctr_getvarval,
   .getvarmult     = rctr_getvarmult,
   .getvarperp     = rctr_getvarperp,
   .getallvarsmult = rctr_getallvarsmult,
   .getvarbasis    = rctr_getvarbasis,
   .getallvarsval  = rctr_getallvarsval,
   .getvartype     = rctr_getvartype,
   .isequNL        = rctr_isequNL,
   .resize         = rctr_resize,
   .setequval      = rctr_setequval,
   .setequmult     = rctr_setequmult,
   .setequname     = rctr_setequname_s,
   .setequtype     = rctr_setequtype,
   .setequvarperp  = rctr_setequvarperp,
   .setequcst      = rctr_setequcst,
   .setvarlb       = rctr_setvarlb,
   .setvarval      = rctr_setvarval,
   .setvarmult     = rctr_setvarmult,
   .setvarname     = rctr_setvarname_s,
   .setvartype     = rctr_setvartype,
   .setvarub       = rctr_setvarub,
};
