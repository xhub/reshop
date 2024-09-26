#ifndef MATHPRGM_PRIV_H
#define MATHPRGM_PRIV_H

#include <assert.h>
#include <limits.h>

#include "equvar_metadata.h"
#include "mathprgm.h"
#include "mathprgm_data.h"
#include "mdl.h"

#ifndef NDEBUG
#define MP_DEBUG(s, ...) trace_empdag("[MP] DEBUG: " s, __VA_ARGS__)
#else
#define MP_DEBUG(s, ...)
#endif

int mp_setobjequ_internal(MathPrgm *mp, rhp_idx objequ) NONNULL;
int mp_setobjvar_internal(MathPrgm *mp, rhp_idx vidx) NONNULL;
MathPrgm *mp_new_fake(void) NONNULL;

static inline void _setvarrole(MathPrgm *mp, rhp_idx vi, VarRole type)
{
   assert(mp->mdl->ctr.varmeta);
   MP_DEBUG("Var '%s' has now type '%s'\n", ctr_printvarname(&mp->mdl->ctr, vi),
            varrole_name(type));
   (mp->mdl->ctr.varmeta)[vi].type = type;
}

static inline void _addvarbasictype(MathPrgm *mp, rhp_idx vi, VarPptyType ppty)
{
   assert(mp->mdl->ctr.varmeta);
   MP_DEBUG("Var '%s' has now property %u\n", ctr_printvarname(&mp->mdl->ctr, vi),
            ppty);
   (mp->mdl->ctr.varmeta)[vi].ppty |= ppty;
}

static inline void _setequrole(MathPrgm *mp, rhp_idx ei, EquRole role)
{
   MP_DEBUG("Equ '%s' has now type '%s'\n", ctr_printequname(&mp->mdl->ctr, ei),
            equrole_name(role));
   assert(mp->mdl->ctr.equmeta);
   (mp->mdl->ctr.equmeta)[ei].role = role;
}

static inline void _setequppty(MathPrgm *mp, rhp_idx ei, EquPpty ppty)
{
   MP_DEBUG("Equ '%s' has now property %u\n", ctr_printequname(&mp->mdl->ctr, ei),
            ppty);
   assert(mp->mdl->ctr.equmeta);
   (mp->mdl->ctr.equmeta)[ei].ppty = ppty;
}

static inline int _getequdual(MathPrgm *mp, rhp_idx ei)
{
   assert(mp->mdl->ctr.equmeta);
   return (mp->mdl->ctr.equmeta)[ei].dual;
}

static inline EquRole _getequrole(MathPrgm *mp, rhp_idx ei)
{
   assert(mp->mdl->ctr.equmeta);
   return (mp->mdl->ctr.equmeta)[ei].role;
}

static inline int _getvardual(MathPrgm *mp, rhp_idx vi)
{
   assert(mp->mdl->ctr.varmeta);
   return (mp->mdl->ctr.varmeta)[vi].dual;
}

static inline VarRole _getvarrole(MathPrgm *mp, rhp_idx vi)
{
   assert(mp->mdl->ctr.varmeta);
   return (mp->mdl->ctr.varmeta)[vi].type;
}

static inline VarPptyType _getvarsubtype(MathPrgm *mp, rhp_idx vi)
{
   assert(mp->mdl->ctr.varmeta);
   return (mp->mdl->ctr.varmeta)[vi].ppty;
}

static inline bool mp_isobj(const MathPrgm *mp)
{
   assert(mp->sense < RhpNoSense);
   return mp->type == MpTypeOpt;
}

static inline void mpopt_init(struct mp_opt *opt)
{
   opt->objvar = IdxNA;
   opt->objequ = IdxNA;
   opt->objcoef = 1.;
   opt->objvarval2objequval = false;
}

static inline void mpvi_init(struct mp_vi *vi)
{
   vi->num_cons = 0;
   vi->num_zeros = 0;
   vi->num_matches = 0;
}
#endif
