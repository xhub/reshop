#include "reshop_config.h"
#include "asprintf.h"

#include <sys/stat.h>

#include "checks.h"
#include "compat.h"
#include "ctrdat_gams.h"
#include "equ.h"
#include "fs_func.h"
#include "gams_rosetta.h"
#include "gams_utils.h"
#include "lequ.h"
#include "macros.h"
#include "mdl.h"
#include "mdl_gams.h"
#include "pool.h"
#include "printout.h"
#include "reshop-gams.h"
#include "rhp_fwd.h"
#include "rhp_options.h"
#include "sys_utils.h"
#include "var.h"

#ifdef _WIN32
#define WIN32_LEAN_AND_MEAN
#define VC_EXTRALEAN
#include <windows.h>
#include <processenv.h>
#endif


static int adddir2PATH(const char *dir, Model *mdl)
{
#ifdef _WIN32
   DWORD bufsize = 4096;
   char *pathval;
   MALLOC_(pathval, char, bufsize);

   DWORD ret = GetEnvironmentVariableA("PATH", pathval, bufsize);

   if (ret == 0) {
_err1:
      DWORD err = GetLastError();
      if (err == ERROR_ENVVAR_NOT_FOUND) {
         error("%s ERROR: could not get the PATH environment variable!\n", __func__);
      } else {
         error("%s RUNTIME ERROR (code = %u)", err);
      }
      return Error_SystemError;
   } else if (ret > bufsize) {
      bufsize = ret;
      REALLOC_(pathval, char, bufsize);
      ret = GetEnvironmentVariableA("PATH", pathval, bufsize);

      if (!ret) { goto _err1; }
   }


#if 0
      char*  envStrings = GetEnvironmentStringsA();
      if (!envStrings) {
         error("%s ERROR: GetEnvironmentStringsA() failed with error code %lu\n", GetLastError());
      } else {
         errormsg("\tList of environment variables:\n");
         while (*envStrings) {
            error("\t%s\n", envStrings);
            envStrings += strlen(envStrings) + 1;
         }
         FreeEnvironmentStringsA(envStrings);
      }
      return Error_SystemError;
#endif

#else
   const char* pathval = getenv("PATH");
   if (!pathval) {
      error("%s ERROR: could not get the PATH environment variable!\n", __func__);

      return Error_SystemError;
   }
#endif

   char *new_path;
   Container *ctr = mdl ? &mdl->ctr : NULL;
   struct ctrmem CTRMEM working_mem = {.ptr = NULL, .ctr = ctr};

   size_t new_pathsize = strlen(pathval) + strlen(dir) + 2;


   if (ctr) {
      A_CHECK(working_mem.ptr, ctr_getmem(ctr, new_pathsize*sizeof(char)));
      new_path = (char*)working_mem.ptr;
   } else {
      MALLOC_(new_path, char, new_pathsize);
   }

   strcpy(new_path, dir);
#ifdef _WIN32
   strcat(new_path, ";");
#else
   strcat(new_path, ":");
#endif
   strcat(new_path, pathval);

#if defined(_WIN32) || defined(_WIN64)
   FREE(pathval);
   if (!SetEnvironmentVariableA("PATH", new_path)) {
       error("%s ERROR: EnvironmentVariable failed (%d)\n", GetLastError());
      return Error_SystemError;
   }
#else
   SYS_CALL(setenv("PATH", new_path, 1));
#endif

   if (!ctr) { FREE(new_path); }

   return OK;
}

/** 
 *  @brief init the pool from a GAMS pool
 *
 *  @param ctr       the GAMS container
 *  @param gms_pool  the array from the GMO object
 *
 *  @return          the error code
 */
static int gams_initpool_from_gmo(Container *ctr, double * restrict gms_pool, int size)
{
   if (size < 0) {
      error("[GAMS] ERROR: constant pool from GMO has negative size %d", size);
      return Error_RuntimeError;
   }

   if (size == 0) {
      A_CHECK(ctr->pool, pool_create_gams());
      return OK;
   }

   if (!gms_pool) {
      error("[GAMS] ERROR: pool array is NULL, but pool size is %d", size);
      return Error_NullPointer;
   }

   /* TODO: this should disappear */
   if (fabs(gms_pool[0] - 1.) > DBL_EPSILON) {
      ctr->pool = NULL;
      return OK;
   }

   A_CHECK(ctr->pool, pool_new());

   ctr->pool->data = gms_pool;
   ctr->pool->len = size;
   ctr->pool->max = size;
   ctr->pool->own = false;
   ctr->pool->type = ctr->backend;

   for (int i = 0; i < size; ++i) {

      /* 2024.01.19: all SV are greater than GMS_SV_UNDEF */
      double val = gms_pool[i];

      if (RHP_LIKELY(val < GMS_SV_UNDEF && val != GMS_SV_NAINT)) { continue; }

      if (val == GMS_SV_MINF) { gms_pool[i] = -INFINITY; continue; }
      if (val == GMS_SV_PINF) { gms_pool[i] = INFINITY; continue; }
      if (val == GMS_SV_EPS) { gms_pool[i] = 0.; continue; }
      if (val == GMS_SV_UNDEF || val == GMS_SV_NAINT || val == GMS_SV_ACR) {
         error("[GAMS] ERROR: constant pool from GMO contains special value %e "
               "at index %d", val, i);
      } else {
         error("[GAMS] ERROR: constant pool from GMO contains erronous value %e "
               "at index %d", val, i);

      }
      return Error_RuntimeError;

   }

   return OK;
}

/** @brief Load the environment of GAMS such as GMO, GEV, and DCT objects.
 *
 *        These objects are used to identify the problem information.
 *
 *  @param gms       the GAMS container
 *  @param filename  the GAMS model file
 *
 *  @return          the error code
 */
static int load_env_from_file(Model *mdl, const char *filename)
{
   /* TODO: should this be TLS?  */
   static size_t readcount = 1;
   char scrdir[GMS_SSSIZE], buffer[2048];
   FILE *fptr;
   int rc;

   GmsModelData *mdldat = mdl->data;
   GmsContainerData *gms = mdl->ctr.data;

   /* ---------------------------------------------------------------------
    * Fail if gamsdir has not already being given
    * --------------------------------------------------------------------- */

   if (!strlen(mdldat->gamsdir)) {
      error("%s :: no GAMS sysdir was given, unable to continue!\n"
                         "Use gams_setgamsdir to set the GAMS sysdir\n",
                         __func__);
      return Error_RuntimeError;
   }


   /* ---------------------------------------------------------------------
    * Create a scratch directory where GAMS will place working files.
    * --------------------------------------------------------------------- */

   sprintf(buffer, "loadgms%p%zu.tmp", (void *) gms, readcount++);
   strcpy(scrdir, buffer);
   if (mkdir(scrdir, S_IRWXU)) {
      perror("mkdir");
      error("%s ERROR: Could not create directory '%s'\n", __func__, scrdir);
      return Error_SystemError;
   }

   /* ---------------------------------------------------------------------
    * Create an empty convertd.opt file in the scratch directory.
    * --------------------------------------------------------------------- */

   strcat(buffer, DIRSEP "convertd.opt");
   fptr = fopen(buffer, "w");
   if (!fptr) {
      error("%s ERROR: failed to open file %s\n", __func__, buffer);
      return Error_FileOpenFailed;
   }
   fprintf(fptr, "\n");
   fclose(fptr);

   /* ---------------------------------------------------------------------
    * Run GAMS using convertd.
    * --------------------------------------------------------------------- */

   strcpy(buffer, mdldat->gamsdir);
   strcat(buffer, DIRSEP "gams ");
   strcat(buffer, filename);
   strcat(buffer, " SOLVER=CONVERTD" " SCRDIR=");
   strcat(buffer, scrdir);
   strcat(buffer, " output=");
   strcat(buffer, scrdir);
   strcat(buffer, DIRSEP "listing optdir=");
   strcat(buffer, scrdir);
   strcat(buffer, " optfile=1 pf4=0 solprint=0 limcol=0 limrow=0 pc=2");

   if (O_Output_Subsolver_Log) {
      strcat(buffer, " lo=1");
   } else {
      strcat(buffer, " lo=0");
   }

   strcat(buffer, " > /dev/null 2>&1");

   rc = system(buffer);
   if (rc) {
      error("%s :: GAMS call on file %s returned with code %d\n", __func__,
               filename, rc);


      rc = rmfn(scrdir);
      if (rc) {
         error("%s :: scrdir %s was not deleted\n", __func__, scrdir);
      }

      return Error_GAMSCallFailed;
   }

   /* -----------------------------------------------------------------------
    * Set the gams control file
    * ----------------------------------------------------------------------- */

   if (strlen(scrdir) + strlen(DIRSEP) + strlen("gamscntr.dat") >= GMS_SSSIZE) {
      error("[GAMS] ERROR setting up the control file: filename '%s%s%s' has length "
            "greater than %u\n", scrdir, DIRSEP, "gamscntr.dat", GMS_SSSIZE);
      return Error_SystemError;
   }

   SNPRINTF_CHECK(mdldat->gamscntr, GMS_SSSIZE, "%s" DIRSEP "gamscntr.dat", scrdir);

   /* -----------------------------------------------------------------------
    * Create all the GAMS objects: GMO, GEV, DCT, CFG from the model
    * ----------------------------------------------------------------------- */

   S_CHECK(gctrdata_init(gms, mdldat, false));

   /* ---------------------------------------------------------------------
    * Restore the scrdir to the original one
    * --------------------------------------------------------------------- */

   if (mdldat->scrdir[0] != '\0') {
      gevSetStrOpt(gms->gev, gevNameScrDir, mdldat->scrdir);
   }

   /* ---------------------------------------------------------------------
    * Create log and status files GAMS will use. Without creating them,
    * GAMS complaints file open error when we run in parallel.
    * --------------------------------------------------------------------- */

   strcpy(mdldat->logname, scrdir);
   strcat(mdldat->logname, DIRSEP "agentlog");

   fptr = fopen(mdldat->logname, "w");
   if (!fptr) {
      error("%s :: log file %s open failed.\n", __func__, mdldat->logname);
      return Error_FileOpenFailed;
   }
   fclose(fptr);

   strcpy(mdldat->statusname, scrdir);
   strcat(mdldat->statusname, DIRSEP "agentstatus");

   fptr = fopen(mdldat->statusname, "w");
   if (!fptr) {
      error("%s :: status file %s open failed.\n", __func__, mdldat->statusname);
      return Error_FileOpenFailed;
   }
   fclose(fptr);

   MALLOC_(gms->rhsdelta, double, gmoM(gms->gmo)+1);

   return OK;
}

struct rhp_mdl *rhp_gms_newfromcntr(const char *cntrfile)
{
   char buffer[2048];
   SN_CHECK(chk_arg_nonnull(cntrfile, 1, __func__));
   
   Model *mdl;
   AA_CHECK(mdl, mdl_new(RHP_BACKEND_GAMS_GMO));

   FILE* fptr = fopen(cntrfile, "r");
   if (!fptr) {
      error("[GAMS] ERROR: couldn't open control file '%s'\n", cntrfile);
      goto _exit;
   }

   /* GAMSDIR seems to be on the 29th line */
   for (unsigned i = 0; i < 29; ++i) {
      if (!fgets(buffer, sizeof buffer, fptr)) {
         error("[GAMS] ERROR: failed to get %u-th line of control file '%s'\n",
               i, cntrfile);
         goto _exit;
      }
   }

   size_t len = strlen(buffer);
   if (len <= 1) {
      error("[GAMS] ERROR: bogus gamsdir '%s' from control file '%s'\n", buffer,
            cntrfile);
      return NULL;
   }

   /* fgets keeps the newline character in, remove it */
   trim_newline(buffer, len);

   SN_CHECK_EXIT(rhp_gms_setgamsdir(mdl, buffer));
   SN_CHECK_EXIT(rhp_gms_setgamscntr(mdl, cntrfile));

   SN_CHECK_EXIT(gctrdata_init(mdl->ctr.data, mdl->data, false));

   SN_CHECK_EXIT(rhp_gms_fillmdl(mdl));

   SN_CHECK_EXIT(rhp_gms_readempinfo(mdl, NULL));

   SN_CHECK_EXIT(mdl_check(mdl));

    
   return mdl;

_exit:
   mdl_release(mdl);
   return NULL;
}

/**
 * @brief Read a GMS file and fill in the container structure.
 *
 * @ingroup publicAPI
 *
 * @param mdl    the model
 * @param fname  the GMS file 
 *
 * @return       the error code
 */
int rhp_gms_readmodel(Model *mdl, const char *gmsfile)
{
   S_CHECK(chk_mdl(mdl, __func__));
   S_CHECK(chk_arg_nonnull(gmsfile, 2, __func__));

   S_CHECK(load_env_from_file(mdl, gmsfile));

   return rhp_gms_fillmdl(mdl);
}

/**
 * @brief Fill the model from a GAMS input
 *
 * @param   mdl  the model
 *
 * @return       the error code
 */
int rhp_gms_fillmdl(Model *mdl)
{
   S_CHECK(gams_chk_mdlfull(mdl, __func__));

   char buffer[GMS_SSSIZE];

   Container *ctr = &mdl->ctr;
   GmsContainerData *gms = mdl->ctr.data;
   gmoHandle_t gmo = gms->gmo;

   if (!gmo) {
      errormsg("[reshop] ERROR: GMO object is NULL!\n");
      return Error_NullPointer;
   }

   /* ---------------------------------------------------------------------
    * Set array index base to 0.
    * --------------------------------------------------------------------- */

   gmoIndexBaseSet(gmo, 0);

   /* ---------------------------------------------------------------------
    * This looks a bit undocumented, but is used in many solvers
    * \TODO(xhub) investigate if we need this and whether we should euse
    * gmoObjStyleSet(gmo, gmoObjType_Var) ?
    * --------------------------------------------------------------------- */

/*    gmoObjStyleSet(gmo, gmoObjType_Fun);
   (void) gmoObjReformSet (gmo, 1);*/

   /* ---------------------------------------------------------------------
    * Init the common data structures: the model name and export dirs
    * --------------------------------------------------------------------- */

   gmoNameModel(gmo, buffer);
   A_CHECK(mdl->commondata.name, strdup(buffer));

   const GmsModelData *mdldat = mdl->data;
   assert(mdldat->scrdir);

  /* ----------------------------------------------------------------------
   * We don't know how unique the gams scrdir is, so we just create a unique one
   * ---------------------------------------------------------------------- */

   char *exportdir_tmp;
   unsigned uint_max_digits = log2(UINT_MAX)+1;
   int tmplen = asprintf(&exportdir_tmp, "%s" DIRSEP "reshop%-*s", mdldat->scrdir,
                         uint_max_digits, "X");
   if (tmplen <= 0 || tmplen <= uint_max_digits) {
      error("%s ERROR: call to asprintf failed\n", __func__);
      return Error_SystemError;
   }


   exportdir_tmp[tmplen-uint_max_digits] = '\0';

   S_CHECK(new_unique_dirname(exportdir_tmp, tmplen));

   if (mkdir(exportdir_tmp, S_IRWXU)) {
      perror("mkdir");
      error("%s ERROR: Could not create directory '%s'\n", __func__, exportdir_tmp);
      return Error_SystemError;
   }

   mdl->commondata.export_dir_parent = exportdir_tmp;
   mdl->commondata.own_export_dir_parent = true;

   unsigned m = (unsigned)gmoM(gmo);
   unsigned n = (unsigned)gmoN(gmo);
   S_CHECK(ctr_resize(ctr, n, m));

   ctr->n = n;
   ctr->m = m;

  /* ----------------------------------------------------------------------
   * We have to circumvent that the pool is not initialized when there is no
   * NL equations (2024.02.22)
   * ---------------------------------------------------------------------- */

   int pool_size = gmoNLConst(gmo);
   if (gmoNLM(gmo) == 0) {
      pool_size = 0;
   }

   S_CHECK(gams_initpool_from_gmo(ctr, gmoPPool(gmo), pool_size));

   double gms_pinf = gmoPinf(gmo);
   double gms_minf = gmoMinf(gmo);
   double gms_na = gmoValNA(gmo);
   double gmsUndef = gmoValUndf(gmo);


   double flip_marginal = gmoSense(gmo) == gmoObj_Max;

   /* ---------------------------------------------------------------------
    * Get information about the variables.
    * --------------------------------------------------------------------- */

   for (int i = 0, len = ctr->n; i < len; i++) {

      ctr->vars[i].idx = (rhp_idx)i;
      ctr->vars[i].basis = basis_from_gams(gmoGetVarStatOne(gmo, i));
      ctr->vars[i].type = gmoGetVarTypeOne(gmo, i);
      ctr->vars[i].bnd.lb = dbl_from_gams(gmoGetVarLowerOne(gmo, i), gms_pinf, gms_minf, gms_na);
      ctr->vars[i].bnd.ub = dbl_from_gams(gmoGetVarUpperOne(gmo, i), gms_pinf, gms_minf, gms_na);
      ctr->vars[i].value = dbl_from_gams(gmoGetVarLOne(gmo, i), gms_pinf, gms_minf, gms_na);


      double marginal = dbl_from_gams(gmoGetVarMOne(gmo, i), gms_pinf, gms_minf, gms_na);

      if (flip_marginal) {
         ctr->vars[i].multiplier = -marginal;
      } else {
         ctr->vars[i].multiplier = marginal;
      }
   }


   /* ---------------------------------------------------------------------
    * Get information about the equations.
    * --------------------------------------------------------------------- */

   for (int i = 0; i < ctr->m; i++) {
      Equ * restrict e = &ctr->equs[i];
      e->idx = (rhp_idx)i;
      e->basis = basis_from_gams(gmoGetEquStatOne(gmo, i));
      int gams_type = gmoGetEquTypeOne(gmo, i);

      EquTypeFromGams equtype;
      S_CHECK(equtype_from_gams(gams_type, &equtype));
      e->cone = equtype.cone;
      e->object = equtype.object;

      double marginal = dbl_from_gams(gmoGetEquMOne(gmo, i), gms_pinf, gms_minf, gms_na);

      if (flip_marginal) {
         e->multiplier = -marginal;
      } else {
         e->multiplier = marginal;
      }

      double equ_rhs = dbl_from_gams(gmoGetRhsOne(gmo, i), gms_pinf, gms_minf, gms_na);
      equ_set_cst(e, -equ_rhs);
      double equ_level = dbl_from_gams(gmoGetEquLOne(gmo, i), gms_pinf, gms_minf, gms_na);
      if (isfinite(equ_level)) {
         e->value = equ_level - equ_rhs;
      }


      /* ------------------------------------------------------------------
       * Get the linear terms. (nz - nlnz) is used to estimate the number
       * of linear terms in row i.
       *
       * The nonlinear terms are not pulled by default, but on-demand, see 
       * gams_getopcode()
       * ------------------------------------------------------------------ */

      int nz, qnz, nlnz;
      gmoGetRowStat(gmo, i, &nz, &qnz, &nlnz);
      assert(nz >= nlnz);

      Lequ *lequ;
      unsigned lsize = nz - nlnz;
      A_CHECK(lequ, lequ_alloc(nz - nlnz));
      e->lequ = lequ;

      if (lsize == 0) { continue; }

      void *jacptr = NULL;
      do {
         int colidx, nlflag;
         double val;
         gmoGetRowJacInfoOne(gmo, i, &jacptr, &val, &colidx, &nlflag);
         assert(colidx >= 0);

         if (!nlflag) {
            S_CHECK(lequ_add(lequ, colidx, val));
         }
      } while (jacptr);

   }

   return OK;
}

/** @brief set the gams control file for a given model
 *
 *  @param mdl    the model
 *  @param fname  the gams control file
 *
 *  @return       the error code
 */
int rhp_gms_setgamscntr(Model *mdl, const char *cntrfile)
{
   S_CHECK(gams_chk_mdl(mdl, __func__));
   S_CHECK(gams_chk_str(cntrfile, __func__));

   trace_stack("[GAMS] %s model '%.*s' #%u: set gamscntr to '%s'\n",
               mdl_fmtargs(mdl), cntrfile);

   GmsModelData *mdldat = (GmsModelData *)mdl->data;

   STRNCPY_FIXED(mdldat->gamscntr, cntrfile);

   return OK;
}

/** @brief set the gams system directory for a given model
 *
 *  @param mdl     the model
 *  @param dirname the gams system directory
 *
 *  @return        the error code
 */
int rhp_gms_setgamsdir(Model *mdl, const char *gamsdir)
{
   S_CHECK(gams_chk_mdl(mdl, __func__));
   S_CHECK(gams_chk_str(gamsdir, __func__));

   trace_stack("[GAMS] %s model '%.*s' #%u: gamsdir set to '%s'\n", mdl_fmtargs(mdl),
               gamsdir);

   GmsModelData *mdldat = (GmsModelData *)mdl->data;

   /* ----------------------------------------------------------------------
    * Sanitize the path (removing trailing separator)
    * ---------------------------------------------------------------------- */

   STRNCPY_FIXED(mdldat->gamsdir, gamsdir);

   /* ----------------------------------------------------------------------
    * Inject this path into the PATH environment variable
    * ---------------------------------------------------------------------- */

   return adddir2PATH(gamsdir, mdl);
}

/** @brief globally set the gams control file 
 *
 *  @param  fname  the gams control file
 *
 *  @return        the error code
 */
int rhp_gams_setglobalgamscntr(const char *cntrfile)
{
   S_CHECK(gams_chk_str(cntrfile, __func__));

   trace_stack("[GAMS] global gamscntr set to '%s'\n", cntrfile);

   return gams_setgamscntr(cntrfile);
}

/** @brief globally set the gams system directory
 *
 *  @param dirname the gams system directory
 *
 *  @return        the error code
 */
int rhp_gams_setglobalgamsdir(const char *gamsdir)
{
   S_CHECK(gams_chk_str(gamsdir, __func__));

   trace_stack("[GAMS] global gamsdir set to '%s'\n", gamsdir);

   S_CHECK(gams_setgamsdir(gamsdir));

   /* ----------------------------------------------------------------------
    * TODO: Sanitize the path (removing trailing separator)?
    * ---------------------------------------------------------------------- */

   /* ----------------------------------------------------------------------
    * Inject this path into the PATH environment variable
    * ---------------------------------------------------------------------- */

   return adddir2PATH(gamsdir, NULL);
}

int rhp_gms_writesol2gdx(Model *mdl, const char *gdxname) 
{
   S_CHECK(gams_chk_mdlfull(mdl, __func__));
   S_CHECK(gams_chk_str(gdxname, __func__));

   return gmdl_writesol2gdx(mdl, gdxname);
}
