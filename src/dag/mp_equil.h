#ifndef MP_EQUIL_H
#define MP_EQUIL_H

#include "compat.h"
#include "rhp_fwd.h"

Mpe *mpe_dup(const Mpe *mpe_src, Model *mdl) NONNULL MALLOC_ATTR(mpe_free, 1);
int mpe_ve_full(Mpe* mpe) NONNULL;
int mpe_ve_partial(Mpe* mpe, Aequ *aeqn) NONNULL;

#endif
