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
ProbType probtype_from_gams(int type)
{
  switch(type) {
   case gmoProc_none:
      return MdlProbType_none;
   case gmoProc_lp:
      return MdlProbType_lp;
   case gmoProc_mip:
      return MdlProbType_mip;
   case gmoProc_rmip:
      return MdlProbType_none;
   case gmoProc_nlp:
      return MdlProbType_nlp;
   case gmoProc_mcp:
      return MdlProbType_mcp;
   case gmoProc_mpec:
      return MdlProbType_mpec;
   case gmoProc_rmpec:
      return MdlProbType_none;
   case gmoProc_cns:
      return MdlProbType_cns;
   case gmoProc_dnlp:
      return MdlProbType_dnlp;
   case gmoProc_rminlp:
      return MdlProbType_none;
   case gmoProc_minlp:
      return MdlProbType_minlp;
   case gmoProc_qcp:
      return MdlProbType_qcp;
   case gmoProc_miqcp:
      return MdlProbType_miqcp;
   case gmoProc_rmiqcp:
      return MdlProbType_none;
   case gmoProc_emp:
      return MdlProbType_emp;
   case gmoProc_nrofmodeltypes:
   default:
      return MdlProbType_none;
   }
}

/**
 * @brief Translate the ReSHOP model type to GAMS
 *
 * @param type  the ReSHOP model type
 *
 * @return      the GAMS model type
 */
int probtype_to_gams(ProbType type)
{
   switch(type) {
   case MdlProbType_none:
     return gmoProc_none;
   case MdlProbType_lp:
     return gmoProc_lp;
   case MdlProbType_nlp:
     return gmoProc_nlp;
   case MdlProbType_dnlp:
     return gmoProc_dnlp;
   case MdlProbType_mip:
     return gmoProc_mip;
   case MdlProbType_minlp:
     return gmoProc_minlp;
   case MdlProbType_miqcp:
     return gmoProc_miqcp;
   case MdlProbType_qcp:
     return gmoProc_qcp;
   case MdlProbType_mcp:
     return gmoProc_mcp;
   case MdlProbType_mpec:
     return gmoProc_mpec;
   case MdlProbType_emp:
     return gmoProc_emp;
   case MdlProbType_cns:
     return gmoProc_cns;
   default:
     return gmoProc_none;
   }
}




