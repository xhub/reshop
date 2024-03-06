#ifndef GAMS_ROSETTA_H
#define GAMS_ROSETTA_H

#include "reshop_config.h"

#include "equ_data.h"
#include "equvar_data.h"
#include "macros.h"
#include "printout.h"
#include "asnan.h"

#include "cones.h"
#include "mdl_data.h"
#include "option.h"

#include "gmomcc.h"

struct rmdl_gams_opt {
   const char *name;
   OptType type;
   const char *gams_opt_name;
};

extern const struct rmdl_gams_opt rmdl_opt_to_gams[];
extern const size_t rmdl_opt_to_gams_len;

int cone_to_gams_relation_type(Cone cone, int *type);
enum mdl_type mdltype_from_gams(int type);
int mdltype_to_gams(enum mdl_type type);

typedef struct {
   EquObjectType object;     /**< type of equation                            */
   Cone cone;                /**< cone for cone inclusion                     */
} EquTypeFromGams;

static inline double dbl_from_gams(double x, double pinf, double minf, double na)
{
   if (RHP_UNLIKELY(x == pinf))   { return  INFINITY;}
   if (RHP_UNLIKELY(x == minf))   { return -INFINITY;}

   /* TODO(xhub) fix this mess: SNAN gives VALNA, NAN means no value  */
   if (RHP_UNLIKELY(x == na))     { return SNAN_NA; }

   return x; 
}

static inline double dbl_to_gams(double x, double pinf, double minf, double na)
{
   if (isfinite(x)) { return x; }

   if (x == INFINITY) { return pinf; /* GAMS_VALPIN (1e299)*/; }
   if (x == -INFINITY) { return minf; /*GAMS_VALMIN (-1e299) */; }
   /* TODO(xhub) fix this mess: SNAN gives VALNA, NAN means no value  */

   if (x == SNAN_NA || -x == SNAN_NA) { return na; }
//      if (x == SNAN_UNINIT) { return 0.; }
//      if (x == SNAN_UNDEF) { return GMS_SV_UNDEF; }

   return 0.;
}

static inline int basis_to_gams(BasisStatus basis)
{
  switch (basis) {
  case BasisLower:
    return gmoBstat_Lower;
  case BasisUpper:
    return gmoBstat_Upper;
  case BasisBasic:
    return gmoBstat_Basic;
  case BasisSuperBasic:
    return gmoBstat_Super;
  case BasisUnset:
    error("%s :: unset basis status detected!\n", __func__);
    break;
  case BasisFixed:
    error("%s :: fixed basis status detected!\n", __func__);
    break;
  }
  return -1;
}

static inline BasisStatus basis_from_gams(enum gmoVarEquBasisStatus basis)
{
  switch (basis) {
  case gmoBstat_Lower:
    return BasisLower;
  case gmoBstat_Upper:
    return BasisUpper;
  case gmoBstat_Basic:
    return BasisBasic;
  case gmoBstat_Super:
    return BasisSuperBasic;
  default:
    return BasisUnset;
  }
}

static inline int equtype_from_gams(int gams_type, EquTypeFromGams *equtype)
{
   switch (gams_type) {
   case gmoequ_L:
      equtype->cone = CONE_R_MINUS;
      equtype->object = ConeInclusion;
      return OK;
   case gmoequ_E:
      equtype->cone = CONE_0;
      equtype->object = ConeInclusion;
      return OK;
   case gmoequ_G:
      equtype->cone = CONE_R_PLUS;
      equtype->object = ConeInclusion;
      return OK;
   case gmoequ_C:
      equtype->object = ConeInclusion;
      TO_IMPLEMENT("Second Order cone relation are not yet supported");
   case gmoequ_N:
      equtype->object = Mapping;
      equtype->cone = CONE_NONE;
      return OK;
   case gmoequ_B:
      equtype->object = BooleanRelation;
      equtype->cone = CONE_NONE;
      return OK;
   case gmoequ_X:
      TO_IMPLEMENT("external equations are not yet supported");
   default:
      error("[GAMS] ERROR: unsupported value %d for an equation type\n",
            gams_type);
      return Error_RuntimeError;
   }


}

#endif /* GAMS_ROSETTA_H */
