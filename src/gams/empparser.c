#include "asprintf.h"

#include <stdarg.h>
#include <stdbool.h>
#include <stdio.h>
#include <string.h>

#include "compat.h"
#include "container.h"
#include "ctrdat_gams.h"
#include "empdag.h"
#include "empinfo.h"
#include "empinterp.h"
#include "empinterp_checks.h"
#include "empinterp_edgebuilder.h"
#include "empinterp_priv.h"
#include "empinterp_utils.h"
#include "empinterp_vm_compiler.h"
#include "empparser.h"
#include "empparser_priv.h"
#include "empparser_utils.h"
#include "equvar_metadata.h"
#include "macros.h"
#include "mathprgm.h"
#include "mathprgm_priv.h"
#include "ovfinfo.h"
#include "ovf_fn_helpers.h"
#include "ovf_parameter.h"
#include "printout.h"
#include "rhp_options_data.h"
#include "status.h"
#include "toplayer_utils.h"
#include "win-compat.h"

/**
 * @file empparser.c
 *
 * @brief EMP parser functions
 */

/* --------------------------------------------------------------------------
 * References for this file:
 * - https://github.com/lua/lua/blob/v5.4.3/lparser.c
 * - Blog series https://briancallahan.net/blog/20210814.html and code
 *   https://github.com/ibara/pl0c/blob/main/original.c
 *
 * Pratt parser infos:
 * - large number of links: https://www.oilshell.org/blog/2017/03/31.html
 * - https://craftinginterpreters.com/compiling-expressions.html
 *
 * - v8 parsing: https://github.com/v8/v8/blob/master/src/parsing/scanner-inl.h
 * ------------------------------------------------------------------------- */

static const char * const kw_modeling_str[] = {
   "bilevel",
   "dag",
   "dualequ",
   "dualvar",
   "equilibrium",
   "explicit",
   "implicit",
   "max",
   "min",
   "modeltype",
   "nash",
   "ovf",
   "sharedequ",
   "valFn",
   "VF",
   "VI",
   "visol",
};

static const char * const kw_str[] = {
   "GDXIN",
   "LOAD",
   "LOOP",
   "SAMEAS",
   "SUM",
};

static const char * const probtype_str[] = {
  "dnlp",
  "lp",
  "mcp",
  "minlp",
  "mip",
  "miqcp",
  "nlp",
  "qcp",
};

static const char * const tok_str[] = {
   "LABEL",
   "EQUIL",
   "a string",
   "INTEGER",
   "a number",
   "GAMS equation",
   "GAMS parameter",
   "GAMS set",
   "GAMS symbol",
   "GAMS UEL",
   "GAMS variable",
   "DEFVAR",
   "AND",
   "NOT",
   "OR",
   "LANGLE",
   "RANGLE",
   "LPAREN",
   "RPAREN",
   "LBRACK",
   "RBRACK",
   "EQUAL",
   "COMMA",
   "PLUS",
   "MINUS",
   "STAR",
   "SLASH",
   "COLON",
   "DOT",
   "CONDITION",
   "SINGLE_QUOTE",
   "DOUBLE_QUOTE",
   "PERCENT (%)",
   "ERROR",
   "EOF",
   "UNSET",
};

enum ParseMode { ParseCurrent, ParsePeek };

struct dblobj {
   double val;
   unsigned len;
   bool success;
};


NONNULL static
int parse_dualequ_equvar(Interpreter * restrict interp, unsigned * restrict p);

void  tok_free(Token *tok)
{
   switch (tok->type) {
   case TOK_GMS_VAR:
      avar_empty(&tok->payload.v);
      break;
   case TOK_GMS_EQU:
      aequ_empty(&tok->payload.e);
      break;
   case TOK_LABEL:
      FREE(tok->payload.label);
      break;
   default:
      ;
   }

   FREE(tok->iscratch.data);
   FREE(tok->dscratch.data);
   _tok_empty(tok);
}

const char* tok_dupident(const Token *tok) 
{
   const char *ident = emptok_getstrstart(tok);
   unsigned ident_len = emptok_getstrlen(tok);
   return strndup(ident, ident_len);
}

static inline IdentType gdxsymtype_identtype(GdxSymType type, unsigned dim)
{
   switch(type) {
   case dt_set:
      if (dim == 1) return IdentSet;
      return IdentMultiSet;
   case dt_par:
      if (dim == 0) return IdentScalar;
      if (dim == 1) return IdentVector;
      return IdentParam;
   case dt_var:
      return IdentVar;
   case dt_equ:
      return IdentEqu;
   default:
      return IdentNotFound;
   }
}


NONNULL static void parser_update_last_kw(Interpreter *interp)
{
   struct emptok *tok = &interp->cur;
   assert((tok->type >= TOKRANGE_MDL_KW_START && tok->type <= TOKRANGE_OTHER_KW_END) ||
          tok->type == TOK_IDENT); /* We might be trying to parse a label */

   interp->last_kw_info.type = tok->type;
   interp->last_kw_info.linenr = interp->linenr;
   interp->last_kw_info.linestart = interp->linestart;
   interp->last_kw_info.start = tok->start;
   interp->last_kw_info.len = tok->len;
}

void interp_showerr(Interpreter *interp)
{
   /* ---------------------------------------------------------------------
    * We want to show an error message of the form:
    * 
    *                       v~
    *     nA(a, n): min w + VF('unknown VF', ...)   
    *                           ^~~~~~~~~~
    *
    * For this we need to:
    * 1) Compute the offset from the last keyword to the beginning of its line
    * 2) Compute the offset from the current linestart to the start of the token
    * 3) Print the first line
    * 4) Print lines from the beginning of the last keyword to the end of
    * the current line
    * 5) Print the last line
    * --------------------------------------------------------------------- */

   struct emptok * restrict tok;

   if (interp->peekisactive) {
      tok = &interp->peek;
   } else {
      tok = &interp->cur;
   }

   if (interp->last_kw_info.type == TOK_UNSET || tok->len == 0 || !tok->start) {
      const char * start = interp->linestart;
      if (!tok->start || !start) { goto final_error_line; }

      const char * restrict end = strpbrk(tok->start, "\n");
      if (!end) { end = &tok->start[strlen(tok->start)]; }
      error("[empparser] line %u follows:\n%.*s", interp->linenr, (int)(end-start), start);
      goto final_error_line;
   }

   errormsg("[empparser] The error occurred while parsing the following statement:\n");

   assert(interp->last_kw_info.linestart);
   const char * restrict start = interp->last_kw_info.linestart;
   const char * restrict end = strpbrk(tok->start, "\n");
   if (!end) { end = &tok->start[strlen(tok->start)]; }

   assert(end && end-start < INT_MAX);

   ptrdiff_t offsetptr1 = interp->last_kw_info.start - start;
   assert(offsetptr1 >= 0 && offsetptr1 < INT_MAX);
   unsigned offset1 = offsetptr1;

   ptrdiff_t offsetptr2;
   unsigned offset2;
   if (tok->linenr == interp->linenr) {
      offsetptr2 = tok->start - interp->linestart;
      assert(offsetptr2 >= 0 && offsetptr2 < INT_MAX);
      offset2 = offsetptr2;
   } else if (tok->linenr + 1 == interp->linenr) {
      offsetptr2 = tok->start - interp->linestart_old;
      assert(offsetptr2 >= 0 && offsetptr2 < INT_MAX);
      offset2 = offsetptr2;
   } else {
      offset2 = UINT_MAX;
   }


   /* 'c' specifier does not allow for maximum precision, at least one space is
    * printed */
   if (offset1 > 0) {
      error("%*clast keyword\n", offset1, ' ');
   } else {
      errormsg("last keyword\n");
   }

   error("%*s", offset1+1, "v");
   for (unsigned i = 0; i < interp->last_kw_info.len-1; ++i) errormsg("~");
   errormsg("\n");
   error("%.*s\n", (int)(end-start), start);

   if (offset2 == UINT_MAX) {
      error("[empinterp] ERROR: the line number of the last token is %u, but "
            "the interpreter is at line %u, cannot print detailed information\n",
            tok->linenr, interp->linenr);
      goto final_error_line;
   }

   error("%*s", offset2+1, "^");
   for (unsigned i = 0; i < tok->len-1; ++i) errormsg("~");
   errormsg("\n");

   if (offset2 > 0) {
      error("%*clast lexeme\n", offset2, ' ');
   } else {
      errormsg("last lexeme\n");
   }

   if (interp->pre.type == TOK_UNSET) { goto final_error_line; }

   tok = &interp->pre;
   error("[empinterp] The interpreter has the following saved token from "
         "line %u: ", interp->pre.linenr);

   TokenType type = tok->type;
   if (type == TOK_EOF) {
      errormsg("at the end\n");
   }  else if (type == TOK_ERROR) {
      errormsg("token type is ERROR\n");
   } else {
      error("token '%.*s' of type %s.\n", tok->len, tok->start,
            toktype2str(type));
   }

final_error_line:
   error("[empinterp] ERROR in file '%s' at line %u\n",
         interp->empinfo_fname, interp->linenr);

}

const char * toktype2str(TokenType type)
{
   RESHOP_STATIC_ASSERT(ARRAY_SIZE(kw_modeling_str) == TOKLEN_MDL_KW, "review kw_modeling_str")
   RESHOP_STATIC_ASSERT(ARRAY_SIZE(kw_str) == TOKLEN_OTHER_KW, "review kw_str")
   RESHOP_STATIC_ASSERT(ARRAY_SIZE(probtype_str) == TOKLEN_PROBTYPE, "review kw_str")
   RESHOP_STATIC_ASSERT(ARRAY_SIZE(tok_str) == TOKLEN_OTHER, "review kw_str")

   if (type <= TOKRANGE_MDL_KW_END) return kw_modeling_str[type];
   assert(type >= TOKRANGE_OTHER_KW_START);
   if (type <= TOKRANGE_OTHER_KW_END) return kw_str[type - TOKRANGE_OTHER_KW_START];
   assert(type >= TOKRANGE_PROBTYPE_START);
   if (type <= TOKRANGE_PROBTYPE_END) return probtype_str[type - TOKRANGE_PROBTYPE_START];
   assert(type >= TOKRANGE_OTHER_START);
   if (type <= TOKRANGE_OTHER_END) return tok_str[type - TOKRANGE_OTHER_START];

   return "ERROR: token type out of range";
}

void tok_payloadprint(const Token *tok, unsigned mode,
                       const Model *mdl)
{
   switch (tok->type) {
   case TOK_GMS_EQU:
      if (tok->gms_dct.read) {
         aequ_printnames(&tok->payload.e, mode, mdl);
      } else {
         printstr(mode, "\n");
      }
      break;
   case TOK_GMS_VAR:
      if (tok->gms_dct.read) {
         avar_printnames(&tok->payload.v, mode, mdl);
      } else {
         printstr(mode, "\n");
      }
      break;
   case TOK_REAL:
      printout(mode, " %e\n", tok->payload.real);
      break;
   case TOK_INTEGER:
      printout(mode, " %d\n", tok->payload.idx);
      break;
   default:
      printstr(mode, "\n");
   }
}

UNUSED NONNULL static void _tok_print(Token *tok, unsigned mode)
{
   printout(mode, "token '%.*s' of type %s at line %u", tok->len, tok->start,
            toktype2str(tok->type), tok->linenr);
}

static enum mdl_type tok2probtype(TokenType toktype)
{
   enum mdl_type type;
   switch (toktype) {
   case TOK_DNLP:
      type = MdlType_dnlp;
      break;
   case TOK_LP:
      type = MdlType_lp;
      break;
   case TOK_MCP:
      type = MdlType_mcp;
      break;
   case TOK_MINLP:
      type = MdlType_minlp;
      break;
   case TOK_MIP:
      type = MdlType_mip;
      break;
   case TOK_MIQCP:
      type = MdlType_miqcp;
      break;
   case TOK_NLP:
      type = MdlType_nlp;
      break;
   case TOK_QCP:
      type = MdlType_qcp;
      break;
   default:
      type = MdlType_none;
   }
   return type;
}

/**
 * @brief Parse a real number
 *
 * @param str  the string representation of the real number
 * @param obj  the resulting object
 *
 * @return the error code
 */
NONNULL static int _real(const char *str, void *obj)
{
   char* end;
   struct dblobj *o = (struct dblobj *)obj;
   errno = 0;

   double val = strtod(str, &end);

   if (val != 0. && errno) {
      perror("strtod");
      error("%s :: strtod() error, see above\n", __func__);
      return Error_SystemError;
   }

  /* ------------------------------------------------------------------------
   * Has there been any conversion here? The manual says that in that case the
   * returned value should be 0
   * ------------------------------------------------------------------------ */
   if (str == end) {
      assert(val == 0.);
      o->success = false;
      return OK;
   }

   o->val = val;
   o->len = end - str;
   o->success = true;

   return OK;
}

//NOLINTBEGIN
static inline int _string(const char *str, void *strcopy)
{
  TO_IMPLEMENT("string parse");
}

static inline int _grouping(const char *str, void *ttt)
{
  TO_IMPLEMENT("grouping parse");
}

static inline int _unary(const char *str, void *ttt)
{
  TO_IMPLEMENT("unary parse");
}

static inline int _binary(const char *str, void *ttt)
{
  TO_IMPLEMENT("binary parse");
}
//NOLINTEND

typedef int (*parse_fn)(const char *, void *);

struct parse_rules {
   parse_fn prefix;
   parse_fn infix;
   enum precedence prec;
};

const struct parse_rules rules[] = {
   /* Modeling keywords */
   [TOK_BILEVEL]       = {NULL,      NULL,     PREC_NONE},
   [TOK_DAG]           = {NULL,      NULL,     PREC_NONE},
   [TOK_DUALEQU]       = {NULL,      NULL,     PREC_NONE},
   [TOK_DUALVAR]       = {NULL,      NULL,     PREC_NONE},
   [TOK_EQUILIBRIUM]   = {NULL,      NULL,     PREC_NONE},
   [TOK_EXPLICIT]      = {NULL,      NULL,     PREC_NONE},
   [TOK_IMPLICIT]      = {NULL,      NULL,     PREC_NONE},
   [TOK_MAX]           = {NULL,      NULL,     PREC_NONE},
   [TOK_MIN]           = {NULL,      NULL,     PREC_NONE},
   [TOK_NASH]          = {NULL,      NULL,     PREC_NONE},
   [TOK_OVF]           = {NULL,      NULL,     PREC_NONE},
   [TOK_SHAREDEQU]     = {NULL,      NULL,     PREC_NONE},
   [TOK_VALFN]         = {NULL,      NULL,     PREC_NONE},
   [TOK_VF]            = {NULL,      NULL,     PREC_NONE},
   [TOK_VI]            = {NULL,      NULL,     PREC_NONE},
   [TOK_VISOL]         = {NULL,      NULL,     PREC_NONE},
   /* General keywords */
   [TOK_GDXIN]         = {NULL,      NULL,     PREC_NONE},
   [TOK_LOAD]          = {NULL,      NULL,     PREC_NONE},
   [TOK_LOOP]          = {NULL,      NULL,     PREC_NONE},
   [TOK_SAMEAS]        = {NULL,      NULL,     PREC_NONE},
   [TOK_SUM]           = {NULL,      NULL,     PREC_NONE},
   /* Problem type keywords */
   [TOK_DNLP]          = {NULL,      NULL,     PREC_NONE},
   [TOK_LP]            = {NULL,      NULL,     PREC_NONE},
   [TOK_MCP]           = {NULL,      NULL,     PREC_NONE},
   [TOK_MIP]           = {NULL,      NULL,     PREC_NONE},
   [TOK_MINLP]         = {NULL,      NULL,     PREC_NONE},
   [TOK_MIQCP]         = {NULL,      NULL,     PREC_NONE},
   [TOK_NLP]           = {NULL,      NULL,     PREC_NONE}, 
   [TOK_QCP]           = {NULL,      NULL,     PREC_NONE},
    /* Start of non-keyword tokens */
   [TOK_LABEL]         = {NULL,      NULL,     PREC_NONE},
   [TOK_EQUIL]         = {NULL,      NULL,     PREC_NONE},
   [TOK_IDENT]         = {_string,   NULL,     PREC_NONE},
   [TOK_INTEGER]       = {NULL,      NULL,     PREC_NONE},
   [TOK_REAL]          = {_real,     NULL,     PREC_NONE},
   [TOK_GMS_EQU]       = {NULL,      NULL,     PREC_NONE},
   [TOK_GMS_PARAM]     = {NULL,      NULL,     PREC_NONE},
   [TOK_GMS_SET]       = {NULL,      NULL,     PREC_NONE},
   [TOK_GMS_SYMBOL]    = {NULL,      NULL,     PREC_NONE},
   [TOK_GMS_UEL]       = {NULL,      NULL,     PREC_NONE}, 
   [TOK_GMS_VAR]       = {NULL,      NULL,     PREC_NONE}, 
   [TOK_DEFVAR]        = {NULL,      NULL,     PREC_NONE},
   [TOK_AND]           = {NULL,      NULL,     PREC_NONE},
   [TOK_NOT]           = {NULL,      NULL,     PREC_NONE},
   [TOK_OR]            = {NULL,      NULL,     PREC_NONE},
   [TOK_LANGLE]        = {NULL,      NULL,     PREC_NONE},
   [TOK_RANGLE]        = {NULL,      NULL,     PREC_NONE},
   [TOK_LPAREN]        = {_grouping, NULL,     PREC_CALL},
   [TOK_RPAREN]        = {NULL,      NULL,     PREC_NONE},
   [TOK_LBRACK]        = {NULL,      NULL,     PREC_NONE},
   [TOK_RBRACK]        = {NULL,      NULL,     PREC_NONE},
   [TOK_EQUAL]         = {NULL,      NULL,     PREC_NONE},
   [TOK_COMMA]         = {NULL,      NULL,     PREC_NONE},
   [TOK_PLUS]          = {NULL,      _binary,  PREC_TERM},
   [TOK_MINUS]         = {_unary,    _binary,  PREC_TERM},
   [TOK_STAR]          = {NULL,      _binary,  PREC_FACTOR},
   [TOK_SLASH]         = {NULL,      _binary,  PREC_FACTOR},
   [TOK_COLON]         = {NULL,      NULL,     PREC_NONE},
   [TOK_DOT]           = {NULL,      NULL,     PREC_NONE},
   [TOK_CONDITION]     = {NULL,      NULL,     PREC_NONE},
   [TOK_SINGLE_QUOTE]  = {NULL,      NULL,     PREC_NONE},
   [TOK_DOUBLE_QUOTE]  = {NULL,      NULL,     PREC_NONE},
   [TOK_PERCENT]       = {NULL,      NULL,     PREC_NONE},
   [TOK_ERROR]         = {NULL,      NULL,     PREC_NONE},
   [TOK_EOF]           = {NULL,      NULL,     PREC_NONE},
   [TOK_UNSET]         = {NULL,      NULL,     PREC_NONE},
};

static_assert(sizeof(rules)/sizeof(rules[0]) == 1+TOK_UNSET,
              "Check the sizes of rules and TokenType");

static inline void _tok_defvar(Token *tok, const char *str)
{
   tok->start = str;
   tok->len = 2;

   tok->type = TOK_DEFVAR;
}

static inline int _tok_one(Token *tok, const char *str)
{
   tok->start = str;
   tok->len = 1;

   switch (str[0]) {
   case '<':
      tok->type = TOK_LANGLE;
      break;
   case '>':
      tok->type = TOK_RANGLE;
      break;
   case '(':
      tok->type = TOK_LPAREN;
      break;
   case ')':
      tok->type = TOK_RPAREN;
      break;
   case '[':
      tok->type = TOK_LBRACK;
      break;
   case ']':
      tok->type = TOK_RBRACK;
      break;
   case '=':
      tok->type = TOK_EQUAL;
      break;
   case ',':
      tok->type = TOK_COMMA;
      break;
   case '+':
      tok->type = TOK_PLUS;
      break;
   case '-':
      tok->type = TOK_MINUS;
      break;
   case '*':
      tok->type = TOK_STAR;
      break;
   case '/':
      tok->type = TOK_SLASH;
      break;
   case ':':
      tok->type = TOK_COLON;
      break;
   case '.':
      tok->type = TOK_DOT;
      break;
   case '$':
      tok->type = TOK_CONDITION;
      break;
   case '"':
      tok->type = TOK_DOUBLE_QUOTE;
      break;
   case '\'':
      tok->type = TOK_SINGLE_QUOTE;
      break;
   case '%':
      tok->type = TOK_PERCENT;
      break;
   default:
      error("Error: unrecognized token %c\n", str[0]);
      tok->type = TOK_ERROR;
      return Error_EMPIncorrectSyntax;
   }

   return OK;
}

static inline unsigned _toklen(const char *str)
{
   unsigned len = 0;
   while (!(isspace(str[len]) || str[len] == EOF)) {
      len++;
   }

   return len;
}

static inline int _eof(Token *tok)
{
   tok->type = TOK_EOF;
   tok->len  = 0;

   return OK;
}

static inline bool parser_skipwsuntilEOL(Interpreter *interp, unsigned * restrict p)
{
   unsigned _p;
   const char * restrict buf = interp->buf;

   _p = (*p);

   while ((buf[_p] != '\0' && buf[_p] != EOF) && isspace(buf[_p]) && buf[_p] != '\n') _p++;

   (*p) = _p;
   return buf[_p] == '\0' || buf[_p] == EOF;
}

static inline bool parser_skipws_peek(const Interpreter *interp, unsigned * restrict p)
{
   unsigned _p;
   const char * restrict buf = interp->buf;

   _p = (*p);

   while (buf[_p] != '\0' && isspace(buf[_p]) && buf[_p] != '\n') _p++;

   while (buf[_p] == '\n') {
      _p++;

      /* In peek mode we don't want to change things the state of interp */

      while (buf[_p] != '\0' && isspace(buf[_p]) && buf[_p] != '\n') _p++;

      /* -------------------------------------------------------------------
       * Skip commented line
       * ------------------------------------------------------------------- */
      if (buf[_p] == '*') {
         while (buf[_p] != '\0' && buf[_p] != '\n') _p++;
      }
   }

   (*p) = _p;
   return buf[_p] == '\0' || buf[_p] == EOF;
}

static inline bool parser_skipws(Interpreter *interp, unsigned * restrict p)
{
   unsigned _p;
   const char * restrict buf = interp->buf;

   _p = (*p);

   while (buf[_p] != '\0' && isspace(buf[_p]) && buf[_p] != '\n') _p++;

   while (buf[_p] == '\n') {
      _p++;

      interp->linenr++;
      interp->linestart_old = interp->linestart;
      interp->linestart = &buf[_p];

      while (buf[_p] != '\0' && isspace(buf[_p]) && buf[_p] != '\n') _p++;

      /* -------------------------------------------------------------------
       * Skip commented line
       * ------------------------------------------------------------------- */
      if (buf[_p] == '*') {
         while (buf[_p] != '\0' && buf[_p] != '\n') _p++;
      }
   }

   (*p) = _p;
   return buf[_p] == '\0' || buf[_p] == EOF;
}

/**
 * @brief skip over the indices of a GAMS symbol of node
 *
 * @param         buf  the in-memory empinfo content
 * @param[in,out] pos  the position in the buffer
 *
 * @return             the error code
 */
static bool skip_gmsindices(const char * restrict buf, unsigned *pos)
{
   unsigned p = *pos;

   /* quoted UELs can contain ')'. Hence, we record whether we have one or not*/
   bool in_squote = false, in_dquote = false;

   while (buf[p] != '\0' && (in_squote || in_dquote || buf[p] != ')')) {
      char c = buf[p];
      if (c == '\'' && !in_dquote) {
         in_squote = !in_squote;
      }
      if (c == '"' && !in_squote) {
         in_dquote = !in_dquote;
      }
      p++;
   }

   *pos = p;

   return buf[p] == '\0' || buf[p] == EOF;
}

/**
 * @brief skip over a conditional statement
 *
 * @param         buf  the in-memory empinfo content
 * @param[in,out] pos  the position in the buffer
 *
 * @return             the error code
 */
static bool skip_conditional(const char * restrict buf, unsigned *pos)
{
   unsigned p = *pos;

   /* quoted UELs can contain '(' or )'. Hence, we record whether we have one or not*/
   bool in_squote = false, in_dquote = false;

   assert(buf[p-1] == '(');

   unsigned n_parent = 1;

   while (buf[p] != '\0' && n_parent > 0) {
      p++;
      char c = buf[p];
      if (c == '\'' && !in_dquote) {
         in_squote = !in_squote;
      } else if (c == '"' && !in_squote) {
         in_dquote = !in_dquote;
      } else if (!in_dquote && !in_squote) {
         if (c == ')') {
            n_parent--;
         } else if (c == '(') {
            n_parent++;
         }
      }
   }

   *pos = p;

   return buf[p] == '\0' || buf[p] == EOF;
}
static bool _tok_alnum(Token *tok, const char * restrict buf, unsigned *pos)
{
  /* -----------------------------------------------------------------------
   * Get the alphanumeric string
   * ----------------------------------------------------------------------- */

   unsigned lpos = *pos;
   unsigned pos_ = lpos;

   while (isalnum(buf[lpos]) || buf[lpos] == '_') lpos++;

   unsigned len = lpos - pos_;
   *pos = lpos;
   tok->len = len;
   tok->start = &buf[pos_];

   return buf[lpos] == '\0' || buf[lpos] == EOF;
}

static bool _tok_untilws(Token *tok, const char * restrict buf, unsigned * restrict pos)
{
   unsigned lpos = *pos;
   unsigned pos_ = lpos;

   while (!isspace(buf[lpos]) && buf[lpos] != EOF &&  buf[lpos] != '\0') lpos++;

   unsigned len = lpos - pos_;
   *pos = lpos;
   tok->len = len;
   tok->start = &buf[pos_];

   return buf[lpos] == EOF ||  buf[lpos] == '\0';
}

UNUSED static bool _tok_untilEOL(Token *tok, const char * restrict buf, unsigned * restrict pos)
{
   unsigned lpos = *pos;
   unsigned pos_ = lpos;

   while (buf[lpos] != '\n' && buf[lpos] != EOF &&  buf[lpos] != '\0') lpos++;

   unsigned len = lpos - pos_;
   *pos = lpos;
   tok->len = len;
   tok->start = &buf[pos_];

   return buf[lpos] == EOF ||  buf[lpos] == '\0';
}

/**
 * @brief Advance until a character is found is found
 *
 * @param         tok  the token to update
 * @param         buf  the buffer
 * @param         c    the character to find
 * @param[in,out] pos  on input, the current position. On output the position of
 *                     the next occurence of the character
 *
 * @return             true if the end of the buffer
 */
NONNULL
static bool tok_untilchar(Token *tok, const char * restrict buf, char c,
                          unsigned * restrict pos)
{
   unsigned lpos = *pos;
   unsigned pos_ = lpos;

   while (buf[lpos] != '\n' && buf[lpos] != c && buf[lpos] != EOF &&  buf[lpos] != '\0') lpos++;

   unsigned len = lpos - pos_;
   *pos = lpos;
   tok->len = len;
   tok->start = &buf[pos_];

   return buf[lpos] == EOF ||  buf[lpos] == '\0';
}

bool tok_untilwsorchar(Token *tok, const char * restrict buf, char c,
                       unsigned * restrict pos)
{
   unsigned lpos = *pos;
   unsigned pos_ = lpos;

   while (!isspace(buf[lpos]) && buf[lpos] != EOF && buf[lpos] != '\0' && buf[lpos] != c) lpos++;

   unsigned len = lpos - pos_;
   *pos = lpos;
   tok->len = len;
   tok->start = &buf[pos_];

   return buf[lpos] == EOF ||  buf[lpos] == '\0';
}

static void _chk_kw(const char *str1, const char *str2, unsigned len,
                   Token *tok, TokenType type)
{
   if (!strncasecmp(str1, str2, len)) {
      emptok_settype(tok, type);
   }
}

static int chk_wildcard_vars_allowed(Interpreter *interp)
{
   S_CHECK(old_style_check(interp));

   mpid_t mpid = interp->finalize.mp_owns_remaining_vars;
   if (!mpid_regularmp(mpid)) { return OK; }

   error("[empinterp] ERROR line %u: MP(%s) is already owning the variables not "
         "explicitely assigned. It is ill-defined to have more than 1 such MP\n",
         interp->linenr, empdag_getmpname(&interp->mdl->empinfo.empdag, mpid));

   return Error_EMPIncorrectInput;
}

static int chk_wildcard_equs_allowed(Interpreter *interp)
{
   S_CHECK(old_style_check(interp));

   mpid_t mpid = interp->finalize.mp_owns_remaining_equs;
   if (!mpid_regularmp(mpid)) { return OK; }

   error("[empinterp] ERROR line %u: MP(%s) is already owning the equations not "
         "explicitely assigned. It is ill-defined to have more than 1 such MP\n",
         interp->linenr, empdag_getmpname(&interp->mdl->empinfo.empdag, mpid));

   return Error_EMPIncorrectInput;
}

/** @brief Parse an alphanumeric sequence
 *
 * @param tok 
 * @param buf 
 * @param pos 
 * @param interp 
 * @return 
 */
static int tok_alphanum(Token *tok, const char * restrict buf, unsigned *pos,
                        Interpreter *interp)
{
  /* -----------------------------------------------------------------------
   * We seek to parse the string token in the following order:
   * 1. empinfo keywords: modeling / problem type / others
   * 2. GAMS symbol: variable/equation/set name,
   * 3. Leave the token type to TOK_IDENT. Could be label, local var, ...
   * ----------------------------------------------------------------------- */

   unsigned lpos = *pos;
   unsigned pos_ = lpos;

   while (isalnum(buf[lpos]) || buf[lpos] == '_') lpos++;

   unsigned len = lpos - pos_;
   *pos = lpos;
   tok->len = len;
   tok->start = &buf[pos_];

   /* By default we set the type to IDENT to ease the logic */
   emptok_settype(tok, TOK_IDENT);

   /* All keywords have at least 2 letters */
   if (len == 1) goto is_gms_ident;

   /* See https://craftinginterpreters.com/scanning-on-demand.html */
   switch (buf[pos_]) {
   case 'a': /* and */
   case 'A':
      if (len == 3) _chk_kw("nd", &buf[pos_+1], 2, tok, TOK_AND);
      break;

   case 'b': /* bilevel */
   case 'B':
      if (len == 7) _chk_kw("ilevel", &buf[pos_+1], 6, tok, TOK_BILEVEL);
      break;

   case 'd': /* dag, dualequ, dualvar | dnlp */
   case 'D':
      if (len >= 3) {
            if (tolower(buf[pos_+1]) == 'a' && len == 3) {
               _chk_kw("g", &buf[pos_+2], 1, tok, TOK_DAG);
            } else if (len == 7 && !strncmp("ual", &buf[pos_+1], 3)) {
               switch (buf[pos_+4]) {
               case 'e': case 'E': /* dualequ */
                  _chk_kw("qu", &buf[pos_+5], 2, tok, TOK_DUALEQU);
                  break;
               case 'v': case 'V':  /* dualvar */
                  _chk_kw("ar", &buf[pos_+5], 2, tok, TOK_DUALVAR);
                  break;
               default: ;
               }
            } else if (tolower(buf[pos_+1]) == 'n' && len == 4) {
               _chk_kw("lp", &buf[pos_+2], 2, tok, TOK_DNLP);
            }
         }
      break;

   case 'e': /* equilibrium, explicit */
   case 'E':
      if (len >= 8) {
         switch (buf[pos_+1]) {
         case 'q': case 'Q':  /* equilibrium */
            if (len == 11) _chk_kw("uilibrium", &buf[pos_+2], 9, tok, TOK_EQUILIBRIUM);
            break;

         case 'x': case 'X': /* explicit */
            if (len == 8) _chk_kw("plicit", &buf[pos_+2], 6, tok, TOK_EXPLICIT);
            break;

         default: ;
         }
      }
      break;

   case 'g': /* gdxin */
   case 'G':
      if (len == 5) _chk_kw("dxin", &buf[pos_+1], 4, tok, TOK_GDXIN);
      break;

   case 'i': /* implicit */
   case 'I':
      if (len == 8) _chk_kw("mplicit", &buf[pos_+1], 7, tok, TOK_IMPLICIT);
      break;

   case 'l': /* lp, load, loop */
   case 'L':
      if (len == 2) _chk_kw("p", &buf[pos_+1], 1, tok, TOK_LP);
      if (len == 4) {
         _chk_kw("oop", &buf[pos_+1], 3, tok, TOK_LOOP);
         if (tok->type == TOK_IDENT) {
             _chk_kw("oad", &buf[pos_+1], 3, tok, TOK_LOAD);
         }
      }
      break;

   case 'm': /* max, min | mcp, minlp, mip, miqcp */
   case 'M':
      if (len >= 3) {
         switch (buf[pos_+1]) {
         case 'A':
         case 'a': if (len == 3) _chk_kw("x", &buf[pos_+2], 1, tok, TOK_MAX);
            break;
         case 'C':
         case 'c': if (len == 3) _chk_kw("p", &buf[pos_+2], 1, tok, TOK_MCP);
            break;
         case 'I':
         case 'i': if (len == 3) { /*  (min|mip)  */
                  switch (buf[pos_+2]) {
                  case 'n': case 'N': emptok_settype(tok, TOK_MIN); return OK;
                  case 'p': case 'P': emptok_settype(tok, TOK_MIP); return OK;
                  default: ;
                  }
               } else if (len == 5) {
                  switch (buf[pos_+2]) {
                  case 'n': case 'N': _chk_kw("lp", &buf[pos_+3], 2, tok, TOK_MINLP);
                  break;
                  case 'q': case 'Q': _chk_kw("cp", &buf[pos_+3], 2, tok, TOK_MIQCP);
                  break;
                  default: ;
                  }
               }
            break;
         case 'O':
         case 'o': if (len == 9) { _chk_kw("deltype", &buf[pos_+2], 7, tok, TOK_MODELTYPE); }
            break;
         default: ;
         }
      }
      break;

   case 'n': /* nash | nlp */
   case 'N':
      if (len >= 3) {
         switch (buf[pos_+1]) {
         case 'a': case 'A': if (len == 4) _chk_kw("sh", &buf[pos_+2], 2, tok, TOK_NASH);
            break;
         case 'o': case 'O': if (len == 3) _chk_kw("t", &buf[pos_+2], 1, tok, TOK_NOT);
            break;
         case 'l': case 'L': if (len == 3) _chk_kw("p", &buf[pos_+2], 1, tok, TOK_NLP);
            break;
        default: ;
         }
      }
      break;
   case 'o': /* or, ovf */
   case 'O':
      if (len == 2) _chk_kw("r", &buf[pos_+1], 1, tok, TOK_OR);
      if (len == 3) _chk_kw("vf", &buf[pos_+1], 2, tok, TOK_OVF);
      break;
   case 'q': /* qcp */
   case 'Q':
      if (len == 3) _chk_kw("cq", &buf[pos_+1], 2, tok, TOK_QCP);
      break;
   case 's': /* sharedequ, sum */
   case 'S':
      if (len == 9) _chk_kw("haredequ", &buf[pos_+1], 8, tok, TOK_SHAREDEQU);
      if (len == 6) _chk_kw("ameas", &buf[pos_+1], 5, tok, TOK_SAMEAS);
      if (len == 3) _chk_kw("um", &buf[pos_+1], 2, tok, TOK_SUM);
      break;
   case 'v': /* valfn, vi, visol */
   case 'V':
      switch (buf[pos_+1]) {
      case 'a':
      case 'A': if (len == 5) _chk_kw("lfn", &buf[pos_+2], 3, tok, TOK_VALFN);
         break;
      case 'f':
      case 'F': if (len == 2) { emptok_settype(tok, TOK_VF); return OK; }
         break;
      case 'I':
      case 'i': if (len == 2) { emptok_settype(tok, TOK_VI); return OK; }
                if (len == 5) _chk_kw("sol", &buf[pos_+2], 3, tok, TOK_VISOL);
         break;
      default: ;
      }
      break;

   default: ;
   }

   if (emptok_gettype(tok) != TOK_IDENT) goto _exit;

is_gms_ident:
   /* TODO: if we support either gdxin or variable creation using ':=', we should
    * look this this up here. For variable creation, we need to have a concept
    * of scope. We want to inherit the variables in the previous scope, and
    * deallocate these as soon as we leave the scope. Also, the variables
    * are immutable
    *
    * TODO: even for supporting loop/sum we need some concept of variables and
    * scoping, as in a loop/sum we operate over a set, but then each time
    * said set appears, it should be substituted by the elements.
    * */

   /* ---------------------------------------------------------------------
    * In this step, we determine if the token is a GAMS object
    * --------------------------------------------------------------------- */
   S_CHECK(gms_find_ident_in_dct(interp, tok));

   if (tok->type != TOK_IDENT) goto _exit;

   /* ---------------------------------------------------------------------
    * In this step, we try to parse the token as a label definition or reference
    * --------------------------------------------------------------------- */


_exit:
   return OK;
}

static int tok_digit(Token *tok, const char *line, unsigned *pos)
{
   struct dblobj o = {.success = false };
   S_CHECK(_real(line, &o));

   if (!o.success) {
      tok->type = TOK_UNSET;
      return OK;
   }

   tok->type = TOK_REAL;
   tok->payload.real = o.val;
   tok->len = o.len;
   tok->start = line;

   *pos += o.len;
   return OK;
}

static int _lexer(Interpreter *interp, enum ParseMode mode, unsigned *pos)
{
   int status = OK;
   const char *line = interp->buf;
   Token * restrict tok;
   bool eof;

   switch (mode) {
   case ParseCurrent:
      tok = &interp->cur;
      eof = parser_skipws(interp, pos);
      break;
   case ParsePeek:
      tok = &interp->peek;
      eof = parser_skipws_peek(interp, pos);
      break;
   default:
      return runtime_error(interp->linenr);
   }

   unsigned p = *pos;

   tok->linenr = interp->linenr;

   if (eof) {
      status = _eof(tok);
      goto _exit;
   }

   if (isdigit(line[p]) ||  (line[p] == '-' && isdigit(line[p+1]))) {
      S_CHECK(tok_digit(tok, &line[p], pos));
      if (tok->type == TOK_REAL) goto _exit;
   }

   if (isalnum(line[p]) || line[p] == '_') {
      status = tok_alphanum(tok, line, pos, interp);
      goto _exit;
   }

   switch (line[p]) {
   case ':':
      if (line[p+1] == '=') {
         _tok_defvar(tok, &line[p]);
         *pos = p+2;
         goto _exit;
      }
   FALLTHRU
   case '<':
   case '>':
   case '(':
   case ')':
   case '[':
   case ']':
   case '=':
   case ',':
   case '+':
   case '-':
   case '*':
   case '/':
   case '.':
   case '$':
   case '"':
   case '\'':
   case '%':
      *pos = p + 1;
      status = _tok_one(tok, &line[p]);
      goto _exit;
   default:
   {
      unsigned len = _toklen(&line[p]);
      error("[empparser] ERROR file '%s': unknown token '%.*s'\n",
            interp->empinfo_fname, len, &line[p]);
      return tok_err(tok, TOK_UNSET, "unknown token in lexer");
   }
   }

_exit:
   if (O_Output & PO_TRACE_EMPPARSER) {
      trace_empparser("[empparser] Token '%.*s' (%s)", tok->len, &line[p],
                      toktype2str(tok->type));
      if (mode == ParsePeek) {
         trace_empparsermsg(" (peek) ");
      }
      tok_payloadprint(tok, PO_TRACE_EMPPARSER, interp->mdl);
   }

   return status;
}

int peek(Interpreter *interp, unsigned * restrict p, TokenType *toktype)
{
   S_CHECK(_lexer(interp, ParsePeek, p));
   *toktype = parser_getpeektoktype(interp);
   return OK;
}

int tok_expects(const Token *tok, const char *msg, unsigned argc, ...)
{
   int status = OK;
   va_list ap, ap_copy;

   va_start(ap, argc);
   va_copy(ap_copy, ap);

   TokenType toktype = tok->type;

   for (size_t i = 0; i < argc; ++i) {
      TokenType type = (TokenType)va_arg(ap, unsigned);
      if (toktype == type) {
         goto _success;
      }
   }

   status = Error_EMPIncorrectSyntax;
   error("[empparser] ERROR line %u: got the token '%.*s' of type '%s', but expected any of ",
         tok->linenr, tok->len, tok->start, toktype2str(toktype));

   bool first = true;
   for (size_t i = 0; i < argc; ++i) {
      TokenType type = (TokenType)va_arg(ap_copy, unsigned);
      if (!first) {
         printstr(PO_ERROR, ", ");
      } else {
         first = false;
      }

      error("'%s'", toktype2str(type));
   }

   error(".\n[empparser] error msg is: %s\n", msg);

_success:
   va_end(ap);
   va_end(ap_copy);

   return status;
}

int tok_err(const Token *tok, TokenType type_expected, const char *msg)
{
   int offset;
   error("[empinterp] %nError line %u: ", &offset, tok->linenr);

   TokenType type = tok->type;
   if (type == TOK_EOF) {
      printstr(PO_ERROR, "at the end\n");
   }  else if (type == TOK_ERROR) {
      printstr(PO_ERROR, "token type is ERROR\n");
   }  else if (type_expected != TOK_UNSET) {
      error("while parsing '%.*s': expecting %s, got %s.\n",
            tok->len, tok->start, toktype2str(type_expected), toktype2str(type));
   } else {
      error("got unexpected %s '%.*s'\n", toktype2str(type),
            tok->len, tok->start);
   }

   error("%*sError msg: %s\n", offset, "", msg);
   return Error_EMPIncorrectSyntax;
}

bool tokisgms(TokenType toktype)
{
   return (toktype >= TOK_GMS_EQU && toktype <= TOK_GMS_VAR);
}

static inline bool toktype_dagnode_kw(TokenType toktype)
{
   return (toktype == TOK_MIN || toktype == TOK_MAX || toktype == TOK_VI);
}


/**
 * @brief Try to resolve ident as a gams symbol
 *
 * @param interp 
 * @param tok 
 * @return 
 */
int gms_find_ident_in_dct(Interpreter * restrict interp, Token * restrict tok)
{
   Model *mdl = interp->mdl;
   GmsContainerData *gms = (GmsContainerData*)mdl->ctr.data;

   struct gms_dct *gms_dct = &tok->gms_dct;
   char *sym_name;
   int symindex, uelindex;

   if (emptok_getstrlen(tok) >= GMS_SSSIZE) { goto _not_found; }

   { _TOK_GETSTRTMP(tok, mdl, sym_name); }

   symindex = dctSymIndex(gms->dct, sym_name);
   if (symindex <= 0) { /* We do not fail here as it could be a UEL */
      uelindex = dctUelIndex(gms->dct, sym_name);
      if (uelindex <= 0) goto _not_found;

      tok->type = TOK_GMS_UEL;
      gms_dct->idx = uelindex;

      return OK;
   }

   gms_dct->idx = symindex;

   /* ---------------------------------------------------------------------
    * Save the dimension of the symbol index
    * --------------------------------------------------------------------- */

   /* What does a negative value of symdim means? */
   int symdim = MAX(dctSymDim(gms->dct, symindex), 1);
   gms_dct->dim = symdim;

   /* ---------------------------------------------------------------------
    * Save the type of the symbol
    * --------------------------------------------------------------------- */

   int symtype = dctSymType(gms->dct, symindex);
   gms_dct->type = symtype;
   switch (symtype) {
   case dctvarSymType:
      tok->type = TOK_GMS_VAR;
      break;
   case dcteqnSymType:
      tok->type = TOK_GMS_EQU;
      break;
   case dctsetSymType:
      tok->type = TOK_GMS_SET;
      break;
   case dctparmSymType: /* We should error here */
      tok->type = TOK_GMS_PARAM;
      break;
   case dctfuncSymType:
   case dctacrSymType:
   default:
      tok->type = TOK_GMS_SYMBOL;
   }

   gms_dct->read = false;

   return OK;

_not_found:
   gms_dct->idx = IdxNotFound;
   tok->type = TOK_IDENT;

   return OK;
}

int advance(Interpreter * restrict interp, unsigned * restrict p,
             TokenType *toktype)
{
   int status = _lexer(interp, ParseCurrent, p);
   TokenType toktype_ = parser_getcurtoktype(interp);


   *toktype = toktype_;
   if (status != OK) return status;

   /* If we have a keyword, just update the last keyword seen */
   if (toktype_ >= TOKRANGE_MDL_KW_START && toktype_ <= TOKRANGE_OTHER_KW_END) {
      parser_update_last_kw(interp);
      return OK;
   }

   /* ---------------------------------------------------------------------
    * If we have a GMS var or equ and the next token is '(', then we parse 
    * the next group as part of the GMS reference,
    *
    * If we have a set or parameter, we seek to resolve it.
    *
    * WARNING: this can only be done in _advance, not in the _lexer!
    * --------------------------------------------------------------------- */
   switch (toktype_) {
   case TOK_GMS_EQU:
   case TOK_GMS_VAR:
   case TOK_GMS_SET:
   case TOK_GMS_PARAM:
      return interp->ops->gms_parse(interp, p);
   default:
      return OK;
   }
}

/* TODO: DELETE ? */
UNUSED static int _chk_labelbasename(Token *tok, Model *mdl)
{
   assert(tok->type == TOK_IDENT);

   struct ctrdata_gams *gms = (struct ctrdata_gams*)mdl->ctr.data;
   struct ctrmem CTRMEM working_mem = {.ptr = NULL, .ctr = &mdl->ctr};
   const char *ident = emptok_getstrstart(tok);
   unsigned ident_len = emptok_getstrlen(tok);
   A_CHECK(working_mem.ptr, ctr_getmem(&mdl->ctr, sizeof(char) * (ident_len+1)));
   char *equvarname = (char*)working_mem.ptr;
   STRNCPY(equvarname, ident, ident_len);
   equvarname[ident_len] = '\0';

   int symindex = dctSymIndex(gms->dct, equvarname);
   if (symindex > 0) {
      error("%s :: basename of label '%s'", __func__, equvarname);
      printstr(PO_ERROR, " is registered as a symbol in the GAMS model and "
               "therefore cannot be used as a node label\n");
      return Error_EMPIncorrectSyntax;
   }

   return OK;
}

/**
 * @brief Check that a condition makes sense given the GAMS indices the just parsed
 *
 *  It ensures that we don't have  "xxx()$(...)" or "xxx('fixed')$(...)"
 *
 * @param gmsindices  the GAMS indices
 * @param linenr      the line number (for error messages)
 *
 * @return            the error code
 */
static int chk_compat_gmsindices_condition(GmsIndicesData *gmsindices, unsigned linenr)
{
   if (gmsindices->nargs == 0) {
      error("[empparser] ERROR line %u: a condition was given, but no GAMS indices",
            linenr);
      return Error_EMPIncorrectSyntax;
   }

   if (gmsindices->num_sets == 0 || gmsindices->num_localsets) {
      error("[empparser] ERROR line %u: a condition was given, but all GAMS indices"
            " are fixed.\n", linenr);
      return Error_EMPIncorrectSyntax;
   }

   return OK;
}

static int imm_set_regentry(Interpreter * restrict interp, const char *basename,
                            unsigned basename_len, GmsIndicesData *indices)
{
   /* ---------------------------------------------------------------------
    * This function creates the GMS filter and set the constant values in it:
    * - IdentUEL and IdentUniversalSet
    * - IdentLoopIterator, but these needs to be queried
    * --------------------------------------------------------------------- */

   A_CHECK(interp->regentry, regentry_new(basename, basename_len, indices->nargs));
   DagRegisterEntry *regentry = interp->regentry;

   trace_empinterp("[empinterp] New regentry for label with basename '%.*s'\n",
                   basename_len, basename);

   for (unsigned i = 0, len = indices->nargs; i < len; ++i) {

      IdentData *idxident = &indices->idents[i];
      switch (idxident->type) {
      case IdentUEL:
         assert(idxident->idx < INT_MAX && idxident->idx > 0);
         regentry->uels[i] = (int)idxident->idx;
         break;
      case IdentUniversalSet:
         error("[empinterp] ERROR line %u: '*' not allowed in label\n", interp->linenr);
         return Error_EMPIncorrectSyntax;
      case IdentLoopIterator:
      case IdentSet:
      case IdentLocalSet:
         error("[empinterp] ERROR line %u: %s '%.*s' not allowed in label\n",
               interp->linenr, identtype_str(idxident->type), idxident->lexeme.len,
               idxident->lexeme.start);
         return Error_EMPIncorrectSyntax;
      default:
         error("%s :: unexpected failure: got ident '%s' at position %u.\n",
               __func__, identtype_str(idxident->type), i);
         return Error_RuntimeError;
      }
   }

   return OK;
}

static int imm_set_dagrootlabel(Interpreter * restrict interp, const char *identname,
                         unsigned identname_len, GmsIndicesData *indices)
{
   /* ---------------------------------------------------------------------
    * This function creates the GMS filter and set the constant values in it:
    * - IdentUEL and IdentUniversalSet
    * - IdentLoopIterator, but these needs to be queried
    * --------------------------------------------------------------------- */

   A_CHECK(interp->dag_root_label, dag_label_new(identname, identname_len, indices->nargs));
   DagLabel *root_label = interp->dag_root_label;

   for (unsigned i = 0, len = indices->nargs; i < len; ++i) {

      IdentData *idxident = &indices->idents[i];
      switch (idxident->type) {

      case IdentUEL:
         assert(idxident->idx < INT_MAX && idxident->idx > 0);
         root_label->uels[i] = (int)idxident->idx;
         break;

      case IdentUniversalSet:
         error("[empinterp] ERROR line %u: '*' not allowed in label\n", interp->linenr);
         return Error_EMPIncorrectSyntax;

      case IdentLoopIterator:
      case IdentSet:
      case IdentLocalSet:
         error("[empinterp] ERROR line %u: %s '%.*s' not allowed in label\n",
               interp->linenr, identtype_str(idxident->type), idxident->lexeme.len,
               idxident->lexeme.start);
         return Error_EMPIncorrectSyntax;

      default:
         error("%s :: unexpected ERROR: got ident '%s' at position %u.\n",
               __func__, identtype_str(idxident->type), i);
         return Error_RuntimeError;
      }
   }

   return OK;
}

static int imm_add_daglabel(Interpreter * restrict interp, ArcType edge_type,
                            const char *identname, unsigned identname_len,
                            GmsIndicesData *indices)
{
   /* ---------------------------------------------------------------------
    * This function creates the GMS filter and set the constant values in it:
    * - IdentUEL and IdentUniversalSet
    * - IdentLoopIterator, but these needs to be queried
    * --------------------------------------------------------------------- */

   DagLabel *dagl;
   A_CHECK(dagl, dag_label_new(identname, identname_len, indices->nargs));
   dagl->arc_type = edge_type;

   DagRegister *dagreg = &interp->dagregister;
   unsigned dagreg_len = dagreg->len;

   if (dagreg_len == 0) {
      error("[empinterp] ERROR line %u: while parsing the label '%.*s', no label"
            " have been registered in the EMPDAG\n", interp->linenr,
            identname_len, identname);
      return Error_EMPRuntimeError;
   }

   dagl->daguid = interp->uid_parent;

   for (unsigned i = 0, len = indices->nargs; i < len; ++i) {

      IdentData *idxident = &indices->idents[i];
      switch (idxident->type) {
      case IdentUEL:
         assert(idxident->idx < INT_MAX && idxident->idx > 0);
         dagl->uels[i] = (int)idxident->idx;
         break;
      case IdentUniversalSet:
         error("[empinterp] ERROR line %u: '*' not allowed in label\n", interp->linenr);
         return Error_EMPIncorrectSyntax;
      case IdentLoopIterator:
      case IdentSet:
      case IdentLocalSet:
         error("[empinterp] ERROR line %u: %s '%.*s' not allowed in label\n",
               interp->linenr, identtype_str(idxident->type), idxident->lexeme.len,
               idxident->lexeme.start);
         return Error_EMPIncorrectSyntax;
      default:
         error("%s :: unexpected failure: got ident '%s' at position %u.\n",
               __func__, identtype_str(idxident->type), i);
         return Error_RuntimeError;
      }
   }

   S_CHECK(daglabel2edge_add(&interp->label2edge, dagl));

   return OK;
}

typedef int (*vm_fn_label2resolve)(Interpreter * interp, unsigned * restrict p,
                                   const char* argname, unsigned argname_len,
                                   GmsIndicesData* gmsindices);


typedef int (*imm_fn_label2resolve)(Interpreter * interp, const char* argname,
                                    unsigned argname_len, GmsIndicesData* gmsindices);

static int imm_add_Nash_edge(Interpreter * interp, const char* identname,
                             unsigned identname_len, GmsIndicesData* gmsindices)
{
   return imm_add_daglabel(interp, ArcNash, identname, identname_len, gmsindices);
}

static int imm_add_VFobjSimple_edge(Interpreter * interp, const char* identname,
                                    unsigned identname_len, GmsIndicesData* gmsindices)
{
   return imm_add_daglabel(interp, ArcVF, identname, identname_len, gmsindices);
}

static int imm_add_Ctrl_edge(Interpreter * interp, const char* identname,
                             unsigned identname_len, GmsIndicesData* gmsindices)
{
   return imm_add_daglabel(interp, ArcCtrl, identname, identname_len, gmsindices);
}

static int imm_set_dagroot(Interpreter * interp, const char* identname,
                           unsigned identname_len, GmsIndicesData* gmsindices)
{
   return imm_set_dagrootlabel(interp, identname, identname_len, gmsindices);
}

static int vm_set_dagroot(Interpreter * interp, unsigned * restrict p,
                          const char* identname, unsigned identname_len,
                          GmsIndicesData* gmsindices)
{
      error("[empinterp] ERROR line %u: the argument to the DAG keyword must be a unique"
            "label and its specification should only involve UELs.\n", interp->linenr);
      return Error_EMPIncorrectSyntax;
}

/**
 * @brief Add an EMPDAG edge 
 *
 * The current token holds the child node basename.
 *
 * @param interp  the interpreter
 * @param p       the position pointer
 * @param imm_fn  the function to call in immediate mode
 * @param vm_fn   the function to call in VM mode
 * @return 
 */
static int add_edge4label(Interpreter *interp, unsigned *p,
                          imm_fn_label2resolve imm_fn, vm_fn_label2resolve vm_fn)
{
   assert(parser_getcurtoktype(interp) == TOK_IDENT);
   const char *identname = emptok_getstrstart(&interp->cur);
   unsigned identname_len = emptok_getstrlen(&interp->cur);

   TokenType toktype;
   unsigned p2 = *p;
   S_CHECK(peek(interp, &p2, &toktype));

   /* If we are in ImmMode and need to switch to CompilerMode, we have to execute
    * the VM before returning as the EMPDAG uid would be wrong if executed later
    */
   bool switch_back_imm = false;
   GmsIndicesData gmsindices;
   gms_indicesdata_init(&gmsindices);

   if (toktype == TOK_LPAREN) {
      *p = p2;
      S_CHECK(parse_gmsindices(interp, p, &gmsindices));
   }

   if (gmsindices.nargs > 0 && (gmsindices.num_sets > 0 || gmsindices.num_localsets > 0 || gmsindices.num_iterators > 0)) {
      S_CHECK(c_switch_to_compmode(interp, &switch_back_imm));
      S_CHECK(vm_fn(interp, p, identname, identname_len, &gmsindices));
   } else {

      /* ---------------------------------------------------------------------
          * If we are past this point, we are in the immediate mode
          * --------------------------------------------------------------------- */

      if (interp->ops->type != ParserOpsImm) {
         error("[empinterp] line %u: runtime error: no GAMS set to iterate over, "
               "but the interpreter is in Compiler mode. Review the model to make sure "
               "that any loop involves Please report this as a bug.\n",
               interp->linenr);
         return Error_EMPRuntimeError;
      }
      S_CHECK(imm_fn(interp, identname, identname_len, &gmsindices));
   }

   if (switch_back_imm) {
      S_CHECK(c_switch_to_immmode(interp));
   }


   return OK;
}

NONNULL static int ensure_valfn_kwd(Interpreter *interp, unsigned *p)
{
   TokenType toktype;
   unsigned p2 = *p;
   S_CHECK(peek(interp, &p2, &toktype));

   S_CHECK(parser_expect_peek(interp, ".valfn expected after node name", TOK_DOT));

   S_CHECK(peek(interp, &p2, &toktype));
   S_CHECK(parser_expect_peek(interp, "valfn keyword expected after '.'", TOK_VALFN));

   *p = p2;
   
   return OK;
}

static int parse_edgeVF_sum(MathPrgm * restrict mp, Interpreter * restrict interp,
                      unsigned * restrict p)
{
   UNUSED int status = OK;
   TO_IMPLEMENT("edgeVF SUM");
}

typedef enum {
   ObjectiveVF,
   EquationVF,
} TypeVF;

static int parse_ovfparamscalar(Interpreter * interp, unsigned * restrict p,
                                void* ovfdef, const OvfParamDef * restrict pdef,
                                OvfArgType *type, OvfParamPayload *payload)

{
   payload->val = interp->cur.payload.real;
   *type = ARG_TYPE_SCALAR;

   return OK;
}

static int parse_ovfparamvec(Interpreter * interp, unsigned * restrict p,
                                void* ovfdef, const OvfParamDef * restrict pdef,
                                OvfArgType *type, OvfParamPayload *payload)

{
   
   DblScratch * restrict scratch = &interp->cur.dscratch;

   unsigned size_vector = interp->ops->ovf_param_getvecsize(interp, ovfdef, pdef);

   S_CHECK(scratchdbl_ensure(scratch, size_vector));
   double *dat = scratch->data;

   TokenType toktype;
   unsigned idx = 0;

   do {

      S_CHECK(advance(interp, p, &toktype));

      S_CHECK(parser_expect(interp, "Double vector parameter", TOK_REAL));
      dat[idx++] = interp->cur.payload.real;

      S_CHECK(advance(interp, p, &toktype));
   } while (toktype == TOK_COMMA);

   S_CHECK(parser_expect(interp, "Comma ',' or closing ')' expected", TOK_RPAREN));

   payload->vec = scratch->data;
   *type = ARG_TYPE_VEC;

   return OK;
}


static int parse_gamsparamident(Interpreter * interp, unsigned * restrict p,
                                void* ovfdef, const OvfParamDef * restrict pdef,
                                OvfArgType *type, OvfParamPayload *payload)
{
   int status = OK;
   IdentData ident;
   TokenType toktype;
   unsigned param_gidx = UINT_MAX;
   DblScratch *scratch = &interp->cur.dscratch;

   RESOLVE_IDENTAS(interp, &ident, "a GAMS parameter is expected",
                   IdentScalar, IdentVector, IdentLocalVector);

   const char *identstr = tok_dupident(&interp->cur);
   switch (ident.type) {

   case IdentScalar: {
      unsigned idx = namedscalar_findbyname_nocase(&interp->globals.scalars, identstr);
      if (idx == UINT_MAX) {
         error("[empinterp] unexpected runtime error: couldn't find scalar '%s'\n", identstr);
         status = Error_EMPRuntimeError;
         goto _exit;
      }
      payload->val = interp->globals.scalars.list[idx];
      *type = ARG_TYPE_SCALAR;
      break;
   }

   case IdentLocalVector:
   case IdentVector: {
      unsigned p2 = *p;
      S_CHECK_EXIT(peek(interp, &p2, &toktype));
      if (interp->ops->type == ParserOpsImm && toktype != TOK_LPAREN && toktype != TOK_CONDITION) {

            // TODO: what happens in immMode and with TOK_LPAREN or TOK_CONDITION???
         unsigned idx;
         const NamedVecArray *container = ident.type == IdentVector ? 
            &interp->globals.vectors : &interp->globals.localvectors;

         idx = namedvec_findbyname_nocase(container, identstr);
         if (idx == UINT_MAX) {
            error("[empinterp] unexpected runtime error: couldn't find vector '%s'\n", identstr);
            status = Error_EMPRuntimeError;
            goto _exit;
         }

         const Lequ * vec = &interp->globals.vectors.list[idx];
         unsigned size_vector = interp->ops->ovf_param_getvecsize(interp, ovfdef, pdef);
         if (size_vector != UINT_MAX && vec->len != size_vector) {
            error("[empinterp] ERROR line %u: for OVF parameter '%s', "
                  "expecting the vector '%s' to have length %u, got %u instead.\n",
                  interp->linenr, pdef->name, identstr, size_vector, vec->len);
            status = Error_EMPIncorrectInput;
            goto _exit;
         }

         S_CHECK_EXIT(scratchdbl_ensure(scratch, size_vector));
         memcpy(scratch->data, vec->coeffs, size_vector * sizeof(double));

         payload->vec = scratch->data;
         *type = ARG_TYPE_VEC;

         break;
      }
      /* This is the generic way of reading the parameter; we only reach here
       * if the previous "if" condition isn't true */
      S_CHECK_EXIT(interp->ops->read_param(interp, p, &ident, identstr, &param_gidx))
      *type = ARG_TYPE_NESTED;
      payload->gidx = param_gidx;
      break;
   }
   case IdentParam: {
      TO_IMPLEMENT_EXIT("Generic parameter parsing");
   }
   default:
      status = runtime_error(interp->linenr);
      goto _exit;
   }

_exit:
   FREE(identstr);

   return status;
}

/**
 * @brief Parse ".valfn" after a node name
 *
 * @param interp  the Interpreter
 * @param p       the current position
 *
 * @return        the error code
 */
NONNULL static int parse_valfnObj(Interpreter *interp, unsigned *p)
{
   S_CHECK(ensure_valfn_kwd(interp, p));

   S_CHECK(add_edge4label(interp, p, imm_add_VFobjSimple_edge, vm_add_VFobjSimple_edge))

   TokenType toktype;
   unsigned p2 = *p;
   S_CHECK(peek(interp, &p2, &toktype));

   if (toktype == TOK_STAR) {
      TO_IMPLEMENT("'node.valFn *' is not yet supported");
   }

   return OK;
}

NONNULL static
int parse_VFargs_gmsvars_list(Interpreter * restrict interp, unsigned * restrict p,
                              void *ovfdef)
{
   if (interp->ops->type != ParserOpsImm) {
      TO_IMPLEMENT("OVF/CCF with empparser in VM mode");
   }

   TokenType toktype = parser_getcurtoktype(interp);

   while (toktype == TOK_GMS_VAR) {
      S_CHECK(interp->ops->ovf_addarg(interp, ovfdef));
      S_CHECK(advance(interp, p, &toktype));

      if (toktype == TOK_COMMA) {
         S_CHECK(advance(interp, p, &toktype));
      }
   }

   // TODO: document that we cannot mix GAMS variable and labels in VF/CCF
   S_CHECK(parser_expect(interp, "After list of GAMS variables, expecting ')')", TOK_RPAREN));

   return OK;
}

/**
 * @brief Add an edge 
 *
 * We are called in a VF statement, when one of the argument is supposed to be
 * a node label.
 *
 * @param interp 
 * @param p 
 * @return 
 */
NONNULL_AT(1,2) static
int parse_VFargs_labels(Interpreter * restrict interp, unsigned * restrict p, 
                        OvfDef *ovfdef)
{
   unsigned idx1 = interp->label2edge.len;
   unsigned idx2 = interp->labels2edges.len;

   /* takes care of '.valfn' */
   S_CHECK(ensure_valfn_kwd(interp, p));

   S_CHECK(add_edge4label(interp, p, imm_add_VFobjSimple_edge, vm_add_VFobjSimple_edge))

  /* ----------------------------------------------------------------------
   * This is to update the number of EMPDAG children whenever we were in ImmMode
   * and had to switch to CompilerMode
   * ---------------------------------------------------------------------- */

   if (ovfdef) {
      for (unsigned i = idx1, len = interp->label2edge.len; i < len; ++i) {
         ovfdef->num_empdag_children++;
      }
      for (unsigned i = idx2, len = interp->labels2edges.len; i < len; ++i) {
         ovfdef->num_empdag_children += interp->labels2edges.list[i]->num_children;
      }
   }

   TokenType dummy;
   return advance(interp, p, &dummy); 
}

NONNULL_AT(2, 3) static
int parse_VF_CCF(MathPrgm * restrict mp_parent, Interpreter * restrict interp,
                 unsigned * restrict p)
{
  /* ----------------------------------------------------------------------
   * Parse statement of the form: VF('name', args, ...). Steps:
   * 1. Get the name and validate it
   * 2. Get the CCF arguments
   * 3. Parse the kw arguments
   * ---------------------------------------------------------------------- */

   TokenType toktype;
   S_CHECK(advance(interp, p, &toktype))

   S_CHECK(parser_expect(interp, "After 'VF', expecting '(')", TOK_LPAREN));

   S_CHECK(advance(interp, p, &toktype))

   /* ---------------------------------------------------------------------
    * 1. Get the CCF name
    * --------------------------------------------------------------------- */

   PARSER_EXPECTS(interp, "VF name (quoted) is expected",
                  TOK_SINGLE_QUOTE, TOK_DOUBLE_QUOTE);

   char quote = toktype == TOK_SINGLE_QUOTE ? '\'' : '"';

   if (tok_untilchar(&interp->cur, interp->buf, quote, p)) {
      goto _err_EOF_fname;
   }

   unsigned ccfnamelen = emptok_getstrlen(&interp->cur);
   const char *ccfnamestart = emptok_getstrstart(&interp->cur);

   /* Find the string in CCF names */
   unsigned ovf_idx = ovf_findbytoken(ccfnamestart, ccfnamelen);
   if (ovf_idx == UINT_MAX) {
      error("[empinterp] Could not find a CCF named '%.*s'\n", ccfnamelen, ccfnamestart);
      return Error_EMPIncorrectInput;
   }

   /* Consume the end quote */
   (*p)++;

   /* TODO: this should be deleted as the check is implicitly done in 
    * tok_untilchar. Add a test */

//   S_CHECK(advance(interp, p, &toktype))
//   PARSER_EXPECTS(interp, "closing quote is expected",
//                          TOK_SINGLE_QUOTE, TOK_DOUBLE_QUOTE));
//
//   if (quote_is_single && toktype != TOK_SINGLE_QUOTE) {
//      error("[empparser] ERROR line %u: expecting ' as closing quote\n",
//            interp->linenr);
//      return Error_EMPIncorrectSyntax;
//   }
//
//   if (!quote_is_single && toktype != TOK_DOUBLE_QUOTE) {
//      error("[empparser] ERROR line %u: expecting \" as closing quote\n",
//            interp->linenr);
//      return Error_EMPIncorrectSyntax;
//   }

   /* ---------------------------------------------------------------------
    * Add the CCF object
    * --------------------------------------------------------------------- */
   MathPrgm *mp_ccf;
   S_CHECK(interp->ops->ccflib_new(interp, ovf_idx, &mp_ccf));

   S_CHECK(advance(interp, p, &toktype))
   S_CHECK(parser_expect(interp, "expected arguments", TOK_COMMA));

   /* ---------------------------------------------------------------------
    * 2. Parse the CCF arguments
    * Parse the argument: it could be either
    * - nargs(a)
    * - (n1, n2, n3)
    *
    *  and either variables or nodes
    * --------------------------------------------------------------------- */
   S_CHECK(advance(interp, p, &toktype))
   PARSER_EXPECTS(interp, "expected arguments", TOK_GMS_VAR, TOK_IDENT, TOK_LPAREN);

   OvfDef *ovfdef = mp_ccf ? mp_ccf->ccflib.ccf : NULL;

   bool has_lparent = false;
   if (toktype == TOK_LPAREN) {
      has_lparent = true;
      S_CHECK(advance(interp, p, &toktype))
      PARSER_EXPECTS(interp, "expected VF argument to be a variable or node label",
                     TOK_GMS_VAR, TOK_IDENT);
   }
      
   switch (toktype) {
   case TOK_IDENT:
      if (has_lparent) {
         while (toktype == TOK_IDENT) {
            S_CHECK(parse_VFargs_labels(interp, p, ovfdef));
            //S_CHECK(advance(interp, p, &toktype));

            toktype = emptok_gettype(&interp->cur);
            if (toktype != TOK_COMMA) { break; }

            S_CHECK(advance(interp, p, &toktype));
         }
         S_CHECK(parser_expect(interp, "Closing ')'", TOK_RPAREN));
         S_CHECK(advance(interp, p, &toktype));
      } else {
         S_CHECK(parse_VFargs_labels(interp, p, ovfdef));
      }
      break;
   case TOK_GMS_VAR: {
      if (has_lparent) {
         S_CHECK(parse_VFargs_gmsvars_list(interp, p, mp_ccf));
      } else {
         
         S_CHECK(interp->ops->ovf_addarg(interp, ovfdef));
         S_CHECK(advance(interp, p, &toktype));
      }
      }
      break;
   default:
      return runtime_error(interp->linenr);
   }

   toktype = emptok_gettype(&interp->cur);
   if (toktype == TOK_RPAREN) {
      goto _finalize;
   }

   S_CHECK(parser_expect(interp, "expected CCF keyword arguments", TOK_COMMA));

     /* ---------------------------------------------------------------------
      * Step 3: Parse CCF kw args as    kw=arg
      * --------------------------------------------------------------------- */

   const OvfParamDefList *paramsdef;
   S_CHECK(interp->ops->ovf_paramsdefstart(interp, ovfdef, &paramsdef));

   while (toktype == TOK_COMMA) {
      if (parser_skipws(interp, p)) goto _err_EOF_fname;

      if (tok_untilchar(&interp->cur, interp->buf, '=', p)) {
         goto _err_missing_equal;
      }

      const char *kwnamestart = emptok_getstrstart(&interp->cur);
      unsigned kwnamelen = emptok_getstrlen(&interp->cur);

      /* Consume '=' */
      (*p)++;

      const OvfParamDef *pdef;
      unsigned pidx;
      A_CHECK(pdef, ovfparamdef_find(paramsdef, kwnamestart, kwnamelen, &pidx))

      S_CHECK(advance(interp, p, &toktype));

      OvfParamPayload payload;
      OvfArgType type = ARG_TYPE_UNSET;

      switch (toktype) {
      case TOK_IDENT:
         S_CHECK(parse_gamsparamident(interp, p, ovfdef, pdef, &type, &payload));
         break;
      case TOK_REAL:
         S_CHECK(parse_ovfparamscalar(interp, p, ovfdef, pdef, &type, &payload));
         break;
      case TOK_LPAREN:
         S_CHECK(parse_ovfparamvec(interp, p, ovfdef, pdef, &type, &payload));
         break;
      default:
         error("[empparser] ERROR line %u: unsupported token type %s in CCF keyword argument.\n", interp->linenr,
               toktype2str(toktype));
         return Error_EMPRuntimeError;
      }

      trace_empparser("[empinterp] CCF parameter '%s' parsed with type %s\n.",
                      pdef->name, ovf_argtype_str(type));
      S_CHECK(interp->ops->ovf_setparam(interp, ovfdef, pidx, type, payload));

      S_CHECK(advance(interp, p, &toktype));
   }

   S_CHECK(parser_expect(interp, "expecting closing ')'", TOK_RPAREN));

   /* We need a custom ccflib_finalize as in CompilerMode, the parameter list needs
    * to be copied into the mp->ccflib.ovfdef and then mp_finalize can be called
    */
_finalize:

   S_CHECK(interp->ops->ccflib_finalize(interp, mp_ccf));

   /* Note that we do not call advance, as this would be wrong while parsing a CCF in an MP */
   return OK;

_err_EOF_fname:
   error("[empparser] ERROR line %u: while parsing the CCF name got end-of-file",
         interp->linenr);
   return Error_EMPIncorrectSyntax;

_err_missing_equal:
   errormsg("[empparser] ERROR line %u: while parsing CCF arguments, expecting '='\n");
   return Error_EMPIncorrectSyntax;
}

int parse_DAG(Interpreter *interp, unsigned *p)
{
   TokenType toktype = parser_getcurtoktype(interp);

   S_CHECK(advance(interp, p, &toktype));
   S_CHECK(parser_expect(interp, "Node label for DAG keyword argument", TOK_IDENT));

   S_CHECK(add_edge4label(interp, p, imm_set_dagroot, vm_set_dagroot))
   
   return advance(interp, p, &toktype);
}

int parse_Nash(Interpreter *interp, unsigned *p)
{
   TokenType toktype = parser_getcurtoktype(interp);
   assert(toktype == TOK_NASH);

   S_CHECK(advance(interp, p, &toktype));
   S_CHECK(parser_expect(interp, "'(' after Nash keyword)", TOK_LPAREN));

   Mpe *mpe;
   S_CHECK(interp->ops->mpe_new(interp, &mpe));

   do {
      S_CHECK(advance(interp, p, &toktype));
      S_CHECK(parser_expect(interp, "Node label for as Nash keyword argument", TOK_IDENT));

      S_CHECK(add_edge4label(interp, p, imm_add_Nash_edge, vm_nash));

      advance(interp, p, &toktype);
   } while (toktype == TOK_COMMA);

   S_CHECK(parser_expect(interp, "Closing ')' after Nash keyword", TOK_RPAREN));

   S_CHECK(interp->ops->mpe_finalize(interp, mpe));

   return advance(interp, p, &toktype);
}

static int mp_opt_add_name(MathPrgm * restrict mp)
{
   assert(mp_isopt(mp));
   EmpDag * restrict empdag = &mp->mdl->empinfo.empdag;

   mpid_t mpid = mp->id;

   if (empdag_mphasname(empdag, mpid)) { return OK; }

   rhp_idx objvar = mp_getobjvar(mp);

   if (!valid_vi(objvar)) {
      error("[empinterp] ERROR: OPT MP(%s) has invalid objective variable",
            empdag_getmpname(empdag, mpid));
      return Error_EMPRuntimeError;
   }

   char *mpname;
   IO_CALL(asprintf(&mpname, "%s %s", sense2str(mp->sense),
                    mdl_printvarname(mp->mdl, objvar)));


   S_CHECK(empdag_setmpname(empdag, mpid, mpname));

   /* We have to free mp name */
   FREE(mpname);

   return OK;
}

static int err_wildcard(unsigned linenr)
{
   error("[empparser] ERROR at line %u: the wildcard '*' in a problem is no "
         "longer supported. Specify all the variables attached to this problem\n",
         linenr);

   return Error_EMPIncorrectSyntax;
}

static int parse_opt(MathPrgm * restrict mp, Interpreter * restrict interp,
                     unsigned * restrict p)
{
   int status = OK;

   /* Save the keyword info in case we parse another keyword */
   KeywordLexemeInfo opt_kw_info = interp->last_kw_info;

   S_CHECK(interp->ops->mp_settype(interp, mp, MpTypeOpt));

   /* TODO(urg) An analysis should yield the problem type */
//   mp->probtype = MdlProbType_nlp;

   /* ---------------------------------------------------------------------
    * The next token is either the MP type (optional) or the objvar
    * --------------------------------------------------------------------- */

   TokenType toktype;
   S_CHECK(advance(interp, p, &toktype))

   if (toktype >= TOKRANGE_PROBTYPE_START && toktype <= TOKRANGE_PROBTYPE_START) {
      S_CHECK(interp->ops->mp_setprobtype(interp, mp, tok2probtype(toktype)));
      S_CHECK(advance(interp, p, &toktype));
   }

   /* ---------------------------------------------------------------------
    * We could decide to not impose having the objective variable now, but
    * let's KISS for now
    * --------------------------------------------------------------------- */

   S_CHECK_EXIT(parser_expect(interp, "objective variable is expected", TOK_GMS_VAR));
   S_CHECK(interp->ops->mp_setobjvar(interp, mp));
   S_CHECK(advance(interp, p, &toktype));

   /* Provide better error message for old-style syntax */
   if (toktype == TOK_STAR) {
      if (old_style_check(interp) != OK || !mp) { return err_wildcard(interp->linenr); }

      S_CHECK(chk_wildcard_vars_allowed(interp));

      interp->finalize.mp_owns_remaining_vars = mp->id;
      S_CHECK(advance(interp, p, &toktype));
   }

   /* ---------------------------------------------------------------------
    * We either have a '+', which signals a edgeVF specification, or a variable
    * which indicates the start of the variable specification, or an equation
    * if the optimization problem is rather trivial
    * --------------------------------------------------------------------- */

   PARSER_EXPECTS_EXIT(interp, "a GAMS variable or equation or a '+' is expected",
                       TOK_GMS_VAR, TOK_PLUS, TOK_GMS_EQU);

   /* ---------------------------------------------------------------------
    * Read the optional extended objective definition. The basic ingredients are
    * - VafFn declaration(s): VF('name', (args), params...)
    * - expression of the form ``  cst * var * mp  ''; the label mp is required
    * - sum statement: sum(gms_set, gms_expr)
    * --------------------------------------------------------------------- */

   while (toktype == TOK_PLUS) { /* TODO(URG): support TOK_MINUS */

      S_CHECK(advance(interp, p, &toktype))
      PARSER_EXPECTS_EXIT(interp, "edgeVF expression is expected",
                          TOK_GMS_VAR, TOK_REAL, TOK_VALFN, TOK_IDENT, TOK_SUM,
                          TOK_VF);


      if (toktype == TOK_SUM) {
         S_CHECK(parse_edgeVF_sum(mp, interp, p));

      } else if (toktype == TOK_IDENT) {          // Expecting label.valfn
         S_CHECK(parse_valfnObj(interp, p));

      } else if (toktype == TOK_VF) {
         S_CHECK(parse_VF_CCF(mp, interp, p));
      } else {
         TO_IMPLEMENT("complex edgeVF parsing");
      }

      S_CHECK(advance(interp, p, &toktype))
   }

   assert(toktype == TOK_GMS_VAR || toktype == TOK_GMS_EQU);

   /* ------------------------------------------------------------------
    * Read variables belonging to this mathprgm.
    * ------------------------------------------------------------------ */

   while (toktype == TOK_GMS_VAR) {
      S_CHECK(interp->ops->mp_addvars(interp, mp));
      S_CHECK(advance(interp, p, &toktype));
   }

   /* Provide better error message for old-style syntax */
   if (toktype == TOK_STAR) {
      if (old_style_check(interp) != OK || !mp) { return err_wildcard(interp->linenr); }

      S_CHECK(chk_wildcard_vars_allowed(interp));

      interp->finalize.mp_owns_remaining_vars = mp->id;
      S_CHECK(advance(interp, p, &toktype));
   }

   /* ------------------------------------------------------------------
    * Read equations/label belonging to this mp
    * ------------------------------------------------------------------ */

   while (toktype == TOK_GMS_EQU || toktype == TOK_IDENT || toktype == TOK_MINUS) {

      bool is_flipped = false;
      if (toktype == TOK_MINUS) {
         is_flipped = true;
         S_CHECK(advance(interp, p, &toktype));
         S_CHECK_EXIT(parser_expect(interp, "In the constraint part of an MP, "
                               "an equation is expected after '-'", TOK_GMS_EQU));
      }

      if (toktype == TOK_GMS_EQU) {
         S_CHECK(interp->ops->mp_addcons(interp, mp));
         if (is_flipped) {
            S_CHECK(interp->ops->ctr_markequasflipped(interp));
         }

      } else if (toktype == TOK_IDENT) {

         /* ----------------------------------------------------------------
          * Make sure the ident is not followed by a ':' as this could
          * be a declaration on the next line. If we have a '(', then we seek
          * to parse until we are over all the indices and then check for ':'
          * ---------------------------------------------------------------- */
         unsigned p2 = *p;
         S_CHECK(peek(interp, &p2, &toktype));
         if (toktype == TOK_COLON) {
            goto exit_while;
         } else if (toktype == TOK_LPAREN) {
            if (skip_gmsindices(interp->buf, &p2)) {
               error("[empparser] while parsing the indices of '%.*s', got "
                     "end-of-file\n", emptok_getstrlen(&interp->cur),
                     emptok_getstrstart(&interp->cur));
               return Error_EMPIncorrectSyntax;
            }

            /* Consume ')' */
            p2++;
            S_CHECK(peek(interp, &p2, &toktype));

            if (toktype == TOK_CONDITION) {

               S_CHECK(peek(interp, &p2, &toktype));
               if (toktype != TOK_LPAREN) {
                  TO_IMPLEMENT("After a conditional '$', a left parenthesis '(' is expected");
               }

               skip_conditional(interp->buf, &p2);
               p2++; /* consume ')' */
               S_CHECK(peek(interp, &p2, &toktype));
            }

            /* If we reached a node definition, exit */
            if (toktype == TOK_COLON) { goto exit_while; }
         }
 
         /* ----------------------------------------------------------------
          * We have a MP or MPE in the constraints
          * ---------------------------------------------------------------- */
         S_CHECK(add_edge4label(interp, p, imm_add_Ctrl_edge, vm_add_Ctrl_edge))
      }

      S_CHECK(advance(interp, p, &toktype));
   }

   /* Provide better error message for old-style syntax */
   if (toktype == TOK_STAR) {
      if (old_style_check(interp) != OK || !mp) { return err_wildcard(interp->linenr); }

      S_CHECK(chk_wildcard_equs_allowed(interp));

      interp->finalize.mp_owns_remaining_equs = mp->id;
      S_CHECK(advance(interp, p, &toktype));
   }

exit_while:

   return status;

_exit:
   interp->last_kw_info = opt_kw_info;
   return status;
}

static int parse_vi(MathPrgm * restrict mp, 
                    Interpreter * restrict interp, unsigned * restrict p)
{
   /* ------------------------------------------------------------------
    * A VI statement has the form 
    *
    * VI {zerovars} {F var} {constraints}
    *
    * - the zerovars are matches to zero functions
    * - The matching F.var 
    * - The equations on constraints define the feasible set of the VI
    * ------------------------------------------------------------------ */

   int status = OK;
   bool is_empty = true;

   /* Save the keyword info in case we parse another keyword */
   KeywordLexemeInfo vi_kw_info = interp->last_kw_info;

   S_CHECK(interp->ops->mp_settype(interp, mp, RHP_MP_VI));

   TokenType toktype;
   S_CHECK(advance(interp, p, &toktype))

   /* ------------------------------------------------------------------
    * Read zero functions variables
    * ------------------------------------------------------------------ */

   while (toktype == TOK_GMS_VAR) {
      is_empty = false;
      S_CHECK(interp->ops->mp_addzerofunc(interp, mp));
      S_CHECK(advance(interp, p, &toktype));
   }

   /* ------------------------------------------------------------------
    * Read equations/label belonging to this mp
    * ------------------------------------------------------------------ */

   while (toktype == TOK_GMS_EQU || toktype == TOK_REAL || toktype == TOK_MINUS) {
      is_empty = false;

      bool zerofunc = false, is_flipped = false;
      if (toktype == TOK_REAL) {
         double val = interp->cur.payload.real; 
         if (val != 0.) {
            error("[empinterp] ERROR on line %u: while parsing a VI statement, "
                  "unexpected number '%f' parsed. Only '0' is valid for "
                  "zerofunc variables\n", interp->linenr, val);
            status = Error_EMPIncorrectSyntax;
            goto _exit;
         }
         zerofunc = true;

      } else if (toktype == TOK_MINUS) {
         is_flipped = true;
         S_CHECK(advance(interp, p, &toktype));
         S_CHECK_EXIT(parser_expect(interp, "In the mapping part of a VI, "
                               "an equation is expected after '-'", TOK_GMS_EQU));
      }

      interp_save_tok(interp);

      S_CHECK(advance(interp, p, &toktype));

      switch (toktype) {
      case TOK_GMS_VAR:
      {
         /* ----------------------------------------------------------------
          * We have a equ.var or 0.var specification
          * ---------------------------------------------------------------- */
         if (RHP_UNLIKELY(zerofunc)){
            S_CHECK(interp->ops->mp_addzerofunc(interp, mp));
         } else {
            S_CHECK(interp->ops->mp_addvipairs(interp, mp));
            if (is_flipped) {
               S_CHECK(interp->ops->ctr_markequasflipped(interp));
            }
         }
         break;
      }
      case TOK_IDENT:
         /* ----------------------------------------------------------------
          * We have a MP or MPE in the constraints
          * TODO: TOK_REAL case
          * ---------------------------------------------------------------- */
         TO_IMPLEMENT("VI with labels");
         break;
      default: {
         /* ----------------------------------------------------------------
          * We are done parsing the equ.var part, we add a constraint
          * switch bring the equation back into the current token, while saving
          * the current one in pre. Then we can restore it
          * ---------------------------------------------------------------- */
         interp_switch_tok(interp);
         S_CHECK(interp->ops->mp_addcons(interp, mp));
         if (is_flipped) {
            S_CHECK(interp->ops->ctr_markequasflipped(interp));
         }
         interp_restore_savedtok(interp);
         goto _constraints;
      }
      }

      S_CHECK(advance(interp, p, &toktype));
   }

   if (is_empty) {
      error("[empinterp] ERROR line %u: empty VI declaration\n", interp->linenr);
      status = Error_EMPIncorrectSyntax;
      goto _exit;
   }

   /* ------------------------------------------------------------------
    * Add the constraints defining the constraint set
    * ------------------------------------------------------------------ */
_constraints:
   interp_erase_savedtok(interp); /* Removed saved token to get better error msg */
   while (toktype == TOK_GMS_EQU || toktype == TOK_MINUS) {
      bool is_flipped = false;

      if (toktype == TOK_MINUS) {
         is_flipped = true;
         S_CHECK(advance(interp, p, &toktype));
         S_CHECK_EXIT(parser_expect(interp, "In the constraint part of a VI, "
                               "an equation is expected after '-'", TOK_GMS_EQU));
      }

      S_CHECK(interp->ops->mp_addcons(interp, mp));

      if (is_flipped) {
         S_CHECK(interp->ops->ctr_markequasflipped(interp));
      }

      S_CHECK(advance(interp, p, &toktype));
   }

   return status;

_exit:
   interp->last_kw_info = vi_kw_info;
   return status;
}

int parse_equilibrium(Interpreter *interp)
{
   EmpInfo *empinfo = &interp->mdl->empinfo;
   EmpDag *empdag = &empinfo->empdag;

   S_CHECK(equilibrium_sanity_check(interp));

   parsed_equilibrium(interp);

   mdl_settype(interp->mdl, MdlType_emp);

   /* ---------------------------------------------------------------------
    * We add an equilibrium as root
    * --------------------------------------------------------------------- */

   unsigned equil_id;
   char *name;
   A_CHECK(name, strdup("equilibrium"));
   S_CHECK(empdag_addmpenamed(empdag, name, &equil_id));
   S_CHECK(empdag_setroot(empdag, mpeid2uid(equil_id)));

   trace_empinterp("[empinterp] line %u: Found 'equilibrium' keyword. Adding MPE root node\n",
                   interp->linenr);

   return OK;
}

int parse_mp(Interpreter *interp, unsigned *p)
{

   /* ---------------------------------------------------------------------
    * Quick sanity check. An MP declaration is valid in 3 instances:
    * - as part of an EMPDAG
    * - as part of an equilibrium (explicit or implicit)
    * - as a single problem
    *
    * If this is the second MP declaration, then introduce a Nash as root for
    * the EMPDAG
    * --------------------------------------------------------------------- */
   S_CHECK(mp_sanity_check(interp));

   if (_has_single_mp(interp)) {
      _single_mp_to_implicit_Nash(interp);
      S_CHECK(empdag_single_MP_to_Nash(&interp->mdl->empinfo.empdag));
      interp->finalize.mp_owns_remaining_vars = MpId_NA;
      interp->finalize.mp_owns_remaining_equs = MpId_NA;
   } 

   /* ---------------------------------------------------------------------
    * We attempt to parse an MP declaration. It could either be an optimization
    * problem, with sense 'min' or 'max', or a VI, with the keyword 'vi'.
    *
    * Depending on the style of empinfo file, a label might have been parsed and
    * is stored in interp->pre.
    * --------------------------------------------------------------------- */


   MathPrgm *mp;
   TokenType toktype = interp->cur.type;
   switch (toktype) {
   case TOK_MIN:
   case TOK_MAX:
   {
      RhpSense sense = toktype == TOK_MIN ? RhpMin : RhpMax;
      S_CHECK(interp->ops->mp_new(interp, sense, &mp));

      S_CHECK(parse_opt(mp, interp, p));

      /* The old empinfo syntax doesn't have name, try to be better here*/
      if (interp->ops->type == ParserOpsImm ) {
         S_CHECK(mp_opt_add_name(mp))
      }
      break;
   }
   case TOK_VI:
      S_CHECK(interp->ops->mp_new(interp, RhpFeasibility, &mp));
      S_CHECK(parse_vi(mp, interp, p));
      break;

   default:
      return runtime_error(interp->linenr);
   }

   /* ---------------------------------------------------------------------
    * If we have a simple Nash problem, we automatically add the MP to the MPE
    * This MUST be called before finalize
    * --------------------------------------------------------------------- */

   if (is_simple_Nash(interp)) {
      S_CHECK(interp->ops->mpe_addmp(interp, 0, mp));

  /* ----------------------------------------------------------------------
   * If there are no parents, we add this MP as the root
   * ---------------------------------------------------------------------- */

   } else if (_has_no_parent(interp)) {
      DBGUSED EmpDag *empdag = &interp->mdl->empinfo.empdag;
      assert(empdag->mps.len == 1);
      parsed_single_mp(interp);
      /* TODO GITLAB #126 */
      //interp->finalize.mp_owns_remaining_vars = mp->id;
      //interp->finalize.mp_owns_remaining_equs = mp->id;
   }

   S_CHECK(interp->ops->mp_finalize(interp, mp));

   return OK;
}


NONNULL
static int parse_ovfparam(Interpreter * restrict interp, unsigned * restrict p,
                          void *ovfdef, const OvfParamDef *pdef, unsigned pidx)
{
   int status = OK;
   unsigned lindx = 0, p2 = *p;
   double valp = NAN;
   TokenType toktype = parser_getcurtoktype(interp);
   DblScratch *scratch = &interp->cur.dscratch;
   const char *identstr  = NULL;

   enum { SCRATCHDBL_DEFAULT = 100 };

   unsigned param_gidx = UINT_MAX;
   unsigned size_vector = interp->ops->ovf_param_getvecsize(interp, ovfdef, pdef);

   /*  First try to read the value as a double */
   if (pdef->allow_scalar || pdef->allow_vector) {

      bool force_vec = false;
      bool first = true;
      if (!pdef->allow_scalar) { force_vec = true; }

      /* ---------------------------------------------------------------------
       * We already have in cur a parsed token
       * --------------------------------------------------------------------- */

      /* This is outside of the loop as it looks at the current token */
      double val = interp->cur.payload.real;

      while (toktype == TOK_REAL) {

         /* \TODO(xhub) URGENT improve clarity */
         /* At the first pass, we store the value in valp if we need to store
          * the value in a vector*/
         if (lindx == 0 && !force_vec) {
            valp = val;
         } else {
            if (size_vector < UINT_MAX) {
               assert(size_vector > 0);
               S_CHECK(scratchdbl_ensure(scratch, size_vector));
            } else {
                S_CHECK(scratchdbl_ensure(scratch, SCRATCHDBL_DEFAULT));
            }
            switch (lindx) {
            case 1:
               scratch->data[1] = val;
               if (!first) {
                  break;
               }
               FALLTHRU
            case 0:
               if (force_vec) {
                  scratch->data[0] = val;
               } else {
                  assert(isfinite(valp));
                  scratch->data[0] = valp;
               }
               break;
            default:
               if (lindx >= size_vector) {
                  error("%s ERROR: number of parameter provided %u is bigger "
                        "than the expected size %u\n", __func__, 1+lindx,
                        size_vector);
                  return Error_WrongParameterValue;
               }
               if (size_vector == UINT_MAX && lindx >= SCRATCHDBL_DEFAULT) {
                     scratchdbl_ensure(scratch, lindx+1);
               }
               scratch->data[lindx] = val;
            }
            /* TODO: why does this need to be here and not at the end of the if?
             * The latter fails when probabilities are provided */
            first = false;
         }

         /* ---------------------------------------------------------------------
          * Always parse the next token
          * --------------------------------------------------------------------- */

         S_CHECK(peek(interp, &p2, &toktype););
         toktype = interp->peek.type;
         val = interp->peek.payload.real;

         lindx++;
         if (!pdef->allow_vector) {
            break;
         }
      }

      if (first && toktype == TOK_IDENT) {
         IdentData ident;
         RESOLVE_IDENTAS(interp, &ident, "a parameter is expected",
                         IdentScalar, IdentVector, IdentLocalVector);

         identstr = tok_dupident(&interp->cur);

         switch (ident.type) {
         case IdentScalar: {
            unsigned idx = namedscalar_findbyname_nocase(&interp->globals.scalars, identstr);
            if (idx == UINT_MAX) {
               error("[empinterp] unexpected runtime error: couldn't find scalar '%s'\n", identstr);
               status = Error_EMPRuntimeError;
               goto _exit;
            }
            double val_ = interp->globals.scalars.list[idx];
            assert(isfinite(val_));
            valp = val_;
            lindx++;
            break;
         }

         case IdentLocalVector:
         case IdentVector:
            S_CHECK_EXIT(peek(interp, &p2, &toktype));

            if (toktype != TOK_LPAREN && toktype != TOK_CONDITION) {
               unsigned idx;
               const NamedVecArray *container;
               if (ident.type == IdentVector) {
                  container = &interp->globals.vectors;
               } else {
                  container = &interp->globals.localvectors;
               }

               idx = namedvec_findbyname_nocase(container, identstr);
               if (idx == UINT_MAX) {
                  error("[empinterp] unexpected runtime error: couldn't find vector '%s'\n", identstr);
                  status = Error_EMPRuntimeError;
                  goto _exit;
               }

               const Lequ * vec = &interp->globals.vectors.list[idx];

               if (vec->len != size_vector) {
                  error("[empinterp] ERROR line %u: for OVF parameter '%s', "
                        "expecting the vector '%s' to have length %u, got %u instead.\n",
                        interp->linenr, pdef->name, identstr, size_vector, vec->len);
                  status = Error_EMPIncorrectInput;
                  goto _exit;
               }

               S_CHECK_EXIT(scratchdbl_ensure(scratch, size_vector));
               memcpy(scratch->data, vec->coeffs, size_vector * sizeof(double));
               break;
            }
            S_CHECK_EXIT(interp->ops->read_param(interp, p, &ident, identstr, &param_gidx))
            break;
         case IdentParam: {
            FREE(identstr);
            TO_IMPLEMENT("Generic parameter parsing");
         }
         default:
            status = runtime_error(interp->linenr);
            goto _exit;
         }

      FREE(identstr);
      }

   } else {
      error("%s ERROR: parsing other than scalar of vector parameter is not "
            "yet supported\n", __func__);
      return Error_NotImplemented;
   }

   OvfParamPayload payload;
   OvfArgType type = ARG_TYPE_UNSET;

   if (param_gidx < UINT_MAX) {
      type = ARG_TYPE_NESTED;
      payload.gidx = param_gidx;
      S_CHECK(advance(interp, p, &toktype));

   } else if (lindx == 1 && pdef->allow_scalar) {
      assert(isfinite(valp));
      payload.val = valp;
      type = ARG_TYPE_SCALAR;
      S_CHECK(advance(interp, p, &toktype));
   } else if (lindx > 1 && lindx != size_vector) {
      assert(scratch->data);
      assert(isfinite(valp));
      payload.val = valp;
      type = ARG_TYPE_SCALAR;
      S_CHECK(advance(interp, p, &toktype));
   } else if (lindx > 0) {
      payload.vec = scratch->data;
      type = ARG_TYPE_VEC;

      /* If the parameter was parse from reals in the empinfo file,
       * we need to update p with p2.*/
      if (p2 > *p) {
         *p = p2;
      }
      parser_cpypeek2cur(interp);
   } else if (pdef->mandatory) {
      error("[empinterp] Error line %u: unable to parse parameter '%s' for OVF '%s'"
            " got token '%.*s' with type %s\n", interp->linenr, pdef->name, 
            interp->ops->ovf_getname(interp, ovfdef),
            emptok_getstrlen(&interp->peek), emptok_getstrstart(&interp->peek),
            toktype2str(toktype));
      return Error_EMPIncorrectSyntax;
   } else {
      trace_empparser("[empinterp] OVF parameter %s was not found\n",
                      pdef->name);
      return OK;
   }

   trace_empparser("[empinterp] OVF parameter %s parsed with type %s\n",
                   pdef->name, ovf_argtype_str(type));
   S_CHECK(interp->ops->ovf_setparam(interp, ovfdef, pidx, type, payload));

   return OK;

_exit:

   FREE(identstr);
   return status;
}

/**
 *  @brief Parse the OVF declaration within an empinfo file
 *
 *  @param          interp   the interpreter
 *  @param[in,out]  p        the position in the current line
 *
 *  @return                  the error code
 *  */
int parse_ovf(Interpreter * restrict interp, unsigned * restrict p)
{
   /* -----------------------------------------------------------------------
    * We assume that the keyword OVF has already been consumed. Next operations:
    *
    * 1. Get the OVF function name
    * 2. Get the rho variable (the one to be replaced)
    * 3. Parse the argument of the OVF (variables)
    * 4. Parse the parameters (numerical values)
    * ----------------------------------------------------------------------- */

   EmpInfo *empinfo = &interp->mdl->empinfo;

   if (!empinfo->ovf) { S_CHECK(ovfinfo_alloc(empinfo)); }

   TokenType toktype;
   S_CHECK(advance(interp, p, &toktype));
   S_CHECK(parser_expect(interp, "identifier is expected", TOK_IDENT));

   void *ovfdef;
   { /* Block for ctrmem. This is weird because of memory ownership */
   char *name;
   _TOK_GETSTRTMP(&interp->cur, interp->mdl, name);

   S_CHECK(interp->ops->ovf_addbyname(interp, empinfo, name, &ovfdef));
   }

   const char *ovfname;
   A_CHECK(ovfname, interp->ops->ovf_getname(interp, ovfdef));

   /* -----------------------------------------------------------------------
    * 2. Get the rho variable (the one to be replaced)
    * It has to be a single variable 
    * ----------------------------------------------------------------------- */

   S_CHECK(advance(interp, p, &toktype));
   S_CHECK(parser_expect(interp, "OVF variable is expected", TOK_GMS_VAR));

   S_CHECK(interp->ops->ovf_setrhovar(interp, ovfdef));

   /* -----------------------------------------------------------------------
    * 3. Parse all the arguments of variable type to the OVF. At least one
    * argument is expected.
    * ----------------------------------------------------------------------- */

   S_CHECK(advance(interp, p, &toktype));
   S_CHECK(parser_expect(interp, "OVF argument variable is expected", TOK_GMS_VAR));

   while (toktype == TOK_GMS_VAR) {
      S_CHECK(interp->ops->ovf_addarg(interp, ovfdef));
      S_CHECK(advance(interp, p, &toktype));
   }

   /* -----------------------------------------------------------------------
    * 4. Parse all the parameters (numerical values) to the OVF
    * Note that the current token is expected to be a parameter.
    * ----------------------------------------------------------------------- */

   const OvfParamDefList *paramsdef;
   S_CHECK(interp->ops->ovf_paramsdefstart(interp, ovfdef, &paramsdef));

   trace_empparser("[empinterp] OVF '%s' has %u parameter(s)\n", ovfname,
                   *paramsdef->s);

   for (size_t i = 0, len = *paramsdef->s; i < len; ++i) {
      const OvfParamDef *pdef = paramsdef->p[i];

      trace_empparser("[empinterp] parsing parameter '%s' (OVF '%s')\n",
                      pdef->name, ovfname);

      S_CHECK(parse_ovfparam(interp, p, ovfdef, pdef, i));
   }

   trace_empparser("[empinterp] OVF '%s' has been parsed\n", ovfname);

   return interp->ops->ovf_check(interp, ovfdef);
}

int parse_label(Interpreter *interp, unsigned *p)
{
   unsigned pos_ = *p;
   TokenType toktype;
   unsigned label_len = emptok_getstrlen(&interp->cur);
   Model *mdl = interp->mdl;

   char *tmpname /*= _tok_dupident(&interp->cur) */;

   S_CHECK(peek(interp, p, &toktype));

   if (toktype == TOK_LPAREN) {
      tmpname = ctr_ensuremem(&mdl->ctr, label_len, 1);
      strcat(tmpname, "(");
      label_len++;

_loop:
      if (toktype == TOK_SINGLE_QUOTE || toktype == TOK_DOUBLE_QUOTE) {
         S_CHECK(peek(interp, p, &toktype));
      }

      /* Get the ident */
      if (_tok_alnum(&interp->peek, interp->buf, p)) {
         error("[empparser] ERROR in '%s': while scanning for a label, got end-of-file",
               interp->empinfo_fname);
         return Error_EMPIncorrectSyntax;
      }

      unsigned ident_len = emptok_getstrlen(&interp->peek);
      if (ident_len == 0) {
         error("[empparser] expected a string while parsing label with "
               "basename '%.*s' on line %u.\n", emptok_getstrlen(&interp->cur),
               emptok_getstrstart(&interp->cur), interp->linenr);
         error("[empparser] The error occurred at character %u after parsing"
               "'%.*s'", *p, *p - pos_, emptok_getstrstart(&interp->cur));

      }

      tmpname = ctr_ensuremem(&mdl->ctr, label_len, ident_len);
      strncat(tmpname, emptok_getstrstart(&interp->peek), ident_len);
      label_len += ident_len;

      S_CHECK(peek(interp, p, &toktype));
      if (toktype == TOK_SINGLE_QUOTE || toktype == TOK_DOUBLE_QUOTE) {
         S_CHECK(peek(interp, p, &toktype));
      }

      PARSER_EXPECTS(interp, "',' or ')' are expected", TOK_COMMA, TOK_RPAREN);

      if (toktype == TOK_COMMA) {
         tmpname = ctr_ensuremem(&mdl->ctr, label_len, 2);
         strcat(tmpname, ", ");
         label_len += 2;
         goto _loop;
      }

      /* we have a ')' and can therefore exit */
   }

   tmpname = ctr_ensuremem(&mdl->ctr, label_len, 1);
   label_len++;
   strcat(tmpname, ")");

   A_CHECK(interp->cur.payload.label, strdup(tmpname));
   interp->cur.type = TOK_LABEL;
   interp->cur.len = *p - pos_;

   return OK;
}

NONNULL
static GdxReader* _gdxreaders_new(Interpreter* restrict interp)
{
   GdxReaders * restrict gdxreaders = &interp->gdx_readers;
   if (gdxreaders->len >= gdxreaders->max) {
      gdxreaders->max = MAX(2*gdxreaders->max, 2);
      REALLOC_NULL(gdxreaders->list, GdxReader, gdxreaders->max);
   }

   return &gdxreaders->list[gdxreaders->len++];
}

NONNULL
static GdxReader* _gdxreaders_last(Interpreter* restrict interp)
{
   GdxReaders * restrict gdxreaders = &interp->gdx_readers;
   return gdxreaders->len > 0 ? &gdxreaders->list[gdxreaders->len-1] : NULL;
}

NONNULL
static int _parse_gmsopt(Interpreter* restrict interp, unsigned * restrict p,
                         char ** gmsopt)
{
   unsigned lpos = *p;
   assert(interp->read > lpos);
   
   /* The smallest gams option is %gams.scrdir% */
   if (interp->read - lpos < 13*sizeof(char)) {
      errormsg("[empparser] getting to the end of the file while parsing a GAMS option\n");
      return Error_EMPIncorrectSyntax;
   }

   lpos++;
   enum _prefixes {GAMS_OPT, SYSENV_OPT};
   const char * const prefixes[] = {"gams.", "sysenv."};
   enum _gamsopts {GAMS_OPT_SRCDIR, GAMS_OPT_WORKDIR};
   const char * const gamsopts[] = {"scrdir", "workdir"};


   unsigned idx = UINT_MAX;
   for (unsigned i = 0, len = ARRAY_SIZE(prefixes); i < len; ++i) {
      if (strcasecmp(&interp->buf[lpos], prefixes[i]) != 0) {
         idx = i;
         break;
      }
   }

   if (idx == UINT_MAX) {
      error("[empinterp] ERROR line %u: only '%%gams.scrdir%%', '%%gams.workdir%%' "
               "and '%%sysenv.XXX%%' are supported GAMS option\n", interp->linenr);
      return Error_EMPIncorrectSyntax;
   }

   assert(idx == GAMS_OPT || idx == SYSENV_OPT);

   lpos += strlen(prefixes[idx]);

   if (tok_untilwsorchar(&interp->cur, interp->buf, '%', &lpos)) {
      errormsg("[empparser] Reached end-of-file while parsing GAMS options");
      return Error_EMPIncorrectSyntax;
   }

   if (interp->buf[lpos] != '%') {
      error("[empparser] Unexpected ending character '%c' while parsing GAMS options",
            interp->buf[lpos]);
      return Error_EMPIncorrectSyntax;
   }

   unsigned toklen = emptok_getstrlen(&interp->cur);
   const char *tokstr = emptok_getstrstart(&interp->cur);

   if (idx == GAMS_OPT) {
      struct ctrdata_gams *gms = (struct ctrdata_gams*)interp->mdl->ctr.data;
      char buf[GMS_SSSIZE];

      idx = UINT_MAX;
      for (unsigned i = 0, len = ARRAY_SIZE(gamsopts); i < len; ++i) {
         if (strncasecmp(tokstr, gamsopts[i], toklen) != 0) {
            idx = i;
            break;
         }
      }

      if (idx == UINT_MAX) {
         error("[empparser] line %u: unknown GAMS option %.*s. Valid options are:",
               interp->linenr, lpos - *p, &interp->buf[*p]);

         for (unsigned i = 0, len = ARRAY_SIZE(gamsopts); i < len; ++i) {
            error(" %s", gamsopts[i]);
         }
         errormsg("\n");

         return Error_EMPIncorrectSyntax;
      }

      trace_empparser("[empparser] Parsing '%%%s.%s%%'\n", prefixes[GAMS_OPT],
                      gamsopts[idx]);

      switch (idx) {
      case GAMS_OPT_SRCDIR:
         gevGetStrOpt(gms->gev, gevNameScrDir, buf);
         break;
      case GAMS_OPT_WORKDIR:
         gevGetStrOpt(gms->gev, gevNameWrkDir, buf);
         break;
      default:
         error("%s :: unexpected gamsopt index value %u\n", __func__, idx);
         return Error_RuntimeError;
      }

      A_CHECK(*gmsopt, strndup(buf, sizeof(buf)));

   } else if (idx == SYSENV_OPT) {
      const char *envvar = tok_dupident(&interp->cur);
      const char *env = mygetenv(envvar);

      if (!env) {
         error("[empinterp] ERROR line %u: environment variable '%s' undefined",
               interp->linenr, envvar);
         FREE(envvar);
         return Error_EMPRuntimeError;
      }
      FREE(envvar);

      A_CHECK(*gmsopt, strdup(env));

      myfreeenvval(env);

   } else {
      assert(0);
   }
    
   /* +1 is to consume the closing '%' character */
   *p = lpos + 1;

   return OK;
}

int parse_gdxin(Interpreter* restrict interp, unsigned * restrict p)
{
   char *gdxfile = NULL, *gmsopt = NULL;
   int status = OK;


   unsigned p2 = *p;

   /* ---------------------------------------------------------------------
    * We look for the gdx_file name. We have the following case
    * - %gams.XXX%thefile.gdx
    * - relative or absolute path to thefile.gdx
    * --------------------------------------------------------------------- */

   if (parser_skipws(interp, &p2)) {
      goto _err_EOF_fname;
   }

   /* We need to initialize gdxfile */
   A_CHECK(gdxfile, strdup(""));

   char next_char = interp->buf[p2];

   while (next_char != '\n') {
      if (next_char == '%') {
         S_CHECK_EXIT(_parse_gmsopt(interp, &p2, &gmsopt));

         char *gdxfile2;
         IO_CALL_EXIT(asprintf(&gdxfile2, "%s%s", gdxfile, gmsopt));

         FREE(gdxfile);
         FREE(gmsopt);
         gdxfile = gdxfile2;

      } else { /* Here we just copy the string until the EOL or the next '%' */

         if (tok_untilchar(&interp->cur, interp->buf, '%', &p2)) {
            goto _err_EOF_fname;
         }

         char *gdxfile2;
         IO_CALL_EXIT(asprintf(&gdxfile2, "%s%.*s", gdxfile,
                               emptok_getstrlen(&interp->cur),
                               emptok_getstrstart(&interp->cur)));

         FREE(gdxfile);
         gdxfile = gdxfile2;
      }

      next_char = interp->buf[p2];
   } 

   GdxReader *gdxreader;
   A_CHECK_EXIT(gdxreader, _gdxreaders_new(interp));

   S_CHECK(gdx_reader_init(gdxreader, interp->mdl, gdxfile));

   /* The previous call transferred ownership */
   gdxfile = NULL;

   /* The next char is EOL, go past it */
   *p = p2;
   TokenType toktype;
   S_CHECK(advance(interp, p, &toktype));

_exit:
   FREE(gmsopt);
   FREE(gdxfile);

   return status;

_err_EOF_fname:
   FREE(gmsopt);
   FREE(gdxfile);

   errormsg("[empparser] while parsing the filename of 'gdxin', got end-of-file");
   return Error_EMPIncorrectSyntax;

}

DBGUSED static inline bool alias_as_expected(GamsSymData *cursymdata, int type, int idx)
{
   return (cursymdata->type == type) && (cursymdata->idx == idx);
}

int parse_load(Interpreter* restrict interp, unsigned * restrict p)
{
   int status = OK;
   GdxReader *gdxreader = _gdxreaders_last(interp);
   if (!gdxreader) {
      error("[empparser] ERROR line %u: load statement before any gdxin one.\n",
            interp->linenr);
      return Error_EMPIncorrectInput;
   }

   bool symnamegdx_dofree = false;
   const char *symname = NULL, *symnamegdx = NULL;

   /* Skip all whitespace except EOL */
   if (parser_skipwsuntilEOL(interp, p)) {
      goto _err_symname;
   }

   unsigned p2 = *p;

   while (interp->buf[p2] != '\n') {

      if (tok_untilwsorchar(&interp->cur, interp->buf, '=', &p2)) {
         goto _err_symname;
      }

      A_CHECK(symname, tok_dupident(&interp->cur));

      if (interp->buf[p2] == '=') {
         p2++;
         if (_tok_untilws(&interp->peek, interp->buf, &p2)) {
            goto _err_symname;
         }
         A_CHECK_EXIT(symnamegdx, tok_dupident(&interp->peek));
         symnamegdx_dofree = true;

      } else {
         symnamegdx = symname;
         symnamegdx_dofree = false;
      }

      int symidx_;
      if (gdxFindSymbol(gdxreader->gdxh, symname, &symidx_) && symidx_ >= 0) {

         int symdim, symtype;
         char buf[GMS_SSSIZE];
         gdxSymbolInfo(gdxreader->gdxh, symidx_, buf, &symdim, &symtype);
         if (symtype == GMS_DT_EQU || symtype == GMS_DT_VAR) {
            error("[empinterp] ERROR line %u: trying to read a gdx symbol as '%s', "
                  "which has type '%s' in the model. This is not supported for "
                  "'%s' or '%s'. Choose another name.\n", interp->linenr, symname,
                  gmsGdxTypeText[symtype], gmsGdxTypeText[GMS_DT_EQU],
                  gmsGdxTypeText[GMS_DT_VAR]);
            status = Error_EMPIncorrectInput;
            goto _exit;
         }
      }

      /* Read in the gdx file */
      S_CHECK_EXIT(gdx_reader_readsym(gdxreader, symnamegdx));

      int type = gdxreader->cursym.type;

      switch ((GdxSymType)type) {
      case dt_set: {
         assert(gdxreader->setobj.len > 0 && gdxreader->setobj.arr);
         int symdim = gdxreader->cursym.dim;

         if (gdxreader->cursym.dim == 1) {

            S_CHECK_EXIT(namedints_add(&interp->globals.sets, gdxreader->setobj, symname));

         } else if (symdim > 1) {

            int symidx = gdxreader->cursym.idx;
            S_CHECK_EXIT(multisets_add(&interp->globals.multisets,
                                       (MultiSet){.dim = symdim, .idx = symidx, .gdxreader = gdxreader},
                                       symname));
         }

         symname = NULL; /* namedints_add takes ownership of it */
         break;
      }
      case dt_alias: {
         GdxSymType alias_type = gdxreader->cursym.alias_type;
         DBGUSED int alias_idx = gdxreader->cursym.alias_idx;
         unsigned idx, dim = gdxreader->cursym.dim;
         GdxAlias a = {.gdxtype = alias_type, .type = gdxsymtype_identtype(alias_type, dim), .dim = dim };
         A_CHECK_EXIT(a.name, strdup(gdxreader->cursym.alias_name));

         switch (alias_type) {
         case dt_set: {
            if (dim == 1) {
               idx = namedints_findbyname_nocase(&interp->globals.sets, a.name);

               if (idx < UINT_MAX) {
                  a.index = idx;
               } else {
                  status = gdx_reader_readsym(gdxreader, a.name);
                  if (status != OK) { FREE(a.name); goto _exit; }
                  assert(alias_as_expected(&gdxreader->cursym, alias_type, alias_idx));

                  S_CHECK_EXIT(namedints_add(&interp->globals.sets, gdxreader->setobj, a.name));
                  a.index = interp->globals.sets.len-1;
               }
            } else {
               idx = multisets_findbyname_nocase(&interp->globals.multisets, a.name);

               if (idx < UINT_MAX) {
                  a.index = idx;
               } else {
                  status = gdx_reader_readsym(gdxreader, a.name);
                  if (status != OK) { FREE(a.name); goto _exit; }
                  assert(alias_as_expected(&gdxreader->cursym, alias_type, alias_idx));

                  S_CHECK_EXIT(multisets_add(&interp->globals.multisets, gdxreader->multiset, a.name));
                  a.index = interp->globals.sets.len-1;
               }
            }
            break;
         }
         default:
            error("[empinterp] ERROR: in gdxfile '%s', the symbol '%s' is an "
                  "alias, but only alias for sets are supported for now\n",
                  gdxreader->fname, symnamegdx);
            FREE(a.name);
            return Error_NotImplemented;
         }

         const char *tmpname = symname;
         symname = NULL;
         S_CHECK_EXIT(aliases_add(&interp->globals.aliases, a, tmpname));
         break;
      }
      case dt_par: {
         int symdim = gdxreader->cursym.dim;

         if (symdim == 0) {
            S_CHECK_EXIT(namedscalar_add(&interp->globals.scalars, gdxreader->scalar, symname));
         } else if (symdim == 1) {
            S_CHECK_EXIT(namedvec_add(&interp->globals.vectors, *gdxreader->vector, symname));
         } else if (symdim > 1) {
            TO_IMPLEMENT_EXIT("Param parsing with dim > 1");
         }

         symname = NULL; /* namedints_add takes ownership of it */
         break;
      }
      case dt_equ:
         error("[empinterp] ERROR line %u: symbol '%s' is an equation in gdx '%s'\n",
               interp->linenr, symnamegdx, gdxreader->fname);
         status = Error_EMPRuntimeError;
         goto _exit;
      case dt_var:
         error("[empinterp] ERROR line %u: symbol '%s' is a variable in gdx '%s'\n",
               interp->linenr, symnamegdx, gdxreader->fname);
         status = Error_EMPRuntimeError;
         goto _exit;
      }

      if (symnamegdx_dofree) {
         FREE(symnamegdx);
      }
      /* Skip all whitespace except EOL */
      if (parser_skipwsuntilEOL(interp, &p2)) {
         goto _err_symname;
      }
   }

   if (symnamegdx_dofree) {
      FREE(symnamegdx);
   }
   /* The next char is EOL, go past it */
   *p = p2;
   TokenType toktype;

   return advance(interp, p, &toktype);

_exit:
   if (symnamegdx_dofree) {
      FREE(symnamegdx);
   }
   FREE(symname);

   return status;

_err_symname:
   error("[empparser] while parsing for symbol to load in gdx file '%s', got end-of-file",
         gdxreader->fname);

   if (symnamegdx_dofree) {
      FREE(symnamegdx);
   }
   FREE(symname);

   return Error_EMPIncorrectSyntax;

}

/**
 * @brief This function adds a node to the EMPDAG with the appropriate type
 *
 * @param interp  the interpreter
 * @param p       the position pointer
 *
 * @return        the error code
 */
int parse_labeldef(Interpreter * restrict interp, unsigned *p)
{
   /* ---------------------------------------------------------------------
    * We tentatively seek to parse a statement of the form
    *
    * basename(set, 'e')$(cond(set)): (min|max|Nash)
    *
    * where the indices (set, 'e') and the conditional $(cond(set)) are optional.
    * --------------------------------------------------------------------- */

   /* TODO URG: should this be deleted? */
   parser_update_last_kw(interp);

   const char *identname = emptok_getstrstart(&interp->cur);
   unsigned identname_len = emptok_getstrlen(&interp->cur);

   TokenType toktype;
   S_CHECK(advance(interp, p, &toktype));

   GmsIndicesData gmsindices = {.nargs = 0};
   if (toktype == TOK_LPAREN) {
      S_CHECK(parse_gmsindices(interp, p, &gmsindices));
      S_CHECK(advance(interp, p, &toktype));
   }

   if (toktype == TOK_CONDITION) {
      S_CHECK(chk_compat_gmsindices_condition(&gmsindices, interp->linenr));

      return vm_labeldef_condition(interp, p, identname, identname_len, &gmsindices);
   }

   if (gmsindices.num_sets > 0 || gmsindices.num_localsets > 0 || gmsindices.num_iterators > 0) {
      return vm_labeldef_loop(interp, p, identname, identname_len, &gmsindices);
   }

   /* ---------------------------------------------------------------------
    * If we are past this point, we are in the immediate mode
    * --------------------------------------------------------------------- */

   if (interp->ops->type != ParserOpsImm) {
      error("[empinterp] line %u: runtime error: no GAMS set to iterate over, "
            "but the interpreter is in VM mode. Review the model to make sure "
            "that any loop involves Please report this as a bug.\n",
            interp->linenr);
      return Error_EMPRuntimeError;
   }

   if (toktype != TOK_COLON) {
      error("[empparser] Error line %u: unexpected token '%.*s' of type '%s'. "
            "Only valid token would ':' for completing the label named '%.*s'.\n",
            interp->linenr, emptok_getstrlen(&interp->cur),
            emptok_getstrstart(&interp->cur), toktype2str(emptok_gettype(&interp->cur)),
            identname_len, identname);
      return Error_EMPIncorrectSyntax;
   }

   S_CHECK(imm_set_regentry(interp, identname, identname_len, &gmsindices));

   S_CHECK(labdeldef_parse_statement(interp, p));

   return OK;
}

static inline void inc_linenr(Interpreter *interp, unsigned p)
{
   interp->linenr++;
   interp->linestart = &interp->buf[p];
}

int skip_spaces_commented_lines(Interpreter *interp, unsigned *p)
{

   unsigned _p = (*p);
   const char * restrict buf = interp->buf;

  /* ----------------------------------------------------------------------
   * We want to skip over spaces and commented lines at the top
   * ---------------------------------------------------------------------- */

   while(isspace(buf[_p])) { if (buf[_p] == '\n') { inc_linenr(interp, _p+1); } _p++; }

   while (buf[_p] == '*') {
      while (buf[_p] != '\0' && buf[_p] != EOF && buf[_p] != '\n') _p++;

      while(isspace(buf[_p])) { if (buf[_p] == '\n') { inc_linenr(interp, _p+1); } _p++; }
   }

   *p = _p;

   return (buf[_p] == '\0' || buf[_p] == EOF);
}

NONNULL static int create_base_mp(Interpreter *interp, const char *mp_name,
                                  MathPrgm **mp_)
{
   Model * restrict mdl = interp->mdl;
   RhpSense sense;
   S_CHECK(mdl_getsense(mdl, &sense));

   if (!(sense == RhpMin || sense == RhpMax)) {
      error("%s :: no valid objective sense given\n", __func__);
      return Error_Inconsistency;
   }

   /* Lazy way. Do not look for the immediate ops functions*/
   MathPrgm *mp;

   assert(!interp->regentry);
   A_CHECK(interp->regentry, regentry_new(mp_name, strlen(mp_name), 0));
   S_CHECK(interp->ops->mp_new(interp, sense, &mp));
   FREE(interp->regentry);
   interp->regentry = NULL;

   rhp_idx objvar;
   S_CHECK(mdl_getobjvar(mdl, &objvar));

   if (objvar == IdxNA) {
      error("[empinterp] ERROR: while parsing %s keyword: an objective"
            " variable must be specified in the solve statement, but none have "
            "been given!\n", toktype2str(interp->cur.type));
      return Error_EMPIncorrectInput;
   }

   S_CHECK(mp_setobjvar(mp, objvar));

   *mp_ = mp;

   return OK;
}

int parse_bilevel(Interpreter *interp, unsigned *p)
{
   /* ------------------------------------------------------------
    * Here comes the definition of the bilevel problem in GAMS
    *
    * the syntax for the bilevel keyword is
    *
    * bilevel {varupper}
    *    { min objequ {varlower} {constraintslower} }
    *    { vi {equ var} }
    *
    * see https://www.gams.com/latest/docs/UG_EMP_Bilevel.html
    * ------------------------------------------------------------ */

  /* ----------------------------------------------------------------------
   * Sanity check: bilevel is incompatible with DAG, equilibrium, ...
   * ---------------------------------------------------------------------- */

   S_CHECK(bilevel_sanity_check(interp));

   S_CHECK(interp_ops_is_imm(interp));

   bilevel_in_progress(interp);

   /* Save the keyword info in case we parse another keyword */
   KeywordLexemeInfo bilevel_kw_info = interp->last_kw_info;

   int status = OK;

  /* --------------------------------------------------------------------
   * Create the upper level MP
   * -------------------------------------------------------------------- */

   Model * restrict mdl = interp->mdl;
   EmpDag * restrict empdag = &mdl->empinfo.empdag;

   MathPrgm *mp_upper;
   S_CHECK(create_base_mp(interp, "upper level", &mp_upper));

   /* ------------------------------------------------------------------
    * We are ready to parse the bilevel statement
    * ------------------------------------------------------------------ */

   TokenType toktype;
   S_CHECK(advance(interp, p, &toktype))

   /* ------------------------------------------------------------------
    * Read variables belonging to the upper problem
    * ------------------------------------------------------------------ */

   while (toktype == TOK_GMS_VAR) {
      S_CHECK(interp->ops->mp_addvars(interp, mp_upper));
      S_CHECK(advance(interp, p, &toktype));
   }

   /* ---------------------------------------------------------------------
    * We attempt to parse at least one MP declaration, OPT or VI.
    * --------------------------------------------------------------------- */

   PARSER_EXPECTS_EXIT(interp, "optimization or VI problem",
                       TOK_MIN, TOK_MAX, TOK_VI);

   mpeid_t nash_lower = MpeId_NA;

   mpid_t mpid_upper = mp_upper->id;
   bool toktype_is_dagnode_kw = toktype_dagnode_kw(toktype);
   while (toktype_is_dagnode_kw || toktype == TOK_DUALEQU) {

      if (toktype == TOK_DUALEQU) {
         S_CHECK(parse_dualequ_equvar(interp, p));
      } else {
         S_CHECK(parse_mp(interp, p));
      }

      toktype = parser_getcurtoktype(interp);
      toktype_is_dagnode_kw = toktype_dagnode_kw(toktype);

      /* ----------------------------------------------------------------------
       * Adding the first lower level problem needs to be done after parsing
       * the first keyword after the declaration:
       * - If it is min|max|vi, then we have a Nash equilibrium as child. This
       *   gets added as a child to the upper MP. Then, all MP in the empinfo
       *   are children of the equilibrium. 
       * - Otherwise, we only have 1 MP and we set it as the only child of the
       *   upper MP
       *
       * TODO dualvar could appear in and we might want to support it
       * ---------------------------------------------------------------------- */

      mpid_t mpid_last = empdag->mps.len-1;
      if (nash_lower == MpeId_NA) {

         if (toktype_is_dagnode_kw || toktype == TOK_DUALEQU) {

            S_CHECK(empdag_addmpenamed(empdag, strdup("lower level Nash"), &nash_lower));
            S_CHECK(empdag_mpCTRLmpebyid(empdag, mpid_upper, nash_lower))
            S_CHECK(empdag_mpeaddmpbyid(empdag, nash_lower, mpid_last));

         } else {
            S_CHECK(empdag_mpCTRLmpbyid(empdag, mpid_upper, mpid_last))
         }
      } else {
         S_CHECK(empdag_mpeaddmpbyid(empdag, nash_lower, mpid_last));
      }
   }

  /* ----------------------------------------------------------------------
   * In the bilevel case, all the equations not assigned to lower level problems
   * are shoved into the upper lever one ...
   * ---------------------------------------------------------------------- */

   EquMeta * restrict equmeta = mdl->ctr.equmeta;

   for (unsigned i = 0, len = ctr_nequs(&mdl->ctr); i < len; ++i) {
      if (!valid_mpid(equmeta[i].mp_id)) {
         S_CHECK(mp_addconstraint(mp_upper, i));
      }
   }

   S_CHECK(mp_finalize(mp_upper));

   /* This must be last */
   parsed_bilevel(interp);

   return OK;

_exit:
   interp->last_kw_info = bilevel_kw_info;
   return status;
}

NONNULL static
int dualequ_start_error(TokenType toktype, Interpreter *interp)
{
   if (toktype == TOK_REAL && interp->cur.payload.real == 0.) {

      error("[empinterp] ERROR on line %u: invalid token '0' in an dualequ. " 
            "'0' is only valid in a VI statement\n", interp->linenr);

   } else if (toktype == TOK_GMS_VAR) {

      error("[empinterp] ERROR on line %u: variable '%.*s' has no preceding "
            "equation. In an dualequ, no 'zerofunc' variable can be given. " 
            "This is only valid in a VI statement\n", interp->linenr,
            emptok_getstrlen(&interp->cur), emptok_getstrstart(&interp->cur));

   } else {

      error("[empinterp] ERROR line %u: empty dualequ declaration\n",
            interp->linenr);

   }

   return Error_EMPIncorrectSyntax;
}

static int parse_dualequ_equvar(Interpreter * restrict interp,
                                unsigned * restrict p)
{
   int status = OK;

   /* Save the keyword info in case we parse another keyword */
   KeywordLexemeInfo dualequ_kw_info = interp->last_kw_info;

   /* ------------------------------------------------------------------
    * Create a VI MP
    * ------------------------------------------------------------------ */

   MathPrgm *mp;
   S_CHECK(interp->ops->mp_new(interp, RhpFeasibility, &mp));
   S_CHECK(interp->ops->mp_settype(interp, mp, RHP_MP_VI));

   TokenType toktype;
   S_CHECK(advance(interp, p, &toktype))

   /* ------------------------------------------------------------------
    * Read equations/label belonging to this MP
    * ------------------------------------------------------------------ */

   if (toktype != TOK_GMS_EQU && toktype != TOK_MINUS) {
      status = dualequ_start_error(toktype, interp);
      goto _exit;
   }

   while (toktype == TOK_GMS_EQU || toktype == TOK_MINUS) {

      bool is_flipped = false;

      if (toktype == TOK_MINUS) {
         is_flipped = true;
         S_CHECK(advance(interp, p, &toktype));
         S_CHECK_EXIT(parser_expect(interp, "In a 'dualequ' statement, an equation "
                                    "is expected after '-'", TOK_GMS_EQU));
      }

      interp_save_tok(interp);

      S_CHECK(advance(interp, p, &toktype));

      S_CHECK_EXIT(parser_expect(interp, "In a 'dualequ' statement, a variable must "
                                 "follow an equation", TOK_GMS_VAR));
      S_CHECK(interp->ops->mp_addvipairs(interp, mp));

      if (is_flipped) {
         S_CHECK(interp->ops->ctr_markequasflipped(interp));
      }

      S_CHECK(advance(interp, p, &toktype));
   }

   S_CHECK(interp->ops->mp_finalize(interp, mp));

   return OK;

_exit:
   interp->last_kw_info = dualequ_kw_info;
   return status;
}

int parse_dualequ(Interpreter * restrict interp, unsigned * restrict p)
{

  /* ----------------------------------------------------------------------
   * OLD JAMS SYNTAX
   *
   * dualequ statement are of the form: 
   *
   * dualequ [-] equ var
   *
   * This keyword is only valid in an old style empinfo file and in a long
   * form statement.
   *
   * We could have multiple dualequ 
   * ---------------------------------------------------------------------- */

   S_CHECK(dualequ_sanity_check(interp));

   S_CHECK(interp_ops_is_imm(interp));

   /* JAMS barfs at a model without a long form */
   S_CHECK(has_longform_solve(interp));

  /* ----------------------------------------------------------------------
   * More checks
   * ---------------------------------------------------------------------- */

   EmpDag *empdag = &interp->mdl->empinfo.empdag;
   if (!_has_dualequ(interp) && empdag->roots.len > 0) {

      int offset;
      error("[empinterp] %nERROR: first time parsing the 'dualequ' keyword, "
            "expecting an empty EMPDAG, but found root(s):\n", &offset);

      for (unsigned i = 0, len = empdag->roots.len; i < len; ++i) {
         error("%*s[%5u]%s\n", offset, "", i,
               empdag_getname(empdag, empdag->roots.arr[i]));
      }

      return runtime_error(interp->linenr);
   }

   if (_has_dualequ(interp) && empdag->roots.len != 1) {

      int offset;
      error("[empinterp] %nERROR: the 'dualequ' keyword has already being "
            "parsed, but there are %u EMPDAG roots, rather than 1\n", &offset,
            empdag->roots.len);

      for (unsigned i = 0, len = empdag->roots.len; i < len; ++i) {
         error("%*s[%5u]%s\n", offset, "", i,
               empdag_getname(empdag, empdag->roots.arr[i]));
      }

      return runtime_error(interp->linenr);
   }
   
  /* ----------------------------------------------------------------------
   * This is the meat of the function
   * The first time a dualequ is parsed, we need to create a Nash node as an
   * EMPDAG root node.
   * ---------------------------------------------------------------------- */

   mpeid_t root;
   if (empdag->roots.len == 1) {
      daguid_t r = empdag->roots.arr[0];
      if (uidisMP(r)) {
         error("[empinterp] ERROR: EMPDAG root is an MP node, rather than "
               "expected MPE: %s\n", empdag_getname(empdag, r));
         return runtime_error(interp->linenr);
      }

      root = r;

   } else {
      S_CHECK(empdag_addmpenamed(empdag, strdup("Equilibrium"), &root));

      MathPrgm *mp;
      S_CHECK(create_base_mp(interp, "OPT MP", &mp));
      S_CHECK(empdag_mpeaddmpbyid(empdag, root, mp->id));

      /* This MP owns all remaining variables and equation */
      S_CHECK(chk_wildcard_vars_allowed(interp));
      S_CHECK(chk_wildcard_equs_allowed(interp));
      interp->finalize.mp_owns_remaining_vars = mp->id;
      interp->finalize.mp_owns_remaining_equs = mp->id;
   }

   S_CHECK(parse_dualequ_equvar(interp, p));

   mpid_t mp_dualequ = empdag->mps.len-1;
   S_CHECK(empdag_mpeaddmpbyid(empdag, root, mp_dualequ));

   parsed_dualequ(interp);

   return OK;

}

int parse_dualvar(Interpreter * restrict interp, unsigned * restrict p)
{
  /* ----------------------------------------------------------------------
   * dualvar statement are of the form: 
   *
   * dualvar var [-] equ
   *
   * This keyword is valid in JAMS compatibility mode and EMPDAG
   *
   * TODO: minus case
   * ---------------------------------------------------------------------- */
   TokenType toktype;
   S_CHECK(advance(interp, p, &toktype))

   S_CHECK(parser_expect(interp, "expected a GAMS variable", TOK_GMS_VAR));

   S_CHECK(advance(interp, p, &toktype))

   PARSER_EXPECTS(interp, "in a dualvar statement, after the variable a "
                  "(potentially flipped) equation is expected", TOK_MINUS,
                  TOK_GMS_EQU);

   S_CHECK(advance(interp, p, &toktype))
   return OK;
}

static int parse_modeltype(Interpreter * restrict interp, unsigned * restrict p)
{
  /* ----------------------------------------------------------------------
   * OLD JAMS SYNTAX
   *
   * modeltype statement are of the form: 
   *
   * modeltype (mcp|nlp|minlp)
   *
   * This keyword is valid in JAMS compatibility mode
   * ---------------------------------------------------------------------- */
   TokenType toktype;
   S_CHECK(advance(interp, p, &toktype))

   PARSER_EXPECTS(interp, "after modeltype, expecting nlp or mcp or minlp", TOK_NLP, TOK_MCP, TOK_MINLP);

   switch (toktype) {
   case TOK_NLP:
      rhp_options[Options_SolveSingleOptAs].value.i = Opt_SolveSingleOptAsOpt;
      break;
   case TOK_MCP:
      rhp_options[Options_SolveSingleOptAs].value.i = Opt_SolveSingleOptAsMcp;
      break;
   case TOK_MINLP:
      errormsg("[emparser] ERROR: MINLP is not yet a supported modeltype\n");
      return Error_NotImplemented;
   default:
      return runtime_error(interp->linenr);
   }

   return advance(interp, p, &toktype);
}

int labdeldef_parse_statement(Interpreter* restrict interp, unsigned* restrict p)
{
   TokenType toktype;
   S_CHECK(advance(interp, p, &toktype))
   PARSER_EXPECTS(interp, "after a label definition, expecting a problem statement "
                  "(min, max, vi, Nash)", TOK_MIN, TOK_MAX, TOK_VI, TOK_NASH, TOK_VF);
   
   // mark that we have parse an EMPDAG node
   parsed_dag_node(interp);

   switch(toktype) {
   case TOK_MAX:
   case TOK_MIN:
   case TOK_VI:
      return parse_mp(interp, p);
      break;
   case TOK_NASH:
      return parse_Nash(interp, p);
      break;
   case TOK_VF:
      S_CHECK(parse_VF_CCF(NULL, interp, p));
      return advance(interp, p, &toktype);
   default:
      return runtime_error(interp->linenr);
   }

}

static inline int expect_statement(Interpreter * restrict interp)
{
   switch (interp->cur.type) {
   case TOK_BILEVEL:
   case TOK_DAG:
   case TOK_DUALEQU:
   case TOK_DUALVAR:
   case TOK_EOF:
   case TOK_EQUILIBRIUM:
   case TOK_EXPLICIT:
   case TOK_GDXIN:
   case TOK_IDENT:
   case TOK_IMPLICIT:
   case TOK_LOAD:
   case TOK_LOOP:
   case TOK_MAX:
   case TOK_MIN:
   case TOK_MODELTYPE:
   case TOK_NASH:
   case TOK_OVF:
   case TOK_SHAREDEQU:
   case TOK_VALFN:
   case TOK_VI:
   case TOK_VISOL:
      return OK;
   default:
      parser_err(interp, "expecting a statement");
      return Error_EMPIncorrectSyntax;
   }
}

int process_statements(Interpreter * restrict interp, unsigned * restrict p,
                       TokenType toktype)
{
   int status = OK;

   S_CHECK(expect_statement(interp));

   while (true) {

   switch (toktype) {
   case TOK_EOF:
      goto _exit;
   case TOK_BILEVEL:
      S_CHECK_EXIT(parse_bilevel(interp, p));
      break;
   case TOK_DAG:
      S_CHECK_EXIT(parse_DAG(interp, p));
      break;
   case TOK_DEFVAR:
      S_CHECK_EXIT(parse_defvar(interp, p));
      break;
   case TOK_DUALEQU:
      S_CHECK_EXIT(parse_dualequ(interp, p));
      break;
   case TOK_DUALVAR:
      S_CHECK_EXIT(parse_dualvar(interp, p));
      break;
   case TOK_EQUILIBRIUM:
      S_CHECK_EXIT(parse_equilibrium(interp));
      S_CHECK_EXIT(advance(interp, p, &toktype));
      break;
   case TOK_EXPLICIT:
      TO_IMPLEMENT_EXIT("EMPINFO EXPLICIT")
   case TOK_GDXIN:
      S_CHECK_EXIT(parse_gdxin(interp, p));
      break;
   case TOK_IMPLICIT:
      TO_IMPLEMENT_EXIT("EMPINFO IMPLICIT")
   case TOK_LOOP:
      S_CHECK_EXIT(parse_loop(interp, p));
      break;
   case TOK_LOAD:
      S_CHECK_EXIT(parse_load(interp, p));
      break;
   case TOK_MAX:
   case TOK_MIN:
   case TOK_VI:
      S_CHECK_EXIT(parse_mp(interp, p));
      break;
   case TOK_MODELTYPE:
      S_CHECK_EXIT(parse_modeltype(interp, p));
      break;
   case TOK_NASH:
      S_CHECK_EXIT(parse_Nash(interp, p));
      break;
   case TOK_OVF:
      S_CHECK_EXIT(parse_ovf(interp, p));
      break;
   case TOK_SHAREDEQU:
      TO_IMPLEMENT_EXIT("EMPINFO SHAREDEQU")
   case TOK_VALFN:
      TO_IMPLEMENT_EXIT("EMPINFO VALFN")
   case TOK_VISOL:
      TO_IMPLEMENT_EXIT("EMPINFO VISOL")
   case TOK_IDENT: {
      interp_save_tok(interp);
      unsigned p2 = *p;
      S_CHECK_EXIT(peek(interp, &p2, &toktype));
      switch (toktype) {
      case TOK_DEFVAR:
         *p = p2;
         parser_cpypeek2cur(interp);
         S_CHECK_EXIT(parse_defvar(interp, p));
         goto _next;
      default: ;
      }

      /* ------------------------------------------------------------
       * If the next token is a colon ':' and after that a sense kw,
       * then we have a valid node declaration
       *
       * TODO: we could be in a declaration and just parsed a label
       * ------------------------------------------------------------ */

      S_CHECK_EXIT(parse_labeldef(interp, p));

      break;
   }
   default:
     goto _exit;
   }
_next:
   toktype = parser_getcurtoktype(interp);
   }

_exit:
   return status;
}


