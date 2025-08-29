#include "asprintf.h"


#if defined(_WIN32) && !defined(__CYGWIN__)
#define vscprintf _vscprintf
int asprintf(char **strp, const char *format, ...)
{
    va_list ap;
    va_start(ap, format);
    int len = vscprintf(format, ap);
    if (len < 0) { return len; }

    len++;
    char *str = (char*)malloc((size_t) len);
    if (!str) { return -1; };

    int retval = vsnprintf(str, (size_t)len, format, ap);

    if (retval == -1) {
        free(str);
        return -1;
    }

    *strp = str;
    va_end(ap);
    return retval;
}

#endif


