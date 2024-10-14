#ifndef EMPDAG_COMMON_H
#define EMPDAG_COMMON_H

#include <limits.h>
#include <math.h>

#include "checks.h"
#include "compat.h"
#include "empdag_data.h"
#include "equ.h"
#include "macros.h"
#include "printout.h"
#include "rhpidx.h"
#include "status.h"


/** Stage of the EMPDAG */
typedef enum {
   EmpDag_Model,
   EmpDag_Transformed,
   EmpDag_Collapsed,
   EmpDag_Subset,
} EmpDagStage;

/** Type of arc */
typedef enum {
   LinkArcVF,
   LinkArcCtrl,
   LinkArcNash,
   LinkVFObjFn,
   LinkObjAddMap, /**< Not a true arc, used to add maps to objfn */
   LinkViKkt,
   LinkTypeLast = LinkViKkt,
} LinkType;

/** Type of VF arc  */
typedef enum {
   ArcVFUnset,          /**< Unset arc VF type  */
   ArcVFBasic,          /**< child VF appears in one equation with basic weight */
   ArcVFMultipleBasic,  /**< child VF appears in equations with basic weight */
   ArcVFLequ,           /**< child VF appears in one equation with linear sum weight */
   ArcVFMultipleLequ,   /**< child VF appears in equations with linear sum weight */
   ArcVFEqu,            /**< child VF appears in one equation with general weight */
   ArcVFMultipleEqu,    /**< child VF appears in equations with general weight */
   ArcVFLast = ArcVFMultipleEqu,
} ArcVFType;

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

/** Generic EMPDAG arc */
typedef struct empdag_arc {
   LinkType type;
   union {
      ArcVFData Varc;
   };
} EmpDagArc;

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

NONNULL static inline bool valid_arcVF(const ArcVFData *arc)
{
   return arc->type != ArcVFUnset;
}

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
int arcVFb_mul_arcVFb(ArcVFData *arcVF1, const ArcVFData *arcVF2)
{
   if (valid_vi(arcVF1->basic_dat.vi) && valid_vi(arcVF2->basic_dat.vi)) {
      TO_IMPLEMENT("arcVFb_mul_arcVFb yielding polynomial");
   }

   if (!valid_vi(arcVF1->basic_dat.vi)) {
      arcVF1->basic_dat.vi = arcVF2->basic_dat.vi;
   }

   arcVF1->basic_dat.cst *= arcVF2->basic_dat.cst;

   return OK;
}


NONNULL static inline
int arcVFb_mul_arcVF(ArcVFData *edgeVF1, const ArcVFData *edgeVF2)
{
   assert(valid_arcVF(edgeVF1) && valid_arcVF(edgeVF2));

   switch (edgeVF2->type) {
   case ArcVFBasic:
      return arcVFb_mul_arcVFb(edgeVF1, edgeVF2);
   default:
      error("%s :: Unsupported edgeVF type %u", __func__, edgeVF2->type);
      return Error_NotImplemented;
   }
}


NONNULL static inline
int arcVF_mul_arcVF(ArcVFData *arcVF1, const ArcVFData *arcVF2)
{
   assert(valid_arcVF(arcVF1) && valid_arcVF(arcVF2));

   switch (arcVF1->type) {
   case ArcVFBasic:
      return arcVFb_mul_arcVF(arcVF1, arcVF2);
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
   assert(valid_arcVF(arcVF));

   switch (arcVF->type) {
   case ArcVFBasic:
      return arcVFb_mul_lequ(arcVF, len, idxs, vals);
   default:
      error("%s :: Unsupported arcVF type %u", __func__, arcVF->type);
      return Error_NotImplemented;
   }
}

const char *linktype2str(LinkType type);

unsigned arcVFb_getnumcons(ArcVFData *arc, const Model *mdl);

NONNULL static inline
unsigned arcVF_getnumcons(ArcVFData *arc, const Model *mdl)
{
   assert(valid_arcVF(arc));

   switch (arc->type) {
   case ArcVFBasic:
      return arcVFb_getnumcons(arc, mdl);
   case ArcVFMultipleBasic: {
      unsigned res = 0;

      for (unsigned i = 0, len = arc->basics_dat.len; i < len; ++i) {
         res += arcVFb_getnumcons(arc, mdl);
      }
      return res;
   }
   default:
      error("%s :: Unsupported arcVF type %u", __func__, arc->type);
      return UINT_MAX;
   }

}

bool arcVFb_in_objfunc(const ArcVFData *arc, const Model *mdl);

NONNULL static inline
bool arcVF_in_objfunc(const ArcVFData *arc, const Model *mdl)
{
   assert(valid_arcVF(arc));

   switch (arc->type) {
   case ArcVFBasic:
      return arcVFb_in_objfunc(arc, mdl);
   default:
      error("%s :: Unsupported arcVF type %u", __func__, arc->type);
      return Error_NotImplemented;
   }
}

int arcVFb_subei(ArcVFData *arc, rhp_idx ei_old, rhp_idx ei_new);

NONNULL static inline
int arcVF_subei(ArcVFData *arc, rhp_idx ei_old, rhp_idx ei_new)
{
   assert(valid_arcVF(arc));

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
   assert(valid_arcVF(arc));

   switch (arc->type) {
   case ArcVFBasic:
      return arcVFb_has_abstract_objfunc(arc);
   default:
      error("%s :: Unsupported arcVF type %u", __func__, arc->type);
      return Error_NotImplemented;
   }
}
#endif /* EMPDAG_COMMON_H */
