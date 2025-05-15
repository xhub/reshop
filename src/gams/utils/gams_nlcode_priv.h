#ifndef GAMS_NLCODE_PRIV_H
#define GAMS_NLCODE_PRIV_H

#include "rhp_compiler_defines.h"
#include "rhp_defines.h"

/** Algebraic degree special values */
__extension__ typedef enum ENUM_U8 {
   DegFullyNl = UINT8_MAX,    /**<  This is a fully nonlinear                 */
   DegDiv     = 1 << 6,       /**< Indicates a division                       */
   DefMaxPoly = DegDiv-1,     /**<                                            */
} DegTypes;

#define deg_mkdiv(numer, denom)  (DegDiv | ((numer) << 3) | (denom))

u32 compute_nlcode_degree(int len, const int instr[VMT(restrict len)],
                          const int args[VMT(restrict len)]);

#endif
