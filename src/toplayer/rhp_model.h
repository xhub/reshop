#ifndef RHP_MODEL_H
#define RHP_MODEL_H

#include "compat.h"
#include "rhp_fwd.h"

int rmdl_exportmodel(Model *mdl, Model *mdl_solver,
                    Fops *fops) NONNULL_AT(1,2);
int rmdl_get_equation_modifiable(Model* mdl, rhp_idx ei, Equ **e) NONNULL;

#endif
