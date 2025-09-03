#include "asprintf.h"

#include <math.h>
#include <stdio.h>
#include <string.h>

/* TODO(xhub) this is just for printing, move elsewhere */
#include "ccflib_reformulations.h"
#include "ccflib_utils.h"
#include "cmat.h"
#include "container.h"
#include "ctr_rhp.h"
#include "empinfo.h"
#include "equvar_helpers.h"
#include "equvar_metadata.h"
#include "lequ.h"
#include "macros.h"
#include "mathprgm.h"
#include "mathprgm_helpers.h"
#include "mathprgm_priv.h"
#include "mdl.h"
#include "ctrdat_rhp.h"
#include "ovf_common.h"
#include "ovfinfo.h"
#include "ovf_fenchel.h"
#include "printout.h"
#include "status.h"
#include "toplayer_utils.h"


#define mp_fmtargs(mp) mp_getnamelen(mp), mp_getname(mp), (mp)->id

#define DEFINE_STR() \
DEFSTR(MpTypeUndef, "UNDEF") \
DEFSTR(MpTypeOpt, "OPT") \
DEFSTR(MpTypeVi, "VI") \
DEFSTR(MpTypeCcflib, "CCFLIB") \
DEFSTR(MpTypeDual, "DUAL") \
DEFSTR(MpTypeFooc, "FOOC")

/* See GITLAB #83 */
/* DEFSTR(MpTypeMcp, "MCP") \
 DEFSTR(MpTypeQvi, "QVI") \
 DEFSTR(MpTypeMpcc, "MPCC") \
 DEFSTR(MpTypeMpec, "MPEC") \
*/

#define DEFSTR(id, str) char id##_[sizeof(str)];

typedef struct {
   union {
      struct {
      DEFINE_STR()
   };
   char dummystr[1];
   };
} MpTypesNames;
#undef DEFSTR

#define DEFSTR(id, str) .id##_ = str,

static const MpTypesNames mptypesnames = {
DEFINE_STR()
};
#undef DEFSTR

#define DEFSTR(id, str) [id] = offsetof(MpTypesNames, id##_), 
static const unsigned mptypesnames_offsets[] = {
DEFINE_STR()
};
#undef DEFSTR


static_assert(sizeof(mptypesnames_offsets)/sizeof(mptypesnames_offsets[0]) == MpTypeLast+1,
              "Incompatible sizes!");

const char* mptype2str(unsigned type)
{
   if (type >= MpTypeLast) return "ERROR unknown MP type";

   return mptypesnames.dummystr + mptypesnames_offsets[type];
}

static inline int mp_addvarchk(MathPrgm *mp, rhp_idx vi)
{
   assert(mp->mdl->ctr.varmeta);
   int rc = OK;

   S_CHECK(vi_inbounds(vi, ctr_nvars_total(&mp->mdl->ctr), __func__));

   unsigned mp2_id = mp->mdl->ctr.varmeta[vi].mp_id;
   if (mp2_id != MpId_NA) {
      EmpDag *empdag = &mp->mdl->empinfo.empdag;
      if (mp2_id != mp->id) {
         error("[empdag] ERROR: trying to add variable '%s' (#%u) to MP('%s'), "
               "but it already belongs to MP '%s'.\n Shared variable are not "
               "supported yet.\n", ctr_printvarname(&mp->mdl->ctr, vi), vi,
               empdag_getmpname(empdag, mp->id),
               empdag_getmpname(empdag, mp2_id));
      } else {
          error("[empdag] ERROR: variable '%s' (#%u) already belongs to MP('%s'),"
                " and it was supposed to be added again to the same MP node.\n",
                ctr_printvarname(&mp->mdl->ctr, vi), vi,
                empdag_getmpname(empdag, mp->id));
      }
      return Error_Inconsistency;
   }

   mp->mdl->ctr.varmeta[vi].mp_id = mp->id;
   MP_DEBUG("MP(%s) owns var '%s'\n", empdag_getmpname(&mp->mdl->empinfo.empdag, mp->id),
            ctr_printvarname(&mp->mdl->ctr, vi));

   rc = rhp_idx_addsorted(&mp->vars, vi);
   if (rc == Error_DuplicateValue) {
      error("%s :: variable %s is already assigned to MP %d\n",
            __func__, ctr_printvarname(&mp->mdl->ctr, vi), mp->id);
   }

   return rc;
}

static inline int mp_addequchk(MathPrgm *mp, rhp_idx ei)
{
   assert(mp->mdl->ctr.equmeta);
   S_CHECK(ei_inbounds(ei, ctr_nequs_total(&mp->mdl->ctr), __func__));
   int rc = OK;

   unsigned mp2_id = mp->mdl->ctr.equmeta[ei].mp_id;
   if (mp2_id != UINT_MAX) {
      EmpDag *empdag = &mp->mdl->empinfo.empdag;
      error("[MP] ERROR: trying to add equation '%s' to MP '%s', but it already belongs "
            "to MP '%s'.\n For a shared constraint, remember to declare it as "
            "such beforehand.\n", ctr_printequname(&mp->mdl->ctr, ei),
            empdag_getmpname(empdag, mp->id), empdag_getmpname(empdag, mp2_id));
      return Error_Inconsistency;
   }

   mp->mdl->ctr.equmeta[ei].mp_id = mp->id;
   MP_DEBUG("MP(%s) owns equ '%s'\n", empdag_getmpname(&mp->mdl->empinfo.empdag, mp->id),
            ctr_printequname(&mp->mdl->ctr, ei));

   rc = rhp_idx_addsorted(&mp->equs, ei);
   if (rc == Error_DuplicateValue) {
      EmpDag *empdag = &mp->mdl->empinfo.empdag;
      error("[MP] ERROR: equation '%s' is already assigned to MP '%s'\n",
            ctr_printequname(&mp->mdl->ctr, ei), empdag_getmpname(empdag, mp->id));
   }

   return rc;
}

static inline int _setequrolechk(MathPrgm *mp, rhp_idx ei, EquRole role)
{
   EquRole equrole = _getequrole(mp, ei);
   if (equrole != EquUndefined) {
      equmeta_rolemismatchmsg(&mp->mdl->ctr, ei, equrole, EquUndefined, __func__);
      return Error_UnExpectedData;
   }

   _setequrole(mp, ei, role);

   return OK;
}

static int err_noobj(MathPrgm *mp)
{
   error("[empdag] ERROR: MP(%s) is of type %s (#%u) which does not have"
         " an objective function\n", mp_getname(mp), mptype2str(mp->type), mp->type);

   if (MpTypeUndef == mp->type) {
      errormsg("\tMP has undefined type\n");
   }

   return Error_InvalidValue;
}

static unsigned mp_getnamelen_(const MathPrgm * mp, unsigned mp_id)
{
   assert(mp->mdl);
   const struct mp_namedarray *mps = &mp->mdl->empinfo.empdag.mps;
   const char *mp_name = mp_id < mps->len ? mps->names[mp_id] : NULL;

   return mp_name ? strlen(mp_name) : 0;
}

const char* mp_getname_(const MathPrgm * mp, mpid_t mp_id)
{
   if (!mp->mdl) { return "no model"; }

   return empdag_getmpname(&mp->mdl->empinfo.empdag, mp_id);
}

static inline unsigned mp_getnamelen(const MathPrgm * mp)
{
   return mp_getnamelen_(mp, mp->id);
}

NONNULL static bool _mp_own_var(const MathPrgm *mp, rhp_idx vidx) {
  assert(mp->mdl->ctr.varmeta && valid_vi(vidx));
  unsigned mp2_id = mp->mdl->ctr.varmeta[vidx].mp_id;
  if (mp->id == mp2_id) {
    return true;
  }
  return false;
}

/* We assume that eidx is valid */
UNUSED static bool isconstraint_opt(MathPrgm *mp, rhp_idx eidx) {
   if (eidx == mp->opt.objequ) {
      return false;
   }

   return true;
}

UNUSED static bool isconstraint_vi(MathPrgm *mp, rhp_idx eidx) {
   if (mp->vi.num_cons > 0) {
      if (mp->mdl->ctr.equmeta[eidx].role == EquConstraint) {
         return false;
      }
      for (unsigned i = 0; i < mp->equs.len; ++i) {
         if (eidx == mp->equs.arr[i]) {
            return true;
         }
      }
   }

   return false;
}

/**
 * @brief Allocate a mathematical program object
 *
 * @param  mdl  the model for this mathematical program object
 *
 * @return      the error code
 */
static MathPrgm *mp_alloc(Model *mdl) {
   MathPrgm *mp;

   CALLOC_NULL(mp, MathPrgm, 1);

   mp->type = MpTypeUndef;
//   mp->type_subdag = EmpDag_Empty;
   mp->probtype = MdlType_none;

   /* Do not borrow to avoid circular reference */
   mp->mdl = mdl;

   rhp_idx_init(&mp->equs);
   rhp_idx_init(&mp->vars);

   return mp;
}

MathPrgm *mp_new(mpid_t id, RhpSense sense, Model *mdl)
{
   MathPrgm *mp;
   AA_CHECK(mp, mp_alloc(mdl));

   if (id > MpId_MaxRegular) {
      error("[MP] ERROR: cannot create MP with ID %u, the max one is %u\n",
            id, MpId_MaxRegular);
      return NULL;
   }

   mp->id = id;
   mp->sense = sense;

   switch (sense) {
   case RhpFeasibility:
      mpvi_init(&mp->vi);
      mp->type = MpTypeVi;
      break;
   case RhpMin: case RhpMax:
      mpopt_init(&mp->opt);
      mp->type = MpTypeOpt;
      break;
   case RhpDualSense:
      mpopt_init(&mp->opt);
      mp->type = MpTypeDual;
      break;
   default:
      mp->type = MpTypeUndef;
   }

   MP_DEBUG("Starting MP #%u of sense %s\n", mp->id, sense2str(sense));

   return mp;
}

MathPrgm *mp_new_fake(void)
{
   MathPrgm *mp;
   AA_CHECK(mp, mp_alloc(NULL));

   mp->sense = RhpNoSense;
   return mp;
}

MathPrgm *mp_dup(const MathPrgm *mp_src, Model *mdl)
{
   MathPrgm *mp;
   AA_CHECK(mp, mp_new(mp_src->id, mp_src->sense, mdl));

   mp->type = mp_src->type;
   mp->probtype = mp_src->probtype;
   mp->status = mp_src->status;

   switch (mp->type) {
   case MpTypeOpt:
      memcpy(&mp->opt, &mp_src->opt, sizeof(mp->opt));
      break;
   case MpTypeVi:
      memcpy(&mp->vi, &mp_src->vi, sizeof(mp->vi));
      break;
   case MpTypeDual:
      mp->dual = mp_src->dual;
      break;
   case MpTypeCcflib:
      mp->ccflib.ccf = ovfdef_borrow(mp_src->ccflib.ccf);
      if (mp_src->ccflib.mp_instance) {
         errormsg("[MP] ERROR: need to implement duplication of CCFLIB with instance\n");
         mp_free(mp);
         return NULL;
      }
      mp->ccflib.mp_instance = NULL;
      break;
   default:
      error("[MP] ERROR while duplicating MP(%s): type %s is not implemented\n",
            mp_getname(mp_src), mptype2str(mp->type));
      mp_free(mp);
      return NULL;
   }

   assert(rhp_idx_chksorted(&mp_src->equs));
   SN_CHECK(rhp_idx_copy(&mp->equs, &mp_src->equs));
   SN_CHECK(rhp_idx_copy(&mp->vars, &mp_src->vars));

   return mp;
}

void mp_free(MathPrgm *mp)
{
   if (!mp) {
      return;
   }

   trace_refcnt("[MP] Freeing MP(%s)\n", mp_getname(mp));

   if (mp->type == MpTypeCcflib) {
      ovfdef_free(mp->ccflib.ccf);
   }

   rhp_idx_empty(&mp->equs);
   rhp_idx_empty(&mp->vars);

   FREE(mp);
}

/**
 * @brief Set an MP to be a CCFLIB one
 *
 * @param mp          the mathematical program
 * @param ccflib_idx  the index of the CCF to instanciate
 *
 * @return            the error code
 */
int mp_from_ccflib(MathPrgm *mp, unsigned ccflib_idx)
{
   assert(mp->sense == RhpNoSense && mp->type == MpTypeUndef);

   A_CHECK(mp->ccflib.ccf, ovfdef_new(ccflib_idx));
   mp->ccflib.mp_instance = NULL;

   mp->sense = mp->ccflib.ccf->sense;
   mp->type = MpTypeCcflib;

   return OK;
}

int mp_setprobtype(MathPrgm *mp, unsigned probtype)
{
   if (probtype >= mdltypeslen) {
      error( "%s :: MP type %u is above the limit %u\n", __func__,
            probtype, mdltypeslen - 1);
      return Error_InvalidValue;
   }

   mp->probtype = probtype;

   return OK;
}

/**
 * @brief Add an equation to an MP
 *
 * This should only be used when the type of the equation has already been
 * set in a different way. This is for instance useful to add a VI function to
 * an MP. Otherwise use mp_addconstraint() or mp_addvipair()
 *
 * @param mp  the MP
 * @param ei  the equation to add
 *
 * @return    the error code
 */
int mp_addequ(MathPrgm *mp, rhp_idx ei)
{
   S_CHECK(mp_addequchk(mp, ei));
   EquRole equrole = _getequrole(mp, ei);
   if (equrole == EquUndefined) {
      error("%s :: equ '%s' #%u has no defined type, it can't be added to a MP.\n",
            __func__, ctr_printequname(&mp->mdl->ctr, ei), ei);
      return Error_EMPIncorrectInput;
   }

   return OK;
}

/**
 * @brief Set the objective equation of an MP
 *
 * @param mp      the MP
 * @param objequ  the objective equation
 *
 * @return    the error code
 */
int mp_setobjequ(MathPrgm *mp, rhp_idx objequ)
{
   S_CHECK(mp_setobjequ_internal(mp, objequ));
   S_CHECK(mp_addequchk(mp, objequ));

   return OK;
}

/**
 * @brief Set the objective coefficient
 *
 * This is useful when the OPT problem is specified using a objective variable
 *
 * @param mp    the MP
 * @param coef  the coefficient
 *
 * @return      the error code
 */
int mp_setobjcoef(MathPrgm *mp, double coef) {
   assert(isfinite(coef));
   if (mp_isopt(mp)) {
      mp->opt.objcoef = coef;
   } else {
      return err_noobj(mp);
   }

   return OK;
}

int mp_setobjequ_internal(MathPrgm *mp, rhp_idx objequ)
{
   assert(valid_ei(objequ));

   if (!mp_isopt(mp)) {
      return err_noobj(mp);
   }

   rhp_idx objequ_old = mp->opt.objequ;

   if (valid_ei(objequ_old)) {
      S_CHECK(rhp_idx_rmsortednofail(&mp->equs, objequ_old));
   }

   mp->opt.objequ = objequ;
   _setequrole(mp, objequ, EquObjective);

   // HACK: this can create havoc. Should not be done. Try to delete
   rhp_idx objvar = mp->opt.objvar;
   if (valid_vi(objvar)) {
      _addvarbasictype(mp, objvar, VarIsExplicitlyDefined);
   }

   return OK;
}

/* TODO: merge the 2 functions? */
/**
 * @brief Set the objective variable of an MP
 *
 * @param mp      the MP
 * @param objvar  the objective variable
 *
 * @return    the error code
 */
int mp_setobjvar(MathPrgm *mp, rhp_idx objvar)
{
   assert(mp_isvalid(mp));

   if (!valid_vi(objvar)) {
      if (objvar == IdxNA && mp_isopt(mp)) {
         rhp_idx objvar_old = mp->opt.objvar;
         mp->opt.objvar = IdxNA;

         if (valid_vi(objvar_old)) {
            S_CHECK(rhp_idx_rmsorted(&mp->vars, objvar_old));

            /* TODO: rethink that design */
            VarMeta *vmeta = &mp->mdl->ctr.varmeta[objvar_old];
            if (vmeta->type != VarObjective) {
               error("[MP] ERROR: in MP(%s), old objective variable was not "
                     "marked as such.\n", mp_getname(mp));
               return Error_RuntimeError;
            }
            vmeta->type = VarPrimal;
            vmeta->ppty = VarPptyNone;
         }

         return OK;
      }

      error("[mp] ERROR: cannot set objvar of MP to invalid index '%.*s' #%u",
            mp_getnamelen(mp), mp_getname(mp), mp->id);
      return Error_EMPRuntimeError;
   }

   unsigned mp_id = mp->mdl->ctr.varmeta[objvar].mp_id;
   if (!valid_mpid(mp_id)) {
      S_CHECK(mp_addvarchk(mp, objvar));
   } else if (mp_id != mp->id) {
      error("[MP] ERROR: Trying to set objective variable of MP(%.*s) (ID #%u) "
            "to '%s', but the latter belong to MP(%s)\n", mp_fmtargs(mp),
            mdl_printvarname(mp->mdl, objvar), mpid_getname(mp->mdl, mp_id));
      return Error_EMPRuntimeError;
   }

   S_CHECK(mp_setobjvar_internal(mp, objvar));

   return OK;
}

int mp_setobjvar_internal(MathPrgm *mp, rhp_idx vi)
{
   if (mp_isopt(mp)) {
      mp->opt.objvar = vi;

      /*  TODO(xhub) harder check: we should only allow a special value here */
      if (!valid_vi(vi)) {
         return OK;
      }

      _setvarrole(mp, vi, VarObjective);
      switch (mp->sense) {
      case RHP_MIN:
         _addvarbasictype(mp, vi, VarIsObjMin);
         break;
      case RHP_MAX:
         _addvarbasictype(mp, vi, VarIsObjMax);
         break;
      default:
         error("%s :: invalid MP sense %s (#%d)\n", __func__,
               sense2str(mp->sense), mp->sense);
         return Error_EMPRuntimeError;
      }
   } else {
      return err_noobj(mp);
   }

   return OK;
}


/**
 * @brief Fix the objective equation multiplier of the MP to be the marginal if it was a constraint
 *
 * @param mp  the MP
 *
 * @return    the error code
 */
int mp_objequ_setmultiplier(MathPrgm *mp) {

   if (!mp_isopt(mp)) { return OK; }

   rhp_idx objequ = mp->opt.objequ;
   rhp_idx objvar = mp->opt.objvar;

   if (!valid_vi(objvar) || !valid_ei(objequ)) { return OK; }

   Equ *e = &mp->mdl->ctr.equs[objequ];
   double objvarcoeff;
   unsigned pos;
   lequ_find(e->lequ, objvar, &objvarcoeff, &pos);

   /* TODO: what is going on if it is not linear? */
   if (isfinite(objvarcoeff)) {
      assert(mp->mdl);
      /* We can't set e->multiplier since we could have a non-rhp backend */
      Container *ctr = &mp->mdl->ctr;
      S_CHECK(ctr->ops->setequmult(ctr, objequ, 1./objvarcoeff));
   }

   return OK;
}

/** @brief Add a constraint to this MP
 *
 * @param mp the MP
 * @param ei the constraint to add
 *
 * @return   the error code
 */
int mp_addconstraint(MathPrgm *mp, rhp_idx ei)
{
   S_CHECK(mp_addequchk(mp, ei));
   _setequrolechk(mp, ei, EquConstraint);

   /*  TODO(xhub) MPEC,MPCC,EPEC */
   if (mp->type == MpTypeVi) {
      mp->vi.num_cons++;
   }

   return OK;
}

int mp_addconstraints(MathPrgm *mp, Aequ *e) {
   for (size_t i = 0, size = e->size; i < size; ++i) {
      rhp_idx ei = aequ_fget(e, i);
      S_CHECK(mp_addequchk(mp, ei));
      _setequrolechk(mp, ei, EquConstraint);
      /* ------------------------------------------------------------
     * Via 'dualvar', some equations may already have dual variables.
     * Make them owned by this mp
     * ------------------------------------------------------------ */

      rhp_idx dual = _getequdual(mp, ei);
      if (valid_ei(dual)) {
         TO_IMPLEMENT("This code needs to be checked");
         //            if (_getvartype(mp, dual) == Varmeta_Dual) {
         //               S_CHECK(rhp_idx_addsorted(&mp->vars, dual));
         //            }
      }
   }

   /*  TODO(xhub) MPEC,MPCC,EPEC */
   if (mp->type == MpTypeVi) {
      mp->vi.num_cons += e->size;
   }

   return OK;
}

/** @brief Add a variable to an MP
 *
 *  @param mp  the MP
 *  @param vi  the variable index
 *
 *  @return    the error code
 */
int mp_addvar(MathPrgm *mp, rhp_idx vi)
{
   S_CHECK(mp_addvarchk(mp, vi));
   _setvarrole(mp, vi, VarPrimal);

   return OK;
}

/** @brief Add variables to an MP
 *
 *  @param mp  the MP
 *  @param v   the variable container
 *
 *  @return    the error code
 */
int mp_addvars(MathPrgm *mp, Avar *v)
{
   for (size_t i = 0; i < v->size; ++i) {
      rhp_idx vi = avar_fget(v, i);
      S_CHECK(mp_addvarchk(mp, vi));
      _setvarrole(mp, vi, VarPrimal);
   }

   return OK;
}

/**
 *  @brief Add a vi pair (equ,var) to the MP
 *
 *  @param  mp    the MP
 *  @param  ei    the equation index. If invalid, the equ is the zero function
 *  @param  vi    the variable index
 *
 *  @return       the error code
 */
int mp_addvipair(MathPrgm *mp, rhp_idx ei, rhp_idx vi)
{
   /* ---------------------------------------------------------------------
   * Add variable.
   * --------------------------------------------------------------------- */

   S_CHECK(mp_addvarchk(mp, vi));

   if (valid_ei(ei)) {

      /* ------------------------------------------------------------------
     * Add VI equation. Change the equation type to mapping
     * ------------------------------------------------------------------ */

      S_CHECK(mp_addequchk(mp, ei));
      mp->mdl->ctr.equs[ei].object = Mapping;

   } else {

      mp->vi.num_zeros++;
   }

   S_CHECK(rctr_setequvarperp(&mp->mdl->ctr, ei, vi));

   return OK;
}

int mp_isconstraint(const MathPrgm *mp, rhp_idx ei) {
   return Error_NotImplemented;
#if 0
   switch (mp->type) {
   case MP_OPT:
      return isconstraint_opt(mp, rowidx);
   case MP_VI:
      return isconstraint_vi(mp, rowidx);
   default:
      return FALSE;
   }
#endif
}

/** @brief Add a vi pair (equ var) to the MP
 *
 *  @param  mp    the MP
 *  @param  e     the equation indices
 *  @param  v     the variable indices
 *
 *  @return       the error code
 */
int mp_addvipairs(MathPrgm *mp, Aequ *restrict e, Avar *restrict v)
{
   unsigned vlen = v->size;
   if (e && vlen != e->size) {
      error("%s :: the numbers of equations and variables differ: %u vs %u\n",
            __func__, e->size, v->size);
      return Error_DimensionDifferent;
   }

   for (size_t i = 0; i < vlen; ++i) {
      rhp_idx vi = avar_fget(v, i);
      rhp_idx ei = e ? aequ_fget(e, i) : IdxNA;

      S_CHECK(mp_addvipair(mp, ei, vi));
   }

   return OK;
}

static int mp_ccflib_finalize(MathPrgm *mp)
{
   OvfDef *ovfdef = mp->ccflib.ccf;
   S_CHECK(ccflib_finalize(mp->mdl, ovfdef, mp));

   return OK;
}

/** @brief Finalize the MP creation
 *
 * Call this when the MP has been completed. This functions looks to set the
 * objective function
 *
 * @param mp  the MP
 *
 * @return the error code
 */
int mp_finalize(MathPrgm *mp)
{
   if (mp->status & MpFinalized) {
      return OK;
   }

   MP_DEBUG("Finalizing MP '%s'\n", empdag_getmpname(&mp->mdl->empinfo.empdag, mp->id));

   switch (mp->type) {
   case MpTypeCcflib:
      S_CHECK(mp_ccflib_finalize(mp));
      goto _finalize;
      break;
   case MpTypeDual:
      goto _finalize;
      break;
   case MpTypeOpt:
   case MpTypeVi:
      break;
   case MpTypeUndef:
   default:
      error("[MP] ERROR: MP(%s) (ID #%u) has no type set\n",
            empdag_getmpname(&mp->mdl->empinfo.empdag, mp->id), mp->id);
      return Error_EMPRuntimeError;
   }

   if (mp->vars.len == 0 && (mp->type != MpTypeVi || !mp->vi.has_kkt)) {
      EmpDag *empdag = &mp->mdl->empinfo.empdag;

      /* If the EMPDAG has not being finalized, we need to delay here */
      if (!empdag->has_resolved_arcs) { return OK; }

      if (empdag->mps.Varcs->len == 0) {

         error("[MP] ERROR: MP(%s) has no variable assigned to it and no child "
               "in the EMPDAG\n",
               empdag_getmpname(empdag, mp->id));
         return Error_EMPRuntimeError;
      }
      
      // HACK: Check that this is correct
      goto _finalize;
   }

   if (!mp_isopt(mp)) { goto _finalize; }

  /* ----------------------------------------------------------------------
   * We can't enforce the exitence of objvar or objequ if we called from
   * the empinterp and there is a trivial objequ with only objVF contributions
   * ---------------------------------------------------------------------- */

   rhp_idx objvar = mp->opt.objvar;
   if (!valid_ei(mp->opt.objequ)) {
      if (!valid_vi(objvar)) {
         const EmpDag *empdag = &mp->mdl->empinfo.empdag;
         if (!empdag->has_resolved_arcs) { return OK; }

         if (!empdag_mp_hasobjfn_modifiers(empdag, mp->id)) {
            mp_err_noobjdata(mp);
            return Error_IncompleteModelMetadata;
         }

         goto _finalize;
      }

      S_CHECK(mp_identify_objequ(mp));
   }

   rhp_idx objequ = mp->opt.objequ;

   if (valid_vi(objvar)) {
      S_CHECK(mdl_checkobjequvar(mp->mdl, objvar, objequ));
      goto _finalize;
   }

   unsigned type, cone;
   S_CHECK(ctr_getequtype(&mp->mdl->ctr, objequ, &type, &cone));
   if (type != Mapping) {
      error("[MP] ERROR: In MP(%s) objequ '%s' has type %s, should be %s. ",
            mp_getname(mp), mdl_printequname(mp->mdl, objequ), equtype_name(type), equtype_name(Mapping));
      errormsg("If there is an objective variable, it should be added to ");
      errormsg("the MP *before* the objective equation\n");
      return Error_EMPRuntimeError;
   }

_finalize:
   mp->status |= MpFinalized;

   return OK;
}

/* ---------------------------------------------------------------------
 * GETTERS
 * --------------------------------------------------------------------- */

/**
 * @brief Get the ID of an MP
 *
 * @param mp the MP
 *
 * @return   the ID
 */
mpid_t mp_getid(const MathPrgm *mp) { return mp->id; }

double mp_getobjjacval(const MathPrgm *mp) {
   if (mp_isopt(mp)) {
      return mp->opt.objcoef;
   }

   return NAN;
}

/**
 * @brief Get the sense of this MP
 *
 * @param  mp  the mathprgm
 *
 * @return the sense. If the MP has no sense, the RNP_NOSENSE
 */
RhpSense mp_getsense(const MathPrgm *mp) {
   return mp->sense;
}

/**
 * @brief Get the objective equation of an MP
 *
 * @param mp the MP
 *
 * @return the objective equation index (could be invalid)
 */
rhp_idx mp_getobjequ(const MathPrgm *mp) {
   if (mp_isopt(mp)) {
      return mp->opt.objequ;
   }

   if (mp->type == MpTypeCcflib) {
      return IdxCcflib;
   }

   return IdxInvalid;
}

/** @brief Get the objective variable of an MP
 *
 * @param mp the MP
 *
 * @return the objective variable index (could be invalid)
 */
rhp_idx mp_getobjvar(const MathPrgm *mp)
{
   if (mp_isopt(mp)) {
      return mp->opt.objvar;
   }

   return IdxInvalid;
}

void mp_err_noobjdata(const MathPrgm *mp)
{
   const EmpDag *empdag = &mp->mdl->empinfo.empdag;
   error("[MP] ERROR: the optimization MP(%s) has neither an objective "
         "variable or an objective function. This is unsupported, the MP must "
         "have have exactly one of these.\n", empdag_getmpname(empdag, mp->id));
}


inline ModelType mp_getprobtype(const MathPrgm *mp) {
   return mp->probtype;
}

const char *mp_gettypestr(const MathPrgm *mp) {
   return mptype2str(mp->type);
}

unsigned mp_getnumzerofunc(const MathPrgm *mp) {
   if (mp->type == MpTypeVi) {
      return mp->vi.num_zeros;
   }

   return UINT_MAX;
}

unsigned mp_getnumvars(const MathPrgm *mp)
{
   assert(mp->type != MpTypeVi || mp->vars.len - mp_getnumzerofunc(mp) == mp->equs.len - mp_getnumcons(mp));
   return mp->vars.len;
}

unsigned mp_getnumoptvars(const MathPrgm *mp)
{
   assert(mp->type != MpTypeVi || mp->vars.len - mp_getnumzerofunc(mp) == mp->equs.len - mp_getnumcons(mp));

   unsigned num_vars = mp->vars.len;
   rhp_idx objvar = mp->opt.objvar;
   if (!valid_vi(objvar)) {
      return num_vars;
   }

   rhp_idx vi = mp->opt.objvar;
   assert(mdl_valid_vi_(mp->mdl, vi, __func__));

   VarMeta *varmeta = mp->mdl->ctr.varmeta;
   if (varmeta) {
      return num_vars - (var_is_defined_objvar(&varmeta[vi]) ? 1 : 0);
   }

   return num_vars;
}

unsigned mp_getnumcons(const MathPrgm *mp)
{
   switch (mp->type) {
   case MpTypeVi:
      return mp->vi.num_cons;
   case MpTypeOpt:
      return mp->equs.len - (valid_ei(mp->opt.objequ) ? 1 : 0);
   default:
      return UINT_MAX;
   }
}

/**
 * @brief Print the content of an MP
 *
 * @param mp  the MP
 */
void mp_print(MathPrgm *mp)
{
   const Model *mdl = mp->mdl;
   const Container *ctr = &mdl->ctr;

   printout(PO_INFO, " - mathprgm %5d", mp->id);

   if (mp_isopt(mp)) {
      printout(PO_INFO, " (%s) of type %s (%d)\n\n", "optimization",
               mdl_getprobtypetxt(mp_getprobtype(mp)),
               mp_gettype(mp));

      rhp_idx objvar = mp_getobjvar(mp);
      rhp_idx objequ = mp_getobjequ(mp);

      if (valid_vi(objvar)) {
         printout(PO_INFO, " Objective variable is %s (%d) with coefficient %e\n",
                  ctr_printvarname(&mdl->ctr, objvar), objvar,
                  mp_getobjjacval(mp));
      }
      if (valid_ei(objequ)) {
         printout(PO_INFO, " Objective equation is %s (%d)\n",
                  ctr_printequname(ctr, objequ), objequ);
      }

   } else if (mp->type == MpTypeVi) {
      printout(PO_INFO, " (%s)\n\n", "variational inequality");
   } else {
      printout(PO_INFO, " (%s)\n\n", "unknown");
   }

   printout(PO_INFO, "\n Variables owned by this mathprgm:\n");
   for (unsigned i = 0; i < mp->vars.len; ++i) {
      rhp_idx vidx = mp->vars.arr[i];
      printout(PO_INFO, "   [%5d] %s\n", vidx, ctr_printvarname(ctr, vidx));
   }

   printout(PO_INFO, "\n Equations owned by this mathprgm:\n");
   for (unsigned i = 0; i < mp->equs.len; ++i) {
      rhp_idx eidx = mp->equs.arr[i];
      printout(PO_INFO, "   [%5d] %s\n", eidx, ctr_printequname(ctr, eidx));
   }

   if (mdl->backend != RhpBackendReSHOP) {
      return;
   }

   const struct ctrdata_rhp *cdat = (struct ctrdata_rhp *)ctr->data;
   printout(PO_INFO, "\n External Variables in the equations:\n");
   for (unsigned i = 0; i < mp->equs.len; ++i) {
      rhp_idx eidx = mp->equs.arr[i];
      bool has_print_equ = false;

      /*  TODO(xhub) create API to walk over all variables in an eqn */
      struct ctr_mat_elt *me = cdat->equs[eidx];
      while (me) {
         rhp_idx vidx = me->vi;
         if (valid_vi(vidx) && !_mp_own_var(mp, vidx)) {
            if (!has_print_equ) {
               printout(PO_INFO, " [%5d] %s\n", eidx, ctr_printequname(ctr, eidx));
               has_print_equ = true;
            }
            printout(PO_INFO, "       [%5d] %s\n", vidx,
                     ctr_printvarname(ctr, vidx));
         }
         me = me->next_var;
      }
   }

   printout(PO_INFO, "\n");
}

int mp_trim_memory(MathPrgm *mp) {
   assert(mp);

   return OK;
}

/**
 * @brief Remove a variable from the MP
 *
 * @param mp   the MP
 * @param vi   the variable to remove
 *
 * @return     the error code
 */
int mp_rm_var(MathPrgm *mp, rhp_idx vi) {

   if (!valid_vi(vi)) {
      error("%s :: invalid index %d\n", __func__, vi);
      return Error_IndexOutOfRange;
   }

   unsigned mp2_id = mp->mdl->ctr.varmeta[vi].mp_id;
   if (mp2_id != mp->id) {
      error("%s :: variable %d does not belong to MP %d\n", __func__,
            vi, mp->id);
      return Error_Inconsistency;
   }

   mp->mdl->ctr.varmeta[vi].mp_id = UINT_MAX;
   S_CHECK(rhp_idx_rmsorted(&mp->vars, vi));

   return OK;
}

/**
 * @brief Remove a constraint from the MP
 *
 * @param mp   the MP
 * @param ei   the equation to remove
 *
 * @return     the error code
 */
int mp_rm_cons(MathPrgm *mp, rhp_idx ei)
{

   if (!valid_ei(ei)) {
      error("%s :: invalid index %d\n", __func__, ei);
      return Error_IndexOutOfRange;
   }

   EquMeta *equmeta = mp->mdl->ctr.equmeta;
   unsigned mp2_id = equmeta[ei].mp_id;
   if (mp2_id != mp->id) {
      error("%s :: equation '%s' does not belong to MP(%s)\n", __func__,
            mdl_printequname(mp->mdl, ei),
            empdag_getmpname(&mp->mdl->empinfo.empdag, mp->id));
      return Error_Inconsistency;
   }

   equmeta[ei].mp_id = UINT_MAX;
   S_CHECK(rhp_idx_rmsorted(&mp->equs, ei));

   return OK;
}

/**
 * @brief Instantiate the Fenchel dual of an MP
 *
 * @param mp  the MP to instanciate
 *
 * @return    the error code
 */
int mp_instantiate_fenchel_dual(MathPrgm *mp)
{
   Model *mdl = mp->mdl;
   EmpDag *empdag = &mdl->empinfo.empdag;

   if (mp->type != MpTypeDual) {
      error("[MP] ERROR: calling %s on MP(%s) of type %s, should be %s",
            __func__, empdag_getmpname(empdag, mp->id),
            mptype2str(mp->type), mptype2str(MpTypeDual));
      return Error_RuntimeError;
   }

   struct mp_dual *dualdat = &mp->dual;

   MathPrgm *mp_primal;
   S_CHECK(empdag_getmpbyid(empdag, dualdat->mpid_primal, &mp_primal));

   if (mp_primal->type != MpTypeCcflib) {
      TO_IMPLEMENT("Dualization of a generic MP");
   }

   trace_process("[MP] Dualizing MP(%s) into MP(%s) using the Fenchel scheme\n",
                 mp_getname(mp_primal), mp_getname(mp));

   CcflibPrimalDualData ccfdat = {.mp_primal = mp_primal, .mpid_dual = mp->id };
   OvfOpsData ovfd = { .ccfdat = &ccfdat };

   mp->type = MpTypeOpt;

   mp->sense = mp_primal->sense == RhpMin ? RhpMax : RhpMin;

   mpopt_init(&mp->opt);
   mp->status = MpStatusUnset;

   OvfDef *ccf = mp_primal->ccflib.ccf;
   if (ccf->args->size == 0 && ccf->num_empdag_children > 0) {
      S_CHECK(ccflib_dualize_fenchel_empdag(mdl, &ccfdat));
   } else {
      S_CHECK(ovf_fenchel(mdl, OvfType_Ccflib_Dual, ovfd));
   }

   return mp_finalize(mp);
}

int mp_ensure_objfunc(MathPrgm *mp, rhp_idx *ei)
{
   if (!mp_isvalid(mp)) {
      errormsg("[MP] ERROR: invalid MP!\n");
      return Error_RuntimeError;
   }

   if (!mp_isopt(mp)) {
      error("[MP] ERROR: MP(%s) is of type %s, should be %s\n", mp_getname(mp),
            mptype2str(mp->type), mptype2str(MpTypeOpt));
      return Error_RuntimeError;
   }

   rhp_idx ei_ = mp->opt.objequ;
   if (valid_ei(ei_)) {
      *ei = ei_;
      return OK;
   }

   rhp_idx objvar = mp->opt.objvar;

   /* TODO NAMING */
   Equ *eobj = NULL;
   Container *ctr = &mp->mdl->ctr;
   S_CHECK(rctr_add_equ_empty(ctr, ei, &eobj, Mapping, CONE_NONE));

   rhp_idx ei_new = eobj->idx;
   S_CHECK(mp_setobjvar(mp, IdxNA));
   S_CHECK(mp_setobjequ(mp, ei_new));

   if (valid_vi(objvar)) {
      S_CHECK(rctr_equ_addnewvar(ctr, eobj, objvar, 1.));
   }

   EmpDag * empdag = &mp->mdl->empinfo.empdag;
   VarcArray *varcs = empdag->mps.Varcs;
   Varc * varcs_arr = varcs->arr;

   /* Update the objequ in the EMPDAG arcs */
   for (unsigned i = 0, len = varcs->len; i < len; ++i) {
      S_CHECK(arcVF_subei(&varcs_arr[i], ei_, ei_new));
   }

   return OK;
}

int mp_operator_kkt(MathPrgm *mp)
{
   MpType mptype = mp->type;

   switch (mptype) {
   case MpTypeCcflib:
      mp->status |= MpCcflibNeedsFullInstantiation;
      S_CHECK(empdag_mp_needs_instantiation(&mp->mdl->empinfo.empdag, mp->id));
      break;
   case MpTypeOpt:
      break;
   default:
      error("[MP] ERROR: MP(%s) has type %s. Cannot apply kkt operator\n",
            mp_getname(mp), mptype2str(mp->type));
      return Error_EMPIncorrectInput;
   }

   mp_hidable_askkt(mp);

   return OK;
}

int mp_instantiate(MathPrgm *mp)
{
   if (mp->type != MpTypeCcflib) {
      error("[MP] ERROR: MP(%s) has type %s, should be %s\n", mp_getname(mp),
            mptype2str(mp->type), mptype2str(MpTypeCcflib));
      return Error_RuntimeError;
   }

   if (mp->ccflib.mp_instance) {
      error("[MP] ERROR: MP(%s) is already instantiated\n", mp_getname(mp));
      return Error_RuntimeError;
   }

   EmpDag *empdag = &mp->mdl->empinfo.empdag;

   OvfDef *ovf_def = mp->ccflib.ccf; assert(ovf_def);
   RhpSense sense = ovf_def->sense;

   char *name;
   IO_PRINT(asprintf(&name, "%s_instance", mp_getname(mp)));
   /* TODO: NAMING */
   A_CHECK(mp->ccflib.mp_instance, empdag_newmpnamed(empdag, sense, name));
   free(name);

   MathPrgm *mp_instance = mp->ccflib.mp_instance;

   mp_instance->status |= MpIsHidableAsInstance;

   CcflibPrimalDualData  ccfdat = {.mp_primal = mp, .mpid_dual = MpId_NA};
   OvfOpsData ovfd = {.ccfdat = &ccfdat};

   CcflibInstanceData instancedat = {.ops = &ccflib_ops, .ovfd = ovfd};

   return mp_ccflib_instantiate(mp_instance, mp, &instancedat);
}

int mp_add_objfn_mp(MathPrgm *mp_dst, MathPrgm *mp_src)
{
   if (mp_src->type == MpTypeCcflib) {
      if (!mp_src->ccflib.mp_instance) {
         error("[MP] ERROR: MP(%s) should have been instantiated\n", mp_getname(mp_src));
         return Error_RuntimeError;
      }

      mp_src = mp_src->ccflib.mp_instance;
   }
   assert(mp_isopt(mp_dst) && mp_isopt(mp_src));

   trace_process("[MP] Adding the objective function of MP(%s) to MP(%s)\n",
                 mp_getname(mp_src), mp_getname(mp_dst));

   mp_unfinalized(mp_dst);

   Container *ctr = &mp_dst->mdl->ctr;

   rhp_idx objfn_dst;
   S_CHECK(mp_ensure_objfunc(mp_dst, &objfn_dst));
   Equ *eobj_dst = &ctr->equs[objfn_dst];

   if (eobj_dst->object != Mapping) {
      TO_IMPLEMENT("Destination objective is not a mapping");
   }

   rhp_idx objfn_src = mp_getobjequ(mp_src);
   rhp_idx objvar_src = mp_getobjvar(mp_src);

   bool valid_objvar = valid_vi(objvar_src), valid_objequ = valid_ei(objfn_src);

   if (!valid_objequ && !valid_objvar) {
      error("[MP] ERROR: MP(%s) has no valid objective variable or equation\n",
            mp_getname(mp_src));
      return Error_RuntimeError;
   }
 
   if (valid_objequ) {
      return rctr_equ_add_map(ctr, eobj_dst, objfn_src, objvar_src, 1.);
   }

   /* We only have an objvar */
   return rctr_equ_addnewvar(ctr, eobj_dst, objvar_src, 1.);
}

int mp_add_objfn_map(MathPrgm *mp, Lequ * restrict le)
{
   if (!mp_isopt(mp)) {
      error("[MP] ERROR: MP(%s) has type %s, should be %s", mp_getname(mp),
            mptype2str(mp->type), mptype2str(MpTypeOpt));
      return Error_RuntimeError;
   }

   Model *mdl = mp->mdl; assert(mdl);
   Container *ctr = &mdl->ctr;

   rhp_idx objfn;
   S_CHECK(mp_ensure_objfunc(mp, &objfn));
   Equ *eobj = &ctr->equs[objfn];

   if (eobj->object != Mapping) {
      TO_IMPLEMENT("Destination objective is not a mapping");
   }

   rhp_idx * restrict vis = le->vis;
   double * restrict coeffs = le->coeffs;
   for (unsigned i = 0, len = le->len; i < len; ++i) {
      rhp_idx vi = vis[i], ei;
      double c = coeffs[i];

      S_CHECK(ctr_get_defined_mapping_by_var(ctr, vi, &ei));
      
      trace_process("[MP] Adding the mapping defined by '%s' in equation '%s' to "
                    "the objective function of MP(%s) with coefficient %e\n",
                    mdl_printvarname(mdl, vi), mdl_printequname(mdl, ei),
                    mp_getname(mp), c);

      S_CHECK(rctr_equ_add_map(ctr, eobj, ei, vi, c));
   }

   mp_unfinalized(mp);

   return OK;
}

int mp_packing_display(const MathPrgm *mp, uint8_t buf[VMT(static 1024)])
{
   //FIXME size_t len = 0;
   memcpy(buf, &mp->id, sizeof(mp->id));
   buf += sizeof(mp->id);
   *buf++ = (u8)mp->sense;
   *buf++ = (u8)mp->type;
   *buf++ = (u8)mp->status;

   switch (mp->type) {
   case MpTypeOpt:
      memcpy(buf, &mp->opt.objvar, sizeof(rhp_idx)); buf += sizeof(rhp_idx);
      memcpy(buf, &mp->opt.objequ, sizeof(rhp_idx)); buf += sizeof(rhp_idx);
      memset(buf, 0, sizeof(u32));
      break;

   case MpTypeVi:
      memcpy(buf, &mp->vi.num_cons, sizeof(u32)); buf += sizeof(u32);
      memcpy(buf, &mp->vi.num_zeros, sizeof(u32)); buf += sizeof(u32);
      memcpy(buf, &mp->vi.num_matches, sizeof(u32));
      break;

   case MpTypeDual:
      memcpy(buf, &mp->dual.mpid_primal, sizeof(mpid_t)); buf += sizeof(mpid_t);
      memset(buf, 0, 2*sizeof(u32));
      break;

   case MpTypeCcflib:
      memset(buf, 0, 3*sizeof(u32));
      break;

    default:
      error("[MP] ERROR: can't pack MP of type %s\n", mptype2str(mp->type));
      return Error_RuntimeError;
   }

   /*
   const char *name = mp_getname(mp);
   size_t namelen = strlen(name);

   if (namelen > UINT32_MAX) {
      error("[MP] ERROR while packing: name exceeds max length (%u): '%s'\n",
            UINT32_MAX, name);
      return Error_RuntimeError;
   }

   uint32_t slen = (uint32_t)namelen;
   memcpy(buf, &slen, sizeof(slen));
*/
   return OK;
}
