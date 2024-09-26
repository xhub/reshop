#ifndef EMPPARSER_H
#define EMPPARSER_H

#include <stdbool.h>

#include "compat.h"
#include "empinterp_fwd.h"
#include "empparser_data.h"
#include "rhp_fwd.h"


/** @file empparser.h
 *
 * @brief EMP parser functions
 */

/* ---------------------------------------------------------------------
 * Prototypes for tokens
 * --------------------------------------------------------------------- */

NONNULL MALLOC_ATTR(free, 1) const char* tok_dupident(const Token *tok);
int tok_expects(const Token *tok, const char *msg, unsigned argc, ...) NONNULL;
int tok_err(const Token *tok, TokenType type_expected, const char *msg) NONNULL;
bool tokisgms(TokenType toktype);
void tok_payloadprint(const Token *tok, unsigned mode,
                       const Model *mdl) NONNULL_AT(1);
void tok_free(Token *tok);
bool tok_untilwsorchar(Token *tok, const char * restrict buf, char c,
                       unsigned * restrict pos) NONNULL;

const char* toktype2str(TokenType type);
int tok2ident(Token * restrict tok, IdentData * restrict ident) NONNULL;

/* ---------------------------------------------------------------------
 * Prototypes for parsing statements
 * --------------------------------------------------------------------- */

int parse_bilevel(Interpreter *interp, unsigned *p) NONNULL;
int parse_equilibrium(Interpreter *interp) NONNULL;
int parse_gdxin(Interpreter* restrict interp, unsigned * restrict p) NONNULL;
int parse_dualequ(Interpreter * restrict interp, unsigned * restrict p) NONNULL;
int parse_dualvar(Interpreter * restrict interp, unsigned * restrict p) NONNULL;
int parse_load(Interpreter* restrict interp, unsigned * restrict p) NONNULL;
int parse_labeldef(Interpreter * restrict interp, unsigned *p) NONNULL;
NONNULL
int labdeldef_parse_statement(Interpreter* restrict interp, unsigned* restrict p);
int parse_label(Interpreter *interp, unsigned *p) NONNULL;
int parse_mp(Interpreter *interp, unsigned *p) NONNULL;
int parse_DAG(Interpreter *interp, unsigned *p) NONNULL;
int parse_Nash(Interpreter *interp, unsigned *p) NONNULL;
int parse_ovf(Interpreter * restrict interp, unsigned * restrict p) NONNULL;

NONNULL int parser_asUEL(Interpreter *interp, unsigned * restrict p, char quote,
                          TokenType *toktype);
NONNULL int parser_peekasUEL(Interpreter *interp, unsigned * restrict p, char quote,
                             TokenType *toktype);


#endif /* EMPPARSER_H */
