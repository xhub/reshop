#include "reshop_config.h"
#include "asprintf.h"

#include <math.h>
#include <string.h>

#include "mdl.h"
#include "mdl_timings.h"
#include "printout.h"
#include "print_utils.h"
#include "timings.h"

static void simple_timing_init(SimpleTiming *t)
{
   t->number = 0;
   t->max = t->mean = 0;
   t->min = INFINITY;
}

void simple_timing_add(SimpleTiming *t, double newtime)
{
   if (newtime < t->min) { t->min = newtime; }
   if (newtime > t->max) { t->max = newtime; }

   unsigned number = t->number;

   t->mean = (t->mean * number + newtime)/(number+1);
   t->number++;
}

UNUSED static const char* simple_timing_print(const SimpleTiming *t, int *ret)
{
   if (t->number == 0) { return NULL; }

   char *s;

   if (t->number > 1) {
      *ret = asprintf(&s, "total = %.f for %u calls. min/avg/max = %.3f/%.3f/%.3f",
                      t->number * t->mean, t->number, t->min, t->mean, t->max);
   } else {
      *ret = asprintf(&s, "%.3f", t->mean);
   }

   return s;
}

int mdl_timings_alloc(Model *mdl)
{
   Timings *t;
   MALLOC_(t, Timings, 1);
   mdl->timings = t;

   t->gams.gmo2rhp = 0.;
   t->gams.empinfo_file.interp = 0.;
   t->gams.empinfo_file.empdag_edges_built = 0.;

   simple_timing_init(&t->rhp.mdl_creation);
   simple_timing_init(&t->empdag.analysis);

   t->reformulation.container.dualvar = 0.;
   t->reformulation.container.flipped = 0.;
   t->reformulation.container.total = 0.;

   simple_timing_init(&t->reformulation.CCF.equilibrium);
   simple_timing_init(&t->reformulation.CCF.fenchel);
   simple_timing_init(&t->reformulation.CCF.conjugate.PPL);
   simple_timing_init(&t->reformulation.CCF.conjugate.stats);
   t->reformulation.CCF.total = 0.;

   t->reformulation.empdag.total = 0.;

   t->gmo_creation = 0.;

   t->solve.presolve_wall = 0.;
   t->solve.fooc = 0.;
   t->solve.solver_wall = 0.;

   t->postprocessing = 0.;

   t->wall_start = get_walltime();
   t->thrd_start = get_thrdtime();
   t->proc_start = get_proctime();

   t->refcnt = 1;

   return OK;
}

NONNULL static void printsimple(struct lineppty *l, const char *str, const SimpleTiming *t)
{
   if (t->number == 0) { return; }
   if (t->number == 1) { printdbl(l, str, t->mean); return; }

   int offset;
   printout(l->mode, "%*s%s [#/min/maxavg/max = %u/%.3f/%.3f/%.3f]%n", l->ident, "",
            str, t->number, t->min, t->mean, t->max, &offset);

   if (l->colw > offset + 6) {
      printout(l->mode, "%*.3f\n", l->colw - offset, t->number * t->mean);
   } else {
      printstr(l->mode, "\n");
   }

}

void mdl_timings_print(const Model *mdl, unsigned mode)
{
   Timings *t = mdl->timings;

   t->wall_end = get_walltime();
   t->thrd_end = get_thrdtime();
   t->proc_end = get_proctime();

   double total_wall = t->wall_end - t->wall_start;
   double total_thrd = t->thrd_end - t->thrd_start;
   double total_proc = t->proc_end - t->proc_start;

   printout(mode, "\nTimings (in sec) for solving %s model '%.*s' #%u\nTotal time: wall = %.3f; "
            "thread = %.3f; process = %.3f\n", mdl_fmtargs(mdl), total_wall,
            total_thrd, total_proc);

   struct lineppty l = {.mode = mode, .colw = 80, .ident = 0};
   unsigned offset = 2;

   if (t->gams.gmo2rhp > 0) {
      printinfo(&l, "GAMS input");
      l.ident += offset;
      printdbl(&l, "Import GMO model", t->gams.gmo2rhp);
      printdbl(&l, "Empinfo interpretation", t->gams.empinfo_file.interp);
      printdbl(&l, "EmpDag edges creation", t->gams.empinfo_file.empdag_edges_built);
      l.ident -= offset;
   }

   printsimple(&l, "EmpDag analysis", &t->empdag.analysis);

   if (t->reformulation.container.total > 0) {
      printdbl(&l, "Container reformulation", t->reformulation.container.total);
      l.ident += offset;
      printdbl(&l, "Flipped equations", t->reformulation.container.flipped);
      printdbl(&l, "dualVar", t->reformulation.container.dualvar);
      l.ident -= offset;
   }

   if (t->reformulation.CCF.total > 0) {
      printdbl(&l, "CCF reformulation", t->reformulation.CCF.total);
      l.ident += offset;
      printsimple(&l, "Equilibrium", &t->reformulation.CCF.equilibrium);
      printsimple(&l, "Fenchel", &t->reformulation.CCF.fenchel);
      printsimple(&l, "Conjugate", &t->reformulation.CCF.conjugate.stats);
      l.ident += offset;
      printsimple(&l, "PPL", &t->reformulation.CCF.conjugate.PPL);
      l.ident -= 2*offset;
   }
   
   printdbl(&l, "Presolve (wall)", t->solve.presolve_wall);
   printdbl(&l, "First-order OC", t->solve.fooc);
   printdbl(&l, "GMO creation (cumul.)", t->gmo_creation);
   printdbl(&l, "Solver time (wall)", t->solve.solver_wall);
   
   printdbl(&l, "Postprocessing", t->postprocessing);

   printstr(mode, "\n");
   
}

void mdl_timings_rel(Timings *t)
{
   if (!t) { return; }

   if (t->refcnt <= 1) {
      FREE(t);
      return;
   }

   t->refcnt--;
}

Timings* mdl_timings_borrow(Timings *t)
{
   if (t->refcnt == UINT_MAX) {
      errormsg("[timings] ERROR: reached refcnt max value\n");
      return NULL;
   }

   t->refcnt++;

   return t;
}
