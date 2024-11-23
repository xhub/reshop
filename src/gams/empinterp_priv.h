#ifndef EMPINTERP_PRIV_H
#define EMPINTERP_PRIV_H

#include "empinterp.h"
#include "empparser.h"
#include "printout.h"


#define RHP_LOCAL_SCOPE
#define RHP_LIST_PREFIX gdxreaders
#define RHP_LIST_TYPE gdx_readers
#define RHP_ELT_TYPE GdxReader
#define RHP_ELT_FREE gdx_reader_free
#define RHP_ELT_FREE_TAKE_ADDRESS
#define RHP_ELT_INVALID ((GdxReader) {.fname = NULL, .gdxh = NULL})
#include "list_generic.inc"

void interp_showerr(Interpreter *interp) NONNULL;
int process_statement(Interpreter * restrict interp, unsigned * restrict p,
                        TokenType toktype) NONNULL;

const char * identtype2str(IdentType type);

/* ---------------------------------------------------------------------
 * Misc functions
 * --------------------------------------------------------------------- */

int skip_spaces_commented_lines(Interpreter *interp, unsigned *p) NONNULL;
int interp_create_buf(Interpreter *interp) NONNULL;
NONNULL_AT(1,3)
void empinterp_init(Interpreter *interp, Model *mdl, const char *fname);
NONNULL void empinterp_free(Interpreter *interp);


/* ---------------------------------------------------------------------
 * Start inline functions
 * --------------------------------------------------------------------- */

NONNULL static inline
int parser_err(Interpreter* interp, const char *msg)
{
   if (interp->health & PARSER_PANIC) {
      return Error_EMPIncorrectSyntax;
   }

   interp->health |= PARSER_ERROR;

   error("[empinterp] Parser error while processing file '%s'\n", interp->empinfo_fname);
   return tok_err(&interp->cur, TOK_UNSET, msg);
}

NONNULL static inline
void interp_resetlastkw(Interpreter* interp)
{
   interp->last_kw_info.type = TOK_UNSET;
}

NONNULL static inline
void parser_cpypeek2cur(Interpreter *interp)
{
   memcpy(&interp->cur, &interp->peek, sizeof(Token));
   trace_empparser("[empinterp] peek -> current: token '%.*s' of type %s\n",
                   interp->cur.len, interp->cur.start, toktype2str(interp->cur.type));
}

NONNULL static inline
void interp_peekseqstart(Interpreter *interp) { interp->peekisactive = true; }
NONNULL static inline
void  interp_peekseqend(Interpreter *interp) { interp->peekisactive = false; }

NONNULL static inline
void  _tok_empty(Token *tok)
{
   tok->iscratch.data = NULL;
   tok->iscratch.size = 0;
   tok->dscratch.data = NULL;
   tok->dscratch.size = 0;
}

NONNULL static inline
void interp_save_tok(Interpreter *interp)
{
   if (interp->pre.type != TOK_UNSET) {
      tok_free(&interp->pre);
   }

   memcpy(&interp->pre, &interp->cur, sizeof(Token));
   _tok_empty(&interp->cur);

   trace_empparser("[empinterp] Saving token '%.*s' of type %s\n", interp->pre.len,
                   interp->pre.start, toktype2str(interp->pre.type));
}

NONNULL static inline
void interp_restore_savedtok(Interpreter *interp)
{
   assert(interp->pre.type != TOK_UNSET);

   if (interp->cur.type != TOK_UNSET) {
      tok_free(&interp->cur);
   }

   memcpy(&interp->cur, &interp->pre, sizeof(Token));
   interp->pre.type = TOK_UNSET;
}

NONNULL static inline
void interp_erase_savedtok(Interpreter *interp)
{
   if (interp->pre.type != TOK_UNSET) {
      tok_free(&interp->pre);
   }

   interp->pre.type = TOK_UNSET;
}

NONNULL static inline
void interp_switch_tok(Interpreter *interp)
{
   assert(interp->pre.type != TOK_UNSET);
   assert(interp->cur.type != TOK_UNSET);

   Token tmp;
   memcpy(&tmp, &interp->cur, sizeof(Token));

   memcpy(&interp->cur, &interp->pre, sizeof(Token));
   memcpy(&interp->pre, &tmp, sizeof(Token));
}

NONNULL static inline void lexeme_init(Lexeme *lexeme, const Token * restrict tok)
{
   lexeme->linenr = tok->linenr;
   lexeme->len = tok->len;
   lexeme->start = tok->start;

}

NONNULL static inline void ident_init(IdentData *ident, const Token * restrict tok)
{
   lexeme_init(&ident->lexeme, tok);

   ident->type = IdentNotFound;
   ident->idx = UINT_MAX;
   ident->dim = UINT8_MAX;
   ident->ptr = NULL;
}

static inline void gmsindices_init(GmsIndicesData *indices)
{
   indices->nargs = 0;
   indices->num_sets = 0;
   indices->num_loopiterators = 0;
   indices->num_localsets = 0;
}

static inline bool gmsindices_isactive(GmsIndicesData *indices)
{
   return indices->nargs != UINT8_MAX && indices->nargs != 0;
}

static inline bool gmsindices_isset(GmsIndicesData *indices)
{
   return indices->nargs != UINT8_MAX;
}

static inline uint8_t gmsindices_nargs(GmsIndicesData *indices)
{
   return indices->nargs != UINT8_MAX ? indices->nargs : 0;
}

static inline bool gmsindices_deactivate(GmsIndicesData *indices)
{
   if (indices->nargs == UINT8_MAX) { return false; }

   indices->nargs = UINT8_MAX;
   return true;
}

static inline bool gmsindices_needcompmode(GmsIndicesData *indices)
{
   return indices->nargs > 0 &&
   (indices->num_sets > 0 || indices->num_localsets > 0 || indices->num_loopiterators > 0);
}

static inline bool _has_bilevel(const Interpreter *interp)
{
   return interp->state.has_bilevel;
}

static inline bool _has_dag(const Interpreter *interp)
{
   return interp->state.has_dag_node;
}

static inline bool _has_dualequ(const Interpreter *interp)
{
   return interp->state.has_dualequ;
}

static inline bool _has_equilibrium(const Interpreter *interp)
{
   return interp->state.has_equilibrium;
}

static inline bool _has_implicit_Nash(const Interpreter *interp)
{
   return interp->state.has_implicit_Nash;
}

static inline bool _has_single_mp(const Interpreter *interp)
{
   return interp->state.has_single_mp;
}

static inline bool is_simple_Nash(const Interpreter *interp)
{
   return interp->state.has_equilibrium || interp->state.has_implicit_Nash;
}

static inline bool _old_empinfo(const Interpreter *interp)
{
   return interp->state.has_equilibrium || interp->state.has_bilevel ||
           interp->state.has_dualequ;
}

static inline bool bilevel_once(const Interpreter *interp)
{
   return interp->state.has_bilevel || interp->state.bilevel_in_progress;
}

static inline void bilevel_in_progress(Interpreter *interp)
{
   interp->state.bilevel_in_progress = true;
}

static inline void parsed_bilevel(Interpreter *interp)
{
   assert(interp->state.bilevel_in_progress);
   interp->state.bilevel_in_progress = false;
   interp->state.has_bilevel = true;
}

static inline void parsed_dag_node(Interpreter *interp)
{
   interp->state.has_dag_node = true;
}

static inline void parsed_dualequ(Interpreter *interp)
{
   interp->state.has_dualequ = true;
}

static inline void parsed_equilibrium(Interpreter *interp)
{
   interp->state.has_equilibrium = true;
}

static inline void parsed_single_mp(Interpreter *interp)
{
   interp->state.has_single_mp = true;
}

static inline void _single_mp_to_implicit_Nash(Interpreter *interp)
{
   assert(interp->state.has_single_mp);
   interp->state.has_single_mp = false;
   interp->state.has_implicit_Nash = true;
}

static inline bool _has_no_parent(Interpreter *interp)
{
   InterpParsedKwds pk = interp->state;
   return !(pk.has_equilibrium || pk.has_implicit_Nash || pk.has_dag_node ||
            pk.bilevel_in_progress);
}

static inline bool embmode(Interpreter *interp)
{
   return interp->ops && interp->ops->type == InterpreterOpsEmb;
}

static inline bool immmode(Interpreter *interp)
{
   assert(interp->ops);
   return interp->ops->type == InterpreterOpsImm;
}

static inline bool compmode(Interpreter *interp)
{
   assert(interp->ops);
   return interp->ops->type == InterpreterOpsCompiler;
}

typedef int (*interp_ops_generic)(Interpreter * interp, unsigned *p);

static inline int empinterp_ops_dispatch(Interpreter *interp, unsigned *p,
                                         interp_ops_generic fn_imm,
                                         interp_ops_generic fn_vm,
                                         interp_ops_generic fn_emb)
{
   switch (interp->ops->type) {
   case InterpreterOpsImm: return fn_imm(interp, p);
   case InterpreterOpsCompiler: return fn_vm(interp, p);
   case InterpreterOpsEmb: return fn_emb(interp, p);
   default: error("[empinterp] ERROR line %u: dispatch not implemented for ops"
                  "type %d", interp->linenr, interp->ops->type);
      return Error_NotImplemented;
   }
}

#define lexeme_fmtargs(lexeme) (lexeme).len, (lexeme).start
#define ident_fmtargs(ident)   identtype2str((ident)->type), lexeme_fmtargs((ident)->lexeme)
#define tok_fmtargs(tok)       (tok)->len, (tok)->start

#endif // !EMPINTERP_PRIV_H

