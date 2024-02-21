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
typedef enum mdl_probtype {
   MdlProbType_none  = 0,  /**< Not set */
   MdlProbType_lp,         /**< LP    Linear Programm   */
   MdlProbType_nlp,        /**< NLP   NonLinear Programm     */
   MdlProbType_dnlp,       /**< DNLP  Nondifferentiable NLP  */
   MdlProbType_mip,        /**< MIP   Mixed-Integer Programm */
   MdlProbType_minlp,      /**< MINLP Mixed-Integer NLP*/
   MdlProbType_miqcp,      /**< MIQCP Mixed-Integer QCP*/
   MdlProbType_qcp,        /**< QCP   Quadratically Constraint Programm */
   MdlProbType_mcp,        /**< MCP   Mixed Complementarity Programm*/
   MdlProbType_mpec,       /**< MPEC  Math. Prog. with Equilibrium Constraints*/
   MdlProbType_vi,         /**< VI    Variational Inequality */
   MdlProbType_emp,        /**< EMP   Extended Mathematical Programm */
   MdlProbType_cns,        /**< CNS   Constrained Nonlinear System */
   MdlProbType_last = MdlProbType_cns,
} ProbType;


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
   ProbType probtype;           /**< type of the MP */
   RhpSense sense;              /**< sense of the MP */
   rhp_idx objvar;              /**< Objective variable (when applicable) */
   rhp_idx objequ;              /**< Objective equation (when applicable) */
};

extern const unsigned probtypeslen;

const char* backend_name(unsigned backendtype);
unsigned backend_idx(const char *backendname);
const char* probtype_name(ProbType type);
bool probtype_hasmetadata(ProbType type);
bool probtype_isopt(ProbType type);
bool probtype_isvi(ProbType type);

static inline bool valid_probtype(ProbType type) {
   return type > MdlProbType_none && type <= MdlProbType_cns;
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
   d->probtype = MdlProbType_none;
   d->sense = RhpNoSense;
   d->objvar = IdxInvalid;
   d->objequ = IdxInvalid;
}

#endif
