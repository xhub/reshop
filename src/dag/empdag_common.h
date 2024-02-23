#ifndef EMPDAG_COMMON_H
#define EMPDAG_COMMON_H

#include <limits.h>
#include <math.h>
#include <stddef.h>

#include "compat.h"
#include "empdag_data.h"
#include "empinterp_data.h"
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

/** Type of VF edge  */
typedef enum {
   EdgeVFUnset,          /**< Unset edge VF type  */
   EdgeVFBasic,          /**< child VF appears in one equation with basic weight */
   EdgeVFMultipleBasic,  /**< child VF appears in equations with basic weight */
   EdgeVFLequ,           /**< child VF appears in one equation with linear sum weight */
   EdgeVFMultipleLequ,   /**< child VF appears in equations with linear sum weight */
   EdgeVFEqu,            /**< child VF appears in one equation with general weight */
   EdgeVFMultipleEqu,    /**< child VF appears in equations with general weight */
   EdgeVFLast = EdgeVFMultipleEqu,
} EdgeVFType;

/** edge VF weight of the form:   b y  */
typedef struct EdgeVFBasicData {
   rhp_idx ei;            /**< mapping where the term appears */
   rhp_idx vi;            /**< variable index   */
   double cst;            /**< constant part */
} EdgeVFBasicData;


/** edge VF weight of the form: Σᵢ bᵢ yᵢ   */
typedef struct EdgeVFLequData {
   rhp_idx   ei;          /**< mapping where the term appears */
   unsigned  len;         /**< Number of linear terms         */
   rhp_idx*  vis;         /**< Variable indices               */
   double *  vals;        /**<  Coefficients                  */
} EdgeVFLequData;

/** General simple edge VF */
typedef struct EdgeVFEquData {
   rhp_idx ei;            /**< mapping where the term appears */
   Equ *e;                /**< equation capturing the VF weight */
} EdgeVFEquData;

/** edge VF when a child VF appears in different equations of a given MP VF */
typedef struct EdgeVFMultipleBasicData {
   unsigned len;
   unsigned max;
   EdgeVFBasicData *list;
} EdgeVFMultipleBasicData;



/** VF edge data */
typedef struct rhp_empdag_Varc {
   EdgeVFType type;       /**< edgeVF type */
   mpid_t child_id;       /**< MP index */
   union {
      EdgeVFBasicData basic_dat;
      EdgeVFMultipleBasicData basics_dat;
      EdgeVFEquData equ_dat;
      EdgeVFLequData lequ_dat;
   };
} EdgeVF;

/** Generic EMPDAG edge */
typedef struct empdag_edge {
   EdgeType type;
   union {
      struct rhp_empdag_Varc edgeVF;
   };
} EmpDagEdge;

NONNULL static inline bool valid_edgeVF(const EdgeVF *edge)
{
   return edge->type != EdgeVFUnset;
}

/* ----------------------------------------------------------------------
 * Start of the functions for dealing with edge VFs. The general interface
 * edgeVF_XXX then dispatches depending on the type of edgeVF
 *
 * Conventions:
 *   - edgeVFb_XXX is for EdgeVFBasic
 *   - edgeVFl_XXX is for EdgeVFLequ
 *   - edgeVFg_XXX is for EdgeVFEqu, the general form
 * ---------------------------------------------------------------------- */



/**
 * @brief Initialize a basic edgeVF
 *
 * @param edgeVF the edge
 * @param ei     the equation where the edgeVf appears
 */
static inline void edgeVFb_init(struct rhp_empdag_Varc *edgeVF, rhp_idx ei)
{
   edgeVF->type = EdgeVFBasic;
   edgeVF->child_id = UINT_MAX;
   edgeVF->basic_dat.cst = 1.;
   edgeVF->basic_dat.vi = IdxNA;
   edgeVF->basic_dat.ei = ei;

}

NONNULL static inline void edgeVFb_setvar(struct rhp_empdag_Varc *edge, rhp_idx vi)
{
   edge->basic_dat.vi = vi;
}

NONNULL static inline void edgeVFb_setequ(struct rhp_empdag_Varc *edge, rhp_idx ei)
{
   edge->basic_dat.ei = ei;
}

NONNULL static inline void edgeVFb_setcst(struct rhp_empdag_Varc *edge, double cst)
{
   edge->basic_dat.cst = cst;
}

NONNULL static inline void edgeVFb_setmp(struct rhp_empdag_Varc *edge, unsigned mp_id)
{
   edge->child_id = mp_id;
}

NONNULL static inline rhp_idx edgeVFb_getequ(const EdgeVF *edge)
{
   assert(edge->type == EdgeVFBasic);
   return edge->basic_dat.ei;
}

static inline void edgeVF_empty(struct rhp_empdag_Varc *edge)
{
   edge->type = EdgeVFUnset;
   edge->child_id = UINT_MAX;
   edge->basic_dat.cst = NAN;
   edge->basic_dat.vi = IdxInvalid;
   edge->basic_dat.ei = IdxInvalid;

}

NONNULL static inline
int edgeVFb_copy(EdgeVF *dst, const EdgeVF *src)
{
   dst->type = EdgeVFBasic;
   memcpy(&dst->basic_dat, &src->basic_dat, sizeof(EdgeVFBasicData));

   return OK;
}

NONNULL static inline
int edgeVF_copy(EdgeVF *dst, const EdgeVF *src)
{
   dst->child_id = src->child_id;

   switch (src->type) {
   case EdgeVFBasic:
      return edgeVFb_copy(dst, src);
   default:
      error("%s :: Unsupported edgeVF type %u", __func__, src->type);
      return Error_NotImplemented;
   }
}

NONNULL static inline
int edgeVFb_mul_edgeVFb(EdgeVF *edgeVF1, const EdgeVF *edgeVF2)
{
   if (valid_vi(edgeVF1->basic_dat.vi) && valid_vi(edgeVF2->basic_dat.vi)) {
      TO_IMPLEMENT("edgeVFb_mul_edgeVFb yielding polynomial");
   }

   if (!valid_vi(edgeVF1->basic_dat.vi)) {
      edgeVF1->basic_dat.vi = edgeVF2->basic_dat.vi;
   }

   edgeVF1->basic_dat.cst *= edgeVF2->basic_dat.cst;

   return OK;
}


NONNULL static inline
int edgeVFb_mul_edgeVF(EdgeVF *edgeVF1, const EdgeVF *edgeVF2)
{
   assert(valid_edgeVF(edgeVF1) && valid_edgeVF(edgeVF2));

   switch (edgeVF2->type) {
   case EdgeVFBasic:
      return edgeVFb_mul_edgeVFb(edgeVF1, edgeVF2);
   default:
      error("%s :: Unsupported edgeVF type %u", __func__, edgeVF2->type);
      return Error_NotImplemented;
   }
}


NONNULL static inline
int edgeVF_mul_edgeVF(EdgeVF *edgeVF1, const EdgeVF *edgeVF2)
{
   assert(valid_edgeVF(edgeVF1) && valid_edgeVF(edgeVF2));

   switch (edgeVF1->type) {
   case EdgeVFBasic:
      return edgeVFb_mul_edgeVF(edgeVF1, edgeVF2);
   default:
      error("%s :: Unsupported edgeVF type %u", __func__, edgeVF1->type);
      return Error_NotImplemented;
   }
}

/**
 * @brief Multiply a basic edge VF by a linear equation
 *
 * This transform the edgeVFBasic into an edgeVFLequ
 *
 * @param edgeVF the VF edge
 * @param len    the number of linear terms
 * @param idxs   the variable indices
 * @param vals   the values
 *
 * @return       the error code
 */
NONNULL static inline
int edgeVFb_mul_lequ(EdgeVF *edgeVF, unsigned len, unsigned *idxs, double * restrict vals)
{
   if (valid_vi(edgeVF->basic_dat.vi)) {
      error("%s :: polynomial edgeVF are not yet supported", __func__);
      return Error_NotImplemented;
   }

   if (len == 1) {
      edgeVF->basic_dat.cst *= vals[0];
      edgeVF->basic_dat.vi = idxs[0];
      return OK;
   }

   double cst = edgeVF->basic_dat.cst;
   rhp_idx ei = edgeVF->basic_dat.ei;

   edgeVF->type = EdgeVFLequ;
   edgeVF->lequ_dat.ei = ei;
   edgeVF->lequ_dat.len = len;
   MALLOC_(edgeVF->lequ_dat.vis, rhp_idx, len);
   MALLOC_(edgeVF->lequ_dat.vals, double, len);
   memcpy(edgeVF->lequ_dat.vis, idxs, len*sizeof(unsigned));

   if (cst == 1.) {
      memcpy(edgeVF->lequ_dat.vals, vals, len*sizeof(double));
   } else {
      double *dst = edgeVF->lequ_dat.vals;
      for (unsigned i = 0; i < len; ++i) {
         dst[i] = cst*vals[i];
      }
   }

   return OK;
}

/**
 * @brief Multiply an edge VF by a linear equation
 *
 * @param edgeVF the VF edge
 * @param len    the number of linear terms
 * @param idxs   the variable indices
 * @param vals   the values
 *
 * @return       the error code
 */
NONNULL static inline
int edgeVF_mul_lequ(EdgeVF *edgeVF, unsigned len, unsigned *idxs, double *vals)
{
   assert(valid_edgeVF(edgeVF));

   switch (edgeVF->type) {
   case EdgeVFBasic:
      return edgeVFb_mul_lequ(edgeVF, len, idxs, vals);
   default:
      error("%s :: Unsupported edgeVF type %u", __func__, edgeVF->type);
      return Error_NotImplemented;
   }
}



#endif /* EMPDAG_COMMON_H */
