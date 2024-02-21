#ifndef EMPINTERP_PRIV_H
#define EMPINTERP_PRIV_H

#include "empinterp.h"
#include "printout.h"


#define RHP_LOCAL_SCOPE
#define RHP_LIST_PREFIX gdxreaders
#define RHP_LIST_TYPE gdx_readers
#define RHP_ELT_TYPE GdxReader
#define RHP_ELT_INVALID ((GdxReader) {.fname = NULL, .gdxh = NULL})
#include "list_generic.inc"

int gms_find_ident_in_dct(Interpreter * restrict interp, Token * restrict tok) NONNULL;

void interp_showerr(Interpreter *interp) NONNULL;
int process_statements(Interpreter * restrict interp, unsigned * restrict p,
                        TokenType toktype) NONNULL;

const char * identtype_str(IdentType type);

/* ---------------------------------------------------------------------
 * Misc functions
 * --------------------------------------------------------------------- */

int skip_spaces_commented_lines(Interpreter *interp, unsigned *p) NONNULL;

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

   memcpy(&interp->cur, &interp->pre, sizeof(Token));
   interp->pre.type = TOK_UNSET;
}

NONNULL static inline
void interp_erase_savedtok(Interpreter *interp)
{
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

NONNULL static inline void identdata_init(IdentData *data, unsigned linenr,
                                          const Token * restrict tok)
{
   data->lexeme.linenr = linenr;
   data->lexeme.len = tok->len;
   data->lexeme.start = tok->start;
   data->type = IdentNotFound;
   data->idx = UINT_MAX;
   data->dim = UINT8_MAX;
}

static inline void gms_indicesdata_init(GmsIndicesData *indices)
{
   indices->nargs = 0;
   indices->num_sets = 0;
   indices->num_iterators = 0;
   indices->num_localsets = 0;
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

#endif // !EMPINTERP_PRIV_H

