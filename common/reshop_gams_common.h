#ifndef RESHOP_GAMS_COMMON_H
#define RESHOP_GAMS_COMMON_H

#include <stdbool.h>

#include "cfgmcc.h"
#include "dctmcc.h"
#include "gevmcc.h"
#include "gmomcc.h"
#include "optcc.h"

#ifdef GAMS_BUILD
#include "palmcc.h"
#endif


#define LOGMASK         0x1
#define STATUSMASK      0x2
#define ALLMASK         (LOGMASK | STATUSMASK)

typedef struct rhpRec {
   optHandle_t oh;              /**< GAMS option object                     */
   gmoHandle_t gh;              /**< GAMS model object                      */
   gevHandle_t eh;              /**< GAMS environment object                */
   dctHandle_t dh;              /**< GAMS dictionary object                 */
   cfgHandle_t ch;              /**< GAMS configuration object              */
#ifdef GAMS_BUILD
   palHandle_t ph;              /**< GAMS PAL object                        */
#endif
   int oh_created;              /**< TRUE if GAMS option object was created */

   struct rhp_mdl *mdl;         /**< ReSHOP model storage */
} rhpRec_t;

int opt_pushtosolver(rhpRec_t *jh);
int opt_process(rhpRec_t *jh, bool need_init, const char* sysdir);

#endif
