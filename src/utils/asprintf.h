#ifndef ASPRINTF_H
#define ASPRINTF_H

/* needed for (v)asprintf, affects '#include <stdio.h>' */
#include "reshop_config.h"

#include <stdio.h>  /* needed for vsnprintf    */
#include <stdlib.h> /* needed for malloc, free */
#include <stdarg.h> /* needed for va_*         */

#include "compat.h"

#ifdef _MSC_VER
#define vscprintf _vscprintf
int vasprintf(char **strp, const char *format, va_list ap);
int asprintf(char **strp, const char *format, ...);
#else
int vscprintf(const char *format, va_list ap) FORMAT_CHK(1,0);
#endif

#endif // ASPRINTF_H
