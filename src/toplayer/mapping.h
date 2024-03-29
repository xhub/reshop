#ifndef RHP_MAPPING_H
#define RHP_MAPPING_H

#include <assert.h>
#include <math.h>
#include <stdbool.h>

#include "compat.h"
#include "rhp_fwd.h"
#include "rhpidx.h"

/** A mapping object, defined over an equation */
typedef struct {
   rhp_idx ei;               /**< The equation index                           */
   rhp_idx vi_map;           /**< If valid, the output variable of the mapping */
   double coeff;             /**< The coefficient to multiply the equation data */
} MappingObj;

/** Types of mapping */
typedef enum {
   MappingInvalid,           /**< Invalid mapping                             */
   MappingDirect,            /**< Equation is already a mapping               */
   MappingDefined,           /**< Explicitly defined mapping via the variable */
   MappingImplicit,          /**< Implicitly defined mapping via the variable */
} MappingObjType;

bool valid_map(Container *ctr, MappingObj *m);

static inline MappingObj map_direct(rhp_idx ei) 
{
   return (MappingObj){.ei = ei, .vi_map = IdxNA, .coeff = 1.};
}

static inline MappingObj map_defined(rhp_idx ei, rhp_idx vi, double coeff) 
{
   assert(valid_vi(vi) && valid_ei(ei) && isfinite(coeff));
   return (MappingObj){.ei = ei, .vi_map = vi, .coeff = coeff};
}

static inline MappingObj map_implicit(rhp_idx vi) 
{
   assert(valid_vi(vi));
   return (MappingObj){.ei = IdxNA, .vi_map = vi, .coeff = NAN};
}

static inline MappingObjType map_type(MappingObj *m)
{
   if (!valid_vi(m->vi_map)) {
      if (valid_ei(m->ei) && isfinite(m->coeff)) {
         return MappingDirect;
      }
      return MappingInvalid;
   }

   if (!valid_ei(m->ei)) {
      return MappingImplicit;
   }

   if (!isfinite(m->coeff)) { return MappingInvalid; }

   return MappingDefined;
}

#endif
