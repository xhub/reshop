#ifndef PRINTOUT_H
#define PRINTOUT_H

#include "compat.h"
#include "printout_data.h"
#include "rhp_options.h"
#include "status.h"

/** @file printout.h*/

/** @brief Printing utilities */

void printout(unsigned mode, const char *format, ...) FORMAT_CHK(2,3);
void printstr(unsigned mode, const char *str);
void reshop_printfn_gams(const char *msg, int mode, void *data);
void logging_syncenv(void);

#define error(format, ...) printout(PO_ERROR, format, __VA_ARGS__)
#define errormsg(msg) printstr(PO_ERROR, msg)

static inline int error_runtime(void)
{
   errormsg("unexpected runtime error");
   return Error_RuntimeError;
}

#define logger(lvl, format, ...) printout(lvl, format, __VA_ARGS__)
#define debug(...) printout(PO_DEBUG, __VA_ARGS__)

#define chk_potrace(PO_VAL, ...) if (RHP_UNLIKELY(O_Output & (PO_VAL))) { printout(PO_VAL, __VA_ARGS__); }


#define trace_stack(...)          chk_potrace(PO_STACK, __VA_ARGS__)
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

#define trace_model               trace_stack


#define SOLREPORT_FMT             "val = % 3.2e; mult = % 3.2e; basis = %-11s"
#define solreport_gms(e)          (e)->value, (e)->multiplier, basis_name((e)->basis)
#define solreport_gms_v(v)        (v)->value, (v)->multiplier, basis_name((v)->basis)

#endif /* PRINTOUT_H */
