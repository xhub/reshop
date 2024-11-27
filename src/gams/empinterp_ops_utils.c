
#include "empinterp.h"
#include "empinterp_ops_utils.h"
#include "empinterp_priv.h"
#include "gamsapi_utils.h"

#include "dctmcc.h"
#include "gmdcc.h"

static inline int symtype_dct2ident(enum dcttypes dcttype, IdentData *ident)
{
   ident->origin = IdentOriginDct;

   switch (dcttype) {
   case dctfuncSymType:
   case dctsetSymType:
   case dctparmSymType:
   case dctacrSymType:
   default:
      error("[empinterp] ERROR: dct type %d not supported. Please file a bug report\n", dcttype);
      return Error_EMPRuntimeError;
   case dctaliasSymType:
      ident->type = IdentAlias;
      return OK;
   case dctvarSymType:
      ident->type = IdentVar;
      return OK;
   case dcteqnSymType:
      ident->type = IdentEqu;
      return OK;
   }
}


/**
 * @brief Try to resolve a string as a gams symbol using GMD
 *
 * @param gmd        the GMD handle
 * @param sym_name   the symbol name (as a nul-terminated string)
 * @param ident     the symbol data
 * @param gmdcpy    If non-null, the handle to a GMD where a set or parameter will be copied
 *
 * @return        the error code
 */
NONNULL_AT(1,3) static int
gmd_find_symbol(gmdHandle_t gmd, const char sym_name[GMS_SSSIZE], IdentData *ident,
                gmdHandle_t gmdcpy)
{

   void *symptr;
   if (!gmdFindSymbol(gmd, sym_name, &symptr)) { /* We do not fail here as it could be a UEL */
      int uelidx;
      if (!gmdFindUel(gmd, sym_name, &uelidx)) {
         error("[GMD] ERROR while calling 'gmdFindUEL' for lexeme '%s'", sym_name);
         return Error_GamsCallFailed;
      }

      if (uelidx <= 0) { return OK; }

      ident->type = IdentUEL;
      ident->idx = uelidx;

      trace_empinterp("[GMD] Lexeme '%s' resolved as UEL #%d\n", sym_name, uelidx);
      return OK;
   }

  /* ----------------------------------------------------------------------
   * 2024.05.09: Note that gmdFindSymbol never return an alias symbol, but rather
   * the aliased (or target) symbol. Therefore, we don't need to look for it,
   * and we hard fail if we encounter an aliased symbol
   * ---------------------------------------------------------------------- */

   /* ---------------------------------------------------------------------
    * Save the index of the symbol
    * --------------------------------------------------------------------- */

   int symnr;
   if (!gmdSymbolInfo(gmd, symptr, GMD_NUMBER, &symnr, NULL, NULL)) {
      return gmderror(gmd, "[GMD] ERROR: could not query number of symbol '%s'\n", sym_name);
   }

   assert(symnr >= 0);
   // TODO: this is a hack!
   ident->idx = symnr;


   /* ---------------------------------------------------------------------
    * Save the dimension of the symbol index
    * --------------------------------------------------------------------- */

   int symdim;
   if (!gmdSymbolInfo(gmd, symptr, GMD_DIM, &symdim, NULL, NULL)) {
      return gmderror(gmd, "[GMD] ERROR: could not query dimension of symbol '%s'\n", sym_name);
   }
   /* What does a negative value of symdim means? */
   assert(symdim >= 0);

   /* ---------------------------------------------------------------------
    * Save the type of the symbol
    * --------------------------------------------------------------------- */

   int symtype;
   if (!gmdSymbolInfo(gmd, symptr, GMD_TYPE, &symtype, NULL, NULL)) {
      return gmderror(gmd, "[GMD] ERROR: could not query type of symbol '%s'\n", sym_name);
   }

   // TODO: domindices are not lookup now.
#ifdef DO_LOOKUP_DOMINDICES
   if (symdim > 0 && domindices) {
      char dom_names[GMS_MAX_INDEX_DIM][GMS_SSSIZE];
      char *dom_names_ptrs[GMS_MAX_INDEX_DIM];
      GDXSTRINDEXPTRS_INIT( dom_names, dom_names_ptrs );

      void *dom_ptrs[GMS_MAX_INDEX_DIM];
      if (!gmdGetDomain(gmd, symptr, symdim, dom_ptrs, dom_names_ptrs)) {
          return gmderror(gmd, "[GMD] ERROR: could not query the domains of symbol '%s'\n", sym_name);
      }

      for (int i = 0; i < symdim; ++i) {

         if (!dom_ptrs[i]) {
            domindices[i] = 0;
            continue;
         }

         // HACK: symnr should be >= 0, but not right now.
         if (!gmdSymbolInfo(gmd, dom_ptrs[i], GMD_NUMBER, &symnr, NULL, NULL) || symnr < -1) {
            return gmderror(gmd, "[GMD] ERROR: could not query number of domain '%s' #%u "
                            "of symbol '%s'\n", dom_names_ptrs[i], i, sym_name);
         }

         // HACK: symnr should be >= 0, but not right now.
         domindices[i] = symnr >= 0 ? symnr : 0;
      }
   }
#endif

   ident->dim = symdim;
   ident->origin = IdentOriginGmd;
   S_CHECK(gdxsymtype2ident(symtype, ident));

   ident->ptr = symptr;

   trace_empinterp("[GMD] Lexeme '%s' resolved as a %s of dimension %u\n",
                   sym_name, identtype2str(ident->type), symdim);

  /* ----------------------------------------------------------------------
   * If gmdout is set, we copy sets and parameters (if this hasn't been done yet)
   *
   * TODO: gmdFindSymbol returning false when the symbol doesn't exists isn't great
   * ---------------------------------------------------------------------- */

   void *symptr2 = NULL;
   if (gmdcpy && (symtype == GMS_DT_SET || symtype == GMS_DT_PAR) && 
      !gmdFindSymbol(gmdcpy, sym_name, &symptr2)) {

      GMD_CHK(gmdAddSymbol, gmdcpy, sym_name, symdim, symtype, 0, "", &symptr2);

      GMD_CHK(gmdCopySymbol, gmdcpy, symptr2, symptr);
   }

   return OK;
}

/**
 * @brief Try to resolve an identifier using GMD
 *
 * @param interp  the interpreter
 * @param ident   the identifier
 *
 * @return        the error code
 */
int gmd_find_ident(Interpreter * restrict interp, IdentData * restrict ident)
{
   struct emptok *tok = !interp->peekisactive ? &interp->cur : &interp->peek;
   ident_init(ident, tok);

   size_t lexeme_len = ident->lexeme.len;
   if (lexeme_len >= GMS_SSSIZE-1) { return OK; }

   char lexeme[GMS_SSSIZE];
   memcpy(lexeme, ident->lexeme.start, lexeme_len * sizeof(char));
   lexeme[lexeme_len] = '\0';

   gmdHandle_t gmd = interp->gmd; assert(gmd);

   S_CHECK(gmd_find_symbol(gmd, lexeme, ident, interp->gmdcpy));

   return OK;
}

NONNULL_AT(1,3) static int
dct_find_symbol(dctHandle_t dct, const char sym_name[GMS_SSSIZE], IdentData *ident,
                int *domindices)
{

   int symidx = dctSymIndex(dct, sym_name);
   if (symidx <= 0) { /* We do not fail here as it could be a UEL */
      int uelindex = dctUelIndex(dct, sym_name);
      if (uelindex <= 0) { 
         ident->type = IdentNotFound;
         return OK;
      }

      ident->idx = uelindex;
      ident->origin = IdentOriginDct;
      ident->type = IdentUEL;

      return OK;
   }

   ident->idx = symidx;

   /* ---------------------------------------------------------------------
    * Save the dimension of the symbol index
    * --------------------------------------------------------------------- */

   /* What does a negative value of symdim means? */
   int symdim = dctSymDim(dct, symidx);
   ident->dim = symdim;

   /* ---------------------------------------------------------------------
    * Save the indices of the domains
    * --------------------------------------------------------------------- */

   /* What does a negative value of symdim means? */
   if (symdim > 0 && domindices) {
      int dummy;
      dctSymDomIdx(dct, symidx, domindices, &dummy);
   }

   /* ---------------------------------------------------------------------
    * Save the type of the symbol
    * --------------------------------------------------------------------- */

   int symtype = dctSymType(dct, symidx);
   S_CHECK(symtype_dct2ident(symtype, ident));

   ident->ptr = NULL;

   return OK;
}

/**
 * @brief Try to resolve a string lexeme as a gams symbol using a dictionary
 *
 * @param interp  the interpreter
 * @param tok     the token containing the lexeme
 *
 * @return        the error code
 */
int resolve_lexeme_as_gms_symbol(Interpreter * restrict interp, Token * restrict tok)
{
   GamsSymData *symdat = &tok->symdat;
   ident_init(&symdat->ident, tok);
   symdat->read = false;

   unsigned tok_len = emptok_getstrlen(tok);
   if (tok_len >= GMS_SSSIZE) {
      tok->type = TOK_IDENT;
      return OK;
   }

   /* Copy string to end it with NUL */
   char sym_name[GMS_SSSIZE];
   strncpy(sym_name, tok->start, tok_len);
   sym_name[tok_len] = '\0';

  /* ----------------------------------------------------------------------
   * We first try to find the symbol name in the DCT, if it exists
   * ---------------------------------------------------------------------- */

   dctHandle_t dct = interp->dct;
   IdentData *ident = &symdat->ident;

  /* ----------------------------------------------------------------------
   * This would resolve the ident as an equation or variable
   * ---------------------------------------------------------------------- */

   gmdHandle_t gmddct = interp->gmddct;
   if (gmddct) {
      S_CHECK(gmd_find_symbol(gmddct, sym_name, ident, NULL));
   } else if (dct) {
      S_CHECK(dct_find_symbol(dct, sym_name, ident, NULL));
   }

   gmdHandle_t gmd = interp->gmd;
   if (symdat->ident.type == IdentNotFound && gmd) {
      S_CHECK(gmd_find_symbol(gmd, sym_name, ident, interp->gmdcpy));
   }

   tok->type = ident2toktype(symdat->ident.type);

   // HACK: this is needed, especially in embmode
   // TODO: use a status flag in the Token to indicate of data has been read
   if (tok->type == TOK_GMS_VAR) {
      avar_init(&tok->payload.v);
   } else if (tok->type == TOK_GMS_EQU) {
      aequ_init(&tok->payload.e);
   }

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
