#ifndef EMPDAG_COMMON_H
#define EMPDAG_COMMON_H

#include <limits.h>
#include <math.h>

#include "allocators.h"
#include "block_array.h"
#include "checks.h"
#include "compat.h"
#include "empdag_data.h"
#include "equ.h"
#include "macros.h"
#include "printout.h"
#include "rhpidx.h"
#include "status.h"


/** Link type */
typedef enum {
   LinkArcVF,
   LinkArcCtrl,
   LinkArcNash,
   LinkVFObjFn,
   LinkObjAddMap,         /**< Not an EMPDAG arc, used to add maps to objfn */
   LinkObjAddMapSmoothed, /**< Not an EMPDAG arc, used to add maps to objfn */
   LinkViKkt,
   LinkTypeLast = LinkViKkt,
} LinkType;

/** arc VF weight of the form:   b y  */
typedef struct {
   rhp_idx ei;            /**< mapping where the term appears */
   rhp_idx vi;            /**< variable index   */
   double cst;            /**< constant part */
} ArcVFBasicData;


/** arc VF weight of the form: Σᵢ bᵢ yᵢ   */
typedef struct {
   rhp_idx   ei;          /**< mapping where the term appears */
   unsigned  len;         /**< Number of linear terms         */
   rhp_idx*  vis;         /**< Variable indices               */
   double *  vals;        /**<  Coefficients                  */
} ArcVFLequData;

/** General simple arc VF */
typedef struct {
   rhp_idx ei;            /**< mapping where the term appears */
   Equ *e;                /**< equation capturing the VF weight */
} ArcVFEquData;

/** arc VF when a child VF appears in different equations of a given MP VF */
typedef struct {
   unsigned len;
   unsigned max;
   ArcVFBasicData *list;
} ArcVFMultipleBasicData;


/** VF arc data */
typedef struct rhp_empdag_arcVF {
   ArcVFType type;       /**< arcVF type */
   mpid_t mpid_child;       /**< MP index */
   union {
      ArcVFBasicData basic_dat;
      ArcVFMultipleBasicData basics_dat;
      ArcVFEquData equ_dat;
      ArcVFLequData lequ_dat;
   };
} ArcVFData;

/** Generic EMPDAG link */
typedef struct empdag_link {
   LinkType type;
   union {
      ArcVFData Varc;
   };
} EmpDagLink;

const char* arcVFType2str(ArcVFType type);

/** Basic data structure to copy an expression in an equation */
typedef struct {
   Equ *equ;        /**< Equation where to inject the expression */
   union {
      ArcVFBasicData basic_dat;
      ArcVFMultipleBasicData basics_dat;
      ArcVFEquData equ_dat;
      ArcVFLequData lequ_dat;
   };
   NlNode **node;  /**< Node where to inject the data. */
} CopyExprData;


NONNULL static inline bool valid_linktype(LinkType type)
{
   return type <= LinkTypeLast;
}

NONNULL static inline bool valid_Varc(const ArcVFData *arc)
{
   return arc->type != ArcVFUnset;
}

NONNULL bool chk_Varc(const ArcVFData *arc, const Model *mdl);

/* ----------------------------------------------------------------------
 * Start of the functions for dealing with arc VFs. The general interface
 * arcVF_XXX then dispatches depending on the type of arcVF
 *
 * Conventions:
 *   - arcVFb_XXX is for arcVFBasic
 *   - arcVFl_XXX is for arcVFLequ
 *   - arcVFg_XXX is for arcVFEqu, the general form
 * ---------------------------------------------------------------------- */



/**
 * @brief Initialize a basic arcVF
 *
 * @param arcVF the arc
 * @param ei     the equation where the arcVf appears
 */
static inline void arcVFb_init(ArcVFData *arcVF, rhp_idx ei)
{
   arcVF->type = ArcVFBasic;
   arcVF->mpid_child = UINT_MAX;
   arcVF->basic_dat.cst = 1.;
   arcVF->basic_dat.vi = IdxNA;
   arcVF->basic_dat.ei = ei;
}

/**
 * @brief Initialize a basic arcVF
 *
 * @param arcVF the arc
 * @param ei     the equation where the arcVf appears
 */
static inline void arcVFb_init_cst(ArcVFData *arcVF, rhp_idx ei, double cst)
{
   arcVF->type = ArcVFBasic;
   arcVF->mpid_child = UINT_MAX;
   arcVF->basic_dat.cst = cst;
   arcVF->basic_dat.vi = IdxNA;
   arcVF->basic_dat.ei = ei;
}

static NONNULL int arcVFmb_alloc(M_ArenaLink *arena, ArcVFData *arcVF, unsigned nequs)
{
   arcVF->type = ArcVFMultipleBasic;
   arcVF->mpid_child = UINT_MAX;
   arcVF->basics_dat.len = nequs;
   arcVF->basics_dat.max = nequs;

   A_CHECK(arcVF->basics_dat.list, arenaL_alloc_array(arena, ArcVFBasicData, nequs));

   u64 sizes[] = {sizeof(rhp_idx)*nequs, sizeof(double)*nequs};
   void *mems[] = {arcVF->lequ_dat.vis, arcVF->lequ_dat.vals};

   return arenaL_alloc_blocks(arena, 2, mems, sizes);
}

/**
 * @brief Initialize a basic arcVF
 *
 * @param arena  the memory arena
 * @param arcVF  the arc
 * @param nequs  the number of equations on this arc
 * @param eis    the equation where the arcVf appears
 *
 * @return       the error code
 */
static inline
int arcVFmb_init(M_ArenaLink *arena, ArcVFData *arcVF, unsigned nequs,
                 const rhp_idx eis[VMT(static restrict nequs)])
{
   S_CHECK(arcVFmb_alloc(arena, arcVF, nequs));

   for (unsigned i = 0; i < nequs; ++i) {
      ArcVFBasicData *bdat = &arcVF->basics_dat.list[i];
      bdat->vi = IdxNA;
      bdat->cst = 1.;
      bdat->ei = eis[i];
   }

   return OK;
}

/**
 * @brief Initialize a basic multiple arcVF
 *
 * @param arena  the memory arena
 * @param arcVF  the arc
 * @param eis    the equation where the arcVf appears
 *
 * @return       the error code
 */
static inline
int arcVFmb_init_from_aequ(M_ArenaLink *arena, ArcVFData *arcVF, Aequ *eis, DblArrayBlock *csts)
{
   unsigned nequs = aequ_size(eis);

   if (nequs != csts->size) { 
      error("[arcVFmb] ERROR: number of equations and constants do not match: %u vs %u",
            nequs, csts->size);
      return Error_RuntimeError;
   }

   S_CHECK(arcVFmb_alloc(arena, arcVF, nequs));

   for (unsigned i = 0; i < nequs; ++i) {
      ArcVFBasicData *bdat = &arcVF->basics_dat.list[i];
      bdat->vi = IdxNA;
      bdat->cst = dblarrs_fget(csts, i);
      bdat->ei = aequ_fget(eis, i);
   }

   return OK;
}

NONNULL static inline void arcVFb_setvar(ArcVFData *arcVF, rhp_idx vi)
{
   arcVF->basic_dat.vi = vi;
}

NONNULL static inline void arcVFb_setequ(ArcVFData *arcVF, rhp_idx ei)
{
   arcVF->basic_dat.ei = ei;
}

NONNULL static inline void arcVFb_setcst(ArcVFData *arcVF, double cst)
{
   arcVF->basic_dat.cst = cst;
}

NONNULL static inline void arcVFb_setmp(ArcVFData *arcVF, unsigned mp_id)
{
   arcVF->mpid_child = mp_id;
}

NONNULL static inline rhp_idx arcVFb_getequ(const ArcVFData *arcVF)
{
   assert(arcVF->type == ArcVFBasic);
   return arcVF->basic_dat.ei;
}

static inline void arcVF_empty(ArcVFData *arcVF)
{
   arcVF->type = ArcVFUnset;
   arcVF->mpid_child = UINT_MAX;
   arcVF->basic_dat.cst = NAN;
   arcVF->basic_dat.vi = IdxInvalid;
   arcVF->basic_dat.ei = IdxInvalid;

}

NONNULL static inline
int arcVFb_copy(ArcVFData *dst, const ArcVFData *src)
{
   dst->type = ArcVFBasic;
   memcpy(&dst->basic_dat, &src->basic_dat, sizeof(ArcVFBasicData));

   return OK;
}

NONNULL static inline
int arcVF_copy(ArcVFData *dst, const ArcVFData *src)
{
   dst->mpid_child = src->mpid_child;

   switch (src->type) {
   case ArcVFBasic:
      return arcVFb_copy(dst, src);
   default:
      error("%s :: Unsupported arcVF type %u", __func__, src->type);
      return Error_NotImplemented;
   }
}

NONNULL static inline
int arcVFbdat_mul_arcVFbdat(ArcVFBasicData *arcVF1, const ArcVFBasicData *arcVF2)
{
   if (valid_vi(arcVF1->vi) && valid_vi(arcVF2->vi)) {
      TO_IMPLEMENT("arcVFb_mul_arcVFb yielding polynomial");
   }

   if (!valid_vi(arcVF1->vi)) {
      arcVF1->vi = arcVF2->vi;
   }

   arcVF1->cst *= arcVF2->cst;

   return OK;
}

NONNULL static inline
int arcVFb_mul_arcVF(ArcVFBasicData *arcVFbdat, const ArcVFData *arcVF2)
{
   assert(valid_Varc(arcVF2));

   switch (arcVF2->type) {
   case ArcVFBasic:
      return arcVFbdat_mul_arcVFbdat(arcVFbdat, &arcVF2->basic_dat);
   default:
      error("%s :: Unsupported arcVF type %u", __func__, arcVF2->type);
      return Error_NotImplemented;
   }
}

NONNULL static inline
int arcVFmb_mul_arcVF(ArcVFMultipleBasicData *arcVF1dat, const ArcVFData *arcVF2)
{
   assert(valid_Varc(arcVF2));

   switch (arcVF2->type) {
   case ArcVFBasic: {
      ArcVFBasicData *arcs = arcVF1dat->list;
      const ArcVFBasicData *arc2dat = &arcVF2->basic_dat;
      for (unsigned i = 0, len = arcVF1dat->len; i < len; ++i) {
         S_CHECK(arcVFbdat_mul_arcVFbdat(&arcs[i], arc2dat));
      }
      return OK;
   }
   default:
      error("%s :: Unsupported edgeVF type %u", __func__, arcVF2->type);
      return Error_NotImplemented;
   }
}


/**
 * @brief Multiply the data of two arcVF together.
 *
 * @param[in,out] arcVF1 the first arcVF
 * @param[in]     arcVF2 the second arcVF
 *
 * @return               the error code
 */
NONNULL static inline
int arcVF_mul_arcVF(ArcVFData *arcVF1, const ArcVFData *arcVF2)
{
   assert(valid_Varc(arcVF1) && valid_Varc(arcVF2));

   switch (arcVF1->type) {
   case ArcVFBasic:
      return arcVFb_mul_arcVF(&arcVF1->basic_dat, arcVF2);
   case ArcVFMultipleBasic:
      return arcVFmb_mul_arcVF(&arcVF1->basics_dat, arcVF2);
   default:
      error("%s :: Unsupported arcVF type %u", __func__, arcVF1->type);
      return Error_NotImplemented;
   }
}

/**
 * @brief Multiply a basic arc VF by a linear equation
 *
 * This transform the arcVFBasic into an arcVFLequ
 *
 * @param arcVF the VF arc
 * @param len    the number of linear terms
 * @param idxs   the variable indices
 * @param vals   the values
 *
 * @return       the error code
 */
NONNULL static inline
int arcVFb_mul_lequ(ArcVFData *arcVF, unsigned len, rhp_idx *idxs, double * restrict vals)
{
   if (valid_vi(arcVF->basic_dat.vi)) {
      error("%s :: polynomial arcVF are not yet supported", __func__);
      return Error_NotImplemented;
   }

   if (len == 1) {
      arcVF->basic_dat.cst *= vals[0];
      arcVF->basic_dat.vi = idxs[0];
      return OK;
   }

   double cst = arcVF->basic_dat.cst;
   rhp_idx ei = arcVF->basic_dat.ei;

   arcVF->type = ArcVFLequ;
   arcVF->lequ_dat.ei = ei;
   arcVF->lequ_dat.len = len;
   MALLOC_(arcVF->lequ_dat.vis, rhp_idx, len);
   MALLOC_(arcVF->lequ_dat.vals, double, len);
   memcpy(arcVF->lequ_dat.vis, idxs, len*sizeof(unsigned));

   if (cst == 1.) {
      memcpy(arcVF->lequ_dat.vals, vals, len*sizeof(double));
   } else {
      double *dst = arcVF->lequ_dat.vals;
      for (unsigned i = 0; i < len; ++i) {
         dst[i] = cst*vals[i];
      }
   }

   return OK;
}

/**
 * @brief Multiply an arc VF by a linear equation
 *
 * @param arcVF  the VF arc
 * @param len    the number of linear terms
 * @param idxs   the variable indices
 * @param vals   the values
 *
 * @return       the error code
 */
NONNULL static inline
int arcVF_mul_lequ(ArcVFData *arcVF, unsigned len, rhp_idx *idxs, double *vals)
{
   assert(valid_Varc(arcVF));

   switch (arcVF->type) {
   case ArcVFBasic:
      return arcVFb_mul_lequ(arcVF, len, idxs, vals);
   default:
      error("%s :: Unsupported arcVF type %u", __func__, arcVF->type);
      return Error_NotImplemented;
   }
}

const char *linktype2str(LinkType type);

unsigned arcVFb_getnumcons(const ArcVFBasicData *arc, const Model *mdl);

NONNULL static inline
unsigned arcVF_getnumcons(const ArcVFData *arc, const Model *mdl)
{
   assert(valid_Varc(arc));

   switch (arc->type) {
   case ArcVFBasic:
      return arcVFb_getnumcons(&arc->basic_dat, mdl);
   case ArcVFMultipleBasic: {
      unsigned res = 0;
      const ArcVFMultipleBasicData *dat = &arc->basics_dat;

      for (unsigned i = 0, len = dat->len; i < len; ++i) {
         res += arcVFb_getnumcons(&dat->list[i], mdl);
      }
      return res;
   }
   default:
      error("%s :: Unsupported arcVF type %u", __func__, arc->type);
      return UINT_MAX;
   }

}

bool arcVFb_in_objfunc(const ArcVFBasicData *arc, const Model *mdl);

NONNULL static inline
bool arcVF_in_objfunc(const ArcVFData *arc, const Model *mdl)
{
   assert(valid_Varc(arc));

   switch (arc->type) {
   case ArcVFBasic:
      return arcVFb_in_objfunc(&arc->basic_dat, mdl);
   case ArcVFMultipleBasic: {
      const ArcVFMultipleBasicData *dat = &arc->basics_dat;
      for (unsigned i = 0, len = dat->len; i < len; ++i) {
         if (arcVFb_in_objfunc(&dat->list[i], mdl)) return true;
      }
      return false;
   }
   default:
      error("%s :: Unsupported arcVF type %u", __func__, arc->type);
      assert(0);
      return false;
   }
}

int arcVFb_subei(ArcVFData *arc, rhp_idx ei_old, rhp_idx ei_new);

/**
 * @brief Subsitute ei in an arcVF
 *
 * @param  arc     the EMPDAG arc
 * @param  ei_old  the old equation index
 * @param  ei_new  the new equation index
 *
 * @return         the error code
 */
NONNULL static inline
int arcVF_subei(ArcVFData *arc, rhp_idx ei_old, rhp_idx ei_new)
{
   assert(valid_Varc(arc));

   switch (arc->type) {
   case ArcVFBasic:
      return arcVFb_subei(arc, ei_old, ei_new);
   default:
      error("%s :: Unsupported arcVF type %u", __func__, arc->type);
      return Error_NotImplemented;
   }
}

bool arcVFb_has_abstract_objfunc(const ArcVFData *arc);

NONNULL static inline
bool arcVF_has_abstract_objfunc(const ArcVFData *arc)
{
   assert(valid_Varc(arc));

   switch (arc->type) {
   case ArcVFBasic:
      return arcVFb_has_abstract_objfunc(arc);
   default:
      error("%s :: Unsupported arcVF type %u", __func__, arc->type);
      assert(0);
      return false;
   }
}

NONNULL bool arcVFb_chk_equ(const ArcVFBasicData *arc, mpid_t mpid, const Container *ctr);

NONNULL static inline
bool arcVF_chk_equ(const ArcVFData *arc, mpid_t mpid, const Container *ctr)
{
   assert(valid_Varc(arc));

   switch (arc->type) {
   case ArcVFBasic:
      return arcVFb_chk_equ(&arc->basic_dat, mpid, ctr);
   case ArcVFMultipleBasic: {
      const ArcVFMultipleBasicData *dat = &arc->basics_dat;
      for (unsigned i = 0, len = dat->len; i < len; ++i) {
         if (!arcVFb_chk_equ(&dat->list[i], mpid, ctr)) { return false; }
      }
      return true;
      }
   default:
      error("%s :: Unsupported arcVF type %u", __func__, arc->type);
      return false;
   }
}


#endif /* EMPDAG_COMMON_H */
