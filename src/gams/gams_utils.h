#ifndef GAMS_UTILS_H
#define GAMS_UTILS_H

#include "compat.h"

void gams_fix_equvar_names(char *name);
void gams_fix_quote_names(const char *name, char *out);

int gams_chk_str(const char *str, const char *fn);
int gams_opcode_var_to_cst(int opcode);

const char *gams_equtype_name(int equtype);

#endif
