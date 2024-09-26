#include "macros.h"
#include "printout.h"
#include "reshop_data.h"
#include "rhp_search.h"

DBGUSED static bool chk_sorted_intarray(const int * restrict arr, unsigned len)
{
   for (unsigned i = 1; i < len; ++i) {
      if (arr[i-1] > arr[i]) { return false; }
   }

   return true;
}

DBGUSED static bool chk_sorted_uintarray(const unsigned * restrict arr, unsigned len)
{
   for (unsigned i = 1; i < len; ++i) {
      if (arr[i-1] > arr[i]) { return false; }
   }

   return true;
}

void rhp_int_init(IntArray *dat)
{
   dat->len = 0;
   dat->max = 0;
   dat->arr = NULL;
}

int rhp_int_reserve(IntArray *dat, unsigned size)
{
   MALLOC_(dat->arr, int, size);
   dat->max = size;

   return OK;
}

int rhp_int_add(IntArray *dat, int v)
{
   if (dat->len >= dat->max) {
      dat->max = MAX(2*dat->max, dat->len+1);
      REALLOC_(dat->arr, int, dat->max);
   }

   dat->arr[dat->len++] = v;
   return OK;
}

int rhp_int_addsorted(IntArray * restrict dat, int v)
{
   if (dat->len >= dat->max) {
      dat->max = MAX(2*dat->max, dat->len+1);
      REALLOC_(dat->arr, int, dat->max);
   }

   /* Fast check (try to append the value at the end) */
   if (dat->len == 0 || dat->arr[dat->len-1] < v) {
      dat->arr[dat->len++] = v;
      return OK;
   }

   /*  TODO(xhub) we should be able to start from pos = dat->len-1 :( */
   unsigned len = dat->len, pos = len;
   for (unsigned i = 0; i < len; i++) {
      --pos;
      if (dat->arr[pos] < v) {
         pos++;
         break;
      }
      if (dat->arr[pos] == v) {
         error("%s :: integer value %d is already in the list\n", __func__, v);
         return Error_DuplicateValue;
      }
   }

   memmove(&dat->arr[pos+1], &dat->arr[pos], (len-pos) * sizeof(int));
   dat->arr[pos] = v;
   dat->len++;

   assert(chk_sorted_intarray(dat->arr, dat->len));

   return OK;
}

int rhp_int_addseq(IntArray *dat, int start, unsigned len)
{
   if (dat->len + len >= dat->max) {
      dat->max = MAX(2*dat->max, dat->len+len);
      REALLOC_(dat->arr, int, dat->max);
   }

   int *lstart = &dat->arr[dat->len];
   for (size_t i = 0; i < len; ++i) {
      lstart[i] = start+i;
   }

   dat->len += len;
   return OK;
}

int rhp_int_copy(IntArray * restrict dat,
                 const IntArray * restrict dat_src)
{
   if (dat->len > 0) {
      error("%s :: len is non-zero: %d\n", __func__, dat->len);
      return Error_UnExpectedData;
   }

   unsigned len = dat_src->len;

   dat->len = len;

   if (len == 0) {
      return OK;
   }

   if (dat->max < len) {
      dat->max = len;
      REALLOC_(dat->arr, int, len);
   }

   memcpy(dat->arr, dat_src->arr, len*sizeof(int));

   return OK;
}

int rhp_int_extend(IntArray * restrict dat, const IntArray * restrict dat_src)
{

   unsigned len = dat_src->len;

   if (len == 0) {
      return OK;
   }

   unsigned len_ = dat->len;
   dat->len += len;

   if (dat->len > dat->max) {
      dat->max = MAX(2*dat->max, dat->len+1);
      REALLOC_(dat->arr, int, dat->max);
   }

   memcpy(&dat->arr[len_], dat_src->arr, len*sizeof(int));

   return OK;
}

int rhp_int_extend_sorted(IntArray * restrict dat,
                          const IntArray * restrict dat_src)
{
   assert(chk_sorted_intarray(dat->arr, dat->len));
   assert(chk_sorted_intarray(dat_src->arr, dat_src->len));

   unsigned slen = dat_src->len;

   if (slen == 0) {
      return OK;
   }

   unsigned dlen = dat->len;
   if (dlen == 0) {
      return rhp_int_copy(dat, dat_src);
   }

   dat->len += slen;
   unsigned len_new = dat->len;

   if (dat->len > dat->max) {
      dat->max = MAX(2*dat->max, dat->len+1);
      REALLOC_(dat->arr, int, dat->max);
   }

   int * restrict sarr = dat_src->arr, * restrict darr = dat->arr;

   int b1 = sarr[0], b2 = darr[0], e1 = sarr[slen-1], e2 = darr[dlen-1];

   if (b1 <= b2) {
      if (e1 <= b2) {
         memmove(&darr[slen], darr, dlen*sizeof(int));
         memcpy(darr, sarr, slen*sizeof(int));

         return OK;
      }

      unsigned idx = bin_insert_int(sarr, slen, b2);
      memmove(&darr[idx], darr, idx*sizeof(int));
      memcpy(darr, sarr, idx*sizeof(int));

      assert(idx < slen);
      slen -= idx;
      sarr = &sarr[idx];
      darr = &darr[idx];
   } 

   if (e1 >= e2) {
      if (b1 >= e2) {
         memcpy(&darr[dlen], sarr, slen*sizeof(int));

         return OK;
      }

      unsigned idx = bin_insert_int(sarr, slen, e2); assert(idx < slen);

      /* Insert the last slen-idx elements of sarr at the end of dlen */
      unsigned cpy_len = slen-idx;
      memcpy(&darr[dlen], &sarr[idx], (cpy_len)*sizeof(int));

      /* We don't update dlen as we can ignore the bits we just copied in the
       * subsequent search */
      slen = idx;
   }

   while (slen > 0) {

      unsigned idx = bin_insert_int(darr, dlen, sarr[0]);
      unsigned offset = 1;
      darr += idx;
      int v = darr[0];

      /* Maximize the length of the copy */
      while (offset <= slen && sarr[offset] <= v) { offset++; }

      assert(dlen >= idx+offset);
      dlen -= idx+offset;


      memmove(&darr[offset], darr, (len_new-offset)*sizeof(int));
      memcpy(darr, sarr, offset*sizeof(int));

      darr += offset;
      sarr += offset;
      slen -= offset;
   }


   assert(chk_sorted_intarray(dat->arr, len_new));

   return OK;
}

int rhp_int_extend_except_sorted(IntArray * restrict dat,
                                 const IntArray * restrict dat_src,
                                 int val)
{
   /* Quick and dirty implementation */
   S_CHECK(rhp_int_extend_sorted(dat, dat_src));

   return rhp_int_rmsorted(dat, val);
}

int rhp_int_extend_except(IntArray * restrict dat,
                        const IntArray * restrict dat_src,
                        int val)
{
   /* Quick and dirty implementation */
   S_CHECK(rhp_int_extend(dat, dat_src));

   return rhp_int_rm(dat, val);
}

void rhp_int_empty(IntArray *dat)
{
   if (dat->len > 0) {
      FREE(dat->arr);
   }
}

unsigned rhp_int_find(const IntArray *dat, int v)
{
   for (unsigned i = 0, len = dat->len; i < len; ++i) {
      if (dat->arr[i] == v) return i;
   }
   return UINT_MAX;
}

unsigned rhp_int_findsorted(const IntArray *dat, int val)
{
   assert(chk_sorted_intarray(dat->arr, dat->len));

   if (dat->len == 0) { return UINT_MAX; }
   return bin_search_int(dat->arr, dat->len, val);
}

int rhp_int_rmsorted(IntArray *dat, int v)
{
   assert(chk_sorted_intarray(dat->arr, dat->len));

   unsigned pos = rhp_int_findsorted(dat, v);

   if (pos == UINT_MAX) { goto error; }

   dat->len--;
   memmove(&dat->arr[pos], &dat->arr[pos+1], (dat->len-pos) * sizeof(int));

   assert(chk_sorted_intarray(dat->arr, dat->len));

   return OK;

error:
   error("Could not find value %d in the dataset\n", v);
   return Error_NotFound;
}

int rhp_int_rmsortednofail(IntArray *dat, int v)
{
   assert(chk_sorted_intarray(dat->arr, dat->len));

   unsigned pos = rhp_int_findsorted(dat, v);

   if (pos == UINT_MAX) { return OK; }

   dat->len--;
   memmove(&dat->arr[pos], &dat->arr[pos+1], (dat->len-pos) * sizeof(int));

   assert(chk_sorted_intarray(dat->arr, dat->len));

   return OK;
}

int rhp_int_rm(IntArray *dat, int v)
{
   unsigned pos = UINT_MAX;
   for (unsigned len = dat->len, i = len-1; i < len; --i) {
      if (dat->arr[i] == v) { pos = i; break; }
   }

   if (pos == UINT_MAX) goto error;

   dat->len--;
   memmove(&dat->arr[pos], &dat->arr[pos+1], (dat->len-pos) * sizeof(int));

   return OK;

error:
   error("Could not find value %d in the dataset\n", v);
   return Error_NotFound;
}

int rhp_int_set(IntArray *dat, unsigned idx, int v)
{
   if (idx >= dat->max) {
      error("%s :: ERROR: index %u larger than allocated memory %u. Use "
            "rhp_int_reserve() to allocate the memory\n", __func__, idx, dat->max);
      return Error_SizeTooSmall;
   }

   dat->arr[idx] = v;
   return OK;
}

struct rhp_stack_gen {
   unsigned pos;
   unsigned max;
   void* list[] __counted_by(max);
};

void rhp_obj_init(ObjArray *dat)
{
   dat->len = 0;
   dat->max = 0;
   dat->arr = NULL;
}

int rhp_obj_add(ObjArray *dat, void *v)
{
   if (dat->len >= dat->max) {
      dat->max = MAX(2*dat->max, dat->len+1);
      REALLOC_(dat->arr, void*, dat->max);
   }

   dat->arr[dat->len++] = v;
   return OK;
}

void rhp_obj_empty(ObjArray *dat)
{
   if (dat->len > 0) {
      FREE(dat->arr);
   }
}

int rhp_obj_reserve(ObjArray *dat, unsigned size)
{
   if (dat->max < size) {
      REALLOC_(dat->arr, void *, size);
      dat->max = size;
   }

   return OK;
}

struct rhp_stack_gen* rhp_stack_gen_alloc(unsigned len)
{
   struct rhp_stack_gen* s;
   MALLOC_NULL(s, struct rhp_stack_gen, sizeof(struct rhp_stack_gen) + len*sizeof(void*));
   s->pos = 0;
   s->max = len;

   return s;
}

void rhp_stack_gen_free(struct rhp_stack_gen *s)
{
   FREE(s);
}

int rhp_stack_gen_put(struct rhp_stack_gen *s, void* elt)
{
   if (s->pos == s->max) {
      error("%s :: reach the maximum number of elements %d.\n",
                         __func__, s->max);
      return Error_SizeTooSmall;
   }

   s->list[s->pos++] = elt;

   return OK;
}

int rhp_stack_gen_pop(struct rhp_stack_gen *s, void **elt)
{
   if (s->pos == 0) {
      error("%s :: the stack is empty\n", __func__);
      return Error_RuntimeError;
   }

   *elt = s->list[--(s->pos)];

   return OK;
}

size_t rhp_stack_gen_len(struct rhp_stack_gen *s)
{
   return s->pos;
}

void rhp_uint_init(UIntArray *dat)
{
   dat->len = 0;
   dat->max = 0;
   dat->arr = NULL;
}

int rhp_uint_add(UIntArray *dat, unsigned v)
{
   if (dat->len >= dat->max) {
      dat->max = MAX(2*dat->max, dat->len+1);
      REALLOC_(dat->arr, unsigned, dat->max);
   }
   assert(dat->arr);

   dat->arr[dat->len++] = v;
   return OK;
}

int rhp_uint_addsorted(UIntArray * restrict dat, unsigned v)
{
   assert(chk_sorted_uintarray(dat->arr, dat->len));

   if (dat->len >= dat->max) {
      dat->max = MAX(2*dat->max, dat->len+1);
      REALLOC_(dat->arr, unsigned, dat->max);
   }

   /* Fast check (try to append the value at the end */
   if (dat->len == 0 || dat->arr[dat->len-1] < v) {
      dat->arr[dat->len++] = v;
      return OK;
   }

   /*  TODO(xhub) we should be able to start from pos = dat->len-1 :( */
   unsigned len = dat->len, pos = dat->len;
   for (unsigned i = 0; i < len; ++i) {
      --pos;
      if (dat->arr[pos] < v) {
         pos++;
         break;
      }
      if (dat->arr[pos] == v) {
         error("%s :: integer value %d is already in the array\n",
                  __func__, v);
         return Error_DuplicateValue;
      }
   }

   memmove(&dat->arr[pos+1], &dat->arr[pos], (len-pos) * sizeof(unsigned));
   dat->arr[pos] = v;
   dat->len++;

   assert(chk_sorted_uintarray(dat->arr, dat->len));

   return OK;
}

int rhp_uint_addseq(UIntArray *dat, unsigned start, unsigned len)
{
   if (dat->len + len >= dat->max) {
      dat->max = MAX(2*dat->max, dat->len+len);
      REALLOC_(dat->arr, unsigned, dat->max);
   }

   unsigned *lstart = &dat->arr[dat->len];
   for (size_t i = 0; i < len; ++i) {
      lstart[i] = start+i;
   }

   dat->len += len;
   return OK;
}


int rhp_uint_addset(UIntArray *dat, const UIntArray *src)
{
   if (dat->len + src->len >= dat->max) {
      dat->max = MAX(2*dat->max, dat->len+src->len);
      REALLOC_(dat->arr, unsigned, dat->max);
   }

   unsigned *lstart = &dat->arr[dat->len];
   memcpy(lstart, src->arr, src->len*sizeof(unsigned));

   dat->len += src->len;
   return OK;
}

unsigned rhp_uint_find(UIntArray *dat, unsigned v)
{
   for (unsigned i = 0, len = dat->len; i < len; ++i) {
      if (dat->arr[i] == v) return i;
   }

   return UINT_MAX;
}

unsigned rhp_uint_findsorted(const UIntArray *dat, unsigned val)
{
   assert(chk_sorted_uintarray(dat->arr, dat->len));

   if (dat->len == 0) { return UINT_MAX; }
   return bin_search_uint(dat->arr, dat->len, val);
}

int rhp_uint_adduniqsorted(UIntArray *dat, unsigned v)
{
   assert(chk_sorted_uintarray(dat->arr, dat->len));

   if (dat->len > 0) {
      unsigned idx = rhp_uint_findsorted(dat, v);
      if (idx != UINT_MAX) {
         error("%s :: value %u is already present at index %u\n", __func__,
                  v, idx);
         return Error_DuplicateValue;
      }
   }

   return rhp_uint_addsorted(dat, v);
}

int rhp_uint_adduniqnofail(UIntArray *dat, unsigned v)
{
   assert(chk_sorted_uintarray(dat->arr, dat->len));

   if (dat->len > 0 && rhp_uint_findsorted(dat, v) != UINT_MAX) {
      return OK;
   }

   return rhp_uint_addsorted(dat, v);
}
int rhp_uint_copy(UIntArray * restrict dat,
                 const UIntArray * restrict dat_src)
{
   /* ---------------------------------------------------------------------
    * The semantics of this function is to overwrite, without reading,
    * all the data in the destination struct. We could have a safecopy function
    * that checks whether the destination struct has some data. However, the latter
    * could not be used on uninitialized data
    * --------------------------------------------------------------------- */

   unsigned len = dat_src->len;
   dat->len = dat->max = len;

   if (len == 0) {
      dat->arr = NULL;
      return OK;
   }

   MALLOC_(dat->arr, unsigned, len);
   memcpy(dat->arr, dat_src->arr, len*sizeof(unsigned));

   return OK;
}

void rhp_uint_empty(UIntArray *dat)
{
   if (dat->len > 0) {
      FREE(dat->arr);
   }
}

int rhp_uint_reserve(UIntArray *dat, unsigned size)
{
   if (dat->len + size >= dat->max) {
      dat->max = MAX(2*dat->max, dat->len+size);
      REALLOC_(dat->arr, unsigned, dat->max);
   }

   return OK;
}

int rhp_uint_rmsorted(UIntArray *dat, unsigned v)
{
   assert(chk_sorted_uintarray(dat->arr, dat->len));

   if (dat->len == 0) { goto error; }

   unsigned pos = rhp_uint_findsorted(dat, v);

   if (pos == UINT_MAX) { goto error; }

   dat->len--;
   memmove(&dat->arr[pos], &dat->arr[pos+1], (dat->len-pos) * sizeof(unsigned));

   assert(chk_sorted_uintarray(dat->arr, dat->len));

   return OK;

error:
   error("Could not find value %u in the dataset\n", v);
   return Error_NotFound;
}

int rhp_uint_rm(UIntArray *dat, unsigned v)
{
   unsigned pos = dat->len-1;
   for (unsigned i = 0; i < dat->len-1; ++i, --pos) {
      if (dat->arr[pos] < v) {
         error("%s :: could not find value %d in the dataset\n",
                            __func__, v);
         return Error_NotFound;
      }

      if (dat->arr[pos] == v) {
         break;
      }
   }

   dat->len--;
   memmove(&dat->arr[pos], &dat->arr[pos+1], (dat->len-pos) * sizeof(unsigned));

   return OK;
}

int rhp_uint_rmnofail(UIntArray *dat, unsigned v)
{
   unsigned len = dat->len;
   unsigned pos = UINT_MAX;

   for (unsigned i = 0; i < len-1; ++i) {
      if (dat->arr[i] == v) {
         pos = i;
         break;
      }
   }

   if (pos == UINT_MAX) { return OK; }

   dat->len--;
   memmove(&dat->arr[pos], &dat->arr[pos+1], (dat->len-pos) * sizeof(unsigned));

   return OK;
}

void scratchdbl_init(DblScratch *scratch)
{
   scratch->data = NULL;
   scratch->size = 0;
}

void scratchdbl_empty(DblScratch *scratch)
{
   FREE(scratch->data);
}

int scratchdbl_ensure(DblScratch *scratch, unsigned size)
{
   if (!scratch->data || scratch->size < size) {
      REALLOC_(scratch->data, double, size);
      scratch->size = size;
   }

   return OK;
}

void scratchint_init(IntScratch *scratch)
{
   scratch->data = NULL;
   scratch->size = 0;
}

void scratchint_empty(IntScratch *scratch)
{
   FREE(scratch->data);
}

int scratchint_ensure(IntScratch *scratch, unsigned size)
{
   if (!scratch->data || scratch->size < size) {
      REALLOC_(scratch->data, rhp_idx, size);
      scratch->size = size;
   }

   return OK;
}
