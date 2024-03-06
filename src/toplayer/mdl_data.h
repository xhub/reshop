#ifndef MDL_DATA_H_42
#define MDL_DATA_H_42

#include <stdbool.h>

#include "compat.h"
#include "rhpidx.h"

/** @file mdl_data.h
 *
 *  @brief Model related data
 */

/** @brief type of (classical) optimization problem */
__extension__ typedef enum mdl_type ENUM_U8 {
   MdlType_none  = 0,      /**< Not set */
   MdlType_lp,             /**< LP    Linear Programm   */
   MdlType_nlp,            /**< NLP   NonLinear Programm     */
   MdlType_dnlp,           /**< DNLP  Nondifferentiable NLP  */
   MdlType_mip,            /**< MIP   Mixed-Integer Programm */
   MdlType_minlp,          /**< MINLP Mixed-Integer NLP*/
   MdlType_miqcp,          /**< MIQCP Mixed-Integer QCP*/
   MdlType_qcp,            /**< QCP   Quadratically Constraint Programm */
   MdlType_mcp,            /**< MCP   Mixed Complementarity Programm*/
   MdlType_mpec,           /**< MPEC  Math. Prog. with Equilibrium Constraints*/
   MdlType_vi,             /**< VI    Variational Inequality */
   MdlType_emp,            /**< EMP   Extended Mathematical Programm */
   MdlType_cns,            /**< CNS   Constrained Nonlinear System */
   MdlType_last = MdlType_cns,
} ModelType;


typedef enum {
   RhpMin = 0,
   RhpMax = 1,
   RhpFeasibility = 2,
   RhpNoSense = 3,
   RhpSenseLast = RhpNoSense,
} RhpSense;

/** Type of filter ops */
typedef enum {
   FopsEmpty,
   FopsActive,
   FopsSubset,
} FopsType;

/** @brief mathematical programm description */
struct mp_descr {
   ModelType mdltype;           /**< type of the MP */
   RhpSense sense;              /**< sense of the MP */
   rhp_idx objvar;              /**< Objective variable (when applicable) */
   rhp_idx objequ;              /**< Objective equation (when applicable) */
};

extern const unsigned mdltypeslen;

const char* backend_name(unsigned backendtype);
unsigned backend_idx(const char *backendname);

const char* mdltype_name(ModelType type);
bool mdltype_hasmetadata(ModelType type);
bool mdltype_isopt(ModelType type);
bool mdltype_isvi(ModelType type);

static inline bool valid_mdltype(ModelType type) {
   return type > MdlType_none && type <= MdlType_last;
}

static inline bool valid_sense(unsigned sense)
{
   /* WARNING this assumes that the lower value of RhpSense is 0 */
   return sense <= RhpFeasibility;
}

static inline bool valid_optsense(RhpSense sense)
{
   switch (sense) {
   case RhpMin:
   case RhpMax:
      return true;
   default:
      return false;
   }
}

static inline void mp_descr_invalid(struct mp_descr *d)
{
   d->mdltype = MdlType_none;
   d->sense = RhpNoSense;
   d->objvar = IdxInvalid;
   d->objequ = IdxInvalid;
}

#endif
