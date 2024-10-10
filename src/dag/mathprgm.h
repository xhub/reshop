#ifndef MATHPRGM_H
#define MATHPRGM_H

#include <assert.h>

#include "macros.h"
#include "mdl_data.h"
#include "empdag_data.h"
#include "mathprgm_data.h"
#include "printout.h"
#include "reshop.h"
#include "reshop_data.h"
#include "rhp_fwd.h"

/** @file mathprgm.h
 *
 *  @brief Representation of a mathematical programm
 */

struct rhp_empdag_arcVF;


struct mp_mps {
  unsigned size;
  unsigned max;
  MathPrgm **list;
};

struct mpes {
  unsigned size;
  unsigned max;
  Nash **list;
};

/**
 *  @struct mp_opt
 *
 *  Description of a optimization problem
 */
struct mp_opt {
  //   MpSense sense;     /**< sense of this problem */
  rhp_idx objvar; /**< index of the objective variable             */
  rhp_idx objequ; /**< index of the objective equation             */
  double objcoef; /**< coefficient of the objective variable       */
  bool objvarval2objequval; /**< If true, set the objequ value from the objvar */
};

/**
 *  @struct mp_vi
 *
 *  Description of a variational inequality \f$0 \in F(x) + N_X(x) \f$
 */
struct mp_vi {
   unsigned num_cons;    /**< number of constraints in the VI             */
   unsigned num_zeros;   /**< number of VI zero func (\f$ F_i(x) \equiv 0 \f$) */
   unsigned num_matches; /**< number of matches*/
   bool has_kkt;         /**< True of the MP contains the KKT of another MP    */
};

struct mp_ccflib {
   OvfDef *ccf;
   MathPrgm *mp_instance;
};

struct mp_dual {
   mpid_t mpid;
   DualOperatorData dualdat;
};

typedef enum {
   MpFinalized                    = 0x1,  /**< MP has been finalized                    */
   MpIsHidden                     = 0x2,  /**< Hidden MP                                */
   MpIsHidableAsDual              = 0x4,  /**< Hiddable MP: is used as dual             */
   MpIsHidableAsKkt               = 0x8,  /**< Hiddable MP: is used as kkt              */
   MpIsHidableAsInstance          = 0x10,  /**< Hiddable MP: is used as kkt              */
   MpCcflibNeedsFullInstantiation = 0x20,
} MpStatus;

#define MpIsHidable  (MpIsHidableAsDual | MpIsHidableAsKkt | MpIsHidableAsInstance)

/** @struct mathprgm
 *
 *  Mathematical Programm definition
 */
typedef struct rhp_mathprgm {
  mpid_t id;                 /**< mathprgm ID                          */
  RhpSense sense;              /**< mathprgm sense                       */
  MpType type;                 /**< mathprgm type                        */
//  enum EmpDAGType type_subdag; /**< type of the subdag of this node      */
   ModelType probtype;           /**< problem type                         */
   MpStatus status;

  Model *mdl;                  /**< model for this MP                    */

  union {
    struct mp_opt opt;
    struct mp_vi vi;
    struct mp_ccflib ccflib;
    struct mp_dual dual;
  };

  IdxArray equs;               /**< equations owned by the programm      */
  IdxArray vars;               /**< variables owned by the programm      */
} MathPrgm;

/* -------------------------------------------------------------------------
 * Public API functions
 * ------------------------------------------------------------------------- */
int mp_addconstraint(MathPrgm *mp, rhp_idx ei);
int mp_addconstraints(MathPrgm *mp, Aequ *e);
int mp_addequ(MathPrgm *mp, rhp_idx ei);
NONNULL OWNERSHIP_TAKES(3)
int mp_addobjexprbylabel(MathPrgm *mp, const struct rhp_empdag_arcVF *expr,
                         const char *label);
int mp_addobjexprbyid(MathPrgm *mp,
                      const struct rhp_empdag_arcVF *expr) NONNULL;
int mp_addvar(MathPrgm *mp, rhp_idx vi);
int mp_addvars(MathPrgm *mp, Avar *v);
int mp_addvipair(MathPrgm *mp, rhp_idx ei, rhp_idx vi);
int mp_addvipairs(MathPrgm *mp, Aequ *e, Avar *v);
int mp_finalize(MathPrgm *mp) NONNULL;
rhp_idx mp_getobjequ(const MathPrgm *mp);
rhp_idx mp_getobjvar(const MathPrgm *mp);
ModelType mp_getprobtype(const MathPrgm *mp) NONNULL;
RhpSense mp_getsense(const MathPrgm *mp) NONNULL;
void mp_print(MathPrgm *mp, const Model *mdl);
int mp_setobjequ(MathPrgm *mp, rhp_idx ei);
int mp_setobjvar(MathPrgm *mp, rhp_idx vi);
int mp_setprobtype(MathPrgm *mp, unsigned probtype);
int mp_from_ccflib(MathPrgm *mp, unsigned ccflib_idx) NONNULL;


void mp_free(MathPrgm *mp);
MALLOC_ATTR(mp_free, 1)
MathPrgm *mp_new(mpid_t id, RhpSense sense, Model *mdl);
MALLOC_ATTR(mp_free, 1)
MathPrgm *mp_dup(const struct rhp_mathprgm *mp_src,
                              Model *mdl);

int mp_isconstraint(const MathPrgm *mp, rhp_idx eidx) NONNULL;

mpid_t mp_getid(const MathPrgm *mp) NONNULL;
double mp_getobjjacval(const MathPrgm *mp) NONNULL;
const char *mp_gettypestr(const MathPrgm *mp) NONNULL;
unsigned mp_getnumcons(const MathPrgm *mp) NONNULL;
unsigned mp_getnumzerofunc(const MathPrgm *mp) NONNULL;
unsigned mp_getnumvars(const MathPrgm *mp) NONNULL;
unsigned mp_getnumoptvars(const MathPrgm *mp) NONNULL;
int mp_objvarval2objequval(MathPrgm *mp) NONNULL;
int mp_fixobjequval(MathPrgm *mp) NONNULL;

int mp_ensure_objfunc(MathPrgm *mp, rhp_idx *ei) NONNULL;

int mp_trim_memory(MathPrgm *mp) NONNULL;
int mp_setobjcoef(MathPrgm *mp, double coef) NONNULL;
int mp_rm_cons(MathPrgm *mp, rhp_idx ei) NONNULL;
int mp_rm_var(MathPrgm *mp, rhp_idx vi) NONNULL;

int mp_instantiate_fenchel_dual(MathPrgm *mp) NONNULL;
int mp_instantiate(MathPrgm *mp) NONNULL;
int mp_add_objfn_mp(MathPrgm *mp_dst, MathPrgm *mp_src) NONNULL;
int mp_add_objfn_map(MathPrgm *mp, Lequ * restrict le) NONNULL;
int mp_operator_kkt(MathPrgm *mp) NONNULL;

void mp_err_noobjdata(const MathPrgm *mp) NONNULL;

const char* mp_getname_(const MathPrgm * mp, mpid_t mp_id) NONNULL;

const char* mptype2str(unsigned type);

NONNULL static inline MpType mp_gettype(const MathPrgm *mp)
{
  return mp->type;
}

NONNULL static inline const char* mp_getname(const MathPrgm * mp)
{
   return mp_getname_(mp, mp->id);
}

NONNULL static inline bool mp_isopt(const MathPrgm *mp) {
   MpType type = mp->type;
   assert(type != MpTypeUndef);

   switch(type) {
   case MpTypeOpt:
/* See GITLAB #83 */
//   case MpTypeMpec:
//   case MpTypeMpcc:
      return true;
   default:
      return false;
   }
}

NONNULL static inline bool mp_isvi(const MathPrgm *mp) {
   MpType type = mp->type;
   assert(type != MpTypeUndef);

/* See GITLAB #83 */
//   case MpTypeMcp:
   return type == MpTypeVi;
}

NONNULL static inline bool mp_isccflib(const MathPrgm *mp) {
   MpType type = mp->type;
   assert(type != MpTypeUndef);

   return type == MpTypeCcflib;
}

NONNULL static inline bool mp_isdual(const MathPrgm *mp) {
   MpType type = mp->type;
   assert(type != MpTypeUndef);

   return type == MpTypeDual;
}

NONNULL static inline bool mp_isfooc(const MathPrgm *mp) {
   MpType type = mp->type;
   assert(type != MpTypeUndef);

   return type == MpTypeFooc;
}

NONNULL static inline bool mp_ishidable(const MathPrgm *mp) {
   return mp->status & MpIsHidable;
}

NONNULL static inline bool mp_ishidden(const MathPrgm *mp) {
   return mp->status & MpIsHidden;
}

NONNULL static inline bool mp_isfinalized(const MathPrgm *mp) {
   return mp->status & MpFinalized;
}

NONNULL static inline void mp_hide(MathPrgm *mp) {
   mp->status |= MpIsHidden;
}

NONNULL static inline void mp_hidable_asdual(MathPrgm *mp) {

   mp->status |= MpIsHidableAsDual;
}

NONNULL static inline void mp_hidable_askkt(MathPrgm *mp) {

   mp->status |= MpIsHidableAsKkt;
}


NONNULL static inline void mp_unfinalized(MathPrgm *mp) {
   mp->status &= ~MpFinalized;
}

NONNULL static inline void mp_unhide(MathPrgm *mp) {
   mp->status &= ~(MpIsHidden | MpIsHidable);
}

NONNULL static inline int mp_reserve(MathPrgm *mp, unsigned nvars, unsigned nequs)
{
   if (mp->vars.len > 0 || mp->equs.len > 0) {
      error("[mp] ERROR: MP(%s) has %u vars and %u equs\n", mp_getname(mp),
            mp->vars.len, mp->equs.len);
      return Error_RuntimeError;
   }

   S_CHECK(rhp_idx_reserve(&mp->vars, nvars));
   S_CHECK(rhp_idx_reserve(&mp->equs, nequs));

   return OK;
}

NONNULL static inline bool mp_isvalid(const MathPrgm *mp) {
   return (mp->mdl && mpid_regularmp(mp->id));
}

#endif /* MATHPRGM_H */
