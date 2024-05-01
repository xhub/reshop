#include "empinterp.h"
#include "empinterp_priv.h"
#include "empinterp_utils.h"
#include "empparser.h"
#include "empparser_priv.h"
#include "empparser_utils.h"

static int resolve_tokenasgmsidx(Interpreter * restrict interp, unsigned * restrict p,
                                 GmsIndicesData * restrict idxdata,
                                 unsigned idx)
{
   assert(idx < GMS_MAX_INDEX_DIM);
   bool has_single_quote = false, has_double_quote = false;
   IdentData *data = &idxdata->idents[idx];

   /* get the next token */
   TokenType toktype;
   S_CHECK(advance(interp, p, &toktype));

   if (toktype == TOK_SINGLE_QUOTE) {
      has_single_quote = true;
   } else if (toktype == TOK_DOUBLE_QUOTE) {
      has_double_quote = true;
   }

   if (has_single_quote || has_double_quote) {
      char quote = toktype == TOK_SINGLE_QUOTE ? '\'' : '"';
      S_CHECK(parser_asUEL(interp, p, quote, &toktype));

      if (toktype == TOK_UNSET) {
         const Token *tok = &interp->cur;
         error("[empinterp] ERROR line %u: %c%.*s%c is not a UEL\n", interp->linenr,
               quote, tok->len, tok->start, quote);
         return Error_EMPIncorrectSyntax;
      }

      if (toktype != TOK_STAR && toktype != TOK_GMS_UEL) {
         return runtime_error(interp->linenr);
      }

   } else {
      S_CHECK(advance(interp, p, &toktype));
      PARSER_EXPECTS(interp, "A string (subset, variable) is required",
                     TOK_GMS_SET, TOK_STAR, TOK_IDENT);

   }

   switch (toktype) {
   case TOK_GMS_UEL:
      data->type = IdentUEL;
      data->idx = interp->cur.gms_dct.idx; 
      data->dim = 0;

      goto _finalize;
   case TOK_STAR:
      data->type = IdentUniversalSet;
      data->idx = 0; 
      data->dim = 0;

      goto _finalize;
   case TOK_IDENT:
      break; /* We deal with this case now */
   case TOK_GMS_SET:
      TO_IMPLEMENT("Token TOK_GMS_SET");
      break; 
   default:
      error("%s :: unexpected failure.\n", __func__);
      return Error_RuntimeError;
   }

   /* ---------------------------------------------------------------------
    * This code is executed only if tok == TOK_IDENT
    * --------------------------------------------------------------------- */
   assert(toktype == TOK_IDENT);

   RESOLVE_IDENTAS(interp, data, "GAMS index must fulfill these conditions.",
                   IdentLoopIterator, IdentLocalSet, IdentSet);

   switch (data->type) {
   case IdentLocalSet:
      idxdata->num_localsets++;
      break;
   case IdentSet:
      idxdata->num_sets++;
      break;
   case IdentLoopIterator:
      idxdata->num_iterators++;
      break;
   default:
      return runtime_error(interp->linenr);
   }

_finalize:

   return OK;
}

/**
 * @brief Function to parse gams indices
 *
 * This takes care of parsing the indices of a GAMS symbol or label
 *
 * @param interp 
 * @param p 
 * @param idxdata 
 * @return 
 */
int parse_gmsindices(Interpreter * restrict interp, unsigned * restrict p,
                     GmsIndicesData * restrict idxdata)
{
   TokenType toktype;
   unsigned nargs = 0;

  /* ----------------------------------------------------------------------
   * We are expecting to be called at gmssymb('a', '1')
   *                                          ^
   * On exit, we 
   * ---------------------------------------------------------------------- */

   do {
      if (nargs == GMS_MAX_INDEX_DIM) {
         error("[empinterp] ERROR line %u: while parsing the arguments to the "
               "token '%.*s', more than %u were parsed.\n", interp->linenr,
               emptok_getstrlen(&interp->pre), emptok_getstrstart(&interp->pre),
               GMS_MAX_INDEX_DIM);
         return Error_EMPIncorrectSyntax;
      }

      S_CHECK(resolve_tokenasgmsidx(interp, p, idxdata, nargs));

      S_CHECK(advance(interp, p, &toktype));

      nargs++;

   } while (toktype == TOK_COMMA);

   idxdata->nargs = nargs;

   return parser_expect(interp, "Closing ')' expected for GAMS indices", TOK_RPAREN);

}

/**
 * @brief Function to parse gams indices
 *
 * This takes care of parsing the indices of a label definition. Compared to
 * parse_gmsindices, we disallow '*'
 *
 * @param interp 
 * @param p 
 * @param idxdata 
 * @return 
 */
int parse_labeldefindices(Interpreter * restrict interp, unsigned * restrict p,
                          GmsIndicesData * restrict idxdata)
{
   TokenType toktype;
   unsigned nargs = 0;

  /* ----------------------------------------------------------------------
   * We are parsing n('a',...):
   *                  ^
   * TODO: we should add UELs that cannot be found in the DCT
   * ---------------------------------------------------------------------- */

   do {
      if (nargs == GMS_MAX_INDEX_DIM) {
         error("[empinterp] ERROR line %u: while parsing the arguments to the "
               "token '%.*s', more than %u were parsed.\n", interp->linenr,
               emptok_getstrlen(&interp->pre), emptok_getstrstart(&interp->pre),
               GMS_MAX_INDEX_DIM);
         return Error_EMPIncorrectSyntax;
      }

      S_CHECK(resolve_tokenasgmsidx(interp, p, idxdata, nargs));
      if (idxdata->idents[nargs].type == IdentUniversalSet) {
         errormsg("[empinterp] ERROR: '*' is not a valid index in a label "
                  "definition\n");
         return Error_EMPIncorrectSyntax;
      }

      S_CHECK(advance(interp, p, &toktype));

      nargs++;

   } while (toktype == TOK_COMMA);

   idxdata->nargs = nargs;

   return parser_expect(interp, "Closing ')' expected for GAMS indices", TOK_RPAREN);

}
/**
 * @brief Function to parse loop sets
 *
 * It is similar as parse_gmsindices(), but only IdentSet and IdentLocalSet
 * are allowed
 *
 * @param interp 
 * @param p 
 * @param idxdata 
 * @return 
 */
int parse_loopsets(Interpreter * restrict interp, unsigned * restrict p,
                   GmsIndicesData * restrict idxdata)
{
   assert(emptok_gettype(&interp->cur) == TOK_LPAREN);

   TokenType toktype;
   unsigned nargs = 0;

   do {
      if (nargs == GMS_MAX_INDEX_DIM) {
         error("[empinterp] ERROR line %u: while parsing the sets to loop over, "
               "more than %u were parsed.\n", interp->linenr, GMS_MAX_INDEX_DIM);
         return Error_EMPIncorrectSyntax;
      }

      /* get the next token */
      S_CHECK(advance(interp, p, &toktype));
      S_CHECK(parser_expect(interp, "Sets to loop over must are identifiers",
                            TOK_IDENT));

      IdentData *data = &idxdata->idents[nargs];
      RESOLVE_IDENTAS(interp, data, "Loop indices must fulfill these conditions.",
                      IdentLocalSet, IdentSet);

      switch (idxdata->idents[nargs].type) {
      case IdentLocalSet:
         idxdata->num_localsets++;
         break;
      case IdentSet:
         idxdata->num_sets++;
         break;
      default:
         return runtime_error(interp->linenr);
      }

      nargs++;

      S_CHECK(advance(interp, p, &toktype));

   } while (toktype == TOK_COMMA);

   idxdata->nargs = nargs;

   return parser_expect(interp, "Closing ')' expected for loop set(s).", TOK_RPAREN);
}


