#ifndef EMPINFO_H
#define EMPINFO_H

#include <stdbool.h>

#include "rhp_fwd.h"
#include "empdag.h"

/**
 *  @file empinfo.h
 *
 *  @brief the master EMP information data structure
 */

/** @struct empinfo empinfo.h*/
typedef struct empinfo {
   struct ovfinfo *ovf;         /**< OVF information                          */

   EmpDag empdag;               /**< DAG representation of the problem        */

   unsigned num_dualvar;        /**< number of dual variable                  */
   unsigned num_implicit;       /**< number of implicit variable              */
   unsigned num_deffn;          /**< number of mappings                       */
} EmpInfo;

void empinfo_dealloc(EmpInfo *empinfo);
int empinfo_alloc(EmpInfo *empinfo, Model *mdl);
int empinfo_initfromupstream(Model *mdl) NONNULL;

bool empinfo_is_hop(const EmpInfo *empinfo) NONNULL;
bool empinfo_is_vi(const EmpInfo *empinfo) NONNULL;
bool empinfo_is_opt(const EmpInfo *empinfo) NONNULL;

static inline bool empinfo_hasempdag(const EmpInfo *empinfo) {
   return ((empinfo->empdag.type != EmpDag_Unset) 
        && (empinfo->empdag.type != EmpDag_Empty));
}

static inline bool empinfo_has_ovf(const EmpInfo *empinfo)
{
   return empinfo && empinfo->ovf ? true : false;
}

#endif /* EMPINFO_H */
