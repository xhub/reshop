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
#define vscprintf _vscprintf
int vasprintf(char **strp, const char *format, va_list ap);
int asprintf(char **strp, const char *format, ...);
#else
int vscprintf(const char *format, va_list ap) FORMAT_CHK(1,0);
#endif

#endif // ASPRINTF_H
