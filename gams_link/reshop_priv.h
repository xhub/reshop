#ifndef RESHOP_PRIV_H
#define RESHOP_PRIV_H

#include "rhp.h"
#include "gevmcc.h"
#include "cfgmcc.h"

#include "reshop.h"

#define LOGMASK         0x1
#define STATUSMASK      0x2
#define ALLMASK         (LOGMASK | STATUSMASK)

// TODO: delete
struct timeinfo {
   double start_readyapi;       /**< start time of ready api procedure      */
   double end_readyapi;         /**< end time of ready api procedure        */
   double start_callsolver;     /**< start time of call solver procedure    */
   double end_callsolver;       /**< end time of call solver procedure      */
   double start_func;
   double cum_func;
   double start_grad;
   double cum_grad;
};

struct rhpRec {
   optHandle_t oh;              /**< GAMS option object                     */
   gmoHandle_t gh;              /**< GAMS model object                      */
   gevHandle_t eh;              /**< GAMS environment object                */
   dctHandle_t dh;              /**< GAMS dictionary object                 */
   cfgHandle_t ch;              /**< GAMS configuration object              */
   int oh_created;              /**< TRUE if GAMS option object was created */

   struct timeinfo timeinfo;    /**< record timing statistics               */
   struct rhp_mdl *mdl;         /**< ReSHOP model storage */
};

#if defined(__cplusplus)
extern "C" {
#endif

int opt_pushtosolver(rhpRec_t *jh);
int opt_process(rhpRec_t *jh, bool need_init, const char* sysdir);
void printgams(void *env, unsigned mode, const char *str);
void flushgams(void* env);

#if defined(__cplusplus)
}
#endif

#define xstr(s) str(s)
#define str(s) #s
#define CAT(a,b) a##b
#define _APIVER(_GMS_PREFIX) CAT(_GMS_PREFIX,APIVERSION) 


/* This is used to check that the library versions are compatible in both the
 * GAMS driver and reshop */
#define CHK_CORRECT_LIBVER(_gms_prefix, _GMS_PREFIX) \
   if (!_gms_prefix ## CorrectLibraryVersion(msg, sizeof(msg))) { \
      gevLogStat(jh->eh, "[WARNING] " # _GMS_PREFIX  " API version differ: ReSHOP compiled with" \
      " " xstr(_APIVER(_GMS_PREFIX)) ". Error message follows:\n"); \
      gevLogStat(jh->eh, msg); \
      gevLogStat(jh->eh, "This may lead to runtime failures. Continue at your own risk, or try to update ReSHOP\n"); \
   }

#endif /* RESHOP_PRIV_H */
