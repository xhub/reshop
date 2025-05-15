#ifndef RHP_DOT_PNG_H
#define RHP_DOT_PNG_H

#include "rhp_compiler_defines.h"

int dot2png(const char *fname) NONNULL;
int view_png(const char *fname, const char *png_viewer) NONNULL_AT(1);

#endif
