#include "empinterp.h"
#include "empinterp_ops_utils.h"
#include "empinterp_priv.h"
#include "empinterp_utils.h"
#include "empinterp_vm_compiler.h"
#include "empparser.h"
#include "empparser_priv.h"
#include "empparser_utils.h"

static int resolve_tokenasgmsidx(Interpreter * restrict interp, unsigned * restrict p,
                                 GmsIndicesData * restrict gmsindices,
                                 unsigned idx)
{
   assert(idx < GMS_MAX_INDEX_DIM);
   IdentData *ident = &gmsindices->idents[idx];

   /* get the next token */
   TokenType toktype;
   S_CHECK(peek(interp, p, &toktype));

   if (toktype == TOK_SINGLE_QUOTE || toktype == TOK_DOUBLE_QUOTE) {
      char quote = toktype == TOK_SINGLE_QUOTE ? '\'' : '"';
      S_CHECK(parser_peekasUEL(interp, p, quote, &toktype));

      // delete once GG #18 is fixed.
      if (interp->peek.len == 1 && interp->peek.start[0] == '*') {
         toktype = TOK_COLON;
      }

      if (toktype == TOK_UNSET) {
         const Token *tok = &interp->peek;
         error("[empinterp] ERROR on line %u: %c%.*s%c is not a UEL. "
               "All UELs in the empinfo must come from GAMS!\n", interp->linenr,
               quote, tok->len, tok->start, quote);
         return Error_EMPIncorrectSyntax;
      }

      // delete once GG #18 is fixed.
      if (toktype != TOK_GMS_UEL && toktype != TOK_COLON) {
         return runtime_error(interp->linenr);
      }

   } else {
      //S_CHECK(advance(interp, p, &toktype));
      parser_expects_peek(interp, "A set or variable is required", TOK_GMS_SET,
                          TOK_IDENT, TOK_COLON, TOK_STAR);
   }

   switch (toktype) {

   case TOK_GMS_UEL:
      tok2ident(&interp->peek, ident);
      ident->type = IdentUEL;
      goto _finalize;

   case TOK_STAR:
      ident->type = IdentUniversalSet;
      ident->idx = 0; 
      ident->dim = 0;
      goto _finalize;

   case TOK_COLON:
      ident->type = IdentSymbolSlice;
      ident->idx = 0; 
      ident->dim = 0;
      goto _finalize;

   case TOK_IDENT:
      break; /* We deal with this case now */

   case TOK_GMS_SET: 
      tok2ident(&interp->peek, ident);
      gmsindices->num_sets++;
      goto _finalize;

   default:
      error("%s :: unexpected failure.\n", __func__);
      return Error_RuntimeError;
   }

   /* ---------------------------------------------------------------------
    * This code is executed only if tok == TOK_IDENT
    * --------------------------------------------------------------------- */
   assert(toktype == TOK_IDENT);

   resolve_identas(interp, ident, "GAMS index must fulfill these conditions.",
                   IdentLoopIterator, IdentLocalSet, IdentSet);

   switch (ident->type) {
   case IdentLocalSet:
      gmsindices->num_localsets++;
      break;
   case IdentSet: //TODO: this should not be true in Emb mode
      assert(!embmode(interp));
      gmsindices->num_sets++;
      break;
   case IdentLoopIterator:
      gmsindices->num_loopiterators++;
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
 * @param      interp      the interpreter
 * @param      p           the position pointer
 * @param[out] gmsindices  the GAMS indices
 *
 * @return                 the error code
 */
int parse_gmsindices(Interpreter * restrict interp, unsigned * restrict p,
                     GmsIndicesData * restrict gmsindices)
{
   TokenType toktype;
   unsigned nargs = 0, p2 = *p;

   assert(!gmsindices_isactive(gmsindices));

   IdentType symbol_type = interp->last_symbol.type;
   Lexeme symbol_lexeme = interp->last_symbol.lexeme;
   IdentOrigin symbol_origin = interp->last_symbol.origin;
  /* ----------------------------------------------------------------------
   * We are expecting to be called at symbol('a', '1')
   *                                         ^
   * Peek mode is used to parse all the gms indices. This allows to show
   * "symbol" in the error message. Likewise in a loop or sum statement.
   * ---------------------------------------------------------------------- */
   interp_peekseqstart(interp);

   do {
      if (nargs == GMS_MAX_INDEX_DIM) {
         error("[empinterp] ERROR on line %u: while parsing the arguments to the "
               "token '%.*s', more than %u were parsed.\n", interp->linenr,
               emptok_getstrlen(&interp->pre), emptok_getstrstart(&interp->pre),
               GMS_MAX_INDEX_DIM);
         return Error_EMPIncorrectSyntax;
      }

      S_CHECK(resolve_tokenasgmsidx(interp, &p2, gmsindices, nargs));

      S_CHECK(peek(interp, &p2, &toktype));

      nargs++;

      /* Reset last symbol */
      interp->last_symbol.type = symbol_type;
      interp->last_symbol.lexeme = symbol_lexeme;
      interp->last_symbol.origin = symbol_origin;

   } while (toktype == TOK_COMMA);

   gmsindices->nargs = nargs;

   S_CHECK(parser_expect_peek(interp, "Closing ')' expected for GAMS indices", TOK_RPAREN));

   *p = p2;
   interp_peekseqend(interp);

   return OK;
}

/**
 * @brief Function to parse gams indices
 *
 * This takes care of parsing the indices of a label definition. Compared to
 * parse_gmsindices, we disallow '*'
 *
 * @param  interp      the interpreter
 * @param  p           the parser pointer
 * @param  gmsindices  the index structure
 *
 * @return             the error code
 */
// HACK: this is unused
int parse_labeldefindices(Interpreter * restrict interp, unsigned * restrict p,
                          GmsIndicesData * restrict gmsindices)
{
   TokenType toktype;
   unsigned nargs = 0, p2 = *p;

  /* ----------------------------------------------------------------------
   * We are parsing n('a',...):
   *                  ^
   * This is similar to parse_gmsindices(), except that '*' is not valid.
   * This function also operated in peek mode to provide better error message
   * TODO: we should add UELs that cannot be found in the DCT
   * ---------------------------------------------------------------------- */

   do {
      if (nargs == GMS_MAX_INDEX_DIM) {
         error("[empinterp] ERROR on line %u: while parsing the arguments to the "
               "token '%.*s', more than %u were parsed.\n", interp->linenr,
               emptok_getstrlen(&interp->pre), emptok_getstrstart(&interp->pre),
               GMS_MAX_INDEX_DIM);
         return Error_EMPIncorrectSyntax;
      }

      S_CHECK(resolve_tokenasgmsidx(interp, &p2, gmsindices, nargs));
      if (gmsindices->idents[nargs].type == IdentUniversalSet) {
         errormsg("[empinterp] ERROR: '*' is not a valid index in a label "
                  "definition\n");
         return Error_EMPIncorrectSyntax;
      }

      S_CHECK(peek(interp, &p2, &toktype));

      nargs++;

   } while (toktype == TOK_COMMA);

   gmsindices->nargs = nargs;

   S_CHECK(parser_expect_peek(interp, "Closing ')' expected for GAMS indices", TOK_RPAREN));

   *p = p2;
   interp_peekseqend(interp);

   return OK;
}

int resolve_tokasident(Interpreter *interp, IdentData *ident)
{
   /* ---------------------------------------------------------------------
    * Strategy here for resolution:
    * 1. Look for a local variable
    * 2. Look for a global variable (todo)
    * 3. Look for an alias
    * 4. Look for a GAMS set / multiset / scalar / vector / param
    * 5. Look for a symbol in the GMD
    * --------------------------------------------------------------------- */

   char *identstr = NULL;
   struct emptok *tok = !interp->state.peekisactive ? &interp->cur : &interp->peek;
   ident_init(ident, tok);

   if (resolve_local(interp->compiler, ident)) {
      return OK;
   }

   /* 2: global TODO */

   /* Set this here, so that we don't copy this line over and over */
   ident->origin = IdentOriginGdx;

   /* 3. alias */
   A_CHECK(identstr, tok_dupident(tok));
   AliasArray *aliases = &interp->globals.aliases;
   unsigned aliasidx = aliases_findbyname_nocase(aliases, identstr);

   if (aliasidx != UINT_MAX) {
      GdxAlias alias = aliases_at(aliases, aliasidx);
      ident->idx = alias.index;
      ident->type = alias.type;
      ident->dim = alias.dim;
      goto _exit;
   }

    /* 4. Look for a GAMS set / multiset / scalar / vector / param */
   NamedIntsArray *sets = &interp->globals.sets;
   unsigned idx = namedints_findbyname_nocase(sets, identstr);

   if (idx != UINT_MAX) {
      ident->idx = idx;
      ident->type = IdentSet;
      ident->dim = 1;
      ident->ptr = &sets->list[idx];
      goto _exit;
   }

   NamedMultiSets *multisets = &interp->globals.multisets;
   idx = multisets_findbyname_nocase(multisets, identstr);

   if (idx != UINT_MAX) {
      GdxMultiSet ms = multisets_at(multisets, idx);
      ident->idx = ms.idx;
      ident->type = IdentMultiSet;
      ident->dim = ms.dim;
      ident->ptr = ms.gdxreader;
      goto _exit;
   }

   NamedScalarArray *scalars = &interp->globals.scalars;
   idx = namedscalar_findbyname_nocase(scalars, identstr);

   if (idx != UINT_MAX) {
      ident->idx = idx;
      ident->type = IdentScalar;
      ident->dim = 0;
      ident->ptr = &scalars->list[idx];
      goto _exit;
   }

   NamedVecArray *vectors = &interp->globals.vectors;
   idx = namedvec_findbyname_nocase(vectors, identstr);

   if (idx != UINT_MAX) {
      ident->idx = idx;
      ident->type = IdentVector;
      ident->dim = 1;
      ident->ptr = &vectors->list[idx];
      goto _exit;
   }

   /* 5: GMD (for multiset) */
   if (interp->gmd) {
      S_CHECK(gmd_search_ident(interp, ident));

      if (ident->type != IdentNotFound) { goto _exit; }
   }

   /* TODO: Params */
   ident->origin = IdentOriginUnknown;

_exit:
   free(identstr);

   return OK;
}
