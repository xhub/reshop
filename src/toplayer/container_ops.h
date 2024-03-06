#ifndef CONTAINER_OPS_H
#define CONTAINER_OPS_H

/** 
 *  @file container_ops.h
 *
 *  @brief Common operations on algebraic container
 *
 */

#include <stdbool.h>

#include "mdl_data.h"
#include "rhp_fwd.h"

typedef struct container_ops {
   int (*allocdata)(Container *ctr);
   void (*deallocdata)(Container *ctr);
   int (*copyvarname)(const Container *ctr, rhp_idx vi, char *name, unsigned len);
   int (*copyequname)(const Container *ctr, rhp_idx ei, char *name, unsigned len);
   int (*evalequvar)(Container *ctr);
   int (*evalfunc)(const Container *ctr, int si, double *x, double *f,
                   int *numerr);
   int (*evalgrad)(const Container *ctr, int si, double *x, double *f,
                   double *g, double *gx, int *numerr);
   int (*evalgradobj)(const Container *ctr, double *x, double *f,
                      double *g, double *gx, int *numerr);
   int (*getallequsmult)(const Container *ctr, double *mult);
   int (*getallequsval)(const Container *ctr, double *vals);
   int (*getallvarsmult)(const Container *ctr, double *mult);
   int (*getallvarsval)(const Container *ctr, double *vals);
   int (*getequsbasis)(const Container *ctr, Aequ *e, int *basis);
   int (*getequcst)(const Container *ctr, rhp_idx ei, double *val);
   int (*getequsmult)(const Container *ctr, Aequ *e, double *mult);
   int (*getequsval)(const Container *ctr, Aequ *e, double *vals);
   int (*getequbyname)(const Container *ctr, const char* name, rhp_idx *ei);
   int (*getequmult)(const Container *ctr, rhp_idx ei, double *mult);
   int (*getequperp)(const Container *ctr, rhp_idx ei, rhp_idx *vi);
   int (*getequname)(const Container *ctr, rhp_idx ei, const char ** name);
   int (*getequbasis)(const Container *ctr, rhp_idx ei, int *bstat);
   int (*getequval)(const Container *ctr, rhp_idx ei, double *level);
   int (*getequtype)(const Container *ctr, rhp_idx ei, unsigned *type, unsigned *cone);
   int (*getcoljacinfo)(const Container *ctr, rhp_idx vi, void **jacptr,
                        double *jacval, rhp_idx *ei, int *nlflag);
   int (*getrowjacinfo)(const Container *ctr, rhp_idx ei, void **jacptr,
                        double *jacval, rhp_idx *vi, int *nlflag);
   int (*getspecialfloats)(const Container *ctr, double *minf, double *pinf, double* nan);
   int (*getvarbounds)(const Container *ctr, rhp_idx vi, double *lb, double *ub);
   int (*getvarbyname)(const Container *ctr, const char* name, rhp_idx *vi);
   int (*getvarbasis)(const Container *ctr, rhp_idx vi, int *bstat);
   int (*getvarmult)(const Container *ctr, rhp_idx vi, double *mult);
   int (*getvarperp)(const Container *ctr, rhp_idx vi, rhp_idx *ei);
   int (*getvarname)(const Container *ctr, rhp_idx vi, const char ** name);
   int (*getvarlb)(const Container *ctr, rhp_idx vi, double *lower_bound);
   int (*getvarub)(const Container *ctr, rhp_idx vi, double *upper_bound);
   int (*getvarval)(const Container *ctr, rhp_idx vi, double *level);
   int (*getvartype)(const Container *ctr, rhp_idx vi, unsigned *type);
   int (*getvarsbasis)(const Container *ctr, Avar *v, int *basis);
   int (*getvarsmult)(const Container *ctr, Avar *v, double *mult);
   int (*getvarsval)(const Container *ctr, Avar *v, double *vals);
   int (*isequNL)(const Container *ctr, rhp_idx ei, bool *isNL);
   int (*resize)(Container *ctr, unsigned n, unsigned m);
   int (*setequbasis)(Container *ctr, rhp_idx ei, int bstat);
   int (*setequcst)(Container *ctr, rhp_idx ei, double val);
   int (*setequname)(Container *ctr, rhp_idx ei, const char *name);
   int (*setequmult)(Container *ctr, rhp_idx vi, double equm);
   int (*setequtype)(Container *ctr, rhp_idx ei, unsigned type, unsigned cone);
   int (*setequval)(Container *ctr, rhp_idx ei, double val);
   int (*setequvarperp)(Container *ctr, rhp_idx ei, rhp_idx vi);
   int (*setvarbasis)(Container *ctr, rhp_idx vi, int bstat);
   int (*setvarbounds)(Container *ctr, rhp_idx vi, double lb, double ub);
   int (*setvarlb)(Container *ctr, rhp_idx vi, double lb);
   int (*setvarname)(Container *ctr, rhp_idx vi, const char *name);
   int (*setvarmult)(Container *ctr, rhp_idx vi, double varm);
   int (*setvartype)(Container *ctr, rhp_idx vi, unsigned type);
   int (*setvarub)(Container *ctr, rhp_idx vi, double ub);
   int (*setvarval)(Container *ctr, rhp_idx vi, double val);
} CtrOps;

extern const CtrOps ctr_ops_gams;
extern const CtrOps ctr_ops_rhp;
extern const CtrOps ctr_ops_julia;


#endif // !CONTAINER_OPS_H

