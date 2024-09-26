#include "empinterp.h"

#include "dctmcc.h"
#include "empinterp_utils.h"
#include "gamsapi_utils.h"
#include "empinterp_ops_utils.h"

static inline int symtype_dct2rhp(enum dcttypes dcttype, GamsSymData *symdat, Token *tok)
{
   symdat->origin = IdentOriginDct;

   switch (dcttype) {
   case dctfuncSymType:
   case dctsetSymType:
   case dctparmSymType:
   case dctacrSymType:
   default:
      error("[empinterp] ERROR: dct type %d not supported. Please file a bug report\n", dcttype);
      return runtime_error(tok->linenr);
   case dctaliasSymType:
      symdat->type = IdentAlias;
      tok->type = TOK_GMS_ALIAS;
      return OK;
   case dctvarSymType:
      symdat->type = IdentVar;
      tok->type = TOK_GMS_VAR;
      return OK;
   case dcteqnSymType:
      symdat->type = IdentEqu;
      tok->type = TOK_GMS_EQU;
      return OK;
   }
}

/**
 * @brief Try to resolve a string lexeme as a gams symbol using a dictionary
 *
 * @param interp  the interpreter
 * @param tok     the token containing the lexeme
 *
 * @return        the error code
 */
int dct_findlexeme(Interpreter * restrict interp, Token * restrict tok)
{
   GamsSymData *symdat = &tok->symdat;
   int symidx, uelindex;

   unsigned tok_len = emptok_getstrlen(tok);
   if (tok_len >= GMS_SSSIZE) { goto _not_found; }

   /* Copy string to end it with NUL */
   char sym_name[GMS_SSSIZE];
   strncpy(sym_name, tok->start, tok_len);
   sym_name[tok_len] = '\0';

   dctHandle_t dct = interp->dct;

   symidx = dctSymIndex(dct, sym_name);
   if (symidx <= 0) { /* We do not fail here as it could be a UEL */
      uelindex = dctUelIndex(dct, sym_name);
      if (uelindex <= 0) goto _not_found;

      tok->type = TOK_GMS_UEL;
      symdat->idx = uelindex;
      symdat->origin = IdentOriginDct;
      symdat->type = IdentUEL;

      return OK;
   }

   symdat->idx = symidx;

   /* ---------------------------------------------------------------------
    * Save the dimension of the symbol index
    * --------------------------------------------------------------------- */

   /* What does a negative value of symdim means? */
   int symdim = dctSymDim(dct, symidx);
   symdat->dim = symdim;

   /* ---------------------------------------------------------------------
    * Save the indices of the domains
    * --------------------------------------------------------------------- */

   /* What does a negative value of symdim means? */
   if (symdim > 0) {
      int dummy;
      dctSymDomIdx(dct, symidx, symdat->domindices, &dummy);
   }

   /* ---------------------------------------------------------------------
    * Save the type of the symbol
    * --------------------------------------------------------------------- */

   int symtype = dctSymType(dct, symidx);
   S_CHECK(symtype_dct2rhp(symtype, symdat, tok));

   symdat->read = false;
   symdat->ptr = NULL;

   return OK;

_not_found:
   symdat->idx = IdxNotFound;
   tok->type = TOK_IDENT;

   return OK;
}

int get_uelidx_via_dct(Interpreter * restrict interp, const char uelstr[GMS_SSSIZE], int * restrict uelidx)
{
   assert(interp->dct);
   *uelidx = dctUelIndex(interp->dct, uelstr);

   return OK;
}

int get_uelstr_via_dct(Interpreter *interp, int uelidx, unsigned uelstrlen, char *uelstr)
{
   assert(interp->dct);
   char dummyquote = ' ';
   dct_call_rc(dctUelLabel, interp->dct, uelidx, &dummyquote, uelstr, uelstrlen);

   return OK;
}
