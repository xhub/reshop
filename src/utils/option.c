#include "reshop_config.h"

#include <ctype.h>
#include <errno.h>
#include <stdbool.h>
#include <string.h>

#include "checks.h"
#include "env_utils.h"
#include "macros.h"
#include "rhp_options_data.h"
#include "status.h"
#include "option.h"
#include "option_priv.h"
#include "ovf_options.h"
#include "printout.h"
#include "tlsdef.h"

static tlsvar struct option_set* g_optsets[OptSet_Last+1] = {NULL};

static tlsvar struct option_list g_optlist = {
   .len = OptSet_Last+1,
   .optsets = NULL,
};

CONSTRUCTOR_ATTR_PRIO(101) void option_init(void)
{
   if (g_optlist.optsets) return;

   g_optlist.optsets = g_optsets;

   option_addcommon(&g_optlist);
   option_addovf(&g_optlist);
}

const char *opttype_name(OptType type)
{
   switch (type) {
   case OptBoolean:  return "boolean";
   case OptChoice:   return "choice";
   case OptDouble:   return "double";
   case OptInteger:  return "integer";
   case OptString:   return "string";
   default:          return "ERROR unknown OptType";
   }
}

  /* ----------------------------------------------------------------------
   * When additing an OptChoice, one needs to add code in the following 3 functions
   * ---------------------------------------------------------------------- */

typedef struct {
   const char *name;
   int (*setopt)(struct option *opt, const char *optval);
   void (*getopts)(const char *const (**strs)[2], unsigned *len);
   const char*(*getcurname)(unsigned i);
} OptChoiceData;

static const OptChoiceData optchoices[] = {
   { "ovf_reformulation", optovf_setreformulation, optovf_getreformulationdata, ovf_getreformulationstr},
   { "solve_single_opt_as", optsingleopt_set, optsingleopt_getdata, optsingleopt_getcurstr},
};

int optchoice_set(struct option *opt, const char *optval)
{
   if (opt->type != OptChoice) {
      return Error_RuntimeError;
   }

   for (unsigned i = 0; i < ARRAY_SIZE(optchoices); ++i) {
      const OptChoiceData *optdat = &optchoices[i];
      if (!strcasecmp(opt->name, optdat->name)) {
         return optdat->setopt(opt, optval);
      }
   }

   return OK;
}

int optchoice_getopts(struct option *opt, const char *const (**strs)[2], unsigned *len)
{
   if (opt->type != OptChoice) {
      return Error_RuntimeError;
   }

   for (unsigned i = 0; i < ARRAY_SIZE(optchoices); ++i) {
      const OptChoiceData *optdat = &optchoices[i];
      if (!strcasecmp(opt->name, optdat->name)) {
         optdat->getopts(strs, len);
         return OK;
      }
   }

   return Error_OptionNotFound;
}

const char* optchoice_getdefaultstr(struct option *opt)
{
   if (opt->type != OptChoice) {
      return NULL;
   }

   for (unsigned i = 0; i < ARRAY_SIZE(optchoices); ++i) {
      const OptChoiceData *optdat = &optchoices[i];
      if (!strcasecmp(opt->name, optdat->name)) {
         return optdat->getcurname(opt->value.i);
      }
   }

   return NULL;
}

static int posint(int *min, int *max)
{
   *min = 0;
   *max = INT_MAX;
   return OK;
}

int optint_getrange(struct option *opt, int *min, int *max)
{
   if (opt->type != OptInteger) {
      return Error_RuntimeError;
   }

   if (!strcasecmp(opt->name, "output") || !strcasecmp(opt->name, "time_limit")) {
      return posint(min, max);
   }

   if (!strcasecmp(opt->name, "subsolveropt")) {
      *min = 0; *max = 999;
      return OK;
   }

   return Error_NotImplemented;
}

static bool findopt(struct option_list *optlist, const char *name,
                    struct option_set **ret_optset, unsigned *index)
{
   struct option_set *optset;

   for (unsigned i = 0; i < optlist->len; i++) {
      optset = optlist->optsets[i];

      if (optset) {
         for (unsigned j = 0; j < optset->numopts; j++) {
            if (!strcasecmp(name, optset->opts[j].name)) {
               (*ret_optset) = optset;
               (*index) = j;
               return true;
            }
         }
      }
   }

   (*ret_optset) = NULL;
   (*index) = UINT_MAX;

   return false;
}

bool opt_find(const char *name, struct option_set **ret_optset, unsigned *index)
{
   return findopt(&g_optlist, name, ret_optset, index);
}

static void optset_free(struct option_set **optset)
{
   if (optset && (*optset)) {
      FREE((*optset)->opts);
      FREE((*optset));
      (*optset) = NULL;
   }
}

/**
 * @brief Add the option set in the option list
 *
 * @param optlist  the option list
 * @param optset   the option set
 *
 * @return the error code
 */
int optset_add(struct option_list *optlist, struct option_set *optset)
{
   if (optset->id <= OptSet_Last) {
      if (optlist->optsets[optset->id]
          && optlist->optsets[optset->id]->alloced) {
         optset_free(&optlist->optsets[optset->id]);
      }

      optlist->optsets[optset->id] = optset;
      return OK;
   }

   error("%s ERROR: option set #%u is unknown\n", __func__, optset->id);
   return Error_InvalidValue;
}

/**
 * @brief Set an option from a string
 *
 * @param opt  the option
 * @param str  the string containing the value
 *
 * @return     the error code
 */
int opt_setfromstr(struct option *opt, const char *str)
{
   switch (opt->type) {
   case OptDouble:
      {
         errno = 0;
         char *endptr;
         double val = strtod(str, &endptr);
         if (errno != 0) {
            perror("strtod");
            return Error_RuntimeError;
         }

         if (endptr == str) {
            error("%s ERROR: while setting %s, no number found in %s\n", __func__, opt->name, str);
            return Error_RuntimeError;
         }

         opt->value.d = val;
         printout(PO_V, "Option %s set to %e\n", opt->name, val);

         if (*endptr != '\0') {
            printout(PO_INFO, "Further characters after number: '%s' in '%s'\n", endptr, str);
         }
         break;
      }

   case OptInteger:
   case OptBoolean:
      {
         errno = 0;
         char *endptr;
         long val = strtol(str, &endptr, 0);
         if (errno != 0) {
            perror("strtol");
            return Error_RuntimeError;
         }

         if (endptr == str) {
            error("%s ERROR: while setting %s, no number found in %s\n", __func__, opt->name, str);
            return Error_RuntimeError;
         }

         if (val > INT_MAX || val < INT_MIN) {
            error("%s ERROR: while setting %s, parsed value %ld is outside of "
                  "the range for int\n", __func__, opt->name, val);
            return Error_RuntimeError;
         }
         opt->value.i = (int)val;
         printout(PO_V, "Option %s set to %d\n", opt->name, (int)val);

         if (*endptr != '\0') {
            printout(PO_INFO, "Further characters after number: '%s' in '%s'\n", endptr, str);
         }
         break;
      }
   case OptString:
      FREE(opt->value.s);
      opt->value.s = strdup(str);
      printout(PO_V, "Option %s set to %s\n", opt->name, str);
      break;
   case OptChoice:
      S_CHECK(optchoice_set(opt, str));
      printout(PO_V, "Option %s set to %s\n", opt->name, str);
      break;
   default:
      error("%s ERROR: option %s has unkown type %u\n", __func__, opt->name, opt->type);
      return Error_RuntimeError;
   }

   return OK;
}

int optset_syncenv(struct option_set *optset)
{
   int status = OK;
   char *env_varname = NULL;
   size_t optname_maxlen = 512;
   MALLOC_(env_varname, char, optname_maxlen + 5);

   for (size_t j = 0; j < optset->numopts; j++) {
      const char* optname = optset->opts[j].name;
      const char *env_varval = find_rhpenvvar(optname, &env_varname, &optname_maxlen);

      if (env_varval) {
         struct option *opt = &optset->opts[j];
         S_CHECK_EXIT(opt_setfromstr(opt, env_varval));
      }

      myfreeenvval(env_varval);
   }

_exit:
   FREE(env_varname);

   return status;
}

struct option *opt_iter(OptIterator *iter)
{
   while (iter->setid < g_optlist.len) {

      OptSet *optset = g_optlist.optsets[iter->setid];

      if (iter->optnum >= optset->numopts) {
         iter->setid++;
         iter->optnum = 0;
      } else {
         return &optset->opts[iter->optnum++];
      }
   }

   return NULL;
}

