#ifndef CONTAINER_OPS_H
#define CONTAINER_OPS_H

/** 
 *  @file container_ops.h
 *
 *  @brief Common operations on an algebraic container
 *
 */

#include <stdbool.h>

#include "equ_data.h"
#include "mdl_data.h"
#include "rhp_fwd.h"

typedef struct ctr_ops {
   int (*allocdata)       (Container *ctr);
   void (*deallocdata)    (Container *ctr);
   int (*copyvarname)     (const Container *ctr, rhp_idx vi, char *name, unsigned len);
   int (*copyequname)     (const Container *ctr, rhp_idx ei, char *name, unsigned len);
   int (*evalequvar)      (Container *ctr);
   int (*evalfunc)        (const Container *ctr, int si, double *x, double *f,
                           int *numerr);
   int (*evalgrad)        (const Container *ctr, int si, double *x, double *f,
                           double *g, double *gx, int *numerr);
   int (*evalgradobj)     (const Container *ctr, double *x, double *f,
                           double *g, double *gx, int *numerr);
   int (*equvarcounts)    (Container *ctr);
   int (*getallequsbasis) (const Container *ctr, int* basis_infos);
   int (*getallequsdual)  (const Container *ctr, double *dual);
   int (*getallequslevel) (const Container *ctr, double *level);
   int (*getallvarsbasis) (const Container *ctr, int* basis_infos);
   int (*getallvarsdual)  (const Container *ctr, double *dual);
   int (*getallvarslevel) (const Container *ctr, double *level);
   int (*getequsbasis)    (const Container *ctr, Aequ *e, int *ebasis_info);
   int (*getequcst)       (const Container *ctr, rhp_idx ei, double *cst);
   int (*getequsdual)     (const Container *ctr, Aequ *e, double *edual);
   int (*getequslevel)    (const Container *ctr, Aequ *e, double *elevel);
   int (*getequbyname)    (const Container *ctr, const char* name, rhp_idx *ei);
   int (*getequdual)      (const Container *ctr, rhp_idx ei, double *dual);
   int (*getequperp)      (const Container *ctr, rhp_idx ei, rhp_idx *vi);
   int (*getequname)      (const Container *ctr, rhp_idx ei, const char ** name);
   int (*getequbasis)     (const Container *ctr, rhp_idx ei, int *bstat);
   int (*getequlevel)     (const Container *ctr, rhp_idx ei, double *level);
   int (*getequtype)      (const Container *ctr, rhp_idx ei, unsigned *type, unsigned *cone);
   int (*getequexprtype)  (const Container *ctr, rhp_idx ei, EquExprType *type);
   int (*getcoljacinfo)   (const Container *ctr, rhp_idx vi, void **jacptr,
                           double *jacval, rhp_idx *ei, int *nlflag);
   int (*getrowjacinfo)   (const Container *ctr, rhp_idx ei, void **jacptr,
                           double *jacval, rhp_idx *vi, int *nlflag);
   int (*getspecialfloats)(const Container *ctr, double *minf, double *pinf, double* nan);
   int (*getvarbyname)    (const Container *ctr, const char* name, rhp_idx *vi);
   int (*getvarbounds)    (const Container *ctr, rhp_idx vi, double *lb, double *ub);
   int (*getvarbasis)     (const Container *ctr, rhp_idx vi, int *basis_info);
   int (*getvardual)      (const Container *ctr, rhp_idx vi, double *dual);
   int (*getvarperp)      (const Container *ctr, rhp_idx vi, rhp_idx *ei);
   int (*getvarname)      (const Container *ctr, rhp_idx vi, const char ** name);
   int (*getvarlb)        (const Container *ctr, rhp_idx vi, double *lower_bound);
   int (*getvarub)        (const Container *ctr, rhp_idx vi, double *upper_bound);
   int (*getvarlevel)     (const Container *ctr, rhp_idx vi, double *level);
   int (*getvartype)      (const Container *ctr, rhp_idx vi, unsigned *type);
   int (*getvarsbasis)    (const Container *ctr, Avar *v, int *vbasis_info);
   int (*getvarsdual)     (const Container *ctr, Avar *v, double *vdual);
   int (*getvarslevel)    (const Container *ctr, Avar *v, double *vlevel);
   int (*isequNL)         (const Container *ctr, rhp_idx ei, bool *isNL);
   int (*isequcst)        (const Container *ctr, rhp_idx ei, bool *iscst);
   int (*resize)          (Container *ctr, unsigned n, unsigned m);
   int (*setequbasis)     (Container *ctr, rhp_idx ei, int basis_info);
   int (*setequcst)       (Container *ctr, rhp_idx ei, double cst);
   int (*setequdual)      (Container *ctr, rhp_idx ei, double dual);
   int (*setequname)      (Container *ctr, rhp_idx ei, const char *name);
   int (*setequtype)      (Container *ctr, rhp_idx ei, unsigned type, unsigned cone);
   int (*setequlevel)     (Container *ctr, rhp_idx ei, double level);
   int (*setequvarperp)   (Container *ctr, rhp_idx ei, rhp_idx vi);
   int (*setvarbasis)     (Container *ctr, rhp_idx vi, int basis_info);
   int (*setvarbounds)    (Container *ctr, rhp_idx vi, double lb, double ub);
   int (*setvarlb)        (Container *ctr, rhp_idx vi, double lb);
   int (*setvarname)      (Container *ctr, rhp_idx vi, const char *name);
   int (*setvardual)      (Container *ctr, rhp_idx vi, double dual);
   int (*setvartype)      (Container *ctr, rhp_idx vi, unsigned type);
   int (*setvarub)        (Container *ctr, rhp_idx vi, double ub);
   int (*setvarlevel)     (Container *ctr, rhp_idx vi, double level);
} CtrOps;


extern const CtrOps ctr_ops_gams;
extern const CtrOps ctr_ops_rhp;
extern const CtrOps ctr_ops_julia;

#endif // !CONTAINER_OPS_H

