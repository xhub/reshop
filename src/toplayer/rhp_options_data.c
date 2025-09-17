#include "macros.h"
#include "rhp_options_data.h"
#include "option.h"
#include "printout.h"

static const char *const singleopt_names[][2] = {
   {"mcp", "Solve single optimization problem as MCP (KKT system)"},
   {"nlp", "Solve single optimization problem as an optimization problem"}
};

const char* optsingleopt_getcurstr(unsigned i)
{
   if (i == Opt_SolveSingleOptAsMcp || i == Opt_SolveSingleOptAsOpt) {
      return singleopt_names[i][0];
   }

   return "unknown";
}

int optsingleopt_getidxfromstr(const char *buf)
{
   for (size_t i = 0; i < ARRAY_SIZE(singleopt_names); ++i) {
      if (!strcasecmp(buf, singleopt_names[i][0])) { return (int)i; }
   }

   return -1;
}

int optsingleopt_set(struct option *opt, const char *optval)
{
   assert(!strcasecmp(opt->name, "solve_single_opt_as"));

   int val = optsingleopt_getidxfromstr(optval);
   if (val >= 0) {
      opt->value.i = val;
      return OK;
   }

   error("%s ERROR: cannot set option %s to value '%s'\n", __func__, opt->name, optval);
   return Error_WrongOptionValue;
}

void optsingleopt_getdata(const char *const (**strs)[2], unsigned *len)
{
   *strs = singleopt_names;
   *len = ARRAY_SIZE(singleopt_names);
}
