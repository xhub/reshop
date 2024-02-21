#ifndef EMPDAG_PRIV_H
#define EMPDAG_PRIV_H

#include <stdbool.h>

#include "empdag_data.h"
#include "rhp_fwd.h"

/* --------------------------------------------------------------------------
 * Edge related functions
 * -------------------------------------------------------------------------- */

int empdag_addedge_vf_byname(EmpDag *empdag, const char *parent,
                             const char *child);
int empdag_addedge_ctrl_byname(EmpDag *empdag, const char *parent,
                               const char *child);
int empdag_addedge_equil_ctrl_byname(EmpDag *empdag, const char *parent,
                                     const char *child);

bool valid_uid_(const EmpDag *empdag, daguid_t uid, const char* fn);

#endif
