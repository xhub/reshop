#ifndef MDL_GAMS_H
#define MDL_GAMS_H

#include <stdbool.h>

#include "mdl_data.h"
#include "rhp_fwd.h"

#include "gclgms.h"

/** @file mdl_gams.h
 *
 *  @brief GAMS specific model parts
 */

/** @brief GAMS specific model data */
typedef struct gams_modeldata {
   char solvername[GMS_SSSIZE];        /**< Name of the solver */
   char logname[GMS_SSSIZE];           /**< */
   char statusname[GMS_SSSIZE];        /**< */
   char gamscntr[GMS_SSSIZE];          /**< path to the GAMS control file  */
   char gamsdir[GMS_SSSIZE];           /**< directory for the GAMS install */
   char scrdir[GMS_SSSIZE];            /**< working directory              */

   int last_solverid;
   bool delete_scratch;
   void *slvptr;
} GmsModelData;

int gams_chk_mdl(const Model* mdl, const char *fn) NONNULL;
int gams_chk_mdlfull(const Model* mdl, const char *fn) NONNULL;

int gmdl_cdat_setup(Model *mdl_gms, Model *mdl_src) NONNULL;

int gmdl_setprobtype(Model *mdl, enum mdl_probtype probtype) NONNULL;
int gmdl_set_gamsdata_from_env(Model *mdl) NONNULL;
int gmdl_writeasgms(const Model *mdl, const char *filename) NONNULL;
int gmdl_writesol2gdx(Model *mdl, const char *gdxname) NONNULL;

char* gams_getgamsdir(void);
char* gams_getgamscntr(void);
int gams_setgamsdir(const char *dirname);
int gams_setgamscntr(const char *fname);

int gmdl_ensuresimpleprob(Model *mdl) NONNULL;

void gams_unload_libs(void);
int gams_load_libs(const char *sysdir) NONNULL;

#endif
