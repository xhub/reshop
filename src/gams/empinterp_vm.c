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

#include "dctmcc.h"
#include "empdag.h"
#include "empinterp_edgebuilder.h"
#include "empinterp_priv.h"
#include "empinterp_utils.h"
#include "empinterp_vm.h"
#include "empinterp_vm_utils.h"
#include "empinfo.h"
#include "lequ.h"
#include "macros.h"
#include "mathprgm.h"
#include "mdl.h"
#include "ovf_parameter.h"
#include "printout.h"

#include "gclgms.h"
#include "reshop_data.h"


#define READ_BYTE(vm) (*vm->code.ip++)
#define READ_SHORT(vm) \
   (vm->code.ip += 2, \
      (uint16_t)((vm->code.ip[-2] << 8) | vm->code.ip[-1]))

#ifndef NDEBUG
static inline VmValue read_global(EmpVm *vm)
{
   GIDX_TYPE gidx = READ_GIDX(vm);
   assert(gidx < vm->globals.len);
   return vm->globals.list[gidx];
}

static inline unsigned READ_VMUINT(EmpVm *vm)
{
   GIDX_TYPE gidx = READ_GIDX(vm);
   assert(gidx < vm->uints.len);
   return vm->uints.list[gidx];
}

//#define DEBUGVMRUN(fmt, ...) \
//printout(PO_TRACE_EMPINTERP, "[%s] " fmt, opcodes_name(instr), __VA_ARGS__);

#define DEBUGVMRUN(...) \
printout(PO_TRACE_EMPINTERP, __VA_ARGS__);
#define DEBUGVMRUN_EXEC(EXPR) EXPR
#else

#define read_global(vm) (vm->globals.list[READ_GIDX(vm)])
#define READ_VMINT(vm)  (vm->ints.list[READ_GIDX(vm)])
#define DEBUGVMRUN(...) 
#define DEBUGVMRUN_EXEC(EXPR)
#endif


#define MY_NOP(x) x

/* ---------------------------------------------------------------------
 * We compare (after casting) the values and save the result on the stack
 * as a boolean.
 * --------------------------------------------------------------------- */
#define BINARY_CMP(vm, valueType, op) \
    do { \
      push(vm, BOOL_VAL(valueType(pop(vm)) op valueType(pop(vm)))); \
    } while (false)

#define BINARY_OP(vm, op) \
    do { \
      push(vm, NUMBER_VAL(AS_NUMBER((pop(vm)) op AS_NUMBER(pop(vm))))); \
    } while (false)


static inline bool identisequvar(IdentType type)
{
   if (type == IdentVar || type == IdentEqu) return true;

   return false;
}

static inline void print_vmval_full(VmValue val_, EmpVm *vm)
{
   const int pad = 10;
   switch(val_ & MASK_SIGNATURE) {
   case SIGNATURE_VALUES: {
      uint32_t tag = VALUE_TAG(val_);
      switch (tag) {
      case TAG_FALSE:
         trace_empinterp("%*s\n", pad, "FALSE");
         break;
      case TAG_TRUE:
         trace_empinterp("%*s\n", pad, "TRUE");
         break;
      case TAG_NULL:
         trace_empinterp("%*s\n", pad, "NULL");
         break;
      default:
         error("%s :: Unknown TAG value %u", __func__, tag);
      }
      break;
   }
   case SIGNATURE_INTEGER:
      trace_empinterp("%*s: %5d\n", pad, "INT", AS_INT(val_));
      break;
   case SIGNATURE_UINTEGER:
      trace_empinterp("%*s: %5u\n", pad, "UINT", AS_UINT(val_));
      break;
   case SIGNATURE_LOOPVAR: {
      int uel = AS_LOOPVAR(val_);
      char uel_label[GMS_SSSIZE];
      char quote[] = " ";
      if (dctUelLabel(vm->data.dct, uel, quote, uel_label, sizeof(uel_label))) {
         strcpy(uel_label, "ERROR getting LOOPVAR");
      }
      trace_empinterp("%*s: %5d %s%s%s\n", pad, "LOOPVAR", uel, quote, uel_label, quote);
      break;
   }
   case SIGNATURE_STRING:
      trace_empinterp("%*s: %s\n", pad, "STR", AS_STR(val_));
      break;
   case SIGNATURE_MPOBJ: {
      const MathPrgm *mp = AS_PTR(val_);
      if (!mp) {
         trace_empinterpmsg("   MP: NULL!\n");
         break;
      } 
      unsigned mp_id = mp_getid(mp);
      trace_empinterp("%*s: '%s'\n", pad, "MP",
                      empdag_getmpname(&vm->data.mdl->empinfo.empdag, mp_id));
      break;
   }
   case SIGNATURE_MPEOBJ: {
      const Mpe *mpe = AS_PTR(val_);
      if (!mpe) {
         trace_empinterpmsg("  MPE: NULL!\n");
         break;
      } 
      unsigned mpe_id = mpe_getid(mpe);
      trace_empinterp("%*s: '%s'\n", pad, "MPE",
                      empdag_getmpename(&vm->data.mdl->empinfo.empdag, mpe_id));
      break;
   }
   case SIGNATURE_OVFOBJ: {
      const OvfDef *ovf = AS_PTR(val_);
      if (!ovf) {
         trace_empinterpmsg("  OVF: NULL!\n");
         break;
      } 
      unsigned ovf_id = ovf->idx;
      trace_empinterp("%*s: id %5d '%s'\n", pad, "OVF", ovf_id, ovf->name);
      break;
   }
   case SIGNATURE_POINTER:
      trace_empinterp("%*s: %p\n", pad, "PTR", AS_PTR(val_));
      break;
   case SIGNATURE_REGENTRY: {
      DagRegisterEntry *regentry = AS_REGENTRY(val_);
      trace_empinterp("%*s: %.*s\n", pad, "RegEntry", regentry->basename_len, regentry->basename);
      break;
   }
   case SIGNATURE_EDGEOBJ: {
      DagLabels *edgeobj = AS_EDGEOBJ(val_);
      trace_empinterp("%*s: %.*s\n", pad, "EdgeObj", edgeobj->basename_len, edgeobj->basename);
      break;
   }
   case SIGNATURE_GMSSYMITER: {
      VmGmsSymIterator *symiter = AS_GMSSYMITER(val_);
      Lexeme *lexeme = &symiter->symbol.lexeme;
      trace_empinterp("%*s: %.*s\n", pad, "GmsSymIterator", lexeme->len, lexeme->start);
      break;
   }
   default: ;
   }

}

void print_vmval_short(unsigned mode, VmValue v, EmpVm *vm)
{
   switch(v & MASK_SIGNATURE) {
   case SIGNATURE_VALUES: {
      uint32_t tag = VALUE_TAG(v);
      switch (tag) {
      case TAG_FALSE:
         printstr(mode, " FALSE ");
         break;
      case TAG_TRUE:
         printstr(mode, " TRUE ");
         break;
      case TAG_NULL:
         printstr(mode, " NULL ");
         break;
      default:
         error("%s :: Unknown TAG value %u", __func__, tag);
      }
      break;
   }
   case SIGNATURE_INTEGER:
      printout(mode, "%20d ", AS_INT(v));
      break;
   case SIGNATURE_UINTEGER:
      printout(mode, "%20u ", AS_UINT(v));
      break;
   case SIGNATURE_LOOPVAR: {
      int uel = AS_LOOPVAR(v);
      char uel_label[GMS_SSSIZE];
      char quote[] = " ";
      if (dctUelLabel(vm->data.dct, uel, quote, uel_label, sizeof(uel_label))) {
         strcpy(uel_label, "ERROR getting LOOPVAR");
      }
      printout(mode, " %s%s%s ", quote, uel_label, quote);
      break;
   }
   case SIGNATURE_STRING:
      printout(mode, " %s ", AS_STR(v));
      break;
   case SIGNATURE_MPOBJ: {
      const MathPrgm *mp = AS_PTR(v);
      if (!mp) {
         printstr(mode, "   MP: NULL!\n");
         break;
      } 
      unsigned mp_id = mp_getid(mp);
      printout(mode, " MP('%s') ", empdag_getmpname(&vm->data.mdl->empinfo.empdag, mp_id));
      break;
   }
   case SIGNATURE_MPEOBJ: {
      const Mpe *mpe = AS_PTR(v);
      if (!mpe) {
         printstr(mode, " MPE: NULL!\n");
         break;
      } 
      unsigned mpe_id = mpe_getid(mpe);
      printout(mode, " MPE('%s') ", empdag_getmpename(&vm->data.mdl->empinfo.empdag, mpe_id));
      break;
   }
   case SIGNATURE_OVFOBJ: {
      const OvfDef *ovf = AS_PTR(v);
      if (!ovf) {
         printstr(mode, "  OVF: NULL!\n");
         break;
      } 
      unsigned ovf_id = ovf->idx;
      printout(mode, " OVF('%s') ", ovf->name);
      break;
   }
   case SIGNATURE_POINTER:
      printout(mode, "%14p (PTR)", AS_PTR(v));
      break;
   case SIGNATURE_REGENTRY: {
      DagRegisterEntry *regentry = AS_REGENTRY(v);
      printout(mode, "%20.*s", regentry->basename_len, regentry->basename);
      break;
   }
   case SIGNATURE_EDGEOBJ: {
      DagLabels *edgeobj = AS_EDGEOBJ(v);
      printout(mode, "%20.*s", edgeobj->basename_len, edgeobj->basename);
      break;
   }
   case SIGNATURE_GMSSYMITER: {
      VmGmsSymIterator *symiter = AS_GMSSYMITER(v);
      Lexeme *lexeme = &symiter->symbol.lexeme;
      printout(mode, "%20.*s", lexeme->len, lexeme->start);
      break;
   }
   default: ;
   }

}

#define PADDING_VERBOSE 60

static int getpadding(int offset)
{
   return offset < PADDING_VERBOSE ? PADDING_VERBOSE - offset : 0;
}

static void print_symiter(VmGmsSymIterator *symiter, EmpVm *vm)
{
   Lexeme *lexeme = &symiter->symbol.lexeme;
   DEBUGVMRUN("%.*s", lexeme->len, lexeme->start);
   int dim = symiter->symbol.dim;
   if (dim > 0) {
      int *uels = symiter->uels;
      DEBUGVMRUN("%s", "(");
      for (unsigned ii = 0; ii < dim; ++ii) {
         if (ii > 1) { DEBUGVMRUN("%s", ", "); }
         int dummyoffset;
         dct_printuel(vm->data.dct, uels[ii], PO_TRACE_EMPINTERP,
                      &dummyoffset);
      }
      DEBUGVMRUN("%s", ")");
   }
}

/* ----------------------------------------------------------------------
 * Start VM calls
 * ---------------------------------------------------------------------- */





NONNULL static inline void push(struct empvm *vm, VmValue val) {
   if (O_Output & PO_TRACE_EMPINTERP) {
      trace_empinterp("pushed     ");
      print_vmval_full(val, vm);
   }
   *vm->stack_top = val;
   vm->stack_top++;
}

NONNULL static inline VmValue pop(struct empvm *vm) {
  vm->stack_top--;
  assert(vm->stack_top >= vm->stack);
  DEBUGVMRUN_EXEC(print_vmval_full(*vm->stack_top, vm));
  return *vm->stack_top;
}

NONNULL static inline void stack_mvback(struct empvm *vm, unsigned nb) {
  assert(vm->stack_top >= vm->stack+nb);
  vm->stack_top -= nb;
}

static inline VmValue vmpeek(struct empvm *vm, unsigned distance) {
  assert(vm->stack_top - vm->stack >= 1 + distance);
  return vm->stack_top[-(ptrdiff_t)(1 + distance)];
}

static unsigned getlinenr(struct empvm *vm)
{
   size_t offset = vm->code.ip - vm->instr_start;
   if (offset >= vm->code.len) {
      assert(0);
      return UINT_MAX;
   }
   return vm->code.line[offset];
}

static void _print_uels(dctHandle_t dct, unsigned mode, int *uels, unsigned nuels)
{
   char uel_label[GMS_SSSIZE];
   char quote[] = " ";

   for (unsigned i = 0; i < nuels; ++i) {
      if (dctUelLabel(dct, uels[i], quote, uel_label, sizeof(uel_label))) {
         strcpy(uel_label, "ERROR getting UEL");
      }
      printout(mode, "[%5d]\t%s%s%s", uels[i], quote, uel_label, quote);
   }
}

static int gms_resolve_symb(VmData *vmdata, VmGmsSymIterator *symiter)
{
   IdentType type = symiter->symbol.type;
   assert(identisequvar(type));

   DctResolveData data;

   data.type = GmsSymIteratorTypeVm;
   data.symiter.vm = symiter;

   switch (type) {
   case IdentVar:
      data.payload.v = &vmdata->v;
      data.scratch = &vmdata->v_data;
      vmdata->v_current = &vmdata->v;
      break;
   case IdentEqu:
      data.payload.e = &vmdata->e;
      data.scratch = &vmdata->e_data;
      vmdata->e_current = &vmdata->e;
      break;
   default:
      error("%s :: unsupported token '%s'", __func__, identtype_str(type));
      return Error_EMPRuntimeError;
   }


   dctHandle_t dct = vmdata->dct;

   int status = dct_resolve(dct, &data);

   if (status != OK) {
      int idx = (int)symiter->symbol.idx;
      int nuels = dctSymDim(dct, idx);
      if (nuels < 0) {
         error("[empvm_run] Could not query symbol dimension for idx #%d",
               idx);
      } else {
         _print_uels(dct, PO_ERROR, symiter->uels, symiter->symbol.dim);
      }
   }

   return status;
}

static int gms_resolve_extend_symb(VmData *vmdata, VmGmsSymIterator *filter)
{
   IdentType type = filter->symbol.type;
   assert(identisequvar(type));

   S_CHECK(gms_resolve_symb(vmdata, filter));

   switch (type) {
   case IdentVar:
      return avar_extend(&vmdata->v_extend, &vmdata->v);
   case IdentEqu:
      return aequ_extendandown(&vmdata->e_extend, &vmdata->e);
   default:
      error("%s :: unsupported token '%s'", __func__, identtype_str(type));
      return Error_EMPRuntimeError;
   }
}

static int gms_extend_init(VmData *vmdata, IdentType type)
{
   assert(identisequvar(type));

   switch (type) {
   case IdentVar:
      avar_reset(&vmdata->v_extend);
      break;
   case IdentEqu:
      aequ_reset(&vmdata->e_extend);
      break;
   default:
      error("%s :: unsupported token '%s'", __func__, identtype_str(type));
      return Error_EMPRuntimeError;
   }

   return OK;
}

static int gms_extend_sync(VmData *vmdata, IdentType type)
{
   assert(identisequvar(type));

   switch (type) {
   case IdentVar:
      vmdata->v_current = &vmdata->v_extend;
      DEBUGVMRUN_EXEC(avar_printnames(vmdata->v_current, PO_TRACE_EMPINTERP, vmdata->mdl));
      break;
   case IdentEqu:
      vmdata->e_current = &vmdata->e_extend;
      DEBUGVMRUN_EXEC(aequ_printnames(vmdata->e_current, PO_TRACE_EMPINTERP, vmdata->mdl));
      break;
   default:
      error("%s :: unsupported token '%s'", __func__, identtype_str(type));
      return Error_EMPRuntimeError;
   }

   return OK;
}

static int gms_membership_test(UNUSED VmData *vmdata, VmGmsSymIterator *symiter,
                               bool *res)
{
   IdentType type = symiter->symbol.type;

   switch (type) {
   case IdentMultiSet:
      assert(symiter->symbol.ptr);
      return gdx_reader_boolean_test(symiter->symbol.ptr, symiter, res);
   case IdentLocalSet:
   case IdentSet: {
      IntArray *obj = symiter->symbol.ptr;
      assert(obj && valid_set(*obj));
      int uel = symiter->uels[0];
      unsigned idx = rhp_int_find(obj, uel);
      *res = idx < UINT_MAX;
      return OK;
   }
   default:
      error("[empvm_run] unexpected runtime error: no membership test for ident '%s'",
            identtype_str(type));
      return Error_EMPRuntimeError;
   }


}

struct empvm* empvm_new(Interpreter *interp)
{
   struct empvm* vm;
   MALLOC_NULL(vm, struct empvm, 1);

   vm->stack_top = vm->stack;
   vmvals_resize(&vm->globals, 10);
   vmcode_resize(&vm->code, 100);

   vmobjs_init(&vm->newobjs);
   vmobjs_init(&vm->objs);

   rhp_uint_init(&vm->uints);
   rhp_int_init(&vm->ints);

   vm->data.uid_grandparent = EMPDAG_UID_NONE;
   vm->data.uid_parent = EMPDAG_UID_NONE;
   aequ_init(&vm->data.e);
   avar_init(&vm->data.v);
   aequ_setblock(&vm->data.e_extend, 3);
   avar_setblock(&vm->data.v_extend, 3);
   scratchint_init(&vm->data.e_data);
   scratchint_init(&vm->data.v_data);
   scratchdbl_init(&vm->data.dscratch);
   scratchdbl_ensure(&vm->data.dscratch, 10);
   vm->data.e_current = NULL;
   vm->data.v_current = NULL;

   edgevfobjs_init(&vm->data.edgevfobjs);

   vm->data.mdl = interp->mdl;
   vm->data.globals = &interp->globals;
   vm->data.dagregister = &interp->dagregister;
   vm->data.daglabels = NULL;
   vm->data.daglabels_ws = NULL;
   vm->data.labels2edges = &interp->labels2edges;
   vm->data.label2edge = &interp->label2edge;

   vmvals_add(&vm->globals, UINT_VAL(0));
   vmvals_add(&vm->globals, INT_VAL(0));

   vm->data.dct = interp->dct;


   return vm;
}

void empvm_free(struct empvm* vm)
{
   if (!vm) return;

   /* ---------------------------------------------------------------------
    * All objects in globals below to the VM and must be freed
    * --------------------------------------------------------------------- */

   for (unsigned i = 0, len = vm->globals.len; i < len; ++i) {
      VmValue val = vm->globals.list[i];
      if (IS_PTR(val)) {
         void *obj = AS_PTR(val);
         FREE(obj);
      }
   }

   vmvals_free(&vm->globals);
   rhp_uint_empty(&vm->uints);
   rhp_int_empty(&vm->ints);

   aequ_empty(&vm->data.e);
   avar_empty(&vm->data.v);
   aequ_empty(&vm->data.e_extend);
   avar_empty(&vm->data.v_extend);
   scratchint_empty(&vm->data.e_data);
   scratchint_empty(&vm->data.v_data);
   scratchdbl_empty(&vm->data.dscratch);
}

int empvm_run(struct empvm *vm)
{
   S_CHECK(empvm_dissassemble(vm, PO_TRACE_EMPINTERP));

   int status = OK;
   vm->instr_start = vm->code.ip;
   vm->stack_top = vm->stack;

   trace_empinterp("\n\n");

   while (true) {

      assert(vm->code.ip -  vm->instr_start < vm->code.len);

      uint8_t instr = READ_BYTE(vm);


      /* ---------------------------------------------------------------------
      * Display the stack 
      * --------------------------------------------------------------------- */

      if (O_Output & PO_TRACE_EMPINTERP) {


         if (vm->stack_top > vm->stack) {
            int offset;
            trace_empinterp("stack%n", &offset);
            for (VmValue *val = vm->stack; val < vm->stack_top; val++, offset = 0) {
               trace_empinterp("%*s", 50-offset, "");
               print_vmval_full(*val, vm);
            }
         }

         trace_empinterp("[%5ld] %30s%10s",
                         (vm->code.ip - vm->instr_start) - 1, opcodes_name(instr), "");
      }

      switch (instr) {
      case OP_PUSH_GIDX: {
         VmValue val = read_global(vm);
         push(vm, val);
         break;
      }
      case OP_PUSH_BYTE: {
         uint8_t val = READ_BYTE(vm);
         push(vm, UINT_VAL(val));
         break;
      }
      case OP_PUSH_LIDX: {
         uint8_t slot = READ_BYTE(vm);
         push(vm, vm->locals[slot]);
         break;
      }
      case OP_PUSH_VMUINT: {
         GIDX_TYPE slot = READ_GIDX(vm);
         push(vm, UINT_VAL(vm->uints.list[slot]));
         break;
      }
      case OP_PUSH_VMINT: {
         GIDX_TYPE slot = READ_GIDX(vm);
         push(vm, INT_VAL(vm->ints.list[slot]));
         break;
      }
      case OP_UPDATE_LOOPVAR: {
         uint8_t lidx_loopvar = READ_BYTE(vm);
         uint8_t lidx_idxvar = READ_BYTE(vm);
         IdentType type = READ_BYTE(vm);
         GIDX_TYPE gidx = READ_GIDX(vm);

         unsigned idx = AS_UINT(vm->locals[lidx_idxvar]);
         int offset0, offset1, offset2;
         DEBUGVMRUN("loopvar@%u of %n", lidx_loopvar, &offset0);

         IntArray set;
         switch (type) {
         case IdentLocalSet: {
            assert(gidx < vm->data.globals->localsets.len);
            set = namedints_at(&vm->data.globals->localsets, gidx);
            DEBUGVMRUN("'%s' <- %n", vm->data.globals->localsets.names[gidx], &offset1);
            break;
         }
         case IdentSet: {
            assert(gidx < vm->data.globals->sets.len);
            set = namedints_at(&vm->data.globals->sets, gidx);
            DEBUGVMRUN("'%s' <- %n", vm->data.globals->sets.names[gidx], &offset1);
            break;
         }
         default:
            error("[empvm_run] in %s: unexpected ident type %s", opcodes_name(instr),
                  identtype_str(type));
            goto _exit;
         }

         assert(valid_set(set) && idx < set.len);
         vm->locals[lidx_loopvar] = LOOPVAR_VAL(set.list[idx]);
         DEBUGVMRUN_EXEC({dct_printuel(vm->data.dct, (set.list[idx]), PO_TRACE_EMPINTERP, &offset2);});
         DEBUGVMRUN("%*s%s#%u[lvar%u = %u]\n", getpadding(offset0 + offset1 + offset2), "",
                    type == IdentSet ? "sets" : "localsets", gidx, lidx_idxvar, idx);
         break;
      }
      case OP_LOCAL_COPYFROM_GIDX: {
         uint8_t slot = READ_BYTE(vm);
         vm->locals[slot] = read_global(vm);
         DEBUGVMRUN("lvar@%u = %lu\n", slot, vm->locals[slot] & MASK_PAYLOAD_32);
         break;
      }
      case OP_LOCAL_COPYOBJLEN: {
         uint8_t slot = READ_BYTE(vm);
         IdentType type = READ_BYTE(vm);
         GIDX_TYPE gidx = READ_GIDX(vm);

         unsigned len;
         switch (type) {
         case IdentLocalSet: {
            IntArray obj = namedints_at(&vm->data.globals->localsets, gidx);
            assert(valid_set(obj));
            len = obj.len;
            DEBUGVMRUN("objname is %s of type localset\n", vm->data.globals->localsets.names[gidx]);
            break;
         }
         case IdentLocalVector: {
            Lequ obj = namedvec_at(&vm->data.globals->localvectors, gidx);
            assert(valid_vector(obj));
            len = obj.len;
            DEBUGVMRUN("objname is %s of type localvector\n", vm->data.globals->localvectors.names[gidx]);
            break;
         }
         case IdentSet: {
            IntArray obj = namedints_at(&vm->data.globals->sets, gidx);
            assert(valid_set(obj));
            len = obj.len;
            DEBUGVMRUN("objname is %s of type set\n", vm->data.globals->sets.names[gidx]);
            break;
         }
         case IdentVector: {
            Lequ obj = namedvec_at(&vm->data.globals->vectors, gidx);
            assert(valid_vector(obj));
            len = obj.len;
            DEBUGVMRUN("objname is %s of type vector\n", vm->data.globals->vectors.names[gidx]);
            break;
         }
         default:
            error("[empvm_run] in %s: unexpected ident type %s", opcodes_name(instr),
                  identtype_str(type));
            goto _exit;
         }
         vm->locals[slot] = UINT_VAL(len);
         DEBUGVMRUN("lvar@%u <- %u = len(obj[%u])\n", slot, len, gidx);
         break;

      }

      case OP_GMSSYMITER_SETFROM_LOOPVAR: {
         GIDX_TYPE gidx = READ_GIDX(vm);
         uint8_t idx = READ_BYTE(vm);
         uint8_t lidx = READ_BYTE(vm);

         VmGmsSymIterator *symiter;
         N_CHECK_EXIT(symiter, getgmssymiter(&vm->globals, gidx));

         assert(idx < symiter->symbol.dim);
         assert(IS_LOOPVAR(vm->locals[lidx]));

         int uel = AS_LOOPVAR(vm->locals[lidx]);
         symiter->uels[idx] = uel;

         if (uel > 0) symiter->compact = false;
         int offset1, offset2;
         Lexeme *lexeme = &symiter->symbol.lexeme;
         DEBUGVMRUN("%.*s[%u] <- %n", lexeme->len, lexeme->start, idx, &offset1);
         DEBUGVMRUN_EXEC({dct_printuel(vm->data.dct, uel, PO_TRACE_EMPINTERP, &offset2);})
         int pad = getpadding(offset1 + offset2); 
         DEBUGVMRUN("%*sloopvar@%u\n", pad, "", lidx);


         break;
      }
      case OP_EDGEOBJ_SETFROM_LOOPVAR: {
         GIDX_TYPE gidx = READ_GIDX(vm);
         uint8_t idx = READ_BYTE(vm);
         uint8_t lidx = READ_BYTE(vm);

         DagLabels *dagl;
         N_CHECK_EXIT(dagl, getdaglabels(&vm->globals, gidx));

         assert(idx < dagl->dim);
         assert(IS_LOOPVAR(vm->locals[lidx]));

         int uel = AS_LOOPVAR(vm->locals[lidx]);
         dagl->data[idx] = uel;

         int offset1, offset2;
         DEBUGVMRUN("%.*s[%u] <- %n", dagl->basename_len, dagl->basename, idx, &offset1);
         DEBUGVMRUN_EXEC({dct_printuel(vm->data.dct, uel, PO_TRACE_EMPINTERP, &offset2);})
         DEBUGVMRUN("%*sloopvar@%u\n", getpadding(offset1 + offset2), "", lidx);

         break;
      }
      case OP_REGENTRY_SETFROM_LOOPVAR: {
         GIDX_TYPE gidx = READ_GIDX(vm);
         uint8_t idx = READ_BYTE(vm);
         uint8_t lidx = READ_BYTE(vm);

         DagRegisterEntry *regentry;
         N_CHECK_EXIT(regentry, getregentry(&vm->globals, gidx));

         assert(idx < regentry->dim);
         assert(IS_LOOPVAR(vm->locals[lidx]));

         int uel = AS_LOOPVAR(vm->locals[lidx]);
         regentry->uels[idx] = uel;

         int offset1, offset2; 
         DEBUGVMRUN("%.*s[%u] <- %n", regentry->basename_len,
                    regentry->basename, idx, &offset1);
         DEBUGVMRUN_EXEC({dct_printuel(vm->data.dct, uel, PO_TRACE_EMPINTERP, &offset2);})
         DEBUGVMRUN("%*sloopvar@%u\n", getpadding(offset1 + offset2), "", lidx);

         break;
       }
      case OP_LOCAL_COPYTO_LOCAL: {
         uint8_t src = READ_BYTE(vm);
         uint8_t dst = READ_BYTE(vm);
         vm->locals[dst] = vm->locals[src];
         break;
      }
      case OP_LOCAL_INC: {
         uint8_t lidx = READ_BYTE(vm);
         assert(IS_UINT(vm->locals[lidx]));
         VM_VALUE_INC(vm->locals[lidx]);
         DEBUGVMRUN("lidx@%u <- %u\n", lidx, AS_UINT(vm->locals[lidx]));
         break;
      }
      case OP_STACKTOP_COPYTO_LOCAL: {
         uint8_t lidx = READ_BYTE(vm);
         vm->locals[lidx] = vmpeek(vm, 0);
         DEBUGVMRUN("lidx@%u <- %u\n", lidx, AS_UINT(vm->locals[lidx]));
         break;
      }
      case OP_LSET_ADD: {
         GIDX_TYPE gidx = READ_GIDX(vm);
         uint8_t slot = READ_BYTE(vm);
         assert(gidx < vm->data.globals->localsets.len);
         IntArray *set = &vm->data.globals->localsets.list[gidx];

         assert(valid_set(*set));
         assert(IS_LOOPVAR(vm->locals[slot]));

         int val = AS_INT(vm->locals[slot]);
         S_CHECK(rhp_int_add(set, val));
         break;
      }
      case OP_LSET_RESET: {
         GIDX_TYPE gidx = READ_GIDX(vm);
         assert(gidx < vm->data.globals->localsets.len);
         IntArray *set = &vm->data.globals->localsets.list[gidx];
         assert(valid_set(*set));
         set->len = 0;
         break;
      }
      case OP_LVEC_ADD: {
         TO_IMPLEMENT("OP_GVEC_ADD");
      }
      case OP_LVEC_RESET: {
         GIDX_TYPE gidx = READ_GIDX(vm);
         assert(gidx < vm->data.globals->vectors.len);
         Lequ *vec = &vm->data.globals->vectors.list[gidx];
         assert(valid_vector(*vec));
         vec->len = 0;
         break;
      }
      case OP_OVFPARAM_SYNC: {
         unsigned vmvec_lidx = READ_BYTE(vm);
         unsigned param_gidx = READ_GIDX(vm);

         assert(vm->data.dscratch.data);
         assert(param_gidx < vm->globals.len);

         VmVector * restrict vmvec = AS_OBJ(vm->locals[vmvec_lidx]);
         OvfParam * param = AS_OBJ(vm->globals.list[param_gidx]);
         unsigned size = vmvec->len;
         param->type = ARG_TYPE_VEC;
         param->size_vector = size;
         MALLOC_(param->vec, double, size);
         memcpy(param->vec, vm->data.dscratch.data, size * sizeof(double));

         /* Reset the length for the next use */
         vmvec->len = 0;
         
         DEBUGVMRUN("param %p has now value\n", (void*)param);
         DEBUGVMRUN_EXEC({ for (unsigned i = 0; i < size; ++i) { trace_empinterp("[%5u] %e\n", i, param->vec[i]); }});
         break;
      }

      case OP_VECTOR_EXTEND: {
         unsigned vmvec_lidx = READ_BYTE(vm);
         unsigned vecidx_lidx = READ_BYTE(vm);

         assert(IS_PTR(vm->locals[vmvec_lidx]));
         assert(IS_LOOPVAR(vm->locals[vecidx_lidx]));

         VmVector * restrict vmvec = AS_OBJ(vm->locals[vmvec_lidx]);
         unsigned len = vmvec->len;
         DblScratch *dscratch = &vm->data.dscratch;
         assert(dscratch->data);

         if (len >= dscratch->size) {
            unsigned size = MAX(2*dscratch->size, len+1);
            REALLOC_EXIT(dscratch->data, double, len);
            dscratch->size = size;
         }

         unsigned pos;
         int vecidx = AS_LOOPVAR(vm->locals[vecidx_lidx]);
         lequ_find(vmvec->data, vecidx, &dscratch->data[len], &pos);

         if (pos == UINT_MAX || !isfinite(dscratch->data[len])) {
            char buf[GMS_SSSIZE] = " ";
            char quote = ' ';
            
            dctUelLabel(vm->data.dct, vecidx, &quote, buf, sizeof(buf));
            error("[empvm_run] runtime error: in '%.*s', no UEL %c%s%c (#%u)\n",
                  vmvec->lexeme.len, vmvec->lexeme.start, quote, buf, quote, vecidx);
            print_vector(vmvec->data, PO_ERROR, vm->data.dct);
            status = Error_EMPIncorrectInput;
            goto _exit;
         }
         DEBUGVMRUN("vec[%u] <- %e = %.*s(", len, dscratch->data[len],
                    vmvec->lexeme.len, vmvec->lexeme.start);
         DEBUGVMRUN_EXEC({
               int dummyoffset;
               dct_printuel(vm->data.dct, (vecidx), PO_TRACE_EMPINTERP, &dummyoffset);
               trace_empinterpmsg(")\n");});
         vmvec->len++;
         break;
      }
      case OP_NIL:      push(vm, NULL_VAL);              break;
      case OP_TRUE:     push(vm, BOOL_VAL(true));       break;
      case OP_FALSE:    push(vm, BOOL_VAL(false));      break;
      case OP_POP:      pop(vm);                        break;
      case OP_EQUAL: {
         VmValue val1 = pop(vm);
         VmValue val2 = pop(vm);
         if (IS_LOOPVAR(val1)) {
            val1 = INT_VAL(AS_LOOPVAR(val1));
         }
         if (IS_LOOPVAR(val2)) {
            val1 = INT_VAL(AS_LOOPVAR(val1));
         }
         push(vm, BOOL_VAL(val1 == val2));
      break;
      }
      case OP_GREATER:  BINARY_CMP(vm, BOOL_VAL,   >);  break;
      case OP_LESS:     BINARY_CMP(vm, BOOL_VAL,   <);  break;
      case OP_ADD:      BINARY_OP(vm,  +);              break;
      case OP_SUBTRACT: BINARY_OP(vm,  -);              break;
      case OP_MULTIPLY: BINARY_OP(vm,  *);              break;
      case OP_DIVIDE:   BINARY_OP(vm,  /);              break;
      case OP_NOT: {
         assert(IS_BOOL(vm->stack_top[-1]));
         push(vm, BOOL_VAL(! AS_BOOL(pop(vm))));
         break;
      }
      case OP_NEGATE:
         push(vm, NUMBER_VAL(-AS_NUMBER(pop(vm))));
         break;
      case OP_JUMP: {
         uint16_t offset = READ_SHORT(vm);
         vm->code.ip += offset;
         DEBUGVMRUN("\n");
         break;
      }
      case OP_JUMP_IF_TRUE: {
         uint16_t offset = READ_SHORT(vm);
         if (AS_BOOL(pop(vm))) vm->code.ip += offset;
         DEBUGVMRUN("\n");
         break;
      }
      case OP_JUMP_IF_FALSE: {
         uint16_t offset = READ_SHORT(vm);
         if (!AS_BOOL(pop(vm))) vm->code.ip += offset;
         DEBUGVMRUN("\n");
         break;
      }
      case OP_JUMP_IF_TRUE_NOPOP: {
         uint16_t offset = READ_SHORT(vm);
         if (AS_BOOL(vmpeek(vm, 0))) vm->code.ip += offset;
         DEBUGVMRUN("\n");
         break;
      }
      case OP_JUMP_IF_FALSE_NOPOP: {
         uint16_t offset = READ_SHORT(vm);
         if (!AS_BOOL(vmpeek(vm, 0))) vm->code.ip += offset;
         DEBUGVMRUN("\n");
         break;
      }
      case OP_JUMP_BACK: {
         uint16_t offset = READ_SHORT(vm);
         vm->code.ip -= offset;
         DEBUGVMRUN("\n");
         break;
      }
      case OP_JUMP_BACK_IF_FALSE: {
         uint16_t offset = READ_SHORT(vm);
         if (!AS_BOOL(pop(vm))) vm->code.ip -= offset;
         DEBUGVMRUN("\n");
         break;
      }
      case OP_GMSSYM_RESOLVE: {
         GIDX_TYPE gidx = READ_GIDX(vm);
         VmGmsSymIterator *symiter;

         N_CHECK_EXIT(symiter, getgmssymiter(&vm->globals, gidx));

         DEBUGVMRUN_EXEC({print_symiter(symiter, vm); DEBUGVMRUN("%s", "\n"); })

         status = gms_resolve_symb(&vm->data, symiter);
         if (status != OK) { goto _exit; }

         break;
      }
      case OP_GMS_RESOLVE_EXTEND: {
         GIDX_TYPE gidx = READ_GIDX(vm);
         VmGmsSymIterator *symiter;

         N_CHECK_EXIT(symiter, getgmssymiter(&vm->globals, gidx));

         DEBUGVMRUN_EXEC({print_symiter(symiter, vm); DEBUGVMRUN("%s", "\n"); })

         status = gms_resolve_extend_symb(&vm->data, symiter);
         if (status != OK) { goto _exit; }

         break;
      }
      case OP_GMS_EQUVAR_INIT: {
         uint8_t identtype = READ_BYTE(vm);
         S_CHECK(gms_extend_init(&vm->data, identtype));
         break;
      }
      case OP_GMS_EQUVAR_SYNC: {
         uint8_t identtype = READ_BYTE(vm);
         S_CHECK(gms_extend_sync(&vm->data, identtype));
         break;
      }
      case OP_GMS_MEMBERSHIP_TEST: {
         GIDX_TYPE gidx = READ_GIDX(vm);
         VmGmsSymIterator *symiter;

         N_CHECK_EXIT(symiter, getgmssymiter(&vm->globals, gidx));

         bool res;
         status = gms_membership_test(&vm->data, symiter, &res);

         if (status != OK) { goto _exit; }

         DEBUGVMRUN_EXEC({print_symiter(symiter, vm);
         DEBUGVMRUN("%s", "  -->  ");
         })

         push(vm, BOOL_VAL(res));
         break;
      }
      case OP_CALL_API: {
         uint8_t api_idx = READ_BYTE(vm);
         assert(api_idx < empapis_len);
         const EmpApiCall callobj = empapis[api_idx];
         const empapi fn = callobj.fn;
         ptrdiff_t argc = callobj.argc;
         assert(argc <= vm->stack_top - vm->stack);
         DEBUGVMRUN("%s with %ld stack args\n", empapis_names[api_idx], argc);

         int rc = fn(&vm->data, argc, vm->stack_top - argc);
         if (rc != OK) {
            error("[empvm_run] error %d while calling '%s'\n", rc,
                  empapis_names[api_idx]);
            status = rc;
            goto _exit;
         }
         /* By convention, the "top" of the call stack is the object worked on
          * and we keep it here to avoid pushing it later on*/
         if (argc > 1) stack_mvback(vm, argc-1);
         break;
      }
      case OP_NEW_OBJ: {
         uint8_t newobj_call_idx = READ_BYTE(vm);
         assert(newobj_call_idx < empnewobjs_len);

         const EmpNewObjCall callobj = empnewobjs[newobj_call_idx];
         const empnewobj fn = callobj.fn;
         ptrdiff_t argc = callobj.argc;
         assert(argc <= vm->stack_top - vm->stack);
         DEBUGVMRUN("%s with %ld stack args\n", empnewobjs_names[newobj_call_idx],
                    argc);

         void *o = fn(&vm->data, argc, vm->stack_top - argc);
         if (!o) {
            error("[empvm_run] allocation failed in '%s'\n",
                  empnewobjs_names[newobj_call_idx]);
            return Error_RuntimeError;
         }

         stack_mvback(vm, argc);
         push(vm, callobj.obj2vmval(o));

         if (callobj.get_uid) {
            vm->data.uid_parent = callobj.get_uid(o);
         }
         break;
      }
      case OP_EDGE_DUP_DAGL: {
         GIDX_TYPE gidx = READ_GIDX(vm);
         assert(gidx < vm->globals.len);
         assert(IS_EDGEOBJ(vm->globals.list[gidx]));

         DagLabels *dagl_src = AS_EDGEOBJ(vm->globals.list[gidx]);
         DagLabel *dagl_cpy;
         A_CHECK(dagl_cpy, dag_labels_dupaslabel(dagl_src));
         dagl_cpy->daguid = vm->data.uid_parent;

         S_CHECK(daglabel2edge_add(vm->data.label2edge, dagl_cpy));

         break;
      }
      case OP_EDGE_INIT: {
         GIDX_TYPE gidx = READ_GIDX(vm);
         assert(gidx < vm->globals.len);
         assert(IS_EDGEOBJ(vm->globals.list[gidx]));

         DagLabels *dagl_src = AS_EDGEOBJ(vm->globals.list[gidx]);
         DagLabels *dagl_cpy;
         A_CHECK(dagl_cpy, dag_labels_dup(dagl_src));
         dagl_cpy->daguid = vm->data.uid_parent;

         S_CHECK(daglabels2edges_add(vm->data.labels2edges, dagl_cpy));

         vm->data.daglabels = dagl_cpy;
         REALLOC_(vm->data.daglabels_ws, int, dagl_cpy->num_var);
         break;
      }
      case OP_DAGL_STORE: {
         assert(vm->data.daglabels);
         assert(vm->data.daglabels_ws);
         DagLabels *dagl = vm->data.daglabels;

         /* This is a dummy read. Otherwise the VM dissassembler is lost */
         uint8_t nargs = READ_BYTE(vm);
         assert(dagl->num_var == nargs);

         S_CHECK(scratchint_ensure(&vm->data.e_data, nargs));
         IntScratch *iscratch = &vm->data.e_data;

         for (unsigned i = 0; i < dagl->num_var; ++i) {
            uint8_t slot = READ_BYTE(vm);
            int uel_lidx = AS_INT(vm->locals[slot]);
            iscratch->data[i] = uel_lidx;
         }

         S_CHECK(dag_labels_add(dagl, iscratch->data));
         break;
      }
      case OP_DAGL_FINALIZE: {
         assert(vm->data.daglabels);
         assert(vm->data.daglabels_ws);
         DagLabels *dagl = vm->data.daglabels;

         /* ---------------------------------------------------------------
          * If there are no child, then delete the label
          * --------------------------------------------------------------- */

         if (dagl->num_children == 0) {
            vm->data.labels2edges->len--;
         }
         vm->data.daglabels = NULL;
         break;
      }
         // TODO: Delete?
      case OP_SET_DAGUID_FROM_REGENTRY: {
         DagRegister *dagregister = vm->data.dagregister;
         unsigned reglen = dagregister->len;

         if (reglen == 0) {
            errormsg("[empvm_run] ERROR: dagregister is empty. Please report this\n");
            return Error_EMPRuntimeError;
         }
         vm->data.uid_parent = dagregister->list[reglen-1]->daguid;
         assert(valid_uid(vm->data.uid_parent));
         break;
      }
      case OP_END: {
         if (vm->stack != vm->stack_top) {
            errormsg("[empvm_run]: ERROR: stack non-empty at the end.\n");
         }
         vm->code.ip = vm->instr_start;
         DEBUGVMRUN("\n");
         return OK;
      }
      default:
         error("[empvm_run]: ERROR: unhandled instruction %s #%u\n", opcodes_name(instr), instr);
         return Error_EMPRuntimeError;
      }
   }



_exit:

   error("[empvm_run] Error occurred on line %u\n", getlinenr(vm));
   return status;
}
