#ifndef CTR_COMMON_H
#define CTR_COMMON_H

#include "compat.h"
#include <stdbool.h>

struct equvar_pair {
   rhp_idx equ;
   rhp_idx var;
   int cost;
};

struct equvar_eval {
   bool *var2evals;
   unsigned len;
   unsigned max;
   struct equvar_pair *pairs;
};

#endif
