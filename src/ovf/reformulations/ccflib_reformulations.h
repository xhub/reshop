#ifndef CCFLIB_REFORMULATION_H
#define CCFLIB_REFORMULATION_H

#include "rhp_fwd.h"
#include "ovfinfo.h"

int ccflib_equil(Model *mdl) NONNULL;
int ccflib_dualize_fenchel_empdag(Model *mdl, CcflibPrimalDualData *ccfprimaldualdat) NONNULL;


#endif 
