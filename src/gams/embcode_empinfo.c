
#include "compat.h"
#include <sys/stat.h>

#include "embcode_empinfo.h"
#include "empinfo.h"
#include "empinterp.h"
#include "empinterp_ops_utils.h"
#include "empinterp_priv.h"
#include "empinterp_utils.h"
#include "empinterp_vm.h"
#include "empinterp_vm_compiler.h"
#include "empparser_priv.h"
#include "empparser_utils.h"
#include "fs_func.h"
#include "gamsapi_utils.h"
#include "macros.h"
#include "mathprgm.h"
#include "mathprgm_priv.h"
#include "ovf_parameter.h"
#include "status.h"

#include "gmdcc.h"

NONNULL_AT(1) static
int embcode_register_basename(Interpreter *interp, DagRegisterEntry *regentry)
{
   S_CHECK(dagregister_add(&interp->dagregister, regentry));

    return OK;
}

static int emb_read_gms_symbol(Interpreter* restrict interp, UNUSED unsigned * p)
{
  /* ----------------------------------------------------------------------
   * In EMBCODE mode, we do not read symbols. We just fake it for the
   * reminder of the code.
   *
   * However, we still need to parse the conditional if present
   * ---------------------------------------------------------------------- */

   interp->cur.symdat.read = true;

   unsigned p2 = *p;
   TokenType toktype;
   S_CHECK(peek(interp, &p2, &toktype));

   if (toktype == TOK_CONDITION) {
      *p = p2;
      parser_cpypeek2cur(interp);
      S_CHECK(vm_parse_condition(interp, p));
//      S_CHECK(advance(interp, p, &toktype))
   }

   return OK;
}


NONNULL static
int emb_identaslabels(Interpreter * restrict interp, unsigned * restrict p, LinkType linktype)
{
   UNUSED const char* basename = emptok_getstrstart(&interp->cur);
   UNUSED unsigned basename_len = emptok_getstrlen(&interp->cur);

   TokenType toktype;
   S_CHECK(advance(interp, p, &toktype))

   if (toktype != TOK_LPAREN) {
      TO_IMPLEMENT("Label without indices");
   }

   GmsIndicesData indices;
   gmsindices_init(&indices);

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

static int emb_mp_setaschild(UNUSED Interpreter *interp, UNUSED MathPrgm *mp)
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

static int emb_nash_new(Interpreter *interp, Nash **nash)
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

   *nash = NULL;

   return OK;
}

static int emb_nash_addmp(UNUSED Interpreter *interp, UNUSED unsigned nash_id,
                         UNUSED MathPrgm *mp)
{
   return OK;
}

static int emb_nash_finalize(UNUSED Interpreter *interp, UNUSED Nash *nash)
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
                            UNUSED void *ovfdef_data, UNUSED unsigned pidx,
                            UNUSED OvfArgType type, UNUSED OvfParamPayload payload)
{
   return OK;
}

static int emb_ovf_check(UNUSED Interpreter* restrict interp, void *ovfdef_data)
{
   OvfDef *ovfdef = ovfdef_data;
   ovfdef_free(ovfdef);
   return OK;
}


static int emb_mp_ccflib_new(Interpreter* restrict interp, unsigned ccflib_idx,
                             MathPrgm **mp)
{

   MathPrgm *mp_ = mp_new_fake();
   S_CHECK(mp_from_ccflib(mp_, ccflib_idx));

   DagRegisterEntry *regentry = interp->regentry;
   if (regentry) {
      S_CHECK(embcode_register_basename(interp, regentry));
      interp->regentry = NULL;
   }

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
                          unsigned *param_gidx)
{
   return OK;
}

static int emb_read_elt_vector(Interpreter *interp, const char *identstr,
                               IdentData *ident, GmsIndicesData *gmsindices, double *val)
{
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


const InterpreterOps parser_ops_emb = {
   .type                  = InterpreterOpsEmb,
   .ccflib_new            = emb_mp_ccflib_new,
   .ccflib_finalize       = emb_mp_ccflib_finalize,
   .ctr_markequasflipped  = emb_ctr_markequasflipped,
   .gms_get_uelidx        = get_uelidx_via_gmd,
   .gms_get_uelstr        = get_uelstr_via_gmd,
   .identaslabels         = emb_identaslabels,
   .mp_addcons            = emb_mp_addcons,
   .mp_addvars            = emb_mp_addvars,
   .mp_addvipairs         = emb_mp_addvipairs,
   .mp_addzerofunc        = emb_mp_addzerofunc,
   .mp_finalize           = emb_mp_finalize,
   .mp_new                = emb_mp_new,
   .mp_setaschild         = emb_mp_setaschild,
   .mp_setobjvar          = emb_mp_setobjvar,
   .mp_setprobtype        = emb_mp_setprobtype,
   .nash_finalize          = emb_nash_finalize,
   .nash_new               = emb_nash_new,
   .nash_addmp             = emb_nash_addmp,
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
   .resolve_tokasident    = gmd_find_ident,
   .read_gms_symbol       = emb_read_gms_symbol,
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

   gmdHandle_t gmdcpy;
   if (!gmdCreate(&gmdcpy, msg, sizeof(msg))) {
      error("[embcode] ERROR: cannot create output GMD object: %s\n", msg);
      return Error_EMPRuntimeError;
   }

   interp->gmdcpy = gmdcpy;

   S_CHECK(copy_UELs(gmd, gmdcpy));

   S_CHECK(interp_create_buf(interp));

   return OK;
}

static NONNULL int embcode_empinfo_finalize(Interpreter *interp, const char *scrdir)
{
   int status = OK;
   if (!interp->gmdcpy) { return OK; }

   char *fname;
   MALLOC_(fname, char, strlen(scrdir) + MAX(strlen(EMBCODE_GMDOUT_FNAME), strlen("empinfo.dat")) + 1);

   strcpy(fname, scrdir);
   size_t scrdir_len = strlen(fname);

   strcat(fname, EMBCODE_GMDOUT_FNAME);

  /* ----------------------------------------------------------------------
   * Save the GMD
   * ---------------------------------------------------------------------- */

   gmdHandle_t gmd = interp->gmdcpy;
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
   memcpy(&fname[scrdir_len], "empinfo.dat", sizeof("empinfo.dat"));

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
   empinterp_init(&interp, NULL, fname);

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

   S_CHECK_EXIT(empinterp_process_statements(&interp, &p, toktype))

   toktype = parser_getcurtoktype(&interp);
   if (toktype != TOK_EOF) {
      status = parser_err(&interp,
                          "At the end of the file: unexpected superfluous token, "
                          "no further token were expected.");
      goto _exit;

   }
   

_exit:
   if (status != OK) {
      interp_showerr(&interp);
   } else {
      status = embcode_empinfo_finalize(&interp, scrdir);

   }

   empinterp_free(&interp);

   return status;

   return OK;
}

