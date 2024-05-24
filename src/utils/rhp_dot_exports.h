#ifndef RHP_DOT_EXPORTS_H
#define RHP_DOT_EXPORTS_H

#include "rhp_fwd.h"

int dot2png(const char *fname) NONNULL;
int view_png(const char *fname, Model *mdl) NONNULL;
int view_equ_as_png(Model *mdl, rhp_idx ei) NONNULL;

#endif
