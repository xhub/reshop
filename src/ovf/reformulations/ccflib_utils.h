#ifndef CCFLIB_UTILS_H
#define CCFLIB_UTILS_H

#include "ovfinfo.h"
#include "rhp_LA.h"
#include "var.h"

typedef struct {
   Avar y;                     /**< Variable of the active dual MP */
   OvfOpsData ovfd;
   const OvfOps *ops;
   Equ *eobj;
   SpMat B;
   double *b;
} CcflibInstanceData;

int mp_ccflib_instantiate(MathPrgm *mp_instance, MathPrgm *mp_ccflib,
                          CcflibInstanceData *instancedat);


#endif
