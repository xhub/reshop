#include "asprintf.h"

#include <assert.h>
#include <ctype.h>
#include <stdlib.h>
#include <string.h>

#include "container.h"
#include "ctr_gams.h"
#include "ctrdat_gams.h"
#include "equ.h"
#include "equvar_helpers.h"
#include "equvar_metadata.h"
#include "gams_exportempinfo.h"
#include "gams_macros.h"
#include "gams_rosetta.h"
#include "gams_solve.h"
#include "gams_utils.h"
#include "macros.h"
#include "mathprgm.h"
#include "mdl.h"
#include "mdl_gams.h"
#include "mdl_ops.h"
#include "mdl_priv.h"
#include "mdl_rhp.h"
#include "mdl_transform.h"
#include "reshop_error_handling.h"
#include "rhp_options.h"
#include "rmdl_options.h"
#include "timings.h"
#include "toplayer_utils.h"
#include "var.h"
#include "printout.h"

#include "gevmcc.h"
#include "gmomcc.h"

/* ---------------------------------------------------------------------
 * Kludge for JAMS and equilibrium
 * --------------------------------------------------------------------- */

static int kludge_jams_equilibrium(Container *ctr, GmsModelData * restrict mdldat,
                                   GmsContainerData * restrict gms)
{
   char buf[GMS_SSSIZE];
   GmsContainerData gmstmp = {.owndct = false};
   GmsModelData mdldatatmp;

   gevGetStrOpt(gms->gev, gevNameScrDir, buf);
   SNPRINTF_CHECK(mdldatatmp.gamscntr, GMS_SSSIZE, "%s" DIRSEP "empcntr.dat", buf);
   STRNCPY_FIXED(mdldatatmp.gamsdir, mdldat->gamsdir);

   S_CHECK(gcdat_loadmdl(&gmstmp, &mdldatatmp));

   gevGetCurrentSolver(gmstmp.gev, gmstmp.gmo, buf);

   if (!strcasecmp(buf, "path") && !strcasecmp(buf, "pathvi")) {
      gcdat_rel(&gmstmp);
      return OK;
   }

   if (!gmoDictionary(gmstmp.gmo)) {
      trace_solreport("[GAMS] WARNING: no dictionary in the GAMS EMP control file. "
                      "Values can't be reported and some may be missing\n");
      gcdat_rel(&gmstmp);
      return OK;
   }

   /* -------------------------------------------------------------------
       * Load the values "legacy mode" from one of the files on disk
       *
       * Then copy the values
       * ------------------------------------------------------------------- */

   trace_solreport("[GAMS] due to JAMS, loading values in legacy mode from '%s'\n",
                   mdldatatmp.gamscntr);

   gmoIndexBaseSet(gmstmp.gmo, 0);
   gmoLoadSolutionLegacy(gmstmp.gmo);

   for (int i = 0, max = gmoN(gmstmp.gmo); i < max; i++) {
      gmoGetVarNameOne(gmstmp.gmo, i, buf);
      if (buf[0] == 'x') {
         rhp_idx vidx = atoi(&buf[1]) - 1;
         assert(vidx < ctr->n);
         double val = gmoGetVarLOne(gmstmp.gmo, i);
         gmoSetVarLOne(gms->gmo, vidx, val);
         trace_solreport("[solreport] variable '%s' set to %e\n",
                         ctr_printvarname(ctr, vidx), val);
         gmoSetVarMOne(gms->gmo, vidx, gmoGetVarMOne(gmstmp.gmo, i));
         gmoSetVarStatOne(gms->gmo, vidx, gmoGetVarStatOne(gmstmp.gmo, i));
      }
   }

   /* ----------------------------------------------------------------------
       * the GMO API doesn't have the gmoGetEquMOne or gmoGetEquStatOne
       * ---------------------------------------------------------------------- */
   struct ctrmem CTRMEM working_mem = {.ptr = NULL, .ctr = ctr};
   A_CHECK(working_mem.ptr, ctr_getmem(ctr, (MAX(ctr->m, 1))*(sizeof(double)+sizeof(int))));
   double *marginals = (double*)working_mem.ptr;
   int *bstats = (int*)&marginals[ctr->m];


   for (size_t k = 0; k < ctr->m; ++k) {
      marginals[k] = GMS_SV_UNDEF;
      bstats[k] = gmoBstat_Super;
   }

   for (int i = 0, max = gmoM(gmstmp.gmo); i < max; i++) {
      gmoGetEquNameOne(gmstmp.gmo, i, buf);
      if (buf[0] == 'e') {
         rhp_idx eidx = atoi(&buf[1]) - 1;
         assert(eidx < ctr->m);
         gmoSetEquLOne(gms->gmo, eidx, gmoGetEquLOne(gmstmp.gmo, i));
         marginals[eidx] = gmoGetEquMOne(gmstmp.gmo, i);
         bstats[eidx] = gmoGetEquStatOne(gmstmp.gmo, i);
      }
   }

   gmoSetEquM(gms->gmo, marginals);
   gmoSetEquStat(gms->gmo, bstats);


   /* -------------------------------------------------------------------
       * Close the GMO
       * ------------------------------------------------------------------- */

   gcdat_rel(&gmstmp);
   return OK;
}

static int gams_getobjjacval(const Model *mdl, double *objjacval);
static int gams_getobjvar(const Model *mdl, int *objvar);
static int gams_getmodelstat(const Model *mdl, int *modelstat);
static int gams_getsolvestat(const Model *mdl, int *solvestat);
static int gams_export(Model *mdl, Model *mdl_dst);

static int gams_allocdata(Model *mdl)
{
   GmsModelData *mdldat;
   CALLOC_(mdldat, GmsModelData, 1);

   mdl->data = mdldat;

   const char *gamsdir = gams_getgamsdir();
   const char *gamscntr = gams_getgamscntr();

   if (gamsdir) {
     printout(PO_STACK, "[GAMS] gamsdir set to global value '%s'.\n", gamsdir);
     STRNCPY_FIXED(mdldat->gamsdir, gamsdir);
   }

   if (gamscntr) {
     printout(PO_STACK, "[GAMS] gamscntr set to global value '%s'.\n", gamscntr);
     STRNCPY_FIXED(mdldat->gamscntr, gamscntr);
   }

   mdldat->last_solverid = -1;
   mdldat->delete_scratch = true;
   mdldat->slvptr = NULL;

   return OK;
}

static void gams_deallocdata(Model *mdl)
{
   GmsModelData *mdldat = mdl->data;
   GmsContainerData *gms = mdl->ctr.data;

   if (!gms || !gms->initialized) {
      FREE(gms);
      mdl->ctr.data = NULL;
      FREE(mdl->data);
      return;
   }

   int keep = gevGetIntOpt(gms->gev, gevKeep);
   if (!keep) {
      char buffer[GMS_SSSIZE];
      gevGetStrOpt(gms->gev, gevNameScrDir, buffer);
      /*  \TODO(xhub) check that we are not trying to remove the current directory ...*/
      if (mdldat->delete_scratch && buffer[0] != '\0') {
         printout(PO_INFO, "%s :: scr directory is empty!\n", __func__);
         mdldat->delete_scratch = false;
      }
      if (mdldat->delete_scratch) {

         int rc = rmfn(buffer);
         if (rc) {
            error("%s :: scrdir %s was not deleted\n", __func__, buffer);
         }

         mdldat->delete_scratch = false;
      }
   }

   if (mdldat->last_solverid != -1 && mdldat->slvptr
       && cfgAlgAllowsModifyProblem(gms->cfg, mdldat->last_solverid)) {
      cfgAlgFree(gms->cfg, mdldat->last_solverid, &mdldat->slvptr);
   }

   mdl->ctr.ops->deallocdata(&mdl->ctr);

   FREE(mdl->data);
}

static int gams_getmodelstat(const Model *mdl, int *modelstat)
{
   int stat;

   stat = gmoModelStat(((const GmsContainerData *) mdl->ctr.data)->gmo);
   (*modelstat) = stat;

   return OK;
}

/**
 * @brief Solve the model (stored in GMO) using GAMS gev interface
 *
 * @param mdl  the GAMS model
 *
 * @return     the error code
 */
static int gams_solve(Model *mdl)
{
   double start = get_walltime();
   char buf[GMS_SSSIZE], optname[GMS_SSSIZE+3];
   int solverid, solverlog, rc;

   Container *ctr = &mdl->ctr;
   GmsModelData *mdldat = mdl->data;
   GmsContainerData *gms = ctr->data;

   if (!gms->gmo) {
      error("%s ERROR in %s model '%.*s' #%u: GMO object is NULL \n", __func__,
            mdl_fmtargs(mdl));
      return Error_NullPointer;
   }
   if (!gms->gev) {
      error("%s ERROR in %s model '%.*s' #%u: GEV object is NULL \n", __func__,
            mdl_fmtargs(mdl));
      return Error_NullPointer;
   }

   assert(gmoN(gms->gmo) == ctr->n);
   assert(gmoM(gms->gmo) == ctr->m);

   /* ---------------------------------------------------------------------
    * If required, use convert to print the model
    * --------------------------------------------------------------------- */

   if (optvalb(mdl, Options_Dump_Scalar_Models) &&
         strncasecmp(mdldat->solvername, "convert", strlen("convert")) != 0) {

      gevGetStrOpt(gms->gev, gevNameScrDir, optname);
      size_t len_scrdir = strlen(optname);

      /* ------------------------------------------------------------------
       * Perform the necessary work to use convert:
       *   - backup the existing solvername
       *   - set the solver name
       *   - set the option file
       * ------------------------------------------------------------------ */

      if (mdldat->solvername[0] != '\0') {
         strncpy(buf, mdldat->solvername, GMS_SSSIZE-1);
         buf[GMS_SSSIZE-1] = '\0';

      } else {
         buf[0] = '\0';
      }

      STRNCPY_FIXED(mdldat->solvername, "convert");
      strncat(optname, DIRSEP"convert.opt", sizeof(optname)-len_scrdir-1);

      FILE *opt = fopen(optname, "w");
      if (!opt) {
         error("%s ERROR: could not create file %s\n", __func__, optname);
         perror("fopen");
         return Error_NullPointer;
      }

      gmoNameOptFileSet(gms->gmo, optname);

      /* Reset optname to the directory name */
      optname[len_scrdir] = '\0';

      size_t mdlname_len = mdl_getnamelen(mdl);
      assert(mdlname_len > 0);

      struct ctrmem CTRMEM working_mem = {.ptr = NULL, .ctr = ctr};
      A_CHECK(working_mem.ptr, ctr_getmem(ctr, (1+mdlname_len) * sizeof(char)));
      char *mdlname = working_mem.ptr;
      memcpy(mdlname, mdl_getname(mdl), mdlname_len);
      mdlname[mdlname_len] = '\0';

      char * restrict name = mdlname;
      while (*name != '\0') {
         char c = *name;
         if (!(c >= 'a' && c <= 'z') &&
             !(c >= 'A' && c <= 'Z') &&
             !(c >= '0' && c <= '9') &&
             !(c == '_')) {

            *name = '_';
         }

         name++;
      }

      IO_CALL(fprintf(opt, "gams %s" DIRSEP "%s-%u.gms\n", optname,
                      mdlname, mdl->id));
      IO_CALL(fprintf(opt, "Dict=%s" DIRSEP "dict-%s-%u.txt", optname,
                      mdlname, mdl->id));
      IO_CALL(fclose(opt));

      int old_O_Subsolveropt = O_Subsolveropt;
      O_Subsolveropt = -1;
      gmoOptFileSet(gms->gmo, 1);

      /* ------------------------------------------------------------------
       * Create the convert export
       * ------------------------------------------------------------------ */

      S_CHECK(gams_solve(mdl));

#if defined(__linux__) || defined(__APPLE__)

#if defined(__GNUC__)
#pragma GCC diagnostic push
#pragma GCC diagnostic ignored "-Wformat"
#endif

     char *cmd;
      int ret = asprintf(&cmd, "sed -n -e 'y/(),-/____/' -e 's:^ *\\([exbi][0-9][0-9]*\\) \\(.*\\):s/\\1/\\2/g:gp' "
                         "'%s" DIRSEP "dict-%s-%u.txt' | sed -n '1!G;h;$p' > '%s" DIRSEP "%s-%u.sed'",
                         optname, mdlname, mdl->id, optname, mdlname, mdl->id);

      if (ret == -1) { goto skip; }
      ret = system(cmd);
      FREE(cmd);

      ret = asprintf(&cmd, "sed -f '%1$s" DIRSEP "%2$s-%3$u.sed' '%1$s" DIRSEP "%2$s-%3$u.gms' > '%1$s" DIRSEP "%2$s-%3$u-named.gms'",
                     optname, mdlname, mdl->id);

      if (ret == -1) { goto skip; }
      ret = system(cmd);
      FREE(cmd);
#if defined(__GNUC__)
#pragma GCC diagnostic pop
#endif
skip:
#endif
      /* ------------------------------------------------------------------
       * Restore the original solver name
       * ------------------------------------------------------------------ */

      if (buf[0] != '\0') {
         STRNCPY_FIXED(mdldat->solvername, buf);
      } else {
         mdldat->solvername[0] = '\0';
      }

      optname[0] = '\0';

      O_Subsolveropt = old_O_Subsolveropt;
      
   }

   /* ---------------------------------------------------------------------
    * If the solver name is not chosen yet, set the solver name.
    * --------------------------------------------------------------------- */

   if (mdldat->solvername[0] == '\0') {

      /* ------------------------------------------------------------------
       * We currently support LP, MI(NL)P, (D)NLP, QCP, and MCP.
       * ------------------------------------------------------------------ */

      ModelType probtype;
      S_CHECK(mdl_gettype(mdl, &probtype))

      enum gmoProcType gams_probtype = mdltype_to_gams(probtype);

      gevGetSolver(gms->gev, gams_probtype, mdldat->solvername);

      /* -------------------------------------------------------------------
       * This code here is to deal with the case where the user set the
       * solver to be ReSHOP (like for lp, nlp). To avoid looping, break the
       * loop by setting the default solver
       *
       * TODO(Xhub) set an option for that
       * ------------------------------------------------------------------- */

      if (0 == strncasecmp(mdldat->solvername, "reshop", strlen("reshop"))) {
         gevGetSolverDefault(gms->gev, gams_probtype, mdldat->solvername);
         trace_stack("[model] %s model '%.*s' #%u: setting solver to %s",
                     mdl_fmtargs(mdl), mdldat->solvername);
      }

      for (unsigned i = 0; mdldat->solvername[i] != '\0'; i++) {
         mdldat->solvername[i] = tolower(mdldat->solvername[i]);
      }
   }

   /* If needed, create empinfo.txt for GAMS */
   if (!strncasecmp("JAMS", mdldat->solvername, strlen("JAMS"))) {
      S_CHECK(gms_exportempinfo(mdl));
   }

   solverid = cfgAlgNumber(gms->cfg, mdldat->solvername);

   if (solverid <= 0) {
      error("%s :: ERROR: could not find solver named '%s'\n", __func__,
              mdldat->solvername);
      return Error_SolverInvalidName;
   }

   mdldat->last_solverid = solverid;
   /* This reset the interrupt handler */
   gevTerminateClear(gms->gev);

   {

      /* ------------------------------------------------------------------
       * Set the subsolver's logging mode.
       *  - gevSolverSameStreams: output into the same stream of RESHOP.
       *  - gevSolverQuiet      : no output
       * ------------------------------------------------------------------ */

      const char *subsolver_log = mygetenv("RHP_OUTPUT_SUBSOLVER_LOG");
      if (O_Output_Subsolver_Log || subsolver_log) {
         solverlog = gevSolverSameStreams;
      } else {
         solverlog = gevSolverQuiet;
      }
      myfreeenvval(subsolver_log);

      /* ------------------------------------------------------------------
       * Set the subsolver's option file name and its number.
       * If O_Subsolveropt is 0, then disable the option.
       * ------------------------------------------------------------------ */

      if (O_Subsolveropt > 0) {
         gevGetStrOpt(gms->gev, gevNameWrkDir, buf);

         if (O_Subsolveropt == 1) {
            SNPRINTF_CHECK(optname, GMS_SSSIZE, "%s.opt", mdldat->solvername);
         } else {
            SNPRINTF_CHECK(optname, GMS_SSSIZE, "%s.op%d", mdldat->solvername, O_Subsolveropt);
         }

         strcat(buf, optname);
         gmoNameOptFileSet(gms->gmo, buf);
         gmoOptFileSet(gms->gmo, O_Subsolveropt);

      } else if (O_Subsolveropt == -1) {
        /*  Do nothing */
      } else {
         gmoOptFileSet(gms->gmo, 0);
      }

      trace_stack("[GAMS] Solving %s model '%.*s' #%u with solver=\"%s\", "
                  "gamsdir=\"%s\", gamscntr=\"%s\"\n", mdl_fmtargs(mdl),
                  mdldat->solvername, mdldat->gamsdir, mdldat->gamscntr);
      int status_jmp = RESHOP_SETJMP_INTERNAL_START;
      if (status_jmp == OK) {
         rc = gevCallSolver(gms->gev,
                            gms->gmo,
                            "",                      /* name of control file   */
                            mdldat->solvername,         /* name of solver         */
//                            gevSolveLinkCallModule, /* solvelink option       */
                            gevSolveLinkLoadLibrary, /*  solvelink option       */
                            solverlog,               /* log option             */
                            mdldat->logname,            /* log file name          */
                            mdldat->statusname,         /* status file name       */
                            GMS_SV_NA,               /* resource limit         */
                            GMS_SV_NAINT,            /* iteration limit        */
                            GMS_SV_NAINT,            /* domain violation limit */
                            GMS_SV_NA,               /* optcr                  */
                            GMS_SV_NA,               /* optca                  */
                            NULL,                    /* handle to solver job   */
                            buf);                    /* message                */

         if (rc) {
            error("[GAMS] ERROR: gevCallSolver() failed with " "message: %s\n",
                  buf);
            return Error_GamsSolverCallFailed;
         }

         RESHOP_SETJMP_INTERNAL_STOP;
      } else {
         error("[GAMS] ERROR: Call to GAMS solver %s via gevCallSolver() failed with a "
               "major error! Some details were (hopefully) printed above.\n",
               mdldat->solvername);

         /*  TODO(xhub) HIGH clean up this! */
#if (defined(__linux__) || defined(__APPLE__))
#include <execinfo.h>
         errormsg("Providing another backtrace:\n");
         void *array[90];
         int size = ARRAY_SIZE(array);

         size = backtrace(array, size);

         char **bck_str = backtrace_symbols(array, size);

         if (bck_str) {
            for (unsigned i = 0; i < size; ++i) {
               errormsg(bck_str[i]);
               errormsg("\n");
            }
            FREE(bck_str);
         } else {
            errormsg("Call to backtrace_symbols failed!");
         }
#endif

         return status_jmp;
      }
   }

   trace_process("[GAMS] %s model '%.*s' #%u solved with statuses model=%s; solve=%s\n",
                 mdl_fmtargs(mdl), modelStatusTxt[gmoModelStat(gms->gmo)],
                 solveStatusTxt[gmoSolveStat(gms->gmo)]);

   if (!strcasecmp(mdldat->solvername, "jams")) {
      S_CHECK(kludge_jams_equilibrium(ctr, mdldat, gms));
   }

   // TODO: review this.
   /* ---------------------------------------------------------------------
    * We now have the basis information. Thus we set it to true.
    * --------------------------------------------------------------------- */

   gmoHaveBasisSet(gms->gmo, 1);

   /* ---------------------------------------------------------------------
    * Disable the options we set before.
    * --------------------------------------------------------------------- */

   gmoAltBoundsSet(gms->gmo, 0);
   gmoAltRHSSet(gms->gmo, 0);
   gmoForceLinearSet(gms->gmo, 0);

   mdl->timings->solve.solver_wall += get_walltime() - start;

   return OK;
}

static int gams_checkmdl(Model *mdl)
{

   if (!mdl->ctr.vars) {
      return OK;
   }

   for (size_t i = 0; i < mdl->ctr.n; ++i) {
      Var *v = &mdl->ctr.vars[i];

      /* -------------------------------------------------------------------
       * If the variable is fixed, then set the value to the right one
       * ------------------------------------------------------------------- */
      if (var_isfixed(v)) {
         v->value = v->bnd.lb;
      }
   }

   S_CHECK(empdag_fini(&mdl->empinfo.empdag));

   return OK;
}

static int gams_checkobjequvar(const Model *mdl, rhp_idx objvar, rhp_idx objequ)
{
   return OK;
}

static int gams_copyassolvable_no_transform(Model *mdl, Model *mdl_src)
{
   BackendType backend = mdl_src->backend;

   switch (backend) {
   case RHP_BACKEND_GAMS_GMO:
      return gams_export(mdl_src, mdl);
   case RHP_BACKEND_JULIA:
   case RHP_BACKEND_RHP:
      return mdl_export(mdl_src, mdl);
   default:
      return backend_throw_notimplemented_error(backend, __func__);
   }
}

static int gams_copyassolvable(Model *mdl, Model *mdl_src)
{
   ModelType probtype;
   S_CHECK(mdl_gettype(mdl_src, &probtype));

   switch (probtype) {
   case MdlType_lp:
   case MdlType_nlp:
   case MdlType_qcp:
   case MdlType_mip:
   case MdlType_minlp:
   case MdlType_miqcp:
   case MdlType_mcp:
   case MdlType_mpec:
      return  gams_copyassolvable_no_transform(mdl, mdl_src);
   case MdlType_vi: {
      Model *mdl_mcp;
      S_CHECK(mdl_transform_tomcp(mdl_src, &mdl_mcp));
      S_CHECK(gams_copyassolvable_no_transform(mdl, mdl_mcp));
      mdl_release(mdl_mcp);
      return OK;
   }
   case MdlType_emp: {
      Model *mdl_gmo_exportable;
      S_CHECK(mdl_transform_emp_togamsmdltype(mdl_src, &mdl_gmo_exportable));
      S_CHECK(gams_copyassolvable_no_transform(mdl, mdl_gmo_exportable));
      mdl_release(mdl_gmo_exportable);
      return OK;
   }
   default:
      error("%s ERROR: Model type %s is not yet supported\n", __func__, backend_name(probtype));
      return Error_NotImplemented;
   }
}

static int gams_copysolveoptions(Model *mdl, const Model *mdl_src)
{
   assert(mdl_src);
   GmsContainerData *gms_dst = (GmsContainerData *)mdl->ctr.data;

   if (!gms_dst->initialized) {
      error("%s ERROR: GEV is not initialized!\n", __func__);
      return Error_NotInitialized;
   }

   switch (mdl_src->backend) {
   case RHP_BACKEND_GAMS_GMO:
   {
      /* Copy a few interesting parameters */
      const GmsContainerData *gms_src = (const GmsContainerData *)mdl_src->ctr.data;
      gmoOptFileSet(gms_dst->gmo, gmoOptFile(gms_src->gmo));
      gevSetIntOpt(gms_dst->gev, gevKeep, gevGetIntOpt(gms_src->gev, gevKeep));
      gevSetDblOpt(gms_dst->gev, gevOptCR, gevGetDblOpt(gms_src->gev, gevOptCR));
      gevSetDblOpt(gms_dst->gev, gevOptCA, gevGetDblOpt(gms_src->gev, gevOptCA));
      break;
   }
   case RHP_BACKEND_RHP:
   case RHP_BACKEND_JULIA:
   {
      union opt_t val;
      S_CHECK(mdl_getoption(mdl_src, "solver_option_file_number", &val.i));
      gmoOptFileSet(gms_dst->gmo, val.i);

      mdl_getoption(mdl_src, "keep_files", &val.b);
      gevSetIntOpt(gms_dst->gev, gevKeep, val.b);
      mdl_getoption(mdl_src, "rtol", &val.d);
      gevSetDblOpt(gms_dst->gev, gevOptCR, val.d);
      mdl_getoption(mdl_src, "atol", &val.d);
      gevSetDblOpt(gms_dst->gev, gevOptCA, val.d);
      break;
   }
   default:
      error("%s :: unsupported source container %d", __func__,
               mdl_src->backend);
      return Error_InvalidValue;
   }


   return OK;
}

static int gams_copystatsfromsolver(Model *mdl, const Model *mdl_solver)
{
   if (mdl_solver->backend != RHP_BACKEND_GAMS_GMO) {
      logger(PO_INFO, "[gams] reporting solver stats from backend %s is not yet "
             "supported\n", backend_name(mdl_solver->backend));
      return OK;
   }

   /* TODO GAMS REVIEW everything below */
   enum gmoHeadnTail stats[] = {
      gmoHiterused ,
      gmoHresused  ,
      gmoHobjval   ,
      gmoHdomused  ,
//      gmoHmarginals,
      gmoHetalg    ,
      gmoTmipnod   ,
      gmoTninf     ,
      gmoTnopt     ,
      gmoTmipbest  ,
      gmoTsinf     ,
      gmoTrobj      
   };

   GmsContainerData *gms = mdl->ctr.data;
   GmsContainerData *gms_solver = mdl_solver->ctr.data;
   gmoHandle_t gmo = gms->gmo, gmo_solver = gms_solver->gmo;

   for (unsigned i = 0, len = ARRAY_SIZE(stats); i < len; ++i) {
      gmoSetHeadnTail(gmo, stats[i], gmoGetHeadnTail(gmo_solver, stats[i]));
   }

   /* We assume we always have marginal */
   gmoSetHeadnTail(gmo, gmoHmarginals, 1.);

   /* JAMS used to report gmoModelStat_OptimalLocal even is the solve was
    * successful. On the other hand, PATH returns gmoModelStat_OptimalGlobal */
   if (gmoModelType(gmo_solver) == gmoProc_mcp &&
      gmoModelStat(gmo) == gmoModelStat_OptimalGlobal) {
      gmoModelStatSet(gmo, gmoModelStat_OptimalLocal);
   }

   return OK;
}

static int gmdl_prepare_export(Model *mdl, Model *mdl_dst)
{

   S_CHECK(ctr_prepare_export(&mdl->ctr, &mdl_dst->ctr));
   
   if (mdl_dst->ctr.status & (CtrInstantiable)) {
      mdl_dst->status |= MdlContainerInstantiable;
   } else {
      error("[model] ERROR: while preparing the export in %s model '%.*s' #%u: "
            "container is not ready after %s\n", mdl_fmtargs(mdl_dst), __func__);
      return Error_RuntimeError;
   }
   return OK;
}

static int gams_export2gmo(Model *mdl, Model *mdl_dst)
{
   ModelType mdltype;
   S_CHECK(mdl_gettype(mdl, &mdltype));

   switch (mdltype) {
   case MdlType_lp:
   case MdlType_nlp:
   case MdlType_qcp:
   case MdlType_mip:
   case MdlType_minlp:
   case MdlType_miqcp:
   case MdlType_mcp:
   case MdlType_mpec:
      break;
   case MdlType_vi:
   case MdlType_emp: {

      error("[gams] ERROR in %s model '%.*s' #%u: type %s not exportable to GMO\n",
            mdl_fmtargs(mdl), mdltype_name(mdltype));
      return Error_RuntimeError;
   }

   case MdlType_cns:        /**< CNS   Constrained Nonlinear System */
      error("%s :: CNS is not yet supported\n", __func__);
      return Error_NotImplemented;

   case MdlType_dnlp:       /**< DNLP  Nondifferentiable NLP  */
      error("%s :: nonsmooth NLP are not yet supported\n", __func__);
      return Error_NotImplemented;

   default:
      error("%s :: no solve procedure for a model of type %s\n", __func__,
            mdltype_name(mdltype));
      return Error_NotImplemented;
   }

   S_CHECK(gmdl_prepare_export(mdl, mdl_dst));

   return gmdl_creategmo(mdl, mdl_dst);
}

static int gams_export2rhp(Model *mdl, Model *mdl_dst)
{
   assert(mdl_dst->backend == RHP_BACKEND_RHP);

   if (mdl->ctr.fops) {
      TO_IMPLEMENT("gams_export2rhp() with filtering");
   }

   return rmdl_initfromfullmdl(mdl_dst, mdl);
}

static int gams_export(Model *mdl, Model *mdl_dst)
{
   assert(mdl->backend == RHP_BACKEND_GAMS_GMO);
   BackendType backend = mdl_dst->backend;

   mdl_linkmodels(mdl, mdl_dst);
   assert(!(mdl_dst->status & MdlContainerInstantiable));

   switch (backend) {
   case RHP_BACKEND_GAMS_GMO: S_CHECK(gams_export2gmo(mdl, mdl_dst)); break;
   case RHP_BACKEND_RHP:      S_CHECK(gams_export2rhp(mdl, mdl_dst)); break;
   default: error("%s ERROR: unsupported backend %s", __func__, backend_name(backend));
   }

   return OK;
}

static int gams_getsolvername(const Model *mdl, char const **solvername)
{
   GmsModelData *mdldat = (GmsModelData *)mdl->ctr.data;

   *solvername = mdldat->solvername;

   return OK;
}

static int gams_getsolvestat(const Model *mdl, int *solvestat)
{
   int stat = gmoSolveStat(((const GmsContainerData *) mdl->ctr.data)->gmo);
   (*solvestat) = stat;

   return OK;
}

static int gams_getobjequ(const Model *mdl, rhp_idx *objequ)
{
   int equ = gmoObjRow(((const GmsContainerData *) mdl->ctr.data)->gmo);

   if (equ == GMS_SV_NAINT) {
      (*objequ) = IdxNA;
      return OK;
   }
   if (equ >= 0) {
      (*objequ) = equ;
      return OK;
   }
   (*objequ) = IdxInvalid;
   return Error_NotFound;
}

static int gams_getobjjacval(const Model *mdl, double *objjacval)
{

   (*objjacval) = gmoObjJacVal(((const GmsContainerData *) mdl->ctr.data)->gmo);

   return OK;
}

static int gams_getsense(const Model *mdl, RhpSense *objsense)
{
   return gctr_getsense(&mdl->ctr, objsense);
}

static int gams_getobjvar(const Model *mdl, rhp_idx *objvar)
{
   int var;

   GmsContainerData *gms = (GmsContainerData *)mdl->ctr.data;
   gmoHandle_t gmo = gms->gmo;

   var = gmoObjVar(gmo);

   if (var >= 0) {
      if (var == gmoValNAInt(gmo)) {
         (*objvar) = IdxNA;
      } else {
         (*objvar) = var;
      }
      return OK;
   }

   (*objvar) = IdxInvalid;

   char buf[GMS_SSSIZE];
   error("[GAMS] ERROR: invalid objective variable for model '%s'", gmoNameModel(gmo, buf));
   return Error_NotFound;
}

static int gams_getoption(const Model *mdl, const char *opt, void *val)
{
   assert(val);

   const GmsContainerData *gms = mdl->ctr.data;

   for (size_t i = 0; i < rmdl_opt_to_gams_len; ++i) {
      if (!strcmp(opt, rmdl_opt_to_gams[i].name)) {

         switch(rmdl_opt_to_gams[i].type) {
         case OptDouble:
            (*(double*)val) = gevGetDblOpt(gms->gev, rmdl_opt_to_gams[i].gams_opt_name);
            break;
         case OptInteger:
         case OptBoolean:
            (*(int*)val) = gevGetIntOpt(gms->gev, rmdl_opt_to_gams[i].gams_opt_name);
            break;
         case OptString:
            gevGetStrOpt(gms->gev, rmdl_opt_to_gams[i].gams_opt_name, (char *)val);
            break;
         default:
            error("%s :: unsupported option %s of type %d\n",
                     __func__, opt, rmdl_opt_to_gams[i].type);
            return Error_NotImplemented;
         }

         return OK;
      }
   }

   if (!strcmp(opt, "solver_option_file_number")) {
      (*(int*)val) = gmoOptFile(gms->gmo);
      return OK;
   }

   error("[GAMS] ERROR: no option named '%s' in the common options.\n"
                      "The supported options are:\n", opt);
   for (size_t i = 0; i < rmdl_opt_to_gams_len; ++i) {
     error("%s\n", rmdl_opt_to_gams[i].name);
   }

   errormsg("solver_option_file_number\n");


   return Error_NotFound;
}

static int gams_postprocess(Model *mdl)
{
   return OK;
}

static int gams_reportvalues_from_gams(Container *ctr, const Container *ctr_src)
{
   GmsContainerData *gms = (GmsContainerData *) ctr->data;
   const GmsContainerData *gms_src = (const GmsContainerData *) ctr_src->data;

   unsigned  n = gmoN(gms_src->gmo);
   unsigned m = gmoM(gms_src->gmo);
   if (ctr->n > n || ctr->m > m) {
      error("%s :: the size of the destination gmo is larger than the source one: "
            "n = %u; m = %u vs n = %u; m = %u\n", __func__, ctr->n, ctr->m, n, m);
      return Error_DimensionDifferent;
   }

   size_t max = MAX(m, n);
   struct ctrmem CTRMEM working_mem = {.ptr = NULL, .ctr = ctr};
   A_CHECK(working_mem.ptr, ctr_getmem(ctr, max*sizeof(double)));
   void *pointer = working_mem.ptr;

   gmoGetVarL(gms_src->gmo, (double*)pointer);
   gmoSetVarL(gms->gmo, (double*)pointer);
   gmoGetVarM(gms_src->gmo, (double*)pointer);
   gmoSetVarM(gms->gmo, (double*)pointer);
   gmoGetVarStat(gms_src->gmo, (int*)pointer);
   gmoSetVarStat(gms->gmo, (int*)pointer);
   gmoGetVarCStat(gms_src->gmo, (int*)pointer);
   gmoSetVarCStat(gms->gmo, (int*)pointer);

   gmoGetEquL(gms_src->gmo, (double*)pointer);
   gmoSetEquL(gms->gmo, (double*)pointer);
   gmoGetEquM(gms_src->gmo, (double*)pointer);
   gmoSetEquM(gms->gmo, (double*)pointer);
   gmoGetEquStat(gms_src->gmo, (int*)pointer);
   gmoSetEquStat(gms->gmo, (int*)pointer);
   gmoGetEquCStat(gms_src->gmo, (int*)pointer);
   gmoSetEquCStat(gms->gmo, (int*)pointer);

   return OK;
}

static int rmdl_resetvarbasis_v2(Container *ctr, double objmaxmin)
{
   double minf, pinf, nan;

   ctr_getspecialfloats(ctr, &pinf, &minf, &nan);
   /* TODO: add option for that */
   double tol_bnd = 1e-8;

   for (size_t i = 0, len = ctr_nvars_total(ctr); i < len; i++) {
      Var * restrict v = &ctr->vars[i];

      if (v->basis != BasisBasic) {
         double lev = v->value;
         double lb = v->bnd.lb;
         double ub = v->bnd.ub;

         if (lb != minf && ub != pinf) {
            if (fabs(lb - ub) < tol_bnd) {

               /* ---------------------------------------------------------
                * Fixed variable
                * --------------------------------------------------------- */

               if (v->multiplier * objmaxmin >= 0.) {
                  v->basis = BasisLower;
               } else {
                  v->basis = BasisUpper;
               }
            } else {

               /* ---------------------------------------------------------
                * Doubly-bounded variable
                * --------------------------------------------------------- */

               if (fabs(lev - lb) < tol_bnd) {
                  v->basis = BasisLower;
               } else if (fabs(lev - ub) < tol_bnd) {
                  v->basis = BasisUpper;
               } else {
                  v->basis = BasisSuperBasic;
               }
            }
         } else if (lb != minf) {

            /* ------------------------------------------------------------
             * Lower bounded
             * ------------------------------------------------------------ */
            if (fabs(lev - lb) < tol_bnd) {
               v->basis = BasisLower;
            } else {
               v->basis = BasisSuperBasic;
            }
         } else if (ub != pinf) {

            /* ------------------------------------------------------------
             * Upper bounded
             * ------------------------------------------------------------ */

            if (fabs(lev - ub) < tol_bnd) {
               v->basis = BasisUpper;
            } else {
               v->basis = BasisSuperBasic;
            }
         } else {

            /* ------------------------------------------------------------
             * Free
             * ------------------------------------------------------------ */
            v->basis = BasisSuperBasic;
         }
      }
   }

   return OK;
}

static int rmdl_resetequbasis_v2(Container *ctr, double objmaxmin)
{
   for (unsigned i = 0, len = ctr_nequs_total(ctr); i < len; i++) {
      if (ctr->equs[i].basis == BasisUnset) {
         int gams_type;
         S_CHECK(cone_to_gams_relation_type(ctr->equs[i].cone, &gams_type));
         if (gams_type == gmoequ_E) {
            if (ctr->equs[i].multiplier * objmaxmin >= 0.) {
               ctr->equs[i].basis = BasisLower;
            } else {
               ctr->equs[i].basis = BasisUpper;
            }
         } else if (gams_type == gmoequ_G) {
            ctr->equs[i].basis = BasisLower;
         } else if (gams_type == gmoequ_L) {
            ctr->equs[i].basis = BasisUpper;
         } else {
            ctr->equs[i].basis = BasisSuperBasic;
         }
      }
   }

   return OK;
}


static int gams_reportvalues_from_rhp(Container *ctr, const Model *mdl_src)
{
   GmsContainerData *gms = (GmsContainerData *)ctr->data;

   RhpSense objsense;
   S_CHECK(mdl_getsense(mdl_src, &objsense));

   const Container *ctr_src = &mdl_src->ctr;

  /* ----------------------------------------------------------------------
   * GAMS wants marginals, which are defined as follows:
   * - If the problem sense is min, then the marginal is the dual multiplier
   *   If the problem sense is max, then the marginal is the polar multiplier
   * ---------------------------------------------------------------------- */


   double objmaxmin;
   bool flip_multiplier = false;
   EquMeta * restrict equmeta = NULL;
   VarMeta * restrict varmeta = NULL;
   DBGUSED unsigned mp_len = mdl_src->empinfo.empdag.mps.len;
   MathPrgm ** const mps = mdl_src->empinfo.empdag.mps.arr;

   switch (objsense) {
   case RhpMin:
   case RhpFeasibility:
      objmaxmin = 1;
      break;
   case RhpMax:
      objmaxmin = -1;
      flip_multiplier = true;
      break;
   case RhpNoSense:
      varmeta = mdl_src->ctr.varmeta;
      equmeta = mdl_src->ctr.equmeta;
      objmaxmin = 1.; /* TODO  GITLAB #93 */
      break;
   default:
      error("%s :: unsupported sense %s for %s model '%.*s' #%u\n", __func__,
           sense2str(objsense), mdl_fmtargs(mdl_src));
      return Error_NotImplemented;
   }

   /* TODO  GITLAB #93 */
   if (isfinite(objmaxmin)) {
      S_CHECK(rmdl_resetvarbasis_v2((Container *)ctr_src, objmaxmin));
      S_CHECK(rmdl_resetequbasis_v2((Container *)ctr_src, objmaxmin));
   }

   gmoHandle_t gmo = gms->gmo;
   double gms_pinf = gmoPinf(gmo);
   double gms_minf = gmoMinf(gmo);
   double gms_na = gmoValNA(gmo);

   assert(ctr->n == gmoN(gms->gmo));
   for (int i = 0, len = ctr->n; i < len; ++i) {
      const Var *v = &ctr_src->vars[i];

      if (varmeta) {
         mpid_t mpid = varmeta[i].mp_id;
         if (mpid_regularmp(mpid)) {
            assert(mpid < mp_len);
            flip_multiplier = mp_getsense(mps[mpid]) == RhpMax ? true : false;
         } else {
            flip_multiplier = false;
         }
      }

      double marginal = flip_multiplier ? -v->multiplier : v->multiplier;
      GMSCHK(gmoSetSolutionVarRec(gms->gmo,
                                      i,
                                      dbl_to_gams(v->value, gms_pinf, gms_minf, gms_na),
                                      dbl_to_gams(marginal, gms_pinf, gms_minf, gms_na),
                                      basis_to_gams(v->basis),
                                      gmoCstat_OK));
   }

   assert(ctr->m == gmoM(gms->gmo));
   for (int i = 0, len = ctr->m; i < len; ++i) {
      const Equ *e = &ctr_src->equs[i];
      double value = e->value;
      if (e->object == Mapping) {
         value -= e->p.cst;
      }

      if (equmeta) {
         mpid_t mpid = equmeta[i].mp_id;
         if (mpid_regularmp(mpid)) {
            assert(mpid < mp_len);
            flip_multiplier = mp_getsense(mps[mpid]) == RhpMax ? true : false;
         } else {
            flip_multiplier = false;
         }
      }

      double marginal = flip_multiplier ? -e->multiplier : e->multiplier;



      GMSCHK(gmoSetSolutionEquRec(gms->gmo,
                                      i,
                                      dbl_to_gams(value, gms_pinf, gms_minf, gms_na),
                                      dbl_to_gams(marginal, gms_pinf, gms_minf, gms_na),
                                      basis_to_gams(e->basis),
                                      gmoCstat_OK));
   }

   return OK;
}

static int gams_reportvalues(Model *mdl, const Model *mdl_src)
{
   switch (mdl_src->backend) {
   case RHP_BACKEND_GAMS_GMO:
      return gams_reportvalues_from_gams(&mdl->ctr, &mdl_src->ctr);
   case RHP_BACKEND_RHP:
   case RHP_BACKEND_JULIA:
      return gams_reportvalues_from_rhp(&mdl->ctr, mdl_src);
   default:
      error("%s :: not implement for container of type %s\n", __func__, backend_name(mdl_src->backend));
      return Error_NotImplemented;
   }
}


static int gams_setmodelstat(Model *mdl, int modelstat)
{
   gmoModelStatSet(((GmsContainerData *) mdl->ctr.data)->gmo, modelstat);

   return OK;
}

static int gams_setobjsense(Model *mdl, RhpSense objsense)
{
   int gms_sense;
   switch (objsense) {
   case RhpMin:
      gms_sense = gmoObj_Min;
      break;
   case RhpMax:
      gms_sense = gmoObj_Max;
      break;
   default:
      error("%s :: unsupported sense '%s' #%u\n", __func__,
            sense2str(objsense), objsense);
      return Error_InvalidValue;
   }

   gmoSenseSet(((GmsContainerData *) mdl->ctr.data)->gmo, gms_sense);
   return OK;
}

static int gams_setobjvar(Model *mdl, rhp_idx objvar)
{
   S_CHECK(vi_inbounds(objvar, ctr_nvars(&mdl->ctr), __func__));

   gmoObjVarSet(((GmsContainerData *) mdl->ctr.data)->gmo, objvar);
   return OK;
}

static int gams_setsolvestat(Model *mdl, int solvestat)
{
   gmoSolveStatSet(((GmsContainerData *) mdl->ctr.data)->gmo, solvestat);

   return OK;
}

static int gams_setsolvername(Model *mdl, const char *solvername)
{
   S_CHECK(gams_chk_str(solvername, __func__));

   GmsModelData *mdldat = mdl->data;

   /* This is safe, since we check the length in gams_chk_str  */
   strcpy(mdldat->solvername, solvername);

   return OK;
}

const ModelOps mdl_ops_gams = {
   .allocdata           = gams_allocdata,
   .deallocdata         = gams_deallocdata,
   .checkmdl            = gams_checkmdl,
   .checkobjequvar      = gams_checkobjequvar,
   .copyassolvable      = gams_copyassolvable,
   .copysolveoptions    = gams_copysolveoptions,
   .copystatsfromsolver = gams_copystatsfromsolver,
   .export              = gams_export,
   .getmodelstat        = gams_getmodelstat, /* DEL -> solveinfos */
   .getobjequ           = gams_getobjequ,
   .getobjjacval        = gams_getobjjacval,
   .getsense            = gams_getsense,
   .getobjvar           = gams_getobjvar,
   .getoption           = gams_getoption,
   .getsolvername       = gams_getsolvername,
   .getsolvestat        = gams_getsolvestat, /* DEL -> solveinfos */
   .postprocess         = gams_postprocess,
   .reportvalues        = gams_reportvalues,
   .setmodelstat        = gams_setmodelstat, /* DEL -> solveinfos */
   .setsense            = gams_setobjsense,
   .setobjvar           = gams_setobjvar,
   .setsolvestat        = gams_setsolvestat, /* DEL -> solveinfos */
   .setsolvername       = gams_setsolvername,
   .solve               = gams_solve,
};
