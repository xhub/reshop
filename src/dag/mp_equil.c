#include "empdag.h"
#include "macros.h"
#include "mp_equil.h"
#include "printout.h"
#include "status.h"

/** @file mp_equil.c */

Nash *nash_new(unsigned id, Model *mdl)
{
  assert(mdl);

  Nash *nash;
  MALLOC_NULL(nash, Nash, 1);

  nash->id = id;
  nash->mdl = mdl;

  return nash;
}

void nash_free(Nash *nash)
{
  if (!nash) return;

  FREE(nash);

}

unsigned nash_getid(const Nash *nash) { return nash->id; }


Nash *nash_dup(const Nash *nash_src, Model *mdl)
{
   return nash_new(nash_src->id, mdl);
}

int nash_ve_full(Nash* nash)
{
   nash->ve.compute_ve = true;
   nash->ve.full_ve = true;

   return OK;
}

int nash_ve_partial(Nash* nash, Aequ *aeqn)
{
   nash->ve.compute_ve = true;
   nash->ve.full_ve = false;
  // FIXME: copy? clarify ownership!
   nash->ve.common_mult = aeqn;

   return OK;
}

