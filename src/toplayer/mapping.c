#include "mapping.h"
#include "ctr_rhp.h"
#include "equvar_helpers.h"

bool valid_map(Container *ctr, Mapping *m)
{
   assert(ctr_is_rhp(ctr));
   if (valid_vi(m->vi_map) && !valid_vi_(m->vi_map, rctr_totaln(ctr), __func__)) { return false; }

   if (!valid_ei(m->ei)) { return true; }

   if (!valid_ei_(m->ei, rctr_totalm(ctr), __func__)) { return false; }

   EquObjectType role = ctr->equs[m->ei].object;
   return (valid_vi(m->vi_map) && (role == EQ_CONE_INCLUSION) && isfinite(m->coeff)) ||
          (!valid_vi(m->vi_map) && (role == EQ_MAPPING));
}


