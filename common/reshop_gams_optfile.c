#include <stddef.h>
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
#include "reshop_gams_common.h"

#define MAX(a, b) a > b ? a : b

#define CHK(fn, s, ...) { \
   int status42 = fn(s, __VA_ARGS__); \
   if (status42) { \
      char msg[1024]; \
      (void)snprintf(msg, sizeof(msg), "ERROR: Call to " #fn " for option '%s' failed with error %s (%d)\n", \
         s, rhp_status_descr(status42), status42); \
      rhp_print(msg); \
      rhp_print("\n"); \
      status = status42; \
      goto _exit; \
   } \
}

static int concat(const char* sysdir, const char *fname, char outstr[GMS_SSSIZE])
{
   size_t sysdir_len = strlen(sysdir);
   size_t buf_len = strlen(fname);
   if (sysdir_len >= GMS_SSSIZE) {
      rhp_print("*** ReSHOP: ERROR! sysdir string '%s' is too long!\n", sysdir);
      return 1;
   }

   if (buf_len >= GMS_SSSIZE || sysdir_len >= GMS_SSSIZE-buf_len) {
      rhp_print("*** ReSHOP: ERROR! concatenation of strings is too long:\n%s\n\nand\n\n%s\n", sysdir, fname);
      return 1;
   }

   strcpy(outstr, sysdir);
   strcpy(&outstr[sysdir_len], fname);

   return 0;
}

static void err_fname_missing(char fname[GMS_SSSIZE])
{
   rhp_print("*** ReSHOP: ERROR! Option definition file '%s'\n", fname);
}

static void err_fname_permission(char fname[GMS_SSSIZE])
{
   rhp_print("*** ReSHOP: ERROR! Cannot read (permission issue) option definition file '%s'\n", fname);
}

static int get_deffile(const char* sysdir, char fname[GMS_SSSIZE])
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
      err_fname_permission(fname_sysdir);
      return 1;
   }

   /* fname might just be the filename, without any path */
   rc = concat(sysdir, fname, fname_sysdir);
   if (rc) { return rc; }

   if (!access(fname_sysdir, R_OK)) {
      /* Update the fname for the parent */
      strcpy(fname, fname_sysdir);
      return 0;
   }
 
   if (access(fname_sysdir, F_OK)) {
       err_fname_missing(fname);
       err_fname_missing(fname_sysdir);
    } else {
       err_fname_permission(fname_sysdir);
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
         /* Get the default def file: gamsSysdir/optreshop.def */
         rc = concat(sysdir, "optreshop.def", buf);
      } else {
         rc = get_deffile(sysdir, buf);
         if (rc > 0) return rc;
      }

      if (optReadDefinition(oh, buf)) {
         rhp_print(buf);
         rhp_print("\n");

         for (int i = 1; i <= optMessageCount(oh); i++) {
            optGetMessage(oh, i, buf, &ival);
            rhp_print(buf);
            rhp_print("\n");
         }

         optClearMessages(oh);
 
         return 1;
      }

      for (int i = 1; i <= optMessageCount(oh); i++) {
         optGetMessage(oh, i, buf, &ival);
         rhp_print(buf);
         rhp_print("\n");
      }

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

      for (int i = 1; i <= optMessageCount(oh); i++) {
         optGetMessage(oh, i, buf, &ival);

         if (ival <= optMsgFileLeave || ival == optMsgUserError) {
            rhp_print(buf);
            rhp_print("\n");
         }
      }

      optClearMessages(oh);
      optRecentEnabledSet(oh, 0);
      optEchoSet(oh, 0);
   }

   return 0;
}

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
         unsigned char bval;

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
            bval = ival ? 1 : 0;
            CHK(rhp_opt_setb, s, bval);
            break;

         default:
            break;
         }
      }
   }

_exit:
   return status;
}
