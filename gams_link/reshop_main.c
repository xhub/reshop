#include <assert.h>
#include <stdlib.h>
#include <stdio.h>
#include <string.h>

#include "reshop-gams.h"
#include "reshop_priv.h"

#ifdef _WIN32
#define GMS_CONFIG_FILE "gmscmpNT.txt"
#else
#define GMS_CONFIG_FILE "gmscmpun.txt"
#endif

/** @brief Create a RESHOP object.
 *
 *  @param jh        RESHOP object is stored to jh
 *  @param msgbuf    message buffer to which error string is printed
 *  @param msgbuflen length of the message buffer
 */
void rhpCreate(rhpRec_t **jh, char *msgbuf, int msgbuflen)
{
   msgbuf[0] = '\0';
   assert(jh && NULL == (*jh));
   (*jh) = (rhpRec_t *) calloc(1, sizeof(rhpRec_t));
   if (!*jh) {
      strncat(msgbuf, "Could not allocate ReSHOP object\n", msgbuflen);
   }
   assert((*jh));
}

/** @brief Destroy the given ReSHOP object.
 *
 *  @param jhptr    the ReSHOP object we are to destroy
 */
void rhpFree(rhpRec_t **jhptr)
{
   rhpRec_t *jh;

   assert(jhptr && (*jhptr));
   jh = (*jhptr);

   if (jh->gh) {
      if (jh->oh && jh->oh_created) {
         optFree(&jh->oh);
      }

      if (jh->ch) {
         cfgFree(&jh->ch);
      }
   }

   if (jh->mdl) {
      rhp_mdl_free(jh->mdl);
   }

   /*  TODO(xhub) check if it is  where we have to free this object? */
   free(jh);
}

/** @brief Perform initialization steps before calling the solver.
 *
 *  @param jh RESHOP object
 *  @param gh GMO object RESHOP is to use
 *  @param oh option object RESHOP is to use
 *
 *  @return status 0 if successful
 *                 1 otherwise
 */
int rhpReadyAPI(rhpRec_t *jh, gmoHandle_t gh, optHandle_t oh)
{
   char msg[GMS_SSSIZE], sysdir[GMS_SSSIZE];
   int rc = 0;
   bool opt_need_init;
   struct rhp_mdl *mdl = NULL;

   if (!gh) {
      fprintf(stderr, "RESHOP link expects non-NULL GMO handle\n");
      rc = 1; goto _exit;
   }

   /* TODO(GAMS review): the following 2 GAMS call make no sense */

   if (!gmoGetReady(msg, sizeof(msg))) {
      fprintf(stdout, "%s\n", msg);
      rc = 1; goto _exit;
   }
   jh->gh = gh;

   if (!gevGetReady(msg, sizeof(msg))) {
      fprintf(stdout, "%s\n", msg);
      rc = 1; goto _exit;
   }
   jh->eh = gmoEnvironment(gh);

   /* From now on we can use the GAMS log. */
   /* (data, printfn, flushgams, use_asciicolor)*/
   rhp_set_printops(jh, printgams, flushgams, false);

   /* Get the sysdir to load additional libraries */
   gevGetStrOpt(jh->eh, gevNameSysDir, sysdir);

   if (!dctGetReadyD(sysdir, msg, sizeof(msg))) {
      gevLogStat(jh->eh, msg);
      rc = 1; goto _exit;
   }
   CHK_CORRECT_LIBVER(dct, DCT);

   if (gmoDictionary(gh)) {
      jh->dh = gmoDict(gh);
   } else {
      jh->dh = NULL;
   }

   if (!cfgCreateD(&jh->ch, sysdir, msg, sizeof(msg))) {
      gevLogStat(jh->eh, msg);
      rc = 1; goto _exit;
   }

  /* ----------------------------------------------------------------------
   * Read the cfg file from the gams sysdir
   * ---------------------------------------------------------------------- */

   size_t sysdir_len = strlen(sysdir);
   if (sizeof(sysdir) <= sysdir_len + sizeof(GMS_CONFIG_FILE)) {
      gevLogStat(jh->eh, "*** ReSHOP: SysDir is too long");
      rc = 1; goto _exit;
   }
   strcat(sysdir, GMS_CONFIG_FILE);

   if (cfgReadConfig(jh->ch, sysdir)) {
      gevLogStat(jh->eh, sysdir);
      rc = 1; goto _exit;
   }

   /* Restore sysdir */
   sysdir[sysdir_len] = '\0';

   /* TODO(GAMS review): do we need this? */
   gevTerminateInstall(jh->eh);

   jh->timeinfo.start_readyapi = gevTimeDiffStart(jh->eh);

  /* ----------------------------------------------------------------------
   * Read and process options
   * ---------------------------------------------------------------------- */

   if (!jh->oh) {
      if (!optGetReadyD(sysdir, msg, sizeof(msg))) {
         gevLogStatPChar(jh->eh, "*** ReSHOP: Could not load option library: ");
         gevLogStat(jh->eh, msg);
         rc = 1; goto _exit;
      }

      if (!oh) {
         if (!optCreate(&jh->oh, msg, sizeof(msg))) {
            gevLogStatPChar(jh->eh, "*** ReSHOP: Could not create option struct: ");
            gevLogStat(jh->eh, msg);
            rc = 1; goto _exit;
         }
         jh->oh_created = 1;
      } else {
         jh->oh = oh;
      }
      opt_need_init = true;
   } else {
      opt_need_init = false;
   }

   rc = opt_process(jh, opt_need_init, sysdir);
   if (rc) {
      snprintf(msg, sizeof msg, "*** ReSHOP: Could not process options (rc=%d)", rc);
      gevLogStat(jh->eh, msg);
      rc = 1; goto _exit;
   }

   if (rhp_syncenv()) {
      gevLogStat(jh->eh, "*** ReSHOP: Failed to sync with environment variables");
      rc = 1; goto _exit;
   }

   /* Now we can initialize the GAMS library in ReSHOP */
   if (rhp_gms_loadlibs(sysdir)) {
      gevLogStat(jh->eh, "*** ReSHOP: Could not initialize GAMS library");
      rc = 1; goto _exit;
   }

   mdl = rhp_mdl_new(RHP_BACKEND_GAMS_GMO);

   if (!mdl) {
      gevLogStat(jh->eh, "*** ReSHOP: Could not create a ReSHOP model");
      rc = 1; goto _exit;
   }

   jh->mdl = mdl;

   struct gams_handles gset;
   gset.oh = jh->oh;
   gset.gh = jh->gh;
   gset.eh = jh->eh;
   gset.dh = jh->dh;
   gset.ch = jh->ch;

   rc = rhp_gms_fillgmshandles(mdl, &gset);
   if (rc) {
      gevLogStat(jh->eh,
                 "*** ReSHOP: Could not initialize model from the GAMS objects");
      rc = 1; goto _exit;
   }

   rc = rhp_gms_fillmdl(mdl);
   if (rc) {
      gevLogStat(jh->eh, "*** ReSHOP: Could not fill the model");
      rc = 1; goto _exit;
   }

   jh->timeinfo.end_readyapi = gevTimeDiffStart(jh->eh);

_exit:

   if (rc) {
      if (mdl) {
         rhp_mdl_free(mdl);
      }
   }

   return rc;
}

int rhpCallSolver(rhpRec_t *jh)
{
   int rc;
   char msg[GMS_SSSIZE];
   struct rhp_mdl *mdl_solver  = NULL;

   rc = opt_pushtosolver(jh);
   if (rc) {
      snprintf(msg, sizeof msg, "*** ReSHOP: reading options failed: error is %s (%d)\n", rhp_status_descr(rc), rc);
      gevLogStatPChar(jh->eh, msg);
      goto _exit;
   }

   gmoModelStatSet(jh->gh, gmoModelStat_ErrorNoSolution);
   gmoSolveStatSet(jh->gh, gmoSolveStat_SetupErr);

   /* Process the EMPINFO  */
   rc = rhp_gms_readempinfo(jh->mdl, NULL);
   if (rc) {
      snprintf(msg, sizeof msg, "*** Reading EMPINFO failed: error is %s (%d)\n",
               rhp_status_descr(rc), rc);
      gevLogStatPChar(jh->eh, msg);
      goto _exit;
   }

   mdl_solver = rhp_getsolvermdl(jh->mdl);
   if (!mdl_solver) {
      snprintf(msg, sizeof msg, "*** ReSHOP: couldn't create solver model object\n");
      gevLogStatPChar(jh->eh, msg);
      goto _exit;
   }

   /* Process the solver model (reformulations, ...) */
   rc = rhp_process(jh->mdl, mdl_solver);
   if (rc) {
      snprintf(msg, sizeof msg, "*** EMP transformation failed: error is %s (%d)\n",
               rhp_status_descr(rc), rc);
      gevLogStatPChar(jh->eh, msg);
      goto _exit;
   }

   /* Process the solver model  */
   rc = rhp_solve(mdl_solver);
   if (rc) {
      snprintf(msg, sizeof msg, "*** ReSHOP solve failed: error is %s (%d)\n",
               rhp_status_descr(rc), rc);
      gevLogStatPChar(jh->eh, msg);
      goto _exit;
   }

   /* Perform the postprocessing  */
   rc = rhp_postprocess(mdl_solver);
   if (rc) {
      snprintf(msg, sizeof msg, "*** ReSHOP value report failed: error is %s (%d)\n",
               rhp_status_descr(rc), rc);
      gevLogStatPChar(jh->eh, msg);
      goto _exit;
   }

   //gevLogStatPChar(jh->eh, "Model successfully solved by ReSHOP\n");

_exit:
   if (rc) {
      gmoSolveStatSet(jh->gh, gmoSolveStat_InternalErr);
      gmoModelStatSet(jh->gh, gmoModelStat_ErrorNoSolution);
      /* or gmoModelStat_ErrorUnknown ? */
   }

   /*  TODO(xhub) is this the right place to free, or in rhpFree? */
   rhp_mdl_free(mdl_solver);

   return rc;
}
