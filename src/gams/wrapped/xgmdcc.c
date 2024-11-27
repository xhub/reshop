#include "compat.h"
#include "gams_utils.h"

#if defined(__clang__)
#pragma clang diagnostic push
#pragma clang diagnostic ignored "-Wdocumentation"
#endif


#define GMD_MAIN
#include <stddef.h>
#include "gmdcc.h"
#include "apifiles/C/api/gmdcc.c"

#if defined(__clang__)
#pragma clang diagnostic pop
#endif
