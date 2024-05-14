#ifndef EMPINTERP_OPS_UTILS_H
#define EMPINTERP_OPS_UTILS_H

#include "empinterp_fwd.h"
#include "empparser.h"
#include "gclgms.h"
#include "macros.h"

int resolve_lexeme_as_gmssymb_via_dct(Interpreter * restrict interp, Token * restrict tok) NONNULL;
int get_uelidx_via_dct(Interpreter * restrict interp, const char uelstr[GMS_SSSIZE], int * restrict uelidx) NONNULL;
int get_uelstr_via_dct(Interpreter *interp, int uelidx, unsigned uelstrlen, char *uelstr) NONNULL;

#endif
