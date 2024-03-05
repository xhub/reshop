#include "cmat.h"
#include "container.h"
#include "ctr_rhp.h"
#include "ctrdat_rhp.h"
#include "equ.h"
#include "macros.h"
#include "mdl_rhp.h"
#include "printout.h"
#include "sd_tool.h"
#include "solver_eval.h"
#include "status.h"

struct sort_idx
{
  size_t idx;
  rhp_idx i;
};


#define SORT_NAME rhp
#define SORT_TYPE struct sort_idx
#define SORT_CMP(x, y) ((x).i - (y).i)
#include "sort.h"

static inline NONNULL void sort_jaccol(struct jacdata *restrict jacdata,
                                      struct sort_idx * restrict s,
                                      Equ * restrict ebck,
                                      size_t n)
{
   size_t start = jacdata->p[n];
   size_t len = jacdata->p[n+1] - start;
   memcpy(ebck, &jacdata->equs[start], len * sizeof(struct equ));

   for (size_t i = 0, j = start; i < len; ++i, ++j) {
      s[i].i = jacdata->i[j];
      s[i].idx = i;
   }

   rhp_tim_sort(s, len);

   for (size_t i = 0, j = start; i < len; ++i, ++j) {
     jacdata->i[j] = s[i].i;
     jacdata->equs[j] = ebck[s[i].idx];
   }
}

static inline bool _chk_sort(unsigned *list, size_t len)
{
   rhp_idx prev = list[0];
   for (size_t i = 1; i < len; ++i) {
      if (list[i] <= prev) {
         assert(0);
         return false;
      }
      prev = list[i];
   }

   return true;
}

/**
 * @brief Prepare the data necessary for jacobian computation
 *
 * @param ctr      the container
 * @param jacdata  the data necessary for jacobian computations
 *
 * @return         the error code
 */
int ge_prep_jacdata(Container * restrict ctr, struct jacdata * restrict jacdata)
{
   int status = OK;
   struct ctrmem CTRMEM working_mem = {.ptr = NULL, .ctr = ctr};
   /* ------------------------------------------------------------------
    * TODO(xhub) why is n so important?
    * ------------------------------------------------------------------ */

   struct ctrdata_rhp *model = (struct ctrdata_rhp *) ctr->data;
   size_t total_n = model->total_n;

   jacdata->n = total_n;

   struct sd_tool **adt;

   CALLOC_(adt, struct sd_tool *, ctr->n);

   /* ------------------------------------------------------------------
    * 1. Compute the NNZ of the jacobian
    * ------------------------------------------------------------------ */

   size_t cnt = 0;
   size_t diag_elt = 0;

   for (size_t vi = 0; vi < total_n; ++vi) {
      struct ctr_mat_elt *me = model->vars[vi];

      while (me) {

         /* -----------------------------------------------------------------
          * The model representation may not be ordered
          *
          * TODO(xhub) can this be improved?
          * ----------------------------------------------------------------- */

         if ((size_t)me->ei < total_n) { cnt++; }
         if (me->ei == vi) { diag_elt++; }
         me = me->next_equ;
      }

   }

   if (cnt == 0) {
      error("%s :: the jacobian is empty!\n", __func__);
      status = Error_SolverError;
      goto _exit;
   }

   assert(diag_elt <= total_n);
   cnt += total_n - diag_elt;

   /* ------------------------------------------------------------------
    * 2. Perform the data allocation and prepare the symbolic computation of
    * the derivatives
    * ------------------------------------------------------------------ */

   CALLOC_(jacdata->equs, struct equ, cnt);
   MALLOC_(jacdata->i, unsigned, cnt);
   MALLOC_(jacdata->p, unsigned, total_n+1);

   unsigned * restrict iptr = jacdata->i;
   unsigned * restrict pptr = jacdata->p;
   Equ * restrict equs = jacdata->equs;

   jacdata->nnz = cnt;
   jacdata->nnzmax = cnt;

   for (size_t i = 0; i < total_n; ++i) {
      A_CHECK_EXIT(adt[i], sd_tool_alloc(SDT_ANY, ctr, i));
   }

   /* ------------------------------------------------------------------
    * 3. Compute the equation for each component of the jacobian
    *
    * TODO(xhub) do some optimization based on the type of equation: constant
    * ------------------------------------------------------------------ */

   size_t mem_size = total_n * (sizeof(struct equ) + sizeof(struct sort_idx));
   A_CHECK_EXIT(working_mem.ptr, ctr_getmem(ctr, mem_size));
   struct sort_idx * restrict sort_arr = (struct sort_idx * restrict)working_mem.ptr;
   Equ * restrict ebck = (Equ * restrict)&sort_arr[total_n];

   cnt = 0;
   pptr[0] = 0;
   for (size_t vi = 0; vi < total_n; ++vi) {
      struct ctr_mat_elt *me = model->vars[vi];
      rhp_idx ei_debug = -1; /* TODO: change this. Is 0 a proper value? */
      bool need_sort = false;
      bool need_diag = true;

      while (me) {
         if ((size_t)me->ei < total_n) {
            iptr[cnt] = me->ei;
            equs[cnt].object = Mapping;
            S_CHECK_EXIT(sd_tool_deriv(adt[me->ei], vi, &equs[cnt++]));

            if (me->ei <= ei_debug) {
               need_sort = true;
            }
            if (me->ei == vi) {
              need_diag = false;
            }
            ei_debug = me->ei;
         }
         me = me->next_equ;
      }


      if (need_diag) {
        need_sort = 1;
        equs[cnt].object = Mapping;
        iptr[cnt++] = vi;
      }

      pptr[vi+1] = cnt;
      if (need_sort) {
        sort_jaccol(jacdata, sort_arr, ebck, vi);
      }

      assert(_chk_sort(&iptr[pptr[vi]], cnt - pptr[vi]));
   }

   assert(pptr[total_n] == jacdata->nnz);

_exit:

   for (size_t i = 0; i < ctr->n; ++i) {
      sd_tool_dealloc(adt[i]);
   }
   FREE(adt);

   return status;
}

/**
 * @brief Evaluate all the functional part of a generalised equation at a point
 *
 * @param      ctr  the container
 * @param      x    the current point
 * @param[out] F    the value of the equations at x
 *
 * @return          the number of evaluation error
 */
int ge_eval_func(Container * restrict ctr, double * restrict x, double * restrict F)
{
   /* TODO(Xhub) parallelize  */
   /* TODO(xhub) support constant case AVI with specialised code  */

  int eval_err = 0;
   for (size_t i = 0; i < (size_t)ctr->n; ++i) {
      eval_err += rctr_evalfuncat(ctr, &ctr->equs[i], x, &F[i]);
   }

   return eval_err;
}

/**
 * @brief Evaluate all the jacobian
 *
 * @param ctr      the container
 * @param jacdata  the jacobian data
 * @param x        the current point
 * @param F        the function value
 * @param p        the pointer list
 * @param i        the index list
 * @param vals     the value array
 *
 * @return         the error code
 */
int ge_eval_jacobian(Container *ctr, struct jacdata *jacdata, double *x, double *F, int *p, int *i, double *vals)
{
   /* TODO(Xhub) parallelize  */
   /* TODO(xhub) support constant case AVI with specialized code  */
   for (size_t j = 0; j < jacdata->nnz; ++j) {
      S_CHECK(rctr_evalfuncat(ctr, &jacdata->equs[j], x, &vals[j]));
   }

   return OK;
}

void jacdata_free(struct jacdata *jacdata)
{
   if (jacdata->equs) {
      for (size_t i = 0; i < jacdata->nnz; ++i) {
        equ_free(&jacdata->equs[i]);
      }
      FREE(jacdata->equs);
   }

   FREE(jacdata->i);
   FREE(jacdata->p);

}
