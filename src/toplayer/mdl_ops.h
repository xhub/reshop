#ifndef RHP_MODEL_OPS
#define RHP_MODEL_OPS

/** @file mdl_ops.h
 *
 *  @brief Common operations on model
 *
 */

#include "mathprgm_data.h"
#include "mdl_data.h"
#include "rhp_fwd.h"

typedef struct model_ops {
   int (*allocdata)(Model *mdl);
   void (*deallocdata)(Model *mdl);
   int (*checkmdl)(Model *mdl);
   int (*checkobjequvar)(const Model *mdl, rhp_idx objvar, rhp_idx objequ);
   int (*copyassolvable)(Model *mdl, Model *mdl_src);
   int (*copysolveoptions)(Model *mdl, const Model *mdl_src);
   int (*copystatsfromsolver)(Model *mdl, const Model *mdl_solver);
   int (*export)(Model *mdl, Model *mdl_dst);
   int (*exportsolvable)(Model *mdl, Model *mdl_dst);
   int (*getmodelstat)(const Model *mdl, int *modelstat);
   int (*getobjequ)(const Model *mdl, rhp_idx *objequ);
   int (*getobjjacval)(const Model *mdl, double *objjacval);
   int (*getsense)(const Model *mdl, RhpSense *objsense);
   int (*getobjvar)(const Model *mdl, rhp_idx *objvar);
   int (*getoption)(const Model *mdl, const char *option, void *val);
   int (*getsolvestat)(const Model *mdl, int *solvestat);
   int (*getsolvername)(const Model *mdl, char const ** solvername);
   int (*postprocess)(Model *mdl);
   int (*reportvalues)(Model *mdl, const Model *mdl_src);
   int (*setmodelstat)(Model *mdl, int modelstat);
   int (*setobjvar)(Model *mdl, rhp_idx vi);
   int (*setsense)(Model *mdl, RhpSense objsense);
   int (*setsolvername)(Model *mdl, const char *solvername);
   int (*setsolvestat)(Model *mdl, int solvestat);
   int (*solve)(Model *mdl);
} ModelOps;

extern const ModelOps mdl_ops_gams;
extern const ModelOps mdl_ops_rhp;
#endif
