#ifndef LEQU_H
#define LEQU_H

/* @file lequ.h
 *
 * @brief linear equation storage
 */

#include <stdbool.h>

#include "compat.h"
#include "equ_data.h"
#include "rhp_fwd.h"

void lequ_free(Lequ *lequ);
Lequ *lequ_new(unsigned maxlen) MALLOC_ATTR(lequ_free,1);
Lequ *lequ_new_from_data(unsigned len, const rhp_idx *vis, const double *coeffs)
MALLOC_ATTR(lequ_free,1);

void lequ_empty(Lequ *lequ);
void lequ_init(Lequ *lequ) NONNULL;

int lequ_add(Lequ *lequ, rhp_idx vi, double value ) NONNULL;
int lequ_add_unique(Lequ *lequ, int index, double value ) NONNULL;
int lequ_adds(Lequ *lequ, Avar *v, const double *values) NONNULL;
int lequ_copy(Lequ *dest, const Lequ *src) NONNULL;
int lequ_copy_except(Lequ * restrict dest,
                     const Lequ * restrict src,
                     rhp_idx vi_no) NONNULL;
Lequ* lequ_dup_rosetta(const Lequ *src, const rhp_idx *rosetta) NONNULL;

void lequ_delete(Lequ *le, size_t pos);
int lequ_find(const Lequ *lequ, rhp_idx vi, double *value, unsigned *pos)
NONNULL ACCESS_ATTR(write_only, 3) ACCESS_ATTR(write_only, 4);

void lequ_print(const Lequ *lequ, unsigned mode) NONNULL;
int lequ_reserve(Lequ *lequ, unsigned maxlen ) NONNULL;
int lequ_scal(Lequ *lequ, double coeff) NONNULL;

static inline NONNULL bool lequ_debug_hasvar(Lequ *le, rhp_idx vi)
{
   rhp_idx *vis = le->vis;

   for (size_t i = 0, len = le->len; i < len; ++i) {
      if (vis[i] == vi) {
         return true;
      }
   }
   return false;
}

static inline NONNULL int lequ_debug_quick_chk(rhp_idx vi, unsigned len, rhp_idx *idx, double *vals)
{
   Lequ le = {.max = len, .len = len, .vis = idx, .coeffs = vals};
   return !lequ_debug_hasvar(&le, vi);
}

#endif /* LEQU_H */
