#include "asprintf.h"

#include <float.h>
#include <string.h>

#include "container.h"
#include "cmat.h"
#include "ctr_rhp.h"
#include "ctrdat_rhp.h"
#include "empinfo.h"
#include "equvar_helpers.h"
#include "nltree.h"
#include "filter_ops.h"
#include "macros.h"
#include "mdl.h"
#include "rhp_alg.h"
#include "pool.h"
#include "printout.h"
#include "status.h"
#include "timings.h"

int ctr_compress_equs_check(const Container *ctr_src, Container *ctr_dst, size_t skip_equ)
{
   return ctr_compress_equs_check_x(ctr_src, ctr_dst, skip_equ,  ctr_src->fops);
}

int ctr_compress_equs_check_x(const Container *ctr_src, Container *ctr_dst,
                              size_t skip_equ, const Fops *fops)
{
   unsigned total_m;
   CMatElt **deleted_equs;

   if (ctr_is_rhp(ctr_src)) {
      RhpContainerData *cdat_src = (RhpContainerData *)ctr_src->data;
      total_m = cdat_src->total_m;
      deleted_equs = cdat_src->deleted_equs;
   } else if (ctr_src->backend == RHP_BACKEND_GAMS_GMO) {
      total_m = ctr_src->m;
      deleted_equs = NULL;
   } else {
      error("Unsupported model type %s", backend_name(ctr_src->backend));
      return Error_NotImplemented;
   }

   /* We consider that the deactivated equation are pretty well explained */
   ptrdiff_t n_eq = (skip_equ + ctr_dst->m) - total_m;

   if (n_eq > 0) {

      /* ----------------------------------------------------------------
       * Maybe there are equations with no variables, just constants
       * ---------------------------------------------------------------- */

      for (rhp_idx i = 0; i < total_m; ++i) {

         if (fops && !fops->keep_equ(fops->data, i)) {

            /* ----------------------------------------------------------
             * If the equation was deleted, we are happy
             * ---------------------------------------------------------- */

            if (deleted_equs && deleted_equs[i]) {
               n_eq--;
               continue;
            }

            /* ----------------------------------------------------------
             * Try to see if the equation is a vacuous constraint
             * ---------------------------------------------------------- */

            Equ *e = &ctr_src->equs[i];

            if ((!e->lequ || e->lequ->len == 0) /* empty lequ */
                  && (!e->tree || !e->tree->root)) /* empty tree  */ {

               double cst = equ_get_cst(e);

               switch (e->cone) {
               case CONE_0:
                  if (fabs(cst) > DBL_EPSILON) {
                     error("%s :: dummy constraint '%s' is not fulfilled: 0. != %e\n",
                           __func__, ctr_printequname(ctr_src, i), cst);
                     return Error_ModelInfeasible;
                  }
                  break;
               case CONE_R_MINUS:
                  if (0. < cst) {
                     error("%s :: dummy constraint '%s' #%u is not fulfilled: 0. < %e\n",
                           __func__, ctr_printequname(ctr_src, i), i, cst);
                     return Error_ModelInfeasible;
                  }
                  break;
               case CONE_R_PLUS:
                  if (0. > cst) {
                     error("%s :: dummy constraint '%s' #%u is not fulfilled: 0. > %e\n",
                           __func__, ctr_printequname(ctr_src, i), i, cst);
                     return Error_ModelInfeasible;
                  }
                  break;
               case CONE_NONE:
                  error("%s :: nonsensical equation '%s' #%u: 0 ?? %e\n",
                        __func__, ctr_printequname(ctr_src, i), i, cst);
                  return Error_Inconsistency;
               default:
                  error("%s :: equation '%s' #%u is of type %d but has no "
                        "variables. This case is not implemented yet.\n",
                        __func__, ctr_printequname(ctr_src, i), i, e->object);
                  return Error_NotImplemented;
               }

               /* ----------------------------------------------------
                * We reach this point only if we have determine that
                * the equation is indeed a vacuous constraint
                * ---------------------------------------------------- */

               n_eq--;
            }
         }

         /* -------------------------------------------------------------
          * Check whether we have found every missing bits
          * ------------------------------------------------------------- */

         if (n_eq == 0) {
            break;
         }

      } /*  end for loop over all equations */

      /* -------------------------------------------------------------------
       * Now we have a major issue
       * ------------------------------------------------------------------- */

      if (n_eq > 0) {
         error("[container/export] there are %zu inactive equations(s) that "
               "can't be explained, exiting\n", n_eq);
         return Error_Inconsistency;
      }

   } else if (n_eq < 0) {
      error("%s ERROR: the number of active equations in the compressed model is "
            "smaller than expected: %zu vs %u. We don't know how to deal with "
            "this case.\n", __func__, ctr_dst->m - n_eq, ctr_dst->m);
      return Error_Inconsistency;
   }

   /* -------------------------------------------------------------
    * Adjust the number of equation in the destination container
    *
    * TODO(Xhub) resize to free space? Also this is run unconditionally
    * ------------------------------------------------------------- */

   if (ctr_dst->m != total_m - skip_equ) {
      trace_process("[container] Updating the number of equations from %u to %zu\n",
                    ctr_dst->m, total_m - skip_equ);
      ctr_dst->m = total_m - skip_equ;
   }

   return OK;
}

int ctr_compress_vars_check(size_t ctr_n, size_t total_n, size_t skip_var)
{
   /*  TODO(xhub) how to enforce this when we do our own filtering */
   if (skip_var < total_n - ctr_n) {
      error("%s :: number of inactive variable is inconsistent: via "
               "the model representation, there are %zu, via the "
               "model definition %zu as %zu - %zu\n",
               __func__, skip_var, total_n - ctr_n, total_n, ctr_n);
      return Error_Inconsistency;
   }

   return OK;
}




/**
 * @brief Compress (after optional filtering) the variables of a model into a new one
 *
 * @param ctr_src      the original model
 * @param ctr_dst  the destination model
 * @return         the error code
 */
int rctr_compress_vars(const Container *ctr_src, Container *ctr_dst)
{
   return rctr_compress_vars_x(ctr_src, ctr_dst, ctr_src->fops);
}

NONNULL static inline
int copy_vars_nofops(const Container *ctr_src, Container *ctr_dst,
                     rhp_idx *rev_rosetta, size_t ctr_dst_n)
{
   size_t ctr_src_n = ctr_nvars_total(ctr_src);
   rhp_idx *rosetta_vars = ctr_src->rosetta_vars;


   for (rhp_idx i = 0; i < ctr_src_n; ++i) {

      rhp_idx vi = i;
      assert(vi < ctr_dst_n);
      rosetta_vars[i] = vi;
      rev_rosetta[vi] = i;

      S_CHECK(rctr_copyvar(ctr_dst, &ctr_src->vars[i]));
      assert(ctr_dst->vars[vi].idx == vi);

      DPRINT("%s :: new var #%d: lb = %e; ub = %e; value = %e; marginal = %e\n",
            __func__, ctr_dst->vars[vi].idx, ctr_dst->vars[vi].bnd.lb,
             ctr_dst->vars[vi].bnd.ub, ctr_dst->vars[vi].value, ctr_dst->vars[vi].multiplier);
   }

   return OK;
}

NONNULL static inline
int copy_vars_permutation(const Container *ctr_src, Container *ctr_dst,
                          rhp_idx *rev_rosetta, size_t ctr_dst_n, const Fops *fops,
                          rhp_idx *skip_var)
{
   size_t ctr_src_n = ctr_nvars_total(ctr_src);
   rhp_idx *rosetta_vars = ctr_src->rosetta_vars;
   size_t skip_var_ = 0;

   for (rhp_idx i = 0; i < ctr_src_n; ++i) {
      if (!fops->keep_var(fops->data, i)) {
         skip_var_++;
         rosetta_vars[i] = IdxDeleted;
         continue;
      }

      rhp_idx vi = fops->vars_permutation(fops->data, i);
      assert(valid_vi_(vi, ctr_dst_n, __func__));
      rosetta_vars[i] = vi;
      rev_rosetta[vi] = i;

      S_CHECK(rctr_copyvar(ctr_dst, &ctr_src->vars[i]));

      DPRINT("%s :: new var #%d: lb = %e; ub = %e; value = %e; marginal = %e\n",
            __func__, ctr_dst->vars[vi].idx, ctr_dst->vars[vi].bnd.lb,
             ctr_dst->vars[vi].bnd.ub, ctr_dst->vars[vi].value, ctr_dst->vars[vi].multiplier);
   }

   *skip_var = skip_var_;
   return OK;
}

NONNULL static inline
int copy_vars_fops(const Container *ctr_src, Container *ctr_dst,
                   rhp_idx *rev_rosetta, size_t ctr_dst_n, const Fops *fops,
                   rhp_idx *skip_var)
{
   size_t ctr_src_n = ctr_nvars_total(ctr_src);
   rhp_idx *rosetta_vars = ctr_src->rosetta_vars;
   size_t skip_var_ = 0;

   for (rhp_idx i = 0; i < ctr_src_n; ++i) {
      if (!fops->keep_var(fops->data, i)) {
         skip_var_++;
         rosetta_vars[i] = IdxDeleted;
         continue;
      }

      rhp_idx vi = i - skip_var_;
      assert(valid_vi_(vi, ctr_dst_n, __func__));
      rosetta_vars[i] = vi;
      rev_rosetta[vi] = i;

      S_CHECK(rctr_copyvar(ctr_dst, &ctr_src->vars[i]));
      assert(ctr_dst->vars[vi].idx == vi);

      DPRINT("%s :: new var #%d: lb = %e; ub = %e; level = %e; multiplier = %e\n",
            __func__, ctr_dst->vars[vi].idx, ctr_dst->vars[vi].bnd.lb,
             ctr_dst->vars[vi].bnd.ub, ctr_dst->vars[vi].value, ctr_dst->vars[vi].multiplier);
   }

   *skip_var = skip_var_;
   return OK;
}

int rctr_compress_vars_x(const Container *ctr_src, Container *ctr_dst, Fops *fops)
{
   int status = OK;
   size_t ctr_dst_n, ctr_dst_m, ctr_src_n = ctr_nvars_total(ctr_src);

   if (fops) {
      fops->get_sizes(fops->data, &ctr_dst_n, &ctr_dst_m);
   } else {
      ctr_dst_n = ctr_src->n;
      ctr_dst_m = ctr_src->m;
   }

   if (ctr_dst_n == 0) {
      error("%s :: no variables in the destination model!\n", __func__);
      return Error_RuntimeError;
   }

   /* Allocates the data in the destination container  */
   /*  TODO(xhub) PERF is malloc sufficient here? */
   if (!ctr_dst->vars) {

      CALLOC_(ctr_dst->vars, Var, ctr_dst_n);

   } else if (ctr_dst_n > ((RhpContainerData*)ctr_dst->data)->max_n) {

      error("%s ERROR: The variable space is already allocated, but too small: %zu "
            "is needed; %zu is allocated.\n", __func__, ctr_dst_n,
            ((RhpContainerData*)ctr_dst->data)->max_n);
      return Error_UnExpectedData;
   }

   rhp_idx *rev_rosetta, skip_var = 0;
   MALLOC_(rev_rosetta, rhp_idx, ctr_dst_n);

   if (!fops) {
      S_CHECK_EXIT(copy_vars_nofops(ctr_src, ctr_dst, rev_rosetta, ctr_dst_n));
   } else if (fops->vars_permutation) {
      S_CHECK_EXIT(copy_vars_permutation(ctr_src, ctr_dst, rev_rosetta, ctr_dst_n,
                                         fops, &skip_var));
   } else {
      S_CHECK_EXIT(copy_vars_fops(ctr_src, ctr_dst, rev_rosetta, ctr_dst_n,
                                  fops, &skip_var));
   }

   unsigned nvars = ctr_src_n - skip_var;  
   RhpContainerData *ctrdat_dst = (RhpContainerData*)ctr_dst->data;
   avar_setcompact(&ctrdat_dst->var_inherited.v, nvars, 0);
   avar_setandownlist(&ctrdat_dst->var_inherited.v_src, nvars, rev_rosetta);

   return ctr_compress_vars_check(ctr_src->n, ctr_src_n, skip_var);

_exit:
   FREE(rev_rosetta);
   return status;
}

#if 0
int rctr_compress_equs(const Container *ctr_src, Container *ctr_dst)
{
   assert(ctr_is_rhp(ctr_dst));
   RhpContainerData *ctrdat_src = (RhpContainerData *)ctr_src->data;

   Fops *fops = ctrdat_src->fops;

   size_t ctr_dst_n;
   size_t ctr_dst_m;
   if (fops) {
      fops->get_sizes(fops->data, &ctr_dst_n, &ctr_dst_m);
   } else {
      ctr_dst_n = ctr_src->n;
      ctr_dst_m = ctr_src->m;
   }

   if (ctr_dst_m == 0) {
      error("%s :: no equation in the destination model!\n", __func__);
      return Error_RuntimeError;
   }

   if (!ctr_dst->equs) {
      CALLOC_(ctr_dst->equs, struct equ, ctr_dst_m);
   } else if (ctr_dst_m >= ((RhpContainerData*)ctr_src->data)->max_m) {
      error("%s :: The variable space is already allocated, but too small: %zu "
            "is needed; %zu is allocated.\n", __func__, ctr_dst_m,
            ((RhpContainerData*)ctr_src->data)->max_m);
      return Error_UnExpectedData;
   }

   rhp_idx * restrict rosetta_equs = ctr_src->rosetta_equs;
   rhp_idx skip_equ = 0;
   rhp_idx *rev_rosetta;
   MALLOC_(rev_rosetta, rhp_idx, ctr_dst_m);

   for (rhp_idx i = 0, len = ctrdat_src->total_m; i < len; ++i) {
      if (fops && !fops->keep_equ(fops->data, i)) {
         skip_equ++;
         rosetta_equs[i] = IdxDeleted;
         continue;
      }

      rhp_idx ei = i - skip_equ;
      rosetta_equs[i] = ei;
      rev_rosetta[ei] = i;
      assert(ei >= 0 && ei < ctr_dst_m);

      Equ * restrict edst = &ctr_dst->equs[ei];
      const Equ * restrict esrc = &ctr_src->equs[i];
      equ_copymetadata(edst, esrc, ei);

      edst->lequ = NULL;
      edst->tree = NULL;
   }

   unsigned nequs = ctrdat_src->total_m - skip_equ;  
   RhpContainerData *ctrdat_dst = (RhpContainerData*)ctr_dst->data;
   aequ_setcompact(&ctrdat_dst->equ_inherited.e, nequs, 0);
   aequ_setandownlist(&ctrdat_dst->equ_inherited.e_src, nequs, rev_rosetta);

   S_CHECK(rctr_compress_check_equ(ctr_src, ctr_dst, skip_equ));

   return OK;
}
#endif

/** 
 *  @brief Perform the presolve for an optimization problem
 *
 *  This function solves all the optimization problems that were stored as
 *  subproblems. Those are usually used to initialize variables values
 *
 *  @param mdl     the model
 *  @param backend the backend to use 
 *
 *  @return     the error code
 */
int rmdl_presolve(Model *mdl, unsigned backend)
{
   unsigned offset_pool;
   Container *ctr = &mdl->ctr;
   RhpContainerData *cdat = (RhpContainerData *)ctr->data;

   unsigned nb = 0;
   for (unsigned s = 0, cur_stage = cdat->current_stage; s <= cur_stage; ++s) {
      struct auxmdl *s_subctr = &cdat->stage_auxmdl[cur_stage - s];

      nb += s_subctr->len;
   }

   if (nb == 0) {
      return OK;
   }

   double start = get_walltime();

   S_CHECK(ctr_ensure_pool(ctr));

   offset_pool = ctr->pool->len;

   /* ----------------------------------------------------------------------
    * Ensure that the pool size is large enough to hold the values of all variables.
    * ---------------------------------------------------------------------- */

   if (ctr->pool->len + cdat->total_n > ctr->pool->max) {
      unsigned new_max = ctr->pool->len + cdat->total_n;

      if (ctr->pool->own) {
         ctr->pool->max = new_max;
         REALLOC_(ctr->pool->data, double, ctr->pool->max);
      } else {
         S_CHECK(pool_copy_and_own_data(ctr->pool, new_max));
      }
   }

   ctr->pool->len += cdat->total_n;
   double *pool_data = ctr->pool->data;

   for (unsigned vi = 0, i = offset_pool; vi < (unsigned)cdat->total_n; ++vi, ++i) {
      pool_data[i] = ctr->vars[vi].value;
   }

   /* ----------------------------------------------------------------------
    * HACK: save existing "model" data before solving auxiliary models
    * ---------------------------------------------------------------------- */

   EmpDag empdag_orig;
   memcpy(&empdag_orig, &mdl->empinfo.empdag, sizeof(EmpDag));

   ModelType mdltype;
   S_CHECK(mdl_gettype(mdl, &mdltype));

   bool cpy_fops = false;
   Fops fops_orig;
   if (ctr->fops) {
      memcpy(&fops_orig, ctr->fops, sizeof(Fops));
      cpy_fops = true;
   }


   int status = OK;
   Model *mdl_solver = mdl_new(backend);
   Container *ctr_solver = &mdl_solver->ctr;

   const char *mdl_name = mdl_getname(mdl);

  /* ----------------------------------------------------------------------
   * We need to copy this information as it will get overwritten when the
   * stage number is increased in rmdl_prepare_export()
   * ---------------------------------------------------------------------- */

   struct auxmdl *stages_auxmdl;
   unsigned cur_stage = cdat->current_stage;
   MALLOC_(stages_auxmdl, struct auxmdl, cur_stage+1);
   memcpy(stages_auxmdl, cdat->stage_auxmdl, (cur_stage+1) * sizeof(struct auxmdl));

   for (unsigned s = 0; s <= cur_stage; ++s) {
      struct auxmdl *s_subctr = &stages_auxmdl[cur_stage - s];

      /* -------------------------------------------------------------------
       * Descending order
       * ------------------------------------------------------------------- */

      for (unsigned i = 0, j = s_subctr->len-1; i < s_subctr->len; ++i, --j) {
         struct filter_subset* fs = s_subctr->filter_subset[j];
         S_CHECK_EXIT(filter_subset_activate(fs, mdl, offset_pool));

         char *name;
         IO_CALL_EXIT(asprintf(&name, "%s_s%u_i%u", mdl_name, s, i));
         S_CHECK_EXIT(mdl_setname(mdl_solver, name));
         FREE(name);
         /* TODO: add option for selection subsolver */

         mdl_solver->ctr.n = mdl_solver->ctr.m = 0;
         mdl_solver->status = 0;
         mdl_solver->ctr.status = 0;

         trace_process("[model] Presolving model stage %5u iter %5u\n",s, i);
         S_CHECK_EXIT(mdl_export(mdl, mdl_solver));

         /* TODO(Xhub) URG add option for displaying that  */
#if defined(DEBUG)
         strncpy(ctr_solver->data, "CONVERTD", GMS_SSSIZE-1);
         S_CHECK_EXIT(ctr_callsolver(&mdl_solver));
         mdl_setsolvername(mdl_solver, "");
#endif
         
         S_CHECK_EXIT(mdl_solve(mdl_solver));
         /* Phase 1: report the values from the solver to the RHP */
         /* TODO(xhub) optimize and iterate over the valid equations */

         /* We read those here, since they may not exist at the beginning  */
         rhp_idx *rosetta_vars = ctr->rosetta_vars;
         rhp_idx *rosetta_equs = ctr->rosetta_equs;


         struct ctrmem CTRMEM working_mem = {.ptr = NULL, .ctr = ctr};
         size_t arrsize = MAX(ctr_solver->n, ctr_solver->m);
         A_CHECK(working_mem.ptr, ctr_getmem_old(ctr, 2*arrsize*sizeof(double)));

         double * restrict vals = (double*)working_mem.ptr;
         double * restrict mults = &vals[arrsize];

         S_CHECK_EXIT(ctr_getallvarsval(ctr_solver, vals));
         S_CHECK_EXIT(ctr_getallvarsmult(ctr_solver, mults));

         for (unsigned k = 0, l = 0; k < (unsigned)cdat->total_n; ++k) {

            if (valid_vi(rosetta_vars[k])) {
               /* Update the values in the container */
               Var * restrict vdst = &ctr->vars[k];
               vdst->value = vals[l];
               vdst->multiplier = mults[l];

               /* Update the values in the pool */
               ctr->pool->data[offset_pool+k] = vdst->value;

               /*  TODO(xhub) URG why is this commented?/ */
//               update_neg(neg_var_vals, -ctr_solver->vars[l].level)

               ++l;
            }
         }

         S_CHECK_EXIT(ctr_getallequsval(ctr_solver, vals));
         S_CHECK_EXIT(ctr_getallequsmult(ctr_solver, mults));

         for (unsigned k = 0, l = 0; k < (unsigned)cdat->total_m; ++k) {
            if (valid_ei(rosetta_equs[k])) {
               /* Update the values in the container */
               /*  TODO(xhub) is the level useful? */
               Equ * restrict edst = &ctr->equs[k];
               edst->value = vals[l];
               edst->multiplier = mults[l];
               ++l;
            }
         }

         /* TODO(Xhub) this doesn't look good, but now we hardcheck this.
          * Find a way to improve this. If we solve multiple time a subset
          * of a model, we might want to not allocate all the time*/
         FREE(ctr->rosetta_vars);
         FREE(ctr->rosetta_equs);

         /* mdl_export links mdl and mdl_solver */
         mdl_release(mdl);
         mdl_solver->mdl_up = NULL;
      }
   }

   FREE(stages_auxmdl);

   /* ----------------------------------------------------------------------
    * restore the model
    * ---------------------------------------------------------------------- */

   memcpy(&mdl->empinfo.empdag, &empdag_orig, sizeof(EmpDag));
   S_CHECK(mdl_settype(mdl, mdltype));

_exit:
   if (cpy_fops) {
      memcpy(ctr->fops, &fops_orig, sizeof(Fops));
   } else {
      ctr->fops->type = FopsEmpty;
      ctr->fops->data = NULL;
   }

   mdl_release(mdl_solver);

   trace_process("[model] End of presolving for %s model '%.*s' #%u\n", mdl_fmtargs(mdl));
   mdl->timings->solve.presolve_wall = get_walltime() - start;

   return status;
}
