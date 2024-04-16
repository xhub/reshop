#ifndef RHP_OPTION_DATA
#define RHP_OPTION_DATA


#include <stdbool.h>

#include "rhp_fwd.h"
#include "compat.h"

typedef enum {
   Opt_SolveSingleOptAsMcp = 0,
   Opt_SolveSingleOptAsOpt = 1,
} Opt_SolveSingleOptMethod;

const char* optsingleopt_getcurstr(unsigned i);
int optsingleopt_set(struct option *optovf_reformulation, const char *optval);
void optsingleopt_getdata(const char *const (**strs)[2], unsigned *len) NONNULL;
int optsingleopt_getidxfromstr(const char *buf) NONNULL;

#endif
