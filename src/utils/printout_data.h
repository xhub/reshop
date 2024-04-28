#ifndef PRINTOUT_DATA_H
#define PRINTOUT_DATA_H

#include <limits.h>

#include "compat.h"

/* The printout mode is made of 3 parts:
 * xx  yyyyyyyyyyyyy zzz....
 *
 * xx: destination 
 * yy: verbosity 
 * zz: tracing information
 *
 * A tracing message is printed on all destination
 * A non-tracing message is the union of a verbosity level and a destination
 */

enum printout_modes {
   PO_LSTFILE        = 0,
   PO_TTY            = 0x1,
   PO_LOGFILE        = 0x2,
   PO_ALLDEST        = PO_TTY | PO_LOGFILE,

   PO_ERROR          = PO_ALLDEST,

   PO_LEVEL_INFO     = 0x4,
   PO_LEVEL_V        = 0x8,
   PO_LEVEL_VV       = 0xc,
   PO_LEVEL_VVV      = 0x10,
   PO_LEVEL_DEBUG    = PO_LEVEL_VVV,
   PO_MAX_VERBOSITY  = PO_LEVEL_VVV,
   PO_MASK_LEVEL     = 0xFc,

   PO_INFO           = PO_LEVEL_INFO  | PO_ALLDEST,
   PO_V              = PO_LEVEL_V     | PO_ALLDEST,
   PO_VV             = PO_LEVEL_VV    | PO_ALLDEST,
   PO_VVV            = PO_LEVEL_VVV   | PO_ALLDEST,
   PO_DEBUG          = PO_LEVEL_DEBUG | PO_ALLDEST,

   LOG_INFO          = PO_LEVEL_INFO  | PO_LOGFILE,
   LOG_V             = PO_LEVEL_V     | PO_LOGFILE,
   LOG_VV            = PO_LEVEL_VV    | PO_LOGFILE,
   LOG_VVV           = PO_LEVEL_VVV   | PO_LOGFILE,
   LOG_DEBUG         = PO_LEVEL_DEBUG | PO_LOGFILE,

   LST_INFO          = PO_LEVEL_INFO  | PO_LSTFILE,
   LST_V             = PO_LEVEL_V     | PO_LSTFILE,
   LST_VV            = PO_LEVEL_VV    | PO_LSTFILE,
   LST_VVV           = PO_LEVEL_VVV   | PO_LSTFILE,
   LST_DEBUG         = PO_LEVEL_DEBUG | PO_LSTFILE,

   TTY_INFO          = PO_LEVEL_INFO  | PO_TTY,
   TTY_V             = PO_LEVEL_V     | PO_TTY,
   TTY_VV            = PO_LEVEL_VV    | PO_TTY,
   TTY_VVV           = PO_LEVEL_VVV   | PO_TTY,
   TTY_DEBUG         = PO_LEVEL_DEBUG | PO_TTY,

   PO_NONTRACING     = 0xFF,

   PO_STACK               = 0x100,   /**< Display stack-related info                         */
   PO_TRACE_REFCNT        = 0x200,   /**< Trace the refcnt changes                           */
   PO_TRACE_EMPINTERP     = 0x400,   /**< Trace the interpretation of an empinfo file        */
   PO_TRACE_EMPPARSER     = 0x800,   /**< Trace the parsing of empinfo file                  */
   PO_TRACE_SOLREPORT     = 0x1000,  /**< Trace the reporting of solution (backward step)    */
   PO_TRACE_PROCESS       = 0x2000,  /**< Trace the model processing (forward step)          */
   PO_TRACE_CONTAINER     = 0x4000,  /**< Trace the model representation changes             */
   PO_TRACE_EMPDAG        = 0x8000,  /**< Trace the EMPDAG definition                        */
   PO_TRACE_FOOC          = 0x10000, /**< Trace the FOOC computations                        */

};

struct reshop_print {
  void (*printout)(unsigned mode, const char *format, ...) FORMAT_CHK(2, 3);
  void (*printstr)(unsigned mode, const char *str);
};

#endif /* PRINTOUT_DATA_H */
