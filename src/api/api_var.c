#include "checks.h"
#include "macros.h"
#include "reshop.h"
#include "var.h"

Avar* rhp_avar_new(void)
{
   return avar_new();
}

Avar* rhp_avar_newcompact(unsigned size, unsigned start)
{
   return avar_newcompact(size, start);
}

Avar* rhp_avar_newlist(unsigned size, rhp_idx *indx)
{
   SN_CHECK(chk_arg_nonnull(indx, 2, __func__));

   return avar_newlistborrow(size, indx);
}

Avar* rhp_avar_newlistcopy(unsigned size, rhp_idx *vis)
{
   SN_CHECK(chk_arg_nonnull(vis, 2, __func__));

   return avar_newlistcopy(size, vis);
}

void rhp_avar_free(Avar* v)
{
   avar_free(v);
}

int rhp_avar_get(const Avar *v, unsigned i, rhp_idx *vi)
{
   S_CHECK(chk_arg_nonnull(v, 1, __func__));
   S_CHECK(chk_arg_nonnull(vi, 3, __func__));

   return avar_get(v, i, vi);
}

int rhp_avar_set_list(Avar *v, unsigned size, rhp_idx *list)
{
   S_CHECK(chk_arg_nonnull(v, 1, __func__));
   S_CHECK(chk_arg_nonnull(list, 3, __func__));

   return avar_set_list(v, size, list);
}

unsigned rhp_avar_size(const Avar *v)
{
   S_CHECK(chk_arg_nonnull(v, 1, __func__));

   return avar_size(v);
}

const char* rhp_avar_gettypename(const struct rhp_avar *v)
{
   SN_CHECK(chk_arg_nonnull(v, 1, __func__));
   return aequvar_typestr(v->type);
}

/**
 * @brief Check if an abstract variable contains a given index
 *
 * @param  v   the variable container
 * @param  vi  the variable index
 *
 * @return     0 if it is not contained, positive number if it is, a negative number for an error
 */
char rhp_avar_contains(const Avar *v, rhp_idx vi)
{
  if (chk_avar(v, __func__) != OK) { return -1; }
  if (!valid_vi(vi)) { return -2; }

  return avar_contains(v, vi) ? 1 : 0;
}


