#include "checks.h"
#include "container.h"
#include "ctrdat_gams.h"
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

   if (mdl->backend != RHP_BACKEND_GAMS_GMO) {
      error("%s :: Model is of type %s, expected %s", fn,
            backend_name(mdl->backend), backend_name(RHP_BACKEND_GAMS_GMO));
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

   if (mdl->backend != RHP_BACKEND_GAMS_GMO) {
      error("%s ERROR: Model is of type %s, expected %s", fn,
            backend_name(mdl->backend), backend_name(RHP_BACKEND_GAMS_GMO));
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

int gmdl_setprobtype(Model *mdl, enum mdl_probtype probtype)
{
   assert(mdl->backend == RHP_BACKEND_GAMS_GMO);
   Container *ctr = &mdl->ctr;
   const GmsContainerData *gms = ctr->data;

   if (gms->initialized) {
      enum gmoProcType gams_probtype = probtype_to_gams(probtype);
      if (gams_probtype == gmoProc_none) {
         error("[model] ERROR: GAMS does not support modeltype %s\n", probtype_name(probtype));
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
   if (mdl->backend != RHP_BACKEND_GAMS_GMO) {
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

   trace_empdag("[empdag] %s model '%.*s' #%u has now type %s\n", mdl_fmtargs(mdl),
                empdag_typename(empdag->type));

   return OK;
}
