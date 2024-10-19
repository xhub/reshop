/* This code is inspired by the wonderful clox interpreter developed
 * in the book 'Crafting Interpreters', which falls under the MIT LICENSE.
 * This file is then redistributed under the same terms, that is:

    Permission is hereby granted, free of charge, to any person obtaining a copy
    of this software and associated documentation files (the "Software"), to
    deal in the Software without restriction, including without limitation the
    rights to use, copy, modify, merge, publish, distribute, sublicense, and/or
    sell copies of the Software, and to permit persons to whom the Software is
    furnished to do so, subject to the following conditions:

    The above copyright notice and this permission notice shall be included in
    all copies or substantial portions of the Software.

    THE SOFTWARE IS PROVIDED "AS IS", WITHOUT WARRANTY OF ANY KIND, EXPRESS OR
    IMPLIED, INCLUDING BUT NOT LIMITED TO THE WARRANTIES OF MERCHANTABILITY,
    FITNESS FOR A PARTICULAR PURPOSE AND NONINFRINGEMENT. IN NO EVENT SHALL THE
    AUTHORS OR COPYRIGHT HOLDERS BE LIABLE FOR ANY CLAIM, DAMAGES OR OTHER
    LIABILITY, WHETHER IN AN ACTION OF CONTRACT, TORT OR OTHERWISE, ARISING
    FROM, OUT OF OR IN CONNECTION WITH THE SOFTWARE OR THE USE OR OTHER DEALINGS
    IN THE SOFTWARE.

*/

#ifndef EMPPARSER_VM_H
#define EMPPARSER_VM_H

#include <stdbool.h>
#include <stdint.h>

#include "compat.h"
#include "empinterp.h"
#include "equ.h"
#include "reshop_data.h"
#include "var.h"

typedef enum OpCode {
   OP_PUSH_GIDX,        // 1 arg: 16 bits offset
   OP_PUSH_BYTE,         // 1 arg: next instr
   OP_PUSH_LIDX,         // 1 arg: lidx
   OP_PUSH_VMUINT,        // 1 arg: 16 bits index
   OP_PUSH_VMINT,         // 1 arg: 16 bits index
   OP_UPDATE_LOOPVAR,
   OP_POP,
   OP_NIL,
   OP_TRUE,
   OP_FALSE,
   OP_GMSSYMITER_SETFROM_LOOPVAR,  // 2 arg: local idx and gmd filter idx
   OP_REGENTRY_SETFROM_LOOPVAR,  // 2 arg: local idx and gmd filter idx
   OP_LOCAL_COPYFROM_GIDX,
   OP_LOCAL_COPYOBJLEN,
   OP_LOCAL_COPYTO_LOCAL,
   OP_LOCAL_INC,
   OP_STACKTOP_COPYTO_LOCAL,
   OP_LSET_ADD,
   OP_LSET_RESET,
   OP_LVEC_ADD,
   OP_LVEC_RESET,
   OP_VECTOR_EXTEND,
   OP_OVFPARAM_SYNC,
   OP_EQUAL,                      // pops 2 arg
   OP_GREATER,
   OP_LESS,
   OP_ADD,
   OP_SUBTRACT,
   OP_MULTIPLY,
   OP_DIVIDE,
   OP_NOT,
   OP_NEGATE,
   OP_JUMP,
   OP_JUMP_IF_TRUE,          // 1 arg: 16 bits offset
   OP_JUMP_IF_FALSE,         // 1 arg: 16 bits offset
   OP_JUMP_IF_TRUE_NOPOP,    // 1 arg: 16 bits offset
   OP_JUMP_IF_FALSE_NOPOP,   // 1 arg: 16 bits offset
   OP_JUMP_BACK,             // 1 arg: 16 bits offset
   OP_JUMP_BACK_IF_FALSE,    // 1 arg: 16 bits offset
   OP_GMS_EQUVAR_READ_INIT,       // 1 arg: IdentType
   OP_GMS_EQUVAR_READ_SYNC,       // 1 arg: IdentType
   OP_GMS_SYMBOL_READ_SIMPLE, // 1 arg: a global idx
   OP_GMS_SYMBOL_READ_EXTEND,    // 1 arg: a global idx
   OP_GMS_MEMBERSHIP_TEST,   // 1 arg: a global idx; PUSH a BOOL on the stack
   OP_EMPAPI_CALL,           // 2 args: api_idx in enum EmpApi and argc
   OP_NEW_OBJ,               // 1 arg is in enum EmpNewObj
   OP_LINKLABELS_INIT,
   OP_LINKLABELS_DUP,
   OP_LINKLABELS_KEYWORDS_UPDATE,
   OP_LINKLABELS_SETFROM_LOOPVAR,
   OP_LINKLABELS_STORE,
   OP_LINKLABELS_FINI,
   OP_DUAL_LABELS_STORE,
   OP_SCALAR_SYMBOL_TRACKER_INIT,
   OP_SCALAR_SYMBOL_TRACKER_CHECK,
   OP_SET_DAGUID_FROM_REGENTRY,
   OP_HACK_SCALAR2VMDATA,
   OP_END,
   OP_MAXCODE,
} EmpVmOpCode;


#define NAN_BOXING


#ifdef NAN_BOXING

typedef uint64_t VmValue;

#else

typedef union {
   bool b;
   char c;
   unsigned char uc;
   int i;
   unsigned u;
   double d;
   void *ptr;
} VmValue;

#endif




/* This is a hack to make sure the compiler and the VM read the same data */
#define READ_GIDX      READ_SHORT
#define GIDX_TYPE      uint16_t
#define GIDX_MAX       UINT16_MAX
#define EMIT_GIDX(tape, gidx)   emit_short(tape, gidx); assert((gidx) < GIDX_MAX);

#define INSTR_MAX      UINT8_MAX
#define LIDX_TYPE      uint8_t
#define LIDX_MAX       UINT8_MAX


enum CstDefaults {
   CstZeroUInt,
   CstZeroInt,
};

/* The next two num should be kept in sync with parser_ops:
 * their union cover all functions there
 */

enum EmpApi {
   FN_CCFLIB_FINALIZE,
   FN_CCFLIB_UPDATEPARAMS,
   FN_CTR_MARKEQUASFLIPPED,
   FN_MP_ADDCONS,
   FN_MP_ADDVARS,
   FN_MP_ADDVIPAIRS,
   FN_MP_ADDZEROFUNC,
   FN_MP_ADD_MAP_OBJFN,
   FN_MP_FINALIZE,
   FN_MP_SETOBJVAR,
   FN_MP_SETPROBTYPE,
   FN_NASH_ADDMPBYID,
   FN_NASH_FINALIZE,
   FN_OVF_ADDARG,
   FN_OVF_FINALIZE,
   FN_OVF_SETRHO,
   FN_OVF_UPDATEPARAMS,
   FN_OVFPARAM_SETDEFAULT,
   FN_OVFPARAM_UPDATE,
};

enum EmpObjNew {
   FN_CCFLIB_NEW,
   FN_MP_NEW,
   FN_NASH_NEW,
   FN_OVF_NEW,
};

typedef struct arcvfobj {
   mpid_t id_parent;
   mpid_t id_child;
} ArcVFObj;


typedef struct arcvfobj_array {
   unsigned len;
   unsigned max;
   ArcVFObj *arr;
} ArcVFObjArray;

#define RHP_LOCAL_SCOPE
#define RHP_ARRAY_PREFIX arcvfobjs
#define RHP_ARRAY_TYPE arcvfobj_array
#define RHP_ELT_TYPE struct arcvfobj
#define RHP_ELT_INVALID ((ArcVFObj){.id_parent = MpId_NA, .id_child = MpId_NA})
#include "array_generic.inc"

/** Help ensure that exactly 1 record of a symbol has been read */
typedef enum {
   ScalarSymbolInactive,  /**<  No tracking is happening    */
   ScalarSymbolZero,      /**< No Symbol has been read yet  */
   ScalarSymbolRead,      /**< A symbol has been read       */
} ScalarSymbolStatus;


typedef struct {
   ScalarSymbolStatus scalar_tracker; /**< Tracker to ensure 1 record is read */

   /* data for reading symbols (equations, variables, sets, parameters) */
   Aequ e;
   Avar v;
   Aequ e_extend;
   Avar v_extend;
   IntScratch e_data;
   IntScratch v_data;
   IntScratch iscratch;
   DblScratch dscratch;
   int ival;
   double dval;
   unsigned inrecs;
   unsigned dnrecs;

   Aequ *e_current;
   Avar *v_current;

   ArcVFObjArray arcvfobjs;
   daguid_t uid_grandparent;    /**< uid of the grand parent node            */
   daguid_t uid_parent;         /**< uid of the parent node                  */
   int *linklabel_ws;  /* why is this not iscratch ?*/

   /* Borrowed data follow */
   Model *mdl;
   void *dct;
   void *gmd;
   Interpreter *interp;
   CompilerGlobals *globals;
   DagRegister *dagregister;
   LinkLabels *linklabels;
   LinkLabels2Arcs *linklabels2arcs;
   LinkLabel2Arc *linklabel2arc;
   DualsLabelArray *dual_labels;
} VmData;

typedef int (*empapi)(VmData *data, unsigned argc, const VmValue *values);
typedef void* (*empnewobj)(VmData *data,  unsigned argc, const VmValue *values);

typedef struct {
   const empapi fn;
   uint8_t argc;
} EmpApiCall;

typedef struct {
   const empnewobj fn;
   unsigned (*get_uid)(void *obj);
   VmValue (*obj2vmval)(void *obj);
   uint8_t argc;
} EmpNewObjCall;

extern const EmpApiCall empapis[];
extern const EmpNewObjCall empnewobjs[];
extern const unsigned empapis_len;
extern const unsigned empnewobjs_len;
extern const char * const empapis_names[];
extern const char * const empnewobjs_names[];

const char* opcodes_name(enum OpCode op);

#define UINT8_COUNT (UINT8_MAX + 1)
#define STACK_MAX   UINT8_COUNT

typedef struct {
   unsigned len;
   unsigned max;
   uint8_t* ip;
   unsigned* line;
} VmCode;

typedef struct VmValueArray {
   unsigned len;
   unsigned max;
   VmValue *arr;
} VmValueArray;

VmValue vmvals_getnil(void);

/* TODO: delete? */
typedef struct {
   unsigned depth;
   void *obj;
} VmObj;

#define RHP_LOCAL_SCOPE
#define RHP_ARRAY_PREFIX vmvals
#define RHP_ARRAY_TYPE VmValueArray
#define RHP_ELT_TYPE VmValue
#define RHP_ELT_INVALID (vmvals_getnil())
#include "array_generic.inc"

typedef struct VmObjArray {
   unsigned len;
   unsigned max;
   void **arr;
} VmObjArray;

#define RHP_LOCAL_SCOPE
#define RHP_ARRAY_PREFIX vmobjs
#define RHP_ARRAY_TYPE VmObjArray
#define RHP_ELT_TYPE void*
#define RHP_ELT_INVALID NULL
#include "array_generic.inc"

/** EMP VM struct */
typedef struct empvm {
   VmValue stack[STACK_MAX];
   VmValue* stack_top;
   VmValue locals[UINT8_COUNT];
   VmValueArray globals;
   VmCode code;
   uint8_t *instr_start;

   VmObjArray newobjs;
   VmObjArray objs;
   UIntArray uints;
   IntArray ints;

   VmData data;
} EmpVm;



void empvm_free(struct empvm *vm);
struct empvm* empvm_new(Interpreter *interp) MALLOC_ATTR(empvm_free, 1) NONNULL;
int empvm_run(struct empvm *vm) NONNULL;
int empvm_dissassemble(EmpVm *vm, unsigned mode) NONNULL;

void print_vmval_short(unsigned mode, VmValue v, EmpVm *vm) NONNULL;


static inline void vmcode_init(VmCode *code)
{
   code->len = 0;
   code->max = 0;
   code->ip = NULL;
   code->line = NULL;
}

static inline int vmcode_resize(VmCode *code, unsigned size)
{
   MALLOC_(code->ip, uint8_t, size);
   MALLOC_(code->line, unsigned, size);
   code->len = 0;
   code->max = size;

   return OK;
}

static inline int vmcode_stripmem(VmCode *code)
{
   if (code->max > code->len) {
      REALLOC_(code->ip, uint8_t, code->len);
      REALLOC_(code->line, unsigned, code->len);
   }

   return OK;
}

static inline int vmcode_add(VmCode *code, uint8_t instr, unsigned linenr)
{

   if (code->len >= code->max) {
      code->max = MAX(2*code->max, code->len+1);
      REALLOC_(code->ip, uint8_t, code->max);
      REALLOC_(code->line, unsigned, code->max);
   }


   code->ip[code->len] = instr;
   code->line[code->len++] = linenr;

   return OK;
}

static inline void vmcode_free(VmCode *code)
{
   FREE(code->ip);
   FREE(code->line);
}

#endif // !EMPPARSER_VM_H
