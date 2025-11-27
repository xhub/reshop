#include <math.h>
#include <stdio.h>

#include "macros.h"
#include "mdl.h"
#include "printout.h"
#include "status.h"
#include "var.h"

static const char* const var_type_names[] = {
   "continuous",
   "binary",
   "integer",
   "SOS1",
   "SOS2",
   "semi-continuous",
   "semi-integer",
   "indicator",
   "polyhedral cone",
   "SOC",
   "RSOC",
   "EXP cone",
   "dual EXP cone",
   "POWER cone",
   "dual POWER cone",
};


/** @brief Create an abstract variable
 *
 * @return the abstract variable, or NULL if the allocation failed
 */
Avar* avar_new(void)
{
   Avar *avar;
   MALLOC_NULL(avar, Avar, 1);
   avar->type = EquVar_List;
   avar->own = false;
   avar->size = 0;
   avar->list = NULL;

   return avar;
}

/**
 *  @brief create a (vector) variable of continuous indices
 *
 *  @param size   size of the variable
 *  @param start  index of the first component of the variable
 *
 *  @return       the new abstract variable
 *
 */
Avar* avar_newcompact(unsigned size, unsigned start)
{
   Avar *avar;
   MALLOC_NULL(avar, Avar, 1);
   avar->type = EquVar_Compact;
   avar->size = size;
   avar->start = start;

   return avar;
}

/**
 *  @brief create a (vector) variable which the indices are not necessarily in
 *  consecutive order
 *
 *  @warning This does not copy the list of indices. The callee is responsible
 *  for ensuring that the memory remains valid and for freeing it.
 *  Use avar_newlistcopy() the list should be copied and managed.
 *
 *  @param size  size of the variable
 *  @param vis   array of indices
 *
 *  @return      the new abstract variable
 *
 */
Avar* avar_newlistborrow(unsigned size, rhp_idx *vis)
{
   Avar *avar;
   MALLOC_NULL(avar, Avar, 1);
   avar->type = EquVar_List;
   avar->own = false;
   avar->size = size;
   avar->list = vis;

   return avar;
}

/**
 *  @brief create a (vector) variable which the indices are not necessarily in
 *  consecutive order
 *
 *  This function duplicates the list of indices
 *
 *  @param size  size of the variable
 *  @param vis   array of indices
 *
 *  @return      the new abstract variable
 *
 */
Avar* avar_newlistcopy(unsigned size, rhp_idx *vis)
{
   Avar *avar;
   MALLOC_NULL(avar, Avar, 1);
   avar->type = EquVar_List;
   avar->own = true;
   avar->size = size;
   MALLOC_EXIT_NULL(avar->list, rhp_idx, size);
   memcpy(avar->list, vis, size*sizeof(rhp_idx));

   return avar;
_exit:
  FREE(avar);
  return NULL;
}

/**
 * @brief Create an abstract variable based on blocks
 *
 * @param block_size  the number of blocks
 *
 * @return            the error code
 */
Avar* avar_newblock(unsigned block_size)
{
   Avar *v;
   MALLOC_NULL(v, Avar, 1);
   SN_CHECK_EXIT(avar_setblock(v, block_size));

   return v;

_exit:
   FREE(v);
   return NULL;
}

void avar_init(Avar* v)
{
   v->type = EquVar_Unset;
   v->own = false;
   v->size = 0;
   v->list = NULL;
}

/**
 * @brief Initialize from an index array
 *
 * @param v    the avar
 * @param vis  the index array
 */
void avar_fromIdxArray(Avar* v, const IdxArray *vis)
{
   avar_init(v);
   v->type = EquVar_List;
   v->size = vis->len;
   v->list = vis->arr;
}

/**
 * @brief Copy an avar into another one
 *
 * @param dst   the destination (copy)
 * @param src   the source
 *
 * @return      the error code
 */
int avar_copy(Avar * restrict dst, const Avar * restrict src)
{
   dst->type = src->type;

   switch (src->type) {
   case EquVar_Compact:
      dst->start = src->start;
      break;
   case EquVar_List:
      assert(!dst->list || dst->own);
      if (dst->list && dst->size < src->size) {
         REALLOC_(dst->list, rhp_idx, src->size);
      } else if (!dst->list) {
         MALLOC_(dst->list, rhp_idx, src->size);
      }
      memcpy(dst->list, src->list, src->size*sizeof(rhp_idx));
      dst->own = true;
      break;
   default:
      TO_IMPLEMENT("Block Var");
   }

   dst->size = src->size;
   return OK;
}

/**
 * @brief Empty the abstract variable and free the memory if needed
 *
 * @param v  the variable to empty
 */
void avar_empty(Avar *v)
{
  switch (v->type) {
  case  EquVar_List:
  case  EquVar_SortedList:
    if (v->own) FREE(v->list);
    v->list = NULL;
    v->size = 0;
    break;
  case EquVar_Block:
  {
    if (v->blocks) {
      AvarBlock *b = v->blocks;
      for (unsigned i = 0, len = b->len; i < len; ++i) {
        avar_empty(&b->v[i]);
      }
    }
    /* TODO: this is inconsistent with aequ*/
    if (v->own) FREE(v->blocks);
    v->blocks = NULL;
    v->size = 0;
    break;
  }
  default:
    ;
  }
}

/**
 * @brief Reset the abstract variable: the memory is not freed
 *
 * @param v  the variable to empty
 */
void avar_reset(Avar *v)
{
   v->size = 0;

   switch (v->type) {
   case EquVar_Compact:
      v->start = IdxInvalid;
      break;
   case  EquVar_List:
   case  EquVar_SortedList:
      v->own = false;
      v->list = NULL;
      break;
   case EquVar_Block: {
      if (v->blocks) {
         AvarBlock *b = v->blocks;
         for (size_t i = 0, len = b->len; i < len; ++i) {
            avar_reset(&b->v[i]);
         }
         b->len = 0;
      }
      break;
   }
   default:
      ;
   }
}

static int avar_copy_block_block(Avar *v, const Avar *w)
{
   assert(v->type == EquVar_Block && w->type == EquVar_Block);
   AvarBlock *bv = v->blocks;
   const AvarBlock *bw = w->blocks;
   unsigned lenv = bv->len;
   unsigned lenw = bw->len;

   if (lenv + lenw >= bv->max) {
      unsigned old_max = bv->max;
      unsigned max = MAX(2*bv->max, lenv+lenw+1);
      size_t alloc_size = sizeof(AvarBlock) + sizeof(Avar)*(max);
      REALLOC_BYTES(bv, alloc_size, AvarBlock)
      v->blocks = bv;
      bv->max = max;
      for (unsigned i = old_max; i < max; ++i) {
         avar_init(&bv->v[i]);
      }
   }
   
   for (unsigned i = 0, j = lenv; i < lenw; ++i, ++j) {
      avar_copy(&bv->v[j], &bw->v[i]);
   }

   v->size += w->size;
   bv->len += lenw;
   return OK;
}

static int avar_block_extend(Avar *v, const Avar *w)
{
   assert(v->type == EquVar_Block);

   if (w->type == EquVar_Block) { return avar_copy_block_block(v, w); }

   AvarBlock *b = v->blocks;
   unsigned len = b->len;
   if (len >= b->max) {
      unsigned old_max = b->max;
      unsigned max = MAX(2*b->max, len+1);
      size_t alloc_size = sizeof(AvarBlock) + sizeof(Avar)*(max);
      REALLOC_BYTES(b, alloc_size, AvarBlock)
      v->blocks = b;
      b->max = max;
      for (unsigned i = old_max; i < max; ++i) {
         avar_init(&b->v[i]);
      }
   }
   
   avar_copy(&b->v[len], w);
   v->size += w->size;
   b->len++;

   return OK;
}


int avar_extend(Avar *v, const Avar *w)
{
   if (w->type == EquVar_Unset) {
      errormsg("ERROR: uninitialized variable used!\n");
      return Error_RuntimeError;
   }

   switch (v->type) {
   case EquVar_Compact:
   case EquVar_List:
   case EquVar_SortedList:
      TO_IMPLEMENT("avar_extend for non-block avar")
   case EquVar_Block:
      return avar_block_extend(v, w);
   case EquVar_Unset:
      errormsg("ERROR: uninitialized variable used!\n");
      return Error_RuntimeError;
   default:
      error("%s :: unsupported avar type %d\n", __func__, v->type);
      return Error_RuntimeError;
   }
}

/**
 * @brief Find the index of a variable
 *
 * @param v   the abstract var
 * @param vi  the variable index
 *
 * @return    the reshop index, which is either the index or contains an error
 *            code. Check with valid_vi() its validity
 */
unsigned avar_findidx(const Avar* v, rhp_idx vi)
{
   if (v->size == 0) {
      return IdxNotFound;
   }

   switch (v->type) {
   case EquVar_Compact:
      if (vi < v->start) {
         return IdxNotFound;
      } else {
         unsigned d = vi - v->start;
         /*  TODO(xhub) check this arithmetic */
         return d < v->size ? d : IdxNotFound;
      }
   case EquVar_List:
      for (rhp_idx i = 0; i < v->size; ++i) {
         if (v->list[i] == vi) return i;
      }
      break;
   case EquVar_SortedList:
    {
      size_t lb = 0, ub = v->size-1, mid = (v->size-1)/2;
      while(true) {
        if (lb > ub) {
            return IdxNotFound;
        }
        rhp_idx val = v->slist[mid];
        if (val > vi) {
          assert(mid >= 1);
          ub = mid - 1;
        } else if (val < vi) {
          lb = mid + 1;
        } else {
          return mid;
        }
        /* Do not overflow */
        mid = lb + (ub - lb)/2;
      }
      break;
    }
   case EquVar_Block:
    for (unsigned i = 0; i < v->blocks->len; ++i) {
        unsigned idx = avar_findidx(&v->blocks->v[i], vi);
        if (valid_idx(idx)) {
          return idx;
        }
      }
    break;
   default:
      error("%s :: unsupported avar type %d\n", __func__, v->type);
      return IdxError;
   }

   return IdxNotFound;
}

/**
 * @brief Set the abstract as a list
 *
 * @param v      the abstract variable
 * @param size   the length of the list
 * @param vis    the array of indices
 *
 * @return      the error code
 */
int avar_aslist(Avar *v, unsigned size, rhp_idx vis[VMT(static size)])
{
   v->type = EquVar_List;
   v->own = false;
   v->size = size;
   v->list = vis;

   return OK;
}

int avar_setblock(Avar *v, unsigned block_size)
{
   v->type = EquVar_Block;
   v->own = true;
   v->size = 0;
   size_t alloc_size = sizeof(AvarBlock) + sizeof(Avar)*(block_size);
   MALLOCBYTES_(v->blocks, AvarBlock, alloc_size);
   v->blocks->len = 0;
   v->blocks->max = block_size;
   for (size_t i = 0, len = block_size; i < len; ++i) {
      avar_init(&v->blocks->v[i]);
   }

  return OK;
}

/**
 * @brief Free all the memory allocated in the variable
 *
 * @param v  the variable to free
 */
void avar_free(Avar* v)
{
   if (v) {
     avar_empty(v);
     free(v);
   }
}

int avar_subset(Avar* v, const rhp_idx *idx, unsigned len, Avar *v_subset)
{
   assert(v_subset);

   v_subset->type = EquVar_List;
   v_subset->own = true;
   v_subset->size = len;
   MALLOC_(v_subset->list, rhp_idx, len);

   switch (v->type) {
   case EquVar_Compact:
      for (size_t i = 0; i < len; ++i) {
         assert(idx[i] < v->size);
         v_subset->list[i] = v->start + idx[i];
      }
      break;
   case EquVar_List:
      for (size_t i = 0; i < len; ++i) {
         assert(idx[i] < v->size);
         v_subset->list[i] = v->list[idx[i]];
      }
      break;
   default:
      TO_IMPLEMENT("Block Var");
   }

   return OK;
}

/* NOTE: we use vidx and not vi to not trigger a typemap in SWIG.
 * A VariableRef needs a Model, and Vars can exists without one */
/**
 * @brief Get the variable stored at an index
 *
 * @param      v     the abstract variable
 * @param      i     the index of the abstract variable
 * @param[out] vidx  the variable index
 *
 * @return           the error code
 */
int avar_get(const Avar *v, unsigned i, rhp_idx *vidx)
{

  if (i >= v->size) {
     error("%s :: index %d is greater than the size %u of the "
                        "variable\n", __func__, i, v->size);
     return Error_IndexOutOfRange;
  }

  *vidx = avar_fget(v, i);

  return OK;
}

/**
 * @brief Return the size of an abstract variable
 *
 * @param v  the abstract variable
 *
 * @return   the length, or 0 is the argument is NULL
 */
unsigned avar_size(const Avar *v)
{
   if (!v) {
      error("%s :: Null pointer given as argument!\n", __func__);
      return 0;
   }

  return v->size;
}

bool avar_block_contains(const AvarBlock *b, rhp_idx vi)
{
   const Avar *v = b->v;
   for (size_t i = 0, len = b->len; i < len; ++i) {
      if (avar_contains(&v[i], vi)) {
         return true;
      }
   }

   return false;
}

rhp_idx avar_block_get(const AvarBlock *b, unsigned i)
{
   const Avar *v = b->v;
   size_t s = 0;
   for (size_t j = 0, len = b->len; j < len; ++j) {
      unsigned vj_size = v[j].size;
      if (i >= s && i < s + vj_size) {
         return avar_fget(&v[j], i-s);
      }
      s += vj_size;
   }

   return IdxInvalid;
}

void avar_printnames(const Avar *v, unsigned mode, const Model *mdl)
{
   unsigned size = v->size;
   printout(mode, "avar of size %u of type %s.\n", size, aequvar_typestr(v->type));


   for (unsigned i = 0; i < size; ++i) {
      rhp_idx vi = avar_fget(v, i);
       printout(mode, "\t[%5u]: #[%5u] %s\n", i, vi, ctr_printvarname(&mdl->ctr, vi));
   }
   
}

int var_set_bounds(Var *v, double lb, double ub)
{
   v->bnd.lb = lb;
   v->bnd.ub = ub;
   return OK;
}

void var_print(const Var *var)
{
   const char *vtypename = var->type < VAR_TYPE_LEN ? var_type_names[var->type] :
                                                      "INVALID";
   printout(PO_INFO, " Level = %5.2f; Bound = [%5.2e,%5.2e]; Type = %s\n",
            var->value, var->bnd.lb, var->bnd.ub, vtypename);
}

int var_compar(const void *a, const void *b)
{
   int idx_a, idx_b;

   idx_a = ((const Var *) a)->idx;
   idx_b = ((const Var *) b)->idx;

   if (idx_a < idx_b) {
      return -1;
   }
   if (idx_a == idx_b) {
      return 0;
   }
   return 1;
}

/**
 * @brief Update bounds for variable depending on the type
 *
 * For binary variables, set the bounds to [0,1], and value
 * to 0 if the original value is negative, 1 otherwise.
 *
 * For integer variables, set the bounds to [ceil(lb), floor(ub)],
 * where the original bounds where [lb,ub], and round the original
 * variable value.
 *
 * @param var   variable to update
 * @param type  type of the variable
 */
void var_update_bnd(Var *var, enum var_type type)
{
  /* TODO(xhub) use semiint representation?  */
  switch (type) {
  case VAR_B:
//    var->bnd.lb = 0;
//    var->bnd.ub = 1;
    var->value = var->value <= 0. ? 0 : 1;
    return;
  case VAR_I:
    var->bnd.lb = ceil(var->bnd.lb);
    var->bnd.ub = floor(var->bnd.ub);
    var->value = round(var->value);
    return;
  default:
    return;
  }
}

RESHOP_STATIC_ASSERT(sizeof(var_type_names)/sizeof(char*) ==  VAR_TYPE_LEN,
                     "var_type and var_type_names must be synchronized")
