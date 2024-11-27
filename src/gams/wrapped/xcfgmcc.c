#include "compat.h"
#include "gams_utils.h"

#if defined(__clang__)
#pragma clang diagnostic push
#pragma clang diagnostic ignored "-Wdocumentation"
#endif

#define CFG_MAIN
#include <stddef.h>
#include "cfgmcc.h"
#include "apifiles/C/api/cfgmcc.c"

#if defined(__clang__)
#pragma clang diagnostic pop
#endif
