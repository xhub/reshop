#include <stddef.h>

#include "cones.h"
#include "status.h"
#include "printout.h"

/**
 * @brief Cone description
 */
const char * const _cone_descr[CONE_LEN] = {
   "None",
   "R_+",
   "R_-",
   "R",
   "{0}",
   "Polyhedral",
   "SOC",
   "Rotated SOC",
   "Exponential",
   "Dual Exponential",
   "Power",
   "Dual Power",
};

/**
 * @brief Description of cones
 *
 * @param cone  the cone type
 *
 * @return      a string representing the cone type
 */
const char *cone_name(unsigned cone)
{
   if (cone < CONE_LEN) {
      return _cone_descr[cone];
   }

   return "unknown";
}

/**
 * @brief Compute the dual of a cone
 *
 * @param      cone            cone
 * @param      cone_data       optional data for the cone
 * @param[out] dual_cone       dual cone type
 * @param[out] dual_cone_data  optional data for the dual cone
 *
 * @return                the error code
 */
int cone_dual(enum cone cone, void *cone_data,
               enum cone *dual_cone, void **dual_cone_data)
{
   switch (cone) {
   case CONE_R_PLUS:
     *dual_cone = CONE_R_PLUS;
     (*dual_cone_data) = NULL;
     break;
  case CONE_R_MINUS:
     *dual_cone = CONE_R_MINUS;
     (*dual_cone_data) = NULL;
     break;
  case CONE_R:
     *dual_cone = CONE_0;
     (*dual_cone_data) = NULL;
     break;
  case CONE_0:
     *dual_cone = CONE_R;
     (*dual_cone_data) = NULL;
     break;
  case CONE_SOC:
  case CONE_EXP:
  case CONE_POWER:
  default:
     error("%s :: unsupported cone %s\n", __func__, cone_name(cone));
     return Error_NotImplemented;
   }

   return OK;
}

/**
 * @brief Compute the polar of a cone
 *
 * @param      cone             cone
 * @param      cone_data        optional data for the cone
 * @param[out] polar_cone       polar cone type
 * @param[out] polar_cone_data  optional data for the polar cone
 *
 * @return                      the error code
 */
int cone_polar(enum cone cone, void *cone_data,
               enum cone *polar_cone, void **polar_cone_data)
{
   switch (cone) {
   case CONE_R_PLUS:
     *polar_cone = CONE_R_MINUS;
     (*polar_cone_data) = NULL;
     break;
  case CONE_R_MINUS:
     *polar_cone = CONE_R_PLUS;
     (*polar_cone_data) = NULL;
     break;
  case CONE_R:
     *polar_cone = CONE_0;
     (*polar_cone_data) = NULL;
     break;
  case CONE_0:
     *polar_cone = CONE_R;
     (*polar_cone_data) = NULL;
     break;
  case CONE_SOC:
  case CONE_EXP:
  case CONE_POWER:
  default:
     error("%s :: unsupported cone %s (%d)", __func__, cone_name(cone), cone);
     return Error_NotImplemented;
   }

   return OK;
}


