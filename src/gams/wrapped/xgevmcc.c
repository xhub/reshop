#include "compat.h"
#include "gams_utils.h"

#if defined(__clang__)
#pragma clang diagnostic push
#pragma clang diagnostic ignored "-Wdocumentation"
#endif


#define GEV_MAIN
#include <stddef.h>
#include "gevmcc.h"
#include "apifiles/C/api/gevmcc.c"

#if defined(__clang__)
#pragma clang diagnostic pop
#endif
