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

   DWORD ret = GetEnvironmentVariableA("PATH", pathval, bufsize), err;

   if (ret == 0) {
_err1:
      err = GetLastError();
      if (err == ERROR_ENVVAR_NOT_FOUND) {
         error("%s ERROR: could not get the PATH environment variable!\n", __func__);
      } else {
         error("%s RUNTIME ERROR (code = %lu)",__func__, err);
      }
      return Error_SystemError;

   } else if (ret > bufsize) {
      bufsize = ret;
      REALLOC_(pathval, char, bufsize);
      ret = GetEnvironmentVariableA("PATH", pathval, bufsize);

      if (!ret) { goto _err1; }
   }

#else /* _WIN32 */
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
      A_CHECK(working_mem.ptr, ctr_getmem_old(ctr, new_pathsize*sizeof(char)));
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

#if defined(_WIN32)
   FREE(pathval);
   if (!SetEnvironmentVariableA("PATH", new_path)) {
      error("%s ERROR: EnvironmentVariable failed (%lu)\n", __func__, GetLastError());
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

   if (ctr->pool) { pool_release(ctr->pool); }

   if (size == 0) {
      A_CHECK(ctr->pool, pool_new_gams());
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

/**
 * @brief Load a model from a ReSHOP control file
 *
 * @warning This assumes that the model is a reshop model.
 * It will try to load the empinfo and reshop options
 *
 * @ingroup publicAPI
 *
 * @param       cntrfile  the GAMS control file 
 * @param[out]  mdlout    the model
 *
 * @return           the error code
 */
int rhp_gms_newfromcntr(const char *cntrfile, Model **mdlout)
{
   int status = OK;
   char buffer[2048];

   *mdlout = NULL;
   S_CHECK(chk_arg_nonnull(cntrfile, 1, __func__));
   
   FILE* fptr = fopen(cntrfile, RHP_READ_TEXT);
   if (!fptr) {
      error("[GAMS] ERROR: couldn't open control file '%s'\n", cntrfile);
      return Error_RuntimeError;
   }

   /* GAMSDIR seems to be on the 29th line */
   for (unsigned i = 0; i < 29; ++i) {
      if (!fgets(buffer, sizeof buffer, fptr)) {
         error("[GAMS] ERROR: failed to get %u-th line of control file '%s'\n",
               i, cntrfile);
         IO_CALL(fclose(fptr));
         return Error_RuntimeError;
      }
   }

   IO_CALL(fclose(fptr));

   size_t len = strlen(buffer);
   if (len <= 1) {
      error("[GAMS] ERROR: bogus gamsdir '%s' from control file '%s'\n", buffer,
            cntrfile);
      return Error_RuntimeError;
   }

   /* fgets keeps the newline character in, remove it */
   trim_newline(buffer, len);

   Model *mdl;
   A_CHECK(mdl, mdl_new(RhpBackendGamsGmo));

   S_CHECK_EXIT(rhp_gms_setgamsdir(mdl, buffer));

   S_CHECK_EXIT(rhp_gms_loadlibs(buffer));

   S_CHECK_EXIT(rhp_gms_setgamscntr(mdl, cntrfile));

   S_CHECK_EXIT(gcdat_loadmdl(mdl->ctr.data, mdl->data));

   rhp_gms_set_gamsprintops(mdl);

   /* Print the reshop banner, after setting the printops */
   rhp_print_banner();

   *mdlout = mdl;

   S_CHECK(gmdl_loadrhpoptions(mdl));

   S_CHECK(rhp_gms_fillmdl(mdl));

   S_CHECK(rhp_gms_readempinfo(mdl, NULL));

   S_CHECK(mdl_check(mdl));

   return OK;

_exit:
   mdl_release(mdl);
   return status;
}

/** @brief set the gams control file for a given model
 *
 *  @ingroup publicAPI
 *
 *  @param mdl    the model
 *  @param cntrfile  the gams control file
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
 *  @ingroup publicAPI
 *
 *  @param mdl     the model
 *  @param gamsdir the gams system directory
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
 *  @ingroup publicAPI
 *
 *  @param  cntrfile  the gams control file
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
 * @ingroup publicAPI
 *
 *  @param gamsdir the gams system directory
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

/**
 * @brief Write the GMO solution to a GDX file
 *
 * @ingroup publicAPI
 *
 * @param mdl       the GAMS model
 * @param gdxname   the GDX file name
 *
 * @return         the error code
 */
int rhp_gms_writesol2gdx(Model *mdl, const char *gdxname) 
{
   S_CHECK(gams_chk_mdlfull(mdl, __func__));
   S_CHECK(gams_chk_str(gdxname, __func__));

   return gmdl_writesol2gdx(mdl, gdxname);
}

#define LOGMASK         0x1
#define STATUSMASK      0x2
#define ALLMASK         (LOGMASK | STATUSMASK)

/** @brief Print out log or status message to the screen.
 *         It strips off one new line if it exists.
 *
 *  @param  env          ReSHOP GAMS record as an opaque object
 *  @param  reshop_mode  mode indicating log, status, or both.
 *  @param  str          the actual string
 */
static void gamsprint(void* env, UNUSED unsigned reshop_mode, const char *str)
{
   gevHandle_t gev = (gevHandle_t)env;
   if (!gev) { return; }

   /* TODO(Xhub) support status and all ...  */
   int mode = LOGMASK;

   switch (mode & ALLMASK) {
   case LOGMASK:
      gevLogPChar(gev, str);
      break;
   case STATUSMASK:
      gevStatPChar(gev, str);
      break;
   case ALLMASK:
      gevLogStatPChar(gev, str);
      break;
   }

}

/**
 * @brief Flush the output streams
 *
 * @param env  ReSHOP GAMS record as an opaque object
 */
static void gamsflush(void* env)
{
   gevHandle_t gev = (gevHandle_t)env;
   gevLogStatFlush(gev);

}

/* --------------------------------------------------------------------------
 * What follows is defined in reshop-gams.h, hence not part of the public API
 * -------------------------------------------------------------------------- */
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

   if (gmoIndexBase(gmo) != 0) {
      error("[GAMS/GMO] ERROR: GMO index base should be 0, got %d\n", gmoIndexBase(gmo));
      return Error_GamsIncompleteSetupInfo;
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
   S_CHECK(mdl_setname(mdl, buffer));

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

   mdl->commondata.exports_dir_parent = exportdir_tmp;
   mdl->commondata.own_exports_dir_parent = true;
 
   S_CHECK(mdl_settype(mdl, mdltype_from_gams(gmoModelType(gmo))));

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
      A_CHECK(lequ, lequ_new(nz - nlnz));
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

int rhp_gms_set_gamsprintops(Model *mdl)
{
   S_CHECK(gams_chk_mdlfull(mdl, __func__));

   GmsContainerData *gms = mdl->ctr.data;

   if (!gms->gev) {
      error("%s ERROR: GEV object is NULL!", __func__);
      return Error_GamsIncompleteSetupInfo;
   }

   rhp_set_printops(gms->gev, gamsprint, gamsflush, false);

   return OK;
}

void* rhp_gms_getgmo(struct rhp_mdl *mdl)
{
   SN_CHECK(gams_chk_mdlfull(mdl, __func__));

   GmsContainerData *gms = mdl->ctr.data;

   return gms->gmo;
}

void* rhp_gms_getgev(struct rhp_mdl *mdl)
{
   SN_CHECK(gams_chk_mdlfull(mdl, __func__));

   GmsContainerData *gms = mdl->ctr.data;

   return gms->gev;
}

const char* rhp_gms_getsysdir(struct rhp_mdl *mdl)
{
   SN_CHECK(gams_chk_mdlfull(mdl, __func__));
   GmsModelData *gmdldat = mdl->data;

   return gmdldat->gamsdir;
}

int rhp_gms_set_solvelink(struct rhp_mdl *mdl, unsigned solvelink)
{
   S_CHECK(gams_chk_mdlfull(mdl, __func__));
   GmsContainerData *gms = mdl->ctr.data;

   switch(solvelink) {
   case gevSolveLinkCallScript:
   case gevSolveLinkCallModule:
   case gevSolveLinkAsyncGrid:
   case gevSolveLinkAsyncSimulate:
   case gevSolveLinkLoadLibrary:
      break;
   default:
      error("[GAMS] ERROR: cannot set solvelink to value %u\n", solvelink);
      return Error_InvalidArgument;
   }

   gms->solvelink = solvelink;
   return OK;
}
