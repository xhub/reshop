#ifndef EMPPARSER_UTILS_H
#define EMPPARSER_UTILS_H

#include "compat.h"
#include "empinterp_fwd.h"

int parse_gmsindices(Interpreter * restrict interp, unsigned * restrict p,
                     GmsIndicesData * restrict idxdata) NONNULL;
int parse_loopsets(Interpreter * restrict interp, unsigned * restrict p,
                   GmsIndicesData * restrict idxdata) NONNULL;
int parse_labeldefindices(Interpreter * restrict interp, unsigned * restrict p,
                          GmsIndicesData * restrict idxdata);

#endif
