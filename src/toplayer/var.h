#ifndef VAR_H
#define VAR_H

#ifndef _ISOC99_SOURCE
#define _ISOC99_SOURCE      /*  See feature_test_macros(7) */
#endif

#include "asnan.h"

#include <assert.h>
#include <float.h>
#include <math.h>
#include <stdbool.h>

#include "cones.h"
#include "equvar_data.h"
#include "rhp_fwd.h"

/** 
 *  @file var.h
 *
 *  @brief variable representation
 */

/** types of variables */
enum var_type {
   VAR_X          = 0,        /**< continuous variable            */
   VAR_B          = 1,        /**< binary variable                */
   VAR_I          = 2,        /**< integer variable               */
   VAR_SOS1       = 3,        /**< special order set 1 (SOS1)     */
   VAR_SOS2       = 4,        /**< special order set 2 (SOS2)     */
   VAR_SC         = 5,        /**< semi-continuous variable       */
   VAR_SI         = 6,        /**< semi-integer variable          */
   VAR_IND        = 7,        /**< indicator variable             */
   VAR_POLYHEDRAL = 8,        /**< variable in a polyhedral cone  */
   VAR_SOC        = 9,        /**< variable in a SOC              */
   VAR_RSOC       = 10,       /**< variable in a rotated SOC      */
   VAR_EXP        = 11,       /**< variable in a EXP cone         */
   VAR_DEXP       = 12,       /**< variable in a dual EXP         */
   VAR_POWER      = 13,       /**< variable in a POWER cone       */
   VAR_DPOWER     = 14,       /**< variable in a dual POWER cone  */
   VAR_TYPE_LEN
};

/**
 * @brief Variable bounds
 */
struct var_bnd {
   double lb;                 /**< lower bound                        */
   double ub;                 /**< upper bound                        */
};

/**
 * @brief cone support for variable
 */
struct var_cone {
   enum cone type;           /**< cone type                          */
   void* data;                /**< cone data                          */
};

struct var_semiint {
   unsigned lb;
   unsigned ub;
};

/** @brief scalar variable definition */
typedef struct var {
   rhp_idx idx;               /**< index (0-based)                       */
   BasisStatus basis;         /**< Basis status                           */
   enum var_type type;        /**< type of variable                       */
   bool is_conic;             /**< true if the variable is in a conic constraint */
   bool is_deleted;           /**< true if the variable has been deleted  */
   double value;              /**< value                                  */
   double multiplier;         /**< box multiplier value                   */
   union {
      struct var_bnd bnd;     /**< lower and upper bounds (if applicable) */
      struct var_cone cone;   /**< Cone information (if applicable)       */
      struct var_semiint si;  /**< Semiinteger information (if applicable)*/
   };
} Var;

 /**
  *  @brief container for variable
  */
typedef struct rhp_avar {
   enum a_equvar_type type;       /**< type of abstract variable */
   bool own;            /**< Does it own its data?                      */
   unsigned size;       /**< size (or length) of the variable*/
   union {
      rhp_idx start;   /**< start of the indices*/
      rhp_idx *list;        /**< list of indices*/
      rhp_idx *slist;        /**< list of indices*/
      struct avar_block* blocks;  /**< block of variables */
   };
} Avar;

/**
 * @brief Block of abstract variable
 */
typedef struct avar_block {
   unsigned len;     /**< number of avar */
   unsigned max;     /**< maximum number of avar */
   Avar v[];  /**< list of avar */
} AvarBlock;

/*
 * * @brief generator for programmatically defining variables
 */
struct var_genops {
   enum var_type (*set_type)(const void* env, unsigned indx);
   double (*get_lb)(const void* env, unsigned indx);
   double (*get_ub)(const void* env, unsigned indx);
   double (*set_level)(const void* env, unsigned indx);
   double (*set_marginal)(const void* env, unsigned indx);
};


void avar_free(Avar* v);
Avar* avar_new(void) MALLOC_ATTR(avar_free,1);
Avar* avar_newcompact(unsigned size, unsigned start) MALLOC_ATTR(avar_free,1);
Avar* avar_newlist(unsigned size, rhp_idx *list) NONNULL MALLOC_ATTR(avar_free,1);
Avar* avar_newlistcopy(unsigned size, rhp_idx *vis) NONNULL MALLOC_ATTR(avar_free,1);
Avar* avar_newblock(unsigned block_size) MALLOC_ATTR(avar_free,1);

int avar_get(const Avar *v, unsigned i, rhp_idx *vidx) NONNULL;
int avar_set_list(Avar *v, unsigned size, rhp_idx *indx);
unsigned avar_size(const Avar *v);

void avar_init(Avar* v) NONNULL;
int avar_copy(Avar * restrict dst, const Avar * restrict src) NONNULL;
void avar_empty(Avar *v) NONNULL;
void avar_reset(Avar *v) NONNULL;
int avar_extend(Avar *v, const Avar *w) NONNULL;
unsigned avar_findidx(const Avar* v, rhp_idx vidx) NONNULL;
int avar_subset(Avar* v, rhp_idx *idx, unsigned len, Avar *v_subset) NONNULL;
rhp_idx avar_block_get(const struct avar_block *b, unsigned i);
bool avar_block_contains(const struct avar_block *b, rhp_idx vi);
int avar_setblock(Avar *v, unsigned block_size) NONNULL;
void avar_printnames(const Avar *v, unsigned mode, const Model *mdl) NONNULL;

int var_compar(const void *a, const void *b);
void var_print(const struct var *var);
int var_set_bounds(struct var *v, double lb, double ub);
void var_update_bnd(struct var *var, enum var_type type);


/**
 * @brief Get the variable index at position i
 *
 * @param v  the abstract variable
 * @param i  the element to get
 *
 * @return   the equation index at position i
 */
static inline NONNULL rhp_idx avar_fget(const Avar *v, unsigned i)
{
   assert(v->size > 0);
   switch (v->type) {
   case EquVar_Compact:
      return v->start + (rhp_idx)i;
   case EquVar_SortedList:
      return v->slist[i];
   case EquVar_List:
      return v->list[i];
   case EquVar_Block:
      return avar_block_get(v->blocks, i);
    default:
      assert(0);
    ;
   }

   return IdxError;
}

/**
 * @brief Check if a variable index is contained in the abstract variable
 *
 * @param v   the variable
 * @param vi  variable index
 *
 * @return    true if the variable is contained in it, false otherwise
 */
static NONNULL inline bool avar_contains(const Avar *v, rhp_idx vi)
{
   unsigned size = v->size;
   if (size == 0 || !valid_vi(vi)) {
      return false;
   }

   switch (v->type) {
   case EquVar_Compact:
      return (vi >= v->start) && (vi < (int)(v->start + size));
   case EquVar_List:
      for (size_t i = 0; i < size ; ++i) {
         if (v->list[i] == vi) { return true; }
      }
      return false;
   case EquVar_SortedList:
    {
      size_t lb = 0, ub = v->size-1, mid = (v->size-1)/2;
      while(true) {
        if (lb > ub) {
            return false;
        }
        rhp_idx val = v->slist[mid];
        if (val > vi) {
          assert(mid >= 1);
          ub = mid - 1;
        } else if (val < vi) {
          lb = mid + 1;
        } else {
          return true;
        }
        /* Do not overflow */
        mid = lb + (ub - lb)/2;
      }
    }
   case EquVar_Block:
      return avar_block_contains(v->blocks, vi);
   default:
      assert(0);
    ;
   }

   return false;
}

static inline NONNULL void avar_setcompact(Avar *v, unsigned size, rhp_idx start)
{
   v->type = EquVar_Compact;
   v->size = size;
   v->start = start;
}

static inline NONNULL void avar_setlist(Avar *v, unsigned size, rhp_idx *list)
{
   v->type = EquVar_List;
   v->own = false;
   v->size = size;
   v->list = list;
}

OWNERSHIP_TAKES(3)
static inline NONNULL void avar_setandownlist(Avar *v, unsigned size, rhp_idx *list)
{
   v->own = true;
   v->type = EquVar_List;
   v->size = size;
   v->list = list;
}

OWNERSHIP_TAKES(3)
static inline NONNULL void avar_setandownsortedlist(Avar *v, unsigned size, rhp_idx *list)
{
   v->own = true;
   v->type = EquVar_SortedList;
   v->size = size;
   v->list = list;
}

static inline NONNULL struct avar_block* avar_getblock(Avar *v)
{
   return v->blocks;
}

static inline NONNULL void avar_setsize(Avar *v, unsigned size)
{
   assert(v);
   v->size = size;
}

static inline void var_init(struct var *v, rhp_idx idx, enum var_type type)
{
   v->idx = idx;
   v->basis = BasisUnset;
   v->type = type;
   v->is_conic = false;
   v->is_deleted = false;
   v->value = SNAN_UNINIT;
   v->multiplier = SNAN_UNINIT;
}


static inline bool var_validtype(unsigned type) {
   if (type < VAR_TYPE_LEN) return true;
   
   return false;
}

static inline bool var_isfixed(struct var *v)
{
   switch (v->type) {
   case VAR_X:
   case VAR_B:
   case VAR_I:
   case VAR_SOS1:
   case VAR_SOS2:
   case VAR_SC:
   case VAR_SI:
      return (fabs(v->bnd.ub - v->bnd.lb) < DBL_EPSILON) ? true : false;
   default:
      return false;
   }
}

static inline bool var_bndinvalid(struct var *v)
{
   switch (v->type) {
   case VAR_X:
   case VAR_B:
   case VAR_I:
   case VAR_SOS1:
   case VAR_SOS2:
   case VAR_SC:
   case VAR_SI:
      return ((v->bnd.ub - v->bnd.lb) < -DBL_EPSILON) ? true : false;
   default:
      return false;
   }
}
#endif /* VAR_H */
