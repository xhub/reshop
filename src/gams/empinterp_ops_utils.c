
#include "empinterp.h"
#include "empinterp_ops_utils.h"
#include "empinterp_priv.h"
#include "empinterp_vm_utils.h"
#include "empparser_priv.h"
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
gmd_search_symbol(gmdHandle_t gmd, const char sym_name[GMS_SSSIZE], IdentData *ident,
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
int gmd_search_ident(Interpreter * restrict interp, IdentData * restrict ident)
{
   struct emptok *tok = !interp->peekisactive ? &interp->cur : &interp->peek;
   ident_init(ident, tok);

   size_t lexeme_len = ident->lexeme.len;
   if (lexeme_len >= GMS_SSSIZE-1) { return OK; }

   char lexeme[GMS_SSSIZE];
   memcpy(lexeme, ident->lexeme.start, lexeme_len * sizeof(char));
   lexeme[lexeme_len] = '\0';

   gmdHandle_t gmd = interp->gmd; assert(gmd);

   S_CHECK(gmd_search_symbol(gmd, lexeme, ident, interp->gmdcpy));

   return OK;
}

/**
 * @brief Try to resolve an identifier using GMD
 *
 * @param      gmdh   the GMD symbol pointer
 * @param      filter   the filter
 * @param[out] res      true if symbol matches the filter
 *
 * @return        the error code
 */
int gmd_boolean_test(void *gmdh, VmGmsSymIterator *filter, bool *res)
{
   gmdHandle_t gmd = gmdh;

   char  uels_str[GLOBAL_MAX_INDEX_DIM][GLOBAL_UEL_IDENT_SIZE];
   const char *uels_strp[GLOBAL_MAX_INDEX_DIM];

   const int *uels = filter->uels;
   unsigned dim = filter->ident.dim;

   for (unsigned i = 0; i < dim; ++i) {
      uels_strp[i] = uels_str[i];
      GMD_CHK(gmdGetUelByIndex, gmd, uels[i], uels_str[i]);
   }

   void *symptr = filter->ident.ptr; assert(symptr);

   void *symiterptr = NULL;
   *res = gmdFindRecord(gmd, symptr, uels_strp, &symiterptr);

   assert(*res == (symiterptr != NULL));

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
      S_CHECK(gmd_search_symbol(gmddct, sym_name, ident, NULL));
   } else if (dct) {
      S_CHECK(dct_find_symbol(dct, sym_name, ident, NULL));
   }

   gmdHandle_t gmd = interp->gmd;
   if (ident->type == IdentNotFound && gmd) {
      S_CHECK(gmd_search_symbol(gmd, sym_name, ident, interp->gmdcpy));
   }

   tok->type = ident2toktype(ident->type);

   // HACK: this is needed, especially in embmode
   // TODO: use a status flag in the Token to indicate of data has been read
   if (tok->type == TOK_GMS_VAR) {
      avar_init(&tok->payload.v);
   } else if (tok->type == TOK_GMS_EQU) {
      aequ_init(&tok->payload.e);
   }

   if (ident->type != IdentNotFound && &interp->cur == tok) {
      interp_set_last_gms_symbol(interp, ident);
   }

   return OK;

}

static inline int get_uelidx_via_dct(Interpreter * restrict interp, const char uelstr[VMT(static 1)],
                                     int * restrict uelidx)
{
   assert(interp->dct);
   *uelidx = dctUelIndex(interp->dct, uelstr);

   return OK;
}

int get_uelstr_for_empdag_node(Interpreter *interp, int uelidx, unsigned uelstrlen, char *uelstr)
{
   if (interp->gmd) {
      GMD_CHK_RET(gmdGetUelByIndex, interp->gmd, uelidx, uelstr);
      return OK;
   }

   char dummyquote = ' ';
   if (interp->gmddct) {
      gmdHandle_t gmd = interp->gmddct;
      GMD_CHK_RET(gmdGetUelByIndex, gmd, uelidx, uelstr);

      return OK;
   }
   assert(interp->dct);
   dct_call_rc(dctUelLabel, interp->dct, uelidx, &dummyquote, uelstr, uelstrlen);

   return OK;
}

NONNULL static inline
int find_uelidx_gmd(gmdHandle_t gmd, const char uelstr[VMT(static 1)],
                    int * restrict uelidx)
{
   if (!gmdFindUel(gmd, uelstr, uelidx)) {
      error("[embcode] ERROR while calling 'gmsFindUel' for lexeme '%s'", uelstr);
      return gmderr(gmd);
   }

   return OK;
}

int find_uelidx(Interpreter * restrict interp, const char uelstr[VMT(static 1)],
                int * restrict uelidx)
{
   IdentData *symbol = &interp->last_symbol;
   if (interp->last_symbol.type == IdentNotFound) {
      error("[empinterp] ERROR: cannot process UEL '%s' outside of a symbol. "
            "Please file a bug\n", uelstr);
      return Error_EMPRuntimeError;
   }

   gmdHandle_t gmd;
   int rc = Error_EMPRuntimeError;

   if (embmode(interp)) {
      gmd = interp->gmd;
      return gmd ? find_uelidx_gmd(gmd, uelstr, uelidx) : Error_EMPRuntimeError;
   }

   switch (symbol->type) {
      /* Look for UEL in DCT */
   case IdentEqu: case IdentVar: 
      gmd = interp->gmddct;
      if (gmd) {
         rc = find_uelidx_gmd(gmd, uelstr, uelidx);
      } else if (interp->dct) {
         rc = get_uelidx_via_dct(interp, uelstr, uelidx);
      }

      if (rc != OK || *uelidx > 0) { return rc; }
      break;
   case IdentEmpDagLabel:
      if (!interp->gmd) {
         gmd = interp->gmddct;
         if (gmd) {
            return find_uelidx_gmd(gmd, uelstr, uelidx);
         }
         if (interp->dct) {
            return get_uelidx_via_dct(interp, uelstr, uelidx);
         }

         return runtime_error(interp->linenr);
      }
      FALLTHRU
   case IdentSet: case IdentParam: case IdentVector:
      return find_uelidx_gmd(interp->gmd, uelstr, uelidx);
   default:
      return runtime_error(interp->linenr);
   }

   gmd = interp->gmd;
   if (gmd) {
      return rc;
   }

   if (!gmdFindUel(gmd, uelstr, uelidx)) {
      return gmderr(gmd);
   }

   if (*uelidx > 0) {
      int offset;
      error("[empinterp] %nERROR: UEL '%s' was found in the GAMS database, "
            "but does not appear in any equation of variable.\n", &offset, uelstr);

      if (symbol->type == IdentEqu) {
         error("%*sHint: Check the equation '%.*s' appears in the MODEL statement\n",
               offset, "", lexeme_fmtargs(symbol->lexeme));
      } else if (symbol->type == IdentVar) {
         error("%*sHint: Check that the variable '%.*s' appears at least in one "
               "of the equation listed in the MODEL statement\n", offset, "",
               lexeme_fmtargs(symbol->lexeme));
      } 
   }

   return Error_EMPIncorrectInput;
}

#ifdef NO_DELETE_2024_12_18
static inline int convert_uelidx(gmdHandle_t gmddst, gmdHandle_t gmdsrc, int uelidx)
{
   char uelstr[GMS_SSSIZE];

   if (gmdGetUelByIndex(gmdsrc, uelidx, uelstr)) {
      return -gmderr(gmdsrc);
   }

   if (gmdFindUel(gmddst, uelstr, &uelidx)) {
      return -gmderr(gmddst);
   }

   return uelidx;
}

int uelidx_check_or_convert(gmdHandle_t gmd, gmdHandle_t gmddct, int uelidx, struct origins origins)
{
   if (origins.sym != IdentOriginDct && origins.sym != IdentOriginGmd) {
      assert(0 && "symbol has unsupported origin");
      return -1;
   }

   if (origins.uel != IdentOriginDct && origins.uel != IdentOriginGmd) {
      assert(0 && "UEL has unsupported origin");
      return -1;
   }

   if (origins.sym == origins.uel) {
      return uelidx;
   }

   gmdHandle_t gmddst, gmdsrc;

   switch (origins.uel) {
   case IdentOriginDct:
      gmddst = gmd;
      gmdsrc = gmddct;
      break;
   case IdentOriginGmd:
      gmddst = gmddct;
      gmdsrc = gmd;
      break;
   default:
      assert(0 && "UEL has unsupported origin");
      return -Error_EMPRuntimeError;
   }

   return convert_uelidx(gmddst, gmdsrc, uelidx);
}

// TODO delete once newer GMD is in master
int uelidx_check_or_convert_dct_vs_gmd(gmdHandle_t gmd, dctHandle_t dct,
                                       int uelidx, struct origins origins)
{
  if (origins.sym != IdentOriginDct && origins.sym != IdentOriginGmd) {
      assert(0 && "symbol has unsupported origin");
      return -1;
   }

   if (origins.uel != IdentOriginDct && origins.uel != IdentOriginGmd) {
      assert(0 && "UEL has unsupported origin");
      return -1;
   }

   if (origins.sym == origins.uel) {
      return uelidx;
   }

   char uelstr[GLOBAL_UEL_IDENT_SIZE];
   int uelidx_dst;

  switch (origins.uel) {
   case IdentOriginDct: {
      char quote_[] = " ";
      if (dctUelLabel(dct, uelidx, quote_, uelstr, sizeof(uelstr))) {
         error("[DCT] Could not find UEL #%d\n", uelidx);
         return -Error_EMPRuntimeError;
      } 

      if (gmdFindUel(gmd, uelstr, &uelidx_dst)) {
         return -gmderr(gmd);
      }
      break;
   }

   case IdentOriginGmd:
      if (gmdGetUelByIndex(gmd, uelidx, uelstr)) {
         return -gmderr(gmd);
      }

      uelidx_dst = dctUelIndex(dct, uelstr);
      if (uelidx <= 0) {
         error("[empinterp] ERROR: could not find uel '%s' in the DCT\n", uelstr);
         return -Error_EMPRuntimeError;
      } 

      break;
   default:
      assert(0 && "UEL has unsupported origin");
      return -Error_EMPRuntimeError;
   }

   return uelidx_dst;

}
#endif
