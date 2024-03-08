#include "macros.h"
#include "printout.h"
#include "reshop_data.h"
#include "rhp_search.h"

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

   if (len == 0) {
      dat->len = dat->max = 0;
      dat->arr = NULL;
      return OK;
   }

   dat->len = len;
   dat->max = len;
   MALLOC_(dat->arr, int, len);
   memcpy(dat->arr, dat_src->arr, len*sizeof(int));

   return OK;
}

void rhp_int_empty(IntArray *dat)
{
   if (dat->len > 0) {
      FREE(dat->arr);
   }
}

unsigned rhp_int_find(IntArray *dat, int v)
{
   for (unsigned i = 0, len = dat->len; i < len; ++i) {
      if (dat->arr[i] == v) return i;
   }
   return UINT_MAX;
}

unsigned rhp_int_findsorted(const IntArray *dat, int val)
{
   if (dat->len == 0) { return UINT_MAX; }
   return bin_search_int(dat->arr, dat->len, val);
}

int rhp_int_rmsorted(IntArray *dat, int v)
{
   unsigned pos = UINT_MAX;
   for (unsigned len = dat->len, i = len-1; i < len; --i) {
      if (dat->arr[i] < v) { goto error; } 

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
   if (dat->len == 0) { return UINT_MAX; }
   return bin_search_uint(dat->arr, dat->len, val);
}

int rhp_uint_adduniq(UIntArray *dat, unsigned v)
{
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
