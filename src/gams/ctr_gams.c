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
   if (ctr->backend != RHP_BACKEND_GAMS_GMO) {
      error("%s :: the container.has the wrong type: expected %s "
                         "(id %d), got %s (id %d).\n", fn,
                         backend_name(RHP_BACKEND_GAMS_GMO), RHP_BACKEND_GAMS_GMO,
                         backend_name(ctr->backend), ctr->backend);
      return Error_InvalidValue;
   }

   return gams_chk_ctrdata(ctr, fn);
}

static int gcdat_new(GmsContainerData * restrict gms, GmsModelData * restrict mdldat)
{
   char buffer[GMS_SSSIZE];

   /* ---------------------------------------------------------------------
    * Fail if gamsdir has not already being given
    * --------------------------------------------------------------------- */

   if (!strlen(mdldat->gamsdir)) {
      errormsg("[GAMS] ERROR: no GAMS sysdir was given, unable to continue!\n"
               "Use gams_setgamsdir to set the GAMS sysdir\n");
      return Error_RuntimeError;
   }

   /* Load all the libs */
   S_CHECK(gams_load_libs(mdldat->gamsdir));

   /* ---------------------------------------------------------------------
    * Initialize GMO and GEV libraries.
    * --------------------------------------------------------------------- */

   if (!gmoCreateDD(&gms->gmo, mdldat->gamsdir, buffer, sizeof(buffer))
       || !gevCreateDD(&gms->gev, mdldat->gamsdir, buffer, sizeof(buffer))) {
      error("[GAMS] ERROR: loading GMO or GEV failed with message '%s'\n", 
            buffer);
      return Error_RuntimeError;
   }

   /* ---------------------------------------------------------------------
    * Load control file.
    *
    *  gevCompleteEnvironment(ctr_rmdl->gev, NULL, NULL, NULL, NULL);
    * --------------------------------------------------------------------- */

   if (!strlen(mdldat->gamscntr)) {
      errormsg("[GAMS] ERROR: the control file is empty\n");
      return Error_GAMSIncompleteSetupInfo;
   }

   if (gevInitEnvironmentLegacy(gms->gev, mdldat->gamscntr)) {
      error("[GAMS] ERROR: loading control file '%s' failed\n",
               mdldat->gamscntr);
      return Error_GAMSCallFailed;
   }

   /* ---------------------------------------------------------------------
    * Register GAMS environment.
    * --------------------------------------------------------------------- */

   if (gmoRegisterEnvironment(gms->gmo, gms->gev, buffer)) {
      error("[GAMS] ERROR: registering GAMS environment failed with error '%s'\n",
            buffer);
      return Error_GAMSCallFailed;
   }

   /* ---------------------------------------------------------------------
    * Create a configuration object.
    * --------------------------------------------------------------------- */

   if (!cfgCreateD(&gms->cfg, mdldat->gamsdir, buffer, sizeof(buffer))) {
      error("[GAMS] ERROR: creating cfg object failed with message '%s'\n",
            buffer);
      return Error_GAMSCallFailed;
   }

   /* ---------------------------------------------------------------------
    * Read the configuration file.
    * --------------------------------------------------------------------- */

   /* TODO: this could not work with gamsconfig.yaml */

   size_t len = strlen(mdldat->gamsdir);
   size_t dirsep_len = strlen(DIRSEP);
   bool has_dirsep = true;

   if (len >= dirsep_len) {
      assert(dirsep_len > 0);
      len -= 1;
      dirsep_len -= 1;
      for (size_t i = 0; i <= dirsep_len; ++i) {
         if (mdldat->gamsdir[len-i] != DIRSEP[dirsep_len-i]) {
            has_dirsep = false;
            break;
         }
      }
   }

   /* TODO: GAMS review*/

   size_t slen = len + dirsep_len + strlen(GMS_CONFIG_FILE);
   if (slen > sizeof(buffer)-1) {
      error("[GAMS] ERROR: filename '%s%s%s' has size %zu, max is %zu",
            mdldat->gamsdir, DIRSEP, GMS_CONFIG_FILE, slen, sizeof(buffer)-1);
      return Error_NameTooLong4Gams;
   }

   strcpy(buffer, mdldat->gamsdir);
   if (!has_dirsep) {
      strcat(buffer, DIRSEP);
   }
   strcat(buffer, GMS_CONFIG_FILE);

   if (cfgReadConfig(gms->cfg, buffer)) {
      error("[GAMS] ERROR: could not read configuration file %s\n", buffer);
      return Error_GAMSCallFailed;
   }

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
int gcdat_init(GmsContainerData * restrict gms, GmsModelData * restrict mdldat)
{
   char buffer[GMS_SSSIZE];

   S_CHECK(gcdat_new(gms, mdldat));

   GAMS_CHECK1(dctCreate, &gms->dct, buffer);
   gms->owndct = true;


   trace_stack("[GAMS] Successful initialization GAMS model with "
               "gamsdir='%s'; gamscntr='%s'\n", mdldat->gamsdir, mdldat->gamscntr);

   return OK;
}

int gcdat_loadmdl(GmsContainerData * restrict gms, GmsModelData * restrict mdldat)
{
   char buffer[GMS_SSSIZE];

   S_CHECK(gcdat_new(gms, mdldat));

   if (gmoLoadDataLegacy(gms->gmo, buffer)) {
      error("[GAMS] ERROR: Loading model data failed with message '%s'\n",
            buffer);
      return Error_GAMSCallFailed;
   }

   gms->dct = gmoDict(gms->gmo);
   gevGetStrOpt(gms->gev, gevNameScrDir, mdldat->scrdir);

   gmoNameModel(gms->gmo, buffer);
   trace_process("[GAMS] Loaded GMO model named '%s' with %u vars and %u equs "
                 "from %s\n", buffer, gmoN(gms->gmo), gmoM(gms->gmo),
                 mdldat->gamscntr);

   return OK;
}

int gams_load_libs(const char *sysdir)
{
   char msg[GMS_SSSIZE];

   if (!gmoLibraryLoaded() && !gmoGetReadyD(sysdir, msg, sizeof(msg))) {
      error("%s\n", msg);
      return 1;
   }
   CHK_CORRECT_LIBVER(gmo, GMO, msg);

   if (!gevLibraryLoaded() && !gevGetReadyD(sysdir, msg, sizeof(msg))) {
      error("%s\n", msg);
      return 1;
   }
   CHK_CORRECT_LIBVER(gev, GEV, msg);

   if (!dctLibraryLoaded() && !dctGetReadyD(sysdir, msg, sizeof(msg))) {
      error("%s\n", msg);
      return 1;
   }
   CHK_CORRECT_LIBVER(dct, DCT, msg);

   if (!cfgLibraryLoaded() && !cfgGetReadyD(sysdir, msg, sizeof(msg))) {
      error("%s\n", msg);
      return 1;
   }
   CHK_CORRECT_LIBVER(cfg, CFG, msg);

   if (!optLibraryLoaded() && !optGetReadyD(sysdir, msg, sizeof(msg))) {
      error("%s\n", msg);
      return 1;
   }
   CHK_CORRECT_LIBVER(opt, OPT, msg);

   if (!gdxLibraryLoaded() && !gdxGetReadyD(sysdir, msg, sizeof(msg))) {
      error("%s\n", msg);
      return 1;
   }
   CHK_CORRECT_LIBVER(gdx, GDX, msg);

   return OK;
}

void gams_unload_libs(void)
{
   if (cfgLibraryLoaded()) cfgLibraryUnload();
   if (dctLibraryLoaded()) dctLibraryUnload();
   if (gdxLibraryLoaded()) gdxLibraryUnload();
   if (gevLibraryLoaded()) gevLibraryUnload();
   if (gmoLibraryLoaded()) gmoLibraryUnload();
   if (optLibraryLoaded()) optLibraryUnload();
}

void gctrdata_rel(GmsContainerData *gms)
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
}

/**
 * @brief Get the opcode (from the GMO object) for an equation
 *
 * @warning On output, the memory array used for instrs and args
 *          is temporary (from the container workspace). Do immediately
 *          consume it or copy it to a long-term memory
 *
 * @param      ctr      the model
 * @param      eidx     the equation index
 * @param[out] codelen  the length of the opcode
 * @param[out] instrs   the instructions
 * @param[out] args     the arguments
 *
 * @return              the error code
 */
int gctr_getopcode(Container *ctr, rhp_idx ei, int *codelen, int **instrs, int **args)
{
   assert(ctr->backend == RHP_BACKEND_GAMS_GMO);
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

      struct ctrmem CTRMEM working_mem = {.ptr = NULL, .ctr = ctr};
      A_CHECK(working_mem.ptr, ctr_getmem(ctr, max_codelen*2*sizeof(int)));
      *instrs = (int *)working_mem.ptr;
      *args = &(*instrs)[max_codelen];

      int *linstrs = *instrs;
      int *largs = *args;

      GAMS_CHECK(gmoDirtyGetRowFNLInstr(gmo, ei, codelen, linstrs, largs));

      break;
   }
   case gmoorder_L:
      *codelen = 0;
      break;
   case gmoorder_ERR:
      error("%s :: an error occurred when probing for the type of equation '%s'\n",
            __func__, ctr_printequname(ctr, ei));
      return Error_GAMSCallFailed;

   default:
      error("%s :: wrong return code %d from gmoGetEquOrderOne when probing "
            "for the type of equation '%s'\n", __func__, nlflag,
            ctr_printequname(ctr, ei));
      return Error_GAMSCallFailed;
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

   rhp_idx ei_up = cdat_equ_inherited(ctr->data, ei);
   if (valid_ei(ei_up)) {
      Container *ctr_up = ctr->ctr_up;

      switch (ctr_up->backend) {

         /* ---------------------------------------------------------------------
          * If the ancestor is GAMS, then we directly get the opcode and we
          * just have to transform it
          * --------------------------------------------------------------------- */

         case RHP_BACKEND_GAMS_GMO:
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

         case RHP_BACKEND_RHP:
         case RHP_BACKEND_JULIA: {

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
                  backend_name(ctr_up->backend), ctr_up->backend);
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
