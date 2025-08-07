#include "checks.h"
#include "macros.h"
#include "printout.h"
#include "reshop.h"
#include "rhpgui_launcher.h"
#include <ctype.h>

static int chk_name(const char *optname, const char *fn)
{
   if (!optname) {
      error("%s ERROR: option name is NULL!\n", fn);
      return Error_NullPointer;
   }

   return OK;
}

static int chk_opttype(struct option *opt, OptType type, const char *fn)
{
   if (!opt) {
      error("%s ERROR: opt is NULL!\n", fn);
      return Error_NullPointer;
   }

   if (opt->type != type) {
      error("%s ERROR: for opt %s, expected type '%s', got '%s'\n", fn,
            opt->name, opttype_name(type), opttype_name(opt->type));
      return Error_NullPointer;

   }

   return OK;
}

int rhp_opt_setb(const char *name, unsigned char bval)
{
   unsigned index;
   struct option_set *optset;
   S_CHECK(chk_name(name, __func__));

   if (opt_find(name, &optset, &index)) {
      S_CHECK(chk_opttype(&optset->opts[index], OptBoolean, __func__))
      optset->opts[index].value.b = bval;

      if (!strcmp(name, "gui")) {
         imgui_start(NULL);
      }

      return OK;
   }

   return Error_OptionNotFound;
}

int rhp_opt_setc(const char *name, const char *str)
{
   unsigned index;
   struct option_set *optset;
   S_CHECK(chk_name(name, __func__));

   if (opt_find(name, &optset, &index)) {
      S_CHECK(chk_opttype(&optset->opts[index], OptChoice, __func__))
      S_CHECK(optchoice_set(&optset->opts[index], str));
      return OK;
   }

   return Error_OptionNotFound;
}

int rhp_opt_setd(const char *name, double dval)
{
   unsigned index;
   struct option_set *optset;
   S_CHECK(chk_name(name, __func__));

   if (opt_find(name, &optset, &index)) {
      S_CHECK(chk_opttype(&optset->opts[index], OptDouble, __func__))
      optset->opts[index].value.d = dval;
      return OK;
   }

   return Error_OptionNotFound;
}

int rhp_opt_seti(const char *name, int ival)
{
   unsigned index;
   struct option_set *optset;
   S_CHECK(chk_name(name, __func__));

   if (opt_find(name, &optset, &index)) {
      S_CHECK(chk_opttype(&optset->opts[index], OptInteger, __func__))
      optset->opts[index].value.i = ival;
      return OK;
   }

   return Error_OptionNotFound;
}

static int _option_set_str(struct option *opt, const char *value)
{
   opt->value.s = strdup(value);
   return OK;
}

int rhp_opt_sets(const char *name, const char *value)
{
   struct option_set *optset;
   unsigned index;
   S_CHECK(chk_name(name, __func__));

   if (opt_find(name, &optset, &index)) {
      S_CHECK(chk_opttype(&optset->opts[index], OptString, __func__))
      S_CHECK(_option_set_str(&optset->opts[index], value));
      return OK;
   }

   return Error_OptionNotFound;
}

int rhp_opt_getb(const char *name, int *b)
{
   unsigned index;
   struct option_set *optset;

   if (opt_find(name, &optset, &index)) {
      *b = optset->opts[index].value.b;
      return OK;
   }

   return Error_OptionNotFound;
}

int rhp_opt_getd(const char *name, double *d)
{
   unsigned index;
   struct option_set *optset;

   if (opt_find(name, &optset, &index)) {
      *d = optset->opts[index].value.d;
      return OK;
   }

   return Error_OptionNotFound;
}

int rhp_opt_geti(const char *name, int *i)
{
   unsigned index;
   struct option_set *optset;

   if (opt_find(name, &optset, &index)) {
      *i = optset->opts[index].value.i;
      return OK;
   }

   return Error_OptionNotFound;
}

int rhp_opt_gets(const char *name, const char **str)
{
   unsigned index;
   struct option_set *optset;

   if (opt_find(name, &optset, &index)) {
      *str = optset->opts[index].value.s;
      return OK;
   }

   *str = NULL;
   return Error_OptionNotFound;
}

/**
 * @brief Get the type of an option
 *
 * @param       name  the name of the option
 * @param[out]  type  the option type
 *
 * @return the type of option, or INT_MAX if there is no option with this name
 */
int rhp_opt_gettype(const char *name, unsigned *type)
{
   unsigned index;
   struct option_set *optset;

   if (chk_arg_nonnull(name, 1, __func__)) {
      return Error_NullPointer;
   }

   if (opt_find(name, &optset, &index)) {
      *type = optset->opts[index].type;
      return OK;
   }

   error("[option] ERROR: unknown option %s\n", name);

   return Error_OptionNotFound;
}

/**
 * @brief Set option from a string formatted like "optname optval"
 *
 * @param optstring the string
 *
 * @return          the error code
 */
int rhp_opt_setfromstr(char *optstring)
{
   S_CHECK(chk_arg_nonnull(optstring, 1, __func__));

   if (optstring[0] == '*') {
      return Error_WrongOptionValue;
   }

   unsigned start = 0;
   while (isspace(optstring[start])) { start++; }

   if (optstring[start] == '\0') {
      return Error_WrongOptionValue;
   }

   char *optval, *optname = &optstring[start];

   unsigned end = start;
   while (isalnum(optstring[end]) || optstring[end] == '_') { end++; }

   unsigned pos1 = end;
   char c1 = optstring[pos1];
   optstring[pos1] = '\0';

  /* ----------------------------------------------------------------------
   * To simplify the logic, we plough throw the string until we find the end
   *
   * Note that if c1 is NUL, then we are already at the end
   * ---------------------------------------------------------------------- */

   if (c1 != '\0') {

      start = end;
      while (isspace(optstring[start])) { start++; }
      optval = &optstring[start];

      /* Look for the end of the string */
      end = start;
      while (optstring[end] != '\0') { end++; }

      /* Backtrack to remove trailing spaces */
      while (isspace(optstring[end-1])) { end--; }
   } else {
      optval = &optstring[end];
   }

   unsigned pos2 = end;
   char c2 = optstring[pos2];
   optstring[pos2] = '\0';

   int status;
   struct option_set *optset;
   unsigned index;

   if (opt_find(optname, &optset, &index)) {
      status = opt_setfromstr(&optset->opts[index], optval);
   } else {
      status = Error_OptionNotFound;
   }

   optstring[pos1] = c1;
   optstring[pos2] = c2;

   return status;
}


