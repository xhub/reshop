#ifndef GAMS_LOGGING_H
#define GAMS_LOGGING_H

#include <stdbool.h>
#include "rhp_fwd.h"

#include "gevmcc.h"

typedef struct gevStatLogCallbackNewGev {
   gevHandle_t gev_upstream;    /**< Upstream gev                             */
} gevStatLogCallbackGevUpstream;

typedef struct gevStatLogCallbackSameGev {
   Tgevlswrite_t lswrite;       /**< The original GEV callback                */
   void *usrmem;                /**< The original GEV custom data             */
} gevStatLogCallbackSameGev;

typedef union {
   gevStatLogCallbackGevUpstream gevupstream;
   gevStatLogCallbackSameGev samegev;
} gevStatLogCallbackData;

typedef struct {
   char prefixchar;             /**< prefix character, by default '='         */
   bool copymode;               /**< true if in copy mode, false otherwise    */
   Model *mdl_solver;           /**< The solver GAMS/GMO model */
   Model *mdl_cmex;             /**< The GAMS/GMO model from CMEX */
   Tgevlswrite_t loggerFn;      /**< The logger function */
   gevStatLogCallbackData loggerDat; /**< The logger data */
} GevLoggingCallbackData;

typedef enum {
   GevLoggerCbUpstream,  /**< Use an upstream GEV object (distinct from solver one) */
   GevLoggerCbSame,      /**< Log using the same GEV as the one inhereted from GAMS */
   GevLoggerCbRhp,       /**< Use the ReSHOP print function                         */
} gevStatLogCallbackType;

typedef enum {
   GevStatusMsg = 1,
   GevLogMsg    = 2,
} GevLoggingMode;

NONNULL
int gev_logger_callback_init(Model *mdl_solver, gevStatLogCallbackType cbtype);

#endif /* GAMS_LOGGING_H */
