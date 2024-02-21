#ifndef EQUVAR_DATA_H
#define EQUVAR_DATA_H

/** 
 *  @file equvar_data.h
 *
 *  @brief data common to equations and variables
 */

#include <limits.h>
#include <stdbool.h>

#include "compat.h"
#include "rhpidx.h"

/** type of abstract variable or equation */
typedef enum a_equvar_type {
   EquVar_Compact     = 0,  /**< Compact: continuous indices */
   EquVar_List        = 1,  /**< List: given as a list of indices */
   EquVar_SortedList  = 2,  /**< List: given as a sorted list of indices */
   EquVar_Block       = 3,  /**< Block: collection of abstract variable/equation */
   EquVar_Unset       = 4,   /**< Unset */
} AbstractEquVarType;

typedef enum {
   BasisUnset      = 0,
   BasisLower      = 1,
   BasisUpper      = 2,
   BasisBasic      = 3,
   BasisSuperBasic = 4,
   BasisFixed      = 5,
} BasisStatus;

const char* aequvar_typestr(AbstractEquVarType type);
const char* basis_name(BasisStatus basis);

#endif /* EQUVAR_DATA_H */
