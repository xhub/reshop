#include "asnan.h"

#include <float.h>
#include <math.h>

#include "cmat.h"
#include "ctr_rhp.h"
#include "nltree_priv.h"
#include "status.h"
#include "container.h"
#include "equ.h"
#include "nltree.h"
#include "lequ.h"
#include "macros.h"
#include "mdl.h"
#include "printout.h"

/**
 * @brief Allocate an abstract equation
 *
 * @ingroup publicAPI
 *
 * @return the abstraction equation, or NULL is the allocation failed
 */
Aequ *aequ_new(void)
{
   Aequ *aequ;
   MALLOC_NULL(aequ, Aequ, 1);
   aequ->type = EquVar_Unset;
   aequ->own = false;
   aequ->size = 0;
   aequ->list = NULL;

   return aequ;
}
/**
 *  @brief create a (vector) equation of continuous indices
 *
 *  @ingroup publicAPI
 *
 *  @param size   size of the variable
 *  @param start  index of the first component of the variable
 *
 *  @return       the error code
 *
 */
Aequ* aequ_newcompact(unsigned size, rhp_idx start)
{
   Aequ *aequ;
   MALLOC_NULL(aequ, Aequ, 1);
   aequ->type = EquVar_Compact;
   aequ->size = size;
   aequ->start = start;

   return aequ;
}


/**
 *  @brief create a (vector) equation which the indices are not necessarily in
 *  consecutive order
 *
 *  @warning This does not copy the list of indices. The callee is responsible
 *  for ensuring that the memory remains valid and for freeing it.
 *  Use aequ_newlistcopy() the list should be copied and managed.
 *
 *  @ingroup publicAPI
 *
 *  @param size  size of the equation
 *  @param list  array of indices
 *
 *  @return      the error code
 *
 */
Aequ* aequ_newlistborrow(unsigned size, rhp_idx *list)
{
   Aequ *aequ;
   MALLOC_NULL(aequ, Aequ, 1);
   aequ->type = EquVar_List;
   aequ->own = false;
   aequ->size = size;
   aequ->list = list;

   return aequ;
}

/**
 *  @brief create a (vector) equation which the indices are not necessarily in
 *  consecutive order
 *
 *  This function duplicates the list of indices
 *
 *  @ingroup publicAPI
 *
 *  @param size  size of the equation
 *  @param eis   array of indices
 *
 *  @return      the object, or NULL if there is an error
 *
 */
Aequ* aequ_newlistcopy(unsigned size, rhp_idx *eis)
{
   Aequ *aequ;
   MALLOC_NULL(aequ, Aequ, 1);
   aequ->type = EquVar_List;
   aequ->own = true;
   aequ->size = size;
   MALLOC_EXIT_NULL(aequ->list, rhp_idx, size);
   memcpy(aequ->list, eis, size*sizeof(rhp_idx));

   return aequ;
_exit:
  FREE(aequ);
  return NULL;
}

/**
 * @brief Create an abstract equation based on blocks
 *
 * @param num_blocks  the number of blocks
 *
 * @return            the error code
 */
Aequ *aequ_newblock(unsigned num_blocks)
{
   Aequ *e;
   MALLOC_NULL(e, Aequ, 1);

   SN_CHECK_EXIT(aequ_setblock(e, num_blocks));

   return e;

_exit:
   FREE(e);
   return NULL;
}

void aequ_init(Aequ *e)
{
   e->type = EquVar_Unset;
   e->size = 0;
   e->list = NULL;
}

/**
 * @brief Empty the abstract equation and free the memory if needed
 *
 * @param e  the equation to empty
 */
void aequ_empty(Aequ *e)
{
  switch (e->type) {
  case EquVar_List:
  case EquVar_SortedList:
    if (e->own) FREE(e->list);
    e->list = NULL;
    e->size = 0;
    break;
  case EquVar_Block:
  {
    if (e->blocks) {
      AequBlock *b = e->blocks;
      for (size_t i = 0, len = b->len; i < len; ++i) {
         aequ_empty(&b->e[i]);
      }
    }
    /* Blocks are unconditional freed  */
    FREE(e->blocks);
    e->blocks = NULL;
    e->size = 0;
    break;
  }
  default:
    ;
  }
}

/**
 * @brief Reset the abstract variable: the memory is not freed
 *
 * @param e  the equation to reset
 */
void aequ_reset(Aequ *e)
{
  e->size = 0;
  switch (e->type) {
  case  EquVar_List:
  case  EquVar_SortedList:
      break;
  case EquVar_Block:
  {
    if (e->blocks) {
      AequBlock *b = e->blocks;
      for (size_t i = 0, len = b->len; i < len; ++i) {
        aequ_reset(&b->e[i]);
      }
      b->len = 0;
    }
    break;
  }
  default:
    ;
  }
}

/**
 * @brief Free the allocated memory in the abstract equation (including the structure itself!)
 *
 * @ingroup publicAPI
 *
 * @param e  the abstract equation
 */
void aequ_free(Aequ *e)
{
   if (!e) { return; }
   aequ_empty(e);
   FREE(e);
}

/**
 * @brief Copy an aequ into another one
 *
 * @param dst   the destination (uninitialized)
 * @param src   the source
 *
 * @return      the error code
 */
static int aequ_copy(Aequ *dst, Aequ *src)
{
   dst->type = src->type;
   dst->size = src->size;

   switch (src->type) {
   case EquVar_Compact:
      dst->start = src->start;
      break;
   case EquVar_List:
   case EquVar_SortedList:
      if (!src->own) {
         MALLOC_(dst->list, rhp_idx, src->size);
         memcpy(dst->list, src->list, src->size*sizeof(rhp_idx));
      } else {
         dst->list = src->list;
         src->list = NULL;
      }
      dst->own = true;
      break;
   default:
      TO_IMPLEMENT("Block Equ");
   }

   return OK;
}

int aequ_setblock(Aequ *e, unsigned block_size)
{
   assert(e);
   e->type = EquVar_Block;
   e->own = true;
   e->size = 0;
   size_t alloc_size = sizeof(AequBlock) + sizeof(Aequ)*(block_size);
   MALLOCBYTES_(e->blocks, AequBlock, alloc_size);
   e->blocks->max = block_size;
   e->blocks->len = 0;
   for (size_t i = 0, len = block_size; i < len; ++i) {
      aequ_init(&e->blocks->e[i]);
   }

   return OK;
}

/**
 * @brief Extend the blocked aequ with another instance
 *
 * @warning: this takes ownership of the given instance!
 *
 * @param e  the block aequ to be extended
 * @param w  the aequ to add
 *
 * @return   the error code
 */
static int aequ_block_extend(Aequ *e, Aequ *w)
{
   assert(e->type == EquVar_Block);
   if (w->type == EquVar_Unset) { return OK; }

   AequBlock *b = e->blocks;
   unsigned len = b->len;
   if (len >= b->max) {
      unsigned max = MAX(2*b->max, len+1);
      size_t alloc_size = sizeof(AequBlock) + sizeof(Aequ)*(max);
      REALLOC_BYTES(b, alloc_size, AequBlock)
      e->blocks = b;
      b->max = max;
   }
   
   aequ_copy(&b->e[len], w);
   e->size += w->size;
   b->len++;

   return OK;
}


/**
 * @brief Extend an aequ with another instance
 *
 * @warning: this takes ownership of the added instance!
 *
 * @param e  the block aequ to be extended
 * @param w  the aequ to add
 *
 * @return   the error code
 */
int aequ_extendandown(Aequ *e, Aequ *w)
{
   switch (e->type) {
   case EquVar_Compact:
   case EquVar_List:
   case EquVar_SortedList:
      TO_IMPLEMENT("")
   case EquVar_Block:
      return aequ_block_extend(e, w);
   default:
      error("%s :: unsupported aequ type %d", __func__, e->type);
      return Error_RuntimeError;
   }
}

/**
 * @brief Get the equation index at position i
 *
 * @ingroup publicAPI
 *
 * @param      e     the abstract equation
 * @param      i     the element to get
 * @param[out] eidx  the equation index
 *
 * @return   the equation index at position i
 */
int aequ_get(const Aequ *e, unsigned i, rhp_idx *eidx)
{
   if (i >= e->size) {
      return Error_IndexOutOfRange;
   }

   if (e->type == EquVar_List && !e->list) {
      error("%s :: abstract equ is of list type, but the list "
                         "pointer is NULL!\n", __func__);
      return Error_NullPointer;
   }
   if (e->type == EquVar_Block) {
      TO_IMPLEMENT("Block Equ");
   }

   *eidx = aequ_fget(e, i);

   return OK;
}

bool aequ_block_contains(const AequBlock *b, rhp_idx ei)
{
   const Aequ *e = b->e;
   for (size_t i = 0, len = b->len; i < len; ++i) {
      if (aequ_contains(&e[i], ei)) {
         return true;
      }
   }

   return false;
}

/**
 * @brief Private function to get the ith element of an aequ of type block
 *
 * @param b  the block
 * @param i  the index
 * @return   the value at the ith place.
 */
rhp_idx aequ_block_get(const AequBlock *b, unsigned i)
{
   const Aequ *e = b->e;
   size_t s = 0;

   for (size_t j = 0, len = b->len; j < len; ++j) {
      unsigned ej_size = e[j].size;
      if (i >= s && i < s + ej_size) {
         return aequ_fget(&e[j], i-s);
      }
      s += ej_size;
   }

   return IdxInvalid;
}

/**
 * @brief Return the size of the abstract equation
 *
 * @ingroup publicAPI
 *
 * @param e  the abstract equation
 *
 * @return   the size, or 0 is the argument is NULL
 */
unsigned aequ_size(const Aequ *e)
{
  return e->size;
}


/**
 * @brief Find the index of a equation
 *
 * @param e   the abstract equ
 * @param ei  the equation index
 *
 * @return    the reshop index, which is either the index or contains an error
 *            code. Check with valid_ei() its validity
 */
unsigned aequ_findidx(const Aequ *e, rhp_idx ei)
{
   if (e->size == 0) {
      return IdxNotFound;
   }

   switch (e->type) {
   case EquVar_Compact:
      if (ei < e->start) {
         return IdxNotFound;
      } else {
         unsigned idx = ei - e->start;
         /*  TODO(xhub) check this arithmetic */
         return idx < e->size ? idx : IdxNotFound;
      }
   case EquVar_List:
      for (unsigned i = 0; i < e->size; ++i) {
         if (e->list[i] == ei) return i;
      }
      break;
   case EquVar_SortedList:
    {
      size_t lb = 0, ub = e->size-1, mid = (e->size-1)/2;
      if (ei < e->slist[0] || ei > e->slist[ub]) { return IdxNotFound; }
      while(true) {
        if (lb >= ub) {
            return ei == e->slist[ub] ? ub : IdxNotFound;
        }
        rhp_idx val = e->slist[mid];
        if (val > ei) {
          assert(mid >= 1);
          ub = mid - 1;
        } else if (val < ei) {
          lb = mid + 1;
        } else {
          return mid;
        }
        /* Do not overflow */
        mid = lb + (ub - lb)/2;
      }
        break;
    }
   case EquVar_Block: {
    unsigned idx = 0;
    for (unsigned i = 0; i < e->blocks->len; ++i) {
        unsigned lidx = aequ_findidx(&e->blocks->e[i], ei);
        if (valid_idx(lidx)) {
          return idx + lidx;
        }
        idx += aequ_size(&e->blocks->e[i]);
      }
    }
    break;
   default:
      error("%s :: unsupported aequ type %d\n", __func__, e->type);
      return IdxError;
   }

   return IdxNotFound;
}

void aequ_printnames(const Aequ *e, unsigned mode, const Model *mdl)
{
   unsigned size = e->size;
   printout(mode, "aequ of size %u of type %s.\n", size, aequvar_typestr(e->type));


   for (unsigned i = 0; i < size; ++i) {
      rhp_idx ei = aequ_fget(e, i);
       printout(mode, "\t[%5u]: #[%5u] %s\n", i, ei, ctr_printequname(&mdl->ctr, ei));
   }
   
}


/* -------------------------------------------------------------------------
 * Equation functions follow
 * ------------------------------------------------------------------------- */




/**
 * @brief Allocate an equation
 *
 * @param lin_maxlen  the initial maximum length of the liner part of the equation
 *
 * @return            the equation, or NULL if the allocation failed
 */
Equ *equ_alloc(unsigned lin_maxlen)
{
   Equ *equ;
   MALLOC_NULL(equ, struct equ, 1);
   lin_maxlen = MAX(lin_maxlen, 10);
   equ->idx = IdxInvalid;
   equ_basic_init(equ);

   AA_CHECK_EXIT(equ->lequ, lequ_new(lin_maxlen));

   equ->tree = NULL;

   return equ;

_exit:
   FREE(equ);
   return NULL;
}

void equ_free(Equ *e)
{
   lequ_free(e->lequ);
   nltree_dealloc(e->tree);
}

void equ_dealloc(Equ **equ)
{
   if (!*equ) return;
   equ_free(*equ);
   FREE((*equ));
   (*equ) = NULL;
}

void equ_basic_init(Equ *e)
{
   e->object = EquTypeUnset;
   e->cone = CONE_NONE;
   e->basis = BasisUnset;
   e->is_quad = false;
   /*  TODO(Xhub) try to get a better control over that */
   e->p.cst = 0.;
   e->value = SNAN_UNINIT;
   e->multiplier = SNAN_UNINIT;
}

int equ_compar(const void *a, const void *b)
{
   int idx_a, idx_b;

   idx_a = ((const Equ *) a)->idx;
   idx_b = ((const Equ *) b)->idx;

   if (idx_a < idx_b) {
      return -1;
   }

   if (idx_a == idx_b) {
      return 0;
   }

   return 1;
}

void equ_print(const Equ *equ)
{
   /*  TODO(xhub) CONE_SUPPORT */
   printout(PO_INFO, " Type = %5d, Cone = %5d, cst = %.5e\n", equ->object, equ->cone, equ->p.cst);

   if (equ->lequ) {
      lequ_print(equ->lequ, PO_INFO);
   }
}

/*  This code doesn't appear to be used, but it may be useful in the future*/
#if 0
int equ_replacevar(Equ *equ, const Var* v, Equ *equsub)
{
   if (equ_isNL(equ) || equ_isNL(equsub))
   {
      if (!equ->tree)
      {
         S_CHECK(nltree_build(equ)); /* FAIL for linear terms */
      }
      if (!equsub->tree)
      {
         S_CHECK(nltree_build(equsub)); /* FAIL for linear terms */
      }

      S_CHECK(nltree_replacevar(equ->tree, v, equsub->tree));
   }

   return OK;
}
#endif

int equ_copymetadata(Equ * restrict edst, const Equ * restrict esrc, rhp_idx ei)
{
   edst->idx = ei;
   edst->basis = esrc->basis;
   edst->object = esrc->object;
   edst->cone = esrc->cone;
   edst->is_quad = esrc->is_quad;

   /* TODO(xhub) CONE SUPPORT */
   edst->p = esrc->p;
   edst->value = esrc->value;
   edst->multiplier = esrc->multiplier;

   return OK;
}

/**
 * @brief Create a copy of an equation
 *
 * @param ctr        the container
 * @param ei_src     the original equation index
 * @param edst       the new equation
 * @param ei_dst     the equation index for the destination
 * @param lin_space  the estimated size of the linear part of the equation
 * @param vi_no      if a valid index, designate a variable not to copy
 *
 * @return           the error code
 */
int equ_copy_to(Container *ctr, rhp_idx ei_src, Equ *edst,
                rhp_idx ei_dst, unsigned lin_space, rhp_idx vi_no)
{
   Equ *esrc = &ctr->equs[ei_src];

   S_CHECK(equ_copymetadata(edst, esrc, ei_dst));

   if (!edst->lequ) {
      A_CHECK(edst->lequ, lequ_new(esrc->lequ->len + lin_space));
   } else {
      if (edst->lequ->len != 0) {
         error("%s :: lequ already present!\n", __func__);
         return Error_UnExpectedData;
      }
      S_CHECK(lequ_reserve(edst->lequ, esrc->lequ->len + lin_space));
   }

   if (valid_vi(vi_no)) {
      S_CHECK(lequ_copy_except(edst->lequ, esrc->lequ, vi_no));
   } else {
      S_CHECK(lequ_copy(edst->lequ, esrc->lequ));
   }

   S_CHECK(rctr_getnl(ctr, esrc));

   if (esrc->tree) {
      A_CHECK(edst->tree, nltree_dup(esrc->tree, NULL, 0));
      edst->tree->idx = ei_dst;
   } else {
      edst->tree = NULL;
   }

   return OK;
}

/**
 * @brief Duplicate an equation
 *
 * @param ctr        the container
 * @param ei_src     the original equation index
 * @param edst       the new equation
 * @param ei_dst     the equation index for the destination
 *
 * @return           the error code
 */
int equ_dup(Container *ctr, rhp_idx ei_src, rhp_idx ei_dst)
{
   assert(ei_src != ei_dst && valid_ei_(ei_src, rctr_totalm(ctr), __func__));
   assert(valid_ei_(ei_dst, rctr_totalm(ctr), __func__));

   Equ * restrict esrc = &ctr->equs[ei_src];
   Equ * restrict edst = &ctr->equs[ei_dst];

   S_CHECK(equ_copymetadata(edst, esrc, ei_dst));

   if (!edst->lequ) {
      A_CHECK(edst->lequ, lequ_new(esrc->lequ->len));
   } else {
      if (edst->lequ->len != 0) {
         error("%s :: lequ already present!\n", __func__);
         return Error_UnExpectedData;
      }
      S_CHECK(lequ_reserve(edst->lequ, esrc->lequ->len));
   }

   S_CHECK(lequ_copy(edst->lequ, esrc->lequ));

   S_CHECK(rctr_getnl(ctr, esrc));

   if (esrc->tree) {
      A_CHECK(edst->tree, nltree_dup(esrc->tree, NULL, 0));
      edst->tree->idx = ei_dst;
   } else {
      edst->tree = NULL;
   }

   return OK;
}

void equ_err_cone(const char *fn, const Equ * restrict e)
{
  error("ERROR: unsupported cone %s for equation %d in function %s\n ",
        cone_name(e->cone), e->idx, fn);
   assert(0);
}

int equ_nltree_fromgams(Equ* e, unsigned codelen, const int *instrs, const int *args)
{
   if (e->tree) {
      error("%s :: the tree for equation %d already exists\n",
               __func__, e->idx);
      return Error_UnExpectedData;
   }

   if (codelen > 0) {
      A_CHECK(e->tree, nltree_buildfromgams(codelen, instrs, args));
      if (valid_ei(e->idx)) {
         e->tree->idx = e->idx;
      } else {
         e->idx = e->tree->idx;
      }
   } 

   return OK;
}

unsigned equ_get_nladd_estimate(Equ *e)
{
   unsigned res = (e->lequ ? e->lequ->len : 0);

   if (e->tree && e->tree->root) {
      NlNode *root = e->tree->root;
      if (root->op == NLNODE_ADD) {
         res += root->children_max;
      } else {
         res += 1;
      }
   }

   return res;
}
