#include "compat.h"
#include "reshop_config.h"

#include <stdio.h>
#ifdef _WIN32
#include <io.h>
#define access _access
#define R_OK 4
#else
#include <unistd.h>
#endif

#include "checks.h"
#include "ctrdat_gams.h"
#include "embcode_empinfo.h"
#include "gams_empinfofile_reader.h"
#include "gams_libver.h"
#include "macros.h"
#include "mdl.h"
#include "mdl_gams.h"
#include "printout.h"
#include "reshop-gams.h"
#include "rhp_fwd.h"
#include "rhp_options.h"

/**
 * @brief Load the GAMS libraries (GMO, GEC, DCT, OPT, CFG)
 *
 * @param sysdir   the GAMS sysdir
 *
 * @return         the error code
 */
int rhp_gms_loadlibs(const char* sysdir)
{
   S_CHECK(chk_arg_nonnull(sysdir, 1, __func__));

   return gams_load_libs(sysdir); 
}

/**
 * @brief Interpret the empinfo file and file the empinfo struct
 *
 * @param mdl    the model
 * @param fname  If non-NULL, the filename of the empinfo file. Otherwise the
 *               value of the empinfofile option is used.
 *               
 * @return       the error code
 */
int rhp_gms_readempinfo(Model *mdl, const char *fname)
{
   char empinfofile[PATH_MAX], gmdfile[PATH_MAX], scrext[GMS_SSSIZE];

   S_CHECK(gams_chk_mdl(mdl, __func__));

   Container *ctr = &mdl->ctr;
   const GmsContainerData *gms = (const GmsContainerData*)ctr->data;
   bool has_embrhp_gmdfile = false;


  /* ----------------------------------------------------------------------
   * If the argument fname is provided, then we use it. Otherwise,
   * else use SCRDIR/empinfo.dat
   * ---------------------------------------------------------------------- */

   if (!fname) {

      gevGetStrOpt(gms->gev, gevNameScrDir, empinfofile);
      gevGetStrOpt(gms->gev, gevNameScrExt, scrext);

      size_t scrdirlen = strlen(empinfofile);
      const char *empinfofile_opt = optvals(mdl, Options_EMPInfoFile);

      bool absolute_path;
#ifdef _WIN32
      /* TODO: This is quite an assumption, room for improvement */
      char c1 = empinfofile_opt[0], c2 = empinfofile_opt[1];
      absolute_path = ((c1 >= 'A' && c1 <= 'Z') || (c1 >= 'a' && c1 <= 'z')) && c2 == ':';
#else
      absolute_path = empinfofile_opt[0] == '/';
#endif

      if (absolute_path) {
         strncpy(empinfofile, empinfofile_opt, PATH_MAX-1);
      } else {
         empinfofile[scrdirlen] = '\0';
         if (scrdirlen + strlen(empinfofile_opt) > sizeof(empinfofile)) {
            error("[empinterp] ERROR: path '%s%s' is too long for this OS\n",
                  empinfofile, empinfofile_opt);
            return Error_SystemError;
         }
         strcat(empinfofile, empinfofile_opt);
      }

      logger(PO_V, "[empinterp] Using option EMPinfo='%s' for EMPinfo file\n", empinfofile);
      FREE(empinfofile_opt);

      if (scrdirlen + strlen(EMBCODE_GMDOUT_FNAME) >= sizeof(gmdfile)) {
         error("[empinterp] ERROR: path '%s%s' is too long for this OS\n",
               gmdfile, EMBCODE_GMDOUT_FNAME);
         return Error_SystemError;
      }
      memcpy(gmdfile, empinfofile, scrdirlen * sizeof(char));
      strcpy(&gmdfile[scrdirlen], EMBCODE_GMDOUT_FNAME);

      has_embrhp_gmdfile = access(gmdfile, R_OK) == 0;
      if (has_embrhp_gmdfile) {
         logger(PO_V, "[empinterp] Using GDX file '%s'\n", gmdfile);
      }

  /* ----------------------------------------------------------------------
   * TODO: implement looking for empinfo.%gams.scrext% and gdx_gms.%gams.scrext%
   * GG #8
   * ---------------------------------------------------------------------- */



   } else {
      logger(PO_V, "[empinterp] Using argument '%s' for EMPinfo file\n", fname);
      strncpy(empinfofile, fname, PATH_MAX-1);
   }

    if (access(empinfofile, R_OK) == -1) {

      if (fname) {
         error("[empinfo] ERROR: No EMP file named '%s' found.\n", empinfofile);
         return Error_FileOpenFailed;
      }

      debug("[GAMS] No EMPinfo file named '%s' found\n", empinfofile);

      return OK;
   }


   EmpInfo *empinfo = &mdl->empinfo;
   S_CHECK(empinfo_alloc(empinfo, mdl));

   /* ---------------------------------------------------------------------
    * Processing the EMPinfo file
    * --------------------------------------------------------------------- */

   S_CHECK(empinterp_process(mdl, empinfofile, has_embrhp_gmdfile ? gmdfile : NULL));

   return OK;
}

NONNULL static int check_gmshandles(struct rhp_gams_handles *gmsh)
{
   if (!gmsh->gh) {
      errormsg("[GAMS] ERROR: NULL gmo object!\n");
      return Error_NullPointer;
   }
   if (!gmsh->eh) {
      errormsg("[GAMS] ERROR: NULL gev object!\n");
      return Error_NullPointer;
   }
   if (!gmsh->dh) {
      errormsg("[GAMS] ERROR: NULL dict object!\n");
      return Error_NullPointer;
   }
   if (!gmsh->ch) {
      errormsg("[GAMS] ERROR: NULL cfg object!\n");
      return Error_NullPointer;
   }

   return OK;
}

/**
 * @brief initialize the internal data of the GAMS model
 *
 * @param mdl   the model to initialize
 * @param gmsh  the gams data
 *
 * @return     the error code
 */
int rhp_gms_fillgmshandles(Model *mdl, struct rhp_gams_handles *gmsh)
{
   S_CHECK(chk_gmdl(mdl, __func__));
   GmsModelData *mdldat = mdl->data;
   GmsContainerData *gms = mdl->ctr.data;

   S_CHECK(check_gmshandles(gmsh));

   gms->gmo = gmsh->gh;
   gms->gev = gmsh->eh;
   gms->dct = gmsh->dh;
   gms->cfg = gmsh->ch;

   gevGetStrOpt(gms->gev, gevNameSysDir, mdldat->gamsdir);
   gevGetStrOpt(gms->gev, gevNameScrDir, mdldat->scrdir);

   gms->owning_handles = false;
   gms->owndct = false;
   gms->initialized = true;

   MALLOC_(gms->rhsdelta, double, gmoM(gmsh->gh)+1);
   memset(&gms->equvar_eval, 0, sizeof(gms->equvar_eval));
   gms->sos_group = NULL;

   /* Set some default just for GAMS:
    * - MCF likes the see the PATH output
    */
   O_Output_Subsolver_Log = 1;

   gmoIndexBaseSet(gms->gmo, 0);

   return OK;
}

int rhp_rc2gmosolvestat(int rc)
{
   /* TODO(GAMS review) When should one use gmoModelStat_ErrorUnknown? */

   switch (rc) {
   case Error_EMPIncorrectInput:
   case Error_EMPIncorrectSyntax:
   case Error_IncompleteModelMetadata:
      return gmoSolveStat_SetupErr;
   case Error_NotImplemented:
      return gmoSolveStat_Solver;
   default:
      return gmoSolveStat_InternalErr;
   }
}


void rhp_printrcmsg(int rc, void *gev)
{
   gevHandle_t gev_ = gev;
   char msg[GMS_SSSIZE];

   switch (rc) {
   case Error_BugPleaseReport:
      (void)snprintf(msg, sizeof msg, "*** ReSHOP ERROR: The model triggered a bug in ReSHOP. "
                     "Please consider filing a bug report. Thanks!");
      gevLogStat(gev_, msg);
      break;
   default: ;
   }
}
