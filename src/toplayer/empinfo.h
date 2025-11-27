#ifndef EMPINFO_H
#define EMPINFO_H

#include <stdbool.h>

#include "rhp_fwd.h"
#include "empdag.h"
#include "equvar_emp.h"

/**
 *  @file empinfo.h
 *
 *  @brief the master EMP information data structure
 */


/** @struct empinfo empinfo.h*/
typedef struct empinfo {
   struct ovfinfo *ovf;     /**< OVF information                          */

   EmpDag empdag;           /**< DAG representation of the problem        */

   EquVarEmp equvar;        /**< EMP related to equations and variables   */

} EmpInfo;

void empinfo_dealloc(EmpInfo *empinfo);
int empinfo_init(Model *mdl);
int empinfo_initfromupstream(Model *mdl) NONNULL;

bool empinfo_is_hop(const EmpInfo *empinfo) NONNULL;
bool empinfo_is_vi(const EmpInfo *empinfo) NONNULL;
bool empinfo_is_opt(const EmpInfo *empinfo) NONNULL;

NONNULL static inline bool empinfo_hasempdag(const EmpInfo *empinfo) {
   return ((empinfo->empdag.type != EmpDag_Unset) 
        && (empinfo->empdag.type != EmpDag_Empty));
}

NONNULL static inline bool empinfo_has_ovf(const EmpInfo *empinfo) {
   return empinfo->ovf ? true : false;
}

NONNULL static inline bool empinfo_has_marginal_vars(const EmpInfo *empinfo) {
   return empinfo->equvar.marginalVars.len > 0;
}

#endif /* EMPINFO_H */
