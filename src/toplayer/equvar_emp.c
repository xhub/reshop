#include "equvar_emp.h"
#include "macros.h"

Disjunction* disjunction_new(unsigned npieces)
{
   Disjunction *disj;
   CALLOCBYTES_NULL(disj, Disjunction, sizeof(Disjunction) + npieces * sizeof(DisjunctionPiece));
   disj->npieces = npieces;

   return disj;
}

void disjunctions_init(Disjunctions* disj_arr)
{
   disj_arr->len = disj_arr->max = 0;
   disj_arr->arr = NULL;
}

void disjunctions_empty(Disjunctions* disj_arr)
{
   unsigned len = disj_arr->len;
   disj_arr->len = disj_arr->max = 0;

   for (unsigned i = 0; i < len; ++i) {
      free(disj_arr->arr[i]);
   }

   FREE(disj_arr->arr);
}

int disjunctions_add(Disjunctions * restrict disj_arr, Disjunction* disj)
{
   if (disj_arr->len >= disj_arr->max) {
      disj_arr->max = MAX(2*disj_arr->max, disj_arr->len+2);
      REALLOC_(disj_arr->arr, Disjunction*, disj_arr->max);
   }

   disj_arr->arr[disj_arr->len++] = disj;

   return OK;
}

int disjunctions_copy(Disjunctions * restrict disj_arr_dst,
                      const Disjunctions * restrict disj_arr_src)
{
   if (disj_arr_src->len == 0) {
      *disj_arr_dst = (Disjunctions){0};
      return OK;
   }

   unsigned len = disj_arr_dst->max = disj_arr_dst->len = disj_arr_src->len;
   MALLOC_(disj_arr_dst->arr, Disjunction*, disj_arr_dst->max);

   Disjunction ** restrict arr_src = disj_arr_src->arr;
   Disjunction ** restrict arr_dst = disj_arr_dst->arr;

   for (unsigned i = 0; i < len; ++i, arr_src++, arr_dst++) {
      unsigned npieces = (*arr_src)->npieces;
      Disjunction *disj;
      A_CHECK(disj, disjunction_new(npieces));
      memcpy(disj->pieces, (*arr_src)->pieces, npieces * sizeof(DisjunctionPiece));
      *arr_dst = disj;
   }

   return OK;
}
