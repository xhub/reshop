#ifndef CONES_H
#define CONES_H

#include <assert.h>
#include <stdbool.h>

/*
 *  @file cones.h
 *
 *  @brief list of cones
 */

/** @brief Type of cones  */
typedef enum cone {
   CONE_NONE = 0,    /**< Unset/non-existent */
   CONE_R_PLUS,      /**< Non-negative real \f$\mathbb{R}_+\f$ */
   CONE_R_MINUS,     /**< Non-positive real \f$\mathbb{R}_-\f$  */
   CONE_R,           /**< Real \f$\mathbb{R}\f$ */
   CONE_0,           /**< Zero singleton */
   CONE_POLYHEDRAL,  /**< Polyhedral cone */
   CONE_SOC,         /**< Second Order cone */
   CONE_RSOC,        /**< Rotated Second Order cone */
   CONE_EXP,         /**< Exponential cone */
   CONE_DEXP,        /**< Dual Exponential cone */
   CONE_POWER,       /**< Power cone */
   CONE_DPOWER,      /**< Dual Power cone */
   __CONES_LEN
} Cone;

/** Cone descriptions */
const char *cone_name(unsigned cone);

int cone_dual(enum cone cone, void *cone_data,
               enum cone *dual_cone, void **dual_cone_data);
int cone_polar(enum cone cone, void *cone_data, enum cone *polar_cone,
               void **polar_cone_data);

/**
 *  @brief Evaluate whether a cone is polyhedral
 *
 *  @param cone  the cone
 *
 *  @return      true if the cone is polyhedral, false otherwise
 */
static inline bool cone_polyhedral(enum cone cone) {
   switch(cone) {
   case CONE_R_PLUS:
   case CONE_R_MINUS:
   case CONE_R:
   case CONE_0:
   case CONE_POLYHEDRAL:
     return true;
   case CONE_SOC:
   case CONE_EXP:
   case CONE_DEXP:
   case CONE_POWER:
   case CONE_DPOWER:
     return false;
   default:
      assert(!"Unsupported case caught!");
     return false;
   }
}

/**
 *  @brief Evaluate whether a cone is 1D and polyhedral
 *
 *  @param cone  the cone
 *
 *  @return      true if the cone is polyhedral, false otherwise
 */
static inline bool cone_1D_polyhedral(enum cone cone) {
   switch(cone) {
   case CONE_R_PLUS:
   case CONE_R_MINUS:
   case CONE_R:
   case CONE_0:
     return true;
   case CONE_POLYHEDRAL:
   case CONE_SOC:
   case CONE_EXP:
   case CONE_DEXP:
   case CONE_POWER:
   case CONE_DPOWER:
     return false;
   default:
      assert(!"Unsupported case caught!");
     return false;
   }
}

#endif /* CONES_H */
