#ifndef RHP_DOT_EXPORTS_H
#define RHP_DOT_EXPORTS_H

#include "rhp_fwd.h"

int view_png_mdl(const char *fname, Model *mdl) NONNULL;
int view_equ_as_png(Model *mdl, rhp_idx ei) NONNULL;
int dot_export_equs(Model *mdl) NONNULL;

#endif
