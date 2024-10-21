#ifndef CTRDAT_GAMS_H
#define CTRDAT_GAMS_H

#include <stdbool.h>

#include "internal_model_common.h"

#include "gmomcc.h"
#include "gevmcc.h"
#include "dctmcc.h"
#include "cfgmcc.h"
#include "gdxcc.h"

typedef struct gams_modeldata GmsModelData;

/** GAMS-specific container data */
typedef struct ctrdata_gams {

   bool owndct;                          /**< If true, owns the dct object   */
   bool owning_handles;                  /**< If true, owns the GAMS objects, except dct */
   bool initialized;

   struct equvar_eval equvar_eval;      /**< Evaluation of variables by equations */

   double *rhsdelta;
   int *sos_group;

   enum gevCallSolverSolveLink solvelink;

   gmoHandle_t gmo;
   gevHandle_t gev;
   dctHandle_t dct;
   cfgHandle_t cfg;
} GmsContainerData;

int gcdat_init_withdct(GmsContainerData *gms, GmsModelData *mdldat) NONNULL;
int gcdat_loadmdl(GmsContainerData *gms, GmsModelData *mdldat) NONNULL;
void gcdat_free_handles(GmsContainerData *gms) NONNULL;

#endif
