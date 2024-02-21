#include "mdl.h"
#include "mdl_data.h"
#include "reshop.h"

const char* rhp_basis_str(enum rhp_basis_status basis)
{
  switch (basis) {
  case RHP_BASIS_LOWER:
    return "lower";
  case RHP_BASIS_UPPER:
    return "upper";
  case RHP_BASIS_BASIC:
    return "basic";
  case RHP_BASIS_SUPERBASIC:
    return "superbasic";
  case RHP_BASIS_UNSET:
    return "unset";
  case RHP_BASIS_FIXED:
    return "fixed";
  default:
   return "ERROR: invalid basis value";
  }
}


