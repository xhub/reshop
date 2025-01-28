#include <math.h>
#include <stdarg.h>

#include "block_array.h"
#include "allocators.h"
#include "macros.h"
#include "printout.h"

DblArrayBlock* dblarrs_allocA(M_ArenaLink *arena, unsigned max)
{
   DblArrayBlock* dblarrs = arenaL_alloc(arena, sizeof(DblArrayBlock)
                                         + (sizeof(DblArrayElt)*max));
   dblarrs->size = 0;
   dblarrs->len = 0;
   dblarrs->max = 0;

   return dblarrs;
}

DblArrayBlock* dblarrs_new_(M_ArenaLink *arena, unsigned len, ...)
{
   DblArrayBlock * restrict dblarrs = dblarrs_allocA(arena, len);
   dblarrs->len = len;

   unsigned sz = 0;

   va_list ap;
   va_start(ap, len);
   for (unsigned i = 0, j = 0; j < len; ++i, j+=2) {
      unsigned size = (unsigned)va_arg(ap, unsigned);
      sz += size;
      dblarrs->blocks[i].size = size;
      dblarrs->blocks[i].arr = (double*)va_arg(ap, double*);
   }
   va_end(ap);

   dblarrs->size = sz;

   return dblarrs;
}

int dblarrs_store(DblArrayBlock* dblarr, double *arr)
{
   TO_IMPLEMENT("[dblarrs] STORE");
}

double dblarrs_fget(DblArrayBlock * restrict dblarr, unsigned idx) 
{
   if (RHP_UNLIKELY(idx >= dblarr->size)) {
      error("[dblarrs] FATAL ERROR: index %u outside of [0,%u)]", idx, dblarr->size);
      return NAN;
   }

   unsigned end = 0, start = 0;
   DblArrayElt * restrict blocks =  dblarr->blocks;
   for (unsigned i = 0, len = dblarr->len; i < len; ++i, blocks++, start = end) {
      end += blocks->size;

      if (idx < end) { return blocks->arr[idx-start]; }
   }

   error("[dblarrs] FATAL RUNTIME ERROR: idx = %u, size = %u", idx, dblarr->size);

   return NAN; 
}
