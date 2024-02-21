#ifndef CONTAINER_HELPERS_H
#define CONTAINER_HELPERS_H

#include "container.h"
#include "equ.h"


static inline enum equ_object_type ctr_equ_type(Container *ctr, size_t idx)
{ return ctr->equs[idx].object; }

static inline enum cone ctr_equ_cone(Container *ctr, size_t idx)
{ return ctr->equs[idx].cone; }

static inline bool ctr_neednames(const Container *ctr) {
  return ctr->status & CtrNeedEquVarNames;
}

static inline void ctr_setneednames(Container *ctr) {
  ctr->status |= CtrNeedEquVarNames;
}

static inline void ctr_unsetneednames(Container *ctr) {
  ctr->status &= ~CtrNeedEquVarNames;
}

#endif
