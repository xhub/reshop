#ifndef EQU_DATA_H
#define EQU_DATA_H

/** 
 *  @file equ_data.h
 *
 *  @brief equation related data
 */

#include "compat.h"

/** @brief Type of equation */
typedef enum equ_object_type {
   EQ_UNSET = 0,          /**< Equation type unset */
   EQ_MAPPING,            /**< Mapping (objective fn, functional part of VI, ...)*/
   EQ_CONE_INCLUSION,     /**< Inclusion in a cone (usual constraint) */
   EQ_BOOLEAN_RELATION,   /**< Boolean relation */
   EQ_UNSUPPORTED,        /**< Unsupported type */ 
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
