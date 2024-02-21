#ifndef EMPPARSER_PRIV_H
#define EMPPARSER_PRIV_H

#include "compat.h"
#include "empinterp.h"
#include "empparser.h"
#include "status.h"

#define PARSER_EXPECTS(_parser, _msg, ...) \
   S_CHECK(tok_expects(&(_parser)->cur, _msg, PP_NARG(__VA_ARGS__), __VA_ARGS__))
#define PARSER_EXPECTS_EXIT(_parser, _msg, ...) \
   S_CHECK_EXIT(tok_expects(&(_parser)->cur, _msg, PP_NARG(__VA_ARGS__), __VA_ARGS__))
#define PARSER_EXPECTS_PEEK(_parser, _msg, ...) \
   S_CHECK(tok_expects(&(_parser)->peek, _msg, PP_NARG(__VA_ARGS__), __VA_ARGS__))

#define _TOK_GETSTRTMP(_tok, _mdl, _tmpstr) \
  struct ctrmem CTRMEM working_mem = {.ptr = NULL, .ctr = &_mdl->ctr}; \
  const char *_ident42 = emptok_getstrstart(_tok); \
  unsigned _ident_len42 = emptok_getstrlen(_tok); \
  A_CHECK(working_mem.ptr, ctr_getmem(&_mdl->ctr, sizeof(char) * (_ident_len42+1))); \
  _tmpstr = (char*)working_mem.ptr; \
  STRNCPY(_tmpstr, _ident42, _ident_len42); \
  _tmpstr[_ident_len42] = '\0';

 /* ---------------------------------------------------------------------
 * Low-level functions. Use with caution!
 * --------------------------------------------------------------------- */

int advance(Interpreter * restrict interp, unsigned * restrict p,
             TokenType *toktype) NONNULL;
int peek(Interpreter *interp, unsigned * restrict p, TokenType *toktype) NONNULL;

int resolve_identas_(Interpreter * restrict interp, IdentData *ident,
                     const char *msg, unsigned argc, ...) NONNULL;

#define RESOLVE_IDENTAS(interp, data, msg, ...) \
   resolve_identas_(interp, data, msg, PP_NARG(__VA_ARGS__), __VA_ARGS__)


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

