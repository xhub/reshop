#ifndef RESHOP_EQUVAR_HELPERS_H
#define RESHOP_EQUVAR_HELPERS_H

#include <stdbool.h>

#include "rhpidx.h"
#include "status.h"

void invalid_ei_errmsg(rhp_idx ei, rhp_idx m, const char * fn);
void invalid_vi_errmsg(rhp_idx vi, rhp_idx n, const char * fn);

static inline bool valid_ei_(rhp_idx ei, rhp_idx m, const char * const fn) {
   bool res = (valid_ei(ei) && ei < m) ? true : false;
   if (!res) {
      invalid_ei_errmsg(ei, m, fn);
   }
   return res;
}

static inline bool valid_vi_(rhp_idx vi, rhp_idx n, const char * const fn) {
   bool res = (valid_vi(vi)  && vi < n) ? true : false;
   if (!res) {
      invalid_vi_errmsg(vi, n, fn);
   }
   return res;
}

static inline int ei_inbounds(rhp_idx ei, rhp_idx m, const char * const fn)
{
   if (!valid_ei_(ei, m, fn)) {
      return Error_IndexOutOfRange;
   }
   return OK;
}

static inline int vi_inbounds(rhp_idx vi, rhp_idx n, const char * const fn)
{
   if (!valid_vi_(vi, n, fn)) {
      return Error_IndexOutOfRange;
   }
   return OK;
}

#endif /* RESHOP_EQUVAR_HELPERS_H  */
