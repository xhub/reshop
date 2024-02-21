#ifndef PRINTOUT_DATA_H
#define PRINTOUT_DATA_H

#include <limits.h>

#include "compat.h"

enum printout_modes {
   PO_SILENT         = 0,
   PO_TTY            = 0x1,
   PO_LOGF           = 0x2,
   PO_ALLDEST        = PO_TTY | PO_LOGF,
   PO_ERROR          = PO_ALLDEST,
   PO_INFO            = 0x4,
   PO_V              = 0x8,
   PO_VV             = 0xc,
   PO_VVV            = 0x10,
   PO_MAX_VERBOSITY  = PO_VVV,
   PO_DEBUG          = PO_VVV,
   PO_LEVEL          = 0xFc,
   PO_NONTRACING     = 0xFF,

   PO_STACK               = 0x100, /**< Display stack-related info                         */
   PO_TRACE_REFCNT        = 0x200, /**< Trace the refcnt changes                           */
   PO_TRACE_EMPINTERP     = 0x400, /**< Trace the interpretation of an empinfo file        */
   PO_TRACE_EMPPARSER     = 0x800, /**< Trace the parsing of empinfo file                  */
   PO_TRACE_SOLREPORT     = 0x1000, /**< Trace the reporting of solution (backward step)    */
   PO_TRACE_PROCESS       = 0x2000, /**< Trace the model processing (forward step)          */
   PO_TRACE_CONTAINER     = 0x4000, /**< Trace the model representation changes             */
   PO_TRACE_EMPDAG        = 0x8000, /**< Trace the EMPDAG definition                        */
   PO_TRACE_FOOC          = 0x10000, /**< Trace the FOOC computations                        */

};

/* Disable all regular listing */




struct reshop_print {
  void (*printout)(unsigned mode, const char *format, ...) FORMAT_CHK(2, 3);
  void (*printstr)(unsigned mode, const char *str);
};

#endif /* PRINTOUT_DATA_H */
