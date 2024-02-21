#ifndef RMDL_DEBUG_H
#define RMDL_DEBUG_H

#include "compat.h"
#include "rhp_fwd.h"

void rmdl_debug_active_vars(Container *ctr) NONNULL;
void rctr_debug_active_equs(Container *ctr) NONNULL;
int rmdl_print(struct rhp_mdl *mdl);


#endif
