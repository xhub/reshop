#include "env_utils.h"
#include "git_version.h"
#include "macros.h"
#include "ovf_options.h"
#include "printout.h"
#include "reshop.h"
#include "rhp_options.h"
#include "status.h"

void rhp_banner(void)
{
  printout(PO_INFO, "ReSHOP %s\n", rhp_git_hash);
}

const char* rhp_version(void)
{
  return rhp_git_hash;
}


void rhp_show_stackinfo(unsigned char val)
{
  if (val) {
    O_Output |= PO_STACK;
  } else {
    O_Output &= ~PO_STACK;
  }
}

void rhp_show_refcnttrace(unsigned char val)
{
  if (val) {
    O_Output |= PO_TRACE_REFCNT;
  } else {
    O_Output &= ~PO_TRACE_REFCNT;
  }
}

void rhp_show_empinterptrace(unsigned char val)
{
  if (val) {
    O_Output |= PO_TRACE_EMPINTERP;
  } else {
    O_Output &= ~PO_TRACE_EMPINTERP;
  }
}

void rhp_show_empparsertrace(unsigned char val)
{
  if (val) {
    O_Output |= PO_TRACE_EMPPARSER;
  } else {
    O_Output &= ~PO_TRACE_EMPPARSER;
  }
}

void rhp_show_solreporttrace(unsigned char val)
{
  if (val) {
    O_Output |= PO_TRACE_SOLREPORT;
  } else {
    O_Output &= ~PO_TRACE_SOLREPORT;
  }
}

void rhp_show_processtrace(unsigned char val)
{
  if (val) {
    O_Output |= PO_TRACE_PROCESS;
  } else {
    O_Output &= ~PO_TRACE_PROCESS;
  }
}

void rhp_show_containertrace(unsigned char val)
{
  if (val) {
    O_Output |= PO_TRACE_CONTAINER;
  } else {
    O_Output &= ~PO_TRACE_CONTAINER;
  }
}

void rhp_show_empdagtrace(unsigned char val)
{
  if (val) {
    O_Output |= PO_TRACE_EMPDAG;
  } else {
    O_Output &= ~PO_TRACE_EMPDAG;
  }
}

void rhp_show_fooctrace(unsigned char val)
{
  if (val) {
    O_Output |= PO_TRACE_FOOC;
  } else {
    O_Output &= ~PO_TRACE_FOOC;
  }
}

static void rhp_show_timings(unsigned char val)
{
   rhp_options[Options_Display_Timings].value.b = val > 0;
}

struct log_opt {
   const char *name;
   void (*fn)(unsigned char);
};

int rhp_syncenv(void)
{
   int status = OK;
   char *env_varname = NULL;
   size_t optname_maxlen = 512;
   MALLOC_(env_varname, char, optname_maxlen + 5);

   struct log_opt log_opts[] = {
      {"container",        rhp_show_containertrace},
      {"empdag", rhp_show_empdagtrace},
      {"empinterp", rhp_show_empinterptrace},
      {"empparser", rhp_show_empparsertrace},
      {"fooc", rhp_show_fooctrace},
      {"process", rhp_show_processtrace},
      {"refcnt", rhp_show_refcnttrace},
      {"timings", rhp_show_timings},
      {"stack", rhp_show_stackinfo},
      {"solreport", rhp_show_solreporttrace},
   };
   const char* loglevel_vals[]   = {"all",   "error", "info",   "v",  "vv",  "vvv"};
   const unsigned loglevel_num[] = {INT_MAX, PO_ERROR, PO_INFO, PO_V, PO_VV, PO_VVV};
   const unsigned n_loglevels = ARRAY_SIZE(loglevel_num);

   const unsigned n_opts = sizeof(log_opts)/sizeof(log_opts[0]);

   const char * restrict env_varval = find_rhpenvvar("LOG", &env_varname, &optname_maxlen);

   /* If RHP_LOG is defined, then try to parse its value as "val1:val2" */
   if (env_varval) {
      size_t len = 0, varlen = strlen(env_varval);

      while (len < varlen) {
         const char *envopt = &env_varval[len];
         unsigned envopt_len = 0;
         unsigned char val = 1;

         if (envopt[0] == '-') { val = 0; envopt++; len++; }

         while (envopt[envopt_len] != ':' && envopt[envopt_len] != '\0') {
            envopt_len++;
         }
         len += envopt_len;

         for (unsigned i = 0; i < n_opts; ++i) {
            unsigned optlen = strlen(log_opts[i].name);
            if (envopt_len == optlen && !strncmp(log_opts[i].name, envopt, optlen)) {
               log_opts[i].fn(val);
               goto _continue;
            }
         }

         /* If we have level, then parse it as level=loglevel */
         if (!strncmp("level", envopt, 5)) {
            if (envopt[5] != '=') {
               error("%s :: expecting an expression 'level=loglevel',", __func__);
               continue;
            }

            const char *loglevel_env = &envopt[6];

            for (unsigned i = 0; i < n_loglevels; ++i) {
               const char *loglevel = loglevel_vals[i];
               if (!strncmp(loglevel, loglevel_env, strlen(loglevel))) {
                  O_Output = (O_Output & ~(unsigned)PO_MASK_LEVEL) + loglevel_num[i];
                  goto _continue;
               }
            }
         }

         /* the 'all' keyword does turn on everything */
         if (!strncmp("all", envopt, 3)) {
            for (unsigned i = 0; i < n_opts; ++i) { log_opts[i].fn(val); }
            O_Output |= PO_MAX_VERBOSITY | PO_ALLDEST;
         }

_continue:
         if (env_varval[len] == ':') { len++; }
         else break;
      }
    }
   myfreeenvval(env_varval);


   FREE(env_varname);

   if (status == OK) {
      status = ovf_syncenv();
   }

   return status;
}
