#ifndef NASH_H
#define NASH_H

#include "compat.h"
#include "rhp_fwd.h"

Nash *nash_dup(const Nash *nash_src, Model *mdl) NONNULL MALLOC_ATTR(nash_free, 1);
int nash_ve_full(Nash* nash) NONNULL;
int nash_ve_partial(Nash* nash, Aequ *aeqn) NONNULL;

#endif
