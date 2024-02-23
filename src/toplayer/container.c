#include <fenv.h>
#include <math.h>

#include "checks.h"
#include "container.h"
#include "container_helpers.h"
#include "container_ops.h"
#include "consts.h"
#include "ctrdat_rhp.h"
#include "equvar_metadata.h"
#include "macros.h"
#include "mem-debug.h"
#include "printout.h"
#include "pool.h"
#include "reshop_data.h"
#include "status.h"
#include "tlsdef.h"

tlsvar char bufvar[SHORT_STRING];
tlsvar char bufvar2[SHORT_STRING];
tlsvar char bufequ[SHORT_STRING];
tlsvar char bufequ2[SHORT_STRING];

static void dealloc_(Container *ctr)
{
   if (!ctr) {
      return;
   }

   if (ctr->ops) {
      ctr->ops->deallocdata(ctr);
   }

   if (ctr->vars) {
      FREE(ctr->vars);
   }

   if (ctr->equs) {
      for (size_t i = 0; i < (size_t)ctr->m; i++) {
         equ_free(&ctr->equs[i]);
      }
      FREE(ctr->equs);
   }

   if (ctr->workspace.inuse) {
      error("%s :: workspace memory was still tagged as used\n", __func__);
   }
   FREE(ctr->workspace.mem);

   FREE(ctr->rosetta_vars);
   FREE(ctr->rosetta_equs);
   FREE(ctr->varmeta);
   FREE(ctr->equmeta);

   pool_release(ctr->pool);

   aequ_empty(&ctr->transformations.flipped_equs);
   aequ_free(ctr->func2eval);
   avar_free(ctr->fixed_vars);

}

/**
 * @brief Free the allocated memory, include the structure itself
 *
 * @param  ctr  the container
 */
void ctr_dealloc(Container *ctr)
{
   dealloc_(ctr);
}

/**
 * @brief Allocate a model for a given architecture
 *
 * @ingroup publicAPI
 *
 * @param  backend  the backend (GAMS, Julia, ReSHOP) of the model
 *
 * @return       the model structure, or NULL if the allocation failed
 */
int ctr_alloc(Container *ctr, BackendType backend)
{
   int status = OK;
   /* ------------------------------------------------------------------------
    * We need to ensure that the rounding mode for floating point is set to
    * nearest. Otherwise, the conversion from double to string may not give the
    * expected results. The rounding mode can be changed by any (external)
    * function. Therefore, we set the value each time we need to write a model
    * to the file --xhub
    * ----------------------------------------------------------------------- */

   fesetround(FE_TONEAREST);

   switch (backend) {
   case RHP_BACKEND_GAMS_GMO:
      ctr->ops = &ctr_ops_gams;
      break;
   case RHP_BACKEND_RHP:
      ctr->ops = &ctr_ops_rhp;
      break;
   case RHP_BACKEND_JULIA:
      ctr->ops = &ctr_ops_julia;
      break;
   default:
      error("%s :: unsupported backend %d\n", __func__, backend);
      goto _exit;
   }

   ctr->backend = backend;

   /* Pool might be initialized depending on the backend */
   ctr->pool = NULL;

   SN_CHECK_EXIT(ctr->ops->allocdata(ctr));

   /* Julia want anonymous variable to have no names ... */
   if (backend != RHP_BACKEND_JULIA) {
     ctr_setneednames(ctr);
   }

   aequ_setblock(&ctr->transformations.flipped_equs, 2);
   ctr->func2eval = aequ_newblock(2);
   ctr->fixed_vars = avar_newcompact(0, IdxNA);

   return OK;

_exit:
   dealloc_(ctr);
   return status;
}

// TODO GITLAB #78
#if 0
int ctr_equcontainvar(const Container *ctr, rhp_idx ei, rhp_idx vi,
                      double *jacval, int *nlflag)
{
   int colidx;
   void *jac;

   jac = NULL;
   do {
      ctr_getrowjacinfo(ctr, ei, &jac, jacval, &colidx, nlflag);

      if (colidx == vi) {
         return true;
      }
   } while (jac != NULL);

   return false;
}
#endif

unsigned ctr_nequs(const Container *ctr)
{
   return ctr->m;
}

unsigned ctr_nvars(const Container *ctr)
{
   if (ctr_is_rhp(ctr)) {
      RhpContainerData *cdat = (RhpContainerData *) ctr->data;
      /* ----------------------------------------------------------------------
       * If the solver model needed objective variables, and those where introduced
       * in the export phase.
       * ---------------------------------------------------------------------- */

      return ctr->n - cdat->pp.remove_objvars;
   }

   return ctr->n;
}

unsigned ctr_nvars_total(const Container *ctr)
{
   if (ctr_is_rhp(ctr)) {
      RhpContainerData *cdat = (RhpContainerData *) ctr->data;
      return cdat->total_n;
   }

   return ctr->n;
   
}

unsigned ctr_nequs_total(const Container *ctr)
{
   if (ctr_is_rhp(ctr)) {
      RhpContainerData *cdat = (RhpContainerData *) ctr->data;
      return cdat->total_m;
   }

   return ctr->m;
}

unsigned ctr_nvars_max(const Container *ctr)
{
   if (ctr_is_rhp(ctr)) {
      RhpContainerData *cdat = (RhpContainerData *) ctr->data;
      return cdat->max_n;
   }

   return ctr->n;
   
}

unsigned ctr_nequs_max(const Container *ctr)
{
   if (ctr_is_rhp(ctr)) {
      RhpContainerData *cdat = (RhpContainerData *) ctr->data;
      return cdat->max_m;
   }

   return ctr->m;
}
/**
 *  @brief Resize the container object
 *
 *  This function resize the space for the variables, equations and their
 *  metadata
 *
 *  @param  ctr  the container
 *  @param  n    the new number of variables
 *  @param  m    the new number of equations
 *
 *  @return      the error code
 */
int ctr_resize(Container *ctr, unsigned nvars, unsigned nequs)
{
   unsigned old_n = ctr->n;
   unsigned old_m = ctr->m;

   /*  TODO(xhub) this must go */
   /* TODO GITLAB #104 */
   ctr->n = 0;
   ctr->m = 0;

   nvars = MAX(nvars, 1);
   nequs = MAX(nequs, 1);

   /* ----------------------------------------------------------------------
    * Perform the memory allocations
    * ---------------------------------------------------------------------- */

   REALLOC_(ctr->vars, Var, nvars);
   REALLOC_(ctr->equs, struct equ, nequs);

   /* ----------------------------------------------------------------------
    * Initialize metadata only if needed
    * ---------------------------------------------------------------------- */

   bool init_varmeta = false;

    if (ctr->varmeta) {
       init_varmeta = true;
       REALLOC_(ctr->varmeta, struct var_meta, nvars);
       REALLOC_(ctr->equmeta, struct equ_meta, nequs);
    }


   /* ----------------------------------------------------------------------
    * Initialize the new variables and equations
    * ---------------------------------------------------------------------- */

   if (nvars > old_n) {
      memset(&ctr->vars[old_n], 0, (nvars-old_n)*sizeof(Var));
      if (init_varmeta) {
         for (unsigned i = old_n; i < nvars; ++i) {
            varmeta_init(&ctr->varmeta[i]);
         }
      }
   }

   if (nequs > old_m) {
      memset(&ctr->equs[old_m], 0, (nequs-old_m)*sizeof(struct equ));
      if (init_varmeta) {
         for (unsigned i = old_m; i < nequs; ++i) {
            equmeta_init(&ctr->equmeta[i]);
         }
      }
   }

   return ctr->ops->resize(ctr, nvars, nequs);
}

/**
 * @brief  store the matching between a variable and an equation
 *
 * @param ctr  the container object
 * @param ei   the equation
 * @param vi   the variable
 *
 * @return     the error code
 */
int ctr_setequvarperp(Container *ctr, rhp_idx ei, rhp_idx vi)
{
   return ctr->ops->setequvarperp(ctr, ei, vi);
}

/**
 * @brief Get some working memory
 *
 * @warning do not claim ownership of the memory. This is just to get some
 * temporary memory. Thread-safety is unknown (should be good)
 *
 * @param ctr   the container object
 * @param size  the size (in bytes)
 *
 * @return      a pointer on the memory
 */
void *ctr_getmem(Container *ctr, size_t size)
{
#ifdef RHP_HAS_CLEANUP
   if (ctr->workspace.inuse) {
      error("%s :: workspace memory already in use\n", __func__);
      return NULL;
   }
#endif

   /* the "+1" is there to put a safeguard with valgrind */
   if (ctr->workspace.size < size) {
      REALLOC_NULL(ctr->workspace.mem, uint8_t, size+1);
      ctr->workspace.size = size;
   }

#ifdef RHP_HAS_CLEANUP
   ctr->workspace.inuse = true;
#endif
   VALGRIND_MAKE_MEM_UNDEFINED(ctr->workspace.mem, size);
   VALGRIND_MAKE_MEM_NOACCESS(&ctr->workspace.mem[size], 1);
   return ctr->workspace.mem;
}

/**
 * @brief Ensure that working memory is large enough
 *
 * @warning do not claim ownership of the memory. This is just to get some
 * temporary memory. Thread-safety is unknown (should be good)
 *
 * @param  ctr         the container object
 * @param  cur_size    the size (in bytes)
 * @param  extra_size  the extra size
 *
 * @return      a pointer on the memory
 */
void *ctr_ensuremem(Container *ctr, size_t cur_size, size_t extra_size)
{

   /* the "+1" is there to put a safeguard with valgrind */
   if (ctr->workspace.size < cur_size+extra_size) {
      ctr->workspace.size = MAX(cur_size + extra_size, 2*cur_size);
      REALLOC_NULL(ctr->workspace.mem, uint8_t, ctr->workspace.size+1);
   }

   UNUSED size_t size = ctr->workspace.size;
   VALGRIND_MAKE_MEM_UNDEFINED(&ctr->workspace.mem[cur_size], size-cur_size);
   VALGRIND_MAKE_MEM_NOACCESS(&ctr->workspace.mem[size], 1);

   return ctr->workspace.mem;
}

/**
 * @brief Mark memory workspace as released
 *
 * @param ctr  the container object
 */
void ctr_relmem(Container *ctr)
{
#ifdef RHP_HAS_CLEANUP
   ctr->workspace.inuse = false;
#endif
   VALGRIND_MAKE_MEM_NOACCESS(ctr->workspace.mem, ctr->workspace.size+1);
}

double ctr_poolval(Container *ctr, unsigned idx)
{
   if (!ctr->pool) {
      error("%s :: no pool in container!\n", __func__);
      return INFINITY;
   }

   if (idx >= ctr->pool->len) {
      error("%s :: requesting pool index %d when the size of the "
                         "pool is %zu\n", __func__, idx, ctr->pool->len);
      return -INFINITY;
   }

   return ctr->pool->data[idx];
}

/**
 * @brief Return the name of a variable
 *
 * @warning the string returned by this function points to a global (TLS) variable.
 * The caller is responsible to immediately use the string or its content.
 * Use ctr_copyvarname() to get the name of a variable in a more durable way.
 *
 * @param ctr  the model
 * @param vi   the variable index
 *
 * @return     the variable name
 */
const char* ctr_printvarname(const Container *ctr, rhp_idx vi)
{
   int status;
   
   if (!valid_vi(vi)) {
      strncpy(bufvar, badidx_str(vi), sizeof(bufvar)-1);
      return bufvar;
   }

   if ((status = ctr_copyvarname(ctr, vi, bufvar, sizeof(bufvar))) != OK) {
      snprintf(bufvar, sizeof(bufvar), "error %s (%d) while querying variable "
               "name with index %d", rhp_status_descr(status), status, vi);
   }

   return bufvar;
}

/**
 * @brief Return the name of a variable
 *
 * @warning the string returned by this function points to a global (TLS) variable.
 * The caller is responsible to immediately use the string or its content.
 * Use ctr_copyvarname() to get the name of a variable in a more durable way.
 *
 * @param ctr  the model
 * @param vi   the variable index
 *
 * @return     the variable name
 */
const char* ctr_printvarname2(const Container *ctr, rhp_idx vi)
{
   int status;
   
   if (!valid_vi(vi)) {
      strncpy(bufvar2, badidx_str(vi), sizeof(bufvar2)-1);
      return bufvar2;
   }

   if ((status = ctr_copyvarname(ctr, vi, bufvar2, sizeof(bufvar2))) != OK) {
      snprintf(bufvar2, sizeof(bufvar2), "error %s (%d) while querying variable "
               "name with index %d", rhp_status_descr(status), status, vi);
   }

   return bufvar2;
}
/**
 * @brief Return the name of a equation
 *
 * @warning the string returned by this function points to a global variable.
 * The caller is responsible to directly use the string or its content.
 * Use ctr_copyequname() to get the name of a equation in a more durable way.
 *
 * @param ctr  the model
 * @param ei   the equation index
 *
 * @return     the equation name
 */
const char* ctr_printequname(const Container *ctr, rhp_idx ei)
{
   if (!valid_ei(ei)) {
      strncpy(bufequ, badidx_str(ei), sizeof(bufequ)-1);
      return bufequ;
   }

   int status = ctr_copyequname(ctr, ei, bufequ, sizeof(bufequ));
   if (status != OK) {
      snprintf(bufequ, sizeof(bufequ), "error %s (%d) while querying equation "
               "name with index %d", rhp_status_descr(status), status, ei);
   }

   return bufequ;
}

/**
 * @brief Return the name of a equation
 *
 * @warning the string returned by this function points to a global variable.
 * The caller is responsible to directly use the string or its content.
 * Use ctr_copyequname() to get the name of a equation in a more durable way.
 *
 * @param ctr  the model
 * @param ei   the equation index
 *
 * @return     the equation name
 */
const char* ctr_printequname2(const Container *ctr, rhp_idx ei)
{
   if (!valid_ei(ei)) {
      strncpy(bufequ2, badidx_str(ei), sizeof(bufequ2)-1);
      return bufequ2;
   }

   int status = ctr_copyequname(ctr, ei, bufequ2, sizeof(bufequ2));
   if (status != OK) {
      snprintf(bufequ2, sizeof(bufequ2), "error %s (%d) while querying equation "
               "name with index %u", rhp_status_descr(status), status, ei);
   }

   return bufequ2;
}

int ctr_setequrhs(Container *ctr, rhp_idx ei, double val)
{

   /*  TODO(Xhub) check for valid equation */
   Equ *e = &ctr->equs[ei];

   switch (e->cone) {
   case CONE_R_PLUS:
   case CONE_R_MINUS:
   case CONE_R:
   case CONE_0:
      return ctr_setcst(ctr, ei, -val);
   case CONE_POLYHEDRAL:
   case CONE_SOC:
   case CONE_RSOC:
   case CONE_EXP:
   case CONE_DEXP:
   case CONE_POWER:
   case CONE_DPOWER:
      error("%s :: trying to set RHS on the conical constraint %s of type %s\n",
            __func__, ctr_printequname(ctr, ei), cone_name(e->cone));
      return Error_InvalidValue;
   case CONE_NONE:
   case __CONES_LEN:
   default:
      error( "%s :: equation %s has an invalid cone %u\n", __func__,
            ctr_printequname(ctr, ei), e->cone);
      return Error_InvalidValue;
   }

}


void ctr_memclean(struct ctrmem *ctrmem)
{
   if (ctrmem && ctrmem->ctr) {
      ctr_relmem(ctrmem->ctr);
   }
}

int ctr_fixedvars(Container *ctr)
{
  Avar *fv = ctr->fixed_vars;
  if (!fv) { /* unconditionally created */ 
    error("%s :: fixed_vars is NULL!\n", __func__);
    return Error_NullPointer;
  }

  avar_empty(fv);
  

  IdxArray dat;
  rhp_idx_init(&dat);

  for (size_t i = 0, n = ctr_nvars_total(ctr); i < n; ++i) {
    Var *v = &ctr->vars[i];
    if (v->is_deleted) continue;
    bool bnd_fixed = (v->type == VAR_X || v->type == VAR_B || v->type == VAR_I) &&
      (v->bnd.lb == v->bnd.ub);

    if (bnd_fixed || v->type == RHP_BASIS_FIXED) {
      S_CHECK(rhp_idx_add(&dat, i));
    }
  }

   if (dat.len > 0) {
      avar_setandownsortedlist(fv, dat.len, dat.arr);
   } else {
      avar_setcompact(fv, 0, IdxNA);
   }

  return OK;
}

int ctr_markequasflipped(Container *ctr, Aequ *e)
{
   return aequ_extendandown(&ctr->transformations.flipped_equs, e);
}


