#include "compat.h"

#include "gclgms.h"

#if defined(GAMSMAJOR) && GAMSMAJOR < 44
#include "apifiles/C/api/gclgms.c"
#endif
