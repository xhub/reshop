#include "gams_logging.h"
#include "gams_macros.h"
#include "reshop_config.h"
#include "asprintf.h"
#include "reshop_gams_common.h"
#include <fcntl.h>

/* For IO related things  */
#ifdef _WIN32

/* For _S_IREAD, _S_IWRITE */
#include <sys/stat.h>

#include <io.h>
#define open _open
#define close _close

#else

#include <unistd.h>

#endif
#include "checks.h"
#include "container.h"
#include "ctrdat_gams.h"
#include "fs_func.h"
#include "gams_rosetta.h"
#include "macros.h"
#include "mdl.h"
#include "mdl_gams.h"
#include "printout.h"
#include "rhp_fwd.h"
#include "tlsdef.h"
#include "win-compat.h"

static tlsvar char *gamsdir = NULL;
static tlsvar char *gamscntr = NULL;

#ifndef CLEANUP_FNS_HAVE_DECL
static
#endif
void DESTRUCTOR_ATTR cleanup_gams(void) 
{
  FREE(gamsdir);
  FREE(gamscntr);
   gams_unload_libs();
}


static int ensure_matrixfile(const char *path)
{

   char *filename;
   IO_PRINT(asprintf(&filename, "%s" DIRSEP "gamsmatr.dat", path));

#ifdef _WIN32
   int fh = open(filename, _O_WRONLY | _O_CREAT, _S_IREAD | _S_IWRITE ); // C4996 
#else
   int fh = open(filename, O_CREAT|O_WRONLY, S_IRWXU);
#endif

   if (fh == -1) {
      perror("open");
      error("While trying to open '%s'\n", filename);
      goto _exit;
   } else {
      int err = close(fh);

      if (err == -1) {
         perror("Close failed");
         goto _exit;
      }
   }

   FREE(filename);

   return OK;

_exit:
   FREE(filename);
   return Error_SystemError;
}


char* gams_getgamsdir(void)
{
   return gamsdir;
}

char* gams_getgamscntr(void)
{
   return gamscntr;
}

int gams_setgamsdir(const char* dirname)
{
   FREE(gamsdir);
   A_CHECK(gamsdir, strdup(dirname)); 

   return OK;
}

int gams_setgamscntr(const char *fname)
{
   FREE(gamscntr);
   A_CHECK(gamscntr, strdup(fname)); 

   return OK;
}

int gams_chk_mdl(const Model* mdl, const char *fn)
{
   S_CHECK(chk_mdl(mdl, __func__));

   if (mdl->backend != RhpBackendGamsGmo) {
      error("%s :: Model is of type %s, expected %s", fn,
            backend2str(mdl->backend), backend2str(RhpBackendGamsGmo));
      return Error_WrongModelForFunction;
   }

   if (!mdl->data) {
      error("%s :: GAMS model data in not initialized yet!\n", fn);
      return Error_NotInitialized;
   }

   return OK;
}

int gams_chk_mdlfull(const Model* mdl, const char *fn)
{
   S_CHECK(chk_mdl(mdl, __func__));

   if (mdl->backend != RhpBackendGamsGmo) {
      error("%s ERROR: Model is of type %s, expected %s", fn,
            backend2str(mdl->backend), backend2str(RhpBackendGamsGmo));
      return Error_WrongModelForFunction;
   }

   if (!mdl->data) {
      error("%s ERROR: GAMS model data in not initialized yet!\n", fn);
      return Error_NotInitialized;
   }

   GmsContainerData *gms = mdl->ctr.data;
   if (!gms->initialized) {
      error("[GAMS] ERROR in %s(): missing GAMS objects in %s model '%.*s' #%u\n",
            fn, mdl_fmtargs(mdl));
      return Error_RuntimeError;
   }

   return OK;
}

int gmdl_setprobtype(Model *mdl, enum mdl_type probtype)
{
   assert(mdl->backend == RhpBackendGamsGmo);
   Container *ctr = &mdl->ctr;
   const GmsContainerData *gms = ctr->data;

   if (gms->initialized) {
      enum gmoProcType gams_probtype = mdltype_to_gams(probtype);
      if (gams_probtype == gmoProc_none) {
         error("[model] ERROR: GAMS does not support modeltype %s\n", mdltype_name(probtype));
         return Error_NotImplemented;
      }

      gmoModelTypeSet(gms->gmo, gams_probtype);
   } else {
      error("[model] ERROR in %s model '%.*s' #%u: uninitialized GMO\n",
            mdl_fmtargs(mdl));
      return Error_NotInitialized;
   }

   return OK;
}

/* TODO(xhub) document + rename as gams  */
/**
 * @brief Write (via convert) a GAMS model as a gms
 *
 * @param mdl       model
 * @param filename  filename (currently not used)
 *
 * @return          the error code
 */
int gmdl_writeasgms(const Model *mdl, const char *filename)
{
   if (mdl->backend != RhpBackendGamsGmo) {
      return OK;
   }

   strncpy(mdl->ctr.data, "CONVERT", 20);
   S_CHECK(mdl_solve((Model *)mdl));
   strncpy(mdl->ctr.data, "", 2);

   return OK;
}

int gmdl_set_gamsdata_from_env(Model *mdl)
{
  const char* gamsdir_env = mygetenv("RHP_GAMSDIR");
  if (gamsdir_env) {
    S_CHECK(rhp_gms_setgamsdir(mdl, gamsdir_env));
  } else {
    errormsg("Specify RHP_GAMSDIR!\n");
    return Error_RuntimeError;
  }
   myfreeenvval(gamsdir_env);

  const char* gamscntr_env = mygetenv("RHP_GAMSCNTR_FILE");
  if (gamscntr_env) {
    S_CHECK(rhp_gms_setgamscntr(mdl, gamscntr_env));
  } else {
    errormsg("Specify RHP_GAMSCNTR_FILE!\n");
    return Error_RuntimeError;
  }
   myfreeenvval(gamscntr_env);

  return OK;
}

int gmdl_writesol2gdx(Model *mdl, const char *gdxname)
{
   assert(gams_chk_mdl(mdl, __func__) == OK);

   GmsContainerData *gms = mdl->ctr.data;
   GMSCHK(gmoUnloadSolutionGDX(gms->gmo, gdxname, 1, 1, 1));

   return OK;
}

int gmdl_ensuresimpleprob(Model *mdl)
{
   EmpDag *empdag = &mdl->empinfo.empdag;
   assert(empdag->mps.len == 0);

   rhp_idx objvar, objequ;
   RhpSense sense;
   S_CHECK(mdl_getobjvar(mdl, &objvar));
   S_CHECK(mdl_getobjequ(mdl, &objequ));
   S_CHECK(mdl_getsense(mdl, &sense));

   empdag->type = EmpDag_Empty;

   empdag->simple_data.sense = sense;
   empdag->simple_data.objvar = objvar;
   empdag->simple_data.objequ = objequ;

   S_CHECK(mdl_settype(mdl, MdlType_none));
   S_CHECK(mdl_analyze_modeltype(mdl));

   trace_empdag("[empdag] %s model '%.*s' #%u has now EMPDAG type %s\n", mdl_fmtargs(mdl),
                empdag_typename(empdag->type));

   return OK;
}

/**
 * @brief Setup GAMS object before filling GMO object
 *
 * @param mdl_gms  The GAMS model (destination)
 * @param mdl_src  The source model
 *
 * @return         The error code
 */
int gmdl_cdat_create(Model *mdl_gms, Model *mdl_src)
{
   char gamsctrl_new[GMS_SSSIZE], scrdir[GMS_SSSIZE], buf[GMS_SSSIZE];
   GmsModelData *mdldat = mdl_gms->data;

   /* ----------------------------------------------------------------------
    * Deal with the GAMS Control File crazyness
    *
    * This is required before calling gcdat_init
    * ---------------------------------------------------------------------- */

   bool has_gmsdata = false;
   Model *mdl_gms_up = mdl_src;

   while (mdl_gms_up) {
      if (mdl_gms_up->backend == RhpBackendGamsGmo) {
         has_gmsdata = true;
         break;
      }

      mdl_gms_up = mdl_gms_up->mdl_up;
   }

   /* FIXME: shouldn't this be has_gmsdata ?*/
   if (mdl_gms_up) {
      assert(mdl_gms_up->backend == RhpBackendGamsGmo);
      const GmsModelData *mdldat_up = mdl_gms_up->data;
      Container *ctr_up = &mdl_gms_up->ctr;
      const GmsContainerData *gms = ctr_up->data;
      gevHandle_t gev = gms->gev;

      /* -------------------------------------------------------------------
       * Get a new scratch dir
       * ------------------------------------------------------------------- */

      gevGetStrOpt(gev, gevNameScrDir, scrdir);
      if(!dir_exists(scrdir)) {
         error("[gams] ERROR: cannot access scrdir '%s' of %s model '%.*s' #%u\n",
               scrdir, mdl_fmtargs(mdl_gms_up));
         return Error_SystemError;
      }

      /* -------------------------------------------------------------------
       * Copy the GAMS directory
       * ------------------------------------------------------------------- */

      if (strlen(mdldat_up->gamsdir)) {
         trace_backend("[GAMS] %s model '%.*s' #%u: gamsdir value '%s' inherited"
                       " from %s model '%.*s' #%u\n", mdl_fmtargs(mdl_gms),
                       mdldat_up->gamsdir, mdl_fmtargs(mdl_gms_up));
         STRNCPY_FIXED(mdldat->gamsdir, mdldat_up->gamsdir);
      } else {
         gevGetStrOpt(gev, gevNameSysDir, mdldat->gamsdir);
         trace_backend("[GAMS] %s model '%.*s' #%u: gamsdir value '%s' set from GEV\n",
                       mdl_fmtargs(mdl_gms), mdldat->gamsdir);
      }

      if(!dir_exists(mdldat_up->gamsdir)) {
         error("[GAMS] ERROR: cannot access gamsdir '%s'\n", mdldat_up->gamsdir);
         return Error_SystemError;
      }

      trace_model("[GAMS] %s model '%.*s' #%u: original scrdir is '%s'\n",
                  mdl_fmtargs(mdl_gms), scrdir);

      S_CHECK(ensure_matrixfile(scrdir));
      size_t len_namescr = strlen(scrdir);
      const char * dirname = mdl_gms->commondata.name ? mdl_gms->commondata.name : "reshop";
      strncat(scrdir, dirname, GMS_SSSIZE - len_namescr + 1);

      S_CHECK(new_unique_dirname(scrdir, GMS_SSSIZE));

      /* --------------------------------------------------------------------
       * WARNING: never ever call gevSetStrOpt for gevNameScrDir before
       * gevDuplicateScratchDir, since the substitution won't happen.
       * gevDuplicateScratchDir takes care of replacing the scrdir name
       * -------------------------------------------------------------------- */


      /* \TODO(xhub) understand why we need a logfile */
      char logfile_new[GMS_SSSIZE];
      STRNCPY_FIXED(logfile_new, scrdir);

      strncat(logfile_new, DIRSEP "gamslog.dat", strlen(logfile_new)-1);

      if (gevDuplicateScratchDir(gev, scrdir, logfile_new, gamsctrl_new)) {
         errormsg("[GAMS] ERROR: call to gevDuplicateScratchDir failed\n");
         return Error_SystemError;
      }

      /* We know for sure we can delete the scratch dir */
      mdldat->delete_scratch = true;

      STRNCPY_FIXED(mdldat->gamscntr, gamsctrl_new);
      trace_model("[model] %s model '%.*s' #%u: gamscntr from gevDuplicateScratchDir()"
               " is '%s'\n", mdl_fmtargs(mdl_gms), mdldat->gamscntr);

      /* TODO: is this still needed?
       * WARNING: this must be after gevDuplicateScratchDir() */
      S_CHECK(ensure_matrixfile(scrdir));

   } else {

      /* We need to get a dummy gamscntr.dat file */
      if (!strlen(mdldat->gamscntr)) {
         errormsg("[GAMS] ERROR: no dummy gamscntr.dat file provided\n");
         return Error_GamsIncompleteSetupInfo;
      }

      if (!strlen(mdldat->gamsdir)) {
         errormsg("[GAMS] ERROR: no GAMS system directory provided\n");
         return Error_GamsIncompleteSetupInfo;
      }
   }

   /* ----------------------------------------------------------------------
    * Create all the GAMS objects: GMO, GEV, DCT, CFG
    * ---------------------------------------------------------------------- */

   Container *ctrgms = &mdl_gms->ctr;
   GmsContainerData *gmsdst = (GmsContainerData *)ctrgms->data;

   S_CHECK(gcdat_init_withdct(mdl_gms->ctr.data, mdldat));

   gmoHandle_t gmodst = gmsdst->gmo;
   dctHandle_t dctdst = gmsdst->dct;
   assert(0 == dctNRows(dctdst) && 0 == dctNCols(dctdst));

   /* ----------------------------------------------------------------------
    * Prepare the GMO object: set the model type, index base = 0 (for variable
    * and equation indices), set the name, and reuse optfile name to initialize
    * the directory where the option file are expected to be found. The proper
    * logic is done in GMO
    * ---------------------------------------------------------------------- */

   gmoIndexBaseSet(gmodst, 0);
   gmoNameModelSet(gmodst, mdl_src->commondata.name);
   gmoModelTypeSet(gmodst, mdltype_to_gams(mdl_src->commondata.mdltype));

   if (mdl_gms_up) {
      const GmsContainerData *gms = mdl_gms_up->ctr.data;
      gmoHandle_t gmo = gms->gmo;
      gmoNameOptFileSet(gmodst, gmoNameOptFile(gmo, buf));
   }

   /* ----------------------------------------------------------------------
    * Set the modeltype here: it is needed and has to be done after the new GAMS
    * env has been created
    * ---------------------------------------------------------------------- */

   S_CHECK(mdl_copyprobtype(mdl_gms, mdl_src));

   /* TODO(xhub) make an option for that */
   gevSetIntOpt(gmsdst->gev, gevKeep, 1);

   /* Set the GEV callback */
   S_CHECK(gev_logger_callback_init(mdl_gms, has_gmsdata ? GevLoggerCbUpstream : GevLoggerCbRhp));

   /* Initialize the GMO object with the problem size */
   GMSCHK_ERRMSG(gmoInitData(gmodst, ctrgms->m, ctrgms->n, 0), gmodst, buf);

   /* -----------------------------------------------------------------------
    * Initialize the dictionary object with the problem size
    * TODO: we need the number of UELs here
    * ---------------------------------------------------------------------- */

   if (!dctSetBasicCountsEx(dctdst, ctrgms->m, ctrgms->n, 0, buf, sizeof buf)) {
      error("[GAMS/DCT] ERROR while calling dctSetBasicCountsEx: %s\n", buf);
      return Error_GamsCallFailed;
   }

   return OK;
}

int gmdl_loadrhpoptions(Model *mdl)
{
   assert(gams_chk_mdl(mdl, __func__) == OK);
   GmsModelData *mdldat = mdl->data;
   GmsContainerData *gms = mdl->ctr.data;

   rhpRec_t r = {.eh = gms->gev, .gh = gms->gmo, .ch = gms->cfg};

   const char *sysdir = mdldat->gamsdir;
   char msg[GMS_SSSIZE];

   if (!optGetReadyD(sysdir, msg, sizeof(msg))) {
      gevLogStatPChar(r.eh, "[GAMS] ERROR: Could not load option library: ");
      gevLogStat(r.eh, msg);
      return Error_GamsCallFailed;
   }

   if (!optCreate(&r.oh, msg, sizeof(msg))) {
      gevLogStatPChar(r.eh, "[GAMS] ERROR: Could not create option struct: ");
      gevLogStat(r.eh, msg);
      return Error_GamsCallFailed;
   }

   int rc = opt_process(&r, true, sysdir);
   if (rc) {
      error("[GAMS] ERROR: processing option file failed with rc = %d", rc);
      return Error_GamsCallFailed;
   }

   rc = opt_pushtosolver(&r);
   if (rc) {
      error("[GAMS] ERROR: setting ReSHOP options failed with rc = %d", rc);
      return Error_RuntimeError;
   }

   optFree(&r.oh);

   return OK;
}
