#include <math.h>
#include <stdbool.h>
#include <stdio.h>

#include "macros.h"
#include "printout.h"
#include "status.h"

static inline NONNULL int pprint_f(double d, FILE *f, bool need_sign)
{
   if (d == 0 || (fabs(d) >= 0.01 && fabs(d) < 10000)) {
      if (need_sign) {
         IO_CALL(fprintf(f, " %+.2f ", d));
      } else {
         IO_CALL(fprintf(f, " %.2f ", d));
      }
   }  else if (!isfinite(d)) {
      IO_CALL(fprintf(f, " %E ", d));
   } else {
      int exponent  = (int)lrint(floor(log10( fabs(d))));
      double base   = d * pow(10, -1.0*exponent);
      if (need_sign) {
         IO_CALL(fprintf(f, " %+.2lf10^{%d} ", base, exponent));
      } else {
         IO_CALL(fprintf(f, " %.2lf10^{%d} ", base, exponent));
      }
   }

   return OK;
}


