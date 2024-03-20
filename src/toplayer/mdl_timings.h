#ifndef MDL_TIMINGS_H
#define MDL_TIMINGS_H

#include "rhp_fwd.h"

typedef struct ccf_timings {
   unsigned number;
   double min;
   double max;
   double mean;
} SimpleTiming;

typedef struct conj_ccf_timings {
   SimpleTiming stats;
   SimpleTiming PPL;
} ConjugateCcfTiming;

typedef struct path_timings {
   double func_evals;
   double jacobian_evals;
   double path_solve;
} PathTiming;

typedef struct mdl_timings {
   struct {
      double gmo2rhp;
      struct {
         double interp;
         double empdag_edges_built;
      } empinfo_file;
   } gams;

   struct {
      SimpleTiming mdl_creation;
   } rhp;

   struct {
      SimpleTiming analysis;
   } empdag;

   struct {
      struct {
         double flipped;
         double dualvar;
         double total;
      } container;
      struct {
         SimpleTiming equilibrium;
         SimpleTiming fenchel;
         ConjugateCcfTiming conjugate;
         double total;
      } CCF;
   } reformulation;

   double gmo_creation;

   struct {
      double presolve_wall;
      double fooc;
      double solver_wall;
   } solve;

   double postprocessing;

   double wall_start;
   double wall_end;
   double thrd_start;
   double thrd_end;
   double proc_start;
   double proc_end;

   unsigned refcnt;

} Timings;

void simple_timing_add(SimpleTiming *t, double newtime) NONNULL;

int mdl_timings_alloc(Model *mdl) NONNULL;
void mdl_timings_print(const Model *mdl, unsigned mode) NONNULL;

Timings* mdl_timings_borrow(Timings *t) NONNULL;
void mdl_timings_rel(Timings *t);

#endif
