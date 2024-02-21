#ifndef CTR_GAMS_H
#define CTR_GAMS_H

#include "mdl_data.h"
#include "rhp_fwd.h"

/** @file ctr_gams.h */

int gams_chk_ctr(const Container *ctr, const char *fn);

int gctr_getsense(const Container *ctr, RhpSense *objsense) NONNULL;
int gctr_genopcode(Container *ctr, rhp_idx ei, int *codelen, int **instrs, int **args) NONNULL;
int gctr_getopcode(Container *ctr, rhp_idx ei, int *codelen, int **instrs, int **args) NONNULL;

#endif
