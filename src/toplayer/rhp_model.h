#ifndef RHP_MODEL_H
#define RHP_MODEL_H

#include "compat.h"
#include "rhp_fwd.h"

int rmdl_exportmodel(Model *mdl, Model *mdl_solver,
                    Fops *fops) NONNULL_AT(1,2);

#endif
