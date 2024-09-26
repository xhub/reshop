#include "equ_data.h"

const char * const _equ_type_name[] = {
   "unset",
   "mapping",
   "defined mapping",
   "cone inclusion",
   "boolean",
};

const char *equtype_name(unsigned type)
{
   if (type < EquTypeUnsupported) {
      return _equ_type_name[type];
   }

   return "ERROR unsupported";
}
