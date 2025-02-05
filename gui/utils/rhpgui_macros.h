#ifndef RHPGUI_MACROS_H
#define RHPGUI_MACROS_H

#include <stdlib.h>

#include "rhp_compiler_defines.h"

#define MALLOC_GUI(ptr,type,n) { ((ptr) = (type*)malloc((n)*sizeof(type))); \
   assert(((ptr) || n == 0) && "Allocation error for object " #ptr); \
   if (RHP_UNLIKELY(!(ptr))) { return 1; } \
}


#endif // RHPGUI_MACROS_H

