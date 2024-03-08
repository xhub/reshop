#ifndef EMPPARSER_VM_UTILS_H
#define EMPPARSER_VM_UTILS_H

#include "empinterp_vm.h"
#include <assert.h>
#include <stdint.h>

#ifdef NAN_BOXING

/* Main idea of NaN-boxing:
 * - A NaN is any floating point value with all the exponents bits set to 1
 * - This is MASK_EXPONENT
 * - the difference between qNaN and sNaN is in the MASK_QUIET bit
 * - The upper 16 bits (MASK_SIGNATURE) are giving us the type of object
 * - Any kind of pointers has the sign bit set.
 */

/* TODO:
 * - is it possible to use sNaN also? This would double the number of signatures
 * - SIGNATURE_VALUES are used only for a small number of constants. Maybe we can remove that
 *   and pack it with integer or loopvar
 */


#define MASK_SIGN           ((uint64_t)0x8000000000000000)
#define MASK_EXPONENT       ((uint64_t)0x7ff0000000000000)
#define MASK_QUIET          ((uint64_t)0x0008000000000000)
#define MASK_TYPE           ((uint64_t)0x0007000000000000)
#define MASK_SIGNATURE      ((uint64_t)0xffff000000000000)
#define MASK_PAYLOAD_PTR    ((uint64_t)0x0000ffffffffffff)
#define MASK_PAYLOAD_32     ((uint64_t)0x00000000ffffffff)


#define MASK_TYPE_NAN       ((uint64_t)0x0000000000000000)
#define MASK_TYPE_VALUES    ((uint64_t)0x0001000000000000)
#define MASK_TYPE_INTEGER   ((uint64_t)0x0002000000000000)
#define MASK_TYPE_UINTEGER  ((uint64_t)0x0003000000000000)
#define MASK_TYPE_LOOPVAR   ((uint64_t)0x0004000000000000)
/* Is MASK_TYPE needed? */
/* We can get rid of MASK_TYPE_NULL if needed */

#define MASK_TYPE_STRING       ((uint64_t)0x0001000000000000)
#define MASK_TYPE_MPOBJ        ((uint64_t)0x0002000000000000)
#define MASK_TYPE_MPEOBJ       ((uint64_t)0x0003000000000000)
#define MASK_TYPE_OVFOBJ       ((uint64_t)0x0004000000000000)
#define MASK_TYPE_GMSSYMITER   ((uint64_t)0x0005000000000000)
#define MASK_TYPE_REGENTRY     ((uint64_t)0x0006000000000000)
#define MASK_TYPE_ARCOBJ      ((uint64_t)0x0007000000000000)

// SNAN would be MASK_EXPONENT
#define QNAN                (MASK_EXPONENT | MASK_QUIET)

#define SIGNATURE_QNAN       (QNAN)
#define SIGNATURE_VALUES    (QNAN | MASK_TYPE_VALUES)
#define SIGNATURE_INTEGER   (QNAN | MASK_TYPE_INTEGER)
#define SIGNATURE_UINTEGER  (QNAN | MASK_TYPE_UINTEGER)
#define SIGNATURE_LOOPVAR   (QNAN | MASK_TYPE_LOOPVAR)

/* All pointers do match SIGNATURE_POINTER, and we distinguish a few types */
#define SIGNATURE_POINTER     (QNAN | MASK_SIGN )

#define SIGNATURE_STRING      (SIGNATURE_POINTER | MASK_TYPE_STRING)
#define SIGNATURE_MPOBJ       (SIGNATURE_POINTER | MASK_TYPE_MPOBJ)
#define SIGNATURE_MPEOBJ      (SIGNATURE_POINTER | MASK_TYPE_MPEOBJ)
#define SIGNATURE_OVFOBJ      (SIGNATURE_POINTER | MASK_TYPE_OVFOBJ)
#define SIGNATURE_GMSSYMITER  (SIGNATURE_POINTER | MASK_TYPE_GMSSYMITER)
#define SIGNATURE_REGENTRY    (SIGNATURE_POINTER | MASK_TYPE_REGENTRY)
#define SIGNATURE_ARCOBJ     (SIGNATURE_POINTER | MASK_TYPE_ARCOBJ)


#define TAG_FALSE           1
#define TAG_TRUE            2
#define TAG_NULL            3

#define FALSE_VAL           ((VmValue) (SIGNATURE_VALUES | TAG_FALSE))
#define TRUE_VAL            ((VmValue) (SIGNATURE_VALUES | TAG_TRUE))
#define NULL_VAL            ((VmValue) (SIGNATURE_VALUES | TAG_NULL))

#define VALUE_TAG(value)    ((value) & ~MASK_SIGNATURE)

/* A finite double cannot have all its exponent set to 1 */
#define IS_FINITE_DBL(value)    (((value) & MASK_EXPONENT) != MASK_EXPONENT)

#define IS_NIL(value)       ((value) == NULL_VAL)

#define IS_UEL(value)       (((value) & MASK_SIGNATURE) == SIGNATURE_INTEGER)
#define IS_INT(value)       (((value) & MASK_SIGNATURE) == SIGNATURE_INTEGER)
#define IS_UINT(value)      (((value) & MASK_SIGNATURE) == SIGNATURE_UINTEGER)
#define IS_LOOPVAR(value)   (((value) & MASK_SIGNATURE) == SIGNATURE_LOOPVAR)
#define IS_BOOL(value)      ((((value) & MASK_SIGNATURE) == SIGNATURE_VALUES) && \
                            (VALUE_TAG(value) <= 2))

#define IS_PTR(value)        (((value) & MASK_SIGNATURE) == SIGNATURE_POINTER)
#define IS_STR(value)        (((value) & MASK_SIGNATURE) == SIGNATURE_STRING)
#define IS_MPOBJ(value)      (((value) & MASK_SIGNATURE) == SIGNATURE_MPOBJ)
#define IS_MPEOBJ(value)     (((value) & MASK_SIGNATURE) == SIGNATURE_MPEOBJ)
#define IS_OVFOBJ(value)     (((value) & MASK_SIGNATURE) == SIGNATURE_OVFOBJ)
#define IS_GMSSYMITER(value) (((value) & MASK_SIGNATURE) == SIGNATURE_GMSSYMITER)
#define IS_REGENTRY(value)   (((value) & MASK_SIGNATURE) == SIGNATURE_REGENTRY)
#define IS_ARCOBJ(value)    (((value) & MASK_SIGNATURE) == SIGNATURE_ARCOBJ)

#define AS_BOOL(value)      ((value) == TRUE_VAL)
#define AS_NUMBER(value)    (value)
#define AS_UINT(value)      (uint32_t)((value) & MASK_PAYLOAD_32)
#define AS_INT(value)       (int32_t)((value) & MASK_PAYLOAD_32)
#define AS_LOOPVAR(value)   (int32_t)((value) & MASK_PAYLOAD_32)


#define AS_OBJ(value) \
    ((void*)(uintptr_t)((value) & MASK_PAYLOAD_PTR)) //NOLINT(performance-no-int-to-ptr)

#define AS_STR(value)        (const char*)AS_OBJ((value))
#define AS_MPOBJ(value)      (MathPrgm*)AS_OBJ((value))
#define AS_MPEOBJ(value)     (Mpe*)AS_OBJ((value))
#define AS_OVFOBJ(value)     (OvfDef*)AS_OBJ((value))
#define AS_GMSSYMITER(value) (void *)AS_OBJ((value))
#define AS_REGENTRY(value)   (void *)AS_OBJ((value))
#define AS_ARCOBJ(value)    (void *)AS_OBJ((value))


#define BOOL_VAL(b)         ((b) ? TRUE_VAL : FALSE_VAL)
#define NUMBER_VAL(num)     (VmValue)(double)(num)
#define UINT_VAL(num)       (((VmValue)((uint32_t)(num))) | SIGNATURE_UINTEGER)
#define INT_VAL(num)        (((VmValue)((int32_t)(num)))  | SIGNATURE_INTEGER)
#define LOOPVAR_VAL(num)    (((VmValue)((int32_t)(num)))  | SIGNATURE_LOOPVAR)

#define OBJ_VAL(obj)    \
   (VmValue)(SIGNATURE_POINTER    |  (uint64_t)(uintptr_t)(obj))
#define STR_VAL(obj)    \
   (VmValue)(SIGNATURE_STRING    |   (uint64_t)(uintptr_t)(obj)) 
#define MPOBJ_VAL(obj)  \
   (VmValue)(SIGNATURE_MPOBJ      |  (uint64_t)(uintptr_t)(obj)) 
#define MPEOBJ_VAL(obj) \
   (VmValue)(SIGNATURE_MPEOBJ     |  (uint64_t)(uintptr_t)(obj)) 
#define OVFOBJ_VAL(obj) \
   (VmValue)(SIGNATURE_OVFOBJ     |  (uint64_t)(uintptr_t)(obj)) 
#define GMSSYMITER_VAL(obj) \
   (VmValue)(SIGNATURE_GMSSYMITER  | (uint64_t)(uintptr_t)(obj)) 
#define REGENTRY_VAL(obj) \
   (VmValue)(SIGNATURE_REGENTRY    | (uint64_t)(uintptr_t)(obj)) 
#define ARCOBJ_VAL(obj) \
  (VmValue)(SIGNATURE_ARCOBJ     |  (uint64_t)(uintptr_t)(obj)) 


#define VM_VALUE_INC(value)  ((value))++

/* Custom defines */
#define AS_PTR AS_OBJ
#define PTR_VAL OBJ_VAL



#else /* NO NAN-BOXING */


#define AS_BOOL(value)     (value).b
#define AS_INT(value)      (value).i
#define AS_NUMBER(value)   (value).d
#define AS_PTR(value)      (value).ptr
#define AS_UINT(value)     (value).u

#define BOOL_VAL(value)    ((VmValue){.b = (value)})
#define INT_VAL(value)     ((VmValue){.i = (value)})
#define NUMBER_VAL(value)  ((VmValue){.d = (value)})
#define PTR_VAL(object)    ((VmValue){.ptr = (void*)(object)})
#define UINT_VAL(value)    ((VmValue){.u = (value)})


#define NIL_VAL UINT_VAL(UINT_MAX)

#endif /* NAN-BOXING */


#define AS_IDX(value)      AS_INT(value) // WARNING: rhp_idx value
#define IDX_VAL(value)     INT_VAL(value)


typedef struct vm_gms_sym_iterator {
   bool compact;
   IdentData symbol;
   int uels[]; // Clang-18 fails to understand __counted_by(symbol.dim)
} VmGmsSymIterator;

const char* vmval_typename(VmValue vmval);
int vmval_is_regentry(VmValueArray * vmvals, unsigned idx) NONNULL;
int vmval_is_arcobj(VmValueArray * vmvals, unsigned idx) NONNULL;
int vmval_is_gmssymiter(VmValueArray * vmvals, unsigned idx) NONNULL;


static inline VmGmsSymIterator * getgmssymiter(VmValueArray * globals, unsigned gidx)
{
   assert(gidx < globals->len);
   assert(IS_GMSSYMITER(globals->arr[gidx]));

   return AS_GMSSYMITER(globals->arr[gidx]);
}

static inline DagLabels* getdaglabels(VmValueArray * globals, unsigned gidx)
{
   assert(gidx < globals->len);
   assert(IS_ARCOBJ(globals->arr[gidx]));

   return AS_ARCOBJ(globals->arr[gidx]);
}

static inline DagRegisterEntry* getregentry(VmValueArray * globals, unsigned gidx)
{
   assert(gidx < globals->len);
   assert(IS_REGENTRY(globals->arr[gidx]));

   return AS_REGENTRY(globals->arr[gidx]);
}

#endif

