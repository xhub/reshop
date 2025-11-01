#include "env_utils.h"
#include "git_version.h"
#include "macros.h"
#include "ovf_options.h"
#include "printout.h"
#include "reshop.h"
#include "reshop_priv.h"
#include "rhp_options.h"
#include "rhpgui_launcher.h"
#include "status.h"
#include "string_utils.h"

#ifdef __x86_64__
#   define RHP_ARCH "amd64"
#elif defined(__i386__)
#   define RHP_ARCH "x86 (32-bit)"
#elif defined(__arm__)
#   define RHP_ARCH "arm (32-bit)"
#elif defined(__aarch64__)
#   define RHP_ARCH "arm64"
#elif defined(__ppc__) || defined(__powerpc__)
#   define RHP_ARCH "PowerPC"
#elif defined(__mips__)
#   define RHP_ARCH "MIPS"
#elif defined(__riscv)
#   define RHP_ARCH "RISC-V"
#elif defined(__sparc__)
#   define RHP_ARCH "SPARC"
#elif defined(_M_X64)
#   define RHP_ARCH "amd64 (MSVC)"
#elif defined(_M_IX86)
#   define RHP_ARCH "x86 (32-bit, MSVC)"
#elif defined(_M_ARM)
#   define RHP_ARCH "arm (MSVC)"
#elif defined(_M_ARM64)
#   define RHP_ARCH "arm64 (MSVC)"
#else
#   error "Unknown architecture, please modify the source code"
#endif

#if defined(_WIN32)
    #if defined(_WIN64)
#  define RHP_OS "Windows (64-bit)"
    #else
#  define RHP_OS "Windows (32-bit)"
    #endif
#elif defined(__APPLE__) && defined(__MACH__)
#  define RHP_OS "macOS"
#elif defined(__linux__)
#  define RHP_OS "Linux"
#elif defined(__FreeBSD__)
#  define RHP_OS "FreeBSD"
#elif defined(__NetBSD__)
#  define RHP_OS "NetBSD"
#elif defined(__OpenBSD__)
#  define RHP_OS "OpenBSD"
#elif defined(__sun) && defined(__SVR4)
#  define RHP_OS "Solaris"
#elif defined(__ANDROID__)
#  define RHP_OS "Android"
#elif defined(__CYGWIN__)
#  define RHP_OS "Cygwin"
#elif defined(__HAIKU__)
#  define RHP_OS "Haiku"
#else
#  define RHP_OS "Unknown"
#endif

/**
 * @brief Print the ReSHOP banner
 * @ingroup publicAPI
 *
 */
void rhp_print_banner(void)
{
  printout(PO_INFO, "\nReSHOP %s on " RHP_OS " / " RHP_ARCH "\t Author: Olivier Huber\n\n", rhp_git_hash);
}

/**
 * @brief Return the string representation of the ReSHOP version
 *
 * @ingroup publicAPI
 *
 * @return  the string representing the ReSHOP version
 */
const char* rhp_version(void)
{
  return rhp_git_hash;
}

/**
 * @brief Set some user or initial backend info
 *
 * This is mostly used for debug and crash reporting
 *
 * @ingroup publicAPI
 */
void rhp_set_userinfo(const char *userinfo)
{
   if (userinfo) {
      set_userinfo(userinfo);
   }
}


/** @brief Control the output of (debugging) backend information in the log for debugging purposes
 *
 *  @ingroup publicAPI
 *
 *  @param boolval if non-zero, the backend information is not displayed. Otherwise it is not.
 */
void rhp_show_backendinfo(unsigned char boolval)
{
  if (boolval) {
    O_Output |= PO_BACKEND;
  } else {
    O_Output &= ~PO_BACKEND;
  }
}

/**
 * @brief Control the output of reference counting information in the log for debugging purposes
 *
 * @ingroup publicAPI
 *
 * @param boolval if non-zero, the reference counting information is not displayed. Otherwise it is not.
 */
void rhp_show_refcnttrace(unsigned char boolval)
{
  if (boolval) {
    O_Output |= PO_TRACE_REFCNT;
  } else {
    O_Output &= ~PO_TRACE_REFCNT;
  }
}

/** @brief Control the output of (debugging) EMP interpreter information in the log for debugging purposes
 *
 *  @ingroup publicAPI
 *
 *  @param boolval if non-zero, the EMP interpreter information is not displayed. Otherwise it is not.
 */
void rhp_show_empinterptrace(unsigned char boolval)
{
  if (boolval) {
    O_Output |= PO_TRACE_EMPINTERP;
  } else {
    O_Output &= ~PO_TRACE_EMPINTERP;
  }
}

/** @brief Control the output of (debugging) EMP parser information in the log for debugging purposes
 *
 *  @ingroup publicAPI
 *
 *  @param boolval if non-zero, the EMP parser information is not displayed. Otherwise it is not.
 */
void rhp_show_empparsertrace(unsigned char boolval)
{
  if (boolval) {
    O_Output |= PO_TRACE_EMPPARSER;
  } else {
    O_Output &= ~PO_TRACE_EMPPARSER;
  }
}

/** @brief Control the output of (debugging) solution reporting information in the log for debugging purposes
 *
 *  @ingroup publicAPI
 *
 *  @param boolval if non-zero, the solution reporting information is not displayed. Otherwise it is not.
 */
void rhp_show_solreporttrace(unsigned char boolval)
{
  if (boolval) {
    O_Output |= PO_TRACE_SOLREPORT;
  } else {
    O_Output &= ~PO_TRACE_SOLREPORT;
  }
}

/** @brief Control the output of (debugging) model processing information in the log for debugging purposes
 *
 *  @ingroup publicAPI
 *
 *  @param boolval if non-zero, the model processing information is not displayed. Otherwise it is not.
 */
void rhp_show_processtrace(unsigned char boolval)
{
  if (boolval) {
    O_Output |= PO_TRACE_PROCESS;
  } else {
    O_Output &= ~PO_TRACE_PROCESS;
  }
}

/** @brief Control the output of (debugging) container information in the log for debugging purposes
 *
 *  @ingroup publicAPI
 *
 *  @param boolval if non-zero, the container information is not displayed. Otherwise it is not.
 */
void rhp_show_containertrace(unsigned char boolval)
{
  if (boolval) {
    O_Output |= PO_TRACE_CONTAINER;
  } else {
    O_Output &= ~PO_TRACE_CONTAINER;
  }
}

/** @brief Control the output of (debugging) EMPDAG information in the log for debugging purposes
 *
 *  @ingroup publicAPI
 *
 *  @param boolval if non-zero, the EMPDAG information is not displayed. Otherwise it is not.
 */
void rhp_show_empdagtrace(unsigned char boolval)
{
  if (boolval) {
    O_Output |= PO_TRACE_EMPDAG;
  } else {
    O_Output &= ~PO_TRACE_EMPDAG;
  }
}

/** @brief Control the output of (debugging) first-order optimality condition information in the log for debugging purposes
 *
 *  @ingroup publicAPI
 *
 *  @param boolval if non-zero, the first-order optimality condition information is not displayed. Otherwise it is not.
 */
void rhp_show_fooctrace(unsigned char boolval)
{
  if (boolval) {
    O_Output |= PO_TRACE_FOOC;
  } else {
    O_Output &= ~PO_TRACE_FOOC;
  }
}

/** @brief Control the output of (debugging) CCF information in the log for debugging purposes
 *
 *  @ingroup publicAPI
 *
 *  @param boolval if non-zero, the CCF information is not displayed. Otherwise it is not.
 */
void rhp_show_ccftrace(unsigned char boolval)
{
  if (boolval) {
    O_Output |= PO_TRACE_CCF;
  } else {
    O_Output &= ~PO_TRACE_CCF;
  }
}

/** @brief Control the output of (debugging) timings information in the log
 *
 *  @ingroup publicAPI
 *
 *  @param boolval if non-zero, the timings information is not displayed. Otherwise it is not.
 */
void rhp_show_timings(unsigned char boolval)
{
   rhp_options[Options_Display_Timings].value.b = boolval > 0;

}

/** @brief Control the output of (debugging) subsolver log
 *
 *  @ingroup publicAPI
 *
 *  @param boolval if non-zero, the subsolver log. Otherwise it is not.
 */
void rhp_show_solver_log(unsigned char boolval)
{
   O_Output_Subsolver_Log = boolval > 0;
}

struct log_opt {
   const char *name;
   void (*fn)(unsigned char);
   const char *help;
};

static const struct log_opt log_opts[] = {
   {"backend",     rhp_show_backendinfo,    "display backend information"},
   {"ccf",         rhp_show_ccftrace,       "lists CCF reformulation information"},
   {"container",   rhp_show_containertrace, "lists algebraic container changes"},
   {"empdag",      rhp_show_empdagtrace,    "lists EMPDAG actions"},
   {"empinterp",   rhp_show_empinterptrace, "lists EMP interpreter actions"},
   {"empparser",   rhp_show_empparsertrace, "lists EMP parser actions"},
   {"fooc",        rhp_show_fooctrace,      "lists first-order optimality conditions computation information"},
   {"process",     rhp_show_processtrace,   "lists processing actions"},
   {"refcnt",      rhp_show_refcnttrace,    "display reference counter information"},
   {"solreport",   rhp_show_solreporttrace, "display solution/values reporting actions"},
   {"solver",      rhp_show_solver_log,     "display solver log"},
   {"timings",     rhp_show_timings,        "display timings information"},
};

static const char* loglevel_strvals[] = {"all",   "error", "info",   "v",  "vv",  "vvv"};
static const unsigned loglevel_vals[] = {INT_MAX, PO_ERROR, PO_INFO, PO_V, PO_VV, PO_VVV};
static const unsigned n_loglevels = ARRAY_SIZE(loglevel_vals);

static const unsigned n_opts = ARRAY_SIZE(log_opts);

static void log_help(void)
{
   pr_info("Help for RHP_LOG values:\n\n");

   for (unsigned i = 0; i < n_opts; ++i) {
      pr_info("    %-30s %s\n", log_opts[i].name, log_opts[i].help);
   }

   pr_info("    %-30s enable all options above\n", "all");
}

static const char * const envvars_help[][2] = {
   {"RHP_COLORS", "Use colors when logging"},
   {"RHP_HELP", "Print this help and quit"},
   {"RHP_REPORT_CRASH", "Report any crash to the reporting system"},
};

/* TODO: document as advanced */
// RHP_GAMSCNTR_FILE
// RHP_GAMSDIR
// RHP_SOLVER_BACKEND
// "RHP_SOLVELINK"

static void print_option_array(unsigned nopts, const struct option opts[VMT(static restrict nopts)])
{
   for (unsigned i = 0; i < nopts; ++i) {
      pr_info("    RHP_");
      const char *opt_name =opts[i].name;
      size_t opt_name_len = strlen(opt_name);

      for (unsigned j = 0; j < opt_name_len; ++j) {
         pr_info("%c", RhpToUpper(*opt_name++));
      }

      size_t offset = 4 + opt_name_len;
      if (offset < 30) {
         int blank = 30 - offset;
         pr_info("%*s", blank, "");
      }

      pr_info(" %s\n", opts[i].description);
   }
}

static void print_envvar_help(void)
{
   pr_info("List of environment variables\n");
   pr_info("\n- The following ReSHOP options can be set by set environment variables:\n\n");

   print_option_array(Options_Last+1, rhp_options);

   pr_info("\n- The following OVF/CCF options can be set by set environment variables:\n\n");

   print_option_array(Options_Ovf_Last+1, ovf_options);

   pr_info("\n- Additionally, the following environment variables influence the ReSHOP behavior:\n\n");

   for (unsigned i = 0, len = ARRAY_SIZE(envvars_help); i <len; ++i) {
      pr_info("    %-30s %s\n", envvars_help[i][0], envvars_help[i][1]);
   }

   pr_info("\n- Finally, logging can be controlled via RHP_LOG:\n");
   log_help();
}

/**
 * @brief Synchronize the ReSHOP options and switches with the current environment variable values
 *
 * @ingroup publicAPI
 *
 * @return the error code
 */
int rhp_syncenv(void)
{
   int status = OK;
   char *env_varname = NULL;
   size_t optname_maxlen = 512;
   MALLOC_(env_varname, char, optname_maxlen + 5);

   if (getenv("RHP_HELP")) {
      print_envvar_help();
      return Error_UserInterrupted;
   }

#ifndef _WIN32
   /* RHP_NODEV=0 is equivalent to RHP_NO_STOP=1 RHP_NO_BACKTRACE=1 */
   const char *nodev = mygetenv("RHP_NODEV");
   if (nodev) {
      setenv("RHP_NO_STOP", "1", 1);
      setenv("RHP_NO_BACKTRACE", "1", 1);
   }
   myfreeenvval(nodev);
#endif

   const char * restrict env_rhplog = find_rhpenvvar("LOG", &env_varname, &optname_maxlen);

   /* If RHP_LOG is defined, then try to parse its value as "val1:val2" */
   if (env_rhplog) {
      size_t len = 0, varlen = strlen(env_rhplog);

      while (len < varlen) {
         const char * restrict envopt = &env_rhplog[len];
         unsigned envopt_len = 0;
         unsigned char val = 1;

         /* negate to disable */
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
               const char *loglevel = loglevel_strvals[i];
               if (!strncmp(loglevel, loglevel_env, strlen(loglevel))) {
                  O_Output = (int)(((unsigned)O_Output & ~(unsigned)PO_MASK_LEVEL) + loglevel_vals[i]);
                  goto _continue;
               }
            }
         }

         /* the 'all' keyword does turn on everything */
         if (!strncmp("all", envopt, 3)) {
            for (unsigned i = 0; i < n_opts; ++i) { log_opts[i].fn(val); }
            O_Output |= PO_MAX_VERBOSITY | PO_ALLDEST;
            goto _continue;
         }

         /* help */
         if (!strncmp("help", envopt, 4)) {

            log_help();
            status = Error_UserInterrupted;
            goto _exit;
         }

         error("\nERROR: wrong value '%s' for RHP_LOG environment variable\n", envopt);
         log_help();
         status = Error_RuntimeError;
         goto _exit;

_continue:
         if (env_rhplog[len] == ':') { len++; }
         else { break; }
      }
    }


_exit:
   myfreeenvval(env_rhplog);
   free(env_varname);

   if (status == OK) {
      status = ovf_syncenv();
   }

   if (status == OK) {
      const char* imgui = mygetenv("RHP_GUI");
      if (imgui) {
         status = imgui_start(imgui);
      }
      myfreeenvval(imgui);
   }

   return status;
}
