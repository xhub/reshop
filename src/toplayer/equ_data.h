#ifndef EQU_DATA_H
#define EQU_DATA_H

/** 
 *  @file equ_data.h
 *
 *  @brief equation related data
 */

#include <stdint.h>
#include "compat.h"

/** @brief Type of equation */
__extension__ typedef enum equ_object_type ENUM_U8 {
   EquTypeUnset = 0,          /**< Equation type unset */
   Mapping,            /**< Mapping (objective fn, functional part of VI, ...)*/
   ConeInclusion,     /**< Inclusion in a cone (usual constraint) */
   BooleanRelation,   /**< Boolean relation */
   EquTypeUnsupported,        /**< Unsupported type */ 
} EquObjectType;

/** @brief Linear equation */
typedef struct lequ {
   unsigned max;    /**< Size of the allocated arrays */
   unsigned len;    /**< Currentnumber of linear terms */

   rhp_idx *vis;    /**< Variable indices */
   double *coeffs;  /**< Coefficients     */
} Lequ;

const char *equtype_name(unsigned type);

#endif /* EQU_DATA_H  */
