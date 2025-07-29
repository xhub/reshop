#ifndef EMPPARSER_UTILS_H
#define EMPPARSER_UTILS_H

#include <stdbool.h>

#include "compat.h"
#include "empinterp_fwd.h"
#include "empparser_data.h"

typedef enum { OperatorKeywordString, OperatorKeywordScalar } OperatorKeywordType;

typedef void (*setter_operator_optchoice)(void *dat, unsigned idx);
typedef void (*setter_operator_double)(void *dat, double val);
typedef void (*setter_operator_uint)(void *dat, unsigned pos);

typedef struct {
   const char *name;
   bool optional;
   OperatorKeywordType type;
   unsigned position;
   union {
      const char * const *kwstrs;
   };
   union {
      setter_operator_optchoice strsetter;
      setter_operator_double dblsetter;
      setter_operator_uint uintsetter;
   };
} OperatorKeyword;

int parse_gmsindices(Interpreter * restrict interp, unsigned * restrict p,
                     GmsIndicesData * restrict gmsindices) NONNULL;
int parse_labeldefindices(Interpreter * restrict interp, unsigned * restrict p,
                          GmsIndicesData * restrict gmsindices);

int resolve_tokasident(Interpreter *interp, IdentData *ident) NONNULL;

NONNULL int parse_operator_kw_args(Interpreter * restrict interp, unsigned * restrict p,
                           unsigned op_kws_size, const OperatorKeyword *op_kws, void *dat);


static inline bool tok_isopeningdelimiter(TokenType toktype)
{
   return (toktype == TOK_LPAREN || toktype == TOK_LBRACK || toktype == TOK_LBRACE);
}

static inline TokenType tok_closingdelimiter(TokenType toktype)
{
   switch (toktype) {
   case TOK_LPAREN: return TOK_RPAREN;
   case TOK_LBRACE: return TOK_RBRACE;
   case TOK_LBRACK: return TOK_RBRACK;

   default:         return TOK_UNSET;
   }
}

#endif
