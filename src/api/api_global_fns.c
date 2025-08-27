#include "env_utils.h"
#include "git_version.h"
#include "macros.h"
#include "ovf_options.h"
#include "printout.h"
#include "reshop.h"
#include "rhp_options.h"
#include "rhpgui_launcher.h"
#include "status.h"

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
#   error "Unkonwn architecture, please modify the source code"
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

         /* help */
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

#ifndef _WIN32
   /* RHP_DEV=0 is equivalent to RHP_NO_STOP=1 RHP_NO_BACKTRACE=1 */
   const char *nodev = mygetenv("RHP_NODEV");
   if (nodev) {
      setenv("RHP_NO_STOP", "1", 1);
      setenv("RHP_NO_BACKTRACE", "1", 1);
   }
   myfreeenvval(nodev);
#endif


_exit:
   myfreeenvval(env_varval);


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
