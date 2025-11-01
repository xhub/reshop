#include <assert.h>
#include <stdlib.h>
#include <stdio.h>
#include <string.h>

#include "ovf_options.h"
#include "macros.h"
#include "printout.h"
#include "status.h"

tlsvar struct option_set ovf_optset = {
   .id           = OptSet_OVF,
   .alloced      = false,
   .numopts      = Options_Ovf_Last+1,
   .opts         = NULL,
};

// clang-format off
tlsvar struct option ovf_options[] = {
   [Options_Ovf_Init_New_Variables] = { "ovf_init_new_variables", "initialize the new variables introduced during OVF/CCF reformulation", OptBoolean, { .b = false   } },
   [Options_Ovf_Reformulation]      = { "ovf_reformulation",      "scheme for reformulating OVF variables and CCF MPs",                   OptChoice,  { .i = OVF_Equilibrium } },
};
// clang-format on

RESHOP_STATIC_ASSERT(ARRAY_SIZE(ovf_options) == Options_Ovf_Last+1, "Sync issue for ovf_options")

static const char* const ovf_reformulation_names[][2] = {
   { "equilibrium", "Nash Equilibrium (or VI formulation)" },
   { "fenchel",     "Fenchel dual (for conic QP)" },
   { "conjugate",   "Conjugate-based reformulation" },
};

const char* ovf_getreformulationstr(unsigned i)
{
   if (i != OVF_Scheme_Unset && i <= OVF_Scheme_Last) {
      return ovf_reformulation_names[i-1][0];
   }

   return "unknown";
}

bool optovf_getreformulationmethod(const char *buf, unsigned *value)
{
   for (unsigned i = 0; i < OVF_Scheme_Last; ++i) {
      if (!strcasecmp(buf, ovf_reformulation_names[i][0])) {
         (*value) = i+1;
         return true;
      }
   }

   *value = UINT_MAX;
   return false;
}

void optovf_getreformulationdata(const char *const (**strs)[2], unsigned *len)
{
   *strs = ovf_reformulation_names;
   *len = 3;
   
}

int optovf_setreformulation(struct option *optovf_reformulation, const char *optval)
{
   assert(!strcasecmp(optovf_reformulation->name, "ovf_reformulation"));

   unsigned val;
   if (optovf_getreformulationmethod(optval, &val)) {
      optovf_reformulation->value.i = (int)val;
      return OK;
   }

   error("[option/ovf] ERROR: cannot set option '%s' to value '%s'. Valid values are:\n",
         optovf_reformulation->name, optval);

   for (unsigned i = 0; i < OVF_Scheme_Last; ++i) {
      error("\t%-15s (%s) \n", ovf_reformulation_names[i][0], ovf_reformulation_names[i][1]);
   }

   return Error_WrongOptionValue;
}

int option_addovf(struct option_list *list)
{
   /* ----------------------------------------------------------------------
    * because we use thread-local storage for ovf_options, it is considered
    * as non-constant. Hence, we need to init it here
    * ---------------------------------------------------------------------- */

   ovf_optset.opts = ovf_options;

   return optset_add(list, &ovf_optset);
}

int ovf_syncenv(void)
{
   const char* formulation_env = mygetenv("RHP_OVF_REFORMULATION");
   if (formulation_env) {
      S_CHECK(optovf_setreformulation(&ovf_options[Options_Ovf_Reformulation], formulation_env));
   }
   myfreeenvval(formulation_env);

   const char* init_vars_env = mygetenv("RHP_OVF_INIT_NEW_VARS");
   if (init_vars_env) {
      O_Ovf_Init_New_Variables = init_vars_env[0] != '\0' && init_vars_env[0] != '0';
   }
   myfreeenvval(init_vars_env);

   return OK;
}
