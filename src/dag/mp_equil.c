#include "empdag.h"
#include "empinfo.h"
#include "macros.h"
#include "mathprgm.h"
#include "mp_equil.h"
#include "printout.h"
#include "reshop.h"
#include "status.h"

/** @file mp_equil.c */

Mpe *mpe_new(unsigned id, Model *mdl)
{
  assert(mdl);

  Mpe *mpe;
  MALLOC_NULL(mpe, Mpe, 1);

  mpe->id = id;
  mpe->mdl = mdl;

  return mpe;
}

void mpe_free(Mpe *mpe)
{
  if (!mpe) return;

  FREE(mpe);

}

unsigned mpe_getid(const Mpe *mpe) { return mpe->id; }


Mpe *mpe_dup(const Mpe *mpe_src, Model *mdl)
{
   return mpe_new(mpe_src->id, mdl);
}

int mpe_ve_full(Mpe* mpe)
{
   assert(mpe);

   mpe->ve.compute_ve = true;
   mpe->ve.full_ve = true;

   return OK;
}

int mpe_ve_partial(Mpe* mpe, Aequ *aeqn)
{
   assert(mpe);

   mpe->ve.compute_ve = true;
   mpe->ve.full_ve = false;
  // FIXME: copy? clarify ownership!
   mpe->ve.common_mult = aeqn;

   return OK;
}

