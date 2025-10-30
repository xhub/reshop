#include "reshop_config.h"

#include <stdarg.h>

#include "empinfo.h"
#include "empinterp.h"
#include "empinterp_linkbuilder.h"
#include "empinterp_ops_utils.h"
#include "empinterp_priv.h"
#include "empinterp_utils.h"
#include "empinterp_vm.h"
#include "empinterp_vm_compiler.h"
#include "empinterp_vm_utils.h"
#include "empparser_priv.h"
#include "empparser_utils.h"
#include "macros.h"
#include "mathprgm.h"
#include "mdl.h"
#include "ovf_parameter.h"
#include "printout.h"

static const char * const attrs_set[] = {
   "first",
   "last",
   "len",
   "off",
   "ord",
   "pos",
   "rev",
   "tlen",
   "tval",
   "uel",
   "val",
};

typedef struct {
   const char *attr;
   EmpVmOpCode opcode;
} SetAttrs;

static const char * const attrs_set_supported[] = {
   "first",
   "last",
};

typedef struct {
   LIDX_TYPE idx_lidx;
   union {
      LIDX_TYPE max_lidx;
      GIDX_TYPE max_gidx;
   };
} IdentLoopIteratorData;

typedef struct {
   IdentType type;
   Lexeme lexeme;
   unsigned depth;
   unsigned idx;  /**< TODO: what is this ? */
   union {
      IdentLoopIteratorData iteratordat;
   };
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

typedef struct {
   unsigned scope_depth;
   unsigned jump_depth;
   unsigned vmstack_depth;
   unsigned vmstack_max;
} CompilerState;

typedef struct empvm_compiler {
   LocalVar locals[UINT8_COUNT];
   unsigned local_count;
   //uint8_t  stacked_tokens[UINT8_COUNT];
   //uint8_t  stacked_tokens_count;

   CompilerState state;

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

const char * identtype2str(IdentType type)
{
   const char *identtype_str_[IdentTypeMaxValue] = {
   [IdentNotFound]        = "not found",
   [IdentLoopIterator]    = "loop iterator",
   [IdentSet]             = "GAMS set",
   [IdentLocalSet]        = "local GAMS set",
   [IdentMultiSet]        = "multidimensional GAMS set",
   [IdentLocalMultiSet]   = "local GAMS multiset",
   [IdentScalar]          = "GAMS scalar parameter",
   [IdentVector]          = "GAMS vector parameter",
   [IdentParam]           = "GAMS multidimensional parameter",
   [IdentLocalScalar]     = "local scalar",
   [IdentLocalVector]     = "local vector",
   [IdentLocalParam]      = "local parameter",
   [IdentInternalIndex]   = "internal index",
   [IdentInternalMax]     = "internal max",
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

NONNULL static 
int parse_condition(Interpreter * restrict interp, unsigned * restrict p,
                    Compiler * restrict c, Tape * restrict tape);

static int parse_loopsets(Interpreter * restrict interp, unsigned * restrict p,
                          GmsIndicesData * restrict indices);
static int patch_jumps(Jumps * restrict jumps, Tape * restrict tape, unsigned depth);




static int delimiter_error(Interpreter *interp, TokenType closing_delimiter, const char *opname)
{
   char buf[256];
   (void)snprintf(buf, sizeof buf, "Expecting a %s to end %s", toktype2str(closing_delimiter),
                  opname);

   return parser_err(interp, buf);
}

// HACK: this should not be here
static const char * const smoothing_scheme_options[] =
   {"LogSumExp", NULL};

static void smoothing_scheme_setter(void *dat, unsigned idx)
{
   assert(idx <= SmoothingOperatorSchemeLast);
   SmoothingOperatorData *smoothing_operator = dat;
   smoothing_operator->scheme = idx;
}

UNUSED static void smoothing_par_dblsetter(void *dat, double val)
{
   SmoothingOperatorData *smoothing_operator = dat;
   smoothing_operator->parameter = val;
}

static void smoothing_par_uintsetter(void *dat, unsigned pos)
{
   SmoothingOperatorData *smoothing_operator = dat;
   smoothing_operator->parameter_position = pos;
}

NONNULL DBGUSED static bool chk_ident_idx(const VmData *vmdat, const IdentData *ident)
{
   switch (ident->type) {
   case IdentSet:
      return ident->idx < vmdat->globals->sets.len;
   default:
      return false;
   }
}

static int vm_gmssymiter_alloc(Compiler* restrict c, const IdentData *ident,
                               VmGmsSymIterator **f, unsigned *gidx);

#define jumps_add_verbose(jumps, j) \
 jumps_add(jumps, j); \
trace_empparser("%s:%d :: JUMP: adding to " #jumps " at pos %u; depth = %5u; addr = %5u\n", __func__, __LINE__, (jumps)->len-1, (j).depth, (j).addr);

// #define jumps_add_verbose jumps_add

static int jump_depth_overflow(void)
{
   error("[empcompiler] ERROR: condition depth reached its maximum value %u. "
         "Reduce the nested conditions.\n", UINT_MAX);

   return Error_EMPIncorrectInput;
}

static int jump_depth_underflow(void)
{
   errormsg("[empinterp] ERROR %u: condition depth reached 0 and can't be decreased. "
         "Inspect the conditions to catch mistakes.\n");

   return Error_EMPIncorrectInput;
}


NONNULL static inline unsigned jump_depth(Compiler *c) {
   return c->state.jump_depth;
}

NONNULL static inline int jump_depth_inc(Compiler *c)
{
   if (RHP_UNLIKELY(c->state.jump_depth == UINT_MAX)) {
      return jump_depth_overflow();
   }

   c->state.jump_depth++;
   return OK;
}

NONNULL static inline int jump_depth_dec(Compiler *c)
{
   if (RHP_UNLIKELY(c->state.jump_depth == 0)) {
      return jump_depth_underflow();
   }

   c->state.jump_depth--;
   return OK;
}

/**
 * @brief Normalize the depth values to be at most a given depth
 *
 * @param jumps  the jumps to normalize
 * @param depth  the depth at which to normalize (upper bound)
 *
 * @return       the error code
 */
static int jump_depth_normalize(Jumps *jumps, unsigned depth)
{
   if (jumps->len == 0) {
      trace_empparsermsg("[empcompiler] ERROR: no jump to normalize\n");
      return Error_RuntimeError;
   }

   trace_empparser("[empcompiler] JUMPS: normalize depth to %u\n", depth);

   for (unsigned len = jumps->len, i = len-1; i < len; --i) {
      Jump * jump = &jumps->list[i];

      /* NOTE: unclear whether we rather want to check all and change the depth when
       * it is > depth */
      if (jump->depth < depth) {
         return OK;
      }

      jump->depth = depth;
   }

   return OK;
}

/**
 * @brief Normalize the falsey jumps to a be less or equal to a given depth
 *
 * This is necessary for the falsey jumps to only be resolve after the loop body.
 * This also checks for any remaining truey jumps.
 *
 * @param c  the EMP compiler
 *
 * @return   the error code
 */
static int c_normalize_jumps_depth(Compiler *c)
{
   /* NOTE: the check below might be too strict. For now, no good example why this
    * would be valid */
   if (c->truey_jumps.len > 0) {
      errormsg("[empinterp] ERROR: unexpected truey jump\n");
      return Error_RuntimeError;
   }

   S_CHECK(jump_depth_normalize(&c->falsey_jumps, c->state.scope_depth));

   return OK;
}

/* ---------------------------------------------------------------------
 * Short design note:
 *
 * - The compiler->locals are used in the VM for scoped variables and loop ones
 *   The opcodes OP_LOCAL operates on them and alwats take as first instruction
 *   the local index.
 * --------------------------------------------------------------------- */


#define UPDATE_STACK_MAX(c, argc) \
   (c)->state.vmstack_max = MAX((c)->state.vmstack_max, (c)->state.vmstack_depth + (argc))


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
   if (!embmode(interp)) {
      interp->ops = &interp_ops_compiler;
   }

   Compiler *c = interp->compiler;

   if (!c) {
      c = compiler_init(interp);
   }

   return c;
}

#define emit_bytes(tape, ...) _emit_bytes(tape, VA_NARG_TYPED(u8, __VA_ARGS__), __VA_ARGS__); 

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

   trace_empparser("[empcompiler] EMITTING jump '%s' @%u\n", opcodes_name(instr),
                   *jump_addr);

   return OK;
}

UNUSED static inline int emit_patchable_byte(Tape *tape, unsigned *addr) {
   S_CHECK(emit_byte(tape, 0xff));
   assert(tape->code->len > 1);
   *addr = tape->code->len - 1;

   return OK;
}

UNUSED static inline int emit_patchable_short(Tape *tape, unsigned *addr) {
   S_CHECK(emit_short(tape, 0xffff));
   assert(tape->code->len > 2);
   *addr = tape->code->len - 2;

   return OK;
}

/* TODO(cleanup): this should be used rather than direct access */
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

static int patch_jump(Tape *tape, unsigned jump_addr)
{
   // -2 to adjust for the bytecode for the jump jump_addr itself.
   assert(jump_addr >= 2 && tape->code->len > jump_addr - 2);
   unsigned jump_val = tape->code->len - jump_addr - 2;

   if (jump_val >= UINT16_MAX) {
      error("[empcompiler] jump %u too large, max is %u", jump_val, UINT16_MAX-1);
      return Error_EMPRuntimeError;
   }

   trace_empparser("[empcompiler] PATCHING jump @%u to %u\n", jump_addr-1, jump_val);

   tape->code->ip[jump_addr] = (jump_val >> 8) & 0xff;
   tape->code->ip[jump_addr + 1] = jump_val & 0xff;

   return OK;
}

UNUSED static int patch_byte(Tape *tape, unsigned addr, uint8_t val) {
   assert(tape->code->len > addr);

   trace_empparser("[empcompiler] PATCHING ip@%u to %s #%u\n", addr,
                   opcodes_name(val), val);

   tape->code->ip[addr] = val;

   return OK;
}

UNUSED static int patch_short(Tape *tape, unsigned addr, uint16_t val) {
   assert(tape->code->len > addr + 1);

   trace_empparser("[empcompiler] PATCHING ip@%u to #%u\n", addr, val);

   tape->code->ip[addr] = (val >> 8) & 0xff;
   tape->code->ip[addr + 1] = val & 0xff;

   return OK;
}

/**
 * @brief Codegen for a NOT in the context of a projection like 'NOT sum { ... }'
 *
 * @param c     the EMP compiler
 * @param tape  the VM tape
 * 
 * @return      the error code
 */
static int emit_not_projection(Compiler * restrict c, Tape * restrict tape)
{
   /* FALSE case: we push false and jump to the continue */
   Jump jump_false = { .depth = UINT_MAX };
   S_CHECK(emit_byte(tape, OP_PUSH_FALSE))
   S_CHECK(emit_jump(tape, OP_JUMP, &jump_false.addr));

   /* TRUE case: we push true and continue */
   S_CHECK(patch_jumps(&c->truey_jumps, tape, jump_depth(c)));
   S_CHECK(emit_byte(tape, OP_PUSH_TRUE))

   /* Make sure the FALSE case comes here */
   S_CHECK(patch_jump(tape, jump_false.addr));

   S_CHECK(emit_byte(tape, OP_NOT));

   Jump jump = { .depth = jump_depth(c) };
   S_CHECK(emit_jump(tape, OP_JUMP_IF_TRUE, &jump.addr));
   S_CHECK(jumps_add_verbose(&c->truey_jumps, jump));

   return OK;
}

static inline void begin_scope(Compiler * restrict c, const char *fn)
{
   c->state.scope_depth++;
   c->state.jump_depth++;
   trace_empparser("[empcompiler] in %s scope depth is now %u; jump depth is %u.\n",
                   fn, c->state.scope_depth, c->state.jump_depth);
}

static inline int end_scope(Interpreter *interp, UNUSED Tape* tape) {
   Compiler * restrict c = interp->compiler;
   c->state.scope_depth--;
   c->state.jump_depth--;

   trace_empparser("[empcompiler] line %u: scope depth is %u; jump depth is %u\n",
                   tape->linenr, c->state.scope_depth, c->state.jump_depth);

   LocalVar *lvar;

   while (c->local_count > 0 && ((lvar = &c->locals[c->local_count - 1])
      && lvar->depth < UINT_MAX && lvar->depth > c->state.scope_depth)) {

      trace_empparser("[empcompiler] locals: removing '%.*s' of type %s\n",
                      lvar->lexeme.len, lvar->lexeme.start,
                      identtype2str(lvar->type));

      free((char*)lvar->lexeme.start);
      c->local_count--;
   }

   if (O_Output & PO_TRACE_EMPPARSER) {
      if (c->local_count > 0) {
         trace_empparsermsg("[empcompiler] locals: remaining locals are:\n");
         for (unsigned i = 0, len = c->local_count; i < len; ++i) {
            lvar = &c->locals[i];
            trace_empparser("[%5u] '%.*s' of type %s\n", i,
                            lvar->lexeme.len, lvar->lexeme.start,
                            identtype2str(lvar->type));
         }
      }
   }

   if (c->state.scope_depth == 0) {

      if (c->state.jump_depth > 0) {
         error("[empcompiler] ERROR on line %u: scope depth is 0, but jump depth is %u. "
               "Please report this as a bug.\n", interp->linenr, c->state.jump_depth);
         return Error_EMPRuntimeError;
      }

      assert(c->truey_jumps.len == 0 && c->falsey_jumps.len == 0);
      /* no keyword is active */
      interp_resetlastkw(interp);

      /* TODO: think whether we want to do a "symbolic" check in Emb mode? */
      if (!embmode(interp) && c->vm->code.len > 0) {
         trace_empparsermsg("\n[empinterp] VM execution\n");
         S_CHECK(vmcode_add(&c->vm->code, OP_END, interp->linenr));
         S_CHECK(empvm_run(c->vm));
      }

      c->vm->code.len = 0;

      if (interp->ops->type == InterpreterOpsCompiler) {
         interp->ops = &interp_ops_imm;
      }
   }

   return OK;
}

static inline bool lexeme_equal(Lexeme *a, Lexeme *b) {
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

      if (lexeme_equal(&ident->lexeme, &local->lexeme)) {
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
            error("%s :: case '%s' not handled", __func__, identtype2str(ident->type));
            return false;
         }

         ident->origin = IdentOriginLocal;
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
                   identtype2str(local->type));

   return OK;
}

static int declare_variable(Compiler *c, Lexeme* tok, LIDX_TYPE *lidx, IdentType type)
{
   // We have no concept of global variables
   //  if (c->state.scope_depth == 0) return OK;

   for (unsigned len = c->local_count, i = len-1; i < len; i--) {
      LocalVar* local = &c->locals[i];
      if (local->depth != UINT_MAX && local->depth < c->state.scope_depth) {
         break; // [negative]
      }

      if (lexeme_equal(tok, &local->lexeme)) {
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
                      c->state.scope_depth);
   }

   c->locals[lvar_idx].depth = c->state.scope_depth;
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
      S_CHECK(emit_bytes(tape, OP_LVAR_COPYFROM_GIDX, (uint8_t)lidx));
      S_CHECK(EMIT_GIDX(tape, initval_gidx));
   }

   c->locals[lidx].depth = c->state.scope_depth;

   *idx = lidx;

   return OK;
}

/**
 * @brief Define two local variables for a loop iterator
 *
 * @param interp   the EMP interpreter
 * @param tape     the VM tape
 * @param ident    the loop iterator
 * @param iterator the loop iterator data
 *
 * @return         the error code
 */
static int define_lvars_from_iterator(Interpreter * restrict interp, Tape * restrict tape,
                                      IdentData * restrict ident, CodegenIteratorData * restrict iterator)
{
   S_CHECK(declare_localvar(interp, tape, &ident->lexeme, "_idx",
                            IdentInternalIndex, CstZeroUInt, &iterator->idx_lidx));

   S_CHECK(declare_localvar(interp, tape, &ident->lexeme, "",
                            IdentLoopIterator, CstZeroUInt, &iterator->iter_lidx));

   /* Copy the indices to the local variable */
   Compiler *c = interp->compiler;
   LocalVar *lvar = &c->locals[iterator->iter_lidx];
   lvar->iteratordat.idx_lidx = iterator->idx_lidx;
   lvar->iteratordat.idx_lidx = iterator->idx_lidx;

   IdentType type = ident->type;
   switch (type) {
   case IdentSet: {
      lvar->iteratordat.max_gidx = iterator->idxmax_gidx;
      break;
   }
   case IdentLocalSet:
      lvar->iteratordat.max_lidx = iterator->idxmax_lidx;
       break;
    default:
       return runtime_error(interp->linenr);
   }

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
   const unsigned niters = iterators->niters;
   IdentData * restrict loopidents = iterators->idents;
   CodegenIteratorData * restrict loopiters = iterators->iters;
   assert(niters < GMS_MAX_INDEX_DIM);
   EmpVm * restrict vm = interp->compiler->vm;

   /* ---------------------------------------------------------------------
    * Loop initialization: go over all arguments and:
    *  - declare 2 local variables per arguments: the loop index and the set element
    *  - Collect all the length of 
    *  - If we loop over a dynamic set, get the size of that set
    * --------------------------------------------------------------------- */

   for (unsigned i = 0; i < niters; ++i) {

      IdentData * restrict ident = &loopidents[i];
      CodegenIteratorData * restrict iterator = &loopiters[i];

      IdentType type = ident->type;
      switch (type) {
      case IdentSet: {
         S_CHECK(vm_store_set_nrecs(interp, vm, ident, &iterator->idxmax_gidx))
         break;
      }
      case IdentLocalSet:
         /* We need to query the size of the dynamic local set */
         S_CHECK(declare_localvar(interp, tape, &ident->lexeme, "_max",
                                  IdentInternalMax, GIDX_MAX, &iterator->idxmax_lidx));

         S_CHECK(emit_bytes(tape, OP_LVAR_COPYOBJLEN, iterator->idxmax_lidx, type));
         S_CHECK(EMIT_GIDX(tape, ident->idx));

            /* --------------------------------------------------------------
             * We check if some sets are empty
             *
             * TODO: this could be done elsewhere, but we need to review the
             * codegen for the FOR loop.
             * ------------------------------------------------------------- */

         S_CHECK(emit_bytes(tape, OP_PUSH_BYTE, 0, OP_PUSH_LIDX,
                            iterator->idxmax_lidx, OP_EQUAL));

         Jump jump = { .depth = jump_depth(interp->compiler) };
         S_CHECK(emit_jump(tape, OP_JUMP_IF_TRUE, &jump.addr));
         S_CHECK(jumps_add_verbose(&interp->compiler->truey_jumps, jump));

         break;

      case IdentLoopIterator:
         continue; /* Skip localvar declaration */
      default:
         return runtime_error(interp->linenr);
      }

      S_CHECK(define_lvars_from_iterator(interp, tape, ident, iterator));
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
   const u8 niters = iterators->niters;
   const IdentData * restrict loopidents = iterators->idents;
   CodegenIteratorData * restrict loopiters = iterators->iters;

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
         opcode_expected = OP_LINKLABELS_SETFROM_LOOPVAR;
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

   for (unsigned i = 0; i < niters; ++i) {

      loopiters[i].tapepos_at_loopstart = tape->code->len;
      const IdentData * restrict ident = &loopidents[i];
      CodegenIteratorData * restrict iterator = &loopiters[i];

      if (ident->type == IdentLoopIterator) {
         continue;
      }

      assert(embmode(interp) || chk_ident_idx(&interp->compiler->vm->data, ident));
 
      /* a_elt  <-  a_set[a_idx] */
      S_CHECK(emit_bytes(tape, OP_LOOPVAR_UPDATE, iterator->iter_lidx,
                         loopiters[i].idx_lidx, ident->type));
      S_CHECK(EMIT_GIDX(tape, ident->idx));

      if (loopobj_gidx < UINT_MAX) {
         assert(iterators->varidxs2pos[i] < GMS_MAX_INDEX_DIM);
         S_CHECK(emit_bytes(tape, opcode_loopobj_update));
         S_CHECK(EMIT_GIDX(tape, loopobj_gidx));
         S_CHECK(emit_bytes(tape, iterators->varidxs2pos[i], iterator->iter_lidx));
      }

   }

   return OK;
}


static int patch_jumps(Jumps * restrict jumps, Tape * restrict tape, unsigned depth)
{
   if (jumps->len == 0) { trace_empparsermsg("[empcompiler] JUMP: nothing to patch\n"); return OK; }

   trace_empparser("[empcompiler] JUMP: patching JUMPs with depth >= %u\n", depth);

   for (unsigned len = jumps->len, i = len-1; i < len; --i) {
      const Jump * jump = &jumps->list[i];
      //assert(jump->depth < UINT_MAX && jump->depth > 0 && jump->addr < UINT_MAX);

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
   unsigned niters = iterators->niters;
   const IdentData* restrict idents = iterators->idents;
   CodegenIteratorData* restrict iters = iterators->iters;

   /* a_idx++; if (a_idx < a_max); jump to loop_start */
   for (unsigned i = niters-1; i < niters; --i) {

      const IdentData * restrict ident = &idents[i];
      CodegenIteratorData * restrict iter = &iters[i];
      unsigned idx_i = iter->idx_lidx;

      if (ident->type == IdentLoopIterator) { continue; }

      S_CHECK(emit_bytes(tape, OP_LVAR_INC, idx_i, OP_PUSH_LIDX, idx_i));

      switch (ident->type) {
      case IdentSet:
         S_CHECK(emit_byte(tape, OP_PUSH_VMUINT));
         S_CHECK(EMIT_GIDX(tape, iter->idxmax_gidx));
         break;
      case IdentLocalSet:
         S_CHECK(emit_bytes(tape, OP_PUSH_LIDX, iter->idxmax_lidx));
         break;
      default:
         error("%s :: unsupported loop index %s", __func__, identtype2str(ident->type));
         return Error_NotImplemented;
      }
      S_CHECK(emit_byte(tape, OP_EQUAL));

      assert(iter->tapepos_at_loopstart < UINT_MAX);
      S_CHECK(emit_jump_back_false(tape, iter->tapepos_at_loopstart));

      if (i > 0) {
         S_CHECK(emit_bytes(tape, OP_LVAR_COPYFROM_GIDX, idx_i));
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
      //assert(jump->depth < UINT_MAX && jump->depth > 0 && jump->addr < UINT_MAX);

      if (jump->depth >= depth) {
         return false;
      }

   }

   return true;
}

static int vm_gmssymiter_alloc(Compiler* restrict c, const IdentData *ident,
                               VmGmsSymIterator **f, unsigned *gidx)
{
   unsigned dim = ident->dim;
   VmGmsSymIterator *symiter;
   MALLOCBYTES_(symiter, VmGmsSymIterator, sizeof(VmGmsSymIterator) + (dim*sizeof(int)));
   *f = symiter;

   symiter->ident = *ident;
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

/**
 * @brief Allocate a linklabel object
 *
 * @param      vm           the EMP VM
 * @param[out] link         the allocated link
 * @param      label        the label start
 * @param      label_len    the label length
 * @param      ndims        the number of dimensions
 * @param      nvardims     the number of varying dimensions inside of a loop
 * @param      children_sz  the 
 * @param      linktype     the link type
 * @param[out] gidx         the global index of the link
 *
 * @return                  the error code
 */
static int vm_linklabels_alloc(EmpVm * restrict vm, LinkLabels **link,
                              const char* label, unsigned label_len,
                              uint8_t ndims, uint8_t nvardims, unsigned children_sz,
                              LinkType linktype, unsigned *gidx)
{
   assert(children_sz == 0); // FIXME: this looks unused
   LinkLabels *link_;
   A_CHECK(link_, linklabels_new(linktype, label, label_len, ndims, nvardims, children_sz));

   assert(linklabels_valid(link_));
   S_CHECK(vmvals_add(&vm->globals, ARCOBJ_VAL(link_)));

   *link = link_;
   *gidx = vm->globals.len - 1;

   return OK;
}

/**
 * @brief Process the GAMS indices associated with an identifier (label/equ/var)
 *
 * - copy the constant indices in the uels
 * - Fill the iterators with the non-constant ones, if any
 * - Update the loop object if one of the index is an iterator of an outside loop
 *
 * @param      indices    the GAMS indices
 * @param      tape       the tape
 * @param[out] iterators  the loop iterators
 * @param[out] uels       the uels of the loop object
 * @param[out] compact    indicator if the label is continous
 *
 * @return                 the error code
 */
static int gmsindices_process(GmsIndicesData *indices, Tape * restrict tape,
                              LoopIterators *iterators, int * restrict uels, bool *compact)
{
   unsigned loopi = 0;
   GIDX_TYPE loopidx_gidx = iterators->loopobj_gidx;
   EmpVmOpCode upd_opcode = iterators->loopobj_opcode;

   assert(iterators->loopobj_gidx < UINT_MAX);
   assert(gmsindices_len(indices) < GMS_MAX_INDEX_DIM);

   for (unsigned i = 0, len = gmsindices_len(indices); i < len; ++i) {

      IdentData *ident = &indices->idents[i];

      switch (ident->type) {

      case IdentUEL:
         assert(ident->idx < INT_MAX && ident->idx > 0);
         uels[i] = (int)ident->idx;
         *compact = false; //TODO: is this the only case where this is false?
         break;

      case IdentSymbolSlice:
         uels[i] = 0;
         break;

      case IdentLoopIterator:
         // HACK: understand what is being done here
         if (upd_opcode < OP_MAXCODE) {
            S_CHECK(emit_byte(tape, upd_opcode));
            S_CHECK(EMIT_GIDX(tape, loopidx_gidx));
            S_CHECK(emit_bytes(tape, i, ident->idx));
         } else {
            memcpy(&iterators->idents[loopi], ident, sizeof(*ident));
            iterators->varidxs2pos[loopi] = i;
            loopi++;
         }

         break; 

      /* -------------------------------------------------------------------
       * We add this ident to the loopiterators
       * ------------------------------------------------------------------- */
      case IdentSet:
      case IdentLocalSet:
         iterators->varidxs2pos[loopi] = i;
         memcpy(&iterators->idents[loopi], ident, sizeof(*ident));
         loopi++;
         break;

      default:
         error("[empcompiler] ERROR: unexpected failure: got ident type '%s' "
               "for lexeme '%*s' at position %u.\n", identtype2str(ident->type),
               ident->lexeme.len, ident->lexeme.start, i);
         return Error_RuntimeError;
      }
   }

   iterators->niters = loopi;

   return OK;
}

/**
 * @brief Initialize loop iterators from a Cartesian products of 1D sets
 *
 * Important: we assume that all indices are either a set or a local set.
 * We only call this function when parsing a sum or loop operator.
 * This is very different from parsing a GAMS symbol / node or iterating over a multiset.
 *
 * @param iterators the iterators
 * @param indices   the indices
 */
static inline void iterators_init_from_gmsindices(LoopIterators* restrict iterators,
                                                 GmsIndicesData* restrict indices)
{
   iterators->loopobj_gidx = UINT_MAX;
   iterators->loopobj_opcode = OP_MAXCODE;
   u8 niters = iterators->niters = gmsindices_len(indices);

   memcpy(iterators->idents, indices->idents, niters*sizeof(IdentData));
}

static int gmssymiter_chk_dim(GmsIndicesData *indices, IdentData *ident, unsigned linenr)
{
   if (indices->nargs != ident->dim) {
      error("[empcompiler] ERROR on line %u: token '%.*s' has dimension %u but %u "
            "indices were given.\n", linenr, ident->lexeme.len,
            ident->lexeme.start, ident->dim, indices->nargs);
      return Error_EMPIncorrectInput;
   }

   return OK;
}

/**
 * @brief Initialize the iterator for reading a GAMS symbol
 *
 * @param       interp           the interpreter
 * @param       ident            the GAMS symbol ident
 * @param       indices          the GAMS indices
 * @param       iterators        the loop iterators
 * @param       tape             the VM tape
 * @param[out]  gmssymiter_gidx  the (global) index of the VM loop object
 *
 * @return      the error code
 */
static int gmssymiter_init(Interpreter * restrict interp, IdentData *ident,
                           GmsIndicesData *indices, LoopIterators *iterators,
                           Tape * restrict tape, unsigned * gmssymiter_gidx)
{
   Compiler *c = interp->compiler;

   VmGmsSymIterator *symiter;
   S_CHECK(vm_gmssymiter_alloc(c, ident, &symiter, gmssymiter_gidx));

   if (indices->nargs == 0) {
      iterators->niters = 0;
      iterators->loopobj_gidx = UINT_MAX;
      return OK;
   }

   iterators->loopobj_gidx = *gmssymiter_gidx;
   iterators->loopobj_opcode = OP_GMSSYMITER_SETFROM_LOOPVAR;

   S_CHECK(gmssymiter_chk_dim(indices, ident, interp->linenr));

  /* ----------------------------------------------------------------------
   * When reading a GAMS symbol, we want to make sure that when the user
   * specified a domain as one of the index, then we just substitute it
   * with ":", that is we take a slice
   * ---------------------------------------------------------------------- */
   assert(interp->cur.symdat.ident.type == ident->type);

   S_CHECK(gmssymiter_fixup_domains(interp, indices));

   return gmsindices_process(indices, tape, iterators, symiter->uels, &symiter->compact);
}


static int linklabels_init(Interpreter * restrict interp, Tape * restrict tape,
                           const char* label, unsigned label_len,
                           LinkType linktype, GmsIndicesData *indices,
                           LoopIterators *loopiterators, unsigned *linklabels_gidx)
{
   Compiler *c = interp->compiler;

   LinkLabels *linklabels;
   uint8_t ndims = gmsindices_len(indices);
   uint8_t nvardims = gmsindices_nvardims(indices);
   assert(ndims >= nvardims);

   unsigned gidx;
   S_CHECK(vm_linklabels_alloc(c->vm, &linklabels, label, label_len, ndims, nvardims, 0,
                               linktype, &gidx));
   *linklabels_gidx = gidx;

   if (ndims == 0) {
      loopiterators->niters = 0;
      loopiterators->loopobj_gidx = UINT_MAX;
      return OK;
   }

   loopiterators->loopobj_gidx = *linklabels_gidx;
   loopiterators->loopobj_opcode = OP_LINKLABELS_SETFROM_LOOPVAR;

   bool dummy;

   S_CHECK(gmsindices_process(indices, tape, loopiterators, linklabels->data, &dummy));

  /* ----------------------------------------------------------------------
   * Copy the coordinate (of the index) where the loop iterators belong,
   * but can't use memcpy as we don't have the same type. Example:
   *
   * n('1', i, '2', j) -> varidxs2pos = [1, 3]
   * ---------------------------------------------------------------------- */

   int *dst = &linklabels->data[ndims];
   for (u8 i = 0; i < nvardims; ++i) {
      *dst++ = loopiterators->varidxs2pos[i];
   }

   return OK;
}

static int
linklabels_init_from_loopiterators(Interpreter * restrict interp, Tape * restrict tape,
                                   const char* label, unsigned label_len,
                                   LinkType linktype, GmsIndicesData *indices,
                                   LoopIterators *loopiterators, unsigned *linklabels_gidx)
{
   Compiler *c = interp->compiler;

   LinkLabels *linklabels;
   uint8_t ndims = gmsindices_len(indices);
   uint8_t nvardims = gmsindices_nvardims(indices);
   assert(ndims >= nvardims);

   unsigned gidx;
   S_CHECK(vm_linklabels_alloc(c->vm, &linklabels, label, label_len, ndims, nvardims, 0,
                               linktype, &gidx));
   *linklabels_gidx = gidx;

   loopiterators->loopobj_gidx = gidx;
   loopiterators->loopobj_opcode = OP_LINKLABELS_SETFROM_LOOPVAR;

   bool dummy;

   S_CHECK(gmsindices_process(indices, tape, loopiterators, linklabels->data, &dummy));

  /* ----------------------------------------------------------------------
   * Copy the coordinate (of the index) where the loop iterators belong,
   * but can't use memcpy as we don't have the same type. Example:
   *
   * n('1', i, '2', j) -> varidxs2pos = [1, 3]
   * ---------------------------------------------------------------------- */

   int *dst = &linklabels->data[ndims];
   for (u8 i = 0; i < nvardims; ++i) {
      *dst++ = loopiterators->varidxs2pos[i];
   }

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
   S_CHECK(gmsindices_process(indices, tape, iterators, regentry->uels, &dummy));

   return OK;
}

/**
 * @brief Create loop iterators and generates the bytecode to start iterations
 *
 * @param      interp    the EMP interpreter
 * @param      p         the position index
 * @param      c         the EMP compiler
 * @param      tape      the VM tape
 * @param[out] loopiter  the resulting loop iterators
 *
 * @return              the error code
 */
static int membership_test_start(Interpreter * restrict interp, unsigned * restrict p,
                                 Compiler *c, Tape *tape, LoopIterators *loopiter)
{
   /* ---------------------------------------------------------------------
    * We initialize the iterators for a case like
    *    v
    * n$(leaf(n))
    *
    * and codegen the instructions to start the iteration
    * ---------------------------------------------------------------------- */

   assert(parser_getcurtoktype(interp) == TOK_IDENT || parser_getcurtoktype(interp) == TOK_GMS_SET);
   IdentData ident_gmsarray;
   S_CHECK(resolve_identas(interp, &ident_gmsarray, "In a conditional, a GAMS set is expected",
                           IdentLocalSet, IdentSet, IdentMultiSet));

   TokenType toktype;

   trace_empparser("[empcompiler] membership test with type '%s' and dimension %u\n",
                   identtype2str(ident_gmsarray.type), ident_gmsarray.dim);


   S_CHECK(advance(interp, p, &toktype));
   S_CHECK(parser_expect(interp, "'(' expected", TOK_LPAREN));

   GmsIndicesData gmsindices;
   gmsindices_init(&gmsindices);
   S_CHECK(parse_gmsindices(interp, p, &gmsindices));

   /* ---------------------------------------------------------------------
    * We have all GAMS indices. Depending on the data, we have different behavior:
    * - If we only have UEL, loopvar, and domain set, then we can just simplify
    *   issue a membership test.
    * - If we have subset or localsets, we need to be more careful.
    * --------------------------------------------------------------------- */

   unsigned gmsfilter_gidx;
   S_CHECK(gmssymiter_init(interp, &ident_gmsarray, &gmsindices, loopiter, tape, &gmsfilter_gidx));

   S_CHECK(loop_initandstart(interp, tape, loopiter));

   /* ---------------------------------------------------------------------
    * Main body of the loop: just perform the test and jump out of the loop
    * if true.
    * --------------------------------------------------------------------- */

   S_CHECK(emit_byte(tape, OP_GMS_MEMBERSHIP_TEST));
   S_CHECK(EMIT_GIDX(tape, gmsfilter_gidx));

   Jump jump = { .depth = jump_depth(c) + 1}; //increase depth just here
   S_CHECK(emit_jump(tape, OP_JUMP_IF_TRUE, &jump.addr));
   S_CHECK(jumps_add_verbose(&interp->compiler->truey_jumps, jump));

   return OK;
}

/**
 * @brief Codeden for 'set1(set2, ...)'
 *
 * This is treated equivalently to ' SUM { set1(set2, ...), yes } '.
 * The name comes from the interpretation of answering the question whether for any
 * tuple (set2, ...), set1(set2, ...) is true.
 *
 * set1 can be a 1D or multiset.
 *
 * @param interp       the EMP interpreter
 * @param p            the position index
 * @param c            the EMP compiler
 * @param tape         the VM tape
 * @param do_emit_not  true if one needs to emit a NOT
 *
 * @return             the error code
 */
static int membership_test(Interpreter * restrict interp, unsigned * restrict p,
                           Compiler *c, Tape *tape, bool do_emit_not)
{
   begin_scope(c, __func__);

   /* Step 1: init iterators and perform membership check */
   LoopIterators loopiter;
   S_CHECK(membership_test_start(interp, p, c, tape, &loopiter));

   /* Step 2:  Increment loop index and check of the condition idx < max */
   tape->linenr = interp->linenr;
   S_CHECK(loop_increment(tape, &loopiter));

   /* Step 3:  End of loop: deal with NOT if present; add the falsey jump */
   /* This isn't very optimized */
   if (do_emit_not) {
      S_CHECK(emit_not_projection(c, tape));
   }

   S_CHECK(end_scope(interp, tape));

   c->gmsfilter.active = false;

   return OK;
}

static int parse_set_in_conditional(Interpreter * restrict interp, unsigned * restrict p,
                                    Compiler *c, Tape *tape, bool do_emit_not)
{
  /* ----------------------------------------------------------------------
   * We are at the position
   *
   *           v 
   *    ident$(condition(set))
   * or                      v
   *    ident$(NOT condition(set))
   * or    v 
   *    i$(i.first)
   * etc
   *
   * GAMS symbols are not read as conditional expression.
   *
   * We need to determine whether we just have a set or set.(first|last)
   * ---------------------------------------------------------------------- */

   TokenType tokpeek;
   unsigned p2 = *p;
   S_CHECK(peek(interp, &p2, &tokpeek));
   int offset = 0;

   /* just set */
   if (tokpeek != TOK_DOT) {
      S_CHECK(membership_test(interp, p, c, tape, do_emit_not));
      return OK;
   }

   S_CHECK(peek(interp, &p2, &tokpeek));
   parser_expects_peek(interp, "In a conditional, an identifier is expected after a dot", TOK_IDENT);

   unsigned toklen = emptok_getstrlen(&interp->peek);
   const char *tokstr = emptok_getstrstart(&interp->peek);

   unsigned idx = UINT_MAX;
   for (unsigned i = 0, len = ARRAY_SIZE(attrs_set); i < len; ++i) {
      if (strncasecmp(tokstr, attrs_set[i], toklen) == 0) {
         idx = i;
         break;
      }
   }

   if (idx == UINT_MAX) {
      error("[empparser] %nERROR line %u: unknown set attribute '%.*s'\n", &offset,
            interp->linenr, toklen, tokstr);
      goto err_unsupported_attr;
   }

   for (unsigned i = 0, len = ARRAY_SIZE(attrs_set_supported); i < len; ++i) {
      if (strncasecmp(tokstr, attrs_set_supported[i], toklen) == 0) {
         idx = i;
         break;
      }
   }

   if (idx == UINT_MAX) {
      error("[empparser] %nERROR line %u: set attribute '%.*s' is not yet supported\n", &offset,
            interp->linenr, toklen, tokstr);
      goto err_unsupported_attr;
   }

   /* Getting an attribute only makes sense for a set under control */
   /* NOTE: the set is still the current token */
   IdentData identdat;
   S_CHECK(resolve_tokasident(interp, &identdat));
   if (identdat.type != IdentLoopIterator) {
      error("[empparser] ERROR line %u: set '%.*s' is not iterated upon (or is not \"under "
            "control\")\n", interp->linenr, toklen, tokstr);
      return Error_EMPIncorrectSyntax;
   }

   *p = p2;

   LocalVar *lvar = &c->locals[identdat.idx];

   if (idx == 0) { // "first"
 
      emit_bytes(tape, OP_PUSH_LIDX, lvar->iteratordat.idx_lidx, OP_PUSH_BYTE, 0, OP_EQUAL);

   } else if (idx == 1) { // "last"

      if (lvar->iteratordat.max_lidx) {
         emit_bytes(tape, OP_PUSH_LIDX, lvar->iteratordat.max_lidx);
      } else {
         emit_bytes(tape, OP_PUSH_VMUINT);
         EMIT_GIDX(tape, lvar->iteratordat.max_gidx);
      }

      emit_bytes(tape, OP_PUSH_LIDX, lvar->iteratordat.idx_lidx, OP_STACKTOP_INC, OP_EQUAL);
   }

   if (do_emit_not) {
      S_CHECK(emit_byte(tape, OP_NOT));
   }

   /* Jump to the loop body if the result is true */
   Jump jump = { .depth = jump_depth(c) };
   S_CHECK(emit_jump(tape, OP_JUMP_IF_TRUE, &jump.addr));
   S_CHECK(jumps_add_verbose(&c->truey_jumps, jump));

   return OK;

err_unsupported_attr:
   error("%*sList of supported set attribute:\n", offset, "");
   for (unsigned i = 0, len = ARRAY_SIZE(attrs_set_supported); i < len; ++i) {
      error("%*s- %s", offset, "", attrs_set_supported[i]);
   }

   return Error_EMPRuntimeError;
}

static int parse_sameas(Interpreter * restrict interp, unsigned * restrict p,
                           Compiler *c, Tape *tape, bool do_emit_not)
{

   TokenType toktype;
   S_CHECK(advance(interp, p, &toktype));
   S_CHECK(parser_expect(interp, "'(' expected", TOK_LPAREN));

   S_CHECK(advance(interp, p, &toktype));

   int uel_idx = -1;
   unsigned lvars[2] = {UINT_MAX, UINT_MAX};
   unsigned *plvar = lvars;
   IdentData ident;

   if (toktype == TOK_SINGLE_QUOTE || toktype == TOK_DOUBLE_QUOTE) {
      char quote = toktype == TOK_SINGLE_QUOTE ? '\'' : '"';
      S_CHECK(parser_asUEL(interp, p, quote, &toktype));
   }

   switch (toktype) {
   case TOK_GMS_UEL:
      uel_idx = interp->cur.symdat.ident.idx;
      break;
   case TOK_GMS_SET:
      if (compmode(interp)) {
         error("[empcompiler] ERROR line %u: unexpected GAMS set '%.*s'\n", interp->linenr,
               tok_fmtargs(&interp->cur));
         return Error_EMPRuntimeError;
      } 
      lvars[0] = ident.idx;
      plvar = &lvars[1];
      break;
   case TOK_IDENT:
      S_CHECK(resolve_identas(interp, &ident, "a iterated GAMS set (or \"under control\")",
                              IdentLoopIterator));
      lvars[0] = ident.idx;
      plvar = &lvars[1];
      break;
   default:
      error("%s :: unexpected failure.\n", __func__);
      return Error_RuntimeError;
   }

   S_CHECK(advance(interp, p, &toktype));

   if (toktype != TOK_COMMA) {
      error("[empcompiler] ERROR on line %u: 2 arguments are required for GAMS logical operator 'sameas'. "
            "While looking for the ',', got '%.*s' of type '%s'.\n", interp->linenr, emptok_getstrlen(&interp->cur),
             emptok_getstrstart(&interp->cur), toktype2str(toktype));
      return Error_EMPIncorrectSyntax;
   }

   S_CHECK(advance(interp, p, &toktype));
   PARSER_EXPECTS(interp, "a GAMS set or UEL", TOK_IDENT, TOK_SINGLE_QUOTE, TOK_DOUBLE_QUOTE);

   if (uel_idx >= 0 && toktype != TOK_IDENT) {
      error("[empcompiler] ERROR on line %u: GAMS logical operator 'sameAs' can only have 1 UEL as argument.\n", interp->linenr);
      return Error_EMPIncorrectSyntax;
   }

   if (toktype == TOK_SINGLE_QUOTE || toktype == TOK_DOUBLE_QUOTE) {
      char quote = toktype == TOK_SINGLE_QUOTE ? '\'' : '"';
      S_CHECK(parser_asUEL(interp, p, quote, &toktype));
   }

   switch (toktype) {
   case TOK_GMS_UEL:
      uel_idx = interp->cur.symdat.ident.idx;
      break;
   case TOK_GMS_SET:
      if (compmode(interp)) {
         error("[empcompiler] ERROR line %u: unexpected GAMS set '%.*s'\n", interp->linenr,
               tok_fmtargs(&interp->cur));
         return Error_EMPRuntimeError;
      } 
      uel_idx = interp->cur.symdat.ident.idx;
      break;
   case TOK_IDENT:
      resolve_identas(interp, &ident, "a GAMS parameter is expected", IdentLoopIterator);
      *plvar = ident.idx;
      break;
   default:
      error("%s :: unexpected failure.\n", __func__);
      return Error_RuntimeError;
   }

   S_CHECK(advance(interp, p, &toktype));

   /* ---------------------------------------------------------------------
    * Task here: compare the value stored in 2 local variable.
    * - If a UEL was given, its value is stored in a local variable that we now create
    * - If we have 2 loop iterators, we just read the values of local variables
    * --------------------------------------------------------------------- */

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

   Jump jump = { .depth = jump_depth(c) + 1 };
   S_CHECK(emit_jump(tape, OP_JUMP_IF_TRUE, &jump.addr));
   S_CHECK(jumps_add_verbose(&c->truey_jumps, jump));

   /* Resolve all remaining jumps linked to a false condition */
   S_CHECK(patch_jumps(&c->falsey_jumps, tape, jump_depth(c)));

 
   return OK;
}

/**
 * @brief Parse the iterators of a loop/sum statement
 *
 * This function is used to parse the first argument to a loop or sum keyword.
 *
 * @param interp          the interpreter
 * @param p               the position pointer
 * @param c               the compiler
 * @param[out] iterators  the iterators
 *
 * @return                the error code
 */
static int parse_loopiters_operator(Interpreter * restrict interp, unsigned * restrict p,
                                    Compiler *c, LoopIterators *iterators)
{
   interp->state.read_gms_symbol = false;
   /* We get the set/sets to iterate over */
   TokenType toktype;
   S_CHECK(advance(interp, p, &toktype))
   PARSER_EXPECTS(interp, "a single GAMS set or a collection", TOK_IDENT, TOK_LPAREN, TOK_GMS_SET);

   GmsIndicesData indices = {0};

   if (toktype == TOK_LPAREN) {
      S_CHECK(parse_loopsets(interp, p, &indices));
   } else {

      IdentData *ident = &indices.idents[0];
      resolve_identas(interp, ident, "GAMS index must fulfill these conditions.",
                      IdentLocalSet, IdentSet);

      switch (ident->type) {
      case IdentLocalSet:
         indices.num_localsets++;
         break;
      case IdentSet:
         indices.num_sets++;
         break;
      default:
         return runtime_error(interp->linenr);
      }

      indices.nargs = 1;
   }

   /* ---------------------------------------------------------------------
    * Loop initialization:
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

   Tape _tape = {.code = &c->vm->code, .linenr = UINT_MAX};
   Tape * restrict const tape = &_tape;
   tape->linenr = interp->linenr;
 
   iterators_init_from_gmsindices(iterators, &indices);
   S_CHECK(loop_initandstart(interp, tape, iterators));

   /* ---------------------------------------------------------------------
    * Step 2:  Parse the conditional (if present)
    * --------------------------------------------------------------------- */

   S_CHECK(advance(interp, p, &toktype))
   PARSER_EXPECTS(interp, "a conditional '$' or comma ','", TOK_CONDITION, TOK_COMMA);

   tape->linenr = interp->linenr;

   if (toktype == TOK_CONDITION) {

      S_CHECK(parse_condition(interp, p, c, tape));

      /* Check for the comma */
      S_CHECK(advance(interp, p, &toktype))
      S_CHECK(parser_expect(interp, "a ',' after conditional", TOK_COMMA));

      tape->linenr = interp->linenr;

   }

   interp->state.read_gms_symbol = true;
   return OK;
}

/**
 * @brief Parse the 1st argument of the sum (classic or projection) operator
 *
 * @param      interp         the interpreter
 * @param      p              the position pointer
 * @param      c              the VM compiler
 * @param      tape           the VM tape
 * @param[out] sum_iterators  the iterators in sum first argument
 * @param[out] need_fwd_jump  on output true, if the callee need to inject a forward jump
 *
 * @return                    the error code 
 */
static int sumtype_parse_arg1(Interpreter * restrict interp, unsigned * restrict p,
                              Compiler *c, Tape * tape, LoopIterators *sum_iterators,
                              bool *need_fwd_jump)
{
   TokenType toktype;
   unsigned p2 = *p;
   S_CHECK(peek(interp, &p2, &toktype));
   unsigned p_ = p2;
   TokenType toktype_;
   S_CHECK(peek(interp, &p_, &toktype_));
   if ((toktype == TOK_GMS_SET || toktype == TOK_IDENT) && toktype_ == TOK_LPAREN) {

      /* ---------------------------------------------------------------------
       * Step 1bis: Case ' sum{ multiset(set1, ..., setN), ... } '
       * This is equivalent to sum{ (setj, ...)$(multiset(set1, ..., setN)), ...}
       * --------------------------------------------------------------------- */

      /* New to rewind and restart as we had 2 peek calls */
      S_CHECK(advance(interp, p, &toktype));

      IdentData multiset;
      S_CHECK(resolve_tokasident(interp, &multiset));

      if (multiset.type != IdentMultiSet && multiset.type != IdentSet) {
         error("[empinterp] ERROR on line %u: only GAMS sets are supported as first "
               "argument to the projection operator 'sum'; got type '%s' for lexeme "
               "'%.*s'\n", interp->linenr, ident_fmtargs(&multiset));
         return Error_EMPIncorrectSyntax;
      }

      S_CHECK(membership_test_start(interp, p, c, tape, sum_iterators));

      /* membership_test_start() does not parse the ','; loop_initandstart() does */
      S_CHECK(advance(interp, p, &toktype));
      S_CHECK(parser_expect(interp, "a comma ',' separating the 'sum' arguments",
                            TOK_COMMA));

      *need_fwd_jump = false;

   } else { /* regular case 'sum{ (set1, ...), ... }' */

      S_CHECK(parse_loopiters_operator(interp, p, c, sum_iterators));

      *need_fwd_jump = true;

   }

   return OK;
}

/* ------------------------------------------------------------------------
 * Design notes for projection implementation.
 *
 * The high-level syntax is SUM { arg1, arg2 }. The generated pseudo-code is
 *
 * 1. init arg1 iterators (arg1_iter)
 * 2. if (arg1.condition and arg1.condition(arg1_iter)) then jump @5 (ARG2_START) end;
 * 3. while(!arg1.iterend(arg1_iter)) { update(arg1_iter); jump @2. }
 * 4. goto END_PROJECTION
 *
 * 5. init arg2 iterations (arg2_iter)
 * 6.  if (!arg2.condition or arg2.condition(arg2_iter)) then goto PROJECTION_TRUE end;
 * 7. while(!arg2.iterend(arg2_iter)) { update(arg2_iter); jump @6. }
 * 8. goto 3. (ARG1_UPDATE)
 *
 * 9. goto (PROJECTION_FALSE)
 * 
 * PROJECTION_TRUE: do xyz
 *
 * PROJECTION_FALSE: continue
 *
 *
 * When negating the projection, there is an added inversion of the result that
 * happens at PROJECTION_TRUE and PROJECTION_FALSE to negate them.
 *
 * Salient point: There needs to be an indicator whether arg1.condition exists . In that
 * case more codegen needs to happen to not continue to 5 in that case
 * ------------------------------------------------------------------------ */
static int parse_projection_in_conditional(Interpreter * restrict interp, unsigned * restrict p,
                                           Compiler * restrict c, Tape * restrict tape,
                                           bool do_emit_not)
{
  /* ----------------------------------------------------------------------
   * We are at the position
   *
   *                 v 
   *    ident(set)$( sum(condition(set), yes) )
   * or              v
   *    ident(set)$( sum(condition(set,set2), yes) )
   * or              v
   *    ident(set)$( sum(set2, condition(set,set2)) )
   * etc
   *
   * We disable reading GAMS symbols as we are constructing a condition expression
   *
   * ---------------------------------------------------------------------- */

   TokenType toktype;
   S_CHECK(advance(interp, p, &toktype));

   if (!tok_isopeningdelimiter(toktype)) { 
      error("[empparser] ERROR on line %u: After the projection operator 'sum', expecting"
            "a delimiter '(', '{', or '['. Got '%.*s'\n", interp->linenr,
            tok_fmtargs(&interp->cur));
      return Error_EMPIncorrectSyntax;
   }

   TokenType closing_delimiter = tok_closingdelimiter(toktype); /* Cheap way to init */


   begin_scope(c, __func__);

   /* ---------------------------------------------------------------------
    * Step 1: Parse sum iterators.
    * Necessary distinction between sum { (set1, ...), ...} and
    * sum { multiset(set1, ...), ...}
    * --------------------------------------------------------------------- */
   LoopIterators sum_iterators;

   /* If arg1 has no test, jsut sets up iterator, need to add to the start of arg2 */
   bool need_truey_jump;
   S_CHECK(sumtype_parse_arg1(interp, p, c, tape, &sum_iterators, &need_truey_jump));

   if (need_truey_jump) {
      Jump jump = {.depth = jump_depth(c)};
      S_CHECK(emit_jump(tape, OP_JUMP, &jump.addr));
      S_CHECK(jumps_add_verbose(&c->truey_jumps, jump));
   }

   /* Resolve the falsey jump to the iterators increments */
   S_CHECK(patch_jumps(&c->falsey_jumps, tape, jump_depth(c)));

   unsigned tapepos_sum_iterators_inc = tape->code->len;
   S_CHECK(loop_increment(tape, &sum_iterators));

   Jump jump_end_arg1 = {.depth = jump_depth(c) };
   S_CHECK(emit_jump(tape, OP_JUMP, &jump_end_arg1.addr));

   /* ---------------------------------------------------------------------
    * Step 2: Deal with the second argument of the projection operator
    * ---------------------------------------------------------------------- */

   /* Patch TRUEY jumps from arg1 */
   S_CHECK(patch_jumps(&c->truey_jumps, tape, jump_depth(c)));

   S_CHECK(advance(interp, p, &toktype));

   if (toktype == TOK_YES) { /* only action: emit jump instruction */

      unsigned p2 = *p;
      S_CHECK(peek(interp, &p2, &toktype));

      /* Case: sum { ..., yes$(...) } */
      if (toktype == TOK_CONDITION) {
         interp_cpypeek2cur(interp);
         *p = p2;
         S_CHECK(parse_condition(interp, p, c, tape));
      }

      Jump jump = {.depth = jump_depth(c)};
      S_CHECK(emit_jump(tape, OP_JUMP, &jump.addr));
      S_CHECK(jumps_add_verbose(&c->truey_jumps, jump));

   } else {

      PARSER_EXPECTS(interp, "In the projection operator 'sum', only GAMS sets are expected",
                     TOK_GMS_MULTISET, TOK_GMS_SET, TOK_IDENT);

      IdentData ident_arg2;
      S_CHECK(resolve_identas(interp, &ident_arg2, "In a conditional, a GAMS set is expected",
                              IdentLocalSet, IdentSet, IdentMultiSet));

      trace_empparser("[empcompiler] membership test with type '%s' and dimension %u\n",
                      identtype2str(ident_arg2.type), ident_arg2.dim);

      GmsIndicesData gmsindices;
      gmsindices_init(&gmsindices);

      S_CHECK(advance(interp, p, &toktype));
      S_CHECK(parser_expect(interp, "'(' expected", TOK_LPAREN));

      S_CHECK(parse_gmsindices(interp, p, &gmsindices));

      if (!embmode(interp) && gmsindices_nvardims(&gmsindices) > 0) {

         u8 ndims = gmsindices_len(&gmsindices);
         u8 nvardims = gmsindices_nvardims(&gmsindices);

         error("[empinterp] ERROR line %u: in the projection operator 'sum', the second argument "
               "must have no uncontrolled dimensions\n", interp->linenr);
         error("    Here, we have %u issue%s\n:", nvardims, nvardims > 1 ? "s" : "");

         for (u8 i = 0; i < ndims; ++i) {
            IdentData *ident = &gmsindices.idents[i];
            if (ident->type == IdentSet || ident->type == IdentLocalSet) {
               error("    - position %2u: %.*s\n", i, lexeme_fmtargs(ident->lexeme)); 
            }
         }

         return Error_EMPIncorrectInput;
      }

      unsigned gmsfilter_gidx;
      LoopIterators arg2iterators;
      S_CHECK(gmssymiter_init(interp, &ident_arg2, &gmsindices, &arg2iterators, tape, &gmsfilter_gidx));

      S_CHECK(emit_byte(tape, OP_GMS_MEMBERSHIP_TEST));
      S_CHECK(EMIT_GIDX(tape, gmsfilter_gidx));

      Jump jump = {.depth = jump_depth(c)};
      S_CHECK(emit_jump(tape, OP_JUMP_IF_TRUE, &jump.addr));
      S_CHECK(jumps_add_verbose(&c->truey_jumps, jump));

      if (!embmode(interp)) { //HACK
         S_CHECK(loop_increment(tape, &arg2iterators));
      }
   }

   /* arg2 did not evaluate to true => jump back to the sum_iterators update */
   S_CHECK(emit_jump_back(tape, tapepos_sum_iterators_inc));

   /* This is the end of arg1 */
   S_CHECK(patch_jump(tape, jump_end_arg1.addr));

   /* ---------------------------------------------------------------------
    * Part 3: end of projection. Deal with 'NOT sum{...}' case
    * ---------------------------------------------------------------------- */
   if (do_emit_not) {
      S_CHECK(emit_not_projection(c, tape));
   }

   Jump jump = {.depth = jump_depth(c)};
   S_CHECK(emit_jump(tape, OP_JUMP, &jump.addr));
   S_CHECK(jumps_add_verbose(&c->falsey_jumps, jump));

   S_CHECK(end_scope(interp, tape));

   /* Get the closing delimiter */
   S_CHECK(advance(interp, p, &toktype));

   if (closing_delimiter != TOK_UNSET && toktype != closing_delimiter) {
      return delimiter_error(interp, closing_delimiter, "projection (sum)");
   }

   return OK;
}

static int parse_conditional(Interpreter * restrict interp, unsigned * restrict p,
                              Compiler * restrict c, Tape * restrict tape, bool do_emit_not)
{

   TokenType toktype = parser_getcurtoktype(interp);
   TokenType closing_delimiter = tok_closingdelimiter(toktype); /* Cheap way to init */

  /* ----------------------------------------------------------------------
   * We are at the position
   *
   *          v 
   *    ident$(condition(set))
   * or                     v
   *    ident$(NOT condition(set))
   * etc
   *
   * We disable reading GAMS symbols as we are constructing a condition expression
   *
   * ---------------------------------------------------------------------- */

   if (tok_isopeningdelimiter(toktype)) { 
      assert(closing_delimiter != TOK_UNSET);
      S_CHECK(advance(interp, p, &toktype));
      PARSER_EXPECTS(interp, "a GAMS set or NOT or sameAs or sum or '(', '{', '['",
                     TOK_IDENT, TOK_NOT, TOK_SAMEAS, TOK_SUM, TOK_GMS_SET,
                     TOK_GMS_MULTISET, TOK_LPAREN, TOK_LBRACE, TOK_LBRACK);
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

      if (tok_isopeningdelimiter(toktype)) {
         S_CHECK(jump_depth_inc(c));
         S_CHECK(parse_conditional(interp, p, c, tape, do_emit_not));
         S_CHECK(jump_depth_dec(c));

         if (do_emit_not) {
            S_CHECK(emit_byte(tape, OP_NOT));
            do_emit_not = false;
         }

      } else {

         if (toktype == TOK_NOT) {

            do_emit_not = true;
            S_CHECK(advance(interp, p, &toktype));
            PARSER_EXPECTS(interp, "a GAMS set or logical operator",
                           TOK_IDENT, TOK_SAMEAS, TOK_SUM, TOK_GMS_SET); //HACK GMSSYMB
            //
         }

         switch(toktype) {

         case TOK_SAMEAS:
            S_CHECK(parse_sameas(interp, p, c, tape, do_emit_not));
            break;
         case TOK_SUM:
            S_CHECK(parse_projection_in_conditional(interp, p, c, tape, do_emit_not))
            break;
         case TOK_IDENT:
         case TOK_GMS_SET:
            /* Now we expect a GAMS set to perform the membership test on*/
            S_CHECK(parse_set_in_conditional(interp, p, c, tape, do_emit_not));
            break;
         default:
            error("[empparser] ERROR: unexpected token type %s\n", toktype2str(toktype));
            return Error_EMPIncorrectSyntax;

         }

         do_emit_not = false;
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

         Jump jump = {.depth = jump_depth(c)};
         S_CHECK(emit_jump(tape, OP_JUMP, &jump.addr));
         S_CHECK(jumps_add_verbose(&c->falsey_jumps, jump));

         S_CHECK(patch_jumps(&c->truey_jumps, tape, jump_depth(c)));

      } else if (toktype == TOK_OR) {

         /* FIXME this is most likely unnecessary and will induce bugs */
         //Jump jump = {.depth = jump_depth(c)};
         //S_CHECK(emit_jump(tape, OP_JUMP_IF_TRUE, &jump.addr));
         //S_CHECK(jumps_add_verbose(&c->truey_jumps, jump));

         S_CHECK(patch_jumps(&c->falsey_jumps, tape, jump_depth(c)));

      } else {
         break;
      }

      S_CHECK(advance(interp, p, &toktype));

   } while (true);

   if (closing_delimiter != TOK_UNSET && toktype != closing_delimiter) {
      return delimiter_error(interp, closing_delimiter, "conditional");
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

   c->state.scope_depth = 0;
   c->state.jump_depth = 0;
   c->state.vmstack_depth = 0;
   c->state.vmstack_max = 0;

   ovfdecl_empty(&c->ovfdecl);

   c->regentry_gidx = UINT_MAX;

   c->gmsfilter.active = false;

   jumps_init(&c->falsey_jumps);
   jumps_init(&c->truey_jumps);

   AA_CHECK(c->vm, empvm_new(interp));

   interp->compiler = c;

   return c;
}

void empvm_compiler_free(Compiler* c)
{
   if (!c) return;

   empvm_free(c->vm);

   jumps_free(&c->falsey_jumps);
   jumps_free(&c->truey_jumps);

   free(c);
}



/**
 * @brief Parse a condition like $(inset(sets))
 *
 * @warning the loop must already have been initialized!
 *
 * @param interp  the interpreter
 * @param p       the position pointer
 * @param c       the VM compiler
 * @param tape    the VM tape
 *
 * @return         the error code
 */
NONNULL static 
int parse_condition(Interpreter * restrict interp, unsigned * restrict p,
                    Compiler * restrict c, Tape * restrict tape)
{
   assert(parser_getcurtoktype(interp) == TOK_CONDITION);

  /* ----------------------------------------------------------------------
   * We are at the position
   *
   *         v
   *    ident$(condition(set))
   * or      v
   *    ident$(NOT condition(set))
   * etc
   *
   * We disable reading GAMS symbols as we are constructing a condition expression
   * ---------------------------------------------------------------------- */

   S_CHECK(jump_depth_inc(c));

   trace_empparser("[empcompiler] line %u: parsing condition at depth %u\n",
                   interp->linenr, jump_depth(c));

   interp->state.read_gms_symbol = false;

   TokenType toktype;
   S_CHECK(advance(interp, p, &toktype));

   bool do_emit_not = false;
   if (toktype == TOK_NOT) {
      do_emit_not = true;
      S_CHECK(advance(interp, p, &toktype));
   }

   /* Not great to make this distinction here. Can't find something nicer */
   if (tok_isopeningdelimiter(toktype)) {
      S_CHECK(parse_conditional(interp, p, c, tape, do_emit_not));
   } else if (toktype == TOK_SUM) {
      S_CHECK(parse_projection_in_conditional(interp, p, c, tape, do_emit_not))
   } else {
      PARSER_EXPECTS(interp, "a GAMS set", TOK_IDENT, TOK_GMS_SET);
      S_CHECK(parse_set_in_conditional(interp, p, c, tape, do_emit_not));
   }

   /* Re-enable reading GAMS symbols */
   interp->state.read_gms_symbol = true;

   trace_empparser("[empcompiler] line %u: condition at depth %u parsed\n", interp->linenr,
                   jump_depth(c));

   S_CHECK(jump_depth_dec(c));

   /* ---------------------------------------------------------------------
    * Convention: unconditionally jump to the loop iterator increase.
    * This implies that the jump if TRUE must be generated inside
    * ---------------------------------------------------------------------- */
   Jump jump = {.depth = jump_depth(c)};
   S_CHECK(emit_jump(tape, OP_JUMP, &jump.addr));
   S_CHECK(jumps_add_verbose(&c->falsey_jumps, jump));

   /* Resolve all remaining jumps linked to a true condition */
   // FIXME
   S_CHECK(patch_jumps(&c->truey_jumps, tape, jump_depth(c)));

   return OK;
}

/**
 * @brief TODO
 *

 *
 * @param interp      the interpreter
 * @param p           the position pointer
 * @param label       the EMPDAG label name
 * @param label_len   the label length
 * @param link_type   the link type
 * @param gmsindices  the indices
 *
 * @return            the error code
 */
static int vm_gmsindicesasarc(Interpreter *interp, unsigned *p, const char *label,
                              unsigned label_len, LinkType link_type,
                              GmsIndicesData *gmsindices)
{
   Compiler *c = interp->compiler;
   EmpVm *vm = c->vm;
   Tape tape_ = {.code = &vm->code, .linenr = interp->linenr};
   Tape * const tape = &tape_;

   /*
    *  We have parsed an expression of the type:
    *  - max obj + MP('name', n(set).valfn$(cond(set)), ...)  ...
    *  - max obj x nLower ...
    *  - Nash(n(set)$(cond(set)))
    *
    */

   /* This defines the loop iterators and the linklabels object */
   LoopIterators loopiters = {.niters = 0};
   unsigned linklabels_gidx;
   S_CHECK(linklabels_init(interp, tape, label, label_len, link_type,
                           gmsindices, &loopiters, &linklabels_gidx));

   assert(vmval_is_arcobj(&vm->globals, linklabels_gidx) == OK);

   /* ---------------------------------------------------------------------
    * If there is no (local) set, then we can just duplicate the value
    * that is in the global index. There must be some iterator values that
    * were updated in labelargs_initloop().
    * 1) Copy the object and set the gidx value on the stack  
    * 2) Set the arguments as index for the  
    * And we are done
    * --------------------------------------------------------------------- */

   uint8_t niters = gmsindices_nvardims(gmsindices);
   if (niters == 0) {
      //assert(gmsindices->num_loopiterators > 0);
      S_CHECK(emit_bytes(tape, OP_LINKLABELS_DUP));
      S_CHECK(EMIT_GIDX(tape, linklabels_gidx));

      return OK;
   }

   S_CHECK(emit_bytes(tape, OP_LINKLABELS_INIT));
   S_CHECK(EMIT_GIDX(tape, linklabels_gidx));


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
      interp_cpypeek2cur(interp);
      S_CHECK(parse_condition(interp, p, c, tape));
   }

   /* ---------------------------------------------------------------------
    * Step : Main body: we add the tuple (iter1, iter2, iter3)
    * This will duplicate the DagLabels and put it on the stack.
    * --------------------------------------------------------------------- */

   assert(niters <= AS_ARCOBJ(vm->globals.arr[linklabels_gidx])->nvardims);
   S_CHECK(emit_bytes(tape, OP_LINKLABELS_STORE, niters));

   for (unsigned i = 0; i < niters; ++i) {
      S_CHECK(emit_byte(tape, loopiters.iters[i].iter_lidx));
   }

   /* ---------------------------------------------------------------------
    * Step 4:  Increment loop index and check of the condition idx < max
    * --------------------------------------------------------------------- */

   /* Resolve all remaining jumps linked to a false condition */
   S_CHECK(patch_jumps(&c->falsey_jumps, tape, jump_depth(c)));

   S_CHECK(loop_increment(tape, &loopiters));

   /* ---------------------------------------------------------------------
    * We are at the end of the loop. We allow for outstanding "true" jump
    * to come here.
    * --------------------------------------------------------------------- */

   S_CHECK(patch_jumps(&c->truey_jumps, tape, jump_depth(c)));

   assert(no_outstanding_jump(&c->truey_jumps, jump_depth(c)));
   assert(no_outstanding_jump(&c->falsey_jumps, jump_depth(c)));

   S_CHECK(emit_bytes(tape, OP_LINKLABELS_FINI));

   gmsindices_deactivate(gmsindices);

   return end_scope(interp, tape);
}

int dualslabels_setupnew(Interpreter *interp, Tape *tape, const char *label,
                         unsigned label_len, GmsIndicesData *gmsindices,
                         DualOperatorData* opdat, LoopIterators *loopiterators, 
                         unsigned *dualslabel_gidx)
{
   DualsLabel *dualslabel;
   uint8_t ndims = gmsindices->nargs;
   uint8_t nvardims = gmsindices_nvardims(gmsindices);
   A_CHECK(dualslabel, dualslabel_new(label, label_len, ndims, nvardims, opdat));

   S_CHECK(dualslabel_arr_add(&interp->dualslabels, dualslabel));

   unsigned gidx = interp->dualslabels.len-1;
   *dualslabel_gidx = gidx;

   /* Add new OP_DUALSLABEL_ADD */
   // TODO: where do we add mpid_dual?
//   S_CHECK(emit_byte(tape, OP_DUALSLABEL_ADD));
//   S_CHECK(EMIT_GIDX(tape, gidx));

   if (gmsindices->nargs == 0) {
      loopiterators->niters = 0;
      loopiterators->loopobj_gidx = UINT_MAX;
      return OK;
   }

   loopiterators->loopobj_gidx = *dualslabel_gidx;
   loopiterators->loopobj_opcode = OP_MAXCODE;

   bool dummy;
   S_CHECK(gmsindices_process(gmsindices, tape, loopiterators, dualslabel->data, &dummy));

  /* ----------------------------------------------------------------------
   * Copy the coordinate (of the index) where the loop iterators belong,
   * but can't use memcpy as we don't have the same type. Example:
   *
   * n('1', i, '2', j) -> varidxs2pos = [1, 3]
   * ---------------------------------------------------------------------- */

   int *dst = &dualslabel->data[ndims];
   for (u8 i = 0; i < nvardims; ++i) {
      *dst++ = loopiterators->varidxs2pos[i];
   }


   return OK;

}

int vm_add_dualslabel(Interpreter *interp, const char *label, unsigned label_len,
                             GmsIndicesData *gmsindices, DualOperatorData *opdat)
{
   Compiler *c = interp->compiler;
   EmpVm *vm = c->vm;
   Tape tape_ = {.code = &vm->code, .linenr = interp->linenr};
   Tape * const tape = &tape_;

   /*
    *  We have parsed an expression of the type:
    *  - max obj + VF('name', n.valfn(set)$(cond(set)), ...)  ...
    *  - max obj x nLower ...
    *  - Nash(n(set)$(cond(set)))
    *
    */

   /* This defines the loop iterators and the linklabels object */
   LoopIterators loopiters = {.niters = 0};
   unsigned dualslabel_gidx;
   S_CHECK(dualslabels_setupnew(interp, tape, label, label_len, gmsindices,
                                opdat, &loopiters, &dualslabel_gidx));

   /* ---------------------------------------------------------------------
    * If there is no (local) set, then we can just duplicate the value
    * that is in the global index. There must be some iterator values that
    * were updated in labelargs_initloop().
    * 1) Copy the object and set the gidx value on the stack  
    * 2) Set the arguments as index for the  
    * And we are done
    * --------------------------------------------------------------------- */

   uint8_t num_iterators_innerloop = gmsindices->num_localsets + gmsindices->num_sets;
   if (num_iterators_innerloop == 0) {
      assert(gmsindices->num_loopiterators > 0);
      S_CHECK(emit_bytes(tape, OP_DUALSLABEL_STORE));
      S_CHECK(EMIT_GIDX(tape, dualslabel_gidx));

      uint8_t num_loopiterators = gmsindices->num_loopiterators;

      S_CHECK(emit_byte(tape, num_loopiterators));

      for (unsigned i = 0; i < num_loopiterators; ++i) {
         S_CHECK(emit_byte(tape, loopiters.idents[i].idx));
      }

      return OK;
   }

   uint8_t num_iterators = num_iterators_innerloop + gmsindices->num_loopiterators;


   // HACK: force only 1 read (vector of dual() don't make sense)
   /* ---------------------------------------------------------------------
    * Start the loop. For each set, we must declare an iterator and loop over
    * the possible values. 
    * --------------------------------------------------------------------- */

   begin_scope(c, __func__);
   S_CHECK(loop_initandstart(interp, tape, &loopiters));

   /* ---------------------------------------------------------------------
    * HACK: do we need condition parsing?
    * If present, Parse the condition
    * --------------------------------------------------------------------- */

#ifdef HACK
   unsigned p2 = *p;
   TokenType toktype;
   S_CHECK(peek(interp, &p2, &toktype));

   if (toktype == TOK_CONDITION) {
      *p = p2;
      S_CHECK(parse_condition(interp, p, c, tape));
   }
#endif
   /* ---------------------------------------------------------------------
    * Step : Main body: we add the tuple (iter1, iter2, iter3)
    * This will duplicate the DagLabels and put it on the stack.
    * --------------------------------------------------------------------- */

   S_CHECK(emit_bytes(tape, OP_DUALSLABEL_STORE));
   S_CHECK(EMIT_GIDX(tape, dualslabel_gidx));
   S_CHECK(emit_bytes(tape, num_iterators));

   for (unsigned i = 0; i < num_iterators; ++i) {
      S_CHECK(emit_byte(tape, loopiters.iters[i].iter_lidx));
   }

   /* ---------------------------------------------------------------------
    * Step 4:  Increment loop index and check of the condition idx < max
    * --------------------------------------------------------------------- */

   /* Resolve all remaining jumps linked to a false condition */
   S_CHECK(patch_jumps(&c->falsey_jumps, tape, jump_depth(c)));

   S_CHECK(loop_increment(tape, &loopiters));

   /* ---------------------------------------------------------------------
    * We are at the end of the loop. We allow for outstanding "true" jump
    * to come here.
    * --------------------------------------------------------------------- */

   S_CHECK(patch_jumps(&c->truey_jumps, tape, jump_depth(c)));

   assert(no_outstanding_jump(&c->truey_jumps, jump_depth(c)));
   assert(no_outstanding_jump(&c->falsey_jumps, jump_depth(c)));

   return end_scope(interp, tape);
}

int vm_add_Varc_dual(Interpreter * interp, UNUSED unsigned *p)
{
   Compiler *c = interp->compiler;
   EmpVm *vm = c->vm;
   Tape tape_ = {.code = &vm->code, .linenr = interp->linenr};
   Tape * const tape = &tape_;

   return emit_byte(tape, OP_VARC_DUAL);
}

// This is most likely used from the embmode
int vm_parse_condition(Interpreter * restrict interp, unsigned * restrict p)
{
   assert(parser_getcurtoktype(interp) == TOK_CONDITION);

   Compiler *c = interp->compiler;
   EmpVm *vm = c->vm;
   Tape tape_ = {.code = &vm->code, .linenr = interp->linenr};
   Tape * const tape = &tape_;

   return parse_condition(interp, p, c, tape);
}

int vm_condition_fini(Interpreter * restrict interp)
{
   Compiler *c = interp->compiler;
   EmpVm *vm = c->vm;
   Tape tape_ = {.code = &vm->code, .linenr = interp->linenr};
   Tape * const tape = &tape_;

   return patch_jumps(&c->falsey_jumps, tape, jump_depth(c));
}

NONNULL static
int c_identaslabels(Interpreter * restrict interp, unsigned * restrict p,
                    LinkType linktype)
{
   const char* label = emptok_getstrstart(&interp->cur);
   unsigned label_len = emptok_getstrlen(&interp->cur);

   TokenType toktype;
   S_CHECK(advance(interp, p, &toktype))

   GmsIndicesData indices;
   gmsindices_init(&indices);

   if (toktype == TOK_LPAREN) {
      S_CHECK(parse_gmsindices(interp, p, &indices));
   }


   S_CHECK(vm_gmsindicesasarc(interp, p, label, label_len, linktype, &indices));
   assert(interp->cur.type == TOK_RPAREN);

   return advance(interp, p, &toktype);
}

/**
 * @brief Function to parse loop sets
 *
 * It is similar as parse_gmsindices(), but only IdentSet and IdentLocalSet
 * are allowed. 
 *
 * @param interp   the interpreter
 * @param p        the position pointer
 * @param indices  the symbol indices
 *
 * @return         the error code
 */
static int parse_loopsets(Interpreter * restrict interp, unsigned * restrict p,
                          GmsIndicesData * restrict indices)
{
   assert(emptok_gettype(&interp->cur) == TOK_LPAREN);

   /* ---------------------------------------------------------------------
    * We are in a position like
    *      v
    * sum((set1, set2), ...)
    * ---------------------------------------------------------------------- */
   TokenType toktype;
   unsigned nargs = 0;

   do {
      if (nargs == GMS_MAX_INDEX_DIM) {
         error("[empinterp] ERROR on line %u: while parsing the sets to loop over, "
               "more than %u were parsed.\n", interp->linenr, GMS_MAX_INDEX_DIM);
         return Error_EMPIncorrectSyntax;
      }

      /* get the next token */
      S_CHECK(advance(interp, p, &toktype));
      PARSER_EXPECTS(interp, "Sets to loop over must be identifiers", TOK_IDENT, TOK_GMS_SET);

      IdentData *data = &indices->idents[nargs];

      if (toktype == TOK_GMS_SET) {
         S_CHECK(tok2ident(&interp->cur, data));
      } else {
         resolve_identas(interp, data, "Loop indices must fulfill these conditions.",
                         IdentLocalSet, IdentSet);
      }

      switch (indices->idents[nargs].type) {
      case IdentLocalSet:
         indices->num_localsets++;
         break;
      case IdentSet:
         indices->num_sets++;
         break;
      default:
         return runtime_error(interp->linenr);
      }

      nargs++;

      S_CHECK(advance(interp, p, &toktype));

   } while (toktype == TOK_COMMA);

   indices->nargs = nargs;

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
  /* ----------------------------------------------------------------------
   * loop statement are of the form: 
   *
   * loop(loopiters, statements)
   * 
   * The first argument "loopiters" can be either a set "i" or a collection "(i,j)"
   * ---------------------------------------------------------------------- */

   /* If we are already parsing, just get a pointer to the compiler. Otherwise
    * initialize the compiler and set the parser_ops to the VM ones */
   Compiler *c = ensure_vm_mode(interp);
   EmpVm *vm = c->vm;
   Tape _tape = {.code = &vm->code, .linenr = UINT_MAX};
   Tape * const tape = &_tape;

   begin_scope(c, __func__);

   trace_empparser("[empcompiler] Loop @%u is starting\n", jump_depth(c));

   /* Consume the opening delimiter (parent / bracket / ...) */
   TokenType toktype;
   S_CHECK(advance(interp, p, &toktype))

   PARSER_EXPECTS(interp, "A delimiter '(' or '{' or '[')", TOK_LPAREN, TOK_LBRACE, TOK_LBRACK);

   TokenType closing_delimiter = tok_closingdelimiter(toktype);

   /* ---------------------------------------------------------------------
    * Step 1: Parse loop iterators
    * --------------------------------------------------------------------- */
   LoopIterators iterators;
   S_CHECK(parse_loopiters_operator(interp, p, c, &iterators));

   /* ---------------------------------------------------------------------
    * Step 2:  Parse the body of the loop
    * --------------------------------------------------------------------- */
   S_CHECK(advance(interp, p, &toktype))

   while (toktype != TOK_EOF && toktype != closing_delimiter) {
      S_CHECK(process_statement(interp, p, toktype));
      toktype = parser_getcurtoktype(interp);
   }

   if (toktype == TOK_EOF) return parser_err(interp, "while parsing the loop body");

   /* ---------------------------------------------------------------------
    * Step 3:  Increment loop index and check of the condition idx < max
    * --------------------------------------------------------------------- */

   tape->linenr = interp->linenr;

   /* Resolve all remaining jumps linked to a false condition */
   S_CHECK(patch_jumps(&c->falsey_jumps, tape, jump_depth(c)));

   S_CHECK(loop_increment(tape, &iterators));

   trace_empparser("[empcompiler] Loop @%u is complete\n", jump_depth(c));

   /* ---------------------------------------------------------------------
    * We are at the end of the loop. We allow for outstanding "true" jump
    * to come here.
    * --------------------------------------------------------------------- */

   S_CHECK(patch_jumps(&c->truey_jumps, tape, jump_depth(c)));

   assert(no_outstanding_jump(&c->truey_jumps, jump_depth(c)));
   assert(no_outstanding_jump(&c->falsey_jumps, jump_depth(c)));

   S_CHECK(end_scope(interp, tape));

   /* Consume the delimiter_endloop token */
//   S_CHECK(_advance(interp, p, &toktype))
   S_CHECK(parser_expect(interp, "end delimiter of loop", closing_delimiter));

   /* Parse the next token */
   S_CHECK(advance(interp, p, &toktype))

   /* ---------------------------------------------------------------------
    * If we are back to the general scope, we execute the VM now
    * --------------------------------------------------------------------- */

   return OK;
}

static int sexpr_init(Tape * restrict tape, SimpleExpr *sexpr)
{
   /* switch_back_imm is set earlier */
   bool switch_back_imm = sexpr->switch_back_imm;
   memset(sexpr, 0, sizeof(*sexpr));
   sexpr->switch_back_imm = switch_back_imm;

   sexpr->p_bck = UINT_MAX;
   gmsindices_init(&sexpr->label_gmsindices);
   gmsindices_deactivate(&sexpr->label_gmsindices);

   return OK;
}

int vm_sexpr_init(Interpreter * interp, SimpleExpr *sexpr)
{
   S_CHECK(c_switch_to_compmode(interp, &sexpr->switch_back_imm));
   Compiler *c = interp->compiler;

   EmpVm *vm = c->vm;
   Tape _tape = {.code = &vm->code, .linenr = UINT_MAX};
   Tape * const tape = &_tape;

   if (!sexpr->switch_back_imm) {
      begin_scope(c, __func__);
   }

   return sexpr_init(tape, sexpr);
}

/**
 * @brief Parse a simple expression in the objective part, potentially within a sum
 *
 * @param      interp  the EMP interpreter
 * @param      p       the position index
 * @param[out] sexpr   the simple expression
 *
 * @return             the error code
 */
static int parse_sexpr(Interpreter * restrict interp, unsigned * restrict p,
                       SimpleExpr * restrict sexpr)
{
   TokenType toktype;
   unsigned p_bck;

   do {
      S_CHECK(advance(interp, p, &toktype));
      PARSER_EXPECTS(interp, "a GAMS variable or parameter or a value function",
                     TOK_GMS_VAR, TOK_GMS_PARAM, TOK_IDENT);

      switch (toktype) {

      case TOK_GMS_VAR:
         if (sexpr->has_var) {
            error("[empinterp] ERROR on line %u: only one variable is allowed in a %s "
                  "statement\n", interp->linenr, toktype2str(TOK_SUM));
            return Error_EMPIncorrectInput;
            }
         sexpr->has_var = true;
         break;

      case TOK_GMS_PARAM:
         if (sexpr->has_param) {
            error("[empinterp] ERROR on line %u: only one parameter is allowed in a %s "
                  "statement\n", interp->linenr, toktype2str(TOK_SUM));
            return Error_EMPIncorrectInput;
            }
         sexpr->has_param = true;
         break;

      case TOK_IDENT: {
         IdentData ident;
         // HACK
         S_CHECK(interp->ops->resolve_tokasident(interp, &ident));

         // HACK
         if (ident.type == IdentNotFound) {
            /* ----------------------------------------------------------------------
             * We expect the identifier to be a label, followed by a valfn
             * ---------------------------------------------------------------------- */
             if (sexpr->has_valfn) {
                error("[empinterp] ERROR on line %u: only one value function (valFn) is allowed in a %s "
                      "statement\n", interp->linenr, toktype2str(TOK_SUM));
                return Error_EMPIncorrectInput;
             }
             sexpr->has_valfn = true;

             interp_set_last_empdag_node_symbol(interp, ident.lexeme.start, ident.lexeme.len);

             tok2lexeme(&interp->cur, &sexpr->label_valfn);

             unsigned p2 = *p;
             TokenType toktype2;
             S_CHECK(peek(interp, &p2, &toktype2));
             if (toktype2 == TOK_LPAREN) {
                  *p = p2;
                  S_CHECK(parse_gmsindices(interp, p, &sexpr->label_gmsindices));
              }

               S_CHECK(advance(interp, p, &toktype));

               S_CHECK(consume_valfn_kwd(interp, p));

               /* HACK: there needs to be an abstraction here */
                  //{ GDB_STOP()}

                  p2 = *p;
                  S_CHECK(peek(interp, &p2, &toktype2));
                  if (toktype2 == TOK_DOT) {
                     *p = p2;
                     S_CHECK(advance(interp, p, &toktype2));
                     S_CHECK(parser_expect(interp, "After a valfn, only the smooth operator is valid",
                                           TOK_SMOOTH));
                     sexpr->has_smooth = true;

                     /* smoothing requires at least one parameter (the value) */
                     //S_CHECK(advance(interp, p, &toktype));
                     //S_CHECK(parser_expect(interp, "After a smooth keyword, an left parenthesis '(' is expected",
                     //                      TOK_LPAREN));
                     //S_CHECK(advance(interp, p, &toktype));
                     
                     
                     OperatorKeyword smoothing_keywords[] = {
                        {.name = "scheme", .optional = true, .type = OperatorKeywordString,
                           .position = UINT_MAX, .kwstrs = smoothing_scheme_options, .strsetter = smoothing_scheme_setter},
                        {.name = "par",    .optional = true, .type = OperatorKeywordScalar,
                           .position = UINT_MAX, //.dblsetter = smoothing_par_dblsetter,
                           .uintsetter = smoothing_par_uintsetter },
                     };


                     SmoothingOperatorData smoothing_operator;
                     smoothing_operator_data_init(&smoothing_operator);

                     S_CHECK(parse_operator_kw_args(interp, p, ARRAY_SIZE(smoothing_keywords),
                                                    smoothing_keywords, &smoothing_operator));

                     sexpr->p_bck = smoothing_operator.parameter_position;

                     sexpr->parse_kwd = true;
                  } else if (toktype2 == TOK_CONDITION) {
                     
                     // HACK embmode
                     if (!embmode(interp) && gmsindices_nvardims(&sexpr->label_gmsindices) > 0) {
                        error("[empinterp] ERROR on line %u: single node label selection via conditional "
                              "is not implemented yet. Please report this to help implemented it\n",
                              interp->linenr);
                        return Error_NotImplemented;
                     }

                     *p = p2;
                     interp_cpypeek2cur(interp);

                     if (immmode(interp)) {
                        error("[empinterp] ERROR on line %u: conditional in immediate mode is "
                              "unsupported.\n", interp->linenr);
                        return Error_EMPRuntimeError;
                     }

                     S_CHECK(vm_parse_condition(interp, p));

                  }




             } else {

             if (sexpr->has_param) {
                error("[empinterp] ERROR on line %u: only one parameter is allowed in a %s "
                      "statement\n", interp->linenr, toktype2str(TOK_SUM));
                return Error_EMPIncorrectInput;
             }
             sexpr->has_param = true;

              double dummyval;
              S_CHECK(parse_identasscalar(interp, p, &dummyval));
             }

         }
         break;

      default: runtime_error(interp->linenr);
      }

      p_bck = *p;

      S_CHECK(advance(interp, p, &toktype));
   } while (toktype == TOK_STAR);

   //backtrack
   *p = p_bck;

   return OK;
}

int vm_parse_sexpr(Interpreter * restrict interp, unsigned * restrict p,
                   SimpleExpr * restrict sexpr)
{
   assert(!immmode(interp));

   return parse_sexpr(interp, p, sexpr);
}

/**
 * @brief Perform codegen for a simple expression
 *
 * @param interp         the Interpreter
 * @param sexpr          the simple expression
 * @param c              the EMP Compiler
 * @param tape           the VM tape
 * @param sum_iterators  the SUM operator iterators, if any
 *
 * @return               the error code
 */
static int sexpr_codegen(Interpreter * restrict interp, SimpleExpr * restrict sexpr,
                         Compiler *c, Tape * restrict tape,
                         LoopIterators * restrict sum_iterators)
{
   unsigned linklabel_gidx = UINT16_MAX;

   // FIXME: this is required, otherwise bogus variables/scalar are added
   if (!sexpr->has_var) {
     S_CHECK(emit_byte(tape, OP_HACK_DEL_SCALARVAR));
   }

   if (!sexpr->has_param) {
     S_CHECK(emit_byte(tape, OP_HACK_DEL_SCALARPARAM));
   }

   if (sexpr->has_valfn) {

      // HACK
      if (gmsindices_nvardims(&sexpr->label_gmsindices) > 0 && !embmode(interp)) {
         TO_IMPLEMENT("Implementation of the SUM operator is incomplete")
      }

      LinkType linktype = sexpr->has_smooth ? LinkObjAddMapSmoothed : LinkArcVF;
      LoopIterators loopiters = {.niters = 0};
      S_CHECK(linklabels_init_from_loopiterators(interp, tape, sexpr->label_valfn.start,
                                                 sexpr->label_valfn.len, linktype,
                                                 &sexpr->label_gmsindices, &loopiters,
                                                 &linklabel_gidx));

      DBGUSED u8 nvardims = sum_iterators ? gmsindices_nvardims(&sexpr->label_gmsindices) : 0;
      assert(embmode(interp) || nvardims == 0);

      /* FIXME: very dirty, there are as many linklabels objects as links ... */
      S_CHECK(emit_byte(tape, OP_LINKLABELS_DUP));
      S_CHECK(EMIT_GIDX(tape, linklabel_gidx));

      if (sexpr->parse_kwd) { // only smooth() operator. We assume to be at n.valn.smooth(
                       //                                                         ^ 
         assert(sexpr->p_bck < UINT_MAX);
         double dummy;
         TokenType toktype_dummy;
         S_CHECK(advance(interp, &sexpr->p_bck, &toktype_dummy));
         if (!embmode(interp)) {
            S_CHECK(parse_identasscalar(interp, &sexpr->p_bck, &dummy));
         }
         S_CHECK(emit_byte(tape, OP_LINKLABELS_KEYWORDS_UPDATE));
      }

   }  else { // FIXME: This looks like a hack
 
      LinkLabels *linklabels;
      S_CHECK(vm_linklabels_alloc(c->vm, &linklabels, NULL, 0, 0, 0, 0, LinkObjAddMap,
                                  &linklabel_gidx));

      // FIXME: same as above
      S_CHECK(emit_byte(tape, OP_LINKLABELS_DUP));
      S_CHECK(EMIT_GIDX(tape, linklabel_gidx));
   }

   return OK;
}

int vm_codegen_sexpr(Interpreter * restrict interp, SimpleExpr * restrict sexpr)
{
   Compiler *c = interp->compiler;

   EmpVm *vm = c->vm;
   Tape _tape = {.code = &vm->code, .linenr = UINT_MAX};
   Tape * const tape = &_tape;

   S_CHECK(sexpr_codegen(interp, sexpr, c, tape, NULL));

   /* Collect falsey jump (in case of a condition) */
   patch_jumps(&c->falsey_jumps, tape, jump_depth(c));

   if (sexpr->switch_back_imm) {
      S_CHECK(c_switch_to_immmode(interp))
   } else {
      S_CHECK(end_scope(interp, tape));
   }


   return OK;
}

/* ------------------------------------------------------------------------
 * Design notes for projection implementation.
 *
 * The high-level syntax is SUM { arg, simple_expr }. The generated pseudo-code is
 *
 * 1. init arg1 iterators (arg1_iter)
 * 2. if (!arg1.condition or arg1.condition(arg1_iter)) then jump @5 (SEXPR_START) end;
 * 3. goto 5 (ARG1_UPDATE)
 *
 * 4. Parse SEXPR
 *
 * 5. while(!arg1.iterend(arg1_iter)) { update(arg1_iter); jump @2. }
 *
 * Salient point: There needs to be an indicator whether arg1.condition exists . In that
 * case more codegen needs to happen to not continue to 4 in that case
 * ------------------------------------------------------------------------ */
int parse_sum(Interpreter * restrict interp, unsigned * restrict p)
{
   int status = OK;
   TokenType toktype;
   bool switch_back_imm = false;
   assert(interp->cur.type == TOK_SUM);

   /* TODO: do we need MP to save its ID? */
   S_CHECK(c_switch_to_compmode(interp, &switch_back_imm));
   Compiler *c = interp->compiler;

   EmpVm *vm = c->vm;
   Tape _tape = {.code = &vm->code, .linenr = UINT_MAX};
   UNUSED Tape * const tape = &_tape;

   trace_empparser("[empcompiler] SUM is starting\n");

   /* Consume the opening delimiter (parent / bracket / ...) */
   S_CHECK(advance(interp, p, &toktype))

   PARSER_EXPECTS(interp, "A delimiter '(' or '{' or [')", TOK_LPAREN, TOK_LBRACE, TOK_LBRACK);

   TokenType closing_delimiter = tok_closingdelimiter(toktype);

   /* ---------------------------------------------------------------------
    * Step 0: Define the VM objects
    * --------------------------------------------------------------------- */
   /* This defines the loop iterators and the linklabels object */
   // XXX understand what loopiterator is doing in our case

   SimpleExpr sexpr;
   S_CHECK(sexpr_init(tape, &sexpr));
   sexpr.switch_back_imm = switch_back_imm;

   begin_scope(c, __func__);

   /* ---------------------------------------------------------------------
    * Step 1: Parse sum iterators
    * --------------------------------------------------------------------- */
   bool no_test_in_arg1;
   LoopIterators sum_iterators;
   S_CHECK(sumtype_parse_arg1(interp, p, c, tape, &sum_iterators, &no_test_in_arg1));

   /* If there was a test in arg1, we need to unconditionally jump to the sum_iterators
    * update and then resolve the truey jumps */
   if (!no_test_in_arg1) {
      Jump jump = { .depth = jump_depth(c) };
      S_CHECK(emit_jump(tape, OP_JUMP, &jump.addr));
      S_CHECK(jumps_add_verbose(&c->falsey_jumps, jump));

      S_CHECK(patch_jumps(&c->truey_jumps, tape, jump_depth(c)));
   }

   /* ---------------------------------------------------------------------
    * Step 2: parse an expression like param * n(...){.dual()}.valFn * var
    * --------------------------------------------------------------------- */
   S_CHECK(parse_sexpr(interp, p, &sexpr));

   // WE need to ensure that all parameters / variables only resolve to scalar quantities
 
   // HACK: move this around, towards the end of the function
   /* Consume the delimiter_endloop token */
   S_CHECK(advance(interp, p, &toktype));
   S_CHECK(parser_expect(interp, "end delimiter of sum", closing_delimiter));

   S_CHECK(sexpr_codegen(interp, &sexpr, c, tape, &sum_iterators));

   /* ---------------------------------------------------------------------
    * Step 3:  Increment loop index and check of the condition idx < max
    * --------------------------------------------------------------------- */

   tape->linenr = interp->linenr;

   /* Resolve all remaining jumps linked to a false condition */
   S_CHECK(patch_jumps(&c->falsey_jumps, tape, jump_depth(c)));

   S_CHECK(loop_increment(tape, &sum_iterators));

   /* ---------------------------------------------------------------------
    * We are at the end of the loop. We allow for outstanding "true" jump
    * to come here.
    * --------------------------------------------------------------------- */

   S_CHECK(patch_jumps(&c->truey_jumps, tape, jump_depth(c)));

   assert(no_outstanding_jump(&c->truey_jumps, jump_depth(c)));
   assert(no_outstanding_jump(&c->falsey_jumps, jump_depth(c)));

   S_CHECK(end_scope(interp, tape));

   /* ---------------------------------------------------------------------
    * If we are back to the general scope, we execute the VM now
    * --------------------------------------------------------------------- */

   if (switch_back_imm) {
      S_CHECK(c_switch_to_immmode(interp))
   }

   return status;
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
      resolve_identas(interp, &ident, "a GAMS set is expected",
                      IdentLocalSet, IdentSet, IdentVector, IdentLocalVector);
   } else {
      S_CHECK(tok2ident(&interp->cur, &ident));
   }

   unsigned dummylen;
   const char *lvar_name = NULL;
   IdentType lvar_type;
   S_CHECK(gen_localvarname(&lexeme, "", &lvar_name, &dummylen));

   unsigned lvar_gidx;
   EmpVmOpCode opcode_add, opcode_reset;
   switch(ident.type) {
   case IdentLocalSet:
   case IdentSet: {
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
   LoopIterators loopiter = {.niters = 1, .loopobj_gidx = UINT_MAX};
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
   S_CHECK(patch_jumps(&c->falsey_jumps, tape, jump_depth(c)));

   S_CHECK(loop_increment(tape, &loopiter));

   /* ---------------------------------------------------------------------
    * We are at the end of the loop. We allow for outstanding "true" jump
    * to come here.
    * --------------------------------------------------------------------- */

   S_CHECK(patch_jumps(&c->truey_jumps, tape, jump_depth(c)));

   assert(no_outstanding_jump(&c->truey_jumps, jump_depth(c)));
   assert(no_outstanding_jump(&c->falsey_jumps, jump_depth(c)));

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
 * @param interp  the interpreter
 * @param p       the position pointer
 * @param labelname  the label
 * @param labelname_len the label length
 * @param gmsindices   the label indices
 * @return  the error code
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
 
   /* Parse $(filter(sets)) */
   S_CHECK(parse_condition(interp, p, c, tape));

   /* All remaining jumps from the condition should now be set to the scope depth.
    * This avoids interference with condition in the label definition */
   S_CHECK(c_normalize_jumps_depth(c));

   TokenType toktype;
   S_CHECK(advance(interp, p, &toktype))

   if (toktype != TOK_COLON) {
      error("[empcompiler] Error line %u: unexpected token '%.*s' of type '%s'",
            interp->linenr, emptok_getstrlen(&interp->cur),
            emptok_getstrstart(&interp->cur), toktype2str(emptok_gettype(&interp->cur)));
   }

   S_CHECK(labdeldef_parse_statement(interp, p));

   /* ---------------------------------------------------------------------
    * Step 4:  Increment loop index and check of the condition idx < max holds
    * --------------------------------------------------------------------- */

   /* Resolve all remaining jumps linked to a false condition */
   S_CHECK(patch_jumps(&c->falsey_jumps, tape, jump_depth(c)));

   S_CHECK(loop_increment(tape, &iterators));

   /* ---------------------------------------------------------------------
    * We are at the end of the loop. We allow for outstanding "true" jump
    * to come here.
    * --------------------------------------------------------------------- */

   S_CHECK(patch_jumps(&c->truey_jumps, tape, jump_depth(c)));

   assert(no_outstanding_jump(&c->truey_jumps, jump_depth(c)));
   assert(no_outstanding_jump(&c->falsey_jumps, jump_depth(c)));

   S_CHECK(end_scope(interp, tape));

   return OK;
}

/**
 * @brief Generate the bytecode for a label definition of the form
 *     nOpt(set): ...
 *
 * @param interp         the interpreter
 * @param p              the position pointer
 * @param labelname      the label
 * @param labelname_len  the label length
 * @param gmsindices     the label indices
 *
 * @return               the error code
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
   S_CHECK(patch_jumps(&c->falsey_jumps, tape, jump_depth(c)));

   S_CHECK(loop_increment(tape, &iterators));

   /* ---------------------------------------------------------------------
    * We are at the end of the loop. We allow for outstanding "true" jump
    * to come here.
    * --------------------------------------------------------------------- */

   S_CHECK(patch_jumps(&c->truey_jumps, tape, jump_depth(c)));

   assert(no_outstanding_jump(&c->truey_jumps, jump_depth(c)));
   assert(no_outstanding_jump(&c->falsey_jumps, jump_depth(c)));

   S_CHECK(end_scope(interp, tape));

   return OK;
}

/**
 * @brief  Add edges in the empdag between current daguid and argument
 *
 * @param interp       the interpreter
 * @param p            the indices
 * @param arctype      the type for all edges
 * @param argname      the basename of the labels
 * @param argname_len  the length of the basename 
 * @param gmsindices   the indices for the basename
 * @return             the error code
 */
static int vm_add_arcs(Interpreter * interp, unsigned * restrict p, LinkType arctype,
                        const char* argname, unsigned argname_len, GmsIndicesData* gmsindices)
{
   //assert(gmsindices->nargs > 0);

   /* TODO URG: review if this is sound */
   UNUSED Compiler *c = ensure_vm_mode(interp);

   /* ---------------------------------------------------------------------
    * If the scope depth is 0, we are not in a loop. Then, the daguid needs
    * to be updated.
    * --------------------------------------------------------------------- */

//   if (c->state.scope_depth == 0) {
//      EmpVm *vm = c->vm;
//      Tape _tape = {.code = &vm->code, .linenr = UINT_MAX};
//      Tape * const tape = &_tape;
//
//      S_CHECK(emit_byte(tape, OP_SET_DAGUID_FROM_REGENTRY));
//
//   }

   S_CHECK(vm_gmsindicesasarc(interp, p, argname, argname_len, arctype, gmsindices));

   return OK;
}

int vm_nash(Interpreter * interp, unsigned * restrict p, const char* argname,
            unsigned argname_len, GmsIndicesData* gmsindices)
{
   return vm_add_arcs(interp, p, LinkArcNash, argname, argname_len, gmsindices);
}

int vm_add_VFobjSimple_arc(Interpreter * interp, unsigned * restrict p, const char* argname,
            unsigned argname_len, GmsIndicesData* gmsindices)
{
   return vm_add_arcs(interp, p, LinkArcVF, argname, argname_len, gmsindices);
}

int vm_add_Ctrl_edge(Interpreter * interp, unsigned * restrict p, const char* argname,
                     unsigned argname_len, GmsIndicesData* gmsindices)
{
   return vm_add_arcs(interp, p, LinkArcCtrl, argname, argname_len, gmsindices);
}

NONNULL static
int ovfdecl_fillparam(EmpVm* vm, OvfDeclData * restrict ovfdecl, unsigned ovf_idx) 
{
   ovfdecl->active = true;

   S_CHECK(ovf_fill_params(&ovfdecl->params, ovf_idx));
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
   c->state.vmstack_depth += 1;

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
   S_CHECK(emit_bytes(tape, OP_EMPAPI_CALL, FN_CCFLIB_UPDATEPARAMS,
                            OP_EMPAPI_CALL, FN_CCFLIB_FINALIZE,
                            OP_POP));

   assert(c->state.vmstack_depth >= 2);
   c->state.vmstack_depth -= 2;

   ovfdecl_empty(&c->ovfdecl);

   return OK;
}

static int c_gms_resolve(Interpreter* restrict interp, unsigned * p)
{
   Compiler *c = interp->compiler;
   assert(interp->gms_sym_iterator.active);

   EmpVm *vm = c->vm;
   Tape tape_ = {.code = &vm->code, .linenr = interp->linenr};
   Tape * const tape = &tape_;

   LoopIterators loopiter = {.niters = 0};
   unsigned symiter_gidx;
   IdentData *ident = &interp->gms_sym_iterator.ident;

   begin_scope(c, __func__);

   S_CHECK(gmssymiter_init(interp, ident, &interp->gms_sym_iterator.indices,
                           &loopiter, tape, &symiter_gidx));

   if (loopiter.niters == 0) {

     /* ---------------------------------------------------------------------
      * We can just resolve and move on.
      * --------------------------------------------------------------------- */

      S_CHECK(emit_byte(tape, OP_GMS_SYMBOL_READ_SIMPLE));
      S_CHECK(EMIT_GIDX(tape, symiter_gidx));

      goto _exit;
   }

   assert(ident->type == IdentEqu || ident->type == IdentVar);
   S_CHECK(emit_bytes(tape, OP_GMS_EQUVAR_READ_INIT, ident->type));

   S_CHECK(loop_initandstart(interp, tape, &loopiter));

   unsigned p2 = *p;
   TokenType toktype;
   S_CHECK(peek(interp, &p2, &toktype));

   if (toktype == TOK_CONDITION) {
      *p = p2;
      interp_save_tok(interp);
      // HACK: this should not be necessary
      interp->cur.type = toktype;
      S_CHECK(parse_condition(interp, p, c, tape));
      interp_restore_savedtok(interp);
   }

   /* ---------------------------------------------------------------------
    * Main body of the loop: just perform the test and jump out of the loop
    * if true.
    * --------------------------------------------------------------------- */

   S_CHECK(emit_byte(tape, OP_GMS_SYMBOL_READ_EXTEND));
   S_CHECK(EMIT_GIDX(tape, symiter_gidx));

   /* ---------------------------------------------------------------------
    * Step 4:  Increment loop index and check of the condition idx < max
    * --------------------------------------------------------------------- */

   /* Resolve all remaining jumps linked to a false condition */
   S_CHECK(patch_jumps(&c->falsey_jumps, tape, jump_depth(c)));

   S_CHECK(loop_increment(tape, &loopiter));

   S_CHECK(emit_bytes(tape, OP_GMS_EQUVAR_READ_SYNC, ident->type));

_exit:

   /* ---------------------------------------------------------------------
    * We are at the end of the loop. We allow for outstanding "true" jump
    * to come here.
    * --------------------------------------------------------------------- */

   S_CHECK(patch_jumps(&c->truey_jumps, tape, jump_depth(c)));

   assert(no_outstanding_jump(&c->truey_jumps, jump_depth(c)));
   assert(no_outstanding_jump(&c->falsey_jumps, jump_depth(c)));

   S_CHECK(end_scope(interp, tape));

   interp->gms_sym_iterator.active = false;
   c->gmsfilter.active = false;

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
   c->state.vmstack_depth += 1;

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

   S_CHECK(emit_bytes(tape, OP_EMPAPI_CALL, FN_OVF_SETRHO));

   return OK;
}

static int c_ovf_addarg(Interpreter* restrict interp, UNUSED void *ovfdef_data)
{
   Compiler *c = interp->compiler;
   assert(c->ovfdecl.active);

   EmpVm * restrict vm = c->vm;
   Tape tape_ = {.code = &vm->code, .linenr = interp->linenr};
   Tape * const tape = &tape_;

   S_CHECK(emit_bytes(tape, OP_EMPAPI_CALL, FN_OVF_ADDARG));

   return OK;
}

/**
 * @brief Pushes the gidx of the OVF onto the stack
 *
 * @param interp       the interpreter
 * @param ovfdef_data  the OVF data
 * @param paramsdef    the OVF parameter definitions
 *
 * @return             the error code
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
   c->state.vmstack_depth++;

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
         error("[empcompiler] ERROR on line %u: mandatory parameter '%s' not found (OVF '%s')\n",
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

   return emit_bytes(tape, OP_EMPAPI_CALL, FN_OVFPARAM_UPDATE);
}

static int c_ovf_check(Interpreter* restrict interp, UNUSED void *ovfdef_data)
{
   Compiler * restrict c = interp->compiler;
   assert(c->ovfdecl.active);

   EmpVm * restrict vm = c->vm;
   Tape tape_ = {.code = &vm->code, .linenr = interp->linenr};
   Tape * const tape = &tape_;

   /* Update the params object, finalize the ovf and pop it */
   S_CHECK(emit_bytes(tape, OP_EMPAPI_CALL, FN_OVF_UPDATEPARAMS,
                            OP_EMPAPI_CALL, FN_OVF_FINALIZE,
                            OP_POP));

   assert(c->state.vmstack_depth >= 2);
   c->state.vmstack_depth -= 2;

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
                      unsigned* restrict param_gidx)
{
   Compiler* restrict c = interp->compiler;

   EmpVm *vm = c->vm;
   Tape tape_ = {.code = &vm->code, .linenr = interp->linenr};
   Tape * const tape = &tape_;

   LoopIterators loopiters = {.niters = 0};
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
   gmsindices_init(indices);

   S_CHECK(parse_gmsindices(interp, p, indices));

   S_CHECK(gmssymiter_init(interp, ident, &interp->gms_sym_iterator.indices, &loopiters,
                              tape, &gmsfilter_gidx));

   if (loopiters.niters != 1) {
      error("[empcompiler] ERROR on line %u: OVF parameter can only have one set as indices, got %u\n",
            interp->linenr, loopiters.niters);
      return Error_EMPIncorrectSyntax;
   }

   /* ---------------------------------------------------------------------
    * Copy the vmvec ovject from global to a local variable
    * --------------------------------------------------------------------- */

   LIDX_TYPE vmvec_lidx;
   S_CHECK(declare_localvar(interp, tape, &ident->lexeme, "_vmvec_obj",
                            IdentInternalIndex, CstZeroUInt, &vmvec_lidx));

   S_CHECK(emit_bytes(tape, OP_LVAR_COPYFROM_GIDX, vmvec_lidx));
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
      interp_cpypeek2cur(interp);
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

   S_CHECK(patch_jumps(&c->falsey_jumps, tape, jump_depth(c)));

   S_CHECK(loop_increment(tape, &loopiters));

   /* ---------------------------------------------------------------------
    * We are at the end of the loop. We allow for outstanding "true" jump
    * to come here.
    * --------------------------------------------------------------------- */

   S_CHECK(patch_jumps(&c->truey_jumps, tape, jump_depth(c)));

   assert(no_outstanding_jump(&c->truey_jumps, jump_depth(c)));
   assert(no_outstanding_jump(&c->falsey_jumps, jump_depth(c)));



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

static int c_read_elt_vector(Interpreter *interp, const char *vectorname,
                             IdentData *ident, GmsIndicesData *gmsindices,
                             UNUSED double *val)
{
   Compiler *c = interp->compiler;

   EmpVm *vm = c->vm;
   Tape tape_ = {.code = &vm->code, .linenr = interp->linenr};
   Tape * const tape = &tape_;

   LoopIterators loopiter = {.niters = 0};
   unsigned gmssymiter_gidx;
   S_CHECK(gmssymiter_init(interp, ident, gmsindices,
                           &loopiter, tape, &gmssymiter_gidx));

   if (loopiter.niters != 0) { 
      TO_IMPLEMENT("loop iterators");
   }

   S_CHECK(emit_byte(tape, OP_GMS_SYMBOL_READ_SIMPLE));
   S_CHECK(EMIT_GIDX(tape, gmssymiter_gidx));

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
   c->state.vmstack_depth += 1;

   *mp = NULL;

   return OK;
}

static int c_mp_addcons(Interpreter *interp, UNUSED MathPrgm *mp)
{
   Compiler *c = interp->compiler;

   EmpVm * restrict vm = c->vm;
   Tape tape_ = {.code = &vm->code, .linenr = interp->linenr};
   Tape * const tape = &tape_;

   S_CHECK(emit_bytes(tape, OP_EMPAPI_CALL, FN_MP_ADDCONS));

   return OK;
}

static int c_mp_addvars(Interpreter *interp, UNUSED MathPrgm *mp)
{
   Compiler *c = interp->compiler;
   EmpVm * restrict vm = c->vm;
   Tape tape_ = {.code = &vm->code, .linenr = interp->linenr};
   Tape * const tape = &tape_;

   S_CHECK(emit_bytes(tape, OP_EMPAPI_CALL, FN_MP_ADDVARS));

   return OK;
}

static int c_mp_addvipairs(Interpreter *interp, UNUSED MathPrgm *mp)
{
   Compiler *c = interp->compiler;
   EmpVm * restrict vm = c->vm;
   Tape tape_ = {.code = &vm->code, .linenr = interp->linenr};
   Tape * const tape = &tape_;

   S_CHECK(emit_bytes(tape, OP_EMPAPI_CALL, FN_MP_ADDVIPAIRS));

   return OK;
}

static int c_mp_addzerofunc(Interpreter *interp, UNUSED MathPrgm *mp)
{
   Compiler *c = interp->compiler;
   EmpVm * restrict vm = c->vm;
   Tape tape_ = {.code = &vm->code, .linenr = interp->linenr};
   Tape * const tape = &tape_;

   S_CHECK(emit_bytes(tape, OP_EMPAPI_CALL, FN_MP_ADDZEROFUNC));

   return OK;
}

static int c_mp_finalize(UNUSED Interpreter *interp, UNUSED MathPrgm *mp)
{
   Compiler *c = interp->compiler;
   EmpVm * restrict vm = c->vm;
   Tape tape_ = {.code = &vm->code, .linenr = interp->linenr};
   Tape * const tape = &tape_;

   S_CHECK(emit_bytes(tape, OP_EMPAPI_CALL, FN_MP_FINALIZE, OP_POP));

   assert(c->state.vmstack_depth >= 1);
   c->state.vmstack_depth -= 1;

   return OK;
}

static int c_mp_setaschild(UNUSED Interpreter *interp, UNUSED MathPrgm *mp)
{
   Compiler *c = interp->compiler;
   EmpVm * restrict vm = c->vm;
   Tape tape_ = {.code = &vm->code, .linenr = interp->linenr};
   Tape * const tape = &tape_;

   S_CHECK(emit_bytes(tape, OP_EMPAPI_CALL, FN_MP_SETID_AS_CHILD));

   return OK;
}

static int c_mp_setobjvar(Interpreter *interp, UNUSED MathPrgm *mp)
{
   Compiler *c = interp->compiler;
   EmpVm * restrict vm = c->vm;
   Tape tape_ = {.code = &vm->code, .linenr = interp->linenr};
   Tape * const tape = &tape_;

   S_CHECK(emit_bytes(tape, OP_EMPAPI_CALL, FN_MP_SETOBJVAR));

   return OK;
}

static int c_mp_setprobtype(UNUSED Interpreter *interp, UNUSED MathPrgm *mp, unsigned probtype)
{
   assert(probtype < UINT8_MAX);
   Compiler *c = interp->compiler;
   EmpVm * restrict vm = c->vm;
   Tape tape_ = {.code = &vm->code, .linenr = interp->linenr};
   Tape * const tape = &tape_;

   S_CHECK(emit_bytes(tape, OP_PUSH_BYTE, probtype, OP_EMPAPI_CALL, FN_MP_SETPROBTYPE));

   return OK;
}

static int c_nash_new(Interpreter *interp, UNUSED Nash **nash)
{
   Compiler *c = interp->compiler;
   EmpVm * restrict vm = c->vm;
   Tape tape_ = {.code = &vm->code, .linenr = interp->linenr};
   Tape * const tape = &tape_;

   S_CHECK(emit_bytes(tape, OP_NEW_OBJ, FN_NASH_NEW));

   return OK;
}

static int c_nash_addmp(Interpreter *interp, nashid_t nashid, UNUSED MathPrgm *mp)
{
   assert(nashid < UINT8_MAX);

   Compiler *c = interp->compiler;
   EmpVm * restrict vm = c->vm;
   Tape tape_ = {.code = &vm->code, .linenr = interp->linenr};
   Tape * const tape = &tape_;

   S_CHECK(emit_bytes(tape, OP_PUSH_BYTE, nashid, OP_EMPAPI_CALL, FN_NASH_ADDMPBYID));

   return OK;
}

static int c_nash_finalize(UNUSED Interpreter *interp, UNUSED Nash *mpe)
{
   Compiler *c = interp->compiler;
   EmpVm * restrict vm = c->vm;
   Tape tape_ = {.code = &vm->code, .linenr = interp->linenr};
   Tape * const tape = &tape_;

   S_CHECK(emit_bytes(tape, OP_EMPAPI_CALL, FN_NASH_FINALIZE, OP_POP));

   assert(c->state.vmstack_depth >= 1);
   c->state.vmstack_depth -= 1;

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

   S_CHECK(emit_bytes(tape, OP_EMPAPI_CALL, FN_CTR_MARKEQUASFLIPPED));

   return OK;
}

static int c_resolve_tokasident(Interpreter * restrict interp, IdentData * restrict ident)
{
   return resolve_tokasident(interp, ident);
}

const struct interp_ops interp_ops_compiler = {
   .type = InterpreterOpsCompiler,
   .ccflib_new = c_ccflib_new,
   .ccflib_finalize = c_ccflib_finalize,
   .ctr_markequasflipped = c_ctr_markequasflipped,
   .read_gms_symbol = c_gms_resolve,
   .identaslabels = c_identaslabels,
   .mp_addcons = c_mp_addcons,
   .mp_addvars = c_mp_addvars,
   .mp_addvipairs = c_mp_addvipairs,
   .mp_addzerofunc = c_mp_addzerofunc,
   .mp_finalize = c_mp_finalize,
   .mp_new = c_mp_new,
   .mp_setaschild        = c_mp_setaschild,
   .mp_setobjvar = c_mp_setobjvar,
   .mp_setprobtype = c_mp_setprobtype,
   .nash_addmp = c_nash_addmp,
   .nash_new = c_nash_new,
   .nash_finalize = c_nash_finalize,
   .ovf_addbyname = c_ovf_addbyname,
   .ovf_addarg = c_ovf_addarg,
   .ovf_paramsdefstart = c_ovf_paramsdefstart,
   .ovf_setparam = c_ovf_setparam,
   .ovf_setrhovar = c_ovf_setrhovar,
   .ovf_check = c_ovf_check,
   .ovf_param_getvecsize = c_ovf_param_getvecsize,
   .ovf_getname = c_ovf_getname,
   .read_param = c_read_param,
   .read_elt_vector = c_read_elt_vector,
   .resolve_tokasident = c_resolve_tokasident,
};


int empvm_finalize(Interpreter *interp)
{
   Compiler *c = interp->compiler;

   if (c->state.scope_depth != 0) {
      error("[empcompiler] ERROR: after parsing the file, the compiler depth"
            "is nonzero: %u", c->state.scope_depth);
   }
   /* no keyword is active */
   interp_resetlastkw(interp);

   assert(c->truey_jumps.len == 0 && c->falsey_jumps.len == 0);

   /* TODO: think whether we want to do a "symbolic" check in Emb mode? */
   if (!embmode(interp) && c->vm->code.len > 0) {
      trace_empparsermsg("\n[empinterp] VM execution\n");
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
   if (!immmode(interp)) {
      *switched = false;

      return OK;
   }

   *switched = true;

   if (!interp->compiler) {
      NS_CHECK(compiler_init(interp));
   }

   EmpVm *vm = interp->compiler->vm;
   if (vm->code.len) {
      TO_IMPLEMENT("temporary switch to vmmode with existing bytecode");
   }

   /* NOTE: this was added (2024.12.05) after noticing that when we switch
       * while processing         root: Nash(ag(i),mkt(j))
       * the vm execution fired up after ag(i)
       *
       * Observe is this creates any issue
       */
   begin_scope(interp->compiler, __func__);

   interp->ops = &interp_ops_compiler;
   vm->data.state.uid_parent = interp->daguid_parent;
   vm->data.state.uid_grandparent = interp->daguid_grandparent;

   trace_empinterp("[empinterp] Switching to VM bytecode. UID parent = '%s' #%u; "
                   "UID GP = '%s' #%u\n",
                   get_daguid_name(&interp->mdl->empinfo.empdag, vm->data.state.uid_parent),
                   vm->data.state.uid_parent,
                   get_daguid_name(&interp->mdl->empinfo.empdag, vm->data.state.uid_grandparent),
                   vm->data.state.uid_grandparent);

   return OK;
}

int c_switch_to_immmode(Interpreter *interp)
{

   Compiler *c = interp->compiler; assert(c);

   EmpVm *vm = c->vm; assert(vm);
   Tape _tape = {.code = &vm->code, .linenr = interp->linenr};
   Tape * const tape = &_tape;

   /* NOTE: see comment in c_switch_to_compmode for why this is needed*/
   end_scope(interp, tape);

   S_CHECK(empvm_finalize(interp));

   if (!embmode(interp)) {
      interp->ops = &interp_ops_imm;
   }

   vm->data.state.uid_parent = EMPDAG_UID_NONE;
   vm->data.state.uid_grandparent = EMPDAG_UID_NONE;

   return OK;
}

void empvm_compiler_setgmd(Interpreter * restrict interp)
{
   interp->compiler->vm->data.gmd = interp->gmd;
   interp->compiler->vm->data.gmddct = interp->gmddct;
}

int hack_scalar2vmdata(Interpreter *interp, unsigned idx)
{
   assert(interp->ops->type == InterpreterOpsCompiler);
   assert(idx < GIDX_MAX);

   Compiler *c = interp->compiler;
   EmpVm * restrict vm = c->vm;
   Tape tape_ = {.code = &vm->code, .linenr = interp->linenr};
   Tape * const tape = &tape_;


   S_CHECK(emit_byte(tape, OP_HACK_SCALAR2VMDATA));
   S_CHECK(EMIT_GIDX(tape, idx));

   return OK;
}
