#include "reshop_config.h"

#include "cones.h"
#include "gams_rosetta.h"
#include "printout.h"
#include "status.h"

#include "gevmcc.h"
#include "gmomcc.h"
#include "gclgms.h"

const struct rmdl_gams_opt rmdl_opt_to_gams[] = {
   {.name = "keep_files", OptBoolean, gevKeep},
   {.name = "atol", OptDouble, gevOptCA},
   {.name = "rtol", OptDouble, gevOptCR},
};

const size_t rmdl_opt_to_gams_len = sizeof(rmdl_opt_to_gams)/sizeof(struct rmdl_gams_opt);

/** 
 *  @brief Set the relation in an equation, based on the cone.
 *
 *  From a relation \f$ <a,x> - b \in K \f$, we can deduce \f$ <a,x> (<=,=,>=) b \f$
 *
 *  @param      cone   the cone K
 *  @param[out] type   the type of relation
 *
 *  @return            the error code
 */
int cone_to_gams_relation_type(enum cone cone, int *type)
{
   switch (cone) {
   case CONE_R_PLUS:
      *type = gmoequ_G;
      break;
   case CONE_R_MINUS:
      *type = gmoequ_L;
      break;
   case CONE_0:
      *type = gmoequ_E;
      break;
   case CONE_R:
      error("%s :: invalid call: the relation is free\n", __func__);
      return Error_InvalidValue;
  case CONE_NONE:
      *type = gmoequ_N;
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

/**
 * @brief Translate the GAMS model type to the ReSHOP one
 *
 * @param type  the GAMS model type
 *
 * @return      the error code
 */
ModelType mdltype_from_gams(int type)
{
  switch(type) {
   case gmoProc_none:
      return MdlType_none;
   case gmoProc_lp:
      return MdlType_lp;
   case gmoProc_mip:
      return MdlType_mip;
   case gmoProc_rmip:
      return MdlType_none;
   case gmoProc_nlp:
      return MdlType_nlp;
   case gmoProc_mcp:
      return MdlType_mcp;
   case gmoProc_mpec:
      return MdlType_mpec;
   case gmoProc_rmpec:
      return MdlType_none;
   case gmoProc_cns:
      return MdlType_cns;
   case gmoProc_dnlp:
      return MdlType_dnlp;
   case gmoProc_rminlp:
      return MdlType_none;
   case gmoProc_minlp:
      return MdlType_minlp;
   case gmoProc_qcp:
      return MdlType_qcp;
   case gmoProc_miqcp:
      return MdlType_miqcp;
   case gmoProc_rmiqcp:
      return MdlType_none;
   case gmoProc_emp:
      return MdlType_emp;
   case gmoProc_nrofmodeltypes:
   default:
      return MdlType_none;
   }
}

/**
 * @brief Translate the ReSHOP model type to GAMS
 *
 * @param type  the ReSHOP model type
 *
 * @return      the GAMS model type
 */
int mdltype_to_gams(ModelType type)
{
   switch(type) {
   case MdlType_none:
     return gmoProc_none;
   case MdlType_lp:
     return gmoProc_lp;
   case MdlType_nlp:
     return gmoProc_nlp;
   case MdlType_dnlp:
     return gmoProc_dnlp;
   case MdlType_mip:
     return gmoProc_mip;
   case MdlType_minlp:
     return gmoProc_minlp;
   case MdlType_miqcp:
     return gmoProc_miqcp;
   case MdlType_qcp:
     return gmoProc_qcp;
   case MdlType_mcp:
     return gmoProc_mcp;
   case MdlType_mpec:
     return gmoProc_mpec;
   case MdlType_emp:
     return gmoProc_emp;
   case MdlType_cns:
     return gmoProc_cns;
   default:
     return gmoProc_none;
   }
}




