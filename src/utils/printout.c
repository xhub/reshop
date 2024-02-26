#include "reshop_config.h"

#include <signal.h>
#include <stdarg.h>
#include <stdio.h>
#include <stdlib.h>

#if defined(__linux__) || defined (__APPLE__)
#include <unistd.h>
#endif

#ifdef _WIN32
#define WIN32_LEAN_AND_MEAN
#define VC_EXTRALEAN
#include <windows.h>
#endif

#include "macros.h"
#include "printout.h"
#include "reshop.h"
#include "tlsdef.h"

#define ANSI_COLOR_RED     "\x1b[31m"
#define ANSI_COLOR_GREEN   "\x1b[32m"
#define ANSI_COLOR_YELLOW  "\x1b[33m"
#define ANSI_COLOR_BLUE    "\x1b[34m"
#define ANSI_COLOR_MAGENTA "\x1b[35m"
#define ANSI_COLOR_CYAN    "\x1b[36m"
#define ANSI_COLOR_RESET   "\x1b[0m"


#define ANSI_BOLD_RED     "\x1b[31;1m"
#define ANSI_BOLD_GREEN   "\x1b[32;1m"
#define ANSI_BOLD_YELLOW  "\x1b[33;1m"
#define ANSI_BOLD_BLUE    "\x1b[34;1m"
#define ANSI_BOLD_MAGENTA "\x1b[35;1m"
#define ANSI_BOLD_CYAN    "\x1b[36;1m"
#define ANSI_BOLD_RESET   "\x1b[0m"

#define ANSI_CURSOR_SAVE     "\x1b[s"
#define ANSI_CURSOR_RESTORE  "\x1b[u"

#define COLOR_REFCNT       "\x1b[38;5;241m"
#define COLOR_EMPPARSER    "\x1b[38;5;82m"
#define COLOR_SOLREPORT    "\x1b[38;5;59m"
#define COLOR_PROCESS    "\x1b[38;5;14m"
#define COLOR_CONTAINER    "\x1b[38;5;208m"

static inline const char *get_mode_color(unsigned mode)
{
   switch (mode) {
   case PO_STACK:
      return ANSI_COLOR_BLUE;
   case PO_TRACE_REFCNT:
      return COLOR_REFCNT;
   case PO_TRACE_EMPDAG:
      return COLOR_PROCESS;
   case PO_TRACE_EMPINTERP:
      return COLOR_EMPPARSER;
   case PO_TRACE_SOLREPORT:
      return COLOR_SOLREPORT;
   case PO_TRACE_PROCESS:
      return COLOR_PROCESS;
   case PO_TRACE_CONTAINER:
      return COLOR_CONTAINER;
   default:
   return "";
   }

}

#if !defined(STDERR_FILENO) && defined(_WIN32)
#define STDERR_FILENO _fileno( stderr )
#endif

#ifdef WITH_BACKTRACE

#if defined(__linux__) || defined (__APPLE__)

#ifdef WITH_BACKWARD

#include "bck_dotrace.h"
#define backtrace bck_dotrace
#define regsig bck_regsig
tlsvar void *bck_obj = NULL;

static DESTRUCTOR_ATTR void cleanup_bck(void)
{
   if (bck_obj) {
      bck_clean(bck_obj);
   }
}

static CONSTRUCTOR_ATTR void register_signals(void)
{
   bck_obj = regsig();
}

#else /* WITH_BACKWARD  */

#ifdef WITH_BACKTRACE

#include <inttypes.h>

#define UNW_LOCAL_ONLY
#include <libunwind.h>

static void backtrace(void)
{
   unw_cursor_t cursor;
   unw_context_t container;

   unw_getcontext(&container);
   unw_init_local(&cursor, &container);

   int n=0;
   while ( unw_step(&cursor) ) {
      unw_word_t ip, sp, off;

      unw_get_reg(&cursor, UNW_REG_IP, &ip);
      unw_get_reg(&cursor, UNW_REG_SP, &sp);

      char symbol[256] = {"<unknown>"};
      char *name = symbol;
      int status;
      status = unw_get_proc_name(&cursor, symbol, sizeof(symbol), &off);

      if (!status) {
         name = symbol;
      } else {
         return;
      }

      printf("#%-2d 0x%016" PRIxPTR " sp=0x%016" PRIxPTR " %s + 0x%" PRIxPTR "\n",
            ++n,
            (uintptr_t)ip,
            (uintptr_t)sp,
            name,
            (uintptr_t)off);

      if ( name != symbol )
         free(name);
   }
}

#else

static void backtrace(void) {};

#endif /* WITH_BACKTRACE */

#endif /*  WITH_BACKWARD  */



#else /* __linux__  */

#include <windows.h>
#include <winbase.h>
#include <DbgHelp.h>

static void backtrace(void)
{
     unsigned       i;
     void         * stack[ 100 ];
     unsigned short frames;
     SYMBOL_INFO  * symbol;
     HANDLE         process;

     process = GetCurrentProcess();

     SymInitialize( process, NULL, TRUE );

     frames               = CaptureStackBackTrace( 0, 100, stack, NULL );
     symbol               = ( SYMBOL_INFO * )calloc( sizeof( SYMBOL_INFO ) + 256 * sizeof( char ), 1 );
     symbol->MaxNameLen   = 255;
     symbol->SizeOfStruct = sizeof( SYMBOL_INFO );

     for( i = 0; i < frames; i++ )
     {
         SymFromAddr( process, ( DWORD64 )( stack[ i ] ), 0, symbol );

         printf( "%u %s - 0x%0X\n", frames - i - 1, symbol->Name, symbol->Address );
     }

     free( symbol );
}

#endif /*  __linux__  */

#else /*  WITH_BACKTRACE */

#if (defined(__linux__) || defined(__APPLE__))
#include <execinfo.h>
#include "reshop_error_handling.h"

static void _sighdl_backtrace(int sigcode, siginfo_t* info, void* _ctr)
{
   void *array[90];
   int size = sizeof array / sizeof array[0];

   size = backtrace(array, size);

#if (defined(_XOPEN_SOURCE) && _XOPEN_SOURCE >= 700) || (defined(_POSIX_C_SOURCE) && _POSIX_C_SOURCE >= 200809L)
   psiginfo(info, 0);
#elif defined(_DEFAULT_SOURCE) || defined(_DARWIN_C_SOURCE)
   psignal(sigcode, "ReSHOP is in trouble!");
#else
   puts("ReSHOP caught a signal: ");
   char nb[4];
   snprintf(nb, sizeof nb, "%d\n", sigcode);
   fputs(nb, stderr);
#endif

   backtrace_symbols_fd(array, size, STDERR_FILENO);

   reshop_fatal_error(Error_SystemError, "ReSHOP caught a signal");
}

static CONSTRUCTOR_ATTR void register_signals(void)
{
   const int posix_signals[] = {
		// Signals for which the default action is "Core".
		SIGABRT,    // Abort signal from abort(3)
		SIGBUS,     // Bus error (bad memory access)
		SIGFPE,     // Floating point exception
		SIGILL,     // Illegal Instruction
//		SIGIOT,     // IOT trap. A synonym for SIGABRT; rm by xhub
		SIGQUIT,    // Quit from keyboard
		SIGSEGV,    // Invalid memory reference
		SIGSYS,     // Bad argument to routine (SVr4)
		SIGTRAP,    // Trace/breakpoint trap
		31,         // Synonymous with SIGSYS
//		SIGUNUSED,  // Synonymous with SIGSYS
		SIGXCPU,    // CPU time limit exceeded (4.2BSD)
		SIGXFSZ,    // File size limit exceeded (4.2BSD)
	};

   for (size_t i = 0; i < sizeof(posix_signals)/sizeof(int); ++i) {
      struct sigaction action;
      memset(&action, 0, sizeof action);
      action.sa_flags = (SA_SIGINFO | SA_NODEFER | SA_RESETHAND);
      sigfillset(&action.sa_mask);
      sigdelset(&action.sa_mask, posix_signals[i]);
      action.sa_sigaction = &_sighdl_backtrace;

      SYS_CALL(sigaction(posix_signals[i], &action, 0));

   }
}
#endif /* (defined(__linux__) || defined(__APPLE__)) */

#endif /* WITH_BACKTRACE */


/* ------------------------------------------------------------------------
 * Start the actual printing relation functions
 * ------------------------------------------------------------------------ */


/** @brief printing utilities */
struct printout_ops {
   void *data;                      /**< opaque data storage */
   void (*flush)(void *data);
   rhp_print_fn print;              /**< print function */
   bool use_asciicolors;            /**< If true, uses ascii colors */
};

static void print_stdout(void *data, unsigned mode, const char *buf);
static void flush_stdout(void *data);


static const struct printout_ops printops_default = {
   .data  = NULL,
   .flush = flush_stdout,
   .print = print_stdout,
   .use_asciicolors = true,
};

static tlsvar struct printout_ops print_ops = {
   .data = NULL,
   .flush = flush_stdout,
   .print = print_stdout,
   .use_asciicolors = true
};

CONSTRUCTOR_ATTR_PRIO(1000) void logging_syncenv(void)
{
   rhp_syncenv();

   /* See https://no-color.org/ */
   const char *no_color = mygetenv("NO_COLOR");

   if (no_color && no_color[0] != '\0') {
      print_ops.use_asciicolors = false;
      myfreeenvval(no_color);
      return;
   }
   myfreeenvval(no_color);

   /* See https://superuser.com/questions/413073/windows-console-with-ansi-colors-handling */
#ifdef _WIN32
   goto _exit; /* TODO: doesn't work on wine*/

#if 0
   HANDLE hOut = GetStdHandle(STD_OUTPUT_HANDLE);
   DWORD dwMode = 0;

   if (!hOut || hOut == INVALID_HANDLE_VALUE) { goto _exit; }
   if (!GetConsoleMode(hOut, &dwMode)) { goto _exit; }
   dwMode |= ENABLE_VIRTUAL_TERMINAL_PROCESSING;
   if (!SetConsoleMode(hOut, dwMode)) { goto _exit; }
   return;
#endif
_exit:
   print_ops.use_asciicolors = false;

#endif
    }
static inline bool do_print(unsigned mode, bool *mode_has_color)
{
   /* This is normal output */
   if ( !(mode & ~PO_NONTRACING) && (mode & PO_MASK_LEVEL) <= (O_Output & PO_MASK_LEVEL) ) {
      *mode_has_color = false;
      return true;
   }

   switch (mode) {
   case PO_STACK:
   case PO_TRACE_REFCNT:
   case PO_TRACE_EMPINTERP:
   case PO_TRACE_EMPPARSER:
   case PO_TRACE_SOLREPORT:
   case PO_TRACE_PROCESS:
   case PO_TRACE_CONTAINER:
   case PO_TRACE_EMPDAG:
   case PO_TRACE_FOOC:
      return (O_Output & mode);
   default:
      return false;
   }
}

/** @brief print facility
 *
 *  @param mode   the mode to use
 *  @param format the format string
 *  @param ...    the arguments for format
 */
void printout(unsigned mode, const char *format, ...)
{
   bool mode_has_color = true;
   if (do_print(mode, &mode_has_color) && format) {
      va_list ap;
      char *buf = NULL;
      int rc = 0;

      va_start(ap, format);
      rc = vsnprintf(buf, rc, format, ap);
      va_end(ap);

      if (rc <= 0) {
         return;
      }
      rc++; /* for '\0' */
      buf = malloc(rc);
      if (!buf) return;

      va_start(ap, format);
      rc = vsnprintf(buf, rc, format, ap);
      va_end(ap);

      if (rc <= 0) {
         FREE(buf);
         return;
      }

      unsigned mode_ops = mode & PO_ALLDEST;
      if (print_ops.use_asciicolors && mode_has_color) {
         print_ops.print(print_ops.data, mode_ops, get_mode_color(mode));
         print_ops.print(print_ops.data, mode_ops, buf);
         print_ops.print(print_ops.data, mode_ops, ANSI_COLOR_RESET);
      } else {
         print_ops.print(print_ops.data, mode_ops, buf);
      }

      FREE(buf);
   }

#ifdef WITH_BACKTRACE
   if (mode == PO_ERROR && !getenv("RHP_NO_BACKTRACE")) {
      print_ops.flush(print_ops.data);
      backtrace();
      if (!getenv("RHP_NO_STOP")) { GDB_STOP() }
   }
#endif

   }

void printstr(unsigned mode, const char *str)
{
   bool mode_has_color = true;
   if (do_print(mode, &mode_has_color) && str) {

      unsigned mode_ops = mode & PO_ALLDEST;
      if (print_ops.use_asciicolors && mode_has_color) {
         print_ops.print(print_ops.data, mode_ops, get_mode_color(mode));
         print_ops.print(print_ops.data, mode_ops, str);
         print_ops.print(print_ops.data, mode_ops, ANSI_COLOR_RESET);
      } else {
         print_ops.print(print_ops.data, mode_ops, str);
      }

   }

#ifdef WITH_BACKTRACE
   if (mode == PO_ERROR && !getenv("RHP_NO_BACKTRACE")) {
      backtrace();
      if (!getenv("RHP_NO_STOP")) { GDB_STOP() }
   }
#endif
}

static void flush_stdout(UNUSED void *data)
{
   fflush(stdout); /*NOLINT(bugprone-unused-return-value,cert-err33-c)*/
}

static void print_stdout(UNUSED void *data, unsigned mode, const char *buf)
{
   if (buf) {
      if (mode == PO_ERROR) {
         fputs(buf, stderr); /*NOLINT(bugprone-unused-return-value,cert-err33-c)*/
      } else {
         fputs(buf, stdout); /*NOLINT(bugprone-unused-return-value,cert-err33-c)*/
      }
   }
}

void rhp_set_printops(void* data, rhp_print_fn print, rhp_flush_fn flush,
                      bool use_asciicolors)
{
   print_ops.data = data;
   print_ops.flush = flush;
   print_ops.print = print;
   print_ops.use_asciicolors = use_asciicolors;
}

void rhp_set_printopsdefault(void)
{
   print_ops.data = printops_default.data;
   print_ops.print = printops_default.print;
   print_ops.use_asciicolors = printops_default.use_asciicolors;
}


