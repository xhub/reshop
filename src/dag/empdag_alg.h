#ifndef EMPDAG_ALG_H
#define EMPDAG_ALG_H

#include "compat.h"
#include "rhp_fwd.h"

//int empdag_determine_roots(EmpDag *empdag);
//int empdag_ppty_dfs(EmpDag *empdag);

int empdag_analysis(EmpDag *empdag) NONNULL;

#endif /* EMPDAG_ALG_H */
