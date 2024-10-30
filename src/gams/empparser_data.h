#ifndef EMPPARSER_DATA_H
#define EMPPARSER_DATA_H

/**
 * @file empparser_data.h
 *
 * @brief EMP parser data
 */

typedef enum emptok_type {
   /* Modeling keywords in alphabetical order! Keep in sync with kw_modeling_str */
   TOK_BILEVEL,
   TOK_DAG,
   TOK_DEFFN,       /**< Define a function/mapping                               */
   TOK_DUAL,        /**< dual operator                                           */
   TOK_DUALEQU,
   TOK_DUALVAR,
   TOK_EQUILIBRIUM, /**< equilibrium keyword (JAMS backward compatibility)       */
   TOK_EXPLICIT,
   TOK_IMPLICIT,
   TOK_KKT,
   TOK_MAX,
   TOK_MIN,
   TOK_MODELTYPE,
   TOK_MP,
   TOK_NASH,
   TOK_OBJFN,
   TOK_OVF,
   TOK_SHAREDEQU,
   TOK_SMOOTH,
   TOK_VALFN,       /**< "valfn" attribute                                       */
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
   TOK_GMS_ALIAS,  /* Start of the GAMS keyword */
   TOK_GMS_EQU,
   TOK_GMS_MULTISET,
   TOK_GMS_PARAM,
   TOK_GMS_SET,
   TOK_GMS_UEL,
   TOK_GMS_VAR,     /* End of the GAMS keyword  */
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

#endif
