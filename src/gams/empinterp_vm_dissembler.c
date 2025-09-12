#include "empinterp_priv.h"
#include "empinterp_vm.h"
#include "empinterp_vm_utils.h"
#include "printout.h"

#include "dctmcc.h"

#define READ_BYTE(vm) (unsigned)(*vm->code.ip++)
#define READ_SHORT(vm) \
   (vm->code.ip += 2, \
      (unsigned)((vm->code.ip[-2] << 8) | vm->code.ip[-1]))

/** Opcode argument type. Only used in the dissassember */
enum OpCodeArgType {
   OPARG_NONE,
   OPARG_BYTE,
   OPARG_SHORT,
   OPARG_IDX,
   OPARG_LIDX,
   OPARG_LIDX_ASSIGN,
   OPARG_GIDX,
   OPARG_GSET_IDX,
   OPARG_LSET_IDX,
   OPARG_LVEC_IDX,
   OPARG_UINT_IDX,
   OPARG_INT_IDX,
   OPARG_APICALL,
   OPARG_NEWOBJ,
   OPARG_JUMP_BCK,
   OPARG_JUMP_FWD,
   OPARG_IDENT_TYPE,
   OPARG_IDENT_IDX,
   OPARG_LINKLABELS_SYNCANDSTORE,
   OPARG_DUALSLABEL_GIDX,
   OPARG_DUALSLABEL_SYNCANDSTORE,
   OPARG_GMSSYMITER,
   OPARG_ARCOBJ,
   OPARG_REGENTRY,
   OPARG_CUSTOM,
};

typedef struct {
   enum OpCodeArgType type;
} OpCodeArg;

typedef struct {
   uint8_t argc;
   OpCodeArg argv;
} OpCodeArgs;

const OpCodeArg opcodes_argv[][OP_MAXCODE] = {
   [OP_PUSH_GIDX] = {{OPARG_GIDX},},
   [OP_PUSH_BYTE] = {{OPARG_BYTE},},
   [OP_PUSH_LIDX] = {{OPARG_LIDX},},
   [OP_UPDATE_LOOPVAR] = {{OPARG_LIDX_ASSIGN},{OPARG_LIDX},{OPARG_IDENT_TYPE},{OPARG_IDENT_IDX}},
   [OP_PUSH_VMUINT] = {{OPARG_UINT_IDX},},
   [OP_PUSH_VMINT] = {{OPARG_INT_IDX},},
   [OP_POP] = {{OPARG_NONE},},
   [OP_NIL] = {{OPARG_NONE},},
   [OP_TRUE] = {{OPARG_NONE},},
   [OP_FALSE] = {{OPARG_NONE},},
   [OP_LOCAL_COPYFROM_GIDX] = {{OPARG_LIDX_ASSIGN}, {OPARG_GIDX},},
   [OP_GMSSYMITER_SETFROM_LOOPVAR] = {{OPARG_GMSSYMITER}, {OPARG_IDX}, {OPARG_LIDX}},
   [OP_REGENTRY_SETFROM_LOOPVAR] = {{OPARG_REGENTRY}, {OPARG_IDX}, {OPARG_LIDX}},
   [OP_LOCAL_COPYTO_LOCAL] = {{OPARG_LIDX}, {OPARG_LIDX}},
   [OP_LOCAL_COPYOBJLEN] = {{OPARG_LIDX}, {OPARG_IDENT_TYPE}, {OPARG_IDENT_IDX}},
   [OP_LOCAL_INC] = {{OPARG_LIDX},},
   [OP_STACKTOP_COPYTO_LOCAL] = {{OPARG_LIDX},},
   [OP_LSET_ADD] = {{OPARG_LSET_IDX},{OPARG_LIDX}},
   [OP_LSET_RESET] = {{OPARG_LSET_IDX},},
   [OP_LVEC_ADD] = {{OPARG_LVEC_IDX},{OPARG_LIDX}},
   [OP_LVEC_RESET] = {{OPARG_LVEC_IDX},},
   [OP_VECTOR_EXTEND] = {{OPARG_LIDX}, {OPARG_LIDX}},
   [OP_OVFPARAM_SYNC] = {{OPARG_LIDX}, {OPARG_GIDX}},
   [OP_EQUAL] = {{OPARG_NONE},},
   [OP_GREATER] = {{OPARG_NONE},},
   [OP_LESS] = {{OPARG_NONE},},
   [OP_ADD] = {{OPARG_NONE},},
   [OP_SUBTRACT] = {{OPARG_NONE},},
   [OP_MULTIPLY] = {{OPARG_NONE},},
   [OP_DIVIDE] = {{OPARG_NONE},},
   [OP_NOT] = {{OPARG_NONE},},
   [OP_NEGATE] = {{OPARG_NONE},},
   [OP_JUMP] = {{OPARG_JUMP_FWD},},
   [OP_JUMP_IF_TRUE] = {{OPARG_JUMP_FWD},},
   [OP_JUMP_IF_FALSE] = {{OPARG_JUMP_FWD},},
   [OP_JUMP_IF_TRUE_NOPOP] = {{OPARG_JUMP_FWD},},
   [OP_JUMP_IF_FALSE_NOPOP] = {{OPARG_JUMP_FWD},},
   [OP_JUMP_BACK] = {{OPARG_JUMP_BCK},},
   [OP_JUMP_BACK_IF_FALSE] = {{OPARG_JUMP_BCK},},
   [OP_GMS_EQUVAR_READ_INIT] = {{OPARG_BYTE},},
   [OP_GMS_EQUVAR_READ_SYNC] = {{OPARG_BYTE},},
   [OP_GMS_SYMBOL_READ_SIMPLE] = {{OPARG_GIDX},},
   [OP_GMS_SYMBOL_READ_EXTEND] = {{OPARG_GIDX},},
   [OP_GMS_MEMBERSHIP_TEST] = {{OPARG_GIDX},},
   [OP_EMPAPI_CALL] = {{OPARG_APICALL},},
   [OP_NEW_OBJ] = {{OPARG_NEWOBJ},},
   [OP_LINKLABELS_INIT] = {{OPARG_GIDX},},
   [OP_LINKLABELS_DUP] = {{OPARG_GIDX},},
   [OP_LINKLABELS_KEYWORDS_UPDATE] = {{OPARG_NONE},},
   [OP_LINKLABELS_SETFROM_LOOPVAR] = {{OPARG_ARCOBJ}, {OPARG_IDX}, {OPARG_LIDX}},
   [OP_LINKLABELS_STORE] = {{OPARG_LINKLABELS_SYNCANDSTORE},},
   [OP_LINKLABELS_FINI] = {{OPARG_NONE},},
   [OP_SET_DAGUID_FROM_REGENTRY] = {{OPARG_NONE},},
   //[OP_DUALSLABEL_ADD] = {{OPARG_DUALSLABEL_GIDX}},
   [OP_DUALSLABEL_STORE] = {{OPARG_DUALSLABEL_SYNCANDSTORE}},
   [OP_VARC_DUAL] = {{OPARG_NONE},},
   [OP_SCALAR_SYMBOL_TRACKER_INIT] = {{OPARG_NONE}},
   [OP_SCALAR_SYMBOL_TRACKER_CHECK] = {{OPARG_NONE}},
   [OP_HACK_SCALAR2VMDATA] = {{OPARG_GIDX}, },
   [OP_END] = {{OPARG_NONE},},
};

const uint8_t opcodes_argc[OP_MAXCODE] = {
   [OP_PUSH_GIDX] = 1,
   [OP_PUSH_BYTE] = 1,
   [OP_PUSH_LIDX] = 1, 
   [OP_UPDATE_LOOPVAR] = 4,
   [OP_PUSH_VMUINT] = 1, 
   [OP_PUSH_VMINT] = 1, 
   [OP_POP] = 0,
   [OP_NIL] = 0,
   [OP_TRUE] = 0,
   [OP_FALSE] = 0,
   [OP_LOCAL_COPYFROM_GIDX] = 2,
   [OP_GMSSYMITER_SETFROM_LOOPVAR] = 3,
   [OP_REGENTRY_SETFROM_LOOPVAR] = 3,
   [OP_LOCAL_COPYTO_LOCAL] = 2,
   [OP_LOCAL_COPYOBJLEN] = 3,
   [OP_LOCAL_INC] = 1,
   [OP_STACKTOP_COPYTO_LOCAL] = 1,
   [OP_LSET_ADD] = 2,
   [OP_LSET_RESET] = 1,
   [OP_LVEC_ADD] = 2,
   [OP_LVEC_RESET] = 1,
   [OP_VECTOR_EXTEND] = 2,
   [OP_OVFPARAM_SYNC] = 2,
   [OP_EQUAL] = 0,
   [OP_GREATER] = 0,
   [OP_LESS] = 0,
   [OP_ADD] = 0,
   [OP_SUBTRACT] = 0,
   [OP_MULTIPLY] = 0,
   [OP_DIVIDE] = 0,
   [OP_NOT] = 0,
   [OP_NEGATE] = 0,
   [OP_JUMP] = 1,
   [OP_JUMP_IF_TRUE] = 1,
   [OP_JUMP_IF_FALSE] = 1,
   [OP_JUMP_IF_TRUE_NOPOP] = 1,
   [OP_JUMP_IF_FALSE_NOPOP] = 1,
   [OP_JUMP_BACK] = 1,
   [OP_JUMP_BACK_IF_FALSE] = 1,
   [OP_GMS_EQUVAR_READ_INIT] = 1,
   [OP_GMS_EQUVAR_READ_SYNC] = 1,
   [OP_GMS_SYMBOL_READ_SIMPLE] = 1,
   [OP_GMS_SYMBOL_READ_EXTEND] = 1,
   [OP_GMS_MEMBERSHIP_TEST] = 1,
   [OP_EMPAPI_CALL] = 1,
   [OP_NEW_OBJ] = 1,
   [OP_LINKLABELS_INIT] = 1,
   [OP_LINKLABELS_DUP] = 1,
   [OP_LINKLABELS_KEYWORDS_UPDATE] = 0,
   [OP_LINKLABELS_SETFROM_LOOPVAR] = 3,
   [OP_LINKLABELS_STORE] = 1,
   [OP_LINKLABELS_FINI] = 0,
//   [OP_DUALSLABEL_ADD] = 1,
   [OP_DUALSLABEL_STORE] = 1,
   [OP_VARC_DUAL] = 0,
   [OP_SCALAR_SYMBOL_TRACKER_INIT] = 0,
   [OP_SCALAR_SYMBOL_TRACKER_CHECK] = 0,
   [OP_SET_DAGUID_FROM_REGENTRY] = 0,
   [OP_HACK_SCALAR2VMDATA] = 1,
   [OP_END] = 0,
};

#define DEFINE_STR() \
 DEFSTR(OP_PUSH_GIDX,"PUSH_GIDX") \
 DEFSTR(OP_PUSH_BYTE,"PUSH_BYTE") \
 DEFSTR(OP_PUSH_LIDX,"PUSH_LIDX") \
 DEFSTR(OP_PUSH_VMUINT,"PUSH_VMUINT") \
 DEFSTR(OP_PUSH_VMINT,"PUSH_VMINT") \
 DEFSTR(OP_UPDATE_LOOPVAR,"UPDATE_LOOPVAR") \
 DEFSTR(OP_POP,"POP") \
 DEFSTR(OP_NIL,"NIL") \
 DEFSTR(OP_TRUE,"TRUE") \
 DEFSTR(OP_FALSE,"FALSE") \
 DEFSTR(OP_GMSSYMITER_SETFROM_LOOPVAR,"GMSSYMITER_SETFROM_LOOPVAR") \
 DEFSTR(OP_REGENTRY_SETFROM_LOOPVAR,"REGENTRY_SETFROM_LOOPVAR") \
 DEFSTR(OP_LOCAL_COPYFROM_GIDX,"LOCAL_COPYFROM_GIDX") \
 DEFSTR(OP_LOCAL_COPYOBJLEN,"LOCAL_COPYOBJLEN") \
 DEFSTR(OP_LOCAL_COPYTO_LOCAL,"LOCAL_COPYTO_LOCAL") \
 DEFSTR(OP_LOCAL_INC,"LOCAL_INC") \
 DEFSTR(OP_STACKTOP_COPYTO_LOCAL,"STACKTOP_COPYTO_LOCAL") \
 DEFSTR(OP_LSET_ADD,"LSET_ADD") \
 DEFSTR(OP_LSET_RESET,"LSET_RESET") \
 DEFSTR(OP_LVEC_ADD,"LVEC_ADD") \
 DEFSTR(OP_LVEC_RESET,"LVEC_RESET") \
 DEFSTR(OP_VECTOR_EXTEND,"VECTOR_EXTEND") \
 DEFSTR(OP_OVFPARAM_SYNC,"OVFPARAM_SYNC") \
 DEFSTR(OP_EQUAL,"EQUAL") \
 DEFSTR(OP_GREATER,"GREATER") \
 DEFSTR(OP_LESS,"LESS") \
 DEFSTR(OP_ADD,"ADD") \
 DEFSTR(OP_SUBTRACT,"SUBTRACT") \
 DEFSTR(OP_MULTIPLY,"MULTIPLY") \
 DEFSTR(OP_DIVIDE,"DIVIDE") \
 DEFSTR(OP_NOT,"NOT") \
 DEFSTR(OP_NEGATE,"NEGATE") \
 DEFSTR(OP_JUMP,"JUMP") \
 DEFSTR(OP_JUMP_IF_TRUE,"JUMP_IF_TRUE") \
 DEFSTR(OP_JUMP_IF_FALSE,"JUMP_IF_FALSE") \
 DEFSTR(OP_JUMP_IF_TRUE_NOPOP,"JUMP_IF_TRUE_NOPOP") \
 DEFSTR(OP_JUMP_IF_FALSE_NOPOP,"JUMP_IF_FALSE_NOPOP") \
 DEFSTR(OP_JUMP_BACK,"JUMP_BACK") \
 DEFSTR(OP_JUMP_BACK_IF_FALSE,"JUMP_BACK_IF_FALSE") \
 DEFSTR(OP_GMS_EQUVAR_READ_INIT,"GMS_EQUVAR_READ_INIT") \
 DEFSTR(OP_GMS_EQUVAR_READ_SYNC,"GMS_EQUVAR_READ_SYNC") \
 DEFSTR(OP_GMS_SYMBOL_READ_SIMPLE,"GMS_SYMBOL_READ_SIMPLE") \
 DEFSTR(OP_GMS_SYMBOL_READ_EXTEND,"GMS_SYMBOL_READ_EXTEND") \
 DEFSTR(OP_GMS_MEMBERSHIP_TEST,"GMS_MEMBERSHIP_TEST") \
 DEFSTR(OP_CALL_API,"CALL_API") \
 DEFSTR(OP_NEW_OBJ,"NEW_OBJ") \
 DEFSTR(OP_LINKLABELS_INIT,"LINKLABELS_INIT") \
 DEFSTR(OP_LINKLABELS_DUP,"LINKLABELS_DUP") \
 DEFSTR(OP_LINKLABELS_KEYWORDS_UPDATE,"OP_LINKLABELS_KEYWORDS_UPDATE") \
 DEFSTR(OP_LINKLABELS_SETFROM_LOOPVAR,"LINKLABELS_SETFROM_LOOPVAR") \
 DEFSTR(OP_LINKLABELS_STORE,"LINKLABELS_STORE") \
 DEFSTR(OP_LINKLABELS_FINI,"LINKLABELS_FINI") \
 DEFSTR(OP_DUALSLABEL_STORE,"DUALSLABEL_STORE") \
 DEFSTR(OP_VARC_DUAL,"VARC_DUAL") \
 DEFSTR(OP_SCALAR_SYMBOL_TRACKER_INIT,"SCALAR_SYMBOL_TRACKER_INIT") \
 DEFSTR(OP_SCALAR_SYMBOL_TRACKER_CHECK,"SCALAR_SYMBOL_TRACKER_CHECK") \
 DEFSTR(OP_SET_DAGUID_FROM_REGENTRY, "SET_DAGUID_FROM_REGENTRY") \
 DEFSTR(OP_HACK_SCALAR2VMDATA, "HACK_SCALAR2VMDATA") \
 DEFSTR(OP_END,"END")
 
//DEFSTR(OP_DUALSLABEL_ADD,"DUALSLABEL_ADD")

#define DEFSTR(id, str) char id[sizeof(str)];

typedef struct {
   union {
      struct {
      DEFINE_STR()
   };
   char dummystr[1];
   };
} OpcodeNames;
#undef DEFSTR

#define DEFSTR(id, str) .id = str,

static const OpcodeNames opnames = {
DEFINE_STR()
};
#undef DEFSTR

#define DEFSTR(id, str) offsetof(OpcodeNames, id),
static const unsigned opnames_offsets[] = {
DEFINE_STR()
};

static_assert(sizeof(opnames_offsets)/sizeof(opnames_offsets[0]) == OP_MAXCODE,
              "Check the sizes of rules and enum emptok_type");



const char * opcodes_name(enum OpCode op)
{
   if (op >= OP_MAXCODE) { return "unknown EmpVm opcode"; }   

   return opnames.dummystr + opnames_offsets[op];
}

static inline int valid_vmidx(unsigned idx, unsigned len, const char* setname)
{
   if (idx >= len) {
      error("\n[empvm] ERROR: argument %u is outside of array '%s' with range [0,%u)\n",
            idx, setname, len);
      return Error_EMPRuntimeError;
   }

   return OK;
}

#define VM_CHK(EXPR) { int status42 = (EXPR); if (status42) status = status42; }

static void print_globals(EmpVm *vm, unsigned mode)
{
   printstr(mode, "\nGlobal table\n");

   for (unsigned i = 0, len = vm->globals.len; i < len; ++i) {

      VmValue v = vm->globals.arr[i];
      printout(mode, "[%5d] %20s", i, vmval_typename(v));

      if (IS_STR(v)) { printout(mode, "%30s\n", AS_STR(v));
      } else if (IS_REGENTRY(v)) {
         DagRegisterEntry *regentry = AS_REGENTRY(v);
         printout(mode, "%30.*s\n", regentry->label_len, regentry->label);
      } else if (IS_ARCOBJ(v)) {
         LinkLabels *linklabel = AS_ARCOBJ(v);
         printout(mode, "%30.*s\n", linklabel->label_len, linklabel->label);
      } else if (IS_GMSSYMITER(v)) {
         VmGmsSymIterator *symiter = AS_GMSSYMITER(v);
         IdentData *sym = &symiter->ident;
         printout(mode, "%30.*s\n", sym->lexeme.len, sym->lexeme.start);
      } else if (IS_INT(v))  {  printout(mode, "%30d\n", AS_INT(v));
      } else if (IS_UINT(v)) {  printout(mode, "%30u\n", AS_UINT(v));
      } else {
         printstr(mode, "\n");
      }

   }
}

int empvm_dissassemble(EmpVm *vm, unsigned mode)
{
   uint8_t *instr_start = vm->code.ip;
   int status = OK;

   print_globals(vm, mode);

   printout(mode, "\n\n%7s  %7s  %28s %20s %20s %20s\n", "op cnt", "addr", "OPCODE NAME",
            "ARG1", "ARG2", "ARG3");

   for (unsigned i = 0, len = vm->code.len, cnt = 0; cnt < len; ++i, cnt = vm->code.ip-instr_start) {
      uint8_t opcode = READ_BYTE(vm);
      if (opcode >= OP_MAXCODE) {
         error("[%5u]  ERROR: Illegal instruction value %u\n", i, opcode);
         return Error_EMPRuntimeError;
      }

      if (opcode == OP_END && cnt != len-1) {
         error("[%5u] ERROR: OP_END before the end of the bytecode\n", i);
         return Error_EMPRuntimeError;
      }

      printout(mode, "[%5u]  [%5td]  %28s ", i, vm->code.ip-instr_start-1, opcodes_name(opcode));
      unsigned argc =  opcodes_argc[opcode];
      const OpCodeArg *argv = opcodes_argv[opcode];
      unsigned val = UINT_MAX;
      uint8_t val8 = UINT8_MAX;
      uint16_t val16;


      for (unsigned j = 0; j < argc; ++j) {
         enum OpCodeArgType type = argv[j].type;

         switch (type) {
         case OPARG_NONE:
            error("\n Illegal argument %u: OPARG_NONE\n", j);
            status = Error_EMPRuntimeError;
            break;
         case OPARG_BYTE:
            val = READ_BYTE(vm);
            printout(mode, "%20u ", val);
            break;
         case OPARG_SHORT:
            val = READ_SHORT(vm);
            printout(mode, "%20u ", val);
            break;
         case OPARG_IDX:
            val = READ_BYTE(vm);
            printout(mode, "[%5u] = ", val);
            break;
         case OPARG_LIDX:
            val = READ_BYTE(vm);
            printout(mode, "%*s@%-6u ", j == 0 ? 20 : 0, "lvar", val);
            break;
         case OPARG_LIDX_ASSIGN:
            val = READ_BYTE(vm);
            assert(j == 0);
            printout(mode, "%*s@%-6u = ", 20, "lvar", val);
            break;
         case OPARG_GIDX: {
            val = READ_GIDX(vm);
            VM_CHK(valid_vmidx(val, vm->globals.len, "vm->globals"));
            VmValue v = vm->globals.arr[val];
            print_vmval_short(mode, v, vm);
            break;
         }
         case OPARG_GSET_IDX:
            val = READ_GIDX(vm);
            VM_CHK(valid_vmidx(val, vm->data.globals->sets.len, "global sets"));
            printout(mode, "%20u ", val);
            break;
         case OPARG_LSET_IDX:
            val = READ_GIDX(vm);
            VM_CHK(valid_vmidx(val, vm->data.globals->localsets.len, "local sets"));
            printout(mode, "%20u ", val);
            break;
         case OPARG_UINT_IDX:
            val = READ_GIDX(vm);
            VM_CHK(valid_vmidx(val, vm->uints.len, "VM uints"));
            printout(mode, "%20u ", val);
            break;
         case OPARG_INT_IDX:
            val = READ_GIDX(vm);
            VM_CHK(valid_vmidx(val, vm->uints.len, "VM int"));
            printout(mode, "%20d ", val);
            break;
         case OPARG_APICALL:
            val8 = READ_BYTE(vm);
            if (val8 >= empapis_len) {
               error("\n Illegal EMPAPI function value %u\n", val8);
               status = Error_EMPRuntimeError;
            }
            printout(mode, "%20s ", empapis_names[val8]);
            break;
         case OPARG_NEWOBJ:
            val8 = READ_BYTE(vm);
            if (val8 >= empnewobjs_len) {
               error("\n Illegal EMPnewobj function value %u\n", val8);
               status = Error_EMPRuntimeError;
            }
            printout(mode, "%20s ", empnewobjs_names[val8]);
            break;
         case OPARG_JUMP_FWD: {
            val16 = READ_SHORT(vm);
            if (&vm->code.ip[val16] > &instr_start[vm->code.len]) {
               error("\n Forward jump too far (%u > %td)\n", val16,
                      &instr_start[vm->code.len]-vm->code.ip);
               status = Error_EMPRuntimeError;
            }
            uint8_t opcode_ = vm->code.ip[val16];
            if (opcode >= OP_MAXCODE) {
               error("Illegal forward jump value %u\n", opcode_);
               status = Error_EMPRuntimeError;
            }
            int offset;
            printout(mode, " @%td%n", (vm->code.ip - instr_start) + val16, &offset);
            printout(mode, "%*s", 20-offset, opcodes_name(opcode_));
            break;
         }
         case OPARG_JUMP_BCK: {
            val16 = READ_SHORT(vm);
            if (val16 > vm->code.ip - instr_start) {
               error("Backward jump value too big %u > %td\n", val16, vm->code.ip - instr_start);
               status = Error_EMPRuntimeError;
            }
            uint8_t opcode_ = vm->code.ip[-val16];
            if (opcode >= OP_MAXCODE) {
               error("Illegal forward jump value %u\n", opcode_);
               status = Error_EMPRuntimeError;
            }
            int offset;
            printout(mode, " @%td%n", (vm->code.ip - instr_start) - val16, &offset);
            printout(mode, "%*s", 20-offset, opcodes_name(opcode_));
            break;
         }
         case OPARG_IDENT_TYPE:
            val8 = READ_BYTE(vm);
            if (val8 >= IdentTypeMaxValue) {
               error("Ident Type value %u unknown\n", val8);
               status = Error_EMPRuntimeError;
            } 
            printout(mode, "%20s ", identtype2str(val8));
            break;
         case OPARG_IDENT_IDX:
            val = READ_GIDX(vm);
            switch (val8) {
            case IdentLocalSet:
               VM_CHK(valid_vmidx(val, vm->data.globals->localsets.len, "local sets"));
               printstr(mode, vm->data.globals->localsets.names[val]);
               break;
            case IdentLocalVector:
               VM_CHK(valid_vmidx(val, vm->data.globals->localvectors.len, "local vectors"));
               printstr(mode, vm->data.globals->localvectors.names[val]);
               break;
            case IdentSet:
               VM_CHK(valid_vmidx(val, vm->data.globals->sets.len, "global sets"));
               printstr(mode, vm->data.globals->sets.names[val]);
               break;
            case IdentVector:
               VM_CHK(valid_vmidx(val, vm->data.globals->vectors.len, "global vectors"));
               printstr(mode, vm->data.globals->vectors.names[val]);
               break;
            default:
               error("[empvm] unexpected ident type %s", identtype2str(val8));
               status = Error_EMPRuntimeError;
            }
            break;

         case OPARG_LINKLABELS_SYNCANDSTORE: {
            uint8_t nargs = READ_BYTE(vm);
            assert(nargs < GMS_MAX_INDEX_DIM);

            printout(mode, "%2u  ", nargs);
            for (unsigned k = 0; k < nargs; ++k) {
               val = READ_BYTE(vm);
               printout(mode, "%20u ", val);
            }
            printout(mode, "\n");
            break;
         }

         case OPARG_DUALSLABEL_GIDX: {
            GIDX_TYPE gidx = READ_GIDX(vm);
            VM_CHK(valid_vmidx(gidx, vm->data.dualslabels->len, "vm->dualslabels"));

            // HACK: improve output here?
            printout(mode, "%20u ", gidx);

            printout(mode, "\n");
            break;
         }

         case OPARG_DUALSLABEL_SYNCANDSTORE: {
            GIDX_TYPE gidx = READ_GIDX(vm);
            VM_CHK(valid_vmidx(gidx, vm->data.dualslabels->len, "vm->dualslabels"));
            printout(mode, "%20u ", gidx);

            uint8_t nargs = READ_BYTE(vm);
            assert(nargs < GMS_MAX_INDEX_DIM);

            printout(mode, "%2u  ", nargs);
            for (unsigned k = 0; k < nargs; ++k) {
               val = READ_BYTE(vm);
               printout(mode, "%20u ", val);
            }

            printout(mode, "\n");
            break;
         }
         case OPARG_GMSSYMITER: {
            val = READ_GIDX(vm);
            VM_CHK(valid_vmidx(val, vm->globals.len, "vm->globals"));
            VmValue v = vm->globals.arr[val];
            if (!IS_GMSSYMITER(v)) {
               error("\n\nERROR: argument should be a GmsSymbIterator, it is rather a %s\n\n", vmval_typename(v));
               status = Error_EMPRuntimeError;
            }
            print_vmval_short(mode, v, vm);
            break;
         }
         case OPARG_ARCOBJ: {
            val = READ_GIDX(vm);
            VM_CHK(valid_vmidx(val, vm->globals.len, "vm->globals"));
            VmValue v = vm->globals.arr[val];
            if (!IS_ARCOBJ(v)) {
               error("\n\nERROR: argument should be an arc object, it is rather a %s\n\n", vmval_typename(v));
               status = Error_EMPRuntimeError;
            }
            print_vmval_short(mode, v, vm);
            break;
         }
         case OPARG_REGENTRY: {
            val = READ_GIDX(vm);
            VM_CHK(valid_vmidx(val, vm->globals.len, "vm->globals"));
            VmValue v = vm->globals.arr[val];
            if (!IS_REGENTRY(v)) {
               error("\n\nERROR: argument should be a register entry, it is rather a %s\n\n", vmval_typename(v));
               status = Error_EMPRuntimeError;
            }
            print_vmval_short(mode, v, vm);
            break;
         }
         case OPARG_CUSTOM:
            printout(mode, "%20s ", "custom arg");
            continue;
         default:
            error("\n Illegal argument %u: %u\n", j, type);
            status = Error_EMPRuntimeError;
         }

      }

      printstr(mode, "\n");

   }

   vm->code.ip = instr_start;

   if (status != OK) {
      errormsg("[empvm] ERROR: bytecode is not correct, see the above error message(s)\n");
   }

   return status;
}
