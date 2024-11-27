#include "compat.h"
#include "gams_utils.h"

#if defined(__clang__)
#pragma clang diagnostic push
#pragma clang diagnostic ignored "-Wdocumentation"
#endif


#define OPT_MAIN
#include <stddef.h>
#include "optcc.h"
#include "apifiles/C/api/optcc.c"

#if defined(__clang__)
#pragma clang diagnostic pop
#endif
