
#include "compat.h"
#include <sys/stat.h>

#include "embcode_empinfo.h"
#include "empinfo.h"
#include "empinterp.h"
#include "empinterp_priv.h"
#include "empinterp_utils.h"
#include "empparser_priv.h"
#include "empparser_utils.h"
#include "fs_func.h"
#include "gamsapi_utils.h"
#include "macros.h"
#include "mathprgm.h"
#include "mathprgm_data.h"
#include "mathprgm_priv.h"
#include "ovf_parameter.h"
#include "status.h"

#include "gmdcc.h"

/**
 * @brief Try to resolve a string lexeme as a gams symbol using a dictionary
 *
 * @param interp  the interpreter
 * @param tok     the token containing the lexeme
 *
 * @return        the error code
 */
static int resolve_lexeme_as_gmssymb_via_gmd(Interpreter * restrict interp, Token * restrict tok)
{
   GamsSymData *symdat = &tok->symdat;

   size_t lexeme_len = emptok_getstrlen(tok);
   if (lexeme_len >= GMS_SSSIZE) { goto _not_found; }

   char lexeme[GMS_SSSIZE];
   memcpy(lexeme, emptok_getstrstart(tok), lexeme_len * sizeof(char));
   lexeme[lexeme_len] = '\0';

   gmdHandle_t gmd = interp->gmd;
   assert(gmd);

   void *symptr;
   if (!gmdFindSymbol(gmd, lexeme, &symptr)) { /* We do not fail here as it could be a UEL */
      int uelidx;
      if (!gmdFindUel(gmd, lexeme, &uelidx)) {
         error("[embcode] ERROR while calling 'gmsFindUEL' for lexeme '%s'", lexeme);
         return Error_GamsCallFailed;
      }

      if (uelidx <= 0) { goto _not_found; }

      tok->type = TOK_GMS_UEL;
      symdat->idx = uelidx;

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

   int symidx;
   if (!gmdSymbolInfo(gmd, symptr, GMD_NUMBER, &symidx, NULL, NULL)) {
      error("[embcode] ERROR: could not query number of symbol '%s'\n", lexeme);
      return Error_GamsCallFailed;
   }
   assert(symidx >= 0);
   // TODO: this is a hack!
   symdat->idx = symidx;


   /* ---------------------------------------------------------------------
    * Save the dimension of the symbol index
    * --------------------------------------------------------------------- */

   int symdim;
   if (!gmdSymbolInfo(gmd, symptr, GMD_DIM, &symdim, NULL, NULL)) {
      error("[embcode] ERROR: could not query dimension of symbol '%s'\n", lexeme);
      return Error_GamsCallFailed;
   }
   /* What does a negative value of symdim means? */
   assert(symdim >= 0);

   /* ---------------------------------------------------------------------
    * Save the type of the symbol
    * --------------------------------------------------------------------- */

   int symtype;
   if (!gmdSymbolInfo(gmd, symptr, GMD_TYPE, &symtype, NULL, NULL)) {
      error("[embcode] ERROR: could not query type of symbol '%s'\n", lexeme);
      return Error_GamsCallFailed;
   }

   switch (symtype) {
   case GMS_DT_VAR:
      tok->type = TOK_GMS_VAR;
      symdat->type = IdentVar;
      break;
   case GMS_DT_EQU:
      tok->type = TOK_GMS_EQU;
      symdat->type = IdentEqu;
      break;
   case GMS_DT_SET:
      if (symdim == 1) {
         tok->type = TOK_GMS_SET;
         symdat->type = IdentGmdSet;
      } else {
         tok->type = TOK_GMS_MULTISET;
         symdat->type = IdentGmdMultiSet;
      }
      break;
   case GMS_DT_PAR:
      tok->type = TOK_GMS_PARAM;
      symdat->type = symdim == 0 ? IdentScalar : symdim == 1 ? IdentVector : IdentParam;
      break;
   case GMS_DT_ALIAS:
      errormsg("[embcode] ERROR: support for alias needs to be done\n");
      return Error_NotImplemented;
      // tok->type = TOK_GMS_ALIAS;
      break;
   default:
      error("[embcode] ERROR: unexpected type %d for symbol '%s'\n", symtype, lexeme);
      return runtime_error(interp->linenr);
   }

   symdim = MAX(symdim, 1);
   symdat->dim = symdim;
   symdat->ptr = symptr;

   symdat->read = false;

  /* ----------------------------------------------------------------------
   * If gmdout is set, we copy sets and parameters (if this hasn't been done yet)
   *
   * TODO: gmdFindSymbol returning false when the symbol doesn't exists isn't great
   * ---------------------------------------------------------------------- */

   void *symptr2 = NULL;
   gmdHandle_t gmdout = interp->gmdout;
   if (gmdout && (symtype == GMS_DT_SET || symtype == GMS_DT_PAR) && 
      !gmdFindSymbol(gmdout, lexeme, &symptr2)) {

      GMD_CHK(gmdAddSymbol, gmdout, lexeme, symdim, symtype, 0, "", &symptr2);

      GMD_CHK(gmdCopySymbol, gmdout, symptr2, symptr);
   }

   return OK;

_not_found:
   symdat->idx = IdxNotFound;
   tok->type = TOK_IDENT;

   return OK;
}

static int get_uelidx_via_gmd(Interpreter * restrict interp, const char uelstr[GMS_SSSIZE], int * restrict uelidx)
{
   gmdHandle_t gmd = interp->gmd;

   if (!gmdFindUel(gmd, uelstr, uelidx)) {
      error("[embcode] ERROR while calling 'gmsFindUEL' for lexeme '%s'", uelstr);
      return Error_GamsCallFailed;
   }

   return OK;
}

static int get_uelstr_via_gmd(Interpreter *interp, int uelidx, unsigned uelstrlen, char uelstr[VMT(static uelstrlen)])
{
   gmdHandle_t gmd = interp->gmd;
   assert(gmd);

   if (uelstrlen < GMS_SSSIZE) {
      error("[embcode] ERROR in %s: string argument is too short\n", __func__);
      return Error_SizeTooSmall;
   }

   GMD_CHK(gmdGetUelByIndex, gmd, uelidx, uelstr);

   return OK;
}

NONNULL static
int embcode_register_basename(Interpreter *interp, DagRegisterEntry *regentry)
{
   S_CHECK(dagregister_add(&interp->dagregister, regentry));

    return OK;
}

static int emb_gms_resolve(Interpreter* restrict interp)
{
   /* TODO(xhub) unclear if we need a real function here */
   #if 0
   TokenType toktype = parser_getcurtoktype(interp);
   DctResolveData data;

   data.type = GmsSymIteratorTypeImm;
   data.symiter.imm.toktype = toktype;
   data.symiter.imm.symidx = interp->cur.gms_dct.idx;
   // needed?
   data.symiter.imm.symiter = &interp->gms_sym_iterator;

   gmdHandle_t gmd = interp->gmd;

   // needed?
   interp->gms_sym_iterator.active = false;
#endif
   return OK;
}

static int emb_gms_parse(Interpreter * restrict interp, unsigned * restrict p)
{
   TokenType toktype;

   /* ---------------------------------------------------------------------
    * We distinguish between peeked token being '(' or not
    * --------------------------------------------------------------------- */

   unsigned p2 = *p;
   S_CHECK(peek(interp, &p2, &toktype));

   if (toktype == TOK_LPAREN) {
      unsigned i = 0;
      interp_peekseqstart(interp);

      do {
         bool has_single_quote = false, has_double_quote = false;

         if (i >= GMS_MAX_INDEX_DIM) {
            error("[embcode] ERROR: EMP identifier '%.*s' has more than %u identifiers!\n",
                  interp->cur.len, interp->cur.start, GMS_MAX_INDEX_DIM);
            return Error_EMPIncorrectInput;
         }

         /* get the next token */
         S_CHECK(peek(interp, &p2, &toktype));

         if (toktype == TOK_SINGLE_QUOTE) {
            has_single_quote = true;
         } else if (toktype == TOK_DOUBLE_QUOTE) {
            has_double_quote = true;
         }

         if (has_single_quote || has_double_quote) {
            char quote = toktype == TOK_SINGLE_QUOTE ? '\'' : '"';
            S_CHECK(parser_peekasUEL(interp, &p2, quote, &toktype));

            if (toktype == TOK_UNSET) {
               const Token *tok = &interp->peek;
               error("[embcode] ERROR line %u: %c%.*s%c is not a UEL\n", interp->linenr,
                     quote, tok->len, tok->start, quote);
               return Error_EMPIncorrectSyntax;
            }

            if (toktype != TOK_STAR && toktype != TOK_GMS_UEL) {
               return runtime_error(interp->linenr);
            }

         } else {
            PARSER_EXPECTS_PEEK(interp, "A string (subset, variable) is required",
                                TOK_GMS_SET, TOK_STAR, TOK_IDENT);

         }

#if 0
         switch (toktype) {
         case TOK_GMS_SET:
            S_CHECK(parser_filter_set(interp, i, -interp->peek.symdat.idx));
            break;
         case TOK_GMS_UEL:
            S_CHECK(parser_filter_set(interp, i, interp->peek.symdat.idx));
            break;
         case TOK_STAR:
            S_CHECK(parser_filter_set(interp, i, 0));
            break;
         default:
            errormsg("[embcode] ERROR: unexpected failure.\n");
            return Error_RuntimeError;
         }
#endif

         i++;

         S_CHECK(peek(interp, &p2, &toktype));

      } while (toktype == TOK_COMMA);

      S_CHECK(parser_expect_peek(interp, "Closing ')' is required", TOK_RPAREN));

      UNUSED ptrdiff_t sym_total_len = interp->peek.start - interp->cur.start + interp->peek.len;

      assert(sym_total_len >= 0 && sym_total_len < INT_MAX);

      if (i != interp->cur.symdat.dim) {
         error("[embcode] ERROR: GAMS symbol '%.*s' has dimension %d but %u "
               "indices were given!\n",
               interp->cur.len, interp->cur.start, interp->cur.symdat.dim, i);
         return Error_EMPIncorrectInput;
      }

      /* update the counter */
      *p = p2;
      interp_peekseqend(interp);

   }

   S_CHECK(emb_gms_resolve(interp));

   interp->cur.symdat.read = true;

   return OK;
}



NONNULL static
int emb_identaslabels(Interpreter * restrict interp, unsigned * restrict p, ArcType edge_type)
{
   UNUSED const char* basename = emptok_getstrstart(&interp->cur);
   UNUSED unsigned basename_len = emptok_getstrlen(&interp->cur);

   TokenType toktype;
   S_CHECK(advance(interp, p, &toktype))

   if (toktype != TOK_LPAREN) {
      TO_IMPLEMENT("Label without indices");
   }

   GmsIndicesData indices;
   gms_indicesdata_init(&indices);

   S_CHECK(parse_gmsindices(interp, p, &indices));

   TO_IMPLEMENT("emb_identaslabels needs to be finished");
}


static int emb_mp_new(Interpreter *interp, RhpSense sense, MathPrgm **mp)
{
   /* ---------------------------------------------------------------------
    * If we had a label, then use it!
    * --------------------------------------------------------------------- */
   char *labelname = NULL;

   DagRegisterEntry *regentry = interp->regentry;
   if (regentry) {
      S_CHECK(genlabelname(regentry, interp, &labelname));
      interp->regentry = NULL;
   }

   S_CHECK(embcode_register_basename(interp, regentry));

   return OK;
}

static int emb_mp_addcons(Interpreter *interp, MathPrgm *mp)
{
   return OK;
}

static int emb_mp_addvars(Interpreter *interp, MathPrgm *mp)
{
   return OK;
}

static int emb_mp_addvipairs(Interpreter *interp, MathPrgm *mp)
{
   return OK;
}

static int emb_mp_addzerofunc(Interpreter *interp, MathPrgm *mp)
{
   return OK;
}

static int emb_mp_finalize(UNUSED Interpreter *interp, MathPrgm *mp)
{
   return OK;
}

static int emb_mp_setobjvar(Interpreter *interp, MathPrgm *mp)
{
   return OK;
}

static int emb_mp_setprobtype(UNUSED Interpreter *interp, MathPrgm *mp, unsigned probtype)
{
   if (probtype >= mdltypeslen) {
      error("[embcode] ERROR MP problem type %u is above the limit %u\n",
            probtype, mdltypeslen - 1);
      return Error_InvalidValue;
   }

   return OK;
}

static int emb_mp_settype(UNUSED Interpreter *interp, MathPrgm *mp, unsigned type)
{
  if (type > MpTypeLast) {
      error("[embcode] ERROR MP type %u is above the limit %d\n", type, MpTypeLast);
      return Error_InvalidValue;
   }

   return OK;
}

static int emb_mpe_new(Interpreter *interp, Nash **mpe)
{
   /* ---------------------------------------------------------------------
    * If we had a label, then use it!
    * --------------------------------------------------------------------- */
   char *labelname = NULL;

   DagRegisterEntry *regentry = interp->regentry;
   if (regentry) {
      S_CHECK(genlabelname(regentry, interp, &labelname));
      interp->regentry = NULL;
   }

   S_CHECK(embcode_register_basename(interp, regentry));

   trace_empinterp("[embcode] line %u: new Nash\n", interp->linenr);

   return OK;
}

static int emb_mpe_addmp(UNUSED Interpreter *interp, UNUSED unsigned mpe_id,
                         UNUSED MathPrgm *mp)
{
   return OK;
}

static int emb_mpe_finalize(UNUSED Interpreter *interp, UNUSED Nash *mpe)
{
   return OK;
}

static int emb_ovf_addbyname(UNUSED Interpreter* restrict interp, UNUSED EmpInfo *empinfo,
                          const char *name, UNUSED void **ovfdef_data)
{
   OvfDef *ovfdef;
   EmpInfo empinfo_fake;
   S_CHECK(ovfinfo_alloc(&empinfo_fake));
   S_CHECK(ovf_addbyname(&empinfo_fake, name, &ovfdef));
   *ovfdef_data = ovfdef;

   FREE(empinfo_fake.ovf);

   return OK;
}

static int emb_ovf_setrhovar(Interpreter* restrict interp, void *ovfdef_data)
{
   return OK;
}

static int emb_ovf_addarg(Interpreter* restrict interp, void *ovfdef_data)
{
   return OK;
}

static int emb_ovf_getparamsdef(UNUSED Interpreter* restrict interp, void *ovfdef_data,
                             const OvfParamDefList **paramsdef)
{
   OvfDef *ovfdef = ovfdef_data;

   *paramsdef = ovf_getparamdefs(ovfdef->idx);

   return OK;
}

static int emb_ovf_setparam(UNUSED Interpreter* restrict interp,
                            UNUSED void *ovfdef_data, UNUSED unsigned i,
                            UNUSED OvfArgType type, UNUSED OvfParamPayload payload)
{
   return OK;
}

static int emb_ovf_check(UNUSED Interpreter* restrict interp, void *ovfdef_data)
{
   OvfDef *ovfdef = ovfdef_data;
   ovf_def_free(ovfdef);
   return OK;
}

static int emb_mp_ccflib_new(Interpreter* restrict interp, unsigned ccflib_idx,
                             MathPrgm **mp)
{

   MathPrgm *mp_ = mp_new_fake();
   S_CHECK(mp_from_ccflib(mp_, ccflib_idx));

   DagRegisterEntry *regentry = interp->regentry;
   S_CHECK(embcode_register_basename(interp, regentry));

   trace_empinterp("[embcode] line %u: new CCFLIB MP\n", interp->linenr);

   *mp = mp_;

   return OK;
}

static int emb_mp_ccflib_finalize(Interpreter* restrict interp, MathPrgm *mp)
{
   /* Free the fake MP we created to have access to the CCF data */
   mp_free(mp);
   return OK;
}

static unsigned emb_ovf_param_getvecsize(UNUSED Interpreter* restrict interp,
                                      void *ovfdef_data, const OvfParamDef *pdef)
{
   return UINT_MAX;
}

static const char* emb_ovf_getname(UNUSED Interpreter* restrict interp, void *ovfdef_data)
{
   OvfDef *ovfdef = ovfdef_data;
   return ovfdef->name;
}

UNUSED static int read_param(UNUSED Interpreter *interp, UNUSED unsigned *p,
                             UNUSED IdentData *data, UNUSED DblScratch *scratch,
                             UNUSED unsigned *param_size)
{
   errormsg("[embcode] Reading a parameter is not supported in immediate mode");
   return Error_NotImplemented;
}

/* ----------------------------------------------------------------------
 * Misc functions
 * ---------------------------------------------------------------------- */

static int emb_ctr_markequasflipped(Interpreter* restrict interp)
{
   return OK;
}

UNUSED static int imm_ctr_dualvar(Interpreter *interp, bool is_flipped)
{
   return OK;
}

static int emb_read_param(Interpreter *interp, unsigned *p, IdentData *data,
                          const char *ident_str, unsigned *param_gidx)
{
   return OK;
}

static int emb_read_elt_vector(Interpreter *interp, const char *identstr,
                               IdentData *ident, GmsIndicesData *gmsindices, double *val)
{
   return OK;
}

const ParserOps parser_ops_emb = {
   .type                  = ParserOpsEmb,
   .ccflib_new            = emb_mp_ccflib_new,
   .ccflib_finalize       = emb_mp_ccflib_finalize,
   .ctr_markequasflipped  = emb_ctr_markequasflipped,
   .gms_get_uelidx        = get_uelidx_via_gmd,
   .gms_get_uelstr        = get_uelstr_via_gmd,
   .gms_parse             = emb_gms_parse,
   .identaslabels         = emb_identaslabels,
   .mp_addcons            = emb_mp_addcons,
   .mp_addvars            = emb_mp_addvars,
   .mp_addvipairs         = emb_mp_addvipairs,
   .mp_addzerofunc        = emb_mp_addzerofunc,
   .mp_finalize           = emb_mp_finalize,
   .mp_new                = emb_mp_new,
   .mp_setobjvar          = emb_mp_setobjvar,
   .mp_setprobtype        = emb_mp_setprobtype,
   .mp_settype            = emb_mp_settype,
   .mpe_finalize          = emb_mpe_finalize,
   .mpe_new               = emb_mpe_new,
   .mpe_addmp             = emb_mpe_addmp,
   .ovf_addbyname         = emb_ovf_addbyname,
   .ovf_addarg            = emb_ovf_addarg,
   .ovf_paramsdefstart    = emb_ovf_getparamsdef,
   .ovf_setparam          = emb_ovf_setparam,
   .ovf_setrhovar         = emb_ovf_setrhovar,
   .ovf_check             = emb_ovf_check,
   .ovf_param_getvecsize  = emb_ovf_param_getvecsize,
   .ovf_getname           = emb_ovf_getname,
   .read_param            = emb_read_param,
   .read_elt_vector       = emb_read_elt_vector,
   .resolve_lexeme_as_gmssymb = resolve_lexeme_as_gmssymb_via_gmd,
};

NONNULL static int write_empinfo(Interpreter *interp, const char *fname)
{
   int status = OK;
   FILE *f = NULL;

   f = fopen(fname, "wb");
   if (!f) {
      error("[embcode] ERROR: cannot open file named %s\n", fname);
      return Error_FileOpenFailed;
   }

   size_t writes = fwrite(interp->buf, sizeof(char), interp->read, f);

   if (writes != interp->read) {
      error("[embcode] ERROR while writing output EMPinfo file '%s': wrote %zu "
             "chars, but was exepcting to write %zu\n", fname, writes, interp->read);
      status = Error_SystemError;
      goto _exit;
   }

_exit:
   SYS_CALL(fclose(f));

   return status;
}

NONNULL static int copy_UELs(gmdHandle_t in, gmdHandle_t out)
{
   int nuels;
   GMD_CHK(gmdInfo, in, GMD_NRUELS, &nuels, NULL, NULL);

   for (int i = 1; i <= nuels; i++) {
      char buf[GMS_SSSIZE];
      int dummy;

      GMD_CHK(gmdGetUelByIndex, in, i, buf);
      GMD_CHK(gmdMergeUel, out, buf, &dummy);
   }

   return OK;
}

NONNULL static int interp_setup_embparse(Interpreter *interp, gmdHandle_t gmd)
{
   char msg[GMS_SSSIZE];
   interp->ops = &parser_ops_emb;
   interp->gmd = gmd;

   gmdHandle_t gmdout;
   if (!gmdCreate(&gmdout, msg, sizeof(msg))) {
      error("[embcode] ERROR: cannot create output GMD object: %s\n", msg);
      return Error_EMPRuntimeError;
   }

   interp->gmdout = gmdout;

   S_CHECK(copy_UELs(gmd, gmdout));

   S_CHECK(interp_create_buf(interp));

   return OK;
}

static NONNULL int embcode_empinfo_finalize(Interpreter *interp, const char *scrdir)
{
   int status = OK;
   if (!interp->gmdout) { return OK; }

   char *fname;
   MALLOC_(fname, char, strlen(scrdir) + strlen(EMBCODE_DIR) + strlen(DIRSEP) + 
           MAX(strlen(EMBCODE_GMDOUT_FNAME), strlen("empinfo.dat")) + 1);

   strcpy(fname, scrdir);
   strcat(fname, EMBCODE_DIR); strcat(fname, DIRSEP);

   if (mkdir(fname, S_IRWXU)) {
      perror("mkdir");
      error("[embcode] ERROR: Could not create output directory '%s'\n", fname);
      status = Error_SystemError;
      goto _exit;
   }

   size_t embrhp_dirlen = strlen(fname);
   strcat(fname, EMBCODE_GMDOUT_FNAME);

  /* ----------------------------------------------------------------------
   * Save the GMD
   * ---------------------------------------------------------------------- */

   gmdHandle_t gmd = interp->gmdout;
   if (!gmdWriteGDX(gmd, fname, true)) {
      char msg[GMS_SSSIZE];

      gmdGetLastError(gmd, msg);

      error("[embcode] ERROR: while writing output GMD as GDX %s: %s", fname, msg);
      status = Error_GamsCallFailed;
      goto _exit;
   }

   trace_empinterp("[embcode] Output GMD saved as '%s'\n", fname);

  /* ----------------------------------------------------------------------
   * Save the empinfo file
   * ---------------------------------------------------------------------- */
   memcpy(&fname[embrhp_dirlen], "empinfo.dat", sizeof("empinfo.dat"));

   S_CHECK_EXIT(write_empinfo(interp, fname));
_exit:
   FREE(fname);

   return status;
}

int embcode_process_empinfo(void *gmdobj, const char *scrdir, const char *fname)
{
   S_CHECK(file_readable(fname));

   gmdHandle_t gmd = (gmdHandle_t) gmdobj;

   char msg[GMS_SSSIZE];

   if (!gmdGetReady(msg, sizeof(msg))) {
      error("[embcode] ERROR: couldn't load the GMD libraries. Msg is '%s'\n", msg);
      return Error_SystemError;
   }

   trace_empinterp("[embcode] Processing EMPinfo file '%s'\n", fname);

   int status = OK;

   Interpreter interp;
   interp_init(&interp, NULL, fname);

   /* ---------------------------------------------------------------------
    * Read the file content in memory
    * --------------------------------------------------------------------- */

   S_CHECK_EXIT(interp_setup_embparse(&interp, gmd));

   /* ---------------------------------------------------------------------
    * Phase I: parse for keywords and if there is a DAG, create the nodes
    *
    * Phase II: If there is a DAG, finalize the EMPDAG (add edges)
    * --------------------------------------------------------------------- */

   unsigned p = 0;
   TokenType toktype;

   bool eof = skip_spaces_commented_lines(&interp, &p);
   if (eof) {
      printout(PO_LOGFILE, "[embcode] Empinfo file %s has no statement\n", fname);

      goto _exit;
   }

   S_CHECK_EXIT(advance(&interp, &p, &toktype));

   /* TODO(xhub) do we want to continue or error if we have an error */

   S_CHECK_EXIT(process_statements(&interp, &p, toktype))

   toktype = parser_getcurtoktype(&interp);
   if (toktype != TOK_EOF) {
      status = parser_err(&interp,
                          "unexpected superfluous token, no further token were expected.");
      goto _exit;

   }
   

_exit:
   if (status != OK) {
      interp_showerr(&interp);
   } else {
      status = embcode_empinfo_finalize(&interp, scrdir);

   }

   interp_free(&interp);

   return status;

   return OK;
}

