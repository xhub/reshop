#ifndef EMPPARSER_H
#define EMPPARSER_H

#include <stdbool.h>
#include <stdlib.h>

#include "compat.h"
#include "rhp_fwd.h"

/**
 * @file empparser.h
 *
 * @brief EMP parser functions
 */

typedef enum emptok_type {
   /* Modeling keywords in alphabetical order! Keep in sync with kw_modeling_str */
   TOK_BILEVEL,
   TOK_DAG,
   TOK_DUALEQU,
   TOK_DUALVAR,
   TOK_EQUILIBRIUM,
   TOK_EXPLICIT,
   TOK_IMPLICIT,
   TOK_MAX,
   TOK_MIN,
   TOK_MODELTYPE,
   TOK_MP,
   TOK_NASH,
   TOK_OVF,
   TOK_SHAREDEQU,
   TOK_VALFN,
   TOK_VI,
   TOK_VISOL,
   /* "Other" Keywords (TODO: find a better name) Keep in sync with kw_str  */
   TOK_GDXIN,
   TOK_LOAD,
   TOK_LOOP,
   TOK_SAMEAS,
   TOK_SUM,
   /* Alphabetical order! Keep in sync with probtype_str */
   TOK_DNLP, /* TOKRANGE_PROBTYPE_START */
   TOK_LP,
   TOK_MCP,
   TOK_MINLP,
   TOK_MIP,
   TOK_MIQCP,
   TOK_NLP,
   TOK_QCP,  /* TOKRANGE_PROBTYPE_END */
   /* Start of non-keyword tokens; Keep in sync with tok_str  */
   TOK_LABEL,
   TOK_EQUIL, /* TODO: delete? */
   TOK_IDENT,
   TOK_INTEGER, /* not parsable for now, useful for VM */
   TOK_REAL,
   /* GAMS-specific data */
   TOK_GMS_EQU,
   TOK_GMS_PARAM,
   TOK_GMS_SET,
   TOK_GMS_SYMBOL,
   TOK_GMS_UEL,
   TOK_GMS_VAR,
   /* Operators */
   TOK_DEFVAR,
   /* Conditionals */
   TOK_AND,
   TOK_NOT,
   TOK_OR,
   /* Single token */
   TOK_LANGLE,
   TOK_RANGLE,
   TOK_LPAREN,
   TOK_RPAREN,
   TOK_LBRACK,
   TOK_RBRACK,
   TOK_EQUAL,
   TOK_COMMA,
   TOK_PLUS,
   TOK_MINUS,
   TOK_STAR,
   TOK_SLASH,
   TOK_COLON,
   TOK_DOT,
   TOK_CONDITION,
   TOK_SINGLE_QUOTE,
   TOK_DOUBLE_QUOTE,
   TOK_PERCENT,
   TOK_ERROR,
   TOK_EOF,
   TOK_UNSET,
} TokenType;

#define  TOKRANGE_MDL_KW_START    TOK_BILEVEL
#define  TOKRANGE_MDL_KW_END      TOK_VISOL
#define  TOKLEN_MDL_KW            (TOKRANGE_MDL_KW_END - TOKRANGE_MDL_KW_START + 1)
#define  TOKRANGE_OTHER_KW_START  TOK_GDXIN
#define  TOKRANGE_OTHER_KW_END    TOK_SUM
#define  TOKLEN_OTHER_KW          (TOKRANGE_OTHER_KW_END - TOKRANGE_OTHER_KW_START + 1)
#define  TOKRANGE_PROBTYPE_START  TOK_DNLP
#define  TOKRANGE_PROBTYPE_END    TOK_QCP
#define  TOKLEN_PROBTYPE          (TOKRANGE_PROBTYPE_END - TOKRANGE_PROBTYPE_START + 1)
#define  TOKRANGE_OTHER_START     TOK_LABEL
#define  TOKRANGE_OTHER_END       TOK_UNSET
#define  TOKLEN_OTHER             (TOKRANGE_OTHER_END - TOKRANGE_OTHER_START + 1)

/* TODO: add NODE_DECL, (MIN|MAX|VI) */
enum precedence {
  PREC_NONE,
  PREC_ASSIGNMENT,  // =
  PREC_OR,          // or
  PREC_AND,         // and
  PREC_EQUALITY,    // == !=
  PREC_COMPARISON,  // < > <= >=
  PREC_TERM,        // + -
  PREC_FACTOR,      // * /
  PREC_UNARY,       // ! -
  PREC_CALL,        // . ()
  PREC_PRIMARY
};

typedef struct interpreter Interpreter;
typedef struct emptok Token;

/* ---------------------------------------------------------------------
 * Prototypes for tokens
 * --------------------------------------------------------------------- */

NONNULL MALLOC_ATTR(free, 1) const char* tok_dupident(const Token *tok);
int tok_expects(const struct emptok *tok, const char *msg, unsigned argc, ...) NONNULL;
int tok_err(const struct emptok *tok, TokenType type_expected, const char *msg) NONNULL;
bool tokisgms(TokenType toktype);
void tok_payloadprint(const struct emptok *tok, unsigned mode,
                       const Model *mdl) NONNULL;
void tok_free(Token *tok);
bool tok_untilwsorchar(Token *tok, const char * restrict buf, char c,
                       unsigned * restrict pos) NONNULL;

const char* toktype2str(TokenType type);

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
