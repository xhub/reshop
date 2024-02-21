#ifndef RESHOP_SOLVERS_H
#define RESHOP_SOLVERS_H

#include "mdl_timings.h"
#include "rhp_fwd.h"

struct jacdata;
struct logh5;

struct pathvi_env {
   Container *ctr;
   struct jacdata *jacdata;
   int (*eval_func)(Container *ctr, double *x, double *F);
   int (*eval_jacobian)(Container *ctr, struct jacdata *jacdata, double *x, double *F, int *p, int *i, double *vals);
};

struct path_env {
   Container *ctr;
   struct jacdata *jacdata;
   struct logh5 *logh5;
   struct path_timings timings;
   int (*eval_func)(Container *ctr, double *x, double *F);
   int (*eval_jacobian)(Container *ctr, struct jacdata *jacdata, double *x, double *F, int *p, int *i, double *vals);
};

int rmdl_solve_asmcp(Model *mdl) NONNULL;

int solver_path(Model * restrict mdl, struct jacdata * restrict jac);


#endif /* RESHOP_SOLVERS_H  */

