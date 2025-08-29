#include "asprintf.h"


#ifdef __GNUC__
int vscprintf(const char *format, va_list ap)
{
    va_list ap_copy;
    va_copy(ap_copy, ap);
    int retval = vsnprintf(NULL, 0, format, ap_copy);
    va_end(ap_copy);
    return retval;
}
#endif

#if defined(_WIN32) && !defined(__CYGWIN__)
int vasprintf(char **strp, const char *format, va_list ap)
{
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
    return retval;
}

int asprintf(char **strp, const char *format, ...)
{
    va_list ap;
    va_start(ap, format);
    int retval = vasprintf(strp, format, ap);
    va_end(ap);
    return retval;
}

#endif


