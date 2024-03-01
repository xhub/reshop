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
   char fullpath[PATH_MAX];

   S_CHECK(gams_chk_mdl(mdl, __func__));

   Container *ctr = &mdl->ctr;
   const GmsContainerData *gms = (const GmsContainerData*)ctr->data;


   if (!fname) {
      const char *empinfofile = optvals(mdl, Options_EMPInfoFile);
      bool absolute_path;
#ifdef _WIN32
      /* TODO: This is quite an assumption, room for improvement */
      absolute_path = (empinfofile[0] >= 'A' && empinfofile[0] <= 'Z') && empinfofile[1] == ':';
#else
      absolute_path = empinfofile[0] == '/';
#endif

      if (absolute_path) {
         strncpy(fullpath, empinfofile, PATH_MAX-1);
      } else {
         gevGetStrOpt(gms->gev, gevNameScrDir, fullpath);
         strncat(fullpath, empinfofile, sizeof(fullpath) - strlen(fullpath) - 1);
      }
      FREE(empinfofile);
   } else {
      strncpy(fullpath, fname, PATH_MAX-1);
   }

    if (access(fullpath, R_OK) == -1) {

      /* TODO: error also if the option has been set */
      if (fname) {
         error("[empinfo] ERROR: No EMP file named '%s' found.\n", fullpath);
         return Error_FileOpenFailed;
      }

      debug("[GAMS] No EMPinfo file named '%s' found\n", fullpath);

      return OK;
   }

   EmpInfo *empinfo = &mdl->empinfo;
   S_CHECK(empinfo_alloc(empinfo, mdl));

   /* ---------------------------------------------------------------------
    * Parse the EMP tokens
    * --------------------------------------------------------------------- */

   S_CHECK(empinterp_process(mdl, fullpath));

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
   GmsModelData *mdldat = mdl->data;
   GmsContainerData *gms = mdl->ctr.data;

   gms->gmo = gmsh->gh;
   gms->gev = gmsh->eh;
   gms->dct = gmsh->dh;
   gms->cfg = gmsh->ch;

   gevGetStrOpt(gms->gev, gevNameSysDir, mdldat->gamsdir);
   gevGetStrOpt(gms->gev, gevNameScrDir, mdldat->scrdir);

   mdldat->delete_scratch = false;

   gms->owning_handles = false;
   gms->owndct = false;

   gms->initialized = true;

   MALLOC_(gms->rhsdelta, double, gmoM(gmsh->gh)+1);

   /* Set some default just for GAMS:
    * - MCF likes the see the PATH output
    */
   O_Output_Subsolver_Log = 1;

   return OK;
}
