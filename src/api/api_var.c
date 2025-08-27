#include "checks.h"
#include "macros.h"
#include "printout.h"
#include "reshop.h"
#include "var.h"

/** @file api_var.c
 * API for (container) variable */


/** @copydoc avar_new
 *  @ingroup publicAPI */
Avar* rhp_avar_new(void)
{
   return avar_new();
}

/** @copydoc avar_newcompact
 *  @ingroup publicAPI */
Avar* rhp_avar_newcompact(unsigned size, unsigned start)
{
   return avar_newcompact(size, start);
}

/** @copydoc avar_newlistborrow
 *  @ingroup publicAPI */
Avar* rhp_avar_newlist(unsigned size, rhp_idx *vis)
{
   SN_CHECK(chk_arg_nonnull(vis, 2, __func__));

   return avar_newlistborrow(size, vis);
}

/** @copydoc avar_newlistcopy
 *  @ingroup publicAPI */
Avar* rhp_avar_newlistcopy(unsigned size, rhp_idx *vis)
{
   SN_CHECK(chk_arg_nonnull(vis, 2, __func__));

   return avar_newlistcopy(size, vis);
}

/** @copydoc avar_free
 *  @ingroup publicAPI */
void rhp_avar_free(Avar* v)
{
   avar_free(v);
}

/** @copydoc avar_get
 *  @ingroup publicAPI */
int rhp_avar_get(const Avar *v, unsigned i, rhp_idx *vidx)
{
   S_CHECK(chk_arg_nonnull(v, 1, __func__));
   S_CHECK(chk_arg_nonnull(vidx, 3, __func__));

   return avar_get(v, i, vidx);
}

/** @copydoc avar_set_list
 *  @ingroup publicAPI */
int rhp_avar_set_list(Avar *v, unsigned size, rhp_idx *vis)
{
   S_CHECK(chk_arg_nonnull(v, 1, __func__));
   S_CHECK(chk_arg_nonnull(vis, 3, __func__));

   return avar_set_list(v, size, vis);
}

/** @brief Get the list associated with a variable container
 *
 * @ingroup publicAPI
 *
 * @param      v    the variable container
 * @param[out] vis  the array of indices
 *
 * @return          the error code
 */
int rhp_avar_get_list(struct rhp_avar *v, rhp_idx **vis)
{
   S_CHECK(chk_arg_nonnull(v, 1, __func__));
   S_CHECK(chk_arg_nonnull((void*)vis, 2, __func__));

   AbstractEquVarType type = v->type;
   if (type == EquVar_List || type == EquVar_SortedList) {
      *vis = v->list;
      return OK;
   }

   error("[avar] ERROR: cannot query the list of an Avar with type '%s'. It must be '%s' "
         "or '%s'\n", aequvar_typestr(type), aequvar_typestr(EquVar_List),
         aequvar_typestr(EquVar_SortedList));

   return Error_RuntimeError;
}

/** @brief Access the type of a variable container
 *
 *  @ingroup publicAPI
 *
 *  @param v the variable container
 *
 *  @return  the type of the variable container
 */
unsigned rhp_avar_gettype(const struct rhp_avar *v)
{
   if (chk_arg_nonnull(v, 1, __func__) != OK) { return EquVar_Invalid; }

   return v->type;
}

/**
 * @brief Return true if the variable container owns its memory
 *
 * @ingroup publicAPI
 *
 * @param v  the variable container
 *
 * @return   true if the variable container owns its memory
 */
bool rhp_avar_ownmem(const struct rhp_avar *v)
{
   if (chk_arg_nonnull(v, 1, __func__) != OK) { return false; }

   return v->own;
}

/** @copydoc avar_size
 *  @ingroup publicAPI */
unsigned rhp_avar_size(const Avar *v)
{
   if (chk_arg_nonnull(v, 1, __func__) != OK) { return 0; }

   return avar_size(v);
}

/** @brief Return the string description of the type of a variable container
 *
 * @ingroup publicAPI
 *
 * @param v  the variable container
 *
 * @return   the string description
 */
const char* rhp_avar_gettypename(const struct rhp_avar *v)
{
   SN_CHECK(chk_arg_nonnull(v, 1, __func__));
   return aequvar_typestr(v->type);
}

/**
 * @brief Check if an abstract variable contains a given index
 *
 * @ingroup publicAPI
 *
 * @param  v   the variable container
 * @param  vi  the variable index
 *
 * @return     0 if it is not contained, positive number if it is, a negative number for an error
 */
short rhp_avar_contains(const Avar *v, rhp_idx vi)
{
  if (chk_avar(v, __func__) != OK) { return -1; }
  if (!valid_vi(vi)) { return -2; }

  return avar_contains(v, vi) ? 1 : 0;
}
