#include "asprintf.h"


#if defined(_WIN32) && !defined(__CYGWIN__)
int asprintf(char **strp, const char *format, ...)
{
    va_list ap;
    va_start(ap, format);
    int len = vsnprintf(NULL, 0, format, ap);
    va_end(ap);

    if (len < 0) { return len; }

    len++;
    char *str = (char*)malloc((size_t) len);
    if (!str) { return -1; };

    va_start(ap, format);
    int retval = vsnprintf(str, (size_t)len, format, ap);
    va_end(ap);

    if (retval < 0) {
        free(str);
        return retval;
    }

    *strp = str;
    return retval;
}

#endif


