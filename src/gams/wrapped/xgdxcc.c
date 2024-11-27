#include "compat.h"
#include "gams_utils.h"

#if defined(__clang__)
#pragma clang diagnostic push
#pragma clang diagnostic ignored "-Wdocumentation"
#endif


#define GDX_MAIN
#include <stddef.h>
#include "gdxcc.h"
#include "apifiles/C/api/gdxcc.c"

#if defined(__clang__)
#pragma clang diagnostic pop
#endif
