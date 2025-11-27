#ifndef EMPPARSER_PRIV_H
#define EMPPARSER_PRIV_H

#include "compat.h"
#include "empinterp.h"
#include "empparser.h"
#include "status.h"

#define PARSER_EXPECTS(_interp, _msg, ...) \
   S_CHECK(tok_expects(&(_interp)->cur, _msg,      VA_NARG_TYPED(TokenType, __VA_ARGS__), __VA_ARGS__))
#define PARSER_EXPECTS_EXIT(_interp, _msg, ...) \
   S_CHECK_EXIT(tok_expects(&(_interp)->cur, _msg, VA_NARG_TYPED(TokenType, __VA_ARGS__), __VA_ARGS__))
#define parser_expects_peek(_interp, _msg, ...) \
   S_CHECK(tok_expects(&(_interp)->peek, _msg,     VA_NARG_TYPED(TokenType, __VA_ARGS__), __VA_ARGS__))

#define _TOK_GETSTRTMP(_tok, _mdl, _tmpstr) \
  struct ctrmem CTRMEM working_mem = {.ptr = NULL, .ctr = &_mdl->ctr}; \
  const char *_ident42 = emptok_getstrstart(_tok); \
  unsigned _ident_len42 = emptok_getstrlen(_tok); \
  A_CHECK(working_mem.ptr, ctr_getmem_old(&_mdl->ctr, sizeof(char) * (_ident_len42+1))); \
  _tmpstr = (char*)working_mem.ptr; \
  STRNCPY(_tmpstr, _ident42, _ident_len42); \
  _tmpstr[_ident_len42] = '\0';

NONNULL int parse_identasscalar(Interpreter * interp, unsigned * restrict p,
                        double *val);
int find_uelidx(Interpreter * restrict interp, const char uelstr[VMT(static 1)],
                int * restrict uelidx) NONNULL;


 /* ---------------------------------------------------------------------
 * Low-level functions. Use with caution!
 * --------------------------------------------------------------------- */

#define L(...)  VA_NARG_TYPED(TokenType, __VA_ARGS__), VA_ARRAY_TYPED(TokenType, __VA_ARGS__)

int advance_chk(Interpreter * restrict interp, unsigned * restrict p,
                TokenType *toktype, unsigned nargs, TokenType toktypes[VMT(static nargs)],
                const char *msg) NONNULL;
int advance_nochk(Interpreter * restrict interp, unsigned * restrict p,
             TokenType *toktype) NONNULL;

/* After celebrating the use of a proper compiler, we could define advance as
// Main macro
#define advance(interp, p, toktype,...) advance_ ## __VA_OPT__(chk) (interp, p, toktype \
  __VA_OPT(,) __VA_ARGS__ )
*/

//#define advance_chk advance_chk_
#define advance advance_nochk

int peek(Interpreter *interp, unsigned * restrict p, TokenType *toktype) NONNULL;

int resolve_identas_(Interpreter * restrict interp, IdentData *ident,
                     const char *msg, unsigned argc, ...) NONNULL;

#define resolve_identas(interp, data, msg, ...) \
   resolve_identas_(interp, data, msg, VA_NARG_TYPED(IdentType, __VA_ARGS__), __VA_ARGS__)


NONNULL
static inline int tok_expect(const Token *tok, const char *msg, TokenType type)
{
   if (tok->type == type) return OK;
   return tok_err(tok, type, msg);
}

NONNULL static inline
int parser_expect(const Interpreter *interp, const char *msg, TokenType type)
{
   return tok_expect(&interp->cur, msg, type);
}

NONNULL static inline
int parser_expect_peek(const Interpreter *interp, const char *msg, TokenType type)
{
   return tok_expect(&interp->peek, msg, type);
}

#endif /* EMPPARSER_PRIV_H */

