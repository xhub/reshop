#include "reshop_config.h"

#include <math.h>
#include <stdio.h>
#include <string.h>

/* TODO(xhub) this is just for printing, move elsewhere */
#include "cmat.h"
#include "container.h"
#include "ctr_rhp.h"
#include "empinfo.h"
#include "equvar_helpers.h"
#include "equvar_metadata.h"
#include "macros.h"
#include "mathprgm.h"
#include "mathprgm_helpers.h"
#include "mathprgm_priv.h"
#include "mdl.h"
#include "ctrdat_rhp.h"
#include "ovfinfo.h"
#include "printout.h"
#include "status.h"
#include "toplayer_utils.h"


#define mp_fmtargs(mp) mp_getnamelen(mp), mp_getname(mp), (mp)->id

#define DEFINE_STR() \
DEFSTR(MpTypeUndef, "UNDEF") \
DEFSTR(MpTypeOpt, "OPT") \
DEFSTR(MpTypeVi, "VI") \
DEFSTR(MpTypeCcflib, "CCFLIB")


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

const char* mptype_str(unsigned type)
{
   if (type >= MpTypeLast) return "ERROR unknown MP type";

   return mptypesnames.dummystr + mptypesnames_offsets[type];
}

static inline int mp_addvarchk(MathPrgm *mp, rhp_idx vi) {
   assert(mp->mdl->ctr.varmeta);
   int rc = OK;

   S_CHECK(vi_inbounds(vi, ctr_nvars_total(&mp->mdl->ctr), __func__));
   unsigned mp2_id = mp->mdl->ctr.varmeta[vi].mp_id;
   if (mp2_id != MpId_NA) {
      EmpDag *empdag = &mp->mdl->empinfo.empdag;
      if (mp2_id != mp->id) {
         error("[empdag] ERROR: trying to add variable '%s' (#%u) to MP '%s', "
               "but it already belongs to MP '%s'.\n Shared variable are not "
               "supported yet.\n", ctr_printvarname(&mp->mdl->ctr, vi), vi,
               empdag_getmpname(empdag, mp->id),
               empdag_getmpname(empdag, mp2_id));
      } else {
          error("[empdag] ERROR: variable '%s' (#%u) already belongs to MP '%s',"
                "but it was tried to be added again.\n",
                ctr_printvarname(&mp->mdl->ctr, vi), vi,
                empdag_getmpname(empdag, mp->id));
      }
      return Error_Inconsistency;
   }
   mp->mdl->ctr.varmeta[vi].mp_id = mp->id;
   MP_DEBUG("MP(%s) owns var '%s'\n", empdag_getmpname(&mp->mdl->empinfo.empdag, mp->id),
            ctr_printvarname(&mp->mdl->ctr, vi));

   rc = rhp_int_addsorted(&mp->vars, vi);
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

static inline int _setequrolechk(MathPrgm *mp, rhp_idx ei, EquRole role) {
   EquRole equrole = _getequrole(mp, ei);
   if (equrole != EquUndefined) {
      equmeta_rolemismatchmsg(&mp->mdl->ctr, ei, equrole, EquUndefined, __func__);
      return Error_UnExpectedData;
   }

   _setequrole(mp, ei, role);

   return OK;
}

static int err_noobj(MathPrgm *mp) {
   error("[empdag] ERROR: MP is of type %s (#%u) which does not have"
         " an objective function\n", mptype_str(mp->type), mp->type);
   if (MpTypeUndef == mp->type) {
      errormsg("\tMP has undefined type\n");
   }
   return Error_InvalidValue;
}

static int _chk_mpid(const Model *mdl, unsigned mp_id, const char * fn)
{
   if (mp_id == UINT_MAX) {
      error("%s :: invalid MP id!\n", fn);
      return Error_EMPRuntimeError;
   }
   if (mp_id >= mdl->empinfo.empdag.mps.len) {
      error("%s :: MP id %u >= %u the number of MPs\n", fn, mp_id,
            mdl->empinfo.empdag.mps.len);
      return Error_EMPRuntimeError;
   }

   return OK;
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
   assert(mp->mdl);

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
static bool isconstraint_opt(MathPrgm *mp, rhp_idx eidx) {
   if (eidx == mp->opt.objequ) {
      return false;
   }

   return true;
}

static bool isconstraint_vi(MathPrgm *mp, rhp_idx eidx) {
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

MathPrgm *mp_new(mpid_t id, RhpSense sense, Model *mdl) {
   MathPrgm *mp;
   AA_CHECK(mp, mp_alloc(mdl));

   if (id > MpId_MaxRegular) {
      error("[MP] ERROR: cannot create MP with ID %u, the max one is %u",
            id, MpId_MaxRegular);
      return NULL;
   }

   mp->id = id;
   mp->next_id = MpId_NA;
   mp->sense = sense;

   switch (sense) {
   case RhpFeasibility:
      mpvi_init(&mp->vi);
      mp->type = MpTypeVi;  break;
   case RhpMin: case RhpMax:
      mpopt_init(&mp->opt);
      mp->type = MpTypeOpt; break;
    default:
      mp->type = MpTypeUndef;
   }

   MP_DEBUG("Starting MP #%u\n", mp->id);

   return mp;
}

MathPrgm *mp_dup(const MathPrgm *mp_src,
                              Model *mdl) {
   MathPrgm *mp;
   AA_CHECK(mp, mp_new(mp_src->id, mp_src->sense, mdl));

   mp->type = mp_src->type;
   mp->probtype = mp_src->probtype;
   mp->status = mp_src->status;

   if (mp_isobj(mp)) {
      memcpy(&mp->opt, &mp_src->opt, sizeof(mp->opt));
      mp->opt.objvarval2objequval = false;
   } else {
      memcpy(&mp->vi, &mp_src->vi, sizeof(mp->vi));
   }

   SN_CHECK(rhp_idx_copy(&mp->equs, &mp_src->equs));
   SN_CHECK(rhp_idx_copy(&mp->vars, &mp_src->vars));

   return mp;
}

void mp_free(MathPrgm *mp) {
   if (!mp) {
      return;
   }

   if (mp->type == MpTypeCcflib) {
      ovf_def_free(mp->ccflib.ccf);
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
int mp_from_ccflib(MathPrgm *mp, unsigned ccflib_idx) {
   assert(mp->sense == RhpNoSense && mp->type == MpTypeUndef);

   A_CHECK(mp->ccflib.ccf, ovf_def_new(ccflib_idx));

   mp->sense = mp->ccflib.ccf->sense;
   mp_settype(mp, MpTypeCcflib);

   return OK;
}

int mp_settype(MathPrgm *mp, unsigned type) {
   if (type > MpTypeLast) {
      error( "%s :: MP type %u is above the limit %d\n", __func__,
            type, MpTypeLast);
      return Error_InvalidValue;
   }

   RhpSense sense = mp->sense;
   // Add MpTypeMcp? See GITLAB #83
   if (((sense == RHP_MAX || sense == RHP_MIN) && (type == MpTypeVi))
   || ((sense == RHP_FEAS) && (type != MpTypeVi))) {
         error("%s :: MP %.*s #%u: type '%s' incompatible with sense '%s'.\n",
               __func__, mp_getnamelen(mp), mp_getname(mp), mp->id,
               mptype_str(type), sense2str(mp->sense));
   }

   mp->type = type;

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

int mp_setobjequ(MathPrgm *mp, rhp_idx ei)
{
   S_CHECK(mp_setobjequ_internal(mp, ei));
   S_CHECK(mp_addequchk(mp, ei));

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
   if (mp_isobj(mp)) {
      mp->opt.objcoef = coef;
   } else {
      return err_noobj(mp);
   }

   return OK;
}

int mp_setobjequ_internal(MathPrgm *mp, rhp_idx objequ)
{
   assert(valid_ei(objequ));

   if (!mp_isobj(mp)) {
      return err_noobj(mp);
   }

   mp->opt.objequ = objequ;
   _setequrole(mp, objequ, EquObjective);

   rhp_idx objvar = mp->opt.objvar;
   if (valid_vi(objvar)) {
      _addvarbasictype(mp, objvar, VarIsExplicitlyDefined);
   }

   return OK;
}

/* TODO: merge the 2 functions? */
int mp_setobjvar(MathPrgm *mp, rhp_idx vi) {
   if (!valid_vi(vi)) {
      if (vi == IdxNA && mp_isobj(mp)) {
         mp->opt.objvar = IdxNA;
         return OK;
      }

      error("[mp] ERROR: cannot set objvar of MP to invalid index '%.*s' #%u",
            mp_getnamelen(mp), mp_getname(mp), mp->id);
      return Error_EMPRuntimeError;
   }

   unsigned mp_id = mp->mdl->ctr.varmeta[vi].mp_id;
   if (!valid_mpid(mp_id)) {
      S_CHECK(mp_addvarchk(mp, vi));
   } else if (mp_id != mp->id) {
      error("[MP] ERROR: Trying to set objective variable of MP(%.*s) (ID #%u) "
            "to '%s', but the latter belong to MP(%s)", mp_fmtargs(mp),
            mdl_printvarname(mp->mdl, vi), mpid_getname(mp->mdl, mp_id));
      return Error_EMPRuntimeError;
   }

   S_CHECK(mp_setobjvar_internal(mp, vi));

   return OK;
}

int mp_setobjvar_internal(MathPrgm *mp, rhp_idx vi)
{
   if (mp_isobj(mp)) {
      mp->opt.objvar = vi;

      /*  TODO(xhub) harder check: we should only allow a special value here */
      if (!valid_vi(vi)) {
         return OK;
      }

      _setvartype(mp, vi, VarObjective);
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


int mp_objvarval2objequval(MathPrgm *mp) {
   assert(mp_isobj(mp) && valid_vi(mp->opt.objvar) && valid_ei(mp->opt.objequ));

   mp->opt.objvarval2objequval = true;

   trace_process("[process] MP %u: adding objvar %s to objequ %s.\n", mp->id,
                 ctr_printvarname(&mp->mdl->ctr, mp->opt.objvar),
                 ctr_printequname(&mp->mdl->ctr, mp->opt.objequ));
   
   return OK;
}

int mp_fixobjequval(MathPrgm *mp) {

   if (!mp_isobj(mp) || !mp->opt.objvarval2objequval) { return OK; }

   rhp_idx objequ = mp->opt.objequ;
   rhp_idx objvar = mp->opt.objvar;

   if (!valid_vi(objvar)) { return OK; }

   assert(valid_ei(objequ) && valid_vi(objvar));

   double objvarval = mp->mdl->ctr.vars[objvar].value;
   trace_solreport("[solreport] MP(%s): setting objequ '%s' value to objvar '%s' "
                   "value %e\n", empdag_getmpname(&mp->mdl->empinfo.empdag, mp->id),
                   ctr_printequname(&mp->mdl->ctr, objequ),
                   ctr_printvarname(&mp->mdl->ctr, objvar), objvarval);

   mp->mdl->ctr.equs[objequ].value = objvarval;

   return OK;
}

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
         //               S_CHECK(rhp_int_addsorted(&mp->vars, dual));
         //            }
      }
   }

   /*  TODO(xhub) MPEC,MPCC,EPEC */
   if (mp->type == MpTypeVi) {
      mp->vi.num_cons += e->size;
   }

   return OK;
}

int mp_addvar(MathPrgm *mp, rhp_idx vidx) {
   S_CHECK(mp_addvarchk(mp, vidx));
   _setvartype(mp, vidx, VarPrimal);

   return OK;
}

int mp_addvars(MathPrgm *mp, Avar *v) {
   for (size_t i = 0; i < v->size; ++i) {
      rhp_idx vi = avar_fget(v, i);
      S_CHECK(mp_addvarchk(mp, vi));
      _setvartype(mp, vi, VarPrimal);
   }

   return OK;
}

/**
 *  @brief Add a vi pair (equ,var) to the MP
 *
 *  @ingroup publicAPI
 *
 *  @param  mp    the mathematical programm
 *  @param  ei    the equation index. If invalid, the equ is the zero function
 *  @param  vi    the variable index
 *
 *  @return       the error code
 */
int mp_addvipair(MathPrgm *mp, rhp_idx ei, rhp_idx vi) {
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

/**
 *  @brief Add a vi pair (equ var) to the MP
 *
 *  @ingroup publicAPI
 *
 *  @param  mp    the mathematical programm
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

   mp->status |= MpFinalized;

   return OK;
}

/**
 * @brief Finalize the MP creation
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

   if (mp->type == MpTypeCcflib) {
      return mp_ccflib_finalize(mp);
   }

   if (mp->vars.len == 0) {
      error("[MP] ERROR: MP(%s) has no variable assigned to it\n",
            empdag_getmpname(&mp->mdl->empinfo.empdag, mp->id));
      return Error_EMPRuntimeError;
   }

   if (mp->type == MpTypeUndef) {
      error("[MP] ERROR: MP(%s) (ID #%u) has no type set\n",
            empdag_getmpname(&mp->mdl->empinfo.empdag, mp->id), mp->id);
      return Error_EMPRuntimeError;
   }

   mp->status |= MpFinalized;

   if (!mp_isobj(mp)) { return OK; }

   rhp_idx objvar = mp->opt.objvar;
   if (!valid_ei(mp->opt.objequ)) {
      if (!valid_vi(objvar)) {
         mp_err_noobjdata(mp);
         return Error_IncompleteModelMetadata;
      }

      S_CHECK(mp_identify_objequ(mp));
   }

   rhp_idx objequ = mp->opt.objequ;

   if (valid_vi(objvar)) {
      return mdl_checkobjequvar(mp->mdl, objvar, objequ);
   }

   unsigned type, cone;
   S_CHECK(ctr_getequtype(&mp->mdl->ctr, objequ, &type, &cone));
   if (type != Mapping) {
      error("[MP] ERROR: In MP(%.*s) (ID #%u) objequ has type %s, should be %s. ",
            mp_fmtargs(mp), equtype_name(type), equtype_name(Mapping));
      errormsg("If there is an objective variable, it should be added to ");
      errormsg("the MP *before* the objective equation\n");
      return Error_EMPRuntimeError;
   }

   return OK;
}

/* ---------------------------------------------------------------------
 * GETTERS
 * --------------------------------------------------------------------- */

unsigned mp_getid(const MathPrgm *mp) { return mp->id; }

double mp_getobjjacval(const MathPrgm *mp) {
   if (mp_isobj(mp)) {
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

rhp_idx mp_getobjequ(const MathPrgm *mp) {
   if (mp_isobj(mp)) {
      return mp->opt.objequ;
   }

   if (mp->type == RHP_MP_CCFLIB) {
      return IdxCcflib;
   }

   return IdxInvalid;
}

rhp_idx mp_getobjvar(const MathPrgm *mp)
{
   if (mp_isobj(mp)) {
      return mp->opt.objvar;
   }

   return IdxInvalid;
}

void mp_err_noobjdata(const MathPrgm *mp)
{
   const EmpDag *empdag = &mp->mdl->empinfo.empdag;
   error("[FOOC] ERROR: the optimization MP(%s) has neither an objective "
         "variable or an objective function. This is unsupported, the MP must "
         "have have exactly one of these\n", empdag_getmpname(empdag, mp->id));
}


inline ModelType mp_getprobtype(const MathPrgm *mp) {
   return mp->probtype;
}

const char *mp_gettypestr(const MathPrgm *mp) {
   return mptype_str(mp->type);
}

unsigned mp_getnumzerofunc(const MathPrgm *mp) {
   if (mp->type == RHP_MP_VI) {
      return mp->vi.num_zeros;
   }

   return UINT_MAX;
}

unsigned mp_getnumvars(const MathPrgm *mp) {
   assert(mp->type != RHP_MP_VI || mp->vars.len - mp_getnumzerofunc(mp) == mp->equs.len - mp_getnumcons(mp));
   return mp->vars.len;
}

unsigned mp_getnumcons(const MathPrgm *mp) {
if (mp->type == RHP_MP_VI) {
      return mp->vi.num_cons;
   }

   return UINT_MAX;
}

void mp_print(MathPrgm *mp, const Model *mdl) {
   const Container *ctr = &mdl->ctr;

   printout(PO_INFO, " - mathprgm %5d", mp->id);

   if (mp_isobj(mp)) {
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

   } else if (mp->type == RHP_MP_VI) {
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

   if (mdl->backend != RHP_BACKEND_RHP) {
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
 * @brief Remove the variable from the MP
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
   S_CHECK(rhp_int_rmsorted(&mp->vars, vi));

   return OK;
}
