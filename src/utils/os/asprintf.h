#ifndef ASPRINTF_H
#define ASPRINTF_H

// IWYU pragma: always_keep

/* needed for (v)asprintf, affects '#include <stdio.h>' */
#include "reshop_config.h"

#include <stdio.h>  /* needed for vsnprintf    */
#include <stdlib.h> /* needed for malloc, free */
#include <stdarg.h> /* needed for va_*         */

#include "compat.h"

#if defined(_WIN32) && !defined(__CYGWIN__)
int asprintf(char **strp, const char *format, ...)
#if defined(__GNUC__) && !defined(__clang__)
__attribute__ ((format (gnu_printf, 2, 3)))
__attribute__ ((format (ms_printf, 2, 3)))
#endif
;
#endif

#endif // ASPRINTF_H
