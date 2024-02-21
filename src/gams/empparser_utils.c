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
      S_CHECK(advance(interp, p, &toktype));
   } else if (toktype == TOK_DOUBLE_QUOTE) {
      has_double_quote = true;
      S_CHECK(advance(interp, p, &toktype));
   }

   PARSER_EXPECTS(interp, "A string (UEL (set element), subset, variable) is required",
                  TOK_GMS_SET, TOK_GMS_UEL, TOK_STAR, TOK_REAL, TOK_IDENT);

   /* -----------------------------------------------------------------
    * If we have a real, it could still be a UEL
    * ------------------------------------------------------- */

   if (toktype == TOK_REAL && (has_single_quote || has_double_quote)) {
      S_CHECK(gms_find_ident_in_dct(interp, &interp->cur));
      toktype = parser_getcurtoktype(interp);
      if (toktype != TOK_GMS_UEL) {
         error("%s :: the number '%.*s' is not a UEL\n", __func__,
               interp->cur.len, interp->cur.start);
         return Error_EMPIncorrectInput;
      }
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

   return OK;

_finalize:

   /* TODO: do we require UELs to be quoted? */

   if (!has_single_quote && !has_double_quote) { return OK;}

   S_CHECK(advance(interp, p, &toktype));

   if (has_single_quote) {
      S_CHECK(parser_expect(interp, "Closing \"'\" expected", TOK_SINGLE_QUOTE));
   } else if (has_double_quote) {
      S_CHECK(parser_expect(interp, "Closing '\"' expected", TOK_DOUBLE_QUOTE));
   }

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
 * parse_gmsindices, we just disallow  '*'
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
         errormsg("[empcompiler] ERROR: '*' is not a valid index in a label "
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


