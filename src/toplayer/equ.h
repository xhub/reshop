#ifndef EQU_H
#define EQU_H

/** @file equ.h
 *
 * @brief equation representation
 */

#include <assert.h>
#include <stdbool.h>

#include "rhp_fwd.h"

#include "asnan_pub.h"
#include "cones.h"
#include "equvar_data.h"
#include "equ_data.h"
#include "rhpidx.h"
#include "status.h"

struct aequ_block;

/** container for equation */
typedef struct rhp_aequ {
   AbstractEquVarType type;       /**< type of abstract equation */
   bool own;                      /**< Own the memory by the pointers*/
   unsigned size;                 /**< size (or length) of the equation*/
   union {
      rhp_idx start;              /**< start of the indices*/
      rhp_idx *list;              /**< list of indices*/
      rhp_idx *slist;             /**< sorted list of indices*/
      struct aequ_block* blocks;  /**< sub equations */
   };
} Aequ;

/** abstract equation as block */
typedef struct aequ_block {
   unsigned len;                  /**< number of blocks            */
   unsigned max;                  /**< maximum number of blocks    */
   Aequ e[] __counted_by(max);    /**< Number of abstract equation */
} AequBlock;

/** @brief Representation of an equation */
typedef struct equ {
   rhp_idx idx;              /**< index, above IdxMaxValid are special values */
   BasisStatus basis;        /**< basis status                                */
   EquObjectType object;     /**< type of equation                            */
   Cone cone;                /**< cone for cone inclusion                     */
   bool is_quad;             /**< equation is quadratic: useful when using NL
                                  path for storing quadratic                  */
   union {
      double cst;            /**< constant in the mapping                     */
      void *cone_data;       /**< additional cone information                 */
   } p;

   double value;             /**< value                                       */
   double multiplier;        /**< multiplier value                            */

   Lequ *lequ;               /**< linear terms                                */
   NlTree *tree;             /**< expression tree for the nonlinear terms     */
} Equ;

void aequ_free(Aequ* e);
Aequ* aequ_new(void)  MALLOC_ATTR(aequ_free,1);
Aequ* aequ_newcompact(unsigned size, rhp_idx start) MALLOC_ATTR(aequ_free,1);
Aequ* aequ_newlistborrow(unsigned size, rhp_idx *eis) NONNULL MALLOC_ATTR(aequ_free,1);
Aequ* aequ_newlistcopy(unsigned size, rhp_idx *eis) NONNULL MALLOC_ATTR(aequ_free,1);
Aequ* aequ_newblock(unsigned num_blocks) MALLOC_ATTR(aequ_free,1);

Aequ* aequ_newblockA_(M_ArenaLink *arena, unsigned nblocks, ...);

#ifdef ONLY_C11_COMPLIANT_COMPILER
/* FIXME: remove as soon as possible */
#ifdef __INTEL_COMPILER
#define aequ_newblockA(arena, ...) aequ_newblockA_(arena, VA_NARG_TYPED(uintptr_t, __VA_ARGS__), __VA_ARGS__)
#else
#define aequ_newblockA(arena, ...) aequ_newblockA_(arena, PP_NARG(__VA_ARGS__), __VA_ARGS__)
#endif
#endif


int aequ_get(const Aequ *e, unsigned i, rhp_idx *ei) NONNULL;
unsigned aequ_size(const Aequ *e) NONNULL;

unsigned aequ_findidx(const Aequ* e, rhp_idx ei) NONNULL;
int aequ_setblock(Aequ *e, unsigned block_size) NONNULL;
int aequ_extendandown(Aequ *e, Aequ *w) NONNULL;
bool aequ_block_contains(const struct aequ_block *b, rhp_idx ei) NONNULL;
rhp_idx aequ_block_get(const struct aequ_block *b, unsigned i) NONNULL;
void aequ_init(Aequ* e) NONNULL;
void aequ_empty(Aequ *e) NONNULL;
void aequ_reset(Aequ *e) NONNULL;
void aequ_printnames(const Aequ *e, unsigned mode, const Model *mdl) NONNULL;


void equ_dealloc(Equ **equ);
Equ *equ_alloc(unsigned lin_maxlen) MALLOC_ATTR(equ_dealloc,1);

void equ_basic_init(Equ *e) NONNULL;
int equ_compar(const void *a, const void *b);
int equ_copymetadata(Equ * restrict edst, const Equ * restrict esrc, rhp_idx ei);
int equ_copy_to(Container *ctr, rhp_idx ei, Equ *edst, rhp_idx ei_dst,
                unsigned lin_space, rhp_idx vi_no) NONNULL;
int equ_dup(Container *ctr, rhp_idx ei, rhp_idx ei_dst) NONNULL;
int equ_nltree_fromgams(Equ* e, unsigned codelen, const int instrs[VMT(restrict codelen)],
                        const int args[VMT(restrict codelen)]) NONNULL_AT(1);
void equ_free(Equ *e) NONNULL;
void equ_print(const Equ *equ);
unsigned equ_get_nladd_estimate(Equ *e) NONNULL;

void equ_err_cone(const char *fn, const Equ * restrict e);

inline static bool valid_aequ(const Aequ *e)
{
   return e->type < EquVar_Unset;
}

/**
 * @brief Get the equation index at position i
 *
 * This function does not perform any checks
 *
 * @param e  the abstract equation
 * @param i  the element to get
 *
 * @return   the equation index at position i
 */
static inline NONNULL rhp_idx aequ_fget(const Aequ *e, unsigned i)
{
   assert(e->size > 0); assert(valid_aequ(e));

   switch (e->type) {
   case EquVar_Compact:
      return e->start + (rhp_idx)i; //NOLINT
   case EquVar_List:
      return e->list[i];
   case EquVar_SortedList:
      return e->slist[i];
   case EquVar_Block:
      return aequ_block_get(e->blocks, i);
   case EquVar_Unset:
   default:
      return IdxError;
   }

   return IdxInvalid;
}

static NONNULL inline bool aequ_contains(const Aequ *e, rhp_idx ei)
{

   unsigned size = e->size;
   if (size == 0 || !valid_ei(ei) || e->type >= EquVar_Unset) {
      return false;
   }

   assert(valid_aequ(e));

   switch (e->type) {
   case EquVar_Compact:
      return (ei >= e->start) && (ei < (rhp_idx)(e->start + size));
   case EquVar_List:
      for (unsigned i = 0; i < size ; ++i) {
         if (e->list[i] == ei) { return true; }
      }
      return false;
   case EquVar_SortedList:
    {
      size_t lb = 0, ub = e->size-1, mid = (e->size-1)/2;
      while(true) {
        if (lb > ub) {
            return false;
        }
        rhp_idx val = e->slist[mid];
        if (val > ei) {
          assert(mid >= 1);
          ub = mid - 1;
        } else if (val < ei) {
          lb = mid + 1;
        } else {
          return true;
        }
        /* Do not overflow */
        mid = lb + (ub - lb)/2;
      }
    }
   case EquVar_Block:
      return aequ_block_contains(e->blocks, ei);
   default:
      assert(0);
    ;
   }

   return false;
}

static inline NONNULL void aequ_ascompact(Aequ *e, unsigned size, rhp_idx start)
{
   e->type = EquVar_Compact;
   e->size = size;
   e->start = start;
}

static inline NONNULL void aequ_aslist(Aequ *e, unsigned size, rhp_idx *list)
{
   e->type = EquVar_List;
   e->own = false;
   e->size = size;
   e->list = list;
}

OWNERSHIP_HOLDS(3)
static inline NONNULL void aequ_asownlist(Aequ *e, unsigned size, rhp_idx *list)
{
   e->own = true;
   e->type = EquVar_List;
   e->size = size;
   e->list = list;
}

OWNERSHIP_HOLDS(3)
static inline NONNULL void aequ_asownsortedlist(Aequ *e, unsigned size, rhp_idx *list)
{
   e->own = true;
   e->type = EquVar_SortedList;
   e->size = size;
   e->list = list;
}

static inline NONNULL struct aequ_block* aequ_getblocks(Aequ *e)
{
   assert(valid_aequ(e));
   return e->blocks;
}

static inline NONNULL void aequ_setsize(Aequ *e, unsigned size)
{
   assert(valid_aequ(e));
   e->size = size;
}

static inline NONNULL bool aequ_nonempty(const Aequ *e)
{
   return valid_aequ(e) && e->size > 0;
}

static inline void equ_add_cst(Equ *e, double val)
{
   switch (e->cone) {
   case CONE_R_PLUS:
   case CONE_R_MINUS:
   case CONE_R:
   case CONE_0:
      e->p.cst += val;
      break;
   case CONE_NONE: {
      EquObjectType otype = e->object;
      if (otype == Mapping || otype == BooleanRelation || otype == DefinedMapping) {
         e->p.cst += val;
         break;
      }
   }
   FALLTHRU
   case CONE_POLYHEDRAL:
   case CONE_SOC:
   case CONE_RSOC:
   case CONE_EXP:
   case CONE_DEXP:
   case CONE_POWER:
   case CONE_DPOWER:
   case CONE_LEN:
   default:
      equ_err_cone(__func__, e);
   }
}

static inline double equ_get_cst(const Equ * restrict e)
{
   switch (e->cone) {
   case CONE_R_PLUS:
   case CONE_R_MINUS:
   case CONE_R:
   case CONE_0:
      return e->p.cst;
   case CONE_NONE: {
      EquObjectType otype = e->object;
      if (otype == Mapping || otype == BooleanRelation || otype == DefinedMapping) {
         return e->p.cst;
      }
   }
   FALLTHRU
   case CONE_POLYHEDRAL:
   case CONE_SOC:
   case CONE_RSOC:
   case CONE_EXP:
   case CONE_DEXP:
   case CONE_POWER:
   case CONE_DPOWER:
   case CONE_LEN:
   default:
      equ_err_cone(__func__, e);
      return rhp_asnan(Error_NotImplemented);
   }
}

static inline void equ_set_cst(Equ *e, double val)
{
   switch (e->cone) {
   case CONE_R_PLUS:
   case CONE_R_MINUS:
   case CONE_R:
   case CONE_0:
      e->p.cst = val;
      break;
   case CONE_NONE:
      if (e->object == Mapping || e->object == BooleanRelation) {
         e->p.cst = val;
         break;
      }
   FALLTHRU
   case CONE_POLYHEDRAL:
   case CONE_SOC:
   case CONE_RSOC:
   case CONE_EXP:
   case CONE_DEXP:
   case CONE_POWER:
   case CONE_DPOWER:
   case CONE_LEN:
   default:
      equ_err_cone(__func__, e);
   }
}

static inline NONNULL bool equ_haslin(const Equ *e)
{
  return e->lequ && e->lequ->len > 0;
}

#endif /* EQU_H */
