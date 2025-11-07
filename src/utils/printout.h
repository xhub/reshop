#ifndef PRINTOUT_H
#define PRINTOUT_H

#include <stdlib.h>

#include "compat.h"
#include "printout_data.h"
#include "rhp_defines.h"
#include "rhp_options.h"
#include "status.h"

/** @file printout.h*/

/** @brief Printing utilities */

void printout(unsigned mode, const char *format, ...) FORMAT_CHK(2,3);
void printstr(unsigned mode, const char *str);
void logging_syncenv(void);
void set_log_fd(rhpfd_t fd);

#define error(format, ...)   printout(PO_ERROR, format, __VA_ARGS__)
#define errormsg(msg)        printstr(PO_ERROR, msg)
#define errbug(format, ...)  printout(PO_ERROR, format " Please report his as a bug\n", __VA_ARGS__)
#define errbugmsg(format)    printout(PO_ERROR, format " Please report his as a bug\n")

void fatal_error(const char *format, ...) FORMAT_CHK(1,2);

static inline int error_runtime(void)
{
   errormsg("[ReSHOP] Unexpected runtime error\n");
   return Error_RuntimeError;
}

static inline bool has_output(unsigned mode)
{
   /* This is normal output */
   if ( !(mode & ~PO_MASK_NONTRACING) &&
         (mode & PO_MASK_LEVEL) <= ((unsigned)O_Output & PO_MASK_LEVEL) ) {
      return true;
   }

   switch (mode) {
   case PO_BACKEND:
   case PO_TRACE_REFCNT:
   case PO_TRACE_EMPINTERP:
   case PO_TRACE_EMPPARSER:
   case PO_TRACE_SOLREPORT:
   case PO_TRACE_PROCESS:
   case PO_TRACE_CONTAINER:
   case PO_TRACE_EMPDAG:
   case PO_TRACE_FOOC:
   case PO_TRACE_CCF:
      return (O_Output & mode);
   default:
      return false;
   }
}

#define logger(lvl, format, ...)    printout(lvl, format, __VA_ARGS__)
#define pr_debug(...)               printout(PO_DEBUG, __VA_ARGS__)
#define pr_info(...)                printout(PO_INFO, __VA_ARGS__)

#define chk_potrace(PO_VAL, ...) if (RHP_UNLIKELY(O_Output & (PO_VAL))) { printout(PO_VAL, __VA_ARGS__); }


#define trace_backend(...)        chk_potrace(PO_BACKEND, __VA_ARGS__)
#define trace_refcnt(format, ...) printout(PO_TRACE_REFCNT, format, __VA_ARGS__)
#define trace_empinterp(...)      chk_potrace(PO_TRACE_EMPINTERP, __VA_ARGS__)
#define trace_empinterpmsg(str)   printstr(PO_TRACE_EMPINTERP, str)
#define trace_empparser(...)      chk_potrace(PO_TRACE_EMPPARSER, __VA_ARGS__)
#define trace_empparsermsg(str)   printstr(PO_TRACE_EMPPARSER, str)
#define trace_empdag(...)         chk_potrace(PO_TRACE_EMPDAG, __VA_ARGS__)
#define trace_process(...)        chk_potrace(PO_TRACE_PROCESS, __VA_ARGS__)
#define trace_processmsg(str)     printstr(PO_TRACE_PROCESS, str)
#define trace_solreport(...)      chk_potrace(PO_TRACE_SOLREPORT, __VA_ARGS__)
#define trace_ctr(...)            chk_potrace(PO_TRACE_CONTAINER, __VA_ARGS__)
#define trace_fooc(...)           chk_potrace(PO_TRACE_FOOC, __VA_ARGS__)
#define trace_ccf(...)            chk_potrace(PO_TRACE_CCF, __VA_ARGS__)

#define trace_model               trace_backend

#define SOLREPORT_FMT             "val = % 3.2e; mult = % 3.2e; basis = %-11s"
#define solreport_gms(e)          (e)->value, (e)->multiplier, basis_name((e)->basis)
#define solreport_gms_v(v)        (v)->value, (v)->multiplier, basis_name((v)->basis)

/* Useful debug output */
#ifndef NDEBUG
#define GMOEXPORT_DEBUG(str, ...) trace_backend("[GMOexport] " str "\n", __VA_ARGS__) \
//  { GDB_STOP(); }
#else
#define GMOEXPORT_DEBUG(...)
#endif

#ifndef NDEBUG
#define SOLREPORT_DEBUG(str, ...) trace_solreport("[solreport] " str, __VA_ARGS__) \
//  { GDB_STOP(); }
#else
#define SOLREPORT_DEBUG(...)
#endif

#endif /* PRINTOUT_H */
