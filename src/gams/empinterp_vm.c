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

#include <inttypes.h>
#include <stddef.h>

#include "dctmcc.h"
#include "empdag.h"
#include "empinterp_linkbuilder.h"
#include "empinterp_priv.h"
#include "empinterp_symbol_resolver.h"
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
   return vm->globals.arr[gidx];
}

static inline unsigned READ_VMUINT(EmpVm *vm)
{
   GIDX_TYPE gidx = READ_GIDX(vm);
   assert(gidx < vm->uints.len);
   return vm->uints.arr[gidx];
}

static inline unsigned READ_VMINT(EmpVm *vm)
{
   GIDX_TYPE gidx = READ_GIDX(vm);
   assert(gidx < vm->ints.len);
   return vm->ints.arr[gidx];
}

/*
#define DEBUGVMRUN(fmt, ...) \
printout(PO_TRACE_EMPINTERP, "[%s] " fmt, opcodes_name(instr), __VA_ARGS__);
*/

#   define DEBUGVMRUN(...) trace_empinterp(__VA_ARGS__);
#   define DEBUGVMRUN_EXEC(EXPR) EXPR

#else /* defined(NDEBUG) */

#   define read_global(vm) (vm->globals.arr[READ_GIDX(vm)])
#   define READ_VMINT(vm)  (vm->ints.arr[READ_GIDX(vm)])
#   define READ_VMUINT(vm)  (vm->uints.arr[READ_GIDX(vm)])
#   define DEBUGVMRUN(...) 
#   define DEBUGVMRUN_EXEC(EXPR)

#endif /* !defined(NDEBUG) */


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


DBGUSED static inline bool identisequvar(IdentType type)
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
   case SIGNATURE_NASHOBJ: {
      const Nash *nash = AS_PTR(val_);
      if (!nash) {
         trace_empinterpmsg("  Nash: NULL!\n");
         break;
      } 
      unsigned nashid = nash_getid(nash);
      trace_empinterp("%*s: '%s'\n", pad, "Nash",
                      empdag_getnashname(&vm->data.mdl->empinfo.empdag, nashid));
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
      trace_empinterp("%*s: %.*s\n", pad, "RegEntry", regentry->label_len, regentry->label);
      break;
   }
   case SIGNATURE_ARCOBJ: {
      LinkLabels *arcobj = AS_ARCOBJ(val_);
      trace_empinterp("%*s: %.*s\n", pad, "ArcObj", arcobj->label_len, arcobj->label);
      break;
   }
   case SIGNATURE_GMSSYMITER: {
      VmGmsSymIterator *symiter = AS_GMSSYMITER(val_);
      Lexeme *lexeme = &symiter->ident.lexeme;
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

      mpid_t mp_id = mp_getid(mp);
      printout(mode, " MP('%s') ", empdag_getmpname(&vm->data.mdl->empinfo.empdag, mp_id));
      break;
   }
   case SIGNATURE_NASHOBJ: {
      const Nash *nash = AS_PTR(v);
      if (!nash) {
         printstr(mode, " Nash: NULL!\n");
         break;
      } 
      unsigned nashid = nash_getid(nash);
      printout(mode, " Nash('%s') ", empdag_getnashname(&vm->data.mdl->empinfo.empdag, nashid));
      break;
   }
   case SIGNATURE_OVFOBJ: {
      const OvfDef *ovf = AS_PTR(v);
      if (!ovf) {
         printstr(mode, "  OVF: NULL!\n");
         break;
      } 
      printout(mode, " OVF('%s') ", ovf->name);
      break;
   }
   case SIGNATURE_POINTER:
      printout(mode, "%14p (PTR)", AS_PTR(v));
      break;
   case SIGNATURE_REGENTRY: {
      DagRegisterEntry *regentry = AS_REGENTRY(v);
      printout(mode, "%20.*s", regentry->label_len, regentry->label);
      break;
   }
   case SIGNATURE_ARCOBJ: {
      LinkLabels *arcobj = AS_ARCOBJ(v);
      printout(mode, "%20.*s", arcobj->label_len, arcobj->label);
      break;
   }
   case SIGNATURE_GMSSYMITER: {
      VmGmsSymIterator *symiter = AS_GMSSYMITER(v);
      Lexeme *lexeme = &symiter->ident.lexeme;
      printout(mode, "%20.*s", lexeme->len, lexeme->start);
      break;
   }
   default: ;
   }

}

#define PADDING_VERBOSE 60

DBGUSED static int getpadding(int offset)
{
   return offset < PADDING_VERBOSE ? PADDING_VERBOSE - offset : 0;
}

DBGUSED static void print_symiter(VmGmsSymIterator *symiter, EmpVm *vm)
{
   DBGUSED Lexeme *lexeme = &symiter->ident.lexeme;
   DEBUGVMRUN("%.*s", lexeme->len, lexeme->start);
   int dim = symiter->ident.dim;
   if (dim > 0) {
      int *uels = symiter->uels;
      DEBUGVMRUN("%s", "(");
      for (unsigned ii = 0; ii < dim; ++ii) {
         if (ii > 0) { DEBUGVMRUN(", "); }
         int dummyoffset;
         dct_printuel(vm->data.dct, uels[ii], PO_TRACE_EMPINTERP,
                      &dummyoffset);
      }
      DEBUGVMRUN("%s", ")");
   }
}
NONNULL static int vmdata_consume_scalardata(VmData *data, double *coeff)
{
   unsigned dnrecs = data->dnrecs;

   if (dnrecs == UINT_MAX) {
      *coeff = 1;
      return OK;
   }

   if (dnrecs > 1) {
      error("[empmv] ERROR: expecting at most one data value, got %u\n", dnrecs);
      return Error_EMPIncorrectInput;
   }

   *coeff = dnrecs == 0 ? 1. : data->dval; assert(isfinite(*coeff));

   data->dnrecs = UINT_MAX;

   data->dval = NAN;

   return OK;
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

// See below, 2024.05.13
#if 0
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
#endif

static int gms_read_symbol(VmData *vmdata, VmGmsSymIterator *symiter)
{
   IdentType type = symiter->ident.type;
   assert(identisequvar(type) || type == IdentVector);

   GmsResolveData data;

   data.type = GmsSymIteratorTypeVm;
   data.symiter.vm = symiter;

   dctHandle_t dct = vmdata->dct;

   switch (type) {
   case IdentVar:
      data.payload.v = &vmdata->v;
      data.iscratch = &vmdata->v_data;
      vmdata->v_current = &vmdata->v;
      break;
   case IdentEqu:
      data.payload.e = &vmdata->e;
      data.iscratch = &vmdata->e_data;
      vmdata->e_current = &vmdata->e;
      break;
   case IdentSet:
      data.iscratch = &vmdata->iscratch;
      break;
   case IdentScalar:
   case IdentVector:
   case IdentParam:
      data.dscratch = &vmdata->dscratch;
      break;
   default:
      error("%s :: unsupported token '%s'", __func__, identtype2str(type));
      return Error_EMPRuntimeError;
   }


   int status;

   switch (type) {
   case IdentVar:
   case IdentEqu:
#ifdef RHP_EXPERIMENTAL 
      if (vmdata->gmddct) {
         char symname[GMS_SSSIZE];
         memcpy(symname, symiter->ident.lexeme.start, symiter->ident.lexeme.len);
         symname[symiter->ident.lexeme.len] = 0;
         status = gmd_read(vmdata->gmddct, vmdata->dct, &data, symname);

         if (status == OK) {
            if (data.nrecs == 1) {
            if (type == IdentVar) {
               avar_setcompact(&vmdata->v, 1, data.itmp);
            } else {
               aequ_setcompact(&vmdata->e, 1, data.itmp);
            }
            } else {
            if (type == IdentVar) {
               avar_setlist(&vmdata->v, data.nrecs, data.iscratch->data);
            } else {
               aequ_setlist(&vmdata->e, data.nrecs, data.iscratch->data);
            }
            }
         }
      } else {
         status = dct_read_equvar(dct, &data);
      }
#else
      status = dct_read_equvar(dct, &data);
#endif
      break;
   case IdentSet:
   case IdentScalar:
   case IdentVector:
   case IdentParam: {
      char symname[GMS_SSSIZE];
      memcpy(symname, symiter->ident.lexeme.start, symiter->ident.lexeme.len);
      symname[symiter->ident.lexeme.len] = 0;
      status = gmd_read(vmdata->gmd, vmdata->dct, &data, symname);
      if (type == IdentSet) {
         vmdata->inrecs = data.nrecs;
         vmdata->ival = data.itmp;
      } else { //data
         vmdata->dnrecs = data.nrecs;
         vmdata->dval = data.dtmp;
      }
      break;
      }
   default:
      error("%s :: unsupported token '%s'", __func__, identtype2str(type));
      return Error_EMPRuntimeError;
   }

   // TODO: this looks off in the log. 2024.05.13
#if 0
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
#endif
   return status;
}

static int gms_read_extend_symb(VmData *vmdata, VmGmsSymIterator *filter)
{
   IdentType type = filter->ident.type;
   assert(identisequvar(type));

   S_CHECK(gms_read_symbol(vmdata, filter));

   switch (type) {
   case IdentVar:
      return avar_extend(&vmdata->v_extend, &vmdata->v);
   case IdentEqu:
      return aequ_extendandown(&vmdata->e_extend, &vmdata->e);
   default:
      error("%s :: unsupported token '%s'", __func__, identtype2str(type));
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
      error("%s :: unsupported token '%s'", __func__, identtype2str(type));
      return Error_EMPRuntimeError;
   }

   return OK;
}

static int gms_equvar_sync(VmData *vmdata, IdentType type)
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
      error("%s :: unsupported token '%s'", __func__, identtype2str(type));
      return Error_EMPRuntimeError;
   }

   return OK;
}

static int vm_membership_test(UNUSED VmData *vmdata, VmGmsSymIterator *symiter,
                               bool *res)
{
   IdentType type = symiter->ident.type;

   switch (type) {
   case IdentMultiSet:
      assert(symiter->ident.ptr);
      return vm_multiset_membership_test(symiter->ident.ptr, symiter, res);
   case IdentLocalSet:
   case IdentSet: {
      IntArray *obj = symiter->ident.ptr;
      assert(obj && valid_set(*obj));
      int uel = symiter->uels[0];
      unsigned idx = rhp_int_find(obj, uel);
      *res = idx < UINT_MAX;
      return OK;
   }
   default:
      error("[empvm_run] unexpected runtime error: no membership test for ident '%s'",
            identtype2str(type));
      return Error_EMPRuntimeError;
   }


}

NONNULL static int vmdata_init(VmData *data)
{
   data->scalar_tracker = ScalarSymbolInactive;

   data->uid_grandparent = EMPDAG_UID_NONE;
   data->uid_parent = EMPDAG_UID_NONE;

   aequ_init(&data->e);
   avar_init(&data->v);
   scratchint_init(&data->e_data);
   scratchint_init(&data->v_data);
   scratchint_init(&data->iscratch);
   scratchdbl_init(&data->dscratch);
 
   S_CHECK(aequ_setblock(&data->e_extend, 3));
   S_CHECK(avar_setblock(&data->v_extend, 3));
   S_CHECK(scratchint_ensure(&data->iscratch, 10));
   S_CHECK(scratchdbl_ensure(&data->dscratch, 10));

   data->e_current = NULL;
   data->v_current = NULL;

   arcvfobjs_init(&data->arcvfobjs);

   data->linklabels = NULL;
   data->linklabel_ws = NULL;

   data->dval = NAN;
   data->ival = UINT_MAX;
   data->inrecs = UINT_MAX;
   data->dnrecs = UINT_MAX;
   data->mpid_dual = MpId_NA;

   return OK;
}

EmpVm* empvm_new(Interpreter *interp)
{
   void *obj;
   CALLOC_NULL(obj, EmpVm, 1);
   EmpVm* vm = (EmpVm*)obj;

   vm->stack_top = vm->stack;

   vmobjs_init(&vm->newobjs);
   vmobjs_init(&vm->objs);

   rhp_uint_init(&vm->uints);
   rhp_int_init(&vm->ints);

   SN_CHECK_EXIT(vmdata_init(&vm->data));

   SN_CHECK_EXIT(vmvals_resize(&vm->globals, 10));
   SN_CHECK_EXIT(vmcode_resize(&vm->code, 100));
   SN_CHECK_EXIT(vmvals_add(&vm->globals, UINT_VAL(0)));
   SN_CHECK_EXIT(vmvals_add(&vm->globals, INT_VAL(0)));



   /* genlabelname needs interpreter for the ops */
   vm->data.interp = interp;

   vm->data.dct = interp->dct;
   vm->data.gmd = interp->gmd;
   vm->data.gmddct = interp->gmddct;
   vm->data.mdl = interp->mdl;
   vm->data.globals = &interp->globals;
   vm->data.dagregister = &interp->dagregister;
   vm->data.linklabels2arcs = &interp->linklabels2arcs;
   vm->data.linklabel2arc = &interp->linklabel2arc;
   vm->data.dualslabels = &interp->dualslabels;

   return vm;

_exit:

   IGNORE_DEALLOC_MISMATCH(empvm_free(vm));
   return NULL;
}

void empvm_free(EmpVm *vm)
{
   if (!vm) return;

   /* ---------------------------------------------------------------------
    * All objects in globals below to the VM and must be freed
    * --------------------------------------------------------------------- */

   for (unsigned i = 0, len = vm->globals.len; i < len; ++i) {
      VmValue val = vm->globals.arr[i];
      if (IS_PTR(val) || IS_STR(val)) {
         void *obj = AS_PTR(val);
         free(obj);
      } else if (IS_ARCOBJ(val)) {
         LinkLabels *link = AS_ARCOBJ(val);
         linklabels_free(link);
      } else if (IS_GMSSYMITER(val)) {
         VmGmsSymIterator *symiter = AS_GMSSYMITER(val);
         free(symiter);
      } else if (IS_REGENTRY(val)) {
         DagRegisterEntry *regentry = AS_REGENTRY(val);
         dagregentry_free(regentry);
      }
   }

   vmvals_free(&vm->globals);
   vmcode_free(&vm->code);
   vmobjs_free(&vm->newobjs);
   vmobjs_free(&vm->objs);
   rhp_uint_empty(&vm->uints);
   rhp_int_empty(&vm->ints);

   VmData * restrict vmdata = &vm->data;
   aequ_empty(&vmdata->e);
   avar_empty(&vmdata->v);
   aequ_empty(&vmdata->e_extend);
   avar_empty(&vmdata->v_extend);
   scratchint_empty(&vmdata->e_data);
   scratchint_empty(&vmdata->v_data);
   scratchint_empty(&vmdata->iscratch);
   scratchdbl_empty(&vmdata->dscratch);
   free(vmdata->linklabel_ws);
   arcvfobjs_free(&vmdata->arcvfobjs);

   free(vm);
}

int empvm_run(struct empvm *vm)
{
   S_CHECK(empvm_dissassemble(vm, PO_TRACE_EMPINTERP));

   int status = OK, poffset;
   unsigned linenr;
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
            trace_empinterp("stack%n", &poffset);
            for (VmValue *val = vm->stack; val < vm->stack_top; val++, poffset = 0) {
               trace_empinterp("%*s", 50-poffset, "");
               print_vmval_full(*val, vm);
            }
         }

         trace_empinterp("[%5td] %30s%10s",
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
         push(vm, UINT_VAL(READ_VMUINT(vm)));
         break;
      }
      case OP_PUSH_VMINT: {
         push(vm, INT_VAL(READ_VMINT(vm)));
         break;
      }
      case OP_UPDATE_LOOPVAR: {
         uint8_t lidx_loopvar = READ_BYTE(vm);
         uint8_t lidx_idxvar = READ_BYTE(vm);
         IdentType type = READ_BYTE(vm);
         GIDX_TYPE gidx = READ_GIDX(vm);

         unsigned idx = AS_UINT(vm->locals[lidx_idxvar]);
         DBGUSED int offset0, offset1, offset2;
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
                  identtype2str(type));
            goto _exit;
         }

         assert(valid_set(set) && idx < set.len);
         vm->locals[lidx_loopvar] = LOOPVAR_VAL(set.arr[idx]);
         DEBUGVMRUN_EXEC({dct_printuel(vm->data.dct, (set.arr[idx]), PO_TRACE_EMPINTERP, &offset2);});
         DEBUGVMRUN("%*s%s#%u[lvar%u = %u]\n", getpadding(offset0 + offset1 + offset2), "",
                    type == IdentSet ? "sets" : "localsets", gidx, lidx_idxvar, idx);
         break;
      }
      case OP_LOCAL_COPYFROM_GIDX: {
         uint8_t slot = READ_BYTE(vm);
         vm->locals[slot] = read_global(vm);
         DEBUGVMRUN("lvar@%u = %" PRIu64 "\n", slot, vm->locals[slot] & MASK_PAYLOAD_32);
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
         default:
            error("[empvm_run] in %s: unexpected ident type %s", opcodes_name(instr),
                  identtype2str(type));
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

         assert(idx < symiter->ident.dim);
         assert(IS_LOOPVAR(vm->locals[lidx]));

         int uel = AS_LOOPVAR(vm->locals[lidx]);
         symiter->uels[idx] = uel;

         if (uel > 0) symiter->compact = false;
         DBGUSED int offset1, offset2;
         DBGUSED Lexeme *lexeme = &symiter->ident.lexeme;
         DEBUGVMRUN("%.*s[%u] <- %n", lexeme->len, lexeme->start, idx, &offset1);
         DEBUGVMRUN_EXEC({dct_printuel(vm->data.dct, uel, PO_TRACE_EMPINTERP, &offset2);})
         DEBUGVMRUN(" #%u%n", uel, &offset2);
         DEBUGVMRUN("%*sloopvar@%u\n", getpadding(offset1 + offset2), "", lidx);


         break;
      }

#ifdef HACK
      case OP_DUALSLABEL_ADD: {
         GIDX_TYPE gidx = READ_GIDX(vm);

         DualsLabel *dualslabel;
         N_CHECK_EXIT(dualslabel, dualslabel_arr_at(vm->data.dualslabels, gidx));

S_CHECK_EXIT(dualslabel_add(dualslabel, mpid_dual));

         break;
      }
#endif 

// HACK: it's unclear this is any kind of good idea. 
// Review OP_LINKLABELS_SETFROM_LOOPVAR 
#ifdef HACK
      case OP_DUALSLABEL_SETFROM_LOOPVAR: {
         GIDX_TYPE gidx = READ_GIDX(vm);
         uint8_t idx = READ_BYTE(vm);
         uint8_t lidx = READ_BYTE(vm);

         DualsLabel *dualslabel;
         N_CHECK_EXIT(dualslabel, dualslabel_arr_at(vm->data.dualslabels, gidx));

         assert(idx < dualslabel->dim);
         assert(IS_LOOPVAR(vm->locals[lidx]));

         int uel = AS_LOOPVAR(vm->locals[lidx]);
         assert(dualslabel->mpid_duals.len > 0); assert(dualslabel->num_var > 0);

         unsigned offset = dualslabel->mpid_duals.len - 1;
         int *uels = &dualslabel->uels_var[(size_t)offset*dualslabel->num_var];
         uels[idx] = uel;

         DBGUSED int offset1, offset2;
         DEBUGVMRUN("%.*s[%u] <- %n", dualslabel->label_len, dualslabel->label, idx, &offset1);
         DEBUGVMRUN_EXEC({dct_printuel(vm->data.dct, uel, PO_TRACE_EMPINTERP, &offset2);})
         DEBUGVMRUN("%*sloopvar@%u\n", getpadding(offset1 + offset2), "", lidx);

         break;
      }
#endif
      case OP_DUALSLABEL_STORE: {
         GIDX_TYPE gidx = READ_GIDX(vm);

         DualsLabel *dualslabel;
         N_CHECK_EXIT(dualslabel, dualslabel_arr_at(vm->data.dualslabels, gidx));

         /* This is a dummy read. Otherwise the VM dissassembler is lost */
         uint8_t nargs = READ_BYTE(vm);

         // HACK: loopiterators are not given here, but count as num_var ...
         assert(dualslabel->num_var == nargs);

         int *uels = NULL;
         if (nargs > 0) {
            S_CHECK_EXIT(scratchint_ensure(&vm->data.e_data, nargs));
            uels = vm->data.e_data.data;

            for (unsigned i = 0; i < nargs; ++i) {
               uint8_t slot = READ_BYTE(vm);
               int uel_lidx = AS_INT(vm->locals[slot]);
               uels[i] = uel_lidx;
            }
         }

         mpid_t mpid_dual = vm->data.mpid_dual;
         if (!mpid_regularmp(mpid_dual)) {
            error("[empvm] ERROR: invalid value %s #%u\n",
                  mpid_specialvalue(mpid_dual), mpid_dual);
         }

         S_CHECK_EXIT(dualslabel_add(dualslabel, uels, nargs, mpid_dual));

         DEBUGVMRUN("#%u: ", dualslabel->mpid_duals.len-1);
         DEBUGVMRUN_EXEC({ for (unsigned i = 0; i < dualslabel->num_var; ++i) {
            int dummy; if (i == 0) {DEBUGVMRUN("(");}
            dct_printuel(vm->data.dct, uels[i], PO_TRACE_EMPINTERP, &dummy);
            if (i == dualslabel->num_var-1) { DEBUGVMRUN(")");}}
            });
         DEBUGVMRUN("\n");
         
         break;
      }

      case OP_LINKLABELS_SETFROM_LOOPVAR: {
         GIDX_TYPE gidx = READ_GIDX(vm);
         uint8_t idx = READ_BYTE(vm);
         uint8_t lidx = READ_BYTE(vm);

         LinkLabels *linklabels;
         N_CHECK_EXIT(linklabels, getlinklabels(&vm->globals, gidx));

         assert(idx < linklabels->dim);
         assert(IS_LOOPVAR(vm->locals[lidx]));

         int uel = AS_LOOPVAR(vm->locals[lidx]);
         linklabels->data[idx] = uel;

         DBGUSED int offset1, offset2;
         DEBUGVMRUN("%.*s[%u] <- %n", linklabels->label_len, linklabels->label, idx, &offset1);
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

         DBGUSED int offset1, offset2; 
         DEBUGVMRUN("%.*s[%u] <- %n", regentry->label_len,
                    regentry->label, idx, &offset1);
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
         S_CHECK_EXIT(rhp_int_add(set, val));
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

         VmVector * restrict vmvec = AS_OBJ(VmVector *, vm->locals[vmvec_lidx]);
         OvfParam * param = AS_OBJ(OvfParam *, vm->globals.arr[param_gidx]);
         unsigned size = vmvec->len;
         param->type = ARG_TYPE_VEC;
         param->size_vector = size;
         MALLOC_EXIT(param->vec, double, size);
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

         VmVector * restrict vmvec = AS_OBJ(VmVector *, vm->locals[vmvec_lidx]);
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
      case OP_GMS_SYMBOL_READ_SIMPLE: {
         GIDX_TYPE gidx = READ_GIDX(vm);
         VmGmsSymIterator *symiter;

         N_CHECK_EXIT(symiter, getgmssymiter(&vm->globals, gidx));

         DEBUGVMRUN_EXEC({print_symiter(symiter, vm); DEBUGVMRUN("%s", "\n"); })

         status = gms_read_symbol(&vm->data, symiter);
         if (status != OK) { goto _exit; }

         break;
      }
      case OP_GMS_SYMBOL_READ_EXTEND: {
         GIDX_TYPE gidx = READ_GIDX(vm);
         VmGmsSymIterator *symiter;

         N_CHECK_EXIT(symiter, getgmssymiter(&vm->globals, gidx));

         DEBUGVMRUN_EXEC({print_symiter(symiter, vm); DEBUGVMRUN("%s", "\n"); })

         status = gms_read_extend_symb(&vm->data, symiter);
         if (status != OK) { goto _exit; }

         break;
      }
      case OP_GMS_EQUVAR_READ_INIT: {
         uint8_t identtype = READ_BYTE(vm);
         S_CHECK_EXIT(gms_extend_init(&vm->data, identtype));
         break;
      }
      case OP_GMS_EQUVAR_READ_SYNC: {
         uint8_t identtype = READ_BYTE(vm);
         S_CHECK_EXIT(gms_equvar_sync(&vm->data, identtype));
         break;
      }
      case OP_GMS_MEMBERSHIP_TEST: {
         GIDX_TYPE gidx = READ_GIDX(vm);
         VmGmsSymIterator *symiter;

         N_CHECK_EXIT(symiter, getgmssymiter(&vm->globals, gidx));

         bool res;
         status = vm_membership_test(&vm->data, symiter, &res);

         if (status != OK) { goto _exit; }

         DEBUGVMRUN_EXEC({print_symiter(symiter, vm);
         DEBUGVMRUN("%s", "  -->  ");
         })

         push(vm, BOOL_VAL(res));
         break;
      }
      case OP_EMPAPI_CALL: {
         uint8_t api_idx = READ_BYTE(vm);
         assert(api_idx < empapis_len);
         const EmpApiCall callobj = empapis[api_idx];
         const empapi fn = callobj.fn;
         ptrdiff_t argc = callobj.argc;
         assert(argc <= vm->stack_top - vm->stack);
         DEBUGVMRUN("%s with %td stack args\n", empapis_names[api_idx], argc);

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
         DEBUGVMRUN("%s with %td stack args\n", empnewobjs_names[newobj_call_idx],
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

      case OP_LINKLABELS_INIT: {
         GIDX_TYPE gidx = READ_GIDX(vm);
         assert(gidx < vm->globals.len);
         assert(IS_ARCOBJ(vm->globals.arr[gidx]));

         LinkLabels *linklabels = AS_ARCOBJ(vm->globals.arr[gidx]);
         LinkLabels *linklabels_cpy;
         A_CHECK_EXIT(linklabels_cpy, linklabels_dup(linklabels));
         linklabels_cpy->daguid_parent = vm->data.uid_parent;

         S_CHECK_EXIT(linklabels2arcs_add(vm->data.linklabels2arcs, linklabels_cpy));


         // HACK: reset variable and double counter
         avar_reset(&vm->data.v);


         vm->data.linklabels = linklabels_cpy;
         unsigned num_var = linklabels_cpy->num_var;
         if (num_var > 0) {
            REALLOC_EXIT(vm->data.linklabel_ws, int, linklabels_cpy->num_var);
         }

         DEBUGVMRUN("\n");
         break;
      }

      case OP_LINKLABELS_DUP: {
         GIDX_TYPE gidx = READ_GIDX(vm);
         assert(gidx < vm->globals.len);
         assert(IS_ARCOBJ(vm->globals.arr[gidx]));

         LinkLabels *linklabels_src = AS_ARCOBJ(vm->globals.arr[gidx]);
         LinkLabel *linklabel_cpy;


         double coeff;
         rhp_idx vi = IdxNA;
         S_CHECK_EXIT(vmdata_consume_scalardata(&vm->data, &coeff));
         // HACK
         //S_CHECK_EXIT(vmdata_consume_scalarvar(&vm->data, &vi));

         A_CHECK_EXIT(linklabel_cpy, linklabels_dupaslabel(linklabels_src, coeff, vi));
         linklabel_cpy->daguid_parent = vm->data.uid_parent;

         S_CHECK_EXIT(linklabel2arc_add(vm->data.linklabel2arc, linklabel_cpy));

         DEBUGVMRUN("\n");
         break;
      }
      case OP_LINKLABELS_STORE: {
         assert(vm->data.linklabels);
         LinkLabels *linklabels = vm->data.linklabels;

         /* This is a dummy read. Otherwise the VM dissassembler is lost */
         uint8_t nargs = READ_BYTE(vm);
         assert(linklabels->num_var == nargs);

         int *uels = NULL;
         if (nargs > 0) {
            S_CHECK_EXIT(scratchint_ensure(&vm->data.e_data, nargs));
            uels = vm->data.e_data.data;

            for (unsigned i = 0; i < linklabels->num_var; ++i) {
               uint8_t slot = READ_BYTE(vm);
               int uel_lidx = AS_INT(vm->locals[slot]);
               uels[i] = uel_lidx;
            }
         }

         double coeff;
         rhp_idx vi;
         S_CHECK_EXIT(vmdata_consume_scalardata(&vm->data, &coeff));
         S_CHECK_EXIT(vmdata_consume_scalarvar(&vm->data, &vi));


         S_CHECK_EXIT(linklabels_add(linklabels, uels, coeff, vi));

         DEBUGVMRUN("#%u: ", linklabels->num_children-1);
         DEBUGVMRUN_EXEC({ for (unsigned i = 0; i < linklabels->num_var; ++i) {
            int dummy; if (i == 0) {DEBUGVMRUN("(");}
            dct_printuel(vm->data.dct, uels[i], PO_TRACE_EMPINTERP, &dummy);
            if (i == linklabels->num_var-1) { DEBUGVMRUN(")");}}
            if (valid_vi(vi)) {DEBUGVMRUN(" vi = %s", mdl_printvarname(vm->data.mdl, vi));}
            if (isfinite(coeff) && coeff != 1.) {DEBUGVMRUN(" c = %e", coeff);}
            });
         DEBUGVMRUN("\n");
         
         break;
      }
      case OP_LINKLABELS_FINI: {
         assert(vm->data.linklabels);
         LinkLabels *linklabels = vm->data.linklabels;

         /* ---------------------------------------------------------------
          * If there are no child, then delete the label
          * --------------------------------------------------------------- */

         if (linklabels->num_children == 0) {
            // TODO: is there a memory leak here?
            // TEST stochastic program with leaf nodes 
            vm->data.linklabels2arcs->len--;
         }
         vm->data.linklabels = NULL;
         DEBUGVMRUN("\n");
         break;
      }
      case OP_LINKLABELS_KEYWORDS_UPDATE: {
         assert(vm->data.linklabels);
         LinkLabels *linklabels = vm->data.linklabels;
         unsigned num_children = linklabels->num_children;

         if (linklabels->num_children == 0) {
            errormsg("[empvm] ERROR: empty linklabels!\n");
            status = Error_EMPRuntimeError;
            goto _exit;
         }

         switch (linklabels->linktype) {
         case LinkObjAddMapSmoothed: {
            double param;
            S_CHECK_EXIT(vmdata_consume_scalardata(&vm->data, &param));
            SmoothingOperatorData *opdat;
            A_CHECK_EXIT(opdat, smoothing_operator_data_new(param));
            linklabels->extras[num_children-1] = opdat;
            DEBUGVMRUN("log-sum-exp: coeff = %e", param);
         }
         default: ;

         }
         DEBUGVMRUN("\n");
         break;
      }

      case OP_VARC_DUAL: {
         assert(valid_uid(vm->data.uid_parent));
         assert(valid_mpid(vm->data.mpid_dual));

         daguid_t uid_parent = vm->data.uid_parent;
         mpid_t mpid_dual = vm->data.mpid_dual;

         /* We don't need it afterwards, reset it */
         vm->data.mpid_dual = MpId_NA;

        double coeff;
        S_CHECK(vmdata_consume_scalardata(&vm->data, &coeff));
 
         ptrdiff_t line_idx = vm->code.ip - vm->instr_start;
        S_CHECK(Varc_dual(vm->data.mdl, vm->code.line[line_idx], uid_parent,
                          mpid2uid(mpid_dual), coeff));


         DEBUGVMRUN("\n");
         break;
      }

         // HACK: Delete?
      case OP_SET_DAGUID_FROM_REGENTRY: {
         DagRegister *dagregister = vm->data.dagregister;
         unsigned reglen = dagregister->len;

         if (reglen == 0) {
            errormsg("[empvm_run] ERROR: dagregister is empty. Please report this\n");
            return Error_EMPRuntimeError;
         }
         vm->data.uid_parent = dagregister->list[reglen-1]->daguid_parent;
         assert(valid_uid(vm->data.uid_parent));

         DEBUGVMRUN("\n");
         break;
      }

      /* Set the scalar tracker value to zero */
      case OP_SCALAR_SYMBOL_TRACKER_INIT:
         switch (vm->data.scalar_tracker) {
         case ScalarSymbolInactive:
         case ScalarSymbolRead:
            vm->data.scalar_tracker = ScalarSymbolZero;
            break;
         default:
            errbugmsg("[empvm] ERROR: scalar symbol tracker has the wrong value");
            status = Error_BugPleaseReport;
            goto _exit;
         }

         DEBUGVMRUN("\n");
         break;

      /* Check that only one value has been read */
      case OP_SCALAR_SYMBOL_TRACKER_CHECK: {
         ScalarSymbolStatus symbol_tracker = vm->data.scalar_tracker;

         switch (symbol_tracker) {
         case ScalarSymbolInactive:
            errbugmsg("[empvm] ERROR: scalar symbol tracker is inactive.");
            status = Error_BugPleaseReport;
            goto _exit;

         case ScalarSymbolRead:
            // TODO: improve to provide the user better error message
            // TODO: TEST!
            errormsg("[empvm] ERROR: More than one value read for a given symbol! "
                     "Exactly one value was expected\n");
            status = Error_EMPIncorrectInput;
            goto _exit;
 
         case ScalarSymbolZero:
            vm->data.scalar_tracker = ScalarSymbolRead;
            break;
         default:
            errbug("[empvm] ERROR: unexpected value %u for symbol tracker.", symbol_tracker);
            status = Error_BugPleaseReport;
            goto _exit;
         }

         DEBUGVMRUN("\n");
         break;
      }

      /* Put a scalar into the read values */
      case OP_HACK_SCALAR2VMDATA: {
         GIDX_TYPE gidx = READ_GIDX(vm); assert(gidx < vm->data.globals->scalars.len);
         double val = vm->data.globals->scalars.list[gidx];
         DEBUGVMRUN("stored %e\n", val);
         vm->data.dval = val;
         break;
      }

      case OP_END: {
         // HACK: turn this into an error?
         if (vm->stack != vm->stack_top) {
            errormsg("[empvm_run]: ERROR: stack non-empty at the end.\n");
         }
         vm->code.ip = vm->instr_start;
         DEBUGVMRUN("\n");
         return OK;
      }

      default:
         errbug("\n\n[empvm_run]: ERROR: unhandled instruction %s #%u\n", opcodes_name(instr), instr);
         status = Error_BugPleaseReport;
         goto _exit;
      }
   }



_exit:

   linenr = getlinenr(vm);
   error("\n\n[empvm_run] %nError occurred on line %u:\n", &poffset, linenr);

   const char * restrict start = vm->data.interp->buf, * restrict end;
   linenr--;
   unsigned linenr_max = linenr;
   do {
      end = strpbrk(start, "\n");
      if (!end) {
         end = &start[strlen(start)];
         break;  //just in case
      }
      if (linenr >= 1) { start = end+1; }
      linenr--;
   } while (linenr < linenr_max);

   error("%*s%.*s\n", poffset, "", (int)(end-start), start);

   vm->data.interp->err_shown = true;

   /* Very important, reset ip, so that free happends at the right place */
   vm->code.ip = vm->instr_start;

   return status;
}
