#include "empinterp_symbol_resolver.h"
#include "empinterp_priv.h"
#include "empparser.h"
#include "empinterp_vm_utils.h"
#include "equ.h"
#include "gamsapi_utils.h"
#include "macros.h"
#include "printout.h"
#include "printout_data.h"

#include "dctmcc.h"
#include "gmdcc.h"
#include "var.h"

static NONNULL
void print_help_missing_record_in_equation(const char *symname)
{
   int offset;
   error("[empinterp] %nIn the model instance, the equation '%s' is defined, "
         "but not with the above tuple.\n", &offset, symname);
   errormsg("\nThere can be numerous causes to this error, here is one:\n"
            "This particular equation has not been defined due to a restriction "
            "over the domain of definition via the dollar control '$(...)'.\n\n"
            "Please check your model statement or implement a restriction with the "
            "conditional '$(...)' after the symbol.\n");
}

static NONNULL
void print_help_missing_equation(const char *symname)
{
   int offset;
   error("[empinterp] %nIn the model instance, the variable '%s' is defined, "
         "but not with the above tuple.\n", &offset, symname);
   errormsg("\nThere can be numerous causes to this error, here is one:\n"
            "The equation was not included in the GAMS 'model' definition\n");
}

static NONNULL
void print_help_missing_record_in_parameter(const char *symname)
{
   int offset;
   error("[empinterp] %nIn parameter '%s', no record found for the above tuple.\n",
         &offset, symname);
}

static NONNULL
void print_help_missing_record_in_variable(const char *symname)
{
   int offset;
   error("[empinterp] %nIn the model instance, the variable '%s' is defined, "
         "but not with the above tuple.\n", &offset, symname);
   errormsg("\nThere can be numerous causes to this error, like:\n"
            "- The equation where the variable appears is not included in the "
            "GAMS 'model' statement\n"
            "- The equation where this variable could appear has not been defined, "
            "due to a restriction over the domain of definition via the dollar "
            "control '$(...)'.\n\n"
            "Please check your model statement or implement a restriction with the "
            "conditional '$(...)' after the symbol.\n");
}

static NONNULL
void print_help_missing_variable(const char *symname)
{
   int offset;
   error("[empinterp] %nIn the model instance, the variable '%s' is absent.\n",
         &offset, symname);
   errormsg("\nThere can be numerous causes to this error, here is one:\n"
            "The equation where the variable appears is not included in the "
            "GAMS 'model' statement\n");
}

static NONNULL
int print_help(TokenType toktype, uint8_t dim, const char *symname)
{
   switch (toktype) {
   case TOK_GMS_VAR:
      if (dim > 0) {
         print_help_missing_record_in_variable(symname);
      } else { /* This should not get executed ... */
         assert(0 && "investigate");
         print_help_missing_variable(symname);
      }
      break;
   case TOK_GMS_EQU:
      if (dim > 0) {
         print_help_missing_record_in_equation(symname);
      } else { /* This should not get executed ... */
         assert(0 && "investigate");
         print_help_missing_equation(symname);
      }
      break;
   case TOK_GMS_PARAM:
      if (dim > 0) {
         print_help_missing_record_in_parameter(symname);
      } else { /* Odd one: missing symbol should not happen here */
         assert(0 && "investigate");
         print_help_missing_equation(symname);
      }
      break;
   case TOK_GMS_SET:
      assert(0 && "investigate");
      break;
   default:
      assert(0 && "investigate");
      return error_runtime();
   }

   return OK;
}

int dct_read_equvar(dctHandle_t dct, GmsResolveData * restrict data)
{
   int symidx, *uels;
   unsigned dim;
   TokenType toktype;
   bool compact;
   char buf[GMS_SSSIZE];

   if (data->type == GmsSymIteratorTypeImm) {
      symidx = data->symiter.imm.symidx;
      dim = data->symiter.imm.symiter->indices.nargs;
      toktype = data->symiter.imm.toktype;
      uels =  data->symiter.imm.symiter->uels;
      compact = data->symiter.imm.symiter->compact;
   } else if (data->type == GmsSymIteratorTypeVm) {
      assert(data->type == GmsSymIteratorTypeVm);
      symidx = (int)data->symiter.vm->ident.idx;
      dim = data->symiter.vm->ident.dim;
      toktype = ident2toktype(data->symiter.vm->ident.type);
      assert(toktype != TOK_ERROR);
      uels =  data->symiter.vm->uels;
      compact = data->symiter.vm->compact;
   } else {
      TO_IMPLEMENT("unknown iterator type");
   }

   assert(dim < GMS_MAX_INDEX_DIM);

   if (O_Output & PO_TRACE_EMPPARSER) {
      char quote = ' ';
      if (dctSymName(dct, symidx, buf, sizeof(buf))) {
         strcpy(buf, "ERROR resolving symbol");
      }
      trace_empinterp("[empinterp] DCT resolution of GAMS symbol '%s' #%d of type %s and dim %u.\n",
                      buf, symidx, toktype2str(toktype), dim);

      if (dim > 1 || (dim == 1 && uels[0] > 0)) {
         trace_empinterp("[DCT] UELs values are:\n");
         for (unsigned i = 0; i < dim; ++i) {
            int uel = uels[i];
            if (uel > 0) { dctUelLabel(dct, uels[i], &quote, buf, sizeof(buf));
            } else { strcpy(buf, "wildcard (:)"); }
            trace_empinterp("%*c [%5d] %c%s%c\n", 11, ' ', uel, quote, buf, quote);
         }
      }
   }

   switch (toktype) {
   case TOK_GMS_VAR:
   case TOK_GMS_EQU:
   {
      /* ------------------------------------------------------------------
       * Return first row/column in the symbol referenced by symindex that
       * is indexed by the UELs in uelindices (uelindices[k]=0 is wildcard).
       * Since the routine can fail you should first check rcindex and then
       * the returned handle.
       * ------------------------------------------------------------------ */

      int idx;
      void *fh = dctFindFirstRowCol(dct, symidx, uels, &idx);
      if (idx < 0) {
         dctSymName(dct, symidx, buf, sizeof(buf));
         error("[empinterp] DCT ERROR: could not find record for symbol %s", buf);
         if (dim > 1 || (dim == 1 && uels[0] > 0)) {
            errormsg("(");

            for (unsigned i = 0; i < dim; ++i) {
               char quote = '\'';
               int uel = uels[i];
               if (i > 0) { errormsg(","); }
               if (uel > 0) {
                  dctUelLabel(dct, uels[i], &quote, buf, sizeof(buf));
                  error("%c%s%c", quote, buf, quote);
               } else { errormsg(":"); }
            }

            errormsg(")");
         }
         errormsg("\n\n");

         dctSymName(dct, symidx, buf, sizeof(buf));
         S_CHECK(print_help(toktype, dim, buf));

         return Error_SymbolNotInTheGamsRim;
      }

      /* ------------------------------------------------------------------
       * Returns the number of records stored for a given symbol.
       * ------------------------------------------------------------------ */

      int size = dctSymEntries(dct, symidx);
      assert(size >= 0);

      if (compact) {
         dctFindClose(dct, fh);

         if (toktype == TOK_GMS_VAR) {
            avar_setcompact(data->payload.v, size, idx);
         } else { /* toktype == TOK_GMS_EQU */
            aequ_ascompact(data->payload.e, size, idx);
         }

         trace_empinterp("[empinterp] Read %u entries for compact symbol, starting at %u\n", size, idx);
      } else {
         rhp_idx i = 0;
         S_CHECK(scratchint_ensure(data->iscratch, size));

         rhp_idx *idxs = data->iscratch->data;

         while (idx >= 0) {
            idxs[i++] = idx;
            dctFindNextRowCol(dct, fh, &idx);
         }
         dctFindClose(dct, fh);

         if (toktype == TOK_GMS_VAR) {
            avar_setlist(data->payload.v, i, idxs);
         } else { /* toktype == TOK_GMS_EQU */
            aequ_aslist(data->payload.e, i, idxs);
         }

         trace_empinterp("[empinterp] Read %u entries for symbol\n", i);
      }
      break;
   }
   case TOK_GMS_SET:
      TO_IMPLEMENT("gms_resolve for sets in immediate mode");
   case TOK_GMS_PARAM:
      TO_IMPLEMENT("gms_resolve for params in immediate mode");
   default:
      error("[empinterp] Unexpected token type '%s' in dct_resolve().\n",
            toktype2str(toktype));
   }

   return OK;
}

void dct_printuelwithidx(dctHandle_t dct, int uel, unsigned mode)
{
   char buf[GMS_SSSIZE] = " ";
   char quote = ' ';

   assert(uel <= dctNUels(dct));
   dctUelLabel(dct, uel, &quote, buf, sizeof(buf));
   printout(mode, "[%5d] %c%s%c", uel, quote, buf, quote);
}

void dct_printuel(dctHandle_t dct, int uel, unsigned mode, int *offset)
{
   char buf[GMS_SSSIZE] = " ";
   char quote[] = "'";

   assert(uel <= dctNUels(dct));

   if (uel == 0) {
      printout(mode, ":%n", offset);
   } else if (dctUelLabel(dct, uel, quote, buf, sizeof(buf))) {
      printout(mode, "UEL#%d%n", uel, offset);
   } else if (quote[0] != ' ') {
      printout(mode, "%c%s%c%n", quote[0], buf, quote[0], offset);
   } else {
      printout(mode, "'%s'%n", buf, offset);
   }
}

void gmd_printuelwithidx(gmdHandle_t gmd, int uel, unsigned mode)
{
   char uelstr[GMS_SSSIZE] = " ";

#ifndef NDEBUG
   int nuels;
   GMD_CHK(gmdInfo, gmd, GMD_NRUELS, &nuels, NULL, NULL);
   assert(uel <= nuels);
#endif

   gmdGetUelByIndex(gmd, uel, uelstr);
   printout(mode, "[%5d] '%s'", uel, uelstr);
}

void gmd_printuel(gmdHandle_t gmd, int uel, unsigned mode, int *offset)
{
   char uelstr[GMS_SSSIZE] = " ";

#ifndef NDEBUG
   int nuels;
   GMD_CHK(gmdInfo, gmd, GMD_NRUELS, &nuels, NULL, NULL);
   assert(uel <= nuels);
#endif

   if (uel == 0) {
      printout(mode, ":%n", offset);
   } else if (gmdGetUelByIndex(gmd, uel, uelstr)) {
      printout(mode, "UEL#%d%n", uel, offset);
   } else {
      printout(mode, "'%s'%n", uelstr, offset);
   }
}

// HACK
int gmd_read(gmdHandle_t gmd, GmsResolveData * restrict data,
             const char *symname)
{
   UNUSED int symnr, *uels;
   unsigned dim;
   TokenType toktype;
   bool full_records;
   char buf[GMS_SSSIZE] = " ";

   if (data->type == GmsSymIteratorTypeImm) {
      symnr = data->symiter.imm.symidx;
      dim = data->symiter.imm.symiter->indices.nargs;
      toktype = data->symiter.imm.toktype;
      uels =  data->symiter.imm.symiter->uels;
      full_records = data->symiter.imm.symiter->compact;
   } else if (data->type == GmsSymIteratorTypeVm) {
      symnr = (int)data->symiter.vm->ident.idx;
      dim = data->symiter.vm->ident.dim;
      toktype = ident2toktype(data->symiter.vm->ident.type);
      uels =  data->symiter.vm->uels;
      full_records = data->symiter.vm->compact;
   } else {
      TO_IMPLEMENT("unknown iterator type");
   }

   assert(dim < GMS_MAX_INDEX_DIM);

   void *symptr = NULL;
   GMD_CHK(gmdFindSymbol, gmd, symname, &symptr);
   if (O_Output & PO_TRACE_EMPINTERP) {
      char quote = '\'';
      GMD_CHK(gmdSymbolInfo, gmd, symptr, GMD_NAME, NULL, NULL, buf);
      trace_empinterp("[GMD] resolving GAMS symbol '%s' of type %s and dim %u.\n",
                      buf, toktype2str(toktype), dim);
      if (dim > 1 || (dim == 1 && uels[0] > 0)) {
         trace_empinterp("[GMD] UELs values are:\n");
         for (unsigned i = 0; i < dim; ++i) {
            int uel = uels[i];
            if (uel > 0) {
               GMD_CHK(gmdGetUelByIndex, gmd, uels[i], buf);
            } else { strcpy(buf, "wildcard (:)"); }
            trace_empinterp("%*c [%5d] %c%s%c\n", 11, ' ', uel, quote, buf, quote);
         }
      }
   }

   switch (toktype) {
   case TOK_GMS_VAR:
   case TOK_GMS_EQU:
   case TOK_GMS_SET:
   case TOK_GMS_PARAM:
      break;
   default:
      error("[empinterp] Unexpected token type '%s'\n", toktype2str(toktype));
      return Error_RuntimeError;
   }

  /* ----------------------------------------------------------------------
   * There are 3 cases:
   * - full symbol read: by construction, the records of GAMS equvar have
   *   continuous indices. Then we could just read the index of the first record index
   *   and be done. THIS IS NOT POSSIBLE RIGHT NOW IN GMD
   * ---------------------------------------------------------------------- */

//   if (full_records && equvar && dct && data->type == GmsSymIteratorTypeImm) {
//      int dctsymidx = dctSymIndex(dct, symname);
//      if (dctsymidx <= 0) {
//         error("[DCT] ERROR: could not find symbol '%s'\n", symname);
//         return Error_SymbolNotInTheGamsRim;
//      }
//
//      int idx;
//      void *fh = dctFindFirstRowCol(dct, dctsymidx, uels, &idx);
//
//      if (idx < 0) {
//         error("[DCT] ERROR: could not find any record for symbol %s", symname);
//         return Error_SymbolNotInTheGamsRim;
//      }
// 
//      int size = dctSymEntries(dct, dctsymidx);
//      assert(size >= 0);
//
//      dctFindClose(dct, fh);
//
//      if (toktype == TOK_GMS_VAR) {
//         avar_setcompact(data->payload.v, size, idx);
//      } else {
//         aequ_setcompact(data->payload.e, size, idx);
//      }
//
//      return OK;
//   }

  /* ----------------------------------------------------------------------
   * TODO: use symnr!
   * ---------------------------------------------------------------------- */

   void *symiterptr = NULL;
   bool single_record = true;
   data->allrecs = true;
   char  uels_str[GLOBAL_MAX_INDEX_DIM][GLOBAL_UEL_IDENT_SIZE];
   const char *uels_strp[GLOBAL_MAX_INDEX_DIM];

   if (!full_records) {
      /* initialize UELs */
      for (unsigned i = 0; i < dim; ++i) {
         uels_strp[i] = uels_str[i];

         if (uels[i] == 0) {
            uels_strp[i] = " ";
            single_record = false;
            continue;
         }

            GMD_CHK(gmdGetUelByIndex, gmd, uels[i], uels_str[i]);

         if (uels[i] != 0) { data->allrecs = false; }
      }
   } else {
      single_record = false;
   }

   if (single_record) {
      if(!gmdFindRecord(gmd, symptr, (const char **)uels_strp, &symiterptr)) {
         goto missing_record_in_symbol;
      }

      double vals[GMS_VAL_MAX];
      int dummyUelIdx[GLOBAL_MAX_INDEX_DIM];
      GMD_CHK(gmdGetRecordRaw, gmd, symiterptr, dim, dummyUelIdx, vals);

      switch (toktype) {
         case TOK_GMS_SET:
         case TOK_GMS_EQU:
         case TOK_GMS_VAR: {
            double val = vals[GMS_VAL_LEVEL];
            S_CHECK(chk_dbl2int(val, __func__));
            data->itmp = (int)val;
            break;
         }
         case TOK_GMS_PARAM: {
            data->dtmp = vals[GMS_VAL_LEVEL];
            break;
         }
         default:
            error("[empinterp] Unexpected token type '%s'\n", toktype2str(toktype));
            return Error_RuntimeError;
      }

      data->nrecs = 1;

   } else { /* Not a single record */

      if (full_records) {
         if (!gmdFindFirstRecord(gmd, symptr, &symiterptr)) {
            goto missing_record_in_symbol;
         }
      } else {
         if (!gmdFindFirstRecordSlice(gmd, symptr, uels_strp, &symiterptr)) {
            goto missing_record_in_symbol;
         }
      }

      /* ------------------------------------------------------------------
          * Returns the number of records stored for a given symbol.
          * ------------------------------------------------------------------ */

      int size;
      GMD_CHK(gmdSymbolInfo, gmd, symptr, GMD_NRRECORDS, &size, NULL, NULL);
      assert(size >= 0);

      rhp_idx i = 0;

      switch (toktype) {
      case TOK_GMS_SET:
      case TOK_GMS_EQU:
      case TOK_GMS_VAR: {

         S_CHECK(scratchint_ensure(data->iscratch, size));
         rhp_idx *idxs = data->iscratch->data;

         do {
            double vals[GMS_VAL_MAX];
            int dummyUelIdx[GLOBAL_MAX_INDEX_DIM];
            GMD_CHK(gmdGetRecordRaw, gmd, symiterptr, dim, dummyUelIdx, vals);
            double val = vals[GMS_VAL_LEVEL];
            S_CHECK(chk_dbl2int(val, __func__));
            idxs[i++] = (int)val;
         } while ( gmdRecordMoveNext(gmd, symiterptr) );

         break;
      }


      case TOK_GMS_PARAM: {

         S_CHECK(scratchdbl_ensure(data->dscratch, size));
         double *dbls = data->dscratch->data;

         do {
            double vals[GMS_VAL_MAX];
            int dummyUelIdx[GLOBAL_MAX_INDEX_DIM];
            GMD_CHK(gmdGetRecordRaw, gmd, symiterptr, dim, dummyUelIdx, vals);
            dbls[i++] = vals[GMS_VAL_LEVEL];
         } while (gmdRecordMoveNext(gmd, symiterptr));

         break;
      }
      default:
         error("[empinterp] Unexpected token type '%s'\n", toktype2str(toktype));
         return Error_RuntimeError;
      }

      data->nrecs = i;

   }

   gmdFreeSymbolIterator(gmd, symiterptr);

   if (O_Output & PO_TRACE_EMPINTERP) {
      trace_empinterp("[GMD] Read %u records", data->nrecs);

      if (toktype == TOK_GMS_PARAM) {
         trace_empinterp(":");
         unsigned nrecs = data->nrecs;
         if (nrecs == 1) {
            trace_empinterp(" %e\n", data->dtmp);
         } else {
            for (unsigned i = 0; i < nrecs; ++i) {
               trace_empinterp(" %e", data->dscratch->data[i]);
            }
            trace_empinterp("\n");

         }
      } else {
         trace_empinterp("\n");
      }
   }

   /* HACK: in Imm mode, we expect the content of a variable and equation to be in
    * different data structure than in VM mode ... */
   if (data->type != GmsSymIteratorTypeImm) { return OK; }

   switch (toktype) {
   case TOK_GMS_VAR:
      if (single_record) {
         avar_setcompact(data->payload.v, 1, data->itmp);
      } else {
         avar_setlist(data->payload.v, data->nrecs, data->iscratch->data);
      }
      break;
   case TOK_GMS_EQU:
      if (single_record) {
         aequ_ascompact(data->payload.e, 1, data->itmp);
      } else {
         aequ_aslist(data->payload.e, data->nrecs, data->iscratch->data);
      }
   default: ;
   }

   return OK;

missing_record_in_symbol: ;
   GMD_CHK(gmdSymbolInfo, gmd, symptr, GMD_NAME, NULL, NULL, buf);
   error("[empinterp] ERROR: in the GMD, could not find record for symbol %s", buf);
   if (dim > 1 || (dim == 1 && uels[0] > 0)) {
      errormsg("(");
      for (unsigned i = 0; i < dim; ++i) {
         char quote = '\'';
         int uel = uels[i];
         if (uel > 0) { GMD_CHK(gmdGetUelByIndex, gmd, uel, buf);
         } else { strcpy(buf, "*"); }
         if (i > 0) { errormsg(","); }
         error("%c%s%c", quote, buf, quote);
      }
      errormsg(")");
   }
   errormsg("\n");
  
   int offset;
   error("[gmd] GMD errors: %n\n", &offset);
   gmdGetLastError(gmd, buf);
   error("%*s%s\n", offset, "", buf);

   S_CHECK(print_help(toktype, dim, symname));

   return Error_SymbolNotInTheGamsRim;

}
