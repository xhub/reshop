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


void rhp_show_backendinfo(unsigned char val)
{
  if (val) {
    O_Output |= PO_BACKEND;
  } else {
    O_Output &= ~PO_BACKEND;
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

void rhp_show_ccftrace(unsigned char val)
{
  if (val) {
    O_Output |= PO_TRACE_CCF;
  } else {
    O_Output &= ~PO_TRACE_CCF;
  }
}

static void rhp_show_timings(unsigned char val)
{
   rhp_options[Options_Display_Timings].value.b = val > 0;
}

static void rhp_show_solver_log(unsigned char val)
{
   O_Output_Subsolver_Log = val > 0;
}

struct log_opt {
   const char *name;
   void (*fn)(unsigned char);
   const char *help;
};

int rhp_syncenv(void)
{
   int status = OK;
   char *env_varname = NULL;
   size_t optname_maxlen = 512;
   MALLOC_(env_varname, char, optname_maxlen + 5);

   struct log_opt log_opts[] = {
      {"container",   rhp_show_containertrace, "lists algebraic container changes"},
      {"ccf",         rhp_show_ccftrace,       "lists CCF reformulation information"},
      {"empdag",      rhp_show_empdagtrace,    "lists EMPDAG actions"},
      {"empinterp",   rhp_show_empinterptrace, "lists EMP interpreter actions"},
      {"empparser",   rhp_show_empparsertrace, "lists EMP parser actions"},
      {"fooc",        rhp_show_fooctrace,      "lists first-order optimality conditions computation information"},
      {"process",     rhp_show_processtrace,   "lists processing actions"},
      {"refcnt",      rhp_show_refcnttrace,    "display reference counter information"},
      {"timings",     rhp_show_timings,        "display timings information"},
      {"stack",       rhp_show_backendinfo,    "display backend information"},
      {"solreport",   rhp_show_solreporttrace, "display solution/values reporting actions"},
      {"solver",      rhp_show_solver_log,     "display solver log"},
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

         /* the 'all' keyword does turn on everything */
         if (!strncmp("help", envopt, 4)) {
            printf("Help for RHP_LOG values:\n\n");
            for (unsigned i = 0; i < n_opts; ++i) {
               /* TODO: do we have access to printf here */
               printf("\t%20s: %s\n", log_opts[i].name, log_opts[i].help);
            }
            printf("\t%20s: enable all options above\n", "all");
            status = Error_WrongOptionValue;
            goto _exit;
         }

_continue:
         if (env_varval[len] == ':') { len++; }
         else { break; }
      }
    }

_exit:
   myfreeenvval(env_varval);


   FREE(env_varname);

   if (status == OK) {
      status = ovf_syncenv();
   }

   return status;
}
