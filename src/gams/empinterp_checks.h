#ifndef EMPINTERP_CHECKS_H
#define EMPINTERP_CHECKS_H

#include "empinterp.h"

int bilevel_sanity_check(Interpreter *interp) NONNULL;
int empdag_sanity_check(Interpreter *interp) NONNULL;
int dualequ_sanity_check(Interpreter *interp) NONNULL;
int equilibrium_sanity_check(Interpreter *interp) NONNULL;
int mp_sanity_check(Interpreter *interp) NONNULL;
int old_style_check(Interpreter *interp) NONNULL;


#endif /* EMPINTERP_CHECKS_H */ 
