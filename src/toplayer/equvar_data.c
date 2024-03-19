#include "equvar_data.h"
#include "rhpidx.h"

const char* aequvar_typestr(enum a_equvar_type type)
{
   switch (type) {
   case EquVar_Compact:
      return "compact";
   case EquVar_List:
      return "list";
   case EquVar_SortedList:
      return "sorted list";
   case EquVar_Block:
      return "block";
   case EquVar_Unset:
      return "unset";
   default:
      return "error";
   }
}

const char * badidx_str(rhp_idx idx)
{
   switch (idx) {
   case IdxInvalid:
      return "Invalid index";
   case IdxNA:
      return "Not Available (NA)";
   case IdxNotFound:
      return "No data found";
   case IdxDeleted:
      return "Data pointed by index was deleted";
   case IdxOutOfRange:
      return "Index is out of range";
   case IdxMaxValid:
      return "Maximum valid index";
   case IdxEmpDagChildNotFound:
      return "Child of EMPdag object not found";
   default:
      return "Index value not documented";
   }
}

const char* basis_name(BasisStatus basis)
{
   switch (basis) {
   case BasisUnset:
      return "unset";
   case BasisLower:
      return "lower bound";
   case BasisUpper:
      return "upper bound";
   case BasisBasic:
      return "basic";
   case BasisSuperBasic:
      return "super basic";
   case BasisFixed:
      return "fixed";
   default:
      return "ERROR invalid";
   }
}

#include "reshop.h"

_Static_assert((uint32_t)RHP_BASIS_UNSET == (uint32_t)BasisUnset, "");
_Static_assert((uint32_t)RHP_BASIS_LOWER == (uint32_t)BasisLower, "");
_Static_assert((uint32_t)RHP_BASIS_UPPER == (uint32_t)BasisUpper, "");
_Static_assert((uint32_t)RHP_BASIS_BASIC == (uint32_t)BasisBasic, "");
_Static_assert((uint32_t)RHP_BASIS_SUPERBASIC == (uint32_t)BasisSuperBasic, "");
_Static_assert((uint32_t)RHP_BASIS_FIXED == (uint32_t)BasisFixed, "");
