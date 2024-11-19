#include <stdarg.h>

#include "compat.h"
#include "ctrdat_gams.h"
#include "empinfo.h"
#include "empinterp.h"
#include "empinterp_linkresolver.h"
#include "empinterp_priv.h"
#include "empinterp_vm_compiler.h"
#include "empparser_priv.h"
#include "gamsapi_utils.h"
#include "gdx_reader.h"
#include "gams_empinfofile_reader.h"
#include "mathprgm.h"
#include "mdl.h"
#include "mdl_gams.h"

#include "dctmcc.h"
#include "gmdcc.h"

NONNULL
static inline void parsedkwds_init(InterpParsedKwds *state)
{
   state->has_bilevel = false;
   state->has_dag_node = false;
   state->has_dualequ = false;
   state->has_equilibrium = false;
   state->has_implicit_Nash = false;
   state->has_single_mp = false;
   state->bilevel_in_progress = false;
}

static inline void emptok_init(struct emptok *tok, unsigned linenr)
{
   tok->type = TOK_UNSET;
   tok->linenr = linenr;
   tok->len = 0;
   tok->start = NULL;
   scratchint_init(&tok->iscratch);
   tok->dscratch.data = NULL;
   tok->dscratch.size = 0;

   // HACK make sure it makes sense
   //memset(tok->payload.label, 0, sizeof(tok->payload));
}

NONNULL
static void finalization_init(InterpFinalization *finalize)
{
   finalize->mp_owns_remaining_equs = MpId_NA;
   finalize->mp_owns_remaining_vars = MpId_NA;
}

void empinterp_init(Interpreter *interp, Model *mdl, const char *fname)
{
   interp->health = PARSER_OK;
   interp->peekisactive = false;
   interp->read_gms_symbol = true;
   interp->err_shown = false;
   interp->linenr = 1;
   interp->read = 0;
   interp->linestart = NULL;
   interp->linestart_old = NULL;
   interp->buf = NULL;
   // HACK UNUSED?
   interp->tmpstr = NULL;
   interp->empinfo_fname = fname;
   interp->tmpstrlen = 0;

   interp->mdl = mdl;
   if (mdl) {
      interp->dct = ((struct ctrdata_gams*)mdl->ctr.data)->dct;
      assert(interp->dct);
   } else {
      interp->dct = NULL;
   }

   interp->gmdcpy = NULL;
   interp->gmd = NULL;
   interp->gmddct = NULL;
   interp->gmd_fromgdx = false;
   interp->gmd_own = false;

   emptok_init(&interp->cur, 0);
   emptok_init(&interp->peek, 0);
   emptok_init(&interp->pre, 0);

   parsedkwds_init(&interp->state);
   interp->last_kw_info.type = TOK_UNSET;
   finalization_init(&interp->finalize);


   interp->ops = &interp_ops_imm;
   interp->compiler = empvm_compiler_init(interp);

   interp->regentry = NULL;
   interp->dag_root_label = NULL;
   dagregister_init(&interp->dagregister);

   linklabels2arcs_init(&interp->linklabels2arcs);
   linklabel2arc_init(&interp->linklabel2arc);
   dual_label_init(&interp->dual_label);
   dualslabel_arr_init(&interp->dualslabels);

   gdxreaders_init(&interp->gdx_readers);
   namedints_init(&interp->globals.sets);
   multisets_init(&interp->globals.multisets);
   aliases_init(&interp->globals.aliases);
   namedscalar_init(&interp->globals.scalars);
   namedvec_init(&interp->globals.vectors);
   namedints_init(&interp->globals.localsets);
   namedvec_init(&interp->globals.localvectors);


   interp->gms_sym_iterator.active = false;
   interp->gms_sym_iterator.compact = true;
   interp->gms_sym_iterator.loop_needed = false;

   gmsindices_init(&interp->gmsindices);
   assert(gmsindices_deactivate(&interp->gmsindices));

   interp->daguid_parent = EMPDAG_UID_NONE;
   interp->daguid_grandparent = EMPDAG_UID_NONE;
   interp->daguid_child = EMPDAG_UID_NONE;
}

NONNULL void empinterp_free(Interpreter *interp)
{
   if (interp->pre.type != TOK_UNSET) {
      tok_free(&interp->pre);
   }
   if (interp->cur.type != TOK_UNSET) {
      tok_free(&interp->cur);
   }
   if (interp->peek.type != TOK_UNSET) {
      tok_free(&interp->peek);
   }

   FREE(interp->buf);
   empvm_compiler_free(interp->compiler);

   for (unsigned i = 0, len = interp->gdx_readers.len; i < len; ++i) {
      gdx_reader_free(&interp->gdx_readers.list[i]);
   }
   gdxreaders_free(&interp->gdx_readers);

   if (interp->gmd && interp->gmd_own) {
      gmdHandle_t obj = interp->gmd;
      if (interp->gmd_fromgdx) {
         gmdCloseGDX(obj, 0);
      }
      gmdFree(&obj);
      interp->gmd = NULL;
   }

   if (interp->gmdcpy) {
      gmdHandle_t obj = interp->gmdcpy;
      gmdFree(&obj);
      interp->gmdcpy = NULL;
   }

   aliases_free(&interp->globals.aliases);
   namedints_freeall(&interp->globals.sets);
   multisets_free(&interp->globals.multisets);

   namedscalar_free(&interp->globals.scalars);
   namedints_free(&interp->globals.localsets);
   namedvec_freeall(&interp->globals.vectors);
   namedvec_freeall(&interp->globals.localvectors);

   dagregister_freeall(&interp->dagregister);

   if (interp->regentry) {
      errormsg("[empinterp] ERROR: while freeing the interpreter, a label entry "
               "wasn't consumed. Please report this bug.\n");
   }

   FREE(interp->regentry);
   interp->regentry = NULL;

   linklabel2arc_freeall(&interp->linklabel2arc);
   linklabels2arcs_freeall(&interp->linklabels2arcs);
   dual_label_freeall(&interp->dual_label);
   dualslabel_arr_freeall(&interp->dualslabels);

   assert(interp->health != PARSER_OK || !gmsindices_isactive(&interp->gmsindices));
}

int interp_create_buf(Interpreter *interp)
{
   assert(interp->empinfo_fname);
   int status = OK;

   const char *fname = interp->empinfo_fname;

   FILE *fptr = fopen(fname, "rb");
   if (!fptr) {
      error("[empinterp] ERROR: could not open empinfo file '%s'.\n", fname);
      return Error_FileOpenFailed;
   }

   SYS_CALL(fseek(fptr, 0L, SEEK_END));
   long size = ftell(fptr);
   if (size < 0) {
      perror("ftell");
      error("[empinterp] ERROR: ftell returned value %ld on file '%s'\n", size, fname);
      goto _exit;
   }

   SYS_CALL(fseek(fptr, 0L, SEEK_SET)); // rewind to start
 
   MALLOC_EXIT(interp->buf, char, (size_t)size+1);

   UNUSED size_t read = fread(interp->buf, sizeof(char), size, fptr);

   /* On windows, there might be a translation of the linefeed */
#if !defined(_WIN32)
   if (read < size) {
      error("[empinterp] Could not read file '%s'.\n", fname);
      status = Error_RuntimeError;
      goto _exit;
   }
#endif

   interp->buf[read] = '\0';
   interp->read = read;
   interp->linestart = interp->buf;

_exit:
   SYS_CALL(fclose(fptr));

   return status;
}

static int interp_loadgmdsets(Interpreter *interp)
{
   assert(interp->gmd); assert(interp->dct);

   gmdHandle_t gmd = interp->gmd;
   dctHandle_t dct = interp->dct;

   int dct_nuels = dctNUels(dct);
   int nsymbols;
   GMD_CHK(gmdInfo, gmd, GMD_NRSYMBOLS, &nsymbols, NULL, NULL);

   char setname[GMS_SSSIZE];

   trace_empinterp("[empinterp] Loading sets from GMD: ");

   bool has_set = false;

   for (int i = 0; i < nsymbols; ++i) {
      void *symptr;
      int symtype, symdim, nrecs;
      GMD_CHK(gmdGetSymbolByIndex, gmd, i+1, &symptr);
      GMD_CHK(gmdSymbolInfo, gmd, symptr, GMD_TYPE, &symtype, NULL, NULL);

      if (symtype != GMS_DT_SET) { continue; }

      GMD_CHK(gmdSymbolInfo, gmd, symptr, GMD_DIM, &symdim, NULL, NULL);

      if (symdim != 1) { continue; }

      GMD_CHK(gmdSymbolInfo, gmd, symptr, GMD_NRRECORDS, &nrecs, NULL, NULL);
      assert(nrecs > 0);

      IntArray set;
      rhp_int_init(&set);
      S_CHECK(rhp_int_reserve(&set, nrecs))

      void *symiterptr = NULL;
      GMD_CHK(gmdFindFirstRecord, gmd, symptr, &symiterptr);

      bool has_next;
      do {
         char uelstr[GLOBAL_UEL_IDENT_SIZE];
         GMD_CHK(gmdGetKey, gmd, symiterptr, 0, uelstr)
         int uelidx = dctUelIndex(dct, uelstr);

         if (uelidx < 0) {
            error("[empinterp] ERROR: cound't find UEL '%s' in DCT\n", uelstr);
            gmdFreeSymbolIterator(gmd, symiterptr);
            return Error_GamsCallFailed;
         }

         if (uelidx > dct_nuels) {
            error("[empinterp] ERROR: UEL '%s' has index %d in DCT, not in [1,%d]",
                  uelstr, uelidx, dct_nuels);
            return Error_RuntimeError;
         }

         rhp_int_add(&set, uelidx);
         has_next = gmdRecordHasNext(gmd, symiterptr);
         if (has_next) { gmdRecordMoveNext(gmd, symiterptr); }
      } while (has_next);

      GMD_CHK(gmdSymbolInfo, gmd, symptr, GMD_NAME, NULL, NULL, setname);
      S_CHECK(namedints_add(&interp->globals.sets, set, strdup(setname)));

      trace_empinterp("%s ", setname);
      has_set = true;

      if (symiterptr) {
         GMD_CHK(gmdFreeSymbolIterator, gmd, symiterptr);
      }
   }

   if (!has_set) {
      trace_empinterp("none\n");
   } else {
      trace_empinterp("\n");
   }


   return OK;
}

static int interp_loadgmdparams(Interpreter *interp)
{
   assert(interp->gmd); assert(interp->dct);

   gmdHandle_t gmd = interp->gmd;
   dctHandle_t dct = interp->dct;
   int nsymbols;
   GMD_CHK(gmdInfo, gmd, GMD_NRSYMBOLS, &nsymbols, NULL, NULL);

   Lequ v;
   char param_name[GMS_SSSIZE];

   trace_empinterp("[empinterp] Loading parameters from GMD: ");

   bool has_param = false;

   for (int i = 0; i < nsymbols; ++i) {
      void *symptr;
      int symtype, symdim, nrecs;
      GMD_CHK(gmdGetSymbolByIndex, gmd, i+1, &symptr);
      GMD_CHK(gmdSymbolInfo, gmd, symptr, GMD_TYPE, &symtype, NULL, NULL);

      if (symtype != GMS_DT_PAR) { continue; }

      GMD_CHK(gmdSymbolInfo, gmd, symptr, GMD_DIM, &symdim, NULL, NULL);

      if (symdim > 1) { continue; }

      GMD_CHK(gmdSymbolInfo, gmd, symptr, GMD_NAME, NULL, NULL, param_name);

      void *symiterptr = NULL;
      GMD_CHK(gmdFindFirstRecord, gmd, symptr, &symiterptr);

      if (symdim == 0) {
         double val;
         GMD_CHK(gmdGetLevel,gmd, symiterptr, &val);
         S_CHECK(namedscalar_add(&interp->globals.scalars, val, strdup(param_name)));
         goto _end_loop;
      }

      /* We have a vector */
      GMD_CHK(gmdSymbolInfo, gmd, symptr, GMD_NRRECORDS, &nrecs, NULL, NULL);
      assert(nrecs > 0);

      lequ_init(&v);
      S_CHECK(lequ_reserve(&v, nrecs))

      bool has_next;
      do {
         char uel[GLOBAL_UEL_IDENT_SIZE];
         GMD_CHK(gmdGetKey, gmd, symiterptr, 0, uel)
         int uelidx = dctUelIndex(dct, uel);
         if (uelidx < 0) {
            error("[empinterp] ERROR: cound't find UEL '%s' in DCT\n", uel);
            return Error_GamsCallFailed;
         }

         double val;
         GMD_CHK(gmdGetLevel,gmd, symiterptr, &val);
         lequ_add(&v, uelidx, val);

         has_next = gmdRecordHasNext(gmd, symiterptr);
         if (has_next) { gmdRecordMoveNext(gmd, symiterptr); }
      } while (has_next);


      S_CHECK(namedvec_add(&interp->globals.vectors, v, strdup(param_name)));

_end_loop:
      trace_empinterp("%s ", param_name);
      has_param = true;

      if (symiterptr) {
         gmdFreeSymbolIterator(gmd, symiterptr);
      }

   }

   if (!has_param) {
      trace_empinterp("none\n");
   } else {
      trace_empinterp("\n");
   }


   return OK;
}
int empinterp_process(Model *mdl, const char *empinfo_fname, const char *gmd_fname) 
{
   trace_empinterp("[empinterp] Processing file '%s'\n", empinfo_fname);

   int status = OK;
   char *gdxfullname = NULL;

   Interpreter interp;
   empinterp_init(&interp, mdl, empinfo_fname);

   /* ---------------------------------------------------------------------
    * Read the file content in memory. If there is a GMD filename, load it
    * --------------------------------------------------------------------- */

   S_CHECK_EXIT(interp_create_buf(&interp));

   if (gmd_fname) {
      char msg[GMS_SSSIZE];
      gmdHandle_t gmd;

      if (!gmdCreate(&gmd, msg, sizeof(msg))) {
         error("[empinterp] ERROR: cannot create GMD object: %s\n", msg);
         return Error_EMPRuntimeError;
      }

      GMD_CHK(gmdSelectRecordStorage, gmd, NULL, 2);
      GMD_CHK(gmdInitFromGDX, gmd, gmd_fname);

      if (O_Output & PO_TRACE_EMPINTERP) {
         int nuels, nsymbols;
         GMD_CHK(gmdInfo, gmd, GMD_NRUELS, &nuels, NULL, NULL);
         GMD_CHK(gmdInfo, gmd, GMD_NRSYMBOLS, &nsymbols, NULL, NULL);

         trace_empinterp("[empinterp] Loaded GMD from file '%s': %d UELs, %d symbols\n",
                         gmd_fname, nuels, nsymbols);
      }

      interp.gmd = gmd;
      interp.gmd_fromgdx = true;
      interp.gmd_own = true;

      S_CHECK_EXIT(interp_loadgmdsets(&interp));
      S_CHECK_EXIT(interp_loadgmdparams(&interp));
   }

#ifdef USE_GMD_EQUVAR 
   if (interp.mdl && interp.dct) {
      GmsContainerData *gms = (GmsContainerData *)mdl->ctr.data;
      dctHandle_t dct = gms->dct; assert(dct);
      char msg[GMS_SSSIZE];

      GmsModelData *gmdldat = (GmsModelData *)mdl->data;
      const char *scrdir = gmdldat->scrdir;
      const char *gdxname = "dctgdx.dat"; 
      IO_CALL_EXIT(asprintf(&gdxfullname, "%s%s", scrdir, gdxname));

      // this returns void ...
      dctWriteGDX(dct, gdxfullname, msg);

      gmdHandle_t gmddct;
      if (!gmdCreate(&gmddct, msg, sizeof(msg))) {
         error("[empinterp] ERROR: cannot create GMD object: %s\n", msg);
         status = Error_EMPRuntimeError;
         goto _exit;
      }

      // 2 if for a tree storage
      GMD_CHK(gmdSelectRecordStorage, gmddct, NULL, 2);
      GMD_CHK(gmdInitFromGDX, gmddct, gdxfullname);
      interp.gmddct = gmddct;

   }
#endif

   /* TODO: weird place and too specific */
   empvm_compiler_setgmd(&interp);

   /* ---------------------------------------------------------------------
    * Phase I: parse for keywords and if there is a DAG, create the nodes
    *
    * Phase II: If there is a DAG, finalize the EMPDAG (add edges)
    * --------------------------------------------------------------------- */

   unsigned p = 0;
   TokenType toktype;

   bool eof = skip_spaces_commented_lines(&interp, &p);
   if (eof) {
      printout(PO_INFO, "[empinterp] Empinfo file %s has no statement\n", empinfo_fname);

      goto _exit;
   }

   S_CHECK_EXIT(advance(&interp, &p, &toktype));
   toktype = parser_getcurtoktype(&interp);

   if (toktype == TOK_ERROR || toktype == TOK_UNSET) {
      error("[empinterp] ERROR: first token has type %s\n", toktype2str(toktype));
      status = Error_EMPRuntimeError;
      goto _exit;
   }

   mdl_settype(mdl, MdlType_emp);
   mdl->empinfo.empdag.has_resolved_arcs = false; /* disable check in mp_finalize */

   /* TODO(xhub) do we want to continue or error if we have an error */

   S_CHECK_EXIT(empinterp_process_statements(&interp, &p, toktype))

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
      status = empinterp_finalize(&interp);

      EmpDag *empdag = &mdl->empinfo.empdag;

      if (status == OK) {
         unsigned mp_len = empdag->mps.len;

         if (mp_len > 0) {
            status = mdl_checkmetadata(mdl); 
         } else {
            status = gmdl_ensuresimpleprob(mdl);
         }

         /* TODO: if mp_len is 1, we might have a trivial/simple model */
         //if (mp_len == 1) {
         //   status = mdl_reset_modeltype(mdl, NULL);
         //}
      }

      if (status == OK) {
         status = empdag_fini(empdag);
      }

   }

   empinterp_free(&interp);
   free(gdxfullname);

   return status;
}


int resolve_identas_(Interpreter * restrict interp, IdentData *ident,
                            const char *msg, unsigned argc, ...)
{
   int status = OK;
   va_list ap, ap_copy;

   va_start(ap, argc);
   va_copy(ap_copy, ap);

   S_CHECK_EXIT(interp->ops->resolve_tokasident(interp, ident));

   IdentType type = ident->type;
   for (unsigned i = 0; i < argc; ++i) {
      IdentType type_ = (IdentType)va_arg(ap, unsigned);
      if (type_ == type) {
         goto _exit;
      }

   }

   if (type == IdentNotFound) {
      Token *tok = interp->peekisactive ? &interp->peek : &interp->cur;
      error("[empinterp] ERROR line %u: ident '%.*s' is unknown\n",
            tok->linenr, emptok_getstrlen(tok), emptok_getstrstart(tok));
      status = Error_EMPIncorrectInput;
      goto _exit;
   }

   status = Error_EMPIncorrectSyntax;
   error("[empinterp] ERROR line %u: ident '%.*s' has type '%s', but expected any of ",
         interp->linenr, emptok_getstrlen(&interp->cur),
         emptok_getstrstart(&interp->cur), identtype2str(type));

   bool first = true;
   for (size_t i = 0; i < argc; ++i) {
      IdentType type_ = (IdentType)va_arg(ap_copy, unsigned);
      if (!first) {
         printstr(PO_ERROR, ", ");
      } else {
         first = false;
      }

      error("'%s'", identtype2str(type_));
   }

   error(".\n[empinterp] error msg is: %s\n", msg);

_exit:
   //assert(data->idx != UINT_MAX);

   va_end(ap);
   va_end(ap_copy);

   return status;
}

int empinterp_finalize(Interpreter *interp)
{
  /* ----------------------------------------------------------------------
   * Finalization steps:
   * 1) If there is still a VM to execute, we do this here
   * 2) EMPDAG finalization: resolve all labels in the register
   * 3) Container finalization: If an MP has been tagged as owning unassigned
   *    equs/vars, then assign all unclaimed equs/vars to it.
   * ---------------------------------------------------------------------- */

 
   /* WARNING: this also executes the VM and must be called before resolving the
    * DagRegister */
   if (interp->compiler) {
      S_CHECK(empvm_finalize(interp));
   }

  /* ----------------------------------------------------------------------
   * Step2: EMPDAG finalization: resolve all labels in the register
   * ---------------------------------------------------------------------- */

   if (interp->dagregister.len > 0) {
      trace_empinterp("[empinterp] Resolving edges for %u nodes in the EMPDAG.\n",
                      interp->dagregister.len);

      S_CHECK(empinterp_resolve_labels(interp));
   }

   S_CHECK(empinterp_set_empdag_root(interp));

   interp->mdl->empinfo.empdag.has_resolved_arcs = true;

  /* ----------------------------------------------------------------------
   * Step 3: Finalize the container
   * - finalize all the OVF (in empinfo->ovfinfo)
   * - if a default MP exists for equ/var, perform the assignement
   * ---------------------------------------------------------------------- */

   Model *mdl = interp->mdl;
   OvfInfo *ovfinfo = mdl->empinfo.ovf;
   OvfDef *ovfdef = ovfinfo ? ovfinfo->ovf_def : NULL;

   while (ovfdef) {
      S_CHECK(ovf_finalize(mdl, ovfdef));
      ovfdef = ovfdef->next;
   }

   mpid_t mp_equs = interp->finalize.mp_owns_remaining_equs;
   mpid_t mp_vars = interp->finalize.mp_owns_remaining_vars;

   if (mpid_regularmp(mp_equs)) {
      /* All unassigned equations now belong to mp_equs */
      DagMpArray *mplist = &interp->mdl->empinfo.empdag.mps;

      if (mp_equs >= mplist->len) {
         error("[empinterp] ERROR in finalization: MP id for remaining equations "
               "has value %u, outside of [0,%u)]", mp_equs,  mplist->len);
         return Error_EMPRuntimeError;
      }

      MathPrgm *mp = mplist->arr[mp_equs];

      EquMeta * restrict emeta = interp->mdl->ctr.equmeta; assert(emeta);

      for (unsigned i = 0, len = interp->mdl->ctr.m; i < len; ++i) {
         if (!valid_mpid(emeta[i].mp_id)) {
            S_CHECK(mp_addconstraint(mp, i));
         }
      }
   }

   if (mpid_regularmp(mp_vars)) {
      /* All unassigned variables now belong to mp_vars */
      DagMpArray *mplist = &interp->mdl->empinfo.empdag.mps;

      if (mp_vars >= mplist->len) {
         error("[empinterp] ERROR in finalization: MP id for remaining variables "
               "has value %u, outside of [0,%u)]", mp_vars,  mplist->len);
         return Error_EMPRuntimeError;
      }

      MathPrgm *mp = mplist->arr[mp_vars];

      VarMeta * restrict vmeta = interp->mdl->ctr.varmeta; assert(vmeta);

      for (unsigned i = 0, len = interp->mdl->ctr.n; i < len; ++i) {
         if (!valid_mpid(vmeta[i].mp_id)) {
            S_CHECK(mp_addvar(mp, i));
         }
      }
   }

   if (mpid_regularmp(mp_equs)) {
      S_CHECK(mp_finalize(interp->mdl->empinfo.empdag.mps.arr[mp_equs]));
   }

   if (mpid_regularmp(mp_vars) && mp_equs != mp_vars) {
      S_CHECK(mp_finalize(interp->mdl->empinfo.empdag.mps.arr[mp_vars]));
   }

   const EmpDag *empdag = &interp->mdl->empinfo.empdag;
   MathPrgm **mparr = empdag->mps.arr;

   for (unsigned i = 0, len = interp->mdl->empinfo.empdag.mps.len; i < len; ++i) {
      MathPrgm *mp = mparr[i];
      if (mp->status & MpFinalized) { continue; }

      if (!mp_isopt(mp)) {
         error("[empinterp] ERROR: MP(%s) has not been finalized and is of type %s\n",
               empdag_getmpname(empdag, mp->id), mp_gettypestr(mp));
      }
      assert(!valid_ei(mp->opt.objequ) && !valid_vi(mp->opt.objvar));
      S_CHECK(mp_finalize(mp));
   }

   return OK;
}
