#ifndef EMPINTERP_UTILS_H
#define EMPINTERP_UTILS_H

#include "compat.h"
#include "empinterp_fwd.h"

int genlabelname(DagRegisterEntry * restrict entry, Interpreter *interp,
                 char **labelname) NONNULL;
OWNERSHIP_RETURNS
DagRegisterEntry* regentry_new(const char *basename, unsigned basename_len, 
                               uint8_t dim) NONNULL;
OWNERSHIP_RETURNS
DagRegisterEntry* regentry_dup(DagRegisterEntry *regentry) NONNULL;

int runtime_error(unsigned linenr);
int interp_ops_is_imm(Interpreter *interp) NONNULL;
int has_longform_solve(Interpreter *interp) NONNULL;

#endif // !EMPINTERP_UTILS_H
