#include "compat.h"
#include "gams_utils.h"

#if defined(__clang__)
#pragma clang diagnostic push
#pragma clang diagnostic ignored "-Wdocumentation"
#endif


#define DCT_MAIN
#include <stddef.h>
#include "dctmcc.h"
#include "apifiles/C/api/dctmcc.c"

#if defined(__clang__)
#pragma clang diagnostic pop
#endif
