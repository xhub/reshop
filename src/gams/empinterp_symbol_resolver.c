#include "empinterp_symbol_resolver.h"
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
            } else { strcpy(buf, "'*'"); }
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
         error("[empinterp] ERROR: in the DCT, could not find record for symbol %s", buf);
         if (dim > 1 || (dim == 1 && uels[0] > 0)) {
            errormsg("(");
            for (unsigned i = 0; i < dim; ++i) {
               char quote = '\'';
               int uel = uels[i];
               if (uel > 0) { dctUelLabel(dct, uels[i], &quote, buf, sizeof(buf));
               } else { strcpy(buf, "'*'"); }
               if (i > 0) { errormsg(","); }
               error("%c%s%c", quote, buf, quote);
            }
            errormsg(")");
         }
         errormsg("\n");

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
            aequ_setcompact(data->payload.e, size, idx);
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
            aequ_setlist(data->payload.e, i, idxs);
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

   assert(uel < dctNUels(dct));
   dctUelLabel(dct, uel, &quote, buf, sizeof(buf));
   printout(mode, "[%5d] %c%s%c", uel, quote, buf, quote);
}

void dct_printuel(dctHandle_t dct, int uel, unsigned mode, int *offset)
{
   char buf[GMS_SSSIZE] = " ";
   char quote[] = "'";

   assert(uel < dctNUels(dct));

   if (uel == 0) {
      printout(mode, "'*'%n", offset);
   } else if (dctUelLabel(dct, uel, quote, buf, sizeof(buf))) {
      printout(mode, "UEL#%d%n", uel, offset);
   } else if (quote[0] != ' ') {
      printout(mode, "%c%s%c%n", quote[0], buf, quote[0], offset);
   } else {
      printout(mode, "'%s'%n", buf, offset);
   }
}

// HACK
int gmd_read(gmdHandle_t gmd, dctHandle_t dct, GmsResolveData * restrict data, const char *symname)
{
   int symnr, *uels;
   unsigned dim;
   TokenType toktype;
   bool compact;
   char buf[GMS_SSSIZE] = " ";

   if (data->type == GmsSymIteratorTypeImm) {
      symnr = data->symiter.imm.symidx;
      dim = data->symiter.imm.symiter->indices.nargs;
      toktype = data->symiter.imm.toktype;
      uels =  data->symiter.imm.symiter->uels;
      compact = data->symiter.imm.symiter->compact;
   } else if (data->type == GmsSymIteratorTypeVm) {
      symnr = (int)data->symiter.vm->ident.idx;
      dim = data->symiter.vm->ident.dim;
      toktype = ident2toktype(data->symiter.vm->ident.type);
      uels =  data->symiter.vm->uels;
      compact = data->symiter.vm->compact;
   } else {
      TO_IMPLEMENT("unknown iterator type");
   }

   assert(dim < GMS_MAX_INDEX_DIM);

  /* ----------------------------------------------------------------------
   * WARNING: this relies on symidx being GMD_NUMBER
   * ---------------------------------------------------------------------- */
   void *symptr = NULL;
   if (data->type == GmsSymIteratorTypeVm) {
       GMD_CHK(gmdFindSymbol, gmd, symname,  &symptr);
   } else {
      GMD_CHK(gmdGetSymbolByNumber, gmd, symnr, &symptr);
   }

   if (O_Output & PO_TRACE_EMPINTERP) {
      char quote = '\'';
      GMD_CHK(gmdSymbolInfo, gmd, symptr, GMD_NAME, NULL, NULL, buf);
      trace_empinterp("[GMD] resolving GAMS symbol '%s' of type %s and dim %u.\n",
                      buf, toktype2str(toktype), dim);
      if (dim > 1 || (dim == 1 && uels[0] > 0)) {
         // HACK: this is wrong
         trace_empinterp("[GMD] UELs values are:\n");
         for (unsigned i = 0; i < dim; ++i) {
            int uel = uels[i];
            if (uel > 0) { if (dct) {
                  char quote_[] = " ";
                  if (dctUelLabel(dct, uel, quote_, buf, sizeof(buf))) {
                     const char err_uel[] = "invalid UEL";
                     memcpy(buf, err_uel, sizeof(err_uel));
                  }
               } else { GMD_CHK(gmdGetUelByIndex, gmd, uels[i], buf); }
            } else { strcpy(buf, "'*'"); }
            trace_empinterp("%*c [%5d] %c%s%c\n", 11, ' ', uel, quote, buf, quote);
         }
      }
   }

   switch (toktype) {
   case TOK_GMS_VAR:
      TO_IMPLEMENT("gmd_resolve for variables");
   case TOK_GMS_EQU:
      TO_IMPLEMENT("gms_resolve for params in immediate mode");
   case TOK_GMS_SET:
   case TOK_GMS_PARAM:
   {

      /* ------------------------------------------------------------------
       * Return first row/column in the symbol referenced by symindex that
       * is indexed by the UELs in uelindices (uelindices[k]=0 is wildcard).
       * Since the routine can fail you should first check rcindex and then
       * the returned handle.
       * ------------------------------------------------------------------ */

      void *symiterptr = NULL;
      bool single_record = true;
      data->allrecs = true;
      char  uels_str[GLOBAL_MAX_INDEX_DIM][GLOBAL_UEL_IDENT_SIZE];
      char *uels_strp[GLOBAL_MAX_INDEX_DIM];

      if (!compact) {
         /* initialize UELs */
         for (unsigned i = 0; i < dim; ++i) {
            uels_strp[i] = uels_str[i];
            if (dct) {
               char quote_[] = " ";
               if (dctUelLabel(dct, uels[i], quote_, uels_strp[i], GLOBAL_UEL_IDENT_SIZE)) {
                  error("[DCT] Could not find UEL #%d\n", uels[i]);
                  return Error_EMPRuntimeError;
               } 

            } else {
               GMD_CHK(gmdGetUelByIndex, gmd, uels[i], uels_str[i]);
            }
            if (uels[i] == 0) { single_record = false; }
            if (uels[i] != 0) { data->allrecs = false; }
         }
      } else {
         single_record = false;
      }

      if (single_record) {
         GMD_FIND_CHK(gmdFindRecord, gmd, symptr, (const char **)uels_strp, &symiterptr);

         double vals[GMS_VAL_MAX];
         GMD_CHK(gmdGetRecordRaw, gmd, symiterptr, dim, uels, vals);
         if (toktype == TOK_GMS_SET) {
            double val = vals[GMS_VAL_LEVEL];
            S_CHECK(chk_dbl2int(val, __func__));
            data->itmp = (int)val;
         } else {
            data->dtmp = vals[GMS_VAL_LEVEL];
         }

         data->nrecs = 1;
      } else { /* Not a single record */

         if (compact) {
            GMD_FIND_CHK(gmdFindFirstRecord, gmd, symptr, &symiterptr);
         } else {
            GMD_FIND_CHK(gmdFindFirstRecordSlice, gmd, symptr, (const char **)uels_strp, &symiterptr);
         }

         /* ------------------------------------------------------------------
          * Returns the number of records stored for a given symbol.
          * ------------------------------------------------------------------ */

         int size;
         GMD_CHK(gmdSymbolInfo, gmd, symptr, GMD_NRRECORDS, &size, NULL, NULL);
         assert(size >= 0);

         S_CHECK(scratchint_ensure(data->iscratch, size));

         rhp_idx i = 0;
         bool has_next;

         if (toktype == TOK_GMS_SET) {

            S_CHECK(scratchint_ensure(data->iscratch, size));
            rhp_idx *idxs = data->iscratch->data;

            do {
               double vals[GMS_VAL_MAX];
               GMD_CHK(gmdGetRecordRaw, gmd, symiterptr, dim, uels, vals);
               double val = vals[GMS_VAL_LEVEL];
               S_CHECK(chk_dbl2int(val, __func__));
               idxs[i++] = (int)val;
               has_next = gmdRecordHasNext(gmd, symiterptr);
               if (has_next) { gmdRecordMoveNext(gmd, symiterptr); }
            } while (has_next);


         } else {

            S_CHECK(scratchdbl_ensure(data->dscratch, size));
            double *dbls = data->dscratch->data;

            do {
               double vals[GMS_VAL_MAX];
               GMD_CHK(gmdGetRecordRaw, gmd, symiterptr, dim, uels, vals);
               dbls[i++] = vals[GMS_VAL_LEVEL];
               has_next = gmdRecordHasNext(gmd, symiterptr);
               if (has_next) { gmdRecordMoveNext(gmd, symiterptr); }
            } while (has_next);

         }

         data->nrecs = i;

      }

      gmdFreeSymbolIterator(gmd, symiterptr);
      break;
   }
   default:
      error("[empinterp] Unexpected token type '%s'\n", toktype2str(toktype));
      return Error_RuntimeError;
   }

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

   return OK;
}


