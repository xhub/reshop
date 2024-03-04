#ifndef CTRDAT_GAMS_H
#define CTRDAT_GAMS_H

#include <stdbool.h>

#include "internal_model_common.h"

#include "gmomcc.h"
#include "gevmcc.h"
#include "dctmcc.h"
#include "cfgmcc.h"
#include "gdxcc.h"

struct gams_modeldata;

typedef struct ctrdata_gams {

   bool owndct;                          /**< If true, owns the dct object   */
   bool owning_handles;
   bool initialized;

   struct equvar_eval equvar_eval;      /**< Evaluation of variables by equations */

   void *info;
   double *rhsdelta;
   int *sos_group;

   gmoHandle_t gmo;
   gevHandle_t gev;
   dctHandle_t dct;
   cfgHandle_t cfg;
} GmsContainerData;

int gcdat_init(GmsContainerData *gms, struct gams_modeldata *mdldat) NONNULL;
int gcdat_loadmdl(GmsContainerData *gms, struct gams_modeldata *mdldat) NONNULL;
void gcdat_rel(GmsContainerData *gms) NONNULL;

#endif
