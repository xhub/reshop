#ifndef RESHOP_CHK_IDX_H
#define RESHOP_CHK_IDX_H

#include "compat.h"
#include "rhp_fwd.h"

int chk_arg_nonnull(const void *arg, unsigned nb, const char *fn);
int chk_avar(const Avar *v, const char *fn);
int chk_aequ(const Aequ *e, const char *fn);
int chk_aequ_nonnull(const Aequ *e, const char *fn);
int chk_ei(const Model *mdl, rhp_idx ei, const char *fn) NONNULL;
int chk_vi(const Model *mdl, rhp_idx vi, const char *fn) NONNULL;
int chk_mdl(const Model *mdl, const char *fn) NONNULL_AT(2);
int chk_mp(const MathPrgm *mp, const char *fn) NONNULL_AT(2);
int chk_mpe(const Mpe *mpe, const char *fn) NONNULL_AT(2);
int chk_var_isnotconic(const Var *v, const Container *ctr,
                       const char *fn) NONNULL;
int chk_uint2int(unsigned v, const char *fn);

#endif
