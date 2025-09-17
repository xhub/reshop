#include "asprintf.h"

#include <ctype.h>

#include "macros.h"
#include "printout.h"
#include "rhp_options.h"
#include "rhp_options_data.h"
#include "string_utils.h"

//   [Options_Cumulative_Iteration_Limit] = { "cumulative_iteration_limit", "", OptInteger, NULL, { .i = 5000 } },
//   [Options_Major_Iteration_Limit]      = { "major_iteration_limit",      "", OptInteger, NULL, { .i = 100  } },
//   [Options_Minor_Iteration_Limit]      = { "minor_iteration_limit",      "", OptInteger, NULL, { .i = 1000 } },
//   [Options_Convergence_Tolerance]      = { "convergence_tolerance",      "Absolute tolerance convergence", OptDouble,  NULL, { .d = 1e-6 } },
//   [Options_Output_Iteration_Log]       = { "output_iteration_log",       "", OptBoolean, NULL, { .b = true } },


// clang-format off
tlsvar struct option rhp_options[] = {
   [Options_Display_EmpDag]        = { "display_empdag",      "Display EMPDAG as png",                     OptBoolean, { .b = false} },
   [Options_Display_Equations]     = { "display_equations",   "Display Equations as png",                  OptString,  { .s = ""} },
   [Options_Display_OvfDag]        = { "display_ovfdag",      "Display OVFDAG as png",                     OptBoolean, { .b = false} },
   [Options_Display_Timings]       = { "display_timings",     "Display timing information",                OptBoolean, { .b = false} },
   [Options_Dump_Scalar_Models]    = { "dump_scalar_model",   "Dump every scalar model via convert",       OptBoolean, { .b = false } },
   [Options_Expensive_Checks]      = { "expensive_checks",    "Perform time consuming consistency checks", OptBoolean, { .b = false} },
   [Options_EMPInfoFile]           = { "EMPInfoFile",         "EMPinfo file to use",                       OptString,  { .s = "empinfo.dat" } },
   [Options_GUI]                   = { "gui",                 "Start GUI",                                 OptBoolean, { .b = false } },
   [Options_Output]                = { "output",              "Output level",                              OptInteger, { .i = PO_INFO } },
   [Options_Output_Subsolver_Log]  = { "output_subsolver_log","whether to output subsolver logs",          OptBoolean, { .b = false } },
   [Options_Pathlib_Name]          = { "pathlib_name",        "path of the PATH library",                  OptString,  { .s = "" } },
   [Options_Png_Viewer]            = { "png_viewer",          "Executable to display png",                 OptString,  { .s = "" } },
   [Options_SolveSingleOptAs]      = { "solve_single_opt_as", "How to solve an empdag with a single MP",   OptChoice,  { .i = Opt_SolveSingleOptAsOpt} },
   [Options_Subsolveropt]          = { "subsolveropt",        "Subsolver option file number",              OptInteger, { .i = 0     } },
   [Options_Time_Limit]            = { "time_limit",          "Maximum running time in seconds",           OptInteger, { .i = 3600 } },
   [Options_Save_EmpDag]           = { "save_empdag",         "Save EMPDAG as png",                        OptBoolean, { .b = false} },
   [Options_Save_OvfDag]           = { "save_ovfdag",         "Save OVFDAG as png",                        OptBoolean, { .b = false} },
};
// clang-format on

RESHOP_STATIC_ASSERT(ARRAY_SIZE(rhp_options) == Options_Last+1, "rhp_options not synchronized")

static tlsvar struct option_set common_optset = {
   .id           = OptSet_Main,
   .alloced      = false,
   .numopts      = Options_Last+1,
   .opts         = NULL,
};

int option_addcommon(struct option_list *list)
{
   /* ----------------------------------------------------------------------
    * because we use thread-local storage for rhp_options, it is considered
    * as non-constant. Hence, we need to init it here
    * ---------------------------------------------------------------------- */

   common_optset.opts = rhp_options;

   return optset_add(list, &common_optset);
}

bool optvalb(const Model *mdl, enum rhp_options_enum opt)
{
  /* ----------------------------------------------------------------------
   * The order of priority:
   * - 1) If the model has options, then look there
   * - 2) Look for env variable
   * - 3) Take the value in the struct
   * ---------------------------------------------------------------------- */

   /* TODO: implement model */

   if ((size_t)opt >= ARRAY_SIZE(rhp_options) || ((int)opt) < 0) {
      error("%s ERROR: option value %d is outside of the range [0, %d]",
            __func__, opt, Options_Last);
      return false;
   }

   struct option *o = &rhp_options[opt];

   if (o->type != OptBoolean) {
      error("%s ERROR: option '%s' is of type %s, expecting %s\n", __func__,
            o->name, opttype_name(o->type), opttype_name(OptBoolean));
      return false;
   }

   char* env_var;

   if (asprintf(&env_var, "RHP_%s", rhp_options[opt].name) < 0) {
      errormsg("%s ERROR: asprintf() failed!");
      return false;
   }

   char * restrict s = env_var;
   while (*s) { *s = RhpToUpper(*s); s++; }

   const char *env = mygetenv(env_var);
   free(env_var);

   if (env) {
      bool res;
      /* Lazy way of checking if set to 0 */
      if (env[0] == '0') {
         res = false;
      } else {
         res = true;
      }
      myfreeenvval(env);

      return res;
   }

   return o->value.b;
}

int optvali(const Model *mdl, enum rhp_options_enum opt)
{
  /* ----------------------------------------------------------------------
   * The order of priority:
   * - 1) If the model has options, then look there
   * - 2) Look for env variable
   * - 3) Take the value in the struct
   * ---------------------------------------------------------------------- */

   /* TODO: implement model */

   if ((size_t)opt >= ARRAY_SIZE(rhp_options) || ((int)opt) < 0) {
      error("%s ERROR: option value %d is outside of the range [0, %d]",
            __func__, opt, Options_Last);
      return false;
   }

   struct option *o = &rhp_options[opt];

   if (o->type == OptChoice) {
      return o->value.i;
   }

   if (o->type != OptInteger) {
      error("%s ERROR: option '%s' is of type %s, expecting %s\n", __func__,
            o->name, opttype_name(o->type), opttype_name(OptInteger));
      return false;
   }

   char* env_var;

   if (asprintf(&env_var, "RHP_%s", rhp_options[opt].name) < 0) {
      errormsg("%s ERROR: asprintf() failed!");
      return false;
   }

   char *s = env_var;
   while (*s) { *s = toupper((unsigned char)*s); s++; }

   const char *env = mygetenv(env_var);
   FREE(env_var);

   if (env) {
      errno = 0;
      long res = strtol(env, NULL, 10);

      if (errno) {
         perror("strtol");
         return INT_MAX;
      }

      if (res >= INT_MAX) {
         error("%s ERROR: environment value %ld for option '%s' greater than %d",
               __func__, res, rhp_options[opt].name, INT_MAX-1);
         return Error_InvalidValue;
      }

      if (res <= INT_MIN) {
         error("%s ERROR: environment value %ld for option '%s' smaller than %d",
               __func__, res, rhp_options[opt].name, INT_MIN+1);
         return Error_InvalidValue;
      }

      int i = (int)res;
      myfreeenvval(env);

      return i;
   }

   return o->value.i;
}

/**
 * @brief Get the string value of an option 
 *
 * @warning this returns a copy of the string. It must be freed
 *
 * @param mdl the model
 * @param opt the option
 *
 * @return    the copy of the option string
 */
char* optvals(const Model *mdl, enum rhp_options_enum opt)
{
  /* ----------------------------------------------------------------------
   * The order of priority:
   * - 1) If the model has options, then look there
   * - 2) Look for env variable
   * - 3) Take the value in the struct
   * ---------------------------------------------------------------------- */

   /* TODO: implement model */

   if (opt > Options_Last || ((int)opt) < 0) {
      error("%s ERROR: option value %d is outside of the range [0, %d]",
            __func__, opt, Options_Last);
      return NULL;
   }

   struct option *o = &rhp_options[opt];

   if (o->type != OptString) {
      error("%s ERROR: option '%s' is of type %s, expecting %s\n", __func__,
            o->name, opttype_name(o->type), opttype_name(OptString));
      return NULL;
   }

   char* env_var;

   if (asprintf(&env_var, "RHP_%s", rhp_options[opt].name) < 0) {
      errormsg("%s ERROR: asprintf() failed!");
      return NULL;
   }

   char *s = env_var;
   while (*s) { *s = toupper((unsigned char)*s); s++; }

   const char *env = mygetenv(env_var);
   char *res = NULL;
   if (env) {
      res = strdup(env);
   }
   myfreeenvval(env);
   free(env_var);

   if (!res) {
      res = strdup(o->value.s);
   }

   return res;
}
