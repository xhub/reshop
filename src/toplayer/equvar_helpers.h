#ifndef RESHOP_EQUVAR_HELPERS_H
#define RESHOP_EQUVAR_HELPERS_H

#include <stdbool.h>

#include "rhpidx.h"
#include "status.h"

void invalid_ei_errmsg(rhp_idx ei, rhp_idx m, const char * fn);
void invalid_vi_errmsg(rhp_idx vi, rhp_idx n, const char * fn);

static inline bool chk_ei_(rhp_idx ei, rhp_idx nequs)
{
   return (valid_ei(ei) && ei < nequs);
} 

static inline bool chk_vi_(rhp_idx vi, rhp_idx nvars)
{
   return (valid_vi(vi) && vi < nvars);
}

static inline bool valid_ei_(rhp_idx ei, rhp_idx m, const char * const fn)
{
   if (RHP_UNLIKELY(!chk_ei_(ei, m))) {
      invalid_ei_errmsg(ei, m, fn);
      return false;
   }
   return true;
}

static inline bool valid_vi_(rhp_idx vi, rhp_idx n, const char * const fn)
{
   if (RHP_UNLIKELY(!chk_vi_(vi, n))) {
      invalid_vi_errmsg(vi, n, fn);
      return false;
   }

   return true;
}

static inline int ei_inbounds(rhp_idx ei, rhp_idx m, const char * const fn)
{
   if (RHP_UNLIKELY(!valid_ei_(ei, m, fn))) {
      return Error_IndexOutOfRange;
   }

   return OK;
}

static inline int vi_inbounds(rhp_idx vi, rhp_idx n, const char * const fn)
{
   if (RHP_UNLIKELY(!valid_vi_(vi, n, fn))) {
      return Error_IndexOutOfRange;
   }

   return OK;
}

#endif /* RESHOP_EQUVAR_HELPERS_H  */
