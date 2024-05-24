#include "gamsapi_utils.h"
#include "reshop_config.h"

#include <stdarg.h>

#include "empinfo.h"
#include "empinterp.h"
#include "empinterp_edgebuilder.h"
#include "empinterp_ops_utils.h"
#include "empinterp_priv.h"
#include "empinterp_utils.h"
#include "empinterp_vm.h"
#include "empinterp_vm_compiler.h"
#include "empinterp_vm_utils.h"
#include "empparser_priv.h"
#include "empparser_utils.h"
#include "mathprgm.h"
#include "ovf_parameter.h"
#include "printout.h"

typedef struct {
   IdentType type;
   Lexeme lexeme;
   unsigned depth;
   unsigned idx;
} LocalVar;

typedef struct {
   const char *name;
   unsigned name_gidx;
   unsigned params_gidx;
   OvfParamList *params;
   const OvfParamDefList *paramsdef;
   bool active;
} OvfDeclData;

typedef struct {
   bool active;
  // unsigned gidx;
} GmsFilterData;

typedef struct {
   unsigned depth;
   unsigned addr;
} Jump;

typedef struct Jumps {
   unsigned len;
   unsigned max;
   Jump *list;
} Jumps;

#define RHP_LOCAL_SCOPE
#define RHP_LIST_PREFIX jumps
#define RHP_LIST_TYPE Jumps
#define RHP_ELT_TYPE Jump
#define RHP_ELT_INVALID ((Jump){.depth = UINT_MAX, .addr = UINT_MAX})
#include "list_generic.inc"

typedef struct empvm_compiler {
   LocalVar locals[UINT8_COUNT];
   unsigned local_count;
   uint8_t  stacked_tokens[UINT8_COUNT];
   uint8_t  stacked_tokens_count;
   unsigned scope_depth;
   unsigned vmstack_depth;
   unsigned vmstack_max;

   OvfDeclData ovfdecl;
   GmsFilterData gmsfilter;
   unsigned regentry_gidx;
   Jumps truey_jumps;
   Jumps falsey_jumps;
   EmpVm *vm;
} Compiler;

typedef struct empvm_tape {
   VmCode *code;
   unsigned linenr;
} Tape;

const char * identtype_str(IdentType type)
{
   const char *identtype_str_[IdentTypeMaxValue] = {
   [IdentNotFound]        = "not found",
   [IdentLoopIterator]    = "loop iterator",
   [IdentGmdSet]           = "GAMS set (1D, from GMD)",
   [IdentGdxSet]           = "GAMS set (1D, from GDX)",
   [IdentLocalSet]        = "local GAMS set",
   [IdentGdxMultiSet]        = "GAMS set (multidimensional, from GDX)",
   [IdentGmdMultiSet]        = "GAMS set (multidimensional, from GMD)",
   [IdentLocalMultiSet]   = "local GAMS multiset",
   [IdentScalar]          = "GAMS scalar parameter",
   [IdentGmdScalar]          = "GAMS scalar parameter",
   [IdentVector]          = "GAMS vector parameter",
   [IdentGmdVector]          = "GAMS vector parameter",
   [IdentParam]           = "GAMS multidimensional parameter",
   [IdentGmdParam]           = "GAMS multidimensional parameter",
   [IdentLocalScalar]     = "local scalar",
   [IdentLocalVector]     = "local vector",
   [IdentLocalParam]      = "local parameter",
   [IdentInternalIndex]   = "internal indexing variable",
   [IdentInternalMax]     = "internal max variable",
   [IdentUEL]             = "GAMS UEL",
   [IdentUniversalSet]    = "GAMS Universal Set",
   [IdentVar]             = "GAMS Variable",
   [IdentEqu]             = "GAMS Equation",
   };

   if (type < IdentTypeMaxValue) {
      return identtype_str_[type];
   }

   return "INVALID IDENTIFIER";
}

NONNULL static int tok2ident(Token * restrict tok, IdentData * restrict ident) 
{
   GamsSymData *symdat = &tok->symdat;

   switch (tok->type) {
   case TOK_GMS_SET:
   case TOK_GMS_MULTISET:
   case TOK_GMS_PARAM:
      break;
   default:
      return runtime_error(tok->linenr);
   }

   ident->type = symdat->type;
   ident->dim = symdat->dim;
   ident->idx = symdat->idx;
   ident->lexeme.start = tok->start;
   ident->lexeme.len = tok->len;
   ident->lexeme.linenr = tok->linenr;

   return OK;
}

static int vm_gmssymiter_alloc(Compiler* restrict c, const IdentData *ident,
                               VmGmsSymIterator **f, unsigned *gidx);

/*
#define jumps_add_verbose(jumps, j) (\
trace_empparser("%s :: JUMP: adding to " #jumps " at pos %u; depth = %5u; addr = %5u\n", __func__, (jumps)->len, (j).depth, (j).addr), \
 jumps_add(jumps, j))
*/

#define jumps_add_verbose jumps_add

/* ---------------------------------------------------------------------
 * Short design note:
 *
 * - The compiler->locals are used in the VM for scoped variables and loop ones
 *   The opcodes OP_LOCAL operates on them and alwats take as first instruction
 *   the local index.
 * --------------------------------------------------------------------- */


static inline void iterators_init(LoopIterators* restrict iterators,
                                  GmsIndicesData* restrict gmsindices)
{
   unsigned nargs = gmsindices->nargs;
   iterators->size = nargs;
   iterators->loopobj_gidx = UINT_MAX;
   memcpy(iterators->idents, gmsindices->idents, nargs*sizeof(IdentData));
}

#define UPDATE_STACK_MAX(c, argc) \
   (c)->vmstack_max = MAX((c)->vmstack_max, (c)->vmstack_depth + (argc))


static inline int emit_byte(Tape *tape, uint8_t byte) {
   return vmcode_add(tape->code, byte, tape->linenr);
}

static inline int _emit_bytes(Tape *tape, unsigned argc, ...) {
   int status = OK;
   va_list ap;

   va_start(ap, argc);

   for (unsigned i = 0; i < argc; ++i) {
      status = emit_byte(tape, (uint8_t)va_arg(ap, unsigned));
      if (status != OK) {
         goto _exit;
      }
   }

_exit:
   va_end(ap);
   return status;
}

NONNULL static Compiler* ensure_vm_mode(Interpreter *interp)
{
   if (interp->ops->type != ParserOpsEmb) {
      interp->ops = &parser_ops_compiler;
   }

   Compiler *c = interp->compiler;

   if (!c) {
      c = compiler_init(interp);
   }

   return c;
}

#define emit_bytes(tape, ...) _emit_bytes(tape, PP_NARG(__VA_ARGS__), __VA_ARGS__); 

static inline int emit_short(Tape *tape, uint16_t short_) {
   S_CHECK(emit_byte(tape, (short_ >> 8) & 0xff));
   return emit_byte(tape, short_ & 0xff);
}

static inline int _emit_jumpback_len(Tape *tape, unsigned loop_start)
{
   unsigned offset = tape->code->len - loop_start + 2;
   if (offset > UINT16_MAX) errormsg("Loop body too large.");

   return emit_short(tape, offset);
}

UNUSED static inline int emit_jump_back(Tape *tape, unsigned loop_start) {
   S_CHECK(emit_byte(tape, OP_JUMP_BACK));
   return _emit_jumpback_len(tape, loop_start);
}

static inline int emit_jump_back_false(Tape *tape, unsigned loop_start) {
   S_CHECK(emit_byte(tape, OP_JUMP_BACK_IF_FALSE));
   return _emit_jumpback_len(tape, loop_start);
}

static inline int emit_jump(Tape *tape, uint8_t instr, unsigned *jump_addr) {
   S_CHECK(emit_byte(tape, instr));
   S_CHECK(emit_short(tape, 0xffff));

   assert(tape->code->len > 2);
   *jump_addr = tape->code->len - 2;

   return OK;
}

/* TODO(cleanup): this should be used rather than direct acess */
UNUSED static inline int make_global(EmpVm *vm, VmValue value, GIDX_TYPE *i) {
   S_CHECK(vmvals_add(&vm->globals, value));
   unsigned idx = vm->globals.len-1;
   if (idx > GIDX_MAX) {
      errormsg("[empcompiler] Too many constants ");
      return Error_EMPRuntimeError;
   }
   *i = (GIDX_TYPE)idx;

   return OK;
}

static int patch_jump(Tape *tape, unsigned jump_addr) {
   // -2 to adjust for the bytecode for the jump jump_addr itself.
   assert(jump_addr >= 2 && tape->code->len > jump_addr - 2);
   unsigned jump = tape->code->len - jump_addr - 2;

   if (jump >= UINT16_MAX) {
      errormsg("[empcompiler] jump too large");
      return Error_EMPRuntimeError;
   }

   trace_empparser("[empcompiler] PATCHING jump @%u to %u\n", jump_addr, jump);

   tape->code->ip[jump_addr] = (jump >> 8) & 0xff;
   tape->code->ip[jump_addr + 1] = jump & 0xff;

   return OK;
}

static inline void begin_scope(Compiler * restrict c, const char *fn)
{
   c->scope_depth++;
   trace_empparser("[empcompiler] scope depth is now %u in %s.\n", (c)->scope_depth, fn);
}

static inline int end_scope(Interpreter *interp, UNUSED Tape* tape) {
   Compiler * restrict c = interp->compiler;
   c->scope_depth--;

   trace_empparser("[empcompiler] scope depth is %u at line %u.\n", c->scope_depth,
                   tape->linenr);
   LocalVar *lvar;
   unsigned old_cnt = c->local_count;
   while (c->local_count > 0 && ((lvar = &c->locals[c->local_count - 1])
      && lvar->depth < UINT_MAX && lvar->depth > c->scope_depth)) {
      c->local_count--;
   }

   if (O_Output & PO_TRACE_EMPPARSER) {
      for (unsigned i = c->local_count; i < old_cnt; ++i) {
         lvar = &c->locals[i];
         trace_empparser("[empcompiler] locals: removing '%.*s' of type %s\n",
                         lvar->lexeme.len, lvar->lexeme.start,
                         identtype_str(lvar->type));
      }

      if (c->local_count > 0) {
         trace_empparsermsg("[empcompiler] locals: remaining locals are:\n");
         for (unsigned i = 0, len = c->local_count; i < len; ++i) {
            lvar = &c->locals[i];
            trace_empparser("[%5u] '%.*s' of type %s\n", i,
                            lvar->lexeme.len, lvar->lexeme.start,
                            identtype_str(lvar->type));
         }
      }
   }

   if (c->scope_depth == 0) {
      assert(c->truey_jumps.len == 0 && c->falsey_jumps.len == 0);
      /* no keyword is active */
      interp_resetlastkw(interp);

      /* TODO: think whether we want to do a "symbolic" check in Emb mode? */
      if (!embmode(interp) && c->vm->code.len > 0) {
         trace_empparsermsg("[empinterp] VM execution\n");
         S_CHECK(vmcode_add(&c->vm->code, OP_END, interp->linenr));
         S_CHECK(empvm_run(c->vm));
      }

      c->vm->code.len = 0;

      if (interp->ops->type == ParserOpsCompiler) {
         interp->ops = &parser_ops_imm;
      }
   }

   return OK;
}

static inline bool _ident_equal(Lexeme *a, Lexeme *b) {
   if (a->len != b->len) return false;
   return strncasecmp(a->start, b->start, a->len) == 0;
}

static inline bool is_internal(const LocalVar *l)
{
   return l->type == IdentInternalIndex;
}

static inline IdentType localvar_gettype(const LocalVar *l)
{
   return l->type;
}

bool resolve_local(Compiler* c, IdentData *ident)
{
   /* unsigned does wrap */
   for (unsigned len = c->local_count, i = len-1; i < len; i--) {
      LocalVar* local = &c->locals[i];
      if (is_internal(local)) continue;

      if (_ident_equal(&ident->lexeme, &local->lexeme)) {
         if (local->depth == UINT_MAX) {
            errormsg("[empcompiler] Can't read local variable in its own initializer.");
         }
         ident->idx = local->idx;
         ident->type = localvar_gettype(local);
         switch(ident->type) {
         case IdentLoopIterator:
            ident->idx = i;
         FALLTHRU
         case IdentScalar:
            ident->dim = 0;
            break;
         case IdentLocalSet:
         case IdentLocalVector:
            ident->dim = 1;
            break;
         default:
            error("%s :: case '%s' not handled", __func__, identtype_str(ident->type));
            return false;
         }
         return true;
      }
   }

   return false;
}

static int add_localvar(Compiler *c, Lexeme lexeme, LIDX_TYPE *lidx, IdentType type)
{
   if (c->local_count == UINT8_COUNT) {
      errormsg("Too many local variables in function.");
      return Error_EMPRuntimeError;
   }

   /* WARNING: do not move this line */
   *lidx = c->local_count;

   LocalVar* local = &c->locals[c->local_count++];
   local->lexeme = lexeme;
   local->depth = UINT_MAX;
   local->type = type;
   local->idx = UINT_MAX;

   trace_empparser("[empcompiler] locals: Adding '%.*s' of type %s\n",
                   local->lexeme.len, local->lexeme.start,
                   identtype_str(local->type));

   return OK;
}

static int declare_variable(Compiler *c, Lexeme* tok, LIDX_TYPE *lidx, IdentType type)
{
   // We have no concept of global variables
   //  if (c->scope_depth == 0) return OK;

   for (unsigned len = c->local_count, i = len-1; i < len; i--) {
      LocalVar* local = &c->locals[i];
      if (local->depth != UINT_MAX && local->depth < c->scope_depth) {
         break; // [negative]
      }

      if (_ident_equal(tok, &local->lexeme)) {
         error("[empcompiler] line %u: the variable '%s' is already defined on line %u, "
               "which is in the same scope\n", tok->linenr, tok->start, local->lexeme.linenr);
         return Error_EMPIncorrectSyntax;
      }
   }

   return add_localvar(c, *tok, lidx, type);
}

static void finalize_variable(Compiler *c, unsigned lvar_idx)
{
   assert(lvar_idx < c->local_count);
   if (O_Output & PO_TRACE_EMPPARSER) {
      LocalVar *local = &c->locals[lvar_idx];
      trace_empparser("[empcompiler] locals: Setting depth of '%.*s' to %u\n",
                      local->lexeme.len, local->lexeme.start,
                      c->scope_depth);
   }

   c->locals[lvar_idx].depth = c->scope_depth;
}

static int gen_localvarname(const Lexeme *lexeme, const char *suffix,
                            const char **name, unsigned *namelen)
{
   const char *ident = lexeme->start;
   assert(ident);
   unsigned identlen = lexeme->len;

   unsigned suffix_len = strlen(suffix);
   unsigned lnamelen = identlen + suffix_len; 

   char *lname;
   MALLOC_(lname, char, lnamelen+1);
   memcpy(lname, ident, identlen*sizeof(char));
   memcpy(&lname[identlen], suffix, suffix_len*sizeof(char));
   lname[lnamelen] = '\0';

   *name = lname;
   *namelen = lnamelen;

   return OK;
}

static int declare_localvar(Interpreter * restrict interp, Tape *tape,
                            const Lexeme * restrict lexeme,
                            const char * restrict suffix,
                            IdentType type, unsigned initval_gidx,
                            LIDX_TYPE * restrict idx)
{
   assert(initval_gidx <= GIDX_MAX);
   Lexeme lvar_lexeme = {.linenr = interp->linenr};
   Compiler * restrict c = interp->compiler;
   LIDX_TYPE lidx;

   S_CHECK(gen_localvarname(lexeme, suffix, &lvar_lexeme.start, &lvar_lexeme.len))
   S_CHECK(add_localvar(c, lvar_lexeme, &lidx, type));

   if (initval_gidx < GIDX_MAX) {
      S_CHECK(emit_bytes(tape, OP_LOCAL_COPYFROM_GIDX, (uint8_t)lidx));
      S_CHECK(EMIT_GIDX(tape, initval_gidx));
   }

   c->locals[lidx].depth = c->scope_depth;

   *idx = lidx;

   return OK;
}

/**
 * @brief Create the local variables for going over the iterators
 *
 * @param interp     the interpreter
 * @param tape       the empvm tape
 * @param iterators  the iterators
 *
 * @return           the error code
 */
static int loop_init(Interpreter * restrict interp, Tape * restrict tape,
                     LoopIterators * restrict iterators)
{
   const unsigned nargs = iterators->size;
   const IdentData * restrict loopidents = iterators->idents;
   IteratorData * restrict loopiters = iterators->iters;
   assert(nargs < GMS_MAX_INDEX_DIM);
   EmpVm * restrict vm = interp->compiler->vm;

   /* ---------------------------------------------------------------------
    * Loop initialization: go over all arguments and:
    *  - declare 2 local variables per arguments: the loop index and the set element
    *  - Collect all the length of 
    *  - If we loop over a dynamic set, get the size of that set
    * --------------------------------------------------------------------- */

   for (unsigned i = 0; i < nargs; ++i) {

      const IdentData * restrict ident = &loopidents[i];
      IteratorData * restrict iterator = &loopiters[i];

      IdentType type = ident->type;
      switch (type) {
      case IdentGdxSet: {
         IntArray loopset = namedints_at(&interp->globals.sets, ident->idx);
         assert(valid_set(loopset));

         S_CHECK(rhp_uint_add(&vm->uints, loopset.len));
         iterator->idxmax_gidx = vm->uints.len-1;
         break;
      }
      case IdentGmdSet: {
         assert(interp->gmd);
         int nrecs;
         GMD_CHK(gmdSymbolInfo, interp->gmd, ident->ptr, GMD_NRRECORDS, &nrecs, NULL, NULL);
         if (nrecs < 0) {
            return runtime_error(interp->linenr);
         }
         S_CHECK(rhp_uint_add(&vm->uints, (unsigned)nrecs));
         iterator->idxmax_gidx = vm->uints.len-1;
         break;
      }
      case IdentLocalSet:
         /* We need to query the size of the dynamic local set */
         S_CHECK(declare_localvar(interp, tape, &ident->lexeme, "_max",
                                  IdentInternalMax, GIDX_MAX, &iterator->idxmax_lidx));

         S_CHECK(emit_bytes(tape, OP_LOCAL_COPYOBJLEN, iterator->idxmax_lidx, type));
         S_CHECK(EMIT_GIDX(tape, ident->idx));

            /* --------------------------------------------------------------
             * We check if some sets are empty
             *
             * TODO: this could be done elsewhere, but we need to review the
             * codegen for the FOR loop.
             * ------------------------------------------------------------- */

         S_CHECK(emit_bytes(tape, OP_PUSH_BYTE, 0, OP_PUSH_LIDX,
                            iterator->idxmax_lidx, OP_EQUAL));

         Jump jump = { .depth = interp->compiler->scope_depth };
         S_CHECK(emit_jump(tape, OP_JUMP_IF_TRUE, &jump.addr));
         S_CHECK(jumps_add_verbose(&interp->compiler->truey_jumps, jump));

         break;
      default:
         return runtime_error(interp->linenr);
      }

      S_CHECK(declare_localvar(interp, tape, &ident->lexeme, "_idx",
                               IdentInternalIndex, CstZeroUInt, &iterator->idx_lidx));

      S_CHECK(declare_localvar(interp, tape, &ident->lexeme, "",
                               IdentLoopIterator, CstZeroUInt, &iterator->iter_lidx));

   }



   return OK;
}

/* Check that an opcode is as expected */
static int loopobj_chk_opcode(EmpVmOpCode given, EmpVmOpCode expected)
{
   if (given != expected) {
      error("[empcompiler] ERROR: expected opcode is %s, got %s\n",
            opcodes_name(expected), opcodes_name(given));
      return Error_RuntimeError;
   }
   return OK;
}


/**
 * @brief Initialize and codegen the update to the loop variables
 *
 * @param interp     the interpreter
 * @param tape       the empvm tape
 * @param iterators  the iterators
 *
 * @return           the error code
 */
int loop_initandstart(Interpreter * restrict interp, Tape * restrict tape,
                      LoopIterators * restrict iterators)
{
   const unsigned nargs = iterators->size;
   const IdentData * restrict loopidents = iterators->idents;
   IteratorData * restrict loopiters = iterators->iters;

   S_CHECK(loop_init(interp, tape, iterators));

   /* ---------------------------------------------------------------------
    * Step 2:  Start of the loop code:
    *          s_elt  <-  s[a_idx]    for s in loopsets
    *
    * We also record the start of the code loop
    * --------------------------------------------------------------------- */

   unsigned loopobj_gidx = iterators->loopobj_gidx;
   EmpVmOpCode opcode_loopobj_update = iterators->loopobj_opcode;

   if (loopobj_gidx < UINT_MAX) {

      VmValueArray *globals = &interp->compiler->vm->globals;
      if (loopobj_gidx >= globals->len) {
         error("[empcompiler] ERROR: global object index %u is not in [0,%u)\n",
               loopobj_gidx, globals->len);
         return Error_EMPRuntimeError;
      }

      VmValue v = globals->arr[loopobj_gidx];
      EmpVmOpCode opcode_expected;
      if (IS_GMSSYMITER(v)) {
         opcode_expected = OP_GMSSYMITER_SETFROM_LOOPVAR;
      } else if (IS_REGENTRY(v)) {
         opcode_expected = OP_REGENTRY_SETFROM_LOOPVAR;
      } else if (IS_ARCOBJ(v)) {
         opcode_expected = OP_EDGEOBJ_SETFROM_LOOPVAR;
      } else {
         error("[empcompiler] ERROR: global object at index %u has invalid type "
               "%s\n", loopobj_gidx, vmval_typename(v));
         return Error_EMPRuntimeError;
      }

      S_CHECK(loopobj_chk_opcode(opcode_loopobj_update, opcode_expected));
   }

  /* ----------------------------------------------------------------------
   * Action to perform:
   * - record the position in the tape at the loop start (for the jump back)
   * - codegen the update the update of the loop variable
   * - if there is an object associated with the loop, codegen the update of
   *   the index corresponding to that iterator
   * ---------------------------------------------------------------------- */

   for (unsigned i = 0; i < nargs; ++i) {

      loopiters[i].tapepos_at_loopstart = tape->code->len;
      const IdentData * restrict ident = &loopidents[i];
      IteratorData * restrict iterator = &loopiters[i];

      /* a_elt  <-  a_set[a_idx] */
      S_CHECK(emit_bytes(tape, OP_UPDATE_LOOPVAR, iterator->iter_lidx,
                         loopiters[i].idx_lidx, ident->type));
      S_CHECK(EMIT_GIDX(tape, ident->idx));

      if (loopobj_gidx < UINT_MAX) {
         assert(iterators->iteridx2dim[i] < GMS_MAX_INDEX_DIM);
         S_CHECK(emit_bytes(tape, opcode_loopobj_update));
         S_CHECK(EMIT_GIDX(tape, loopobj_gidx));
         S_CHECK(emit_bytes(tape, iterators->iteridx2dim[i], iterator->iter_lidx));
      }

   }

   return OK;
}


static inline int patch_jumps(Jumps * restrict jumps, Tape * restrict tape, unsigned depth)
{
   assert(depth > 0);
   if (jumps->len == 0) { trace_empparsermsg("[empcompiler] JUMP: nothing to patch\n"); return OK; }

   for (unsigned len = jumps->len, i = len-1; i < len; --i) {
      const Jump * jump = &jumps->list[i];
      assert(jump->depth < UINT_MAX && jump->depth > 0 && jump->addr < UINT_MAX);

      /* ---------------------------------------------------------------------
       * Exit condition: we reached the end of the jumps we want to patch
       * --------------------------------------------------------------------- */

      if (jump->depth < depth) {
         jumps->len = i+1;

         return OK;
      }

      S_CHECK(patch_jump(tape, jump->addr));
   }

   jumps->len = 0;

   trace_empparsermsg("[empcompiler] JUMP: no more jumps\n");

   return OK;
}

NONNULL static inline
int loop_increment(Tape * restrict tape, LoopIterators* restrict iterators)
{
   unsigned nargs = iterators->size;
   const IdentData* restrict idents = iterators->idents;
   IteratorData* restrict iters = iterators->iters;
   /* a_idx++; if (a_idx < a_max); jump to loop_start */
   for (unsigned i = nargs-1; i < nargs; --i) {
      const IdentData * restrict ident = &idents[i];
      IteratorData * restrict iter = &iters[i];
      unsigned idx_i = iter->idx_lidx;


      S_CHECK(emit_bytes(tape, OP_LOCAL_INC, idx_i, OP_PUSH_LIDX, idx_i));

      switch (ident->type) {
      case IdentGdxSet:
      case IdentGmdSet:
         S_CHECK(emit_byte(tape, OP_PUSH_VMUINT));
         S_CHECK(EMIT_GIDX(tape, iter->idxmax_gidx));
         break;
      case IdentLocalSet:
         S_CHECK(emit_bytes(tape, OP_PUSH_LIDX, iter->idxmax_lidx));
         break;
      default:
         error("%s :: unsupported loop index %s", __func__, identtype_str(ident->type));
         return Error_NotImplemented;
      }
      S_CHECK(emit_byte(tape, OP_EQUAL));

      assert(iters[i].tapepos_at_loopstart < UINT_MAX);
      S_CHECK(emit_jump_back_false(tape, iters[i].tapepos_at_loopstart));

      if (i > 0) {
         S_CHECK(emit_bytes(tape, OP_LOCAL_COPYFROM_GIDX, idx_i));
         S_CHECK(EMIT_GIDX(tape, CstZeroUInt));
      }
   }

   return OK;
}

DBGUSED static inline
int no_outstanding_jump(Jumps * restrict jumps, unsigned depth)
{
   if (O_Output & PO_TRACE_EMPPARSER) {
   trace_empparsermsg("[empcompiler] outstanding jumps:\n");
      for (unsigned len = jumps->len, i = len-1; i < len; --i) {
         Jump * jump = &jumps->list[i];
         trace_empparser("[%5u]: depth: %5u; addr: %5u\n", i, jump->depth, jump->addr);
      }
   }

   for (unsigned len = jumps->len, i = len-1; i < len; --i) {
      const Jump * jump = &jumps->list[i];
      assert(jump->depth < UINT_MAX && jump->depth > 0 && jump->addr < UINT_MAX);

      if (jump->depth >= depth) {
         return false;
      }

   }

   return true;
}

static int dctsymb2ident(const Token * restrict tok, IdentData *equvar)
{
   switch (tok->type) {
   case TOK_GMS_EQU:
      equvar->type = IdentEqu;
      break;
   case TOK_GMS_VAR:
      equvar->type = IdentVar;
      break;
   default:
      error("[empcompiler] ERROR line %u: token '%.*s' is of type '%s', expecting "
            "'%s' or '%s'.\n", tok->linenr, emptok_getstrlen(tok),
            emptok_getstrstart(tok), toktype2str(tok->type), toktype2str(TOK_GMS_EQU),
            toktype2str(TOK_GMS_VAR));
      return Error_EMPRuntimeError;
   }

   equvar->dim = tok->symdat.dim;
   equvar->idx = tok->symdat.idx;
   equvar->lexeme.linenr = tok->linenr;
   equvar->lexeme.len = tok->len;
   equvar->lexeme.start = tok->start;
   
   return OK;
}

static int vm_gmssymiter_alloc(Compiler* restrict c, const IdentData *ident,
                               VmGmsSymIterator **f, unsigned *gidx)
{
   unsigned dim = ident->dim;
   VmGmsSymIterator *symiter;
   MALLOCBYTES_(symiter, VmGmsSymIterator, sizeof(VmGmsSymIterator) + dim*sizeof(int));
   *f = symiter;

   symiter->symbol = *ident;
   symiter->compact = true;

   memset(symiter->uels, 0, dim*sizeof(int));
   S_CHECK(vmvals_add(&c->vm->globals, GMSSYMITER_VAL(symiter)));

   *gidx = c->vm->globals.len - 1;

   return OK;
}

static int vm_regentry_alloc(Compiler* restrict c, const char *basename,
                             unsigned basename_len, uint8_t dim,
                             DagRegisterEntry **regentry, unsigned *gidx)
{
   DagRegisterEntry *regentry_;
   A_CHECK(regentry_, regentry_new(basename, basename_len, dim));

   memset(regentry_->uels, 0, dim*sizeof(int));
   S_CHECK(vmvals_add(&c->vm->globals, REGENTRY_VAL(regentry_)));

   *regentry = regentry_;
   unsigned gidx_ = c->vm->globals.len - 1;
   *gidx = gidx_;
   c->regentry_gidx = gidx_;

   return OK;
}

static int vm_daglabels_alloc(Compiler* restrict c, DagLabels **dagc,
                              const char* nodename, unsigned nodename_len,
                              uint8_t dim, uint8_t num_vars, unsigned size,
                              ArcType arc_type, unsigned *gidx)
{
   DagLabels *ldagc;
   A_CHECK(ldagc, dag_labels_new(nodename, nodename_len, dim, num_vars, size));
   ldagc->arc_type = arc_type;

   S_CHECK(vmvals_add(&c->vm->globals, ARCOBJ_VAL(ldagc)));

   *dagc = ldagc;
   *gidx = c->vm->globals.len - 1;

   return OK;
}

/**
 * @brief Process the GAMS indices
 *
 * - copy the constant indices in the uels
 * - Fill the iterators with the non-constant ones, if any
 * - Update the loop object if one of the index is an iterator of an outside loop
 *
 * @param      indices    the GAMS indices
 * @param[out] iterators  the loop iterators
 * @param      tape       the tape
 * @param[out] uels       the uels of the loop object
 * @param[out] compact    indicator if the 
 *
 * @return                 the error code
 */
static int gmsindices_process(GmsIndicesData *indices, LoopIterators *iterators,
                              Tape * restrict tape, int * restrict uels, bool *compact)
{
   unsigned loopi = 0;
   GIDX_TYPE loopidx_gidx = iterators->loopobj_gidx;
   EmpVmOpCode upd_opcode = iterators->loopobj_opcode;
   assert(iterators->loopobj_gidx < UINT_MAX);

   for (unsigned i = 0, len = indices->nargs; i < len; ++i) {

      IdentData *idxident = &indices->idents[i];
      switch (idxident->type) {
      case IdentUEL:
         assert(idxident->idx < INT_MAX && idxident->idx > 0);
         uels[i] = (int)idxident->idx;
         *compact = false; //TODO: is this the only case where this is false?
         break;
      case IdentUniversalSet:
         uels[i] = 0;
         break;
      case IdentLoopIterator:
         S_CHECK(emit_byte(tape, upd_opcode));
         S_CHECK(EMIT_GIDX(tape, loopidx_gidx));
         S_CHECK(emit_bytes(tape, i, idxident->idx));
         break; 
      /* -------------------------------------------------------------------
       * We add this ident to the loopiterators
       * ------------------------------------------------------------------- */
      case IdentGdxSet:
      case IdentGmdSet:
      case IdentLocalSet:
         iterators->iteridx2dim[loopi] = i;
         memcpy(&iterators->idents[loopi], idxident, sizeof(*idxident));
         loopi++;
         break;
      default:
         error("[empcompiler] ERROR: unexpected failure: got ident type '%s' "
               "for lexeme '%*s' at position %u.\n", identtype_str(idxident->type),
               idxident->lexeme.len, idxident->lexeme.start, i);
         return Error_RuntimeError;
      }
   }

   iterators->size = loopi;

   return OK;
}

static int gmsindices_ident_chk_size(GmsIndicesData *indices, IdentData *ident,
                                     unsigned linenr)
{
   if (indices->nargs != ident->dim) {
      error("[empcompiler] ERROR line %u: token '%.*s' has dimension %u but %u "
            "indices were given.\n", linenr, ident->lexeme.len,
            ident->lexeme.start, ident->dim, indices->nargs);
      return Error_EMPIncorrectInput;
   }

   return OK;
}

static int gmssymiter_init(Interpreter * restrict interp, IdentData *ident,
                           GmsIndicesData *indices, LoopIterators *iterators,
                           Tape * restrict tape, unsigned * gmssymiter_gidx)
{
   Compiler *c = interp->compiler;

   VmGmsSymIterator *symiter;
   S_CHECK(vm_gmssymiter_alloc(c, ident, &symiter, gmssymiter_gidx));

   if (indices->nargs == 0) {
      iterators->size = 0;
      iterators->loopobj_gidx = UINT_MAX;
      return OK;
   }

   iterators->loopobj_gidx = *gmssymiter_gidx;
   iterators->loopobj_opcode = OP_GMSSYMITER_SETFROM_LOOPVAR;

   S_CHECK(gmsindices_ident_chk_size(indices, ident, interp->linenr));

   return gmsindices_process(indices, iterators, tape, symiter->uels, &symiter->compact);
}


static int arcobj_init(Interpreter * restrict interp, Tape * restrict tape,
                        const char* nodename, unsigned nodename_len,
                        ArcType arc_type, GmsIndicesData *indices,
                        LoopIterators *iterators, unsigned *edgeobj_gidx)
{
   Compiler *c = interp->compiler;

   DagLabels *dagc;
   uint8_t dim = indices->nargs;
   uint8_t num_vars = indices->num_sets + indices->num_localsets;
   unsigned dagl_template_gidx;
   S_CHECK(vm_daglabels_alloc(c, &dagc, nodename, nodename_len, dim,
                              num_vars, 0, arc_type, &dagl_template_gidx));
   *edgeobj_gidx = dagl_template_gidx;

   if (indices->nargs == 0) {
      iterators->size = 0;
      iterators->loopobj_gidx = UINT_MAX;
      return OK;
   }

   iterators->loopobj_gidx = *edgeobj_gidx;
   iterators->loopobj_opcode = OP_EDGEOBJ_SETFROM_LOOPVAR;

   bool dummy;
   S_CHECK(gmsindices_process(indices, iterators, tape, dagc->data, &dummy));

   // TODO: document why we need to duplicate this
   memcpy(&dagc->data[dim], iterators->iteridx2dim, iterators->size * sizeof(int));

   return OK;
}

static int regentry_init(Interpreter * restrict interp, const char *labelname,
                         unsigned labelname_len,  GmsIndicesData *indices,
                         LoopIterators *iterators, Tape * restrict tape,
                         unsigned * regentry_gidx)
{
   Compiler *c = interp->compiler;

   DagRegisterEntry *regentry;
   S_CHECK(vm_regentry_alloc(c, labelname, labelname_len, indices->nargs,
                             &regentry, regentry_gidx));

   assert(indices->nargs > 0);

   iterators->loopobj_gidx = *regentry_gidx;
   iterators->loopobj_opcode = OP_REGENTRY_SETFROM_LOOPVAR;

   bool dummy;
   S_CHECK(gmsindices_process(indices, iterators, tape, regentry->uels, &dummy));

   return OK;
}

static int membership_test(Interpreter * restrict interp, unsigned * restrict p,
                           Compiler *c, Tape *tape, bool do_emit_not)
{
   assert(parser_getcurtoktype(interp) == TOK_IDENT);

   begin_scope(c, __func__);

   IdentData ident_gmsarray;
   S_CHECK(RESOLVE_IDENTAS(interp, &ident_gmsarray, "In a conditional, a GAMS set is expected",
                           IdentLocalSet, IdentGdxSet, IdentGmdSet, IdentGdxMultiSet, IdentGmdMultiSet));

   TokenType toktype;

   trace_empparser("[empcompiler] membership test with type '%s' and dimension %u\n",
                  identtype_str(ident_gmsarray.type), ident_gmsarray.dim);

   /* WARNING: This operates on the current token */
   GmsIndicesData *indices = &interp->indices_membership_test;
   gms_indicesdata_init(indices);

// TODO(URG) is needed?  parser_save(interp);

   S_CHECK(advance(interp, p, &toktype));
   S_CHECK(parser_expect(interp, "'(' expected", TOK_LPAREN));

   S_CHECK(parse_gmsindices(interp, p, indices));

   /* ---------------------------------------------------------------------
    * We have all GAMS indices. Depending on the data, we have different behavior:
    * - If we only have UEL, loopvar, and domain set, then we can just simplify
    *   issue a membership test.
    * - If we have subset or localsets, we need to be more careful.
    * --------------------------------------------------------------------- */

   LoopIterators loopiter;

   unsigned gmsfilter_gidx;
   S_CHECK(gmssymiter_init(interp, &ident_gmsarray, indices, &loopiter, tape, &gmsfilter_gidx));

   S_CHECK(loop_initandstart(interp, tape, &loopiter));

   /* ---------------------------------------------------------------------
    * Main body of the loop: just perform the test and jump out of the loop
    * if true.
    * --------------------------------------------------------------------- */

   S_CHECK(emit_byte(tape, OP_GMS_MEMBERSHIP_TEST));
   S_CHECK(EMIT_GIDX(tape, gmsfilter_gidx));

   if (do_emit_not) {
      S_CHECK(emit_byte(tape, OP_NOT));
   }

   Jump jump = { .depth = c->scope_depth };
   S_CHECK(emit_jump(tape, OP_JUMP_IF_TRUE, &jump.addr));
   S_CHECK(jumps_add_verbose(&c->truey_jumps, jump));

   /* ---------------------------------------------------------------------
    * Step 4:  Increment loop index and check of the condition idx < max
    * --------------------------------------------------------------------- */

   tape->linenr = interp->linenr;

   /* Resolve all remaining jumps linked to a false condition */
   S_CHECK(patch_jumps(&c->falsey_jumps, tape, c->scope_depth));

   S_CHECK(loop_increment(tape, &loopiter));

   /* ---------------------------------------------------------------------
    * We do not resolve truey jumps here
    * --------------------------------------------------------------------- */

   S_CHECK(end_scope(interp, tape));

   c->gmsfilter.active = false;

   return OK;
}

static int parse_sameas(Interpreter * restrict interp, unsigned * restrict p,
                           Compiler *c, Tape *tape, bool do_emit_not)
{

   TokenType toktype;
   S_CHECK(advance(interp, p, &toktype));
   S_CHECK(parser_expect(interp, "'(' expected", TOK_LPAREN));

   S_CHECK(advance(interp, p, &toktype));

   bool has_single_quote = false, has_double_quote = false;

   if (toktype == TOK_SINGLE_QUOTE) {
      has_single_quote = true;
      S_CHECK(advance(interp, p, &toktype));
   } else if (toktype == TOK_DOUBLE_QUOTE) {
      has_double_quote = true;
      S_CHECK(advance(interp, p, &toktype));
   }

   int uel_idx = -1;
   unsigned lvars[2] = {UINT_MAX, UINT_MAX};
   unsigned *plvar = lvars;
   IdentData ident;

   switch (toktype) {
   case TOK_GMS_UEL:
      uel_idx = interp->cur.symdat.idx;
      break;
   case TOK_IDENT:
      RESOLVE_IDENTAS(interp, &ident, "a GAMS parameter is expected",
                   IdentLoopIterator);
      lvars[0] = ident.idx;
      plvar = &lvars[1];
      break;
   default:
      error("%s :: unexpected failure.\n", __func__);
      return Error_RuntimeError;
   }

   S_CHECK(advance(interp, p, &toktype));

   if (has_single_quote) {
      S_CHECK(parser_expect(interp, "Closing \"'\" expected", TOK_SINGLE_QUOTE));
      S_CHECK(advance(interp, p, &toktype));
   } else if (has_double_quote) {
      S_CHECK(parser_expect(interp, "Closing '\"' expected", TOK_DOUBLE_QUOTE));
      S_CHECK(advance(interp, p, &toktype));
   }

   if (toktype != TOK_COMMA) {
      error("[empcompiler] ERROR line %u: 2 arguments are required for GAMS logical operator 'sameas'. "
            "While looking for the ',', got '%.*s' of type '%s'.\n", interp->linenr, emptok_getstrlen(&interp->cur),
             emptok_getstrstart(&interp->cur), toktype2str(toktype));
      return Error_EMPIncorrectSyntax;
   }

   S_CHECK(advance(interp, p, &toktype));
   PARSER_EXPECTS(interp, "a GAMS set or UEL", TOK_IDENT, TOK_SINGLE_QUOTE, TOK_DOUBLE_QUOTE);

   if (uel_idx >= 0 && toktype != TOK_IDENT) {
      error("[empcompiler] ERROR line %u: GAMS logical operator 'sameas' can only have 1 UEL as argument.\n", interp->linenr);
      return Error_EMPIncorrectSyntax;
   }

   has_single_quote = false, has_double_quote = false;

   if (toktype == TOK_SINGLE_QUOTE) {
      has_single_quote = true;
      S_CHECK(advance(interp, p, &toktype));
   } else if (toktype == TOK_DOUBLE_QUOTE) {
      has_double_quote = true;
      S_CHECK(advance(interp, p, &toktype));
   }

   switch (toktype) {
   case TOK_GMS_UEL:
      uel_idx = interp->cur.symdat.idx;
      break;
   case TOK_IDENT:
      RESOLVE_IDENTAS(interp, &ident, "a GAMS parameter is expected",
                   IdentLoopIterator);
      *plvar = ident.idx;
      break;
   default:
      error("%s :: unexpected failure.\n", __func__);
      return Error_RuntimeError;
   }

   S_CHECK(advance(interp, p, &toktype));

   if (has_single_quote) {
      S_CHECK(parser_expect(interp, "Closing \"'\" expected", TOK_SINGLE_QUOTE));
      S_CHECK(advance(interp, p, &toktype));
   } else if (has_double_quote) {
      S_CHECK(parser_expect(interp, "Closing '\"' expected", TOK_DOUBLE_QUOTE));
      S_CHECK(advance(interp, p, &toktype));
   }

   /* ---------------------------------------------------------------------
    * Task here: compare the value stored in 2 local variable.
    * - If a UEL was given, its value is stored in a local variable that we now create
    * - If we have 2 loop iterators, we just read the values of local variables
    * --------------------------------------------------------------------- */

   begin_scope(c, __func__);

   if (uel_idx >= 0) {
      S_CHECK(rhp_int_add(&c->vm->ints, uel_idx));
      unsigned uel_gidx = c->vm->ints.len-1;

      S_CHECK(emit_byte(tape, OP_PUSH_VMINT));
      S_CHECK(EMIT_GIDX(tape, uel_gidx));
   } else {
      S_CHECK(emit_bytes(tape, OP_PUSH_LIDX, lvars[1]));
   }

   S_CHECK(emit_bytes(tape, OP_PUSH_LIDX, lvars[0], OP_EQUAL));

   if (do_emit_not) {
      S_CHECK(emit_byte(tape, OP_NOT));
   }

   S_CHECK(parser_expect(interp, "Closing ')' expected", TOK_RPAREN));

   Jump jump = { .depth = c->scope_depth };
   S_CHECK(emit_jump(tape, OP_JUMP_IF_TRUE, &jump.addr));
   S_CHECK(jumps_add_verbose(&c->truey_jumps, jump));

   /* Resolve all remaining jumps linked to a false condition */
   S_CHECK(patch_jumps(&c->falsey_jumps, tape, c->scope_depth));

   S_CHECK(end_scope(interp, tape));

   return OK;
}

static int parse_conditional(Interpreter * restrict interp, unsigned * restrict p,
                              Compiler * restrict c, Tape * restrict tape, unsigned depth)
{

   enum emptok_type toktype = parser_getcurtoktype(interp);
   bool need_rparent = false, emit_not = false;
   assert(depth > 0);

   if (depth == UINT16_MAX) {
      error("[empfinfo] ERROR line %u: maximum depth of %u reached while parsing "
            "conditional expression", interp->linenr, UINT16_MAX);
      return Error_EMPIncorrectInput;
   }

   if (toktype == TOK_LPAREN) { 
      need_rparent = true;
      S_CHECK(advance(interp, p, &toktype));
      PARSER_EXPECTS(interp, "a GAMS set or NOT", TOK_IDENT, TOK_NOT, TOK_SAMEAS);

   }

   do {

   /* ---------------------------------------------------------------------
    * Here we have the following cases: the next token is either
    * - '(' -> Then we go into the nested structure
    * - NOT -> we take note of this and continue
    *
    * - A logical operator:
    *   - SAMEAS
    * OR
    * - An ident that have to be a (local) GAMS SET
    *
    * --------------------------------------------------------------------- */

      if (toktype == TOK_LPAREN) {
         S_CHECK(parse_conditional(interp, p, c, tape, depth+1));
         if (emit_not) {
            S_CHECK(emit_byte(tape, OP_NOT));
            emit_not = false;
         }
      } else {
         if (toktype == TOK_NOT) {
            emit_not = true;
            S_CHECK(advance(interp, p, &toktype));
            PARSER_EXPECTS(interp, "a GAMS set or logical operator",
                           TOK_IDENT, TOK_SAMEAS);
         }

         if (toktype == TOK_SAMEAS) {
            S_CHECK(parse_sameas(interp, p, c, tape, emit_not));
         } else {
            /* Now we expect a GAMS set to perform the membership test on*/
            S_CHECK(membership_test(interp, p, c, tape, emit_not));
         }
         emit_not = false;
      }

      S_CHECK(advance(interp, p, &toktype));

      /* ---------------------------------------------------------------------
       * - TOK_AND:
       *   1) If the condition is false, we jump until we either a TOK_OR or
       *   we go over the scope main body to the increment part
       *   2) we want to resolve all jumps that where on linked to true
       *   condition, usually tied to an TOK_OR, like this one:
       *
       *   (cond1 and (cond2 or cond3) and cond4)
       *
       *   When parsing "and" before cond4, we want to patch the jump of cond2
       *   so as to jump here.
       *
       * - TOK_OR:
       *   1) If the condition is false, we jump until we either a TOK_AND or
       *   the start of the scope main body
       *   2) we want to resolve all jumps that where on linked to false
       *   condition, usually tied to an TOK_AND, like this one:
       *
       *   (cond1 and (cond2 or cond3) or cond4)
       *
       *   When parsing "or" before cond4, we want to patch the jumps of cond1
       *   and cond2 so as to jump here. If (cond2 or cond3) is false, we still
       *   are going to execute the code for cond4 before of the OR.
       * --------------------------------------------------------------------- */

      if (toktype == TOK_AND) {
         Jump jump = {.depth = depth};
         S_CHECK(emit_jump(tape, OP_JUMP_IF_FALSE, &jump.addr));
         S_CHECK(jumps_add_verbose(&c->falsey_jumps, jump));

         S_CHECK(patch_jumps(&c->truey_jumps, tape, depth));

      } else if (toktype == TOK_OR) {
         Jump jump = {.depth = depth};
         S_CHECK(emit_jump(tape, OP_JUMP_IF_TRUE, &jump.addr));
         S_CHECK(jumps_add_verbose(&c->truey_jumps, jump));

         S_CHECK(patch_jumps(&c->falsey_jumps, tape, depth));
      } else {
         break;
      }

   } while (true);

   if (need_rparent && toktype != TOK_RPAREN) {
      return parser_err(interp, "Expecting a closing parenthesis ')' to end conditional");
   }


   return OK;
}

static inline void ovfdecl_empty(OvfDeclData *ovfdecl)
{
   ovfdecl->name = NULL;
   ovfdecl->name_gidx = UINT_MAX;
   ovfdecl->params_gidx = UINT_MAX;
   ovfdecl->params = NULL;
   ovfdecl->paramsdef = NULL;
   ovfdecl->active = false;

}

Compiler* compiler_init(Interpreter * restrict interp)
{
   Compiler *c;
   MALLOC_NULL(c, Compiler, 1);
   c->local_count = 0;
   c->stacked_tokens_count = 0;
   c->scope_depth = 0;
   c->vmstack_depth = 0;
   c->vmstack_max = 0;

   ovfdecl_empty(&c->ovfdecl);

   c->regentry_gidx = UINT_MAX;

   c->gmsfilter.active = false;

   jumps_init(&c->falsey_jumps);
   jumps_init(&c->truey_jumps);

   AA_CHECK(c->vm, empvm_new(interp));

   interp->compiler = c;

   return c;
}

void compiler_free(Compiler* c)
{
   if (!c) return;

   empvm_free(c->vm);
}



/**
 * @brief Parse a condition like $(inset(sets))
 *
 * @warning the loop must already have been initialized!
 *
 * @param interp   
 * @param p 
 * @param c 
 * @param tape 
 *
 * @return         the error code
 */
NONNULL static inline
int parse_condition(Interpreter * restrict interp, unsigned * restrict p,
                    Compiler * restrict c, Tape * restrict tape)
{
   assert(parser_getcurtoktype(interp) == TOK_CONDITION);

   trace_empparser("[empcompiler] line %u: parsing condition\n", interp->linenr);

   TokenType toktype;
   S_CHECK(advance(interp, p, &toktype));

   PARSER_EXPECTS(interp, "a GAMS set or '('", TOK_IDENT, TOK_LPAREN);

      /* Note great to make this distinction here. Can't find something nicer */
   if (toktype == TOK_LPAREN) {
      S_CHECK(parse_conditional(interp, p, c, tape, c->scope_depth+1));
   } else {
      S_CHECK(membership_test(interp, p, c, tape, false));
   }

   /* If the condition evaluates to false, jump to the increment part */
   Jump jump = {.depth =  c->scope_depth};
   //TODO(URG): which operation is required?
   //S_CHECK(emit_jump(tape, OP_JUMP_IF_FALSE, &jump.addr));
   S_CHECK(emit_jump(tape, OP_JUMP, &jump.addr));
   S_CHECK(jumps_add_verbose(&c->falsey_jumps, jump));

   /* Resolve all remaining jumps linked to a true condition */
   return patch_jumps(&c->truey_jumps, tape, c->scope_depth+1);
}

/**
 * @brief 
 *
 *  We have parsed an expression of the type:
 *  - max obj + VF('name', n.valfn(set)$(cond(set)), ...)  ...
 *  - max obj x nLower ...
 *  - Nash(n(set)$(cond(set)))
 *
 * 
 *
 * @param interp 
 * @param p 
 * @param labelname       the EMPDAG label name
 * @param labelname_len 
 * @param edge_type 
 * @param gmsindices 
 *
 * @return                the error code
 */
static int vm_gmsindicesasarc(Interpreter *interp, unsigned *p, const char *nodename,
                               unsigned nodename_len, ArcType arc_type,
                               GmsIndicesData *gmsindices)
{
   Compiler *c = interp->compiler;
   EmpVm *vm = c->vm;
   Tape tape_ = {.code = &vm->code, .linenr = interp->linenr};
   Tape * const tape = &tape_;

   /* This defines the loop iterators and the edgeobj*/
   LoopIterators loopiters = {.size = 0};
   unsigned edge_gidx;
   S_CHECK(arcobj_init(interp, tape, nodename, nodename_len, arc_type,
                        gmsindices, &loopiters, &edge_gidx));

   assert(vmval_is_arcobj(&vm->globals, edge_gidx) == OK);

   /* ---------------------------------------------------------------------
    * If there is no (local) set, then we can just duplicate the value
    * that is in the global index. There must be some iterator values that
    * were updated in labelargs_initloop().
    * 1) Copy the object and set the gidx value on the stack  
    * 2) Set the arguments as index for the  
    * And we are done
    * --------------------------------------------------------------------- */


   uint8_t num_vars = gmsindices->num_localsets + gmsindices->num_sets;
   if (num_vars == 0) {
      assert(gmsindices->num_iterators > 0);
      S_CHECK(emit_bytes(tape, OP_EDGE_DUP_DAGL));
      S_CHECK(EMIT_GIDX(tape, edge_gidx));

      return OK;
   }

   S_CHECK(emit_bytes(tape, OP_EDGE_INIT));
   S_CHECK(EMIT_GIDX(tape, edge_gidx));


   /* ---------------------------------------------------------------------
    * Start the loop. For each set, we must declare an iterator and loop over
    * the possible values. 
    * --------------------------------------------------------------------- */
   begin_scope(c, __func__);
   S_CHECK(loop_initandstart(interp, tape, &loopiters));

   /* ---------------------------------------------------------------------
    * If present, Parse the condition
    * --------------------------------------------------------------------- */

   unsigned p2 = *p;
   TokenType toktype;
   S_CHECK(peek(interp, &p2, &toktype));

   if (toktype == TOK_CONDITION) {
      *p = p2;
      S_CHECK(parse_condition(interp, p, c, tape));
   }

   /* ---------------------------------------------------------------------
    * Step : Main body: we add the tuple (iter1, iter2, iter3)
    * This will duplicate the DagLabels and put it on the stack.
    * --------------------------------------------------------------------- */

   S_CHECK(emit_bytes(tape, OP_DAGL_STORE, num_vars));

   for (unsigned i = 0; i < num_vars; ++i) {
      S_CHECK(emit_byte(tape, loopiters.iters->iter_lidx));
   }

   /* ---------------------------------------------------------------------
    * Step 4:  Increment loop index and check of the condition idx < max
    * --------------------------------------------------------------------- */

   /* Resolve all remaining jumps linked to a false condition */
   S_CHECK(patch_jumps(&c->falsey_jumps, tape, c->scope_depth));

   S_CHECK(loop_increment(tape, &loopiters));

   /* ---------------------------------------------------------------------
    * We are at the end of the loop. We allow for outstanding "true" jump
    * to come here.
    * --------------------------------------------------------------------- */

   S_CHECK(patch_jumps(&c->truey_jumps, tape, c->scope_depth));

   assert(no_outstanding_jump(&c->truey_jumps, c->scope_depth));
   assert(no_outstanding_jump(&c->falsey_jumps, c->scope_depth));

   S_CHECK(emit_bytes(tape, OP_DAGL_FINALIZE));

   return end_scope(interp, tape);
}

NONNULL static
int c_identaslabels(Interpreter * restrict interp, unsigned * restrict p, ArcType edge_type)
{
   const char* nodename = emptok_getstrstart(&interp->cur);
   unsigned nodename_len = emptok_getstrlen(&interp->cur);

   TokenType toktype;
   S_CHECK(advance(interp, p, &toktype))

   GmsIndicesData indices;
   gms_indicesdata_init(&indices);

   if (toktype == TOK_LPAREN) {
      S_CHECK(parse_gmsindices(interp, p, &indices));
   }


   S_CHECK(vm_gmsindicesasarc(interp, p, nodename, nodename_len, edge_type, &indices));
   assert(interp->cur.type == TOK_RPAREN);

   return advance(interp, p, &toktype);
}

/**
 * @brief Function to parse loop sets
 *
 * It is similar as parse_gmsindices(), but only IdentSet and IdentLocalSet
 * are allowed
 *
 * @param interp 
 * @param p 
 * @param idxdata 
 * @return 
 */
static int parse_loopsets(Interpreter * restrict interp, unsigned * restrict p,
                   GmsIndicesData * restrict idxdata)
{
   assert(emptok_gettype(&interp->cur) == TOK_LPAREN);

   TokenType toktype;
   unsigned nargs = 0;

   do {
      if (nargs == GMS_MAX_INDEX_DIM) {
         error("[empinterp] ERROR line %u: while parsing the sets to loop over, "
               "more than %u were parsed.\n", interp->linenr, GMS_MAX_INDEX_DIM);
         return Error_EMPIncorrectSyntax;
      }

      /* get the next token */
      S_CHECK(advance(interp, p, &toktype));
      PARSER_EXPECTS(interp, "Sets to loop over must are identifiers",
                            TOK_IDENT, TOK_GMS_SET);

      IdentData *data = &idxdata->idents[nargs];

      if (toktype == TOK_GMS_SET) {
         S_CHECK(tok2ident(&interp->cur, data));
      } else {
      RESOLVE_IDENTAS(interp, data, "Loop indices must fulfill these conditions.",
                      IdentLocalSet, IdentGdxSet);
      }

      switch (idxdata->idents[nargs].type) {
      case IdentLocalSet:
         idxdata->num_localsets++;
         break;
      case IdentGdxSet:
      case IdentGmdSet:
         idxdata->num_sets++;
         break;
      default:
         return runtime_error(interp->linenr);
      }

      nargs++;

      S_CHECK(advance(interp, p, &toktype));

   } while (toktype == TOK_COMMA);

   idxdata->nargs = nargs;

   return parser_expect(interp, "Closing ')' expected for loop set(s).", TOK_RPAREN);
}

/**
 * @brief Parse a loop statement.
 *
 * @param interp  the interpreter
 * @param p       the position index
 *
 * @return        the error code
 */
int parse_loop(Interpreter * restrict interp, unsigned * restrict p)
{
#ifndef NDEBUG
   bool has_condition = false;
#define loop_has_condition() has_condition = true;
#else
#define loop_has_condition() 
#endif

   /* If we are already parsing, just get a pointer to the compiler. Otherwise
    * initialize the compiler and set the parser_ops to the VM ones */
   Compiler *c = ensure_vm_mode(interp);
   EmpVm *vm = c->vm;
   Tape _tape = {.code = &vm->code, .linenr = UINT_MAX};
   Tape * const tape = &_tape;

   begin_scope(c, __func__);

   trace_empparser("[empcompiler] Loop @%u is starting\n", c->scope_depth);

   /* Consume the opening delimiter (parent / bracket / ...) */
   enum emptok_type toktype;
   S_CHECK(advance(interp, p, &toktype))

   PARSER_EXPECTS(interp, "A delimiter '(' or '[')", TOK_LPAREN, TOK_LBRACK);

   enum emptok_type delimiter_endloop = TOK_UNSET;
   if (toktype == TOK_LPAREN) {
      delimiter_endloop = TOK_RPAREN;
   } else if (toktype == TOK_LBRACK) {
      delimiter_endloop = TOK_RBRACK;
   }


   /* ---------------------------------------------------------------------
    * Step 1: loop initialization:
    * - 1.1: Get the GAMS set we iterate over
    * - 1.2: Define 2 locals variables:
    *        
    *        |  a_idx: Int  |  the loop index  |
    *        |  a_elt: Int  |  the UEL idx     |
    *
    * - 1.3: Save the max value in the array of ints
    *
    *        |  a_len: Int  |  cardinality of the set  |
    * --------------------------------------------------------------------- */

   /* We get the set/sets to iterate over */
   S_CHECK(advance(interp, p, &toktype))
   PARSER_EXPECTS(interp, "a single GAMS set or a collection", TOK_IDENT, TOK_LPAREN);

   GmsIndicesData gmsindices;
   if (toktype == TOK_LPAREN) {
      S_CHECK(parse_loopsets(interp, p, &gmsindices));
   } else {

      IdentData *ident = &gmsindices.idents[0];
      RESOLVE_IDENTAS(interp, ident, "GAMS index must fulfill these conditions.",
                      IdentLocalSet, IdentGdxSet, IdentGmdSet);

      switch (ident->type) {
      case IdentLocalSet:
         gmsindices.num_localsets++;
         break;
      case IdentGdxSet:
      case IdentGmdSet:
         gmsindices.num_sets++;
         break;
      default:
         return runtime_error(interp->linenr);
      }

      gmsindices.nargs = 1;
   }

   tape->linenr = interp->linenr;
   
   LoopIterators iterators;
   iterators_init(&iterators, &gmsindices);
   S_CHECK(loop_initandstart(interp, tape, &iterators));

   /* ---------------------------------------------------------------------
    * Step 3:  Parse the conditional (if present)
    * --------------------------------------------------------------------- */

   S_CHECK(advance(interp, p, &toktype))
   PARSER_EXPECTS(interp, "a conditional '$' or comma ','", TOK_CONDITION, TOK_COMMA);

   tape->linenr = interp->linenr;

   if (toktype == TOK_CONDITION) {
      loop_has_condition()

      S_CHECK(parse_condition(interp, p, c, tape));

      /* Check for the comma */
      S_CHECK(advance(interp, p, &toktype))
      S_CHECK(parser_expect(interp, "a ',' after conditional", TOK_COMMA));

      tape->linenr = interp->linenr;

   }


   /* ---------------------------------------------------------------------
    * Step 3:  Parse the body of the loop
    * --------------------------------------------------------------------- */
   S_CHECK(advance(interp, p, &toktype))

   while (toktype != TOK_EOF && toktype != delimiter_endloop) {
      S_CHECK(process_statements(interp, p, toktype));
      toktype = parser_getcurtoktype(interp);
   }

   if (toktype == TOK_EOF) return parser_err(interp, "while parsing the loop body");

   /* ---------------------------------------------------------------------
    * Step 4:  Increment loop index and check of the condition idx < max
    * --------------------------------------------------------------------- */

   tape->linenr = interp->linenr;

   /* Resolve all remaining jumps linked to a false condition */
   assert(!has_condition || c->falsey_jumps.len > 0);
   S_CHECK(patch_jumps(&c->falsey_jumps, tape, c->scope_depth));

   S_CHECK(loop_increment(tape, &iterators));

   trace_empparser("[empcompiler] Loop @%u is complete\n", c->scope_depth);

   /* ---------------------------------------------------------------------
    * We are at the end of the loop. We allow for outstanding "true" jump
    * to come here.
    * --------------------------------------------------------------------- */

   S_CHECK(patch_jumps(&c->truey_jumps, tape, c->scope_depth));

   assert(no_outstanding_jump(&c->truey_jumps, c->scope_depth));
   assert(no_outstanding_jump(&c->falsey_jumps, c->scope_depth));

   S_CHECK(end_scope(interp, tape));

   /* Consume the delimiter_endloop token */
//   S_CHECK(_advance(interp, p, &toktype))
   S_CHECK(parser_expect(interp, "end delimiter of loop", delimiter_endloop));

   /* Parse the next token */
   S_CHECK(advance(interp, p, &toktype))

   /* ---------------------------------------------------------------------
    * If we are back to the general scope, we execute the VM now
    * --------------------------------------------------------------------- */

   return OK;
}


/* TODO: Is it possible to factorize the parse_loop and parse_defvar functions? */
int parse_defvar(Interpreter * restrict interp, unsigned * restrict p)
{
   /* The identifier is stored in the "pre" */
   assert(parser_getcurtoktype(interp) == TOK_DEFVAR);
   assert(parser_getpretoktype(interp) == TOK_IDENT);
   Compiler *c = interp->compiler;

   EmpVm *vm = c->vm;
   Tape _tape = {.code = &vm->code, .linenr = interp->linenr};
   Tape * const tape = &_tape;
   /* ---------------------------------------------------------------------
    * The steps are as follows:
    * 1) Declare the local variable (in the compiler)
    * 2) Get the ident of the RHS
    * 3) If needed, parse the indices
    * 4) Generate the code
    * 5) Mark the local variable as initialized
    *
    * We expect a statement of the form    localvar :=  set$(cond1())
    * 'localvar' is in the pre-token
    * --------------------------------------------------------------------- */

   /* The name of the variable is stored in the pre-token */
   Lexeme lexeme = {.linenr = interp->linenr, .len = interp->pre.len, .start = interp->pre.start};
   TokenType toktype;
   S_CHECK(advance(interp, p, &toktype));
   PARSER_EXPECTS(interp, "local variable can be defined from a set or parameter",
                  TOK_GMS_SET, TOK_GMS_MULTISET, TOK_GMS_PARAM, TOK_IDENT);

   IdentData ident;

   if (toktype == TOK_IDENT) {
      RESOLVE_IDENTAS(interp, &ident, "a GAMS set is expected",
                      IdentLocalSet, IdentGdxSet, IdentVector, IdentLocalVector);
   } else {
      S_CHECK(tok2ident(&interp->cur, &ident));
   }

   unsigned dummylen;
   const char *lvar_name = NULL;
   IdentType lvar_type;
   S_CHECK(gen_localvarname(&lexeme, "", &lvar_name, &dummylen));

   unsigned lvar_gidx;
   enum OpCode opcode_add, opcode_reset;
   switch(ident.type) {
   case IdentLocalSet:
   case IdentGdxSet:
   case IdentGmdSet: {
      IntArray set = {.len = 0, .max = 5};
      MALLOC_(set.arr, int, set.max);
      S_CHECK(namedints_add(&interp->globals.localsets, set, lvar_name));
      lvar_gidx = interp->globals.localsets.len-1;
      opcode_reset = OP_LSET_RESET;
      opcode_add = OP_LSET_ADD;
      lvar_type = IdentLocalSet;
      break;
   }
   case IdentLocalVector:
   case IdentVector: {
      //Lequ vec = {.len = 0, .max = 0, .value = NULL, .index = NULL};
      //S_CHECK(namedvec_add(&interp->globals.localvectors, vec, lvar_name));
      //lvar_gidx = interp->globals.localvectors.len-1;
      //opcode_reset = OP_LVEC_RESET;
      //opcode_add = OP_LVEC_ADD;
      //lvar_type = IdentLocalVector;
      //break;
      errormsg("[empcompiler] ERROR: defining a local vector has yet to be implemented\n");
      FREE(lvar_name);
      return Error_NotImplemented;
   }
   default:
      FREE(lvar_name);
      return runtime_error(interp->linenr);
   }
   
   LIDX_TYPE lvar_lidx;
   S_CHECK(declare_variable(c, &lexeme, &lvar_lidx, lvar_type))
   c->locals[lvar_lidx].idx = lvar_gidx;

   S_CHECK(emit_byte(tape, opcode_reset));
   S_CHECK(EMIT_GIDX(tape, lvar_gidx));

   /* RIGHT NOW we enter a local scope as we will define variables for looping */

   begin_scope(c, __func__);

      // TODO(LocalVector): parse the gmsindices here
   //unsigned p2 = *p;
   //S_CHECK(_peek(interp, &p2, &toktype));
   //if (toktype == TOK_LPAREN) {

   //}
   /* declare all the required data */
   LoopIterators loopiter = {.size = 1, .loopobj_gidx = UINT_MAX};
   memcpy(&loopiter.idents[0], &ident, sizeof(ident));
   S_CHECK(loop_initandstart(interp, tape, &loopiter));

   /* ---------------------------------------------------------------------
    * Step : Parse the condition
    * --------------------------------------------------------------------- */

   S_CHECK(advance(interp, p, &toktype));
   PARSER_EXPECTS(interp, "local variable can only be a subset defined via conditional (for now)",
                  TOK_CONDITION);

   S_CHECK(parse_condition(interp, p, c, tape));

   /* ---------------------------------------------------------------------
    * Step : Main body: we add the element to the localvar
    * --------------------------------------------------------------------- */

   // TODO(LocalVector): If there are other iterators than the set/param, 
   // we need to get that element in a local variable and then add it.
   // In this case, the last argument to the opcode_add is different
   S_CHECK(emit_byte(tape, opcode_add));
   S_CHECK(EMIT_GIDX(tape, lvar_gidx));
   S_CHECK(emit_byte(tape, loopiter.iters[0].iter_lidx));
  
   /* ---------------------------------------------------------------------
    * Step 4:  Increment loop index and check of the condition idx < max
    * --------------------------------------------------------------------- */

   /* Resolve all remaining jumps linked to a false condition */
   S_CHECK(patch_jumps(&c->falsey_jumps, tape, c->scope_depth));

   S_CHECK(loop_increment(tape, &loopiter));

   /* ---------------------------------------------------------------------
    * We are at the end of the loop. We allow for outstanding "true" jump
    * to come here.
    * --------------------------------------------------------------------- */

   S_CHECK(patch_jumps(&c->truey_jumps, tape, c->scope_depth));

   assert(no_outstanding_jump(&c->truey_jumps, c->scope_depth));
   assert(no_outstanding_jump(&c->falsey_jumps, c->scope_depth));

   S_CHECK(end_scope(interp, tape));

  /* AFTER leaving the scope, set the depth of the var */
   finalize_variable(c, lvar_lidx);

   /* Consume to get the next token */
   S_CHECK(advance(interp, p, &toktype))

   return OK;
}

/**
 * @brief Generate the bytecode for a conditional label definition of the form
 *     nOpt(set)$(inset(set)):  ...
 *
 *
 *
 * @param interp 
 * @param p 
 * @param labelname 
 * @param labelname_len 
 * @param gmsindices 
 * @return 
 */
int vm_labeldef_condition(Interpreter * interp, unsigned * restrict p,
                          const char *labelname, unsigned labelname_len,
                          GmsIndicesData* gmsindices)
{
   assert(gmsindices->nargs > 0);

  /* ----------------------------------------------------------------------
   * We have a statement of the form   nOpt(sets)$(inset(sets)): ...
   *
   * nOpt is in (labelname, labelname_len)
   * sets are in gmsindices
   *
   * We are about to parse the condition. Plan of action:
   * - TODO: Check that we have something to iterate over.
   * - Emit the start of the loop code.
   * - Parse and emit the condition.
   * - IMPORTANT: check for the ':'
   * - Parse the statement
   * - Put the loop finalization
   * --------------------------------------------------------------------- */

   /* If we are already inc compiler mode, just get a pointer to the compiler.
    * Otherwise initialize the compiler and set the parser_ops to the VM ones */
   Compiler *c = ensure_vm_mode(interp);

   EmpVm *vm = c->vm;
   Tape _tape = {.code = &vm->code, .linenr = UINT_MAX};
   Tape * const tape = &_tape;

   /* TODO: this could be useful to rewind */
   UNUSED unsigned codelen_atstart = vm->code.len;

   begin_scope(c, __func__);

   tape->linenr = interp->linenr;
   
   LoopIterators iterators;
   unsigned regentry_gidx;
   S_CHECK(regentry_init(interp, labelname, labelname_len, gmsindices,
                            &iterators, tape, &regentry_gidx));

   S_CHECK(loop_initandstart(interp, tape, &iterators));
   
   /* Parse  $(inset(sets)) */
   S_CHECK(parse_condition(interp, p, c, tape));

   TokenType toktype;
   S_CHECK(advance(interp, p, &toktype))

   if (toktype != TOK_COLON) {
      error("[empcompiler] Error line %u: unexpected token '%.*s' of type '%s'",
            interp->linenr, emptok_getstrlen(&interp->cur),
            emptok_getstrstart(&interp->cur), toktype2str(emptok_gettype(&interp->cur)));
   }

   //S_CHECK(labelargs_initandstart(interp, tape, &iterators));

   S_CHECK(labdeldef_parse_statement(interp, p));

   /* ---------------------------------------------------------------------
    * Step 4:  Increment loop index and check of the condition idx < max holds
    * --------------------------------------------------------------------- */

   /* Resolve all remaining jumps linked to a false condition */
   S_CHECK(patch_jumps(&c->falsey_jumps, tape, c->scope_depth));

   S_CHECK(loop_increment(tape, &iterators));

   /* ---------------------------------------------------------------------
    * We are at the end of the loop. We allow for outstanding "true" jump
    * to come here.
    * --------------------------------------------------------------------- */

   S_CHECK(patch_jumps(&c->truey_jumps, tape, c->scope_depth));

   assert(no_outstanding_jump(&c->truey_jumps, c->scope_depth));
   assert(no_outstanding_jump(&c->falsey_jumps, c->scope_depth));

   S_CHECK(end_scope(interp, tape));

   return OK;
}

/**
 * @brief Generate the bytecode for a label definition of the form
 *     nOpt(set): ...
 *
 * @param interp 
 * @param p 
 * @param labelname 
 * @param labelname_len 
 * @param gmsindices 
 * @return 
 */
int vm_labeldef_loop(Interpreter * interp, unsigned * restrict p,
                     const char *labelname, unsigned labelname_len,
                     GmsIndicesData* restrict gmsindices)
{
   assert(gmsindices->nargs > 0);

   Compiler *c = ensure_vm_mode(interp);

   EmpVm *vm = c->vm;
   Tape _tape = {.code = &vm->code, .linenr = UINT_MAX};
   Tape * const tape = &_tape;

   /* TODO: this could be useful to rewind */
   UNUSED unsigned codelen_atstart = vm->code.len;

   begin_scope(c, __func__);

   tape->linenr = interp->linenr;
   
   LoopIterators iterators;
   unsigned regentry_gidx;
   S_CHECK(regentry_init(interp, labelname, labelname_len, gmsindices,
                         &iterators, tape, &regentry_gidx));
   S_CHECK(loop_initandstart(interp, tape, &iterators));

   /* ---------------------------------------------------------------------
    * If we are called here, we have parsed the gmsindices and there is no
    * condition. We are also sure we 
    * - Check that we have something to iterate over.
    * - Emit the start of the loop code.
    * - Parse and emit the condition
    * - Go to the loop
    * - Put the loop finalization
    * --------------------------------------------------------------------- */

   /* Just consume the ":" token, advance is done in the next function */
   TokenType toktype = parser_getcurtoktype(interp);
   if (toktype != TOK_COLON) {
      error("[empcompiler] Error line %u: unexpected token '%.*s' of type '%s'",
            interp->linenr, emptok_getstrlen(&interp->cur),
            emptok_getstrstart(&interp->cur), toktype2str(emptok_gettype(&interp->cur)));
   }

   S_CHECK(labdeldef_parse_statement(interp, p));

   /* Resolve all remaining jumps linked to a false condition */
   S_CHECK(patch_jumps(&c->falsey_jumps, tape, c->scope_depth));

   S_CHECK(loop_increment(tape, &iterators));

   /* ---------------------------------------------------------------------
    * We are at the end of the loop. We allow for outstanding "true" jump
    * to come here.
    * --------------------------------------------------------------------- */

   S_CHECK(patch_jumps(&c->truey_jumps, tape, c->scope_depth));

   assert(no_outstanding_jump(&c->truey_jumps, c->scope_depth));
   assert(no_outstanding_jump(&c->falsey_jumps, c->scope_depth));

   S_CHECK(end_scope(interp, tape));

   return OK;
}

/**
 * @brief  Add edges in the empdag between current daguid and argument
 *
 * @param interp       the interpreter
 * @param p            the indices
 * @param edgetype     the type for all edges
 * @param argname      the basename of the labels
 * @param argname_len  the length of the basename 
 * @param gmsindices   the indices for the basename
 * @return 
 */
static int vm_add_edges(Interpreter * interp, unsigned * restrict p, ArcType edgetype,
                        const char* argname, unsigned argname_len, GmsIndicesData* gmsindices)
{
   assert(gmsindices->nargs > 0);

   /* TODO URG: review if this is sound */
   UNUSED Compiler *c = ensure_vm_mode(interp);

   /* ---------------------------------------------------------------------
    * If the scope depth is 0, we are not in a loop. Then, the daguid needs
    * to be updated.
    * --------------------------------------------------------------------- */

//   if (c->scope_depth == 0) {
//      EmpVm *vm = c->vm;
//      Tape _tape = {.code = &vm->code, .linenr = UINT_MAX};
//      Tape * const tape = &_tape;
//
//      S_CHECK(emit_byte(tape, OP_SET_DAGUID_FROM_REGENTRY));
//
//   }

   S_CHECK(vm_gmsindicesasarc(interp, p, argname, argname_len, edgetype, gmsindices));

   return OK;
}

int vm_nash(Interpreter * interp, unsigned * restrict p, const char* argname,
            unsigned argname_len, GmsIndicesData* gmsindices)
{
   return vm_add_edges(interp, p, ArcNash, argname, argname_len, gmsindices);
}

int vm_add_VFobjSimple_edge(Interpreter * interp, unsigned * restrict p, const char* argname,
            unsigned argname_len, GmsIndicesData* gmsindices)
{
   return vm_add_edges(interp, p, ArcVF, argname, argname_len, gmsindices);
}

int vm_add_Ctrl_edge(Interpreter * interp, unsigned * restrict p, const char* argname,
                     unsigned argname_len, GmsIndicesData* gmsindices)
{
   return vm_add_edges(interp, p, ArcCtrl, argname, argname_len, gmsindices);
}

NONNULL static
int ovfdecl_fillparam(EmpVm* vm, OvfDeclData * restrict ovfdecl, unsigned ovf_idx) 
{
   ovfdecl->active = true;

   MALLOC_(ovfdecl->params, OvfParamList, 1);
   S_CHECK(ovf_fill_params(ovfdecl->params, ovf_idx));
   S_CHECK(vmvals_add(&vm->globals, PTR_VAL(ovfdecl->params)));
   ovfdecl->params_gidx = vm->globals.len - 1;

   A_CHECK(ovfdecl->paramsdef, ovf_getparamdefs(ovf_idx));

   return OK;
}

/* ---------------------------------------------------------------------
 * Start of parser_ops_vm implementation
 * --------------------------------------------------------------------- */

static int c_ccflib_new(Interpreter* restrict interp, unsigned ccf_idx,
                         MathPrgm **mp)
{
   Compiler *c = interp->compiler;
   OvfDeclData * restrict ovfdecl = &c->ovfdecl;
   assert(!ovfdecl->active);

   EmpVm * restrict vm = c->vm;
   Tape tape_ = {.code = &vm->code, .linenr = interp->linenr};
   Tape * const tape = &tape_;

   assert(ccf_idx < INSTR_MAX);
   S_CHECK(emit_bytes(tape, OP_PUSH_BYTE, ccf_idx));

   /* ---------------------------------------------------------------------
    * If we have a valid regentry template, we pass it along
    * --------------------------------------------------------------------- */

   if (c->regentry_gidx < UINT_MAX) {
      S_CHECK(emit_byte(tape, OP_PUSH_GIDX));
      S_CHECK(EMIT_GIDX(tape, c->regentry_gidx));
      c->regentry_gidx = UINT_MAX;
   } else {
      S_CHECK(emit_byte(tape, OP_NIL));

   }

   /* Allocate the parameter list in ovfdecl */
   S_CHECK(ovfdecl_fillparam(vm, ovfdecl, ccf_idx));

   S_CHECK(emit_bytes(tape, OP_NEW_OBJ, FN_CCFLIB_NEW));

   UPDATE_STACK_MAX(c, empnewobjs[FN_CCFLIB_NEW].argc);
   c->vmstack_depth += 1;

   *mp = NULL;

   return OK;
}

static int c_ccflib_finalize(Interpreter* restrict interp, UNUSED MathPrgm *mp)
{
   Compiler * restrict c = interp->compiler;
   assert(c->ovfdecl.active);

   EmpVm * restrict vm = c->vm;
   Tape tape_ = {.code = &vm->code, .linenr = interp->linenr};
   Tape * const tape = &tape_;

   /* Update the params object, finalize the ovf and pop it */
   S_CHECK(emit_bytes(tape, OP_CALL_API, FN_CCFLIB_UPDATEPARAMS,
                            OP_CALL_API, FN_CCFLIB_FINALIZE,
                            OP_POP));

   assert(c->vmstack_depth >= 2);
   c->vmstack_depth -= 2;

   ovfdecl_empty(&c->ovfdecl);

   return OK;
}

static int c_gms_resolve(Interpreter* restrict interp, unsigned * p)
{
   Compiler *c = interp->compiler;
   assert(interp->gms_sym_iterator.active);
   interp->gms_sym_iterator.active = false;

   EmpVm *vm = c->vm;
   Tape tape_ = {.code = &vm->code, .linenr = interp->linenr};
   Tape * const tape = &tape_;

   LoopIterators loopiter = {.size = 0};
   unsigned symiter_gidx;
   IdentData *ident = &interp->gms_sym_iterator.ident;
   assert(ident->type == IdentEqu || ident->type == IdentVar);

   begin_scope(c, __func__);

   S_CHECK(gmssymiter_init(interp, ident, &interp->gms_sym_iterator.indices,
                           &loopiter, tape, &symiter_gidx));

   if (loopiter.size == 0) {

     /* ---------------------------------------------------------------------
      * We can just resolve and move on.
      * --------------------------------------------------------------------- */

      S_CHECK(emit_byte(tape, OP_GMSSYM_RESOLVE));
      S_CHECK(EMIT_GIDX(tape, symiter_gidx));

      goto _exit;
   }

   S_CHECK(emit_bytes(tape, OP_GMS_EQUVAR_INIT, ident->type));

   S_CHECK(loop_initandstart(interp, tape, &loopiter));

   unsigned p2 = *p;
   TokenType toktype;
   S_CHECK(peek(interp, &p2, &toktype));

   if (toktype == TOK_CONDITION) {
      *p = p2;
      S_CHECK(parse_condition(interp, p, c, tape));

}

   /* ---------------------------------------------------------------------
    * Main body of the loop: just perform the test and jump out of the loop
    * if true.
    * --------------------------------------------------------------------- */

   S_CHECK(emit_byte(tape, OP_GMS_RESOLVE_EXTEND));
   S_CHECK(EMIT_GIDX(tape, symiter_gidx));

   /* ---------------------------------------------------------------------
    * Step 4:  Increment loop index and check of the condition idx < max
    * --------------------------------------------------------------------- */

   /* Resolve all remaining jumps linked to a false condition */
   S_CHECK(patch_jumps(&c->falsey_jumps, tape, c->scope_depth));

   S_CHECK(loop_increment(tape, &loopiter));

   S_CHECK(emit_bytes(tape, OP_GMS_EQUVAR_SYNC, ident->type));

_exit:

   /* ---------------------------------------------------------------------
    * We are at the end of the loop. We allow for outstanding "true" jump
    * to come here.
    * --------------------------------------------------------------------- */

   S_CHECK(patch_jumps(&c->truey_jumps, tape, c->scope_depth));

   assert(no_outstanding_jump(&c->truey_jumps, c->scope_depth));
   assert(no_outstanding_jump(&c->falsey_jumps, c->scope_depth));

   S_CHECK(end_scope(interp, tape));


   c->gmsfilter.active = false;

   return OK;
}

/**
 * @brief Parse a GMS symbol
 *
 * @param interp 
 * @param p
 *
 * @return          the error code
 */
static int c_gms_parse(Interpreter * restrict interp, unsigned * restrict p)
{
   interp_save_tok(interp);

   /* From the current token get all the symbol info */
   S_CHECK(dctsymb2ident(&interp->cur, &interp->gms_sym_iterator.ident));

   GmsSymIterator * restrict filter = &interp->gms_sym_iterator;
   assert(!filter->active);
   filter->active = true;
   filter->compact = true;
   filter->loop_needed = false;

   GmsIndicesData *indices = &filter->indices;
   gms_indicesdata_init(indices);

   TokenType toktype;
   unsigned p2 = *p;
   S_CHECK(peek(interp, &p2, &toktype));

   if (toktype == TOK_LPAREN) {
      *p = p2;
      S_CHECK(parse_gmsindices(interp, p, indices));
   }

   S_CHECK(c_gms_resolve(interp, p));

   interp_restore_savedtok(interp);
   interp->cur.symdat.read = true;

   filter->active = false;

   return OK;
}


static int c_ovf_addbyname(Interpreter* restrict interp, UNUSED EmpInfo *empinfo,
                            const char *name, UNUSED void **ovfdef_data)
{
   Compiler *c = interp->compiler;
   OvfDeclData * restrict ovfdecl = &c->ovfdecl;
   assert(!ovfdecl->active);

   EmpVm * restrict vm = c->vm;
   Tape tape_ = {.code = &vm->code, .linenr = interp->linenr};
   Tape * const tape = &tape_;

   /* We need to copy the name as it is given to us from a temporary space */
   A_CHECK(ovfdecl->name, strdup(name));
   S_CHECK(vmvals_add(&vm->globals, STR_VAL(ovfdecl->name)));
   ovfdecl->name_gidx = vm->globals.len - 1;

   /* Get the parameters for the OVF */
   unsigned ovf_idx = ovf_findbyname(name);
   if (ovf_idx == UINT_MAX) return Error_EMPRuntimeError;

   S_CHECK(ovfdecl_fillparam(vm, ovfdecl, ovf_idx));

    /* call ovf_new with 1 arg: the name */
   S_CHECK(emit_byte(tape, OP_PUSH_GIDX));
   S_CHECK(EMIT_GIDX(tape, ovfdecl->name_gidx));
   S_CHECK(emit_bytes(tape, OP_NEW_OBJ, FN_OVF_NEW));

   UPDATE_STACK_MAX(c, empnewobjs[FN_OVF_NEW].argc);
   c->vmstack_depth += 1;

   trace_empparser("[empcompiler] line %u: adding OVF '%s'; name_gidx = %u; "
                   "params_gidx = %u\n", interp->linenr, name,
                   ovfdecl->name_gidx, ovfdecl->params_gidx);

   ovfdecl->active = true;

   return OK;
}

static int c_ovf_setrhovar(Interpreter* restrict interp, UNUSED void *ovfdef_data)
{
   Compiler *c = interp->compiler;
   assert(c->ovfdecl.active);

   EmpVm * restrict vm = c->vm;
   Tape tape_ = {.code = &vm->code, .linenr = interp->linenr};
   Tape * const tape = &tape_;

   S_CHECK(emit_bytes(tape, OP_CALL_API, FN_OVF_SETRHO));

   return OK;
}

static int c_ovf_addarg(Interpreter* restrict interp, UNUSED void *ovfdef_data)
{
   Compiler *c = interp->compiler;
   assert(c->ovfdecl.active);

   EmpVm * restrict vm = c->vm;
   Tape tape_ = {.code = &vm->code, .linenr = interp->linenr};
   Tape * const tape = &tape_;

   S_CHECK(emit_bytes(tape, OP_CALL_API, FN_OVF_ADDARG));

   return OK;
}

/**
 * @brief Pushes the gidx of the OVF onto the stack
 *
 * @param interp 
 * @param void 
 * @param paramsdef 
 * @return 
 */
static int c_ovf_paramsdefstart(Interpreter* restrict interp, UNUSED void *ovfdef_data,
                                 const OvfParamDefList **paramsdef)
{
   Compiler *c = interp->compiler;
   OvfDeclData * restrict ovfdecl = &c->ovfdecl;
   assert(c->ovfdecl.active);

   EmpVm * restrict vm = c->vm;
   Tape tape_ = {.code = &vm->code, .linenr = interp->linenr};
   Tape * const tape = &tape_;

   S_CHECK(emit_byte(tape, OP_PUSH_GIDX));
   S_CHECK(EMIT_GIDX(tape, ovfdecl->params_gidx));

   UPDATE_STACK_MAX(c, 1);
   c->vmstack_depth++;

   *paramsdef = ovfdecl->paramsdef;

   return OK;
}


static int c_ovf_setparam(Interpreter* restrict interp, UNUSED void *ovfdef_data,
                           unsigned i, OvfArgType type, OvfParamPayload payload)
{
   Compiler *c = interp->compiler;
   OvfDeclData * restrict ovfdecl = &c->ovfdecl;
   assert(ovfdecl->active);
   assert(i < UINT8_MAX);

   EmpVm * restrict vm = c->vm;
   Tape tape_ = {.code = &vm->code, .linenr = interp->linenr};
   Tape * const tape = &tape_;

   const OvfParamDef *pdef = ovfdecl->paramsdef->p[i];

   if (type == ARG_TYPE_UNSET) {
      if (!pdef->mandatory) {
         /*  Here we are fine, this was not a mandatory argument */
         if (pdef->default_val) {
            /* ---------------------------------------------------------------
             * Do nothing: the default function will be called by finalize
             * --------------------------------------------------------------- */

            return OK;
         }

      } else {
         error("[empcompiler] ERROR line %u: mandatory parameter '%s' not found (OVF '%s')\n",
               interp->linenr, pdef->name, ovfdecl->name);
         return Error_NotFound;
      }

   }
   unsigned gidx;
   if (type == ARG_TYPE_NESTED) {
      gidx = payload.gidx;
   } else {
      OvfParam * restrict param_update;
      MALLOC_(param_update, OvfParam, 1);
      param_update->type = type;

      switch (type) {
         case ARG_TYPE_SCALAR:
            param_update->val = payload.val;
            break;
         case ARG_TYPE_VEC:
            param_update->vec = payload.vec;
            break;
         case ARG_TYPE_MAT:
            param_update->mat = payload.mat;
            break;
         default:
            error("%s :: unsupported param type %d\n", __func__, param_update->type);
            FREE(param_update);
            return Error_EMPRuntimeError;
      }

      S_CHECK(vmvals_add(&vm->globals, PTR_VAL(param_update)));
       gidx = vm->globals.len - 1;
   } 

   /* ---------------------------------------------------------------------
    * Stack update:
    * - 1: OvfParamList (already there)
    * - 2: param index (which param to update)
    * - 3: global index for OvfParam value
    * --------------------------------------------------------------------- */

   S_CHECK(emit_bytes(tape, OP_PUSH_BYTE, i));

   S_CHECK(emit_byte(tape, OP_PUSH_GIDX));
   S_CHECK(EMIT_GIDX(tape, gidx));

   UPDATE_STACK_MAX(c, empapis[FN_OVFPARAM_UPDATE].argc-1);

   return emit_bytes(tape, OP_CALL_API, FN_OVFPARAM_UPDATE);
}

static int c_ovf_check(Interpreter* restrict interp, UNUSED void *ovfdef_data)
{
   Compiler * restrict c = interp->compiler;
   assert(c->ovfdecl.active);

   EmpVm * restrict vm = c->vm;
   Tape tape_ = {.code = &vm->code, .linenr = interp->linenr};
   Tape * const tape = &tape_;

   /* Update the params object, finalize the ovf and pop it */
   S_CHECK(emit_bytes(tape, OP_CALL_API, FN_OVF_UPDATEPARAMS,
                            OP_CALL_API, FN_OVF_FINALIZE,
                            OP_POP));

   assert(c->vmstack_depth >= 2);
   c->vmstack_depth -= 2;

   ovfdecl_empty(&c->ovfdecl);

   return OK;
}

/* This is a dummy function: at parse time we cannot know the number of args,
 * hence it is not possible get this information to the parser */
static unsigned c_ovf_param_getvecsize(UNUSED Interpreter* restrict interp,
                                       UNUSED void *ovfdef_data,
                                       UNUSED const struct ovf_param_def *pdef)
{
   return UINT_MAX;
}

static const char* c_ovf_getname(Interpreter* restrict interp, UNUSED void *ovfdef_data)
{
   Compiler *c = interp->compiler;
   OvfDeclData * restrict ovfdecl = &c->ovfdecl;
   assert(ovfdecl->active);

   return ovfdecl->name;

}


static int c_read_param(Interpreter* restrict interp, unsigned *p,
                      IdentData* restrict ident,
                      const char *identstr, unsigned* restrict param_gidx)
{
   Compiler* restrict c = interp->compiler;

   EmpVm *vm = c->vm;
   Tape tape_ = {.code = &vm->code, .linenr = interp->linenr};
   Tape * const tape = &tape_;

   LoopIterators loopiters = {.size = 0};
   unsigned gmsfilter_gidx;

   const Lequ * vec;
   switch (ident->type) {
   case IdentLocalVector:
      assert(ident->idx < interp->globals.localvectors.len);
      vec = &interp->globals.localvectors.list[ident->idx];
      break;
   case IdentVector:
      assert(ident->idx < interp->globals.vectors.len);
      vec = &interp->globals.vectors.list[ident->idx];
      break;
   case IdentParam:
      TO_IMPLEMENT("multidimensional parameter processing");
   default:
      return runtime_error(interp->linenr);
   }

   /* ---------------------------------------------------------------------
    * Preparation: create the VmVector and inject it into globals.
    * Then, during the loop initialization we put this object into a local var
    * --------------------------------------------------------------------- */

   VmVector *vmvec;
   MALLOC_(vmvec, VmVector, 1);
   vmvec->type = ident->type;
   vmvec->len = 0;
   vmvec->lexeme = ident->lexeme;
   vmvec->data = vec;

   S_CHECK(vmvals_add(&vm->globals, PTR_VAL(vmvec)));
   unsigned vmvec_gidx = vm->globals.len-1;


   /* ---------------------------------------------------------------------
    * we want to read a specification  param('a', n)$(inset(n))
    *
    * We use a loop to get this. Steps are:
    * 1) Parse the indices "('a', n)"
    * 2) If present, parse the condition
    * 3) Get the parameter value
    * --------------------------------------------------------------------- */

   /* RIGHT NOW we enter a local scope as we will define variables for looping */

   TokenType toktype;
   S_CHECK(advance(interp, p, &toktype));
   S_CHECK(parser_expect(interp, "GMS indices are expected", TOK_LPAREN));

   begin_scope(c, __func__);
   GmsSymIterator * restrict filter = &interp->gms_sym_iterator;
   assert(!filter->active);
   filter->active = true;
   filter->compact = true;
   filter->loop_needed = false;

   GmsIndicesData *indices = &filter->indices;
   gms_indicesdata_init(indices);

   S_CHECK(parse_gmsindices(interp, p, indices));

   S_CHECK(gmssymiter_init(interp, ident, &interp->gms_sym_iterator.indices, &loopiters,
                              tape, &gmsfilter_gidx));

   if (loopiters.size != 1) {
      error("[empcompiler] ERROR line %u: OVF parameter can only have one set as indices, got %u\n",
            interp->linenr, loopiters.size);
      return Error_EMPIncorrectSyntax;
   }

   /* ---------------------------------------------------------------------
    * Copy the vmvec ovject from global to a local variable
    * --------------------------------------------------------------------- */

   LIDX_TYPE vmvec_lidx;
   S_CHECK(declare_localvar(interp, tape, &ident->lexeme, "_vmvec_obj",
                            IdentInternalIndex, CstZeroUInt, &vmvec_lidx));

   S_CHECK(emit_bytes(tape, OP_LOCAL_COPYFROM_GIDX, vmvec_lidx));
   S_CHECK(EMIT_GIDX(tape, vmvec_gidx));

   /* ---------------------------------------------------------------------
    * We are ready to start the loop
    * --------------------------------------------------------------------- */

   S_CHECK(loop_initandstart(interp, tape, &loopiters));
   unsigned lvecidx_lidx = loopiters.iters[0].iter_lidx;

   /* ---------------------------------------------------------------------
    * Step 2: If there is a condition, parse it and set the jumps
    * --------------------------------------------------------------------- */

   unsigned p2 = *p;
   S_CHECK(peek(interp, &p2, &toktype));

   if (toktype == TOK_CONDITION) {
      *p = p2;
      S_CHECK(parse_condition(interp, p, c, tape));
   }

   /* ---------------------------------------------------------------------
    * Step 3: Main loop body: vmvecin vecidx_lidx to the vector
    * --------------------------------------------------------------------- */
   S_CHECK(emit_bytes(tape, OP_VECTOR_EXTEND, vmvec_lidx, lvecidx_lidx));

   /* ---------------------------------------------------------------------
    * Step 4: Loop increment:
    * Resolve all remaining jumps linked to a false condition
    * --------------------------------------------------------------------- */

   S_CHECK(patch_jumps(&c->falsey_jumps, tape, c->scope_depth));

   S_CHECK(loop_increment(tape, &loopiters));

   /* ---------------------------------------------------------------------
    * We are at the end of the loop. We allow for outstanding "true" jump
    * to come here.
    * --------------------------------------------------------------------- */

   S_CHECK(patch_jumps(&c->truey_jumps, tape, c->scope_depth));

   assert(no_outstanding_jump(&c->truey_jumps, c->scope_depth));
   assert(no_outstanding_jump(&c->falsey_jumps, c->scope_depth));



   /* ---------------------------------------------------------------------
    * We can now update the ovf parameter: we create that object and
    * in OP_OVFPARAM_SYNC, the dscratch will be used to copy it.
    * --------------------------------------------------------------------- */

   OvfParam * restrict param_update;
   MALLOC_(param_update, OvfParam, 1);

   S_CHECK(vmvals_add(&vm->globals, PTR_VAL(param_update)));
   *param_gidx = vm->globals.len - 1;

   S_CHECK(emit_bytes(tape, OP_OVFPARAM_SYNC, vmvec_lidx));
   S_CHECK(EMIT_GIDX(tape, *param_gidx));

   /* END the scope after using vmvec_lidx */
   S_CHECK(end_scope(interp, tape));


   filter->active = false;

   return OK;
}

static int c_mp_new(Interpreter *interp, RhpSense sense, MathPrgm **mp)
{
   assert(sense < UINT8_MAX);

   Compiler *c = interp->compiler;
   EmpVm * restrict vm = c->vm;
   Tape tape_ = {.code = &vm->code, .linenr = interp->linenr};
   Tape * const tape = &tape_;

   S_CHECK(emit_bytes(tape, OP_PUSH_BYTE, sense));

   /* ---------------------------------------------------------------------
    * If we have a valid regentry template, we pass it along
    * --------------------------------------------------------------------- */

   if (c->regentry_gidx < UINT_MAX) {
      S_CHECK(emit_byte(tape, OP_PUSH_GIDX));
      S_CHECK(EMIT_GIDX(tape, c->regentry_gidx));
      c->regentry_gidx = UINT_MAX;
   } else {
      S_CHECK(emit_byte(tape, OP_NIL));

   }


   S_CHECK(emit_bytes(tape, OP_NEW_OBJ, FN_MP_NEW));

   UPDATE_STACK_MAX(c, empnewobjs[FN_MP_NEW].argc);
   c->vmstack_depth += 1;

   *mp = NULL;

   return OK;
}

static int c_mp_addcons(Interpreter *interp, UNUSED MathPrgm *mp)
{
   Compiler *c = interp->compiler;

   EmpVm * restrict vm = c->vm;
   Tape tape_ = {.code = &vm->code, .linenr = interp->linenr};
   Tape * const tape = &tape_;

   S_CHECK(emit_bytes(tape, OP_CALL_API, FN_MP_ADDCONS));

   return OK;
}

static int c_mp_addvars(Interpreter *interp, UNUSED MathPrgm *mp)
{
   Compiler *c = interp->compiler;
   EmpVm * restrict vm = c->vm;
   Tape tape_ = {.code = &vm->code, .linenr = interp->linenr};
   Tape * const tape = &tape_;

   S_CHECK(emit_bytes(tape, OP_CALL_API, FN_MP_ADDVARS));

   return OK;
}

static int c_mp_addvipairs(Interpreter *interp, UNUSED MathPrgm *mp)
{
   Compiler *c = interp->compiler;
   EmpVm * restrict vm = c->vm;
   Tape tape_ = {.code = &vm->code, .linenr = interp->linenr};
   Tape * const tape = &tape_;

   S_CHECK(emit_bytes(tape, OP_CALL_API, FN_MP_ADDVIPAIRS));

   return OK;
}

static int c_mp_addzerofunc(Interpreter *interp, UNUSED MathPrgm *mp)
{
   Compiler *c = interp->compiler;
   EmpVm * restrict vm = c->vm;
   Tape tape_ = {.code = &vm->code, .linenr = interp->linenr};
   Tape * const tape = &tape_;

   S_CHECK(emit_bytes(tape, OP_CALL_API, FN_MP_ADDZEROFUNC));

   return OK;
}

static int c_mp_finalize(UNUSED Interpreter *interp, UNUSED MathPrgm *mp)
{
   Compiler *c = interp->compiler;
   EmpVm * restrict vm = c->vm;
   Tape tape_ = {.code = &vm->code, .linenr = interp->linenr};
   Tape * const tape = &tape_;

   S_CHECK(emit_bytes(tape, OP_CALL_API, FN_MP_FINALIZE, OP_POP));

   assert(c->vmstack_depth >= 1);
   c->vmstack_depth -= 1;

   return OK;
}

static int c_mp_setobjvar(Interpreter *interp, UNUSED MathPrgm *mp)
{
   Compiler *c = interp->compiler;
   EmpVm * restrict vm = c->vm;
   Tape tape_ = {.code = &vm->code, .linenr = interp->linenr};
   Tape * const tape = &tape_;

   S_CHECK(emit_bytes(tape, OP_CALL_API, FN_MP_SETOBJVAR));

   return OK;
}

static int c_mp_setprobtype(UNUSED Interpreter *interp, UNUSED MathPrgm *mp, unsigned probtype)
{
   assert(probtype < UINT8_MAX);
   Compiler *c = interp->compiler;
   EmpVm * restrict vm = c->vm;
   Tape tape_ = {.code = &vm->code, .linenr = interp->linenr};
   Tape * const tape = &tape_;

   S_CHECK(emit_bytes(tape, OP_PUSH_BYTE, probtype, OP_CALL_API, FN_MP_SETPROBTYPE));

   return OK;
}

static int c_mp_settype(UNUSED Interpreter *interp, UNUSED MathPrgm *mp, unsigned type)
{
   assert(type < UINT8_MAX);
   Compiler *c = interp->compiler;
   EmpVm * restrict vm = c->vm;
   Tape tape_ = {.code = &vm->code, .linenr = interp->linenr};
   Tape * const tape = &tape_;

   S_CHECK(emit_bytes(tape, OP_PUSH_BYTE, type, OP_CALL_API, FN_MP_SETTYPE));

   return OK;
}

static int c_mpe_new(Interpreter *interp, UNUSED Nash **mpe)
{
   Compiler *c = interp->compiler;
   EmpVm * restrict vm = c->vm;
   Tape tape_ = {.code = &vm->code, .linenr = interp->linenr};
   Tape * const tape = &tape_;

   S_CHECK(emit_bytes(tape, OP_NEW_OBJ, FN_MPE_NEW));

   return OK;
}

static int c_mpe_addmp(Interpreter *interp, unsigned mpe_id, UNUSED MathPrgm *mp)
{
   assert(mpe_id < UINT8_MAX);

   Compiler *c = interp->compiler;
   EmpVm * restrict vm = c->vm;
   Tape tape_ = {.code = &vm->code, .linenr = interp->linenr};
   Tape * const tape = &tape_;

   S_CHECK(emit_bytes(tape, OP_PUSH_BYTE, mpe_id, OP_CALL_API, FN_MPE_ADDMPBYID));

   return OK;
}

static int c_mpe_finalize(UNUSED Interpreter *interp, UNUSED Nash *mpe)
{
   Compiler *c = interp->compiler;
   EmpVm * restrict vm = c->vm;
   Tape tape_ = {.code = &vm->code, .linenr = interp->linenr};
   Tape * const tape = &tape_;

   S_CHECK(emit_bytes(tape, OP_CALL_API, FN_MPE_FINALIZE, OP_POP));

   assert(c->vmstack_depth >= 1);
   c->vmstack_depth -= 1;

   return OK;
}

/* ----------------------------------------------------------------------
 * Misc functions
 * ---------------------------------------------------------------------- */

static int c_ctr_markequasflipped(Interpreter *interp)
{
   Compiler *c = interp->compiler;
   EmpVm * restrict vm = c->vm;
   Tape tape_ = {.code = &vm->code, .linenr = interp->linenr};
   Tape * const tape = &tape_;

   S_CHECK(emit_bytes(tape, OP_CALL_API, FN_CTR_MARKEQUASFLIPPED));

   return OK;
}

const struct parser_ops parser_ops_compiler = {
   .type = ParserOpsCompiler,
   .ccflib_new = c_ccflib_new,
   .ccflib_finalize = c_ccflib_finalize,
   .ctr_markequasflipped = c_ctr_markequasflipped,
   .gms_get_uelidx        = get_uelidx_via_dct,
   .gms_get_uelstr        = get_uelstr_via_dct,
   .gms_parse = c_gms_parse,
   .identaslabels = c_identaslabels,
   .mp_addcons = c_mp_addcons,
   .mp_addvars = c_mp_addvars,
   .mp_addvipairs = c_mp_addvipairs,
   .mp_addzerofunc = c_mp_addzerofunc,
   .mp_finalize = c_mp_finalize,
   .mp_new = c_mp_new,
   .mp_setobjvar = c_mp_setobjvar,
   .mp_setprobtype = c_mp_setprobtype,
   .mp_settype = c_mp_settype,
   .mpe_addmp = c_mpe_addmp,
   .mpe_new = c_mpe_new,
   .mpe_finalize = c_mpe_finalize,
   .ovf_addbyname = c_ovf_addbyname,
   .ovf_addarg = c_ovf_addarg,
   .ovf_paramsdefstart = c_ovf_paramsdefstart,
   .ovf_setparam = c_ovf_setparam,
   .ovf_setrhovar = c_ovf_setrhovar,
   .ovf_check = c_ovf_check,
   .ovf_param_getvecsize = c_ovf_param_getvecsize,
   .ovf_getname = c_ovf_getname,
   .read_param = c_read_param,
   .resolve_lexeme_as_gmssymb = resolve_lexeme_as_gmssymb_via_dct,
};


int empvm_finalize(Interpreter *interp)
{
   Compiler *c = interp->compiler;

   if (c->scope_depth != 0) {
      error("[empcompiler] ERROR: after parsing the file, the compiler depth"
            "is nonzero: %u", c->scope_depth);
   }
   /* no keyword is active */
   interp_resetlastkw(interp);

   assert(c->truey_jumps.len == 0 && c->falsey_jumps.len == 0);

   /* TODO: think whether we want to do a "symbolic" check in Emb mode? */
   if (!embmode(interp) && c->vm->code.len > 0) {
      trace_empparsermsg("[empinterp] VM execution\n");
      S_CHECK(vmcode_add(&c->vm->code, OP_END, interp->linenr));
      S_CHECK(empvm_run(c->vm));
   }

   c->vm->code.len = 0;
   return OK;
}

/**
 * @brief Switch to CompilerMode if in Imm mode
 *
 * @warning The VM code needs to be executed at the first opportunity
 * as the EMPDAG uid values will be wrong if not.
 *
 * @param       interp    the interpreter 
 * @param[out]  switched  true if we switched to compiler mode
 *
 * @return                the error code
 */
int c_switch_to_compmode(Interpreter *interp, bool *switched)
{
   if (interp->ops->type != ParserOpsCompiler && interp->ops->type != ParserOpsEmb) {
      *switched = true;

      EmpVm *vm = interp->compiler->vm;
      if (vm->code.len) {
         TO_IMPLEMENT("temporary switch to vmmode with existing bytecode");
      }

      interp->ops = &parser_ops_compiler;
      vm->data.uid_parent = interp->uid_parent;
      vm->data.uid_grandparent = interp->uid_grandparent;

      return OK;
   }

   *switched = false;

   return OK;
}

int c_switch_to_immmode(Interpreter *interp)
{
   S_CHECK(empvm_finalize(interp));

   if (interp->ops->type != ParserOpsEmb) {
      interp->ops = &parser_ops_imm;
   }

   EmpVm *vm = interp->compiler->vm;
   vm->data.uid_parent = EMPDAG_UID_NONE;
   vm->data.uid_grandparent = EMPDAG_UID_NONE;

   return OK;
}
