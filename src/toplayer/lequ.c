#include <assert.h>
#include <limits.h>
#include <math.h>
#include <stdio.h>
#include <string.h>

#include "compat.h"
#include "consts.h"
#include "lequ.h"
#include "macros.h"
#include "status.h"
#include "printout.h"
#include "var.h"

#include "dctmcc.h"

DBGUSED static inline bool _all_finite(size_t len, const double *vals)
{
   for (size_t i = 0; i < len; ++i) {
      if (!isfinite(vals[i])) {
         return false;
      }
   }

   return true;
}


UNUSED static bool lequ_chk_idxs(Lequ *le, Avar *v) {
   switch (v->type) {
   case EquVar_Compact:
      for (size_t i = 0; i < v->size; ++i) {
         bool res = lequ_debug_idx(le, v->start + i);
         if(!res) { return res; }
      }
      break;
   case EquVar_List:
      for (size_t i = 0; i < v->size; ++i) {
         bool res = lequ_debug_idx(le, v->list[i]);
         if(!res) { return res; }
      }
      break;
   default:
      return false;
   }

   return true;
}

Lequ *lequ_alloc(int maxlen)
{
   Lequ *lequ;

   MALLOC_NULL(lequ, Lequ, 1);

   lequ->max = MAX(maxlen, 1);
   lequ->len = 0;
   lequ->coeffs = NULL; /* Just in case the first alloc fails  */

   MALLOC_EXIT_NULL(lequ->vis, rhp_idx, lequ->max);
   MALLOC_EXIT_NULL(lequ->coeffs, double, lequ->max);

   return lequ;

_exit:
   FREE(lequ->vis);
   FREE(lequ->coeffs);
   FREE(lequ);
   return NULL;
}

void lequ_dealloc(Lequ *lequ)
{
   if (!lequ) return;

   FREE(lequ->vis);
   FREE(lequ->coeffs);
   FREE(lequ);
}

int lequ_add(Lequ *lequ, rhp_idx vi, double value)
{
   assert(lequ_debug_idx(lequ, vi) && isfinite(value));

   S_CHECK(lequ_reserve(lequ, lequ->len + 1));

   lequ->vis[lequ->len] = vi;
   lequ->coeffs[lequ->len] = value;
   lequ->len++;

   return OK;
}

int lequ_add_unique(Lequ *lequ, rhp_idx vi, double value)
{
   assert(isfinite(value));

   for (unsigned i = 0; i < lequ->len; ++i) {
      if (lequ->vis[i] == vi) {
         lequ->coeffs[i] += value;
         return OK;
      }
   }

   S_CHECK(lequ_reserve(lequ, lequ->len + 1));

   lequ->vis[lequ->len] = vi;
   lequ->coeffs[lequ->len] = value;
   lequ->len++;

   return OK;
}

/**
 * @brief Add a number of variables to an equation
 *
 * @param lequ    the equation
 * @param v       the variables
 * @param values  the values (or coefficients) of the variables
 *
 * @return        the error code
 */
int lequ_adds(Lequ * restrict lequ, Avar * restrict v, const double * restrict values)
{
   unsigned len = v->size;
   unsigned old_len = lequ->len;
   S_CHECK(lequ_reserve(lequ, old_len + len));

   assert(lequ_chk_idxs(lequ, v) && _all_finite(len, values));


   switch (v->type) {
   case EquVar_Compact:
      for (size_t i = 0, j = old_len; i < len; ++i, ++j) {
         lequ->vis[j] = v->start + i;
      }
      break;
   case EquVar_List:
      memcpy(&lequ->vis[old_len], v->list, len*sizeof(int));
      break;
   default:
      TO_IMPLEMENT("Block Var");
   }

   memcpy(&lequ->coeffs[old_len], values, len*sizeof(double));

   lequ->len += len;

   return OK;
}

/** 
 *  @brief copy the values from  lequ to another
 *
 *  The destination lequ must have been allocated and needs to be of the right
 *  size
 *
 *  @param dest  the destination object
 *  @param src   the source object
 *
 */
int lequ_copy(Lequ *dest, const struct lequ *src)
{
   assert(_all_finite(src->len, src->coeffs));

   if (dest->max < src->len) {
      error("%s :: the destination length is not large enough: "
                         "%d < %d\n", __func__, dest->max, src->len);
      return Error_InvalidValue;
   }

   memcpy(dest->vis, src->vis, src->len*sizeof(int));
   memcpy(dest->coeffs, src->coeffs, src->len*sizeof(double));

   dest->len = src->len;

   return OK;
}

/** 
 *  @brief copy the values from  lequ to another
 *
 *  The destination lequ must have been allocated and needs to be of the right
 *  size
 *
 *  @param dest   the destination object
 *  @param src    the source object
 *  @param vi_no  the variable to not copy
 *
 */
int lequ_copy_except(Lequ * restrict dest, const Lequ * restrict src, rhp_idx vi_no)
{
   if (dest->max < src->len) {
      error("%s :: the destination length is not large enough: "
                         "%d < %d\n", __func__, dest->max, src->len);
      return Error_InvalidValue;
   }

   double val;
   unsigned pos;
   S_CHECK(lequ_find(src, vi_no, &val, &pos));

   if (pos == UINT_MAX) {
      error("%s :: Could not find variable index %d\n", __func__, vi_no);
      return Error_NotFound;
   }


   memcpy(dest->vis, src->vis, pos*sizeof(int));
   memcpy(dest->coeffs, src->coeffs, pos*sizeof(double));

   unsigned len = src->len - pos - 1;
   memcpy(&dest->vis[pos], &src->vis[pos+1], len*sizeof(int));
   memcpy(&dest->coeffs[pos], &src->coeffs[pos+1], len*sizeof(double));

   dest->len = src->len-1;

   assert(_all_finite(dest->len, dest->coeffs));
   return OK;
}

/** 
 *  @brief Duplicate an Lequ with translation
 *
 *  The destination lequ must have been allocated and needs to be of the right
 *  size
 *
 *  @param dest  the destination object
 *  @param src   the source object
 *
 */
Lequ* lequ_dup_rosetta(const Lequ * restrict src, const rhp_idx * restrict rosetta)
{
   assert(_all_finite(src->len, src->coeffs));

   unsigned len = src->len;
   Lequ *dst;
   AA_CHECK(dst, lequ_alloc(len));

   memcpy(dst->coeffs, src->coeffs, len*sizeof(double));

   for (unsigned i = 0; i < len; ++i) {
      assert(valid_vi(src->vis[i]));
      dst->vis[i] = rosetta[src->vis[i]];
   }

   dst->len = len;

   return dst;
}

void lequ_delete(Lequ *le, size_t pos)
{
   assert(pos < le->len);
   if(pos >= le->len) return;

   /*  Already reduce the size of the linear equation */
   le->len--;
   size_t len = le->len - pos;
   memmove(&le->coeffs[pos], &le->coeffs[pos+1], len * sizeof(double));
   memmove(&le->vis[pos], &le->vis[pos+1], len * sizeof(rhp_idx));
}

int lequ_find(const Lequ *lequ, rhp_idx vi, double *value, unsigned *pos)
{
   for (unsigned i = 0; i < lequ->len; ++i) {
      if (lequ->vis[i] == vi) {
         *value = lequ->coeffs[i];
         *pos = i;
         return OK;
      }
   }

   *value = SNAN;
   *pos = UINT_MAX;

   return OK;
}

void lequ_print(const Lequ *lequ, unsigned mode)
{
   printout(mode, "\t\tLinear terms:\n");
   for (unsigned i = 0; i < lequ->len; i++) {
      printout(mode, "\t\t  Var[%5u]: %.5e\n",
               lequ->vis[i], lequ->coeffs[i]);
   }
}

/**
 *  @brief  ensure that object can store at least a certain number
 *
 *  @param lequ    the linear equation
 *  @param maxlen  the number of element that should be stored
 *
 *  @return        the error code
 */
int lequ_reserve(Lequ *lequ, unsigned maxlen)
{
   if (lequ->max < maxlen) {
      lequ->max = MAX(2*lequ->max, maxlen);
      REALLOC_(lequ->vis, int, lequ->max);
      REALLOC_(lequ->coeffs, double, lequ->max);
   }

   return OK;
}

int lequ_scal(Lequ *lequ, double coeff)
{
   assert(isfinite(coeff));
   /* TODO(xhub) if large lequ, use cblas  */
   double *vals = lequ->coeffs;
   for (unsigned i = 0, len = lequ->len; i < len; ++i) {
      vals[i] *= coeff;
   }

   return OK;
}
