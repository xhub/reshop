#include <assert.h>
#include <stdlib.h>
#include <stdio.h>
#include <string.h>

#include "reshop_gams_common.h"
#include "reshop-gams.h"
#include "reshop_priv.h"

#ifdef _WIN32
#define GMS_CONFIG_FILE "gmscmpNT.txt"
#else
#define GMS_CONFIG_FILE "gmscmpun.txt"
#endif

#define GAMSSOLVER_ID rhp
/* TODO: support modifyproblem */
//#define GAMSSOLVER_HAVEMODIFYPROBLEM
#include "GamsEntryPoints_tpl.c"

#define EPNAME(FNAME) GAMSSOLVER_CONCAT(GAMSSOLVER_ID, FNAME)

#ifndef _WIN32
__attribute__((constructor))
#endif
static void rhpInit(void)
{
   gmoInitMutexes();
   gevInitMutexes();
   gdxInitMutexes();
   optInitMutexes();
#ifdef GAMS_BUILD
   palInitMutexes();
#endif
}

#ifndef _WIN32
__attribute__((destructor))
#endif
static void rhpFini(void)
{
   gmoFiniMutexes();
   gevFiniMutexes();
   gdxFiniMutexes();
   optFiniMutexes();
#ifdef GAMS_BUILD
   palFiniMutexes();
#endif
}

#ifdef _WIN32
#ifndef WIN32_LEAN_AND_MEAN
#define WIN32_LEAN_AND_MEAN
#endif

#include <windows.h>
#include <process.h>
#if !defined(_MSC_VER)
BOOL WINAPI DllMain( HINSTANCE hInst, DWORD reason, LPVOID reserved);
#endif

BOOL WINAPI DllMain( HINSTANCE hInst, DWORD reason, LPVOID reserved)
{
   switch (reason) {
      case DLL_PROCESS_ATTACH:
         rhpInit();
         break;
      case DLL_PROCESS_DETACH:
         rhpFini();
         break;
      case DLL_THREAD_ATTACH:
      case DLL_THREAD_DETACH:
         /* ignored */
         break;
   }
   return TRUE;
}
#endif

/** @brief Create a RESHOP object.
 *
 *  @param jh        RESHOP object is stored to jh
 *  @param msgbuf    message buffer to which error string is printed
 *  @param msgbuflen length of the message buffer
 */
DllExport int STDCALL EPNAME(Create)(void **Cptr, char *msgBuf, int msgBufLen)
{
   rhpRec_t **jh = (rhpRec_t **)Cptr;
   msgBuf[0] = '\0';
   assert(jh && NULL == (*jh));
   (*jh) = (rhpRec_t *) calloc(1, sizeof(rhpRec_t));
   if (!*jh) {
      strncat(msgBuf, "Could not allocate ReSHOP object\n", msgBufLen);
      return 1;
   }

   return 0;
}

/** @brief Destroy the given ReSHOP object.
 *
 *  @param jhptr    the ReSHOP object we are to destroy
 */
DllExport void STDCALL EPNAME(Free)( void** Cptr)
{
   if (!Cptr || !*Cptr) { return; }

   rhpRec_t *jh = *(rhpRec_t **)Cptr;

   if (jh->gh) {
      if (jh->oh && jh->oh_created) {
         optFree(&jh->oh);
      }

      if (jh->ch) {
         cfgFree(&jh->ch);
      }
   }

#ifdef GAMS_BUILD
   if (jh->ph) { palFree(&jh->ph); }
#endif

   if (jh->mdl) {
      rhp_mdl_free(jh->mdl);
   }

   /*  TODO(xhub) check if it is where we have to free this object? */
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
DllExport int STDCALL EPNAME(ReadyAPI)(void* Cptr, gmoHandle_t gh)
{
   char msg[GMS_SSSIZE], sysdir[GMS_SSSIZE];
   int rc = 0;
   bool opt_need_init;
   struct rhp_mdl *mdl = NULL;

   rhpRec_t *jh = (rhpRec_t *)Cptr;
   if (!jh) {
      (void)fprintf(stderr, "*** ERROR: private structure is NULL\n");
      rc = 1; goto _exit;
   }

   if (!gh) {
      (void)fprintf(stderr, "*** ERROR: ReSHOP link expects non-NULL GMO handle\n");
      rc = 1; goto _exit;
   }

   /* Make sure that the pointers in the {gmo,gev}mcc.h are loaded */

   if (!gmoGetReady(msg, sizeof(msg))) {
      (void)fprintf(stdout, "%s\n", msg);
      rc = 1; goto _exit;
   }
   jh->gh = gh;

   if (!gevGetReady(msg, sizeof(msg))) {
      (void)fprintf(stdout, "%s\n", msg);
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
      gevLogStat(jh->eh, "*** ReSHOP ERROR: GMO lacks a dictionary. This is not supported");
      rc = 1; goto _exit;
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
      gevLogStat(jh->eh, "*** ReSHOP ERROR: SysDir is too long");
      rc = 1; goto _exit;
   }
   strcat(sysdir, GMS_CONFIG_FILE);

   if (cfgReadConfig(jh->ch, sysdir)) {
      gevLogStatPChar(jh->eh, "*** ReSHOP ERROR: Could not parse config file ");
      gevLogStat(jh->eh, sysdir);
      while (cfgNumMsg(jh->ch) > 0) {
         cfgGetMsg(jh->ch, msg);
         gevLogStat(jh->eh, msg);
      }
      rc = 1; goto _exit;
   }


   /* Restore sysdir */
   sysdir[sysdir_len] = '\0';

#ifdef GAMS_BUILD
  if (!palCreateD(&jh->ph, sysdir, msg, sizeof(msg))) {
      gevLogStat(jh->eh, "*** ReSHOP ERROR: Could not create PAL object");
      rc = 1; goto _exit;
  }

   /* Query the name of the solver to get its def file */
   int si = gevGetIntOpt(jh->eh, gevCurSolver);
   gevId2Solver(jh->eh, si, msg);

#if PALAPIVERSION >= 3
   palSetSystemName(jh->ph, msg);
 
    /* print auditline */
    palGetAuditLine(jh->ph, msg);
    gevStatAudit(jh->eh, msg);
#endif

#define PALPTR jh->ph
#define GEVPTR jh->eh
#include "cmagic2.h"

   if  (palLicenseCheck(jh->ph, gmoM(gh), gmoN(gh), gmoNZ(gh), gmoNLNZ(gh), gmoNDisc(gh))) {
      while (palLicenseGetMessage(jh->ph, msg, sizeof(msg))) {
         gevLogStat(jh->eh, msg);
      }
      gevLogStat(jh->eh, "*** ReSHOP ERROR while checking GAMS distribution version");
      gmoSolveStatSet(gh, gmoSolveStat_SetupErr);
      gmoModelStatSet(gh, gmoModelStat_NoSolutionReturned);
      rc = 1; goto _exit;
   }
#endif

   /* TODO(GAMS review): do we need this? */
   gevTerminateInstall(jh->eh);

  /* ----------------------------------------------------------------------
   * Read and process options
   * ---------------------------------------------------------------------- */

   if (!jh->oh) {
      if (!optGetReadyD(sysdir, msg, sizeof(msg))) {
         gevLogStatPChar(jh->eh, "*** ReSHOP ERROR: Could not load option library: ");
         gevLogStat(jh->eh, msg);
         rc = 1; goto _exit;
      }

      if (!optCreate(&jh->oh, msg, sizeof(msg))) {
         gevLogStatPChar(jh->eh, "*** ReSHOP ERROR: Could not create option struct: ");
         gevLogStat(jh->eh, msg);
         rc = 1; goto _exit;
      }

      jh->oh_created = 1;
      opt_need_init = true;
   } else {
      opt_need_init = false;
   }

   rc = opt_process(jh, opt_need_init, sysdir);
   if (rc) {
      (void)snprintf(msg, sizeof msg, "*** ReSHOP ERROR: Could not process options (rc=%d)", rc);
      gevLogStat(jh->eh, msg);
      rc = 1; goto _exit;
   }

   /* Print the reshop banner */
   rhp_print_banner();

   if (rhp_syncenv()) {
      gevLogStat(jh->eh, "\n\n*** ReSHOP ERROR: Failed to sync with environment variables");
      rc = 1; goto _exit;
   }

   /* Now we can initialize the GAMS library in ReSHOP */
   if (rhp_gms_loadlibs(sysdir)) {
      gevLogStat(jh->eh, "\n\n*** ReSHOP ERROR: Could not initialize GAMS library");
      rc = 1; goto _exit;
   }

   mdl = rhp_mdl_new(RhpBackendGamsGmo);

   if (!mdl) {
      gevLogStat(jh->eh, "\n\n*** ReSHOP ERROR: Could not create a ReSHOP model");
      rc = 1; goto _exit;
   }

   jh->mdl = mdl;

   struct rhp_gams_handles gset;
   gset.oh = jh->oh;
   gset.gh = jh->gh;
   gset.eh = jh->eh;
   gset.dh = jh->dh;
   gset.ch = jh->ch;

   rc = rhp_gms_fillgmshandles(mdl, &gset);
   if (rc) {
      gevLogStat(jh->eh, "\n\n*** ReSHOP ERROR: Could not initialize model from the GAMS objects");
      rc = 1; goto _exit;
   }

   rc = rhp_gms_fillmdl(mdl);
   if (rc) {
      gevLogStat(jh->eh, "\n\n*** ReSHOP ERROR: Could not fill the model");
      rc = 1; goto _exit;
   }

_exit:

   if (rc) {
      if (mdl) {
         rhp_mdl_free(mdl);
      }
   }

   return rc;
}

DllExport int STDCALL GAMSSOLVER_CONCAT(GAMSSOLVER_ID,CallSolver)(void* Cptr)
{
   rhpRec_t *jh = (rhpRec_t *)Cptr;
   int rc;
   char msg[GMS_SSSIZE];
   struct rhp_mdl *mdl_solver  = NULL;

   gmoModelStatSet(jh->gh, gmoModelStat_ErrorNoSolution);
   gmoSolveStatSet(jh->gh, gmoSolveStat_SetupErr);

   rc = opt_pushtosolver(jh);
   if (rc) {
      (void)snprintf(msg, sizeof msg, "\n\n*** ReSHOP ERROR: reading options failed! Error message is %s (%d)\n",
               rhp_status_descr(rc), rc);
      gevLogStatPChar(jh->eh, msg);
      return rc; /* Early return */
   }

   /* Process the EMPINFO  */
   rc = rhp_gms_readempinfo(jh->mdl, NULL);
   if (rc) {
      (void)snprintf(msg, sizeof msg, "\n\n*** ReSHOP ERROR: Reading EMPINFO failed! Error message is %s (%d)\n",
               rhp_status_descr(rc), rc);
      gevLogStatPChar(jh->eh, msg);
      goto _exit;
   }

   mdl_solver = rhp_newsolvermdl(jh->mdl);
   if (!mdl_solver) {
      (void)snprintf(msg, sizeof msg, "\n\n*** ReSHOP ERROR: couldn't create solver model object\n");
      gevLogStatPChar(jh->eh, msg);
      goto _exit;
   }

   /* Process the solver model (reformulations, ...) */
   rc = rhp_process(jh->mdl, mdl_solver);
   if (rc) {
      (void)snprintf(msg, sizeof msg, "\n\n*** ReSHOP ERROR: EMP transformation failed! Error message is %s (%d)\n",
               rhp_status_descr(rc), rc);
      gevLogStatPChar(jh->eh, msg);
      goto _exit;
   }

   /* Process the solver model  */
   rc = rhp_solve(mdl_solver);
   if (rc) {
      (void)snprintf(msg, sizeof msg, "\n\n*** ReSHOP ERROR: solve failed! Error message is %s (%d)\n",
               rhp_status_descr(rc), rc);
      gevLogStatPChar(jh->eh, msg);
      goto _exit;
   }

   /* Perform the postprocessing  */
   rc = rhp_postprocess(mdl_solver);
   if (rc) {
      (void)snprintf(msg, sizeof msg, "\n\n*** ReSHOP ERROR: postprocessing failed! Error message is %s (%d)\n",
               rhp_status_descr(rc), rc);
      gevLogStatPChar(jh->eh, msg);
      goto _exit;
   }

_exit:
   if (rc) {
      gmoModelStatSet(jh->gh, gmoModelStat_ErrorNoSolution);
      gmoSolveStatSet(jh->gh, rhp_rc2gmosolvestat(rc));

      rhp_printrcmsg(rc, jh->eh);
   }

   /*  TODO(xhub) is this the right place to free, or in rhpFree? */
   rhp_mdl_free(mdl_solver);

   return rc;
}
