#include <string.h>

#include "print_utils.h"
#include "printout.h"

void printinfo(struct lineppty *l, const char *str)
{
   printout(l->mode, "%*s%s", l->ident, "", str);
}

void printdbl(struct lineppty *l, const char *str, double num)
{
   if (num == 0.) { return; }

   unsigned slen = strlen(str);
   if (l->colw < l->ident + slen + 6) { return; } //silent failure
   unsigned lennum = l->colw - l->ident - slen;

   printout(l->mode, "%*s%s%*.3f\n", l->ident, "", str, lennum, num);
}

void printuint(struct lineppty *l, const char *str, unsigned num)
{
   if (num == 0) { return; }

   unsigned slen = strlen(str);
   if (l->colw < l->ident + slen + 8) { return; } //silent failure
   unsigned lennum = l->colw - l->ident - slen;

   printout(l->mode, "%*s%s%*u\n", l->ident, "", str, lennum, num);
}
