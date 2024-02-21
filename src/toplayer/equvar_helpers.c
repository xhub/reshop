#include <assert.h>

#include "equvar_helpers.h"
#include "printout.h"


void invalid_vi_errmsg(rhp_idx vi, rhp_idx n, const char* const fn)
{
  if (!valid_vi(vi)) {
      error("%s ERROR: invalid variable index '%s'\n", fn, badidx_str(vi));
   } else {
      assert(vi >= n);
      error("%s ERROR: variable index %u is outside [0,%u)\n", fn, vi, n);
   }
}

void invalid_ei_errmsg(rhp_idx ei, rhp_idx m, const char* const fn)
{
  if (!valid_ei(ei)) {
      error("%s ERROR: invalid equation index '%s'\n", fn, badidx_str(ei));
   } else {
      assert(ei >= m);
      error("%s ERROR: equation index %u is outside [0,%u)\n", fn, ei, m);
   }
}

