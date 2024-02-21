
#ifndef EMPINTERP_EDGE_RESOLVER_H
#define EMPINTERP_EDGE_RESOLVER_H

#include "compat.h"

struct interpreter;

int empinterp_set_empdag_root(struct interpreter *interp) NONNULL;
int empinterp_resolve_empdag_edges(struct interpreter *interp) NONNULL;


#endif
