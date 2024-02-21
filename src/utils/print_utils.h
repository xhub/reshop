#ifndef PRINT_UTILS_H
#define PRINT_UTILS_H

#include "compat.h"


struct lineppty {
   unsigned mode;
   unsigned colw;
   unsigned ident;
};

NONNULL void printdbl(struct lineppty *l, const char *str, double num);
NONNULL void printinfo(struct lineppty *l, const char *str);
NONNULL void printuint(struct lineppty *l, const char *str, unsigned num);

#endif
