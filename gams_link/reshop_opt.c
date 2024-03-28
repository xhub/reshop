#include <stdio.h>
#include <string.h>

#ifdef _WIN32
#include <io.h>
#define F_OK 0
#define R_OK 4
#define access _access

#else /* _WIN32 */
#include <unistd.h>
#endif

#include "reshop.h"
//#include "reshop-gams.h"
#include "reshop_priv.h"

#define MAX(a, b) a > b ? a : b

static int concat(gevHandle_t eh, const char* sysdir, const char *fname, char outstr[GMS_SSSIZE])
{
   size_t sysdir_len = strlen(sysdir);
   size_t buf_len = strlen(fname);
   if (sysdir_len >= GMS_SSSIZE) {
      gevLogStatPChar(eh, "*** ReSHOP: ERROR! sysdir string '");
      gevLogStatPChar(eh, sysdir);
      gevLogStatPChar(eh, "' is too long!\n");
      return 1;
   }

   if (buf_len >= GMS_SSSIZE || sysdir_len >= GMS_SSSIZE-buf_len) {
      gevLogStat(eh, "*** ReSHOP: ERROR! concatenation of strings is too long:");
      gevLogStatPChar(eh, sysdir);
      gevLogStat(eh, fname);
      return 1;
   }

   strcpy(outstr, sysdir);
   strcpy(&outstr[sysdir_len], fname);

   return 0;
}

static void err_fname_missing(gevHandle_t eh, char fname[GMS_SSSIZE])
{
   gevLogStatPChar(eh, "*** ReSHOP: ERROR! Option definition file '");
   gevLogStatPChar(eh, fname);
   gevLogStatPChar(eh, "' does not exist\n");
}

static void err_fname_permission(gevHandle_t eh, char fname[GMS_SSSIZE])
{
   gevLogStatPChar(eh, "*** ReSHOP: ERROR! Cannot read (permission issue) option definition file '");
   gevLogStatPChar(eh, fname);
   gevLogStatPChar(eh, "'\n");

}
static int get_deffile(gevHandle_t eh, const char* sysdir, char fname[GMS_SSSIZE])
{
   char fname_sysdir[GMS_SSSIZE];
   int rc = 0;
  /* ----------------------------------------------------------------------
   * We do a nice check of whether we can read the file (R_OK).
   * If not, then we check if the file exists (F_OK). If yes, error.
   * If not, we repeat the process after prepending the sysdir.
   * ---------------------------------------------------------------------- */

   if (!access(fname, R_OK)) { return 0; }

   if (!access(fname, F_OK)) {
      err_fname_permission(eh, fname_sysdir);
      return 1;
   }

   /* fname might just be the filename, without any path */
   rc = concat(eh, sysdir, fname, fname_sysdir);
   if (rc) { return rc; }

   if (!access(fname_sysdir, R_OK)) {
      /* Update the fname for the parent */
      strcpy(fname, fname_sysdir);
      return 0;
   }
 
   if (access(fname_sysdir, F_OK)) {
       err_fname_missing(eh, fname);
       err_fname_missing(eh, fname_sysdir);
    } else {
       err_fname_permission(eh, fname_sysdir);
    }

   return 1;
}

int opt_process(rhpRec_t *jh, bool need_init, const char* sysdir)
{
   int ival, rc;
   char buf[GMS_SSSIZE], solvername[GMS_SSSIZE];
   gevHandle_t eh = jh->eh;
   gmoHandle_t gh = jh->gh;
   optHandle_t oh = jh->oh;
   cfgHandle_t ch = jh->ch;

   if (need_init) {

      int solver_id = gevGetIntOpt(eh, gevCurSolver);
      gevId2Solver(eh, solver_id, solvername);

      if (!cfgDefFileName(ch, solvername, buf)) {
         /* Get the default def file: gamsSysdir/optpath.def */
         rc = concat(eh, sysdir, "optreshop.def", buf);
         if (rc > 0) return rc;
      } else {
         rc = get_deffile(eh, sysdir, buf);
         if (rc > 0) return rc;
      }

      if (optReadDefinition(oh, buf)) {
         gevLogStat(eh, buf);

         gevStatCon(eh);

         for (int i = 1; i <= optMessageCount(oh); i++) {
            optGetMessage(oh, i, buf, &ival);
            gevStatC(eh, buf);
         }

         gevStatCoff(eh);
         optClearMessages(oh);
         return 1;
      }

      gevStatCon(eh);

      for (int i = 1; i <= optMessageCount(oh); i++) {
         optGetMessage(oh, i, buf, &ival);
         gevStatC(eh, buf);
      }

      gevStatCoff(eh);
      optClearMessages(oh);
   } else {
      optResetAll(oh);
   }

   /* Take options from the control file to option object. */
   /*
   optRecentEnabledSet(oh, 1);
   optRecentEnabledSet(oh, 0);
   */

   gmoNameOptFile(gh, buf);

   if (gmoOptFile(gh) && strlen(buf)) {
      optEchoSet(oh, 1);
      optRecentEnabledSet(oh, 1);
      optReadParameterFile(oh, buf);
      gevStatCon(eh);

      for (int i = 1; i <= optMessageCount(oh); i++) {
         optGetMessage(oh, i, buf, &ival);

         if (ival <= optMsgFileLeave || ival == optMsgUserError) {
            gevLogStat(eh, buf);
         }
      }

      optClearMessages(oh);
      gevStatCoff(eh);
      optRecentEnabledSet(oh, 0);
      optEchoSet(oh, 0);
   }

   return 0;
}

#define CHK(fn, s, ...) { \
   int status42 = fn(s, __VA_ARGS__); \
   if (status42) { \
      char msg[1024]; \
      (void)snprintf(msg, sizeof(msg), "ERROR: Call to " #fn " for option '%s' failed with error %s (%d)\n", \
         s, rhp_status_descr(status42), status42); \
      gevLogStatPChar(jh->eh, msg); \
      status = status42; \
      goto _exit; \
   } \
}

// Commented on 2024.02.15
#if 0
#define CHK0(fn, s) { int status42 = fn(s); if (status42) { \
char msg[1024]; \
snprintf(msg, sizeof(msg), "ERROR: Call to " #fn " for option '%s' failed with error %s (%d)\n)", \
s, rhp_status_descr(status42), status42);  gevLogStatPChar(jh->eh, msg); status = status42; goto _exit;} }
#endif

int opt_pushtosolver(rhpRec_t *jh)
{
   optHandle_t oh = jh->oh;
   char s[GMS_SSSIZE], fatbuf[GMS_SSSIZE + GMS_SSSIZE];
   int is_defined, is_defined_recent, refnum, datatype, opttype, subtype;
   int status = RHP_OK;

   gevStatC(jh->eh, "Processing options . . .");

   for (int i = 1, cnt = optCount(oh); i <= cnt; i++) {
      optGetInfoNr(oh, i, &is_defined, &is_defined_recent, &refnum,
                   &datatype, &opttype, &subtype);

      if (refnum == 0 && is_defined) {
         optGetNameNr(oh, i, s);
         int ival;
         double dval;

         switch (opttype) {
         case optTypeInteger:
            optGetIntNr(oh, i, &ival);
            CHK(rhp_opt_seti, s, ival);
            break;

         case optTypeDouble:
            optGetDblNr(oh, i, &dval);
            CHK(rhp_opt_setd, s, dval);
            break;

         case optTypeString:
            optGetStrNr(oh, i, fatbuf);
            CHK(rhp_opt_sets, s, fatbuf);
            break;
         case optTypeEnumStr:
            optGetStrNr(oh, i, fatbuf);
            CHK(rhp_opt_setc, s, fatbuf);
            break;

         case optTypeBoolean:
            optGetIntNr(oh, i, &ival);
            ival = ival ? 1 : 0;
            CHK(rhp_opt_setb, s, ival);
            break;

         default:
            break;
         }
      }
   }

_exit:
   return status;
}
