#include "mdl.h"
#include "mdl_data.h"
#include "reshop.h"

/**
 * @brief Get a textual description of a basis status
 *
 * @ingroup publicAPI
 *
 * @param basis  the basis status
 *
 * @return       the textual description
 */
const char* rhp_basis_str(enum rhp_basis_status basis)
{
  switch (basis) {
  case RhpBasisLower:
    return "lower";
  case RhpBasisUpper:
    return "upper";
  case RhpBasisBasic:
    return "basic";
  case RhpBasisSuperBasic:
    return "superbasic";
  case RhpBasisUnset:
    return "unset";
  case RhpBasisFixed:
    return "fixed";
  default:
   return "ERROR: invalid basis value";
  }
}

/**
 * @brief Get a textual description of a backend type
 *
 * @ingroup publicAPI
 *
 * @param backend  the backend type
 *
 * @return         the textual description
 */
const char* rhp_backend_str(enum rhp_backendtype backend)
{
   return backend2str(backend);
}
