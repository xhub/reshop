#ifndef EMPINTERP_OPS_UTILS_H
#define EMPINTERP_OPS_UTILS_H

#include <stdbool.h>

#include "empinterp_data.h"
#include "empinterp_fwd.h"
#include "gclgms.h"
#include "macros.h"

struct origins {
   IdentOrigin uel;
   IdentOrigin sym;
};

int resolve_lexeme_as_gms_symbol(Interpreter * restrict interp, Token * restrict tok) NONNULL;
int gmd_search_ident(Interpreter * restrict interp, IdentData * restrict ident) NONNULL;
int gmd_boolean_test(void *gmdh, VmGmsSymIterator *filter, bool *res) NONNULL;

int get_uelstr_for_empdag_node(Interpreter *interp, int uelidx, unsigned uelstrlen, char *uelstr) NONNULL;


#ifdef NO_DELETE_2024_12_18
// TODO: delete once newer GMD is available on master
int uelidx_check_or_convert_dct_vs_gmd(gmdHandle_t gmd, dctHandle_t dct, int uelidx, struct origins origins) NONNULL;
int uelidx_check_or_convert(gmdHandle_t gmd, gmdHandle_t gmddct, int uelidx, struct origins origins) NONNULL;
#endif

#endif
