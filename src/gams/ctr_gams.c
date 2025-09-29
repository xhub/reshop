#include "gams_macros.h"
#include "reshop_config.h"

#include <stdlib.h>
#include <string.h>
#include <sys/stat.h>
#include <sys/types.h>

#include "compat.h"
#include "container.h"
#include "ctr_gams.h"
#include "ctr_rhp.h"
#include "ctrdat_gams.h"
#include "ctrdat_rhp.h"
#include "nltree.h"
#include "gams_libver.h"
#include "macros.h"
#include "mdl_gams.h"
#include "printout.h"
#include "reshop.h"
#include "status.h"
#include "tlsdef.h"
#include "win-compat.h"

#include "gmomcc.h"
#include "optcc.h"
#include "gmdcc.h"

/* Looks like this is only used here. In the future, might be non-static if needed */
static int gams_chk_ctrdata(const Container *ctr, const char *fn)
{
   GmsContainerData *gms = (GmsContainerData*)ctr->data;

   if (!gms || !gms->initialized) {
      error("%s :: GMS data in not initialized yet!\n", fn);
      return Error_NotInitialized;
   }

   return OK;
}

int gams_chk_ctr(const Container *ctr, const char *fn)
{
   if (ctr->backend != RhpBackendGamsGmo) {
      error("%s :: the container has the wrong type: expected %s "
                         "(id %d), got %s (id %d).\n", fn,
                         backend2str(RhpBackendGamsGmo), RhpBackendGamsGmo,
                         backend2str(ctr->backend), ctr->backend);
      return Error_InvalidValue;
   }

   return gams_chk_ctrdata(ctr, fn);
}

/**
 * @brief Creates and initializes the GAMS container data (GEV, GMO, and CFG)
 *
 * - The GAMS control file is used to initialize the GEV object, via
 *   gevInitEnvironmentLegacy()
 * - The GMO object is initialized using gmoRegisterEnvironment()
 * - CFG object reads the configuration file "subsys". Note that gamsconfig.yaml seems to
 *   be always read.
 *
 * @param gms      the GAMS container data
 * @param gmdldat  the GAMS model data
 *
 * @return         the error code
 */
static int gcdat_init(GmsContainerData * restrict gms, GmsModelData * restrict gmdldat)
{
   char buf[GMS_SSSIZE];

   /* ---------------------------------------------------------------------
    * Fail if gamsdir has not already being given
    * --------------------------------------------------------------------- */

   if (!strlen(gmdldat->gamsdir)) {
      errormsg("[GAMS] ERROR: no GAMS sysdir was given, unable to continue!\n"
               "Use gams_setgamsdir to set the GAMS sysdir\n");
      return Error_RuntimeError;
   }

   /* Load all the libs */
   S_CHECK(gams_load_libs(gmdldat->gamsdir));

   /* ---------------------------------------------------------------------
    * Initialize GMO and GEV libraries.
    * --------------------------------------------------------------------- */

   if (!gmoCreateDD(&gms->gmo, gmdldat->gamsdir, buf, sizeof(buf))
       || !gevCreateDD(&gms->gev, gmdldat->gamsdir, buf, sizeof(buf))) {
      error("[GAMS] ERROR: loading GMO or GEV failed with message '%s'\n", buf);
      return Error_RuntimeError;
   }

   /* ---------------------------------------------------------------------
    * Load control file.
    *
    *  gevCompleteEnvironment(ctr_rmdl->gev, NULL, NULL, NULL, NULL);
    * --------------------------------------------------------------------- */

   if (!strlen(gmdldat->gamscntr)) {
      errormsg("[GAMS] ERROR: the control file is empty\n");
      return Error_GamsIncompleteSetupInfo;
   }

   if (gevInitEnvironmentLegacy(gms->gev, gmdldat->gamscntr)) {
      error("[GAMS] ERROR: loading control file '%s' failed\n",
               gmdldat->gamscntr);
      return Error_GamsCallFailed;
   }

   /* ---------------------------------------------------------------------
    * Register GAMS environment.
    * --------------------------------------------------------------------- */

   if (gmoRegisterEnvironment(gms->gmo, gms->gev, buf)) {
      error("[GAMS] ERROR: registering GAMS environment failed with error '%s'\n",
            buf);
      return Error_GamsCallFailed;
   }

   /* ---------------------------------------------------------------------
    * Create a configuration object.
    * --------------------------------------------------------------------- */

   if (!cfgCreateD(&gms->cfg, gmdldat->gamsdir, buf, sizeof(buf))) {
      error("[GAMS] ERROR: creating cfg object failed with message '%s'\n",
            buf);
      return Error_GamsCallFailed;
   }

   /* ---------------------------------------------------------------------
    * Read the configuration file.
    * --------------------------------------------------------------------- */

   /* TODO: this could not work with gamsconfig.yaml */

   size_t len = strlen(gmdldat->gamsdir);
   size_t dirsep_len = strlen(DIRSEP);
   bool has_dirsep = true;

   if (len >= dirsep_len) {
      assert(dirsep_len > 0);
      len -= 1;
      dirsep_len -= 1;
      for (size_t i = 0; i <= dirsep_len; ++i) {
         if (gmdldat->gamsdir[len-i] != DIRSEP[dirsep_len-i]) {
            has_dirsep = false;
            break;
         }
      }
   }

   /* TODO: GAMS review*/
   int isDefaultSubsys = gevGetIntOpt(gms->gev, gevisDefaultSubsys);

   if (isDefaultSubsys) {
      size_t slen = len + dirsep_len + strlen(GMS_CONFIG_FILE);
      if (slen > sizeof(buf)-1) {
         error("[GAMS] ERROR: filename '%s%s%s' has size %zu, max is %zu",
               gmdldat->gamsdir, DIRSEP, GMS_CONFIG_FILE, slen, sizeof(buf)-1);
         return Error_NameTooLongForGams;
      }

      strcpy(buf, gmdldat->gamsdir);
      if (!has_dirsep) {
         strcat(buf, DIRSEP);
      }
      strcat(buf, GMS_CONFIG_FILE);

   } else {
      gevGetStrOpt(gms->gev, gevsubsysFile, buf);
      trace_backend("[GAMS] Reading user-defined configuration file '%s'\n", buf)
   }
   strcat(buffer, GMS_CONFIG_FILE);

   /* This is required to get any solver */
   if (cfgReadConfig(gms->cfg, buf)) {
      error("[GAMS] ERROR: could not read %s configuration file '%s'\n",
            isDefaultSubsys ? "default" : "user-defined", buf);
      return Error_GamsCallFailed;
   }

   gms->owndct = false;
   gms->owning_handles = true;
   gms->initialized = true;

   return OK;
}

/**
 * @brief create the GAMS objects (GMO, GEV, DCT and CFG)
 *
 * @param gms       the GAMS container object
 *
 * @return          the error code
 */
int gcdat_init_withdct(GmsContainerData * restrict gms, GmsModelData * restrict gmdldat)
{
   char buffer[GMS_SSSIZE];

  /* ----------------------------------------------------------------------
   * The GmsContainerData is always initialized. If it is already there,
   * delete it first
   * ---------------------------------------------------------------------- */

   if (gms->owning_handles) {
      gcdat_free_handles(gms);
   }

   S_CHECK(gcdat_init(gms, gmdldat));

   GAMS_CHECK1(dctCreate, &gms->dct, buffer);
   gms->owndct = true;

   trace_backend("[GAMS] Successful initialization GAMS model with gamsdir='%s'; "
                 "gamscntr='%s'\n", gmdldat->gamsdir, gmdldat->gamscntr);

   return OK;
}

int gcdat_loadmdl(GmsContainerData * restrict gms, GmsModelData * restrict gmdldat)
{
   char buf[GMS_SSSIZE];

   /* ---------------------------------------------------------------------
    * Init GEV and GMO
    * --------------------------------------------------------------------- */

   S_CHECK(gcdat_init(gms, gmdldat));

  /* ----------------------------------------------------------------------
   * Load GMO 
   * ---------------------------------------------------------------------- */

   if (gmoLoadDataLegacy(gms->gmo, buf)) {
      error("[GAMS] ERROR: Loading model data failed with message '%s'\n", buf);
      return Error_GamsCallFailed;
   }

   gmoNameModel(gms->gmo, buf);

   assert(!gms->dct && !gms->owndct);
   gms->dct = gmoDict(gms->gmo);

   if (!gms->dct) {
       error("[GAMS] ERROR: GAMS/GMO model named '%s' has no dictionary. "
             "Check the solver configuration\n", buf);
      return Error_GamsIncompleteSetupInfo;
   }

   gevGetStrOpt(gms->gev, gevNameScrDir, gmdldat->scrdir);

   trace_process("[GAMS] Loaded GAMS/GMO model named '%s' with %u vars and %u equs "
                 "from %s\n", buf, gmoN(gms->gmo), gmoM(gms->gmo),
                 gmdldat->gamscntr);


   return OK;
}

int gams_load_libs(const char *sysdir)
{
   char msg[GMS_SSSIZE];

   if (!gmoLibraryLoaded() && !gmoGetReadyD(sysdir, msg, sizeof(msg))) {
      error("%s\n", msg);
      return Error_GamsIncompleteSetupInfo;
   }
   CHK_CORRECT_LIBVER(gmo, GMO, msg);

   if (!gevLibraryLoaded() && !gevGetReadyD(sysdir, msg, sizeof(msg))) {
      error("%s\n", msg);
      return Error_GamsIncompleteSetupInfo;
   }
   CHK_CORRECT_LIBVER(gev, GEV, msg);

   if (!dctLibraryLoaded() && !dctGetReadyD(sysdir, msg, sizeof(msg))) {
      error("%s\n", msg);
      return Error_GamsIncompleteSetupInfo;
   }
   CHK_CORRECT_LIBVER(dct, DCT, msg);

   if (!cfgLibraryLoaded() && !cfgGetReadyD(sysdir, msg, sizeof(msg))) {
      error("%s\n", msg);
      return Error_GamsIncompleteSetupInfo;
   }
   CHK_CORRECT_LIBVER(cfg, CFG, msg);

   if (!optLibraryLoaded() && !optGetReadyD(sysdir, msg, sizeof(msg))) {
      error("%s\n", msg);
      return Error_GamsIncompleteSetupInfo;
   }
   CHK_CORRECT_LIBVER(opt, OPT, msg);

   if (!gdxLibraryLoaded() && !gdxGetReadyD(sysdir, msg, sizeof(msg))) {
      error("%s\n", msg);
      return Error_GamsIncompleteSetupInfo;
   }
   CHK_CORRECT_LIBVER(gdx, GDX, msg);

   if (!gmdLibraryLoaded() && !gmdGetReadyD(sysdir, msg, sizeof(msg))) {
      error("%s\n", msg);
      return Error_GamsIncompleteSetupInfo;
   }
   CHK_CORRECT_LIBVER(gmd, GMD, msg);

   return OK;
}

#if defined(__has_feature)
#   if __has_feature(address_sanitizer) // for clang
#ifndef __SANITIZE_ADDRESS__
#       define __SANITIZE_ADDRESS__ // GCC already sets this
#   endif
#endif
#endif

#if defined(__SANITIZE_ADDRESS__)
    // ASAN is enabled . . .
#endif

void gams_unload_libs(void)
{
#ifndef __SANITIZE_ADDRESS__
   if (cfgLibraryLoaded()) cfgLibraryUnload();
   if (dctLibraryLoaded()) dctLibraryUnload();
   if (gdxLibraryLoaded()) gdxLibraryUnload();
   if (gevLibraryLoaded()) gevLibraryUnload();
   if (gmdLibraryLoaded()) gmdLibraryUnload();
   if (gmoLibraryLoaded()) gmoLibraryUnload();
   if (optLibraryLoaded()) optLibraryUnload();
#endif
}

void gcdat_free_handles(GmsContainerData *gms)
{
   if (gms->cfg) {
      cfgFree(&gms->cfg);
   }

   if (gms->gmo) {
      gmoFree(&gms->gmo);
   }

   if (gms->gev) {
      gevFree(&gms->gev);
   }

   if (gms->dct && gms->owndct) {
      dctFree(&gms->dct);
   }

   gms->owning_handles = false;
   gms->owndct = false;
   gms->initialized = false;
}

/**
 * @brief Get the opcode (from the GMO object) for an equation
 *
 * @warning On output, the memory array used for instrs and args
 *          is temporary (from the container workspace). Do immediately
 *          consume it or copy it to a long-term memory
 *
 * @param      ctr      the model
 * @param      ei     the equation index
 * @param[out] codelen  the length of the opcode
 * @param[out] instrs   the instructions
 * @param[out] args     the arguments
 *
 * @return              the error code
 */
int gctr_getopcode(Container *ctr, rhp_idx ei, int *codelen, int **instrs, int **args)
{
   assert(ctr->backend == RhpBackendGamsGmo);
   gmoHandle_t gmo = ((GmsContainerData *)ctr->data)->gmo;
   assert(gmo);

   int nlflag = gmoGetEquOrderOne(gmo, ei);

   switch (nlflag) {
   /*  TODO(xhub) support quadratic expression */
   case gmoorder_Q:
   case gmoorder_NL:
   {
      int max_codelen = gmoNLCodeSizeMaxRow(gmo) + 1;

      assert(max_codelen > 0);

      struct ctrmem working_mem = {.ptr = NULL, .ctr = ctr};
      A_CHECK(working_mem.ptr, ctr_getmem_old(ctr, max_codelen*2*sizeof(int)));
      *instrs = (int *)working_mem.ptr;
      *args = &(*instrs)[max_codelen];

      int *linstrs = *instrs;
      int *largs = *args;

      GMSCHK(gmoDirtyGetRowFNLInstr(gmo, ei, codelen, linstrs, largs));

      break;
   }
   case gmoorder_L:
      *codelen = 0;
      break;
   case gmoorder_ERR:
      error("%s :: an error occurred when probing for the type of equation '%s'\n",
            __func__, ctr_printequname(ctr, ei));
      return Error_GamsCallFailed;

   default:
      error("%s :: wrong return code %d from gmoGetEquOrderOne when probing "
            "for the type of equation '%s'\n", __func__, nlflag,
            ctr_printequname(ctr, ei));
      return Error_GamsCallFailed;
   }

   return OK;
}

/**
 * @brief Generate the GAMS opcode for an equation
 *
 * This function either generates the opcode from the expression  tree or gets
 * it from the original GMO object
 *
 * @param       ctr      the container 
 * @param       ei       the equation index in the uncompressed space
 * @param[out]  codelen  the number of instructions
 * @param[out]  instrs   the instructions
 * @param[out]  args     the arguments
 *
 * @return               the error code
 */
int gctr_genopcode(Container *ctr, rhp_idx ei, int *codelen, int **instrs, int **args)
{
   assert(ctr_is_rhp(ctr) && ei < rctr_totalm(ctr));

   /* ----------------------------------------------------------------------
    * We assume that the container object is the parent of the GAMS container
    * object. Whence, if the index of the equation is within the range of the
    * original container, we generate/get the opcode from that object. Otherwise
    * the opcode is generated from the parent ctr
    * ---------------------------------------------------------------------- */

   rhp_idx ei_up = cdat_ei_upstream(ctr->data, ei);
   if (valid_ei(ei_up)) {
      Container *ctr_up = ctr->ctr_up;

      switch (ctr_up->backend) {

         /* ---------------------------------------------------------------------
          * If the ancestor is GAMS, then we directly get the opcode and we
          * just have to transform it
          * --------------------------------------------------------------------- */

         case RhpBackendGamsGmo:
            {
               S_CHECK(gctr_getopcode(ctr_up, ei_up, codelen, instrs, args));

               /* No need to free instrs or args, it comes from a container workspace */
               break;
            }

         /* ----------------------------------------------------------------
          * The ctr_up is here just use for getting memory, so that's the one
          * passed as argument
          * \TODO(xhub) be careful with RESHOP_MODEL_JULIA, we may switch to an
          * on-demand expression tree
          * ---------------------------------------------------------------- */

         case RhpBackendReSHOP:
         case RhpBackendJulia: {

            Equ *e = &ctr_up->equs[ei_up];
            S_CHECK(rctr_getnl(ctr_up, e));
            if (e->tree && e->tree->root) {
               S_CHECK(nltree_buildopcode(ctr_up, e, instrs, args, codelen));
            } else {
               *codelen = 0;
            }
            break;
         }
         default:
            error("%s :: unsupported container %s (%d)\n", __func__,
                  backend2str(ctr_up->backend), ctr_up->backend);
      }

   } else {

      /* -------------------------------------------------------------------
       * This is a new equation: if there is an expression tree, build the opcode
       * The ctr_up is here just use for getting memory, so that's the one
       * passed as argument. parent_ctr is a const struct
       * ------------------------------------------------------------------- */

      Equ *e = &ctr->equs[ei];
      if (e->tree && e->tree->root) {
         /* TODO(Xhub) fix this horrible thing: we just need the ctr for the memory */
         // HACK ARENA
         S_CHECK(nltree_buildopcode(ctr, e, instrs, args, codelen));
      } else {
         *codelen = 0;
      }

   }

   return OK;
}



int gctr_getsense(const Container *ctr, RhpSense *objsense)
{
   const GmsContainerData *gms = ctr->data;
   int sense = gmoSense(gms->gmo);

  /* ----------------------------------------------------------------------
   * As of Jan 12th, 2024, gmoSense is not a reliable way to get the sense
   * for MCP/EMP models. Other models like CNS could be affected ...
   * ---------------------------------------------------------------------- */

   int gams_modeltype = gmoModelType(gms->gmo);
   if (gams_modeltype == gmoProc_mcp) {
      *objsense = RhpFeasibility;
      return OK;
   }

   if (gams_modeltype == gmoProc_emp) {
      int var = gmoObjVar(gms->gmo);

      if (var < 0 || var == gmoValNAInt(gms->gmo)) {
         *objsense = RhpFeasibility;
         return OK;
      }
   }
    
   if (sense == gmoObj_Min) {
      (*objsense) = RhpMin;
      return OK;
   }

   if (sense == gmoObj_Max) {
      (*objsense) = RhpMax;
      return OK;
   }

   (*objsense) = RhpNoSense;
   return Error_NotFound;
}
