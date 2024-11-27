#include "compat.h"

#if defined(__clang__)
#pragma clang diagnostic push
#pragma clang diagnostic ignored "-Wdocumentation"
#endif

#define GMO_MAIN
#include <stddef.h>
#include "gmomcc.h"
#include "apifiles/C/api/gmomcc.c"

#if defined(__clang__)
#pragma clang diagnostic pop
#endif
