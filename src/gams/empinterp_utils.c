
#include "compat.h"
#include "empinterp_utils.h"
#include "empinterp_vm_utils.h"
#include "mdl.h"
#include "printout.h"
#include "win-compat.h"

#include "gamsapi_utils.h"

static inline TokenType ident2toktype(IdentType type)
{
   switch (type) {
   case IdentVar:
      return TOK_GMS_VAR;
   case IdentEqu:
      return TOK_GMS_EQU;
   default:
      return TOK_UNSET;
   }
}

/* TODO: delete ?*/
static inline IdentType toktype2ident(TokenType toktype, unsigned dim)
{
   switch(toktype) {
   case TOK_GMS_SET:
      if (dim == 1) return IdentSet;
      return IdentMultiSet;
   case TOK_GMS_PARAM:
      if (dim == 0) return IdentScalar;
      if (dim == 1) return IdentVector;
      return IdentParam;
   case TOK_GMS_VAR:
      return IdentVar;
   case TOK_GMS_EQU:
      return IdentEqu;
   default:
      return IdentNotFound;
   }
}


int dct_resolve(dctHandle_t dct, DctResolveData * restrict data)
{
   int symidx, *uels;
   unsigned dim;
   TokenType toktype;
   bool compact;
   if (data->type == GmsSymIteratorTypeImm) {
      symidx = data->symiter.imm.symidx;
      dim = data->symiter.imm.symiter->indices.nargs;
      toktype = data->symiter.imm.toktype;
      uels =  data->symiter.imm.symiter->uels;
      compact = data->symiter.imm.symiter->compact;
   } else {
      assert(data->type == GmsSymIteratorTypeVm);
      symidx = (int)data->symiter.vm->symbol.idx;
      dim = data->symiter.vm->symbol.dim;
      toktype = ident2toktype(data->symiter.vm->symbol.type);
      uels =  data->symiter.vm->uels;
      compact = data->symiter.vm->compact;

   }

   assert(dim < GMS_MAX_INDEX_DIM);

   if (O_Output & PO_TRACE_EMPPARSER) {
      char buf[GMS_SSSIZE];
      char quote = ' ';
      if (dctSymName(dct, symidx, buf, sizeof(buf))) {
         strcpy(buf, "ERROR resolving symbol");
      }
      trace_empparser("[empinterp] resolving GAMS symbol '%s' #%d of type %s and dim %u.\n",
                      buf, symidx, toktype2str(toktype), dim);
      if (dim > 1 || (dim == 1 && uels[0] > 0)) {
         trace_empparsermsg("[empinterp] UELs values are:\n");
         for (unsigned i = 0; i < dim; ++i) {
            int uel = uels[i];
            if (uel > 0) { dctUelLabel(dct, uels[i], &quote, buf, sizeof(buf));
            } else { strcpy(buf, "'*'"); }
            trace_empparser("%*c [%5d] %c%s%c\n", 11, ' ', uel, quote, buf, quote);
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
         error("[empinterp] ERROR: could not find symbol #%d in the DCT.\n", symidx);
         return Error_NotInTheRim;
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
      } else {
         rhp_idx i = 0;
         S_CHECK(scratchint_ensure(data->scratch, size));

         rhp_idx *idxs = data->scratch->data;

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

   dctUelLabel(dct, uel, &quote, buf, sizeof(buf));
   printout(mode, "[%5d] %c%s%c", uel, quote, buf, quote);
}

void dct_printuel(dctHandle_t dct, int uel, unsigned mode, int *offset)
{
   char buf[GMS_SSSIZE] = " ";
   char quote[] = " ";

   if (uel == 0) {
      printout(mode, "'*'%n", offset);
   } else if (dctUelLabel(dct, uel, quote, buf, sizeof(buf))) {
      printout(mode, "UEL#%d%n", uel, offset);
   } else if (quote[0] != ' ') {
      printout(mode, "%c%s%c%n", quote[0], buf, quote[0], offset);
   } else {
      printout(mode, "%s%n", buf, offset);
   }
}

/**
 * @brief Generate the full label from entry (basename and UELs)
 *
 * Set the label name to basename(uel1_name, uel2_name, uel3_name)
 *
 * @param entry           the entry with basename and uels
 * @param dcth            the handle to the dictionary (dct)
 * @param[out] labelname  the generated labelname
 *
 * @return                the error code
 */
int genlabelname(DagRegisterEntry * restrict entry, dctHandle_t dcth,
                 char **labelname)
{
   assert(dcth);
   assert(entry->basename && (entry->basename_len > 0));

   /* No UEL, just copy basename into labelname */
   if (entry->dim == 0) {
      *labelname = strndup(entry->basename, entry->basename_len);
      return OK;
   }

   gdxStrIndex_t uels;
   unsigned uels_len[GMS_MAX_INDEX_DIM];

   size_t strsize = entry->basename_len;
   size_t size = strsize;

   for (unsigned i = 0, len = entry->dim; i < len; ++i) {
      char dummyquote = ' ';
      dct_call_rc(dctUelLabel, dcth, entry->uels[i], &dummyquote, uels[i], sizeof(uels[i]));
      uels_len[i] = strlen(uels[i]);
      strsize += uels_len[i];
   }

   strsize += 2 + entry->dim; /* 2 parenthesis and dim-1 commas and NUL*/
   char *lname;
   MALLOC_(lname, char, strsize);

   memcpy(lname, entry->basename, size);
   lname[size++] = '(';

   unsigned uel_len = uels_len[0];
   memcpy(&lname[size], uels[0], uel_len*sizeof(char));
   size += uel_len;

   for (unsigned i = 1, len = entry->dim; i < len; ++i) {
      lname[size++] = ',';
      uel_len = uels_len[i];
      memcpy(&lname[size], uels[i], uel_len*sizeof(char));
      size += uel_len;
   }

   lname[size++] = ')';
   lname[size] = '\0';
   assert(size+1 == strsize);

   *labelname = lname;

   return OK;
}


DagRegisterEntry* regentry_new(const char *basename, unsigned basename_len,
                               uint8_t dim)
{
   DagRegisterEntry *regentry;
   MALLOCBYTES_NULL(regentry, DagRegisterEntry, sizeof(DagRegisterEntry) + dim*sizeof(int));

   regentry->basename = basename;
   if (basename_len >= UINT16_MAX) {
      error("[empinterp] EMPDAG label '%s' must be smaller than %u\n", basename, UINT16_MAX);
      FREE(regentry);
      return NULL;
   }

   regentry->basename_len = basename_len;
   regentry->dim = dim;

   return regentry;
}

DagRegisterEntry* regentry_dup(DagRegisterEntry *regentry)
{
   DagRegisterEntry *regentry_;
   uint8_t dim = regentry->dim;

   size_t obj_size = sizeof(DagRegisterEntry) + dim*sizeof(int);
   MALLOCBYTES_NULL(regentry_, DagRegisterEntry, obj_size);

   memcpy(regentry_, regentry, obj_size);

   return regentry_;
}

const char *edgetype_str(EdgeType type)
{
   switch (type) {
   case EdgeVFobjSimple:
      return "EdgeVFobjSimple";
   case EdgeVFcons:
      return "EdgeVFcons";
   case EdgeCtrl:
      return "EdgeCtrl";
   case EdgeNash:
      return "EdgeNash";
   default:
      return "ERROR: invalid Edge type";
   }
}

int runtime_error(unsigned linenr)
{
   error("[empinterp] ERROR line %u: unexpected runtime error\n", linenr);
   return Error_RuntimeError;
}

int interp_ops_is_imm(Interpreter *interp)
{

   if (interp->ops->type != ParserOpsImm) {
      error("[empinterp] RUNTIME ERROR at line %u: while parsing the %s "
            "keyword the empinfo interpreter is in the VM mode. In the old-style"
            "empinfo syntax, one cannot use loop or any EMPDAG features.\n",
            interp->linenr, toktype2str(interp->cur.type));
      return Error_EMPRuntimeError;
   }

   return OK;
}

int has_longform_solve(Interpreter *interp)
{
   rhp_idx objvar;
   S_CHECK(mdl_getobjvar(interp->mdl, &objvar));

   if (objvar == IdxNA) {
      error("[empinterp] ERROR: while parsing the %s keyword: the GAMS solve "
            "statement must be in the long form, that is contain a "
            "minimization/maximization statement\n",
            toktype2str(interp->cur.type));

      return Error_EMPIncorrectInput;
   }

   return OK;
}


