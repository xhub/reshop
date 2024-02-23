#ifndef EMPDAG_PRIV_H
#define EMPDAG_PRIV_H

#include <stdbool.h>

#include "empdag_data.h"
#include "rhp_fwd.h"

/* --------------------------------------------------------------------------
 * Edge related functions
 * -------------------------------------------------------------------------- */

bool valid_uid_(const EmpDag *empdag, daguid_t uid, const char* fn);

#endif
