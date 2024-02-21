#ifndef RMDL_PRIV_H
#define RMDL_PRIV_H

#include <assert.h>

#include "equvar_helpers.h"
#include "mdl.h"
#include "rhp_fwd.h"

void rmdl_getobjequ_nochk(const Model *mdl, rhp_idx *objequ) NONNULL;

static inline Equ * rmdl_getequ(const Model *mdl, rhp_idx ei)
{
   assert(mdl_is_rhp(mdl) && ei_inbounds(ei, ctr_nequs_total(&mdl->ctr), __func__) == OK);

   return &mdl->ctr.equs[ei];
}


#endif
