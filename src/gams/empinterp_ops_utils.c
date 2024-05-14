#include "empinterp.h"

#include "dctmcc.h"
#include "empinterp_utils.h"
#include "gamsapi_utils.h"
#include "empinterp_ops_utils.h"

/**
 * @brief Try to resolve a string lexeme as a gams symbol using a dictionary
 *
 * @param interp  the interpreter
 * @param tok     the token containing the lexeme
 *
 * @return        the error code
 */
int resolve_lexeme_as_gmssymb_via_dct(Interpreter * restrict interp, Token * restrict tok)
{
   GamsSymData *symdat = &tok->symdat;
   int symindex, uelindex;

   unsigned tok_len = emptok_getstrlen(tok);
   if (tok_len >= GMS_SSSIZE) { goto _not_found; }

   /* Copy string to end it with NUL */
   char sym_name[GMS_SSSIZE];
   strncpy(sym_name, tok->start, tok_len);
   sym_name[tok_len] = '\0';

   dctHandle_t dct = interp->dct;

   symindex = dctSymIndex(dct, sym_name);
   if (symindex <= 0) { /* We do not fail here as it could be a UEL */
      uelindex = dctUelIndex(dct, sym_name);
      if (uelindex <= 0) goto _not_found;

      tok->type = TOK_GMS_UEL;
      symdat->idx = uelindex;

      return OK;
   }

   symdat->idx = symindex;

   /* ---------------------------------------------------------------------
    * Save the dimension of the symbol index
    * --------------------------------------------------------------------- */

   /* What does a negative value of symdim means? */
   int symdim = MAX(dctSymDim(dct, symindex), 1);
   symdat->dim = symdim;

   /* ---------------------------------------------------------------------
    * Save the type of the symbol
    * --------------------------------------------------------------------- */

   int symtype = dctSymType(dct, symindex);
   symdat->type = symtype;
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
   case dctacrSymType:
   case dctfuncSymType:
   default:
      return runtime_error(interp->linenr);
   }

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
