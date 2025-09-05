#include <ctype.h>
#include <errno.h>
#include <stdbool.h>

#include "gams_logging.h"
#include "ctrdat_gams.h"
#include "mdl.h"
#include "mdl_gams.h"
#include "printout.h"
#include "rhp_fwd.h"

enum GamsCbMode {
   GamsCbStatus = 1,
   GamsCbLog    = 2,
};

static int read_int(const char *msg, char **endptr)
{
   unsigned ii = 2;
   while (isspace(msg[ii])) { ii++; }

   int idx_msg = (int)strtol(&msg[ii], endptr, 10);

   /* Check for various possible errors. */

   if (errno == ERANGE || *endptr == &msg[ii]) {
      error("[GAMS/GEV] ERROR, can't sparse number in '%s'\n", msg);
      return INT_MIN;
   }

   /* we need a message: endptr should point at the text part */
   if (**endptr == '\0') {
      error("[GAMS/GEV] ERROR, can't get text part in '%s'\n", msg);
      return INT_MIN;
   }

   return idx_msg;
}

static void GEV_CALLCONV gevupstream_gevlogger(const char *msg, int mode, void *usrmem)
{
   gevStatLogCallbackGevUpstream *dat = (gevStatLogCallbackGevUpstream*)usrmem;

   if (mode == GamsCbStatus) {
      gevStatPChar(dat->gev_upstream, msg);
   } else if (mode == GamsCbLog) {
      gevLogPChar(dat->gev_upstream, msg);
   } else {
      error("[GAMS/GEV] ERROR in GEV writer: unknown mode %d", mode);
   }
}



static void
write_custom_equvar_errmsg(Model *mdl, rhp_idx idx, size_t msglen,
                           const char msgtxt[VMT(static msglen)],
                           Tgevlswrite_t writerFn, void *writerDat,
                           const char *symtype)
{
   const char *equname = mdl_printequname(mdl, idx);

   size_t sz = msglen + strlen(equname) + 50;

   M_ArenaTempStamp stamp = mdl_memtmp_init(mdl);
   char *msg_ = mdl_memtmp_get(mdl, sz);

   if (!msg_) {
      writerFn("=C ReSHOP ERROR: could not allocate memory!", 2, writerDat);
      return;
   }

   /* Write the message ourselves */
   (void)snprintf(msg_, sz, "=C ERROR %s %s (introduced by ReSHOP): %s", symtype,
                  equname, msgtxt);
   writerFn(msg_, GamsCbStatus, writerDat);

   mdl_memtmp_fini(stamp);
}

static void
write_custom_jabobian_errmsg(Model *mdl, rhp_idx ei, rhp_idx vi, size_t msglen,
                           const char msgtxt[VMT(static msglen)],
                           Tgevlswrite_t writerFn, void *writerDat)
{
   const char *equname = mdl_printequname(mdl, ei);
   const char *varname = mdl_printvarname(mdl, vi);

   size_t sz = msglen + strlen(equname) + strlen(varname) + 50;

   M_ArenaTempStamp stamp = mdl_memtmp_init(mdl);
   char *msg_ = mdl_memtmp_get(mdl, sz);

   if (!msg_) {
      writerFn("=C ReSHOP ERROR: could not allocate memory!", 2, writerDat);
      return;
   }

   /* Write the message ourselves */
   (void) snprintf(msg_, sz, "=C ERROR in the derivative of %s w.r.t. %s (introduced by ReSHOP): %s",
                   equname, varname, msgtxt);
   writerFn(msg_, GamsCbStatus, writerDat);

   mdl_memtmp_fini(stamp);
}

static void write_err(Tgevlswrite_t writerFn, void *writerDat, const char *msg)
{
      writerFn("=C ERROR while processing the following line", GamsCbStatus, writerDat);
      writerFn("=1", GamsCbStatus, writerDat);
      writerFn(msg, GamsCbStatus, writerDat);
      writerFn("=2", GamsCbStatus, writerDat);
}

static void write_badidx(Tgevlswrite_t writerFn, void *writerDat, const char *msg)
{
      writerFn("=C ERROR while processing the following line", GamsCbStatus, writerDat);
      writerFn("=1", GamsCbStatus, writerDat);
      writerFn(msg, GamsCbStatus, writerDat);
      writerFn("=2", GamsCbStatus, writerDat);
      writerFn("=C ERROR TYPE: bad index", GamsCbStatus, writerDat);
}

static void process_equvar_msg(GevLoggingCallbackData *dat, const char *msg)
{
  /* ----------------------------------------------------------------------
   * This function processes a status file messsage of the form
   * =(E|V) mi text
   *
   * The actions are to determine whether the equation/variable belongs to the
   * original GAMS model. If that's the case, we write the following in the
   * upstream status file:
   *
   * =(E|V) mi_orig text
   *
   * Otherwise, the following is passed on:
   *
   * =C New (Equation|Variable) name: text
   * ---------------------------------------------------------------------- */

   assert(msg[1] == 'E' || msg[1] == 'V');
   const bool isEqu = msg[1] == 'E';

   char *endptr;
   int idx_msg = read_int(msg, &endptr);

   Model *mdl = dat->mdl_solver;

   if (idx_msg <= 0) {
      error("[GAMS/GEV] ERROR: %s index %d is negative, it must belong to [1,%u]",
            isEqu ? "Equation" : "Variable", idx_msg, 1 + (isEqu ? mdl_nequs(mdl) : mdl_nvars(mdl)));
      return;
   }

   Tgevlswrite_t writerFn = dat->loggerFn;
   void *writerDat = &dat->loggerDat;

   if ((isEqu && !chk_ei(mdl, idx_msg-1, __func__)) || (!isEqu && !chk_vi(mdl, idx_msg-1, __func__))) {
      write_badidx(writerFn, writerDat, msg);
      return;
   }

   const Model *mdl_gms = dat->mdl_cmex;
   size_t msglen = strlen(endptr);

#ifndef RHP_COMPILER_CL
   size_t strsz = msglen > 2028 ? 2048 : msglen + 20;
   char str[strsz];
#else
   char str[2028];
#endif

   rhp_idx idx = idx_msg-1;
   if (mdl_gms) {

      rhp_idx idx_gms;

      /* --------------------------------------------------------------------
       * The message was for an equation. If it is known to CMEX, forward it
       * to the parent status file with the appropriate model index.
       * If not, print a message ourselves
       * -------------------------------------------------------------------- */

      if (isEqu) {

         idx_gms = mdl_getei_inmdlup(dat->mdl_solver, idx, mdl_gms);

         /* Write message directly */
         if (!valid_ei(idx_gms)) {
            write_custom_equvar_errmsg(mdl, idx, msglen, endptr, writerFn,
                                       writerDat, "Equation");
            return;
         } 

         /* Forward the message */
         (void)snprintf(str, sizeof(str), "=E %d %s", idx_gms+1, endptr);
         writerFn(str, GamsCbStatus, writerDat);
         return;
      } 

      /* --------------------------------------------------------------------
       * The message was for a variable. Do the same as above for this case
       * -------------------------------------------------------------------- */

      idx_gms = mdl_getvi_inmdlup(dat->mdl_solver, idx, mdl_gms);
 
      /* Write message directly */
      if (!valid_vi(idx_gms)) {
         write_custom_equvar_errmsg(mdl, idx, msglen, endptr, writerFn,
                                    writerDat, "Variable");
         return;
      } 

      /* Forward the message */
      (void)snprintf(str, sizeof(str), "=V %d %s", idx_gms+1, endptr);
      writerFn(str, 2, writerDat);
      return;
   }

   /* ----------------------------------------------------------------------
   * No parent GAMS model. We just write the error
   * ---------------------------------------------------------------------- */

   write_custom_equvar_errmsg(mdl, idx, msglen, endptr, writerFn, writerDat,
                              isEqu ? "Equation" : "Variable");
}

typedef enum { EquErr, VarErr, JacErr } StatusErrorCategory;

static void process_collected_errors(GevLoggingCallbackData *dat, const char *msg)
{
  /* ----------------------------------------------------------------------
   * This function processes a status file messsage of the form
   * =5  rownumber errornumber message
   * =6  colnumber errornumber message
   * =7  rownumber colnumber errornumber message
   *
   * The actions are to determine whether the equation/variable belongs to the
   * original GAMS model. If that's the case, we write the following in the
   * upstream status file:
   *
   * =5  rownumber_orig errornumber message
   * =6  colnumber_orig errornumber message
   * =7  rownumber_orig colnumber_orig errornumber message
   *
   * Otherwise, the following is passed on:
   *
   * =C ERROR errornumber in new (Equation|Variable) name: message
   * ---------------------------------------------------------------------- */
   assert(msg[1] == '5' || msg[1] == '6' || msg[1] == '7');
   const StatusErrorCategory errcat = msg[1] - '5';

   Model *mdl = dat->mdl_solver;
   Tgevlswrite_t writerFn = dat->loggerFn;
   void *writerDat = &dat->loggerDat;

   char *endptr;
   const char *msg_orig = msg;
   int idx_msg1 = read_int(msg, &endptr), idx_msg2;

   msg = endptr;

   if (idx_msg1 <= 0) {
      error("[GAMS/GEV] ERROR: %s index %d is negative, it must belong to [1,%u]",
            errcat != VarErr ? "Equation" : "Variable", idx_msg1,
            1 + (errcat != VarErr ? mdl_nequs(mdl) : mdl_nvars(mdl)));
      return;
   }

   rhp_idx idx1 = idx_msg1-1, idx2, idx1_gms, idx2_gms;

   if ((errcat != VarErr && !chk_ei(mdl, idx1, __func__)) || (errcat == VarErr && !chk_vi(mdl, idx1, __func__))) {
      write_badidx(writerFn, writerDat, msg);
      return;
   }

   /* Read variable index */
   if (errcat == JacErr) {

      idx_msg2 = read_int(msg, &endptr);

      if (idx_msg2 <= 0) {
         error("[GAMS/GEV] ERROR: Variable index %d is negative, it must belong to [1,%u]",
               idx_msg2, 1 + mdl_nvars(mdl));
         return;
      }

      idx2 = idx_msg2-1;

      if (!chk_vi(mdl, idx2, __func__)) {
         write_badidx(writerFn, writerDat, msg_orig);
         return;
      }

      msg = endptr;
   }

   const Model *mdl_gms = dat->mdl_cmex;
   size_t msglen = strlen(endptr);
#ifndef RHP_COMPILER_CL
   size_t strsz = msglen > 2028 ? 2048 : msglen + 20;
   char str[strsz];
#else
   char str[2028];
#endif

   switch (errcat) {
   case EquErr:
      idx1_gms = mdl_gms ? mdl_getei_inmdlup(mdl, idx1, mdl_gms) : IdxNA;

      /* Write message directly */
      if (!valid_ei(idx1_gms)) {
         write_custom_equvar_errmsg(mdl, idx1, msglen, endptr, writerFn,
                                    writerDat, "Equation");
         return;
      } 

      /* Forward the message */
      (void)snprintf(str, sizeof(str), "=5 %d %s", idx1_gms+1, endptr);
      writerFn(str, GamsCbStatus, writerDat);
      return;

   case VarErr:
      idx1_gms = mdl_gms ? mdl_getvi_inmdlup(mdl, idx1, mdl_gms) : IdxNA;

      /* Write message directly */
      if (!valid_vi(idx1_gms)) {
         write_custom_equvar_errmsg(mdl, idx1, msglen, endptr, writerFn,
                                    writerDat, "Variable");
         return;
      } 

      /* Forward the message */
      (void)snprintf(str, sizeof(str), "=6 %d %s", idx1_gms+1, endptr);
      writerFn(str, GamsCbStatus, writerDat);
      return;

   case JacErr:
      if (mdl_gms) {
         idx1_gms = mdl_getei_inmdlup(mdl, idx1, mdl_gms);
         idx2_gms = mdl_getvi_inmdlup(mdl, idx2, mdl_gms);
      } else {
         idx1_gms = idx2_gms = IdxNA;
      }

      /* Write message directly */
      if (!valid_ei(idx1_gms) || !valid_vi(idx2_gms)) {
         write_custom_jabobian_errmsg(mdl, idx1, idx2, msglen, endptr, writerFn,
                                    writerDat);
         return;
      } 

      /* Forward the message */
      (void)snprintf(str, sizeof(str), "=7 %d %d %s", idx1_gms+1, idx2_gms+1, endptr);
      writerFn(str, GamsCbStatus, writerDat);
      return;

   default:
      writerFn("=C ReSHOP ERROR: wrong error code", GamsCbStatus, writerDat);
      write_err(writerFn, writerDat, msg_orig);
   }
 
}


/**
 * @brief Status and Log callback function for GEV
 *
 * This either passes along the status message or processes them
 *
 * @param msg      the formatted message
 * @param mode     1 for status, 2 for log
 * @param usrmem   the data as pointer
 */
static void GEV_CALLCONV gams_process_subsolver_log(const char *msg, int mode, void *usrmem)
{
   /* The message signature is =1,..,=9,=C,=E,=V (where the = char may be changed).
   Meaning:
     =0   solver signature
     =1   start to copy    any marker =xxx will stop copy
     =2   stop to copy
     =3   eof  (not longer needed)
     =4   set sysout
     =5   collect: rownumber errornumber message
     =6   collect: colnumber errornumber message
     =7   collect: rownumber colnumber errornumber message
     =8   eject - new page
     =9   new escape character
     =A   copy file unconditional
     =B   copy file when doing a sysout
     =C   copy rest of line
     =E   write equation name   "text" equ text
     =V   write variable name   "text" var equ
     =T   Termination request
     =L   Log anchor
   */

   GevLoggingCallbackData *dat = (GevLoggingCallbackData*)usrmem;
   Tgevlswrite_t writerFn = dat->loggerFn;
   void *writerDat = &dat->loggerDat;

   /* We get a pascal string that starts with its length */
   char msg_[GMS_SSSIZE];
   unsigned char msglen = (unsigned char)msg[0];
   memcpy(msg_, msg+1, msglen);
   msg_[msglen] = '\0';

   /* In log mode, we just forward the message */
   if (mode == GevLogMsg) {
      writerFn(msg_, mode, writerDat);
      return;
   }

   if (dat->copymode) {
      writerFn(msg_, mode, writerDat);
      return;
   }

   if (msg_[0] != dat->prefixchar) {
      error("[GEV] ERROR in callback: incoming message does not start with %c:\n%s\n",
            dat->prefixchar, msg_);
      return;
   }

   /* FIXME: case 'A' and 'B' might fail as the file may be gone whenever CMEX 
   processes the status file */

   switch (msg_[1]) {
   case '1': dat->copymode = true;      writerFn(msg_, mode, writerDat); return;
   case '2': dat->copymode = false;     writerFn(msg_, mode, writerDat); return;
   case '9': dat->prefixchar = msg_[2]; writerFn(msg_, mode, writerDat); return;

   case '0': case '3': case '4': case '8':
   case 'A': case 'B':
   case 'C': case 'L': case 'T':       writerFn(msg_, mode, writerDat); return;

   case 'E': case 'V':                   process_equvar_msg(dat, msg_); return;
   case '5': case '6': case '7':   process_collected_errors(dat, msg_); return;
   default:
      error("[GEV] ERROR in callback: incoming message has unknown %c:\n%s\n",
            dat->prefixchar, msg_);
      writerFn(msg_, mode, writerDat);
   }

}

/** @brief Internal logger function, use only when there is no upstream GEV
 *
 * @param msg   message to print
 * @param mode  type of message
 * @param usrmem  logger data
 */
static void GEV_CALLCONV reshop_gevlogger_internal(const char *msg, int mode,
                                                   UNUSED void *usrmem)
{
   unsigned rhp_mode = mode << 2 | PO_ALLDEST;

   /* In log mode, we just forward the message */
   if (mode == GevLogMsg) {
      printstr(rhp_mode, msg);
      return;
   }

   /* in stat mode, we just cut the '=X' part and paste the message */
   if (strlen(msg) > 2) {
      printstr(rhp_mode, &msg[2]);
   }
}


/** @brief logger function for GAMS/GEV
 *
 * @param msg   message to print
 * @param mode  type of message
 * @param usrmem  logger data
 */
static void GEV_CALLCONV reshop_gevlogger(const char *msg, int mode, void *usrmem)
{
   /* We get a pascal string that starts with its length */
   char msg_[GMS_SSSIZE];
   unsigned char msglen = (unsigned char)msg[0];
   memcpy(msg_, msg+1, msglen);
   msg_[msglen] = '\0';

   unsigned rhp_mode = mode << 2 | PO_ALLDEST;

   /* In log mode, we just forward the message */
   if (mode == GevLogMsg) {
      printstr(rhp_mode, msg_);
      return;
   }

   GevLoggingCallbackData *dat = (GevLoggingCallbackData*)usrmem;

   if (dat->copymode) {
      printstr(rhp_mode, msg_);
      return;
   }

   if (msg_[0] != dat->prefixchar) {
      error("[GEV] ERROR in callback: incoming message does not start with %c:\n%s\n",
            dat->prefixchar, msg_);
      return;
   }

   /* FIXME: case 'A' and 'B' might fail as the file may be gone whenever CMEX 
   processes the status file */

   switch (msg_[1]) {
   case '1': dat->copymode = true;                                       return;
   case '2': dat->copymode = false;                                      return;
   case '9': dat->prefixchar = msg_[2];                                  return;

   /* unsupported for now */
   case '3': case '4': case '8': case 'A': case 'B':                     return;
   case '0':
   case 'C': case 'L': case 'T':          printstr(rhp_mode, &msg_[2]);  return;

   case 'E': case 'V':                   process_equvar_msg(dat, msg_);  return;
   case '5': case '6': case '7':   process_collected_errors(dat, msg_);  return;
   default:
      error("[GEV] ERROR in callback: incoming message has unknown %c:\n%s\n",
            dat->prefixchar, msg_);
   }

}

/**
 * @brief Initialize the GEV logging callback 
 *
 * @param mdl_solver  the GAMS model to be solved
 * @param cbtype      the type of callback
 *
 * @return            the error code
 */
int gev_logger_callback_init(Model *mdl_solver, gevStatLogCallbackType cbtype)
{
   assert(mdl_solver->backend == RhpBackendGamsGmo);
   GevLoggingCallbackData *cbdata = &((GmsModelData*) mdl_solver->data)->gevloggercbdata;

  /* ----------------------------------------------------------------------
   * Initialize the cbdata struct
   * ---------------------------------------------------------------------- */

   cbdata->mdl_solver = mdl_solver;

  /* ----------------------------------------------------------------------
   * Initialization specific to the callback type:
   * writerFn, writerDat, mdl_gms_cmex
   * The appropriate callback is also installed into the GEV
   * ---------------------------------------------------------------------- */

   switch (cbtype) {

   case GevLoggerCbUpstream: { /* There is a distinct upstream GEV */
      cbdata->loggerFn = gevupstream_gevlogger;

      Model *mdl_gms_cmex = mdl_solver->mdl_up;
      while (mdl_gms_cmex) {
         if (mdl_gms_cmex->backend == RhpBackendGamsGmo) {
            break;
         }
         mdl_gms_cmex = mdl_gms_cmex->mdl_up;
      }

      if (!mdl_gms_cmex) {
         errormsg("[GAMS/GMO] ERROR: no parent GAMS model. Please file a bug report.\n");
         return Error_GamsIncompleteSetupInfo;
      }

      cbdata->mdl_cmex = mdl_gms_cmex;
      gevHandle_t gev_upstream = ((GmsContainerData*)mdl_gms_cmex->ctr.data)->gev;

      if (!gev_upstream) {
         errormsg("[GAMS/GMO] ERROR: no upstream GEV object. Please file a bug report.\n");
         return Error_GamsIncompleteSetupInfo;
      }

      cbdata->loggerDat.gevupstream.gev_upstream = gev_upstream;

      gevHandle_t gev_solver = ((GmsContainerData*)mdl_solver->ctr.data)->gev;
      gevRegisterWriteCallback(gev_solver, gams_process_subsolver_log, 1, cbdata);

      } 
      break;

   case GevLoggerCbSame: /* We reuse the same GEV */
      TO_IMPLEMENT("Same GEV");
      /* Here we would use gevSwitchLogStat(Ex) to install the callback */

   case GevLoggerCbRhp: { /* ReSHOP custom Logger */
      gevHandle_t gev_solver = ((GmsContainerData*)mdl_solver->ctr.data)->gev;
      cbdata->loggerFn = reshop_gevlogger_internal;
      gevRegisterWriteCallback(gev_solver, reshop_gevlogger, 1, cbdata);
      cbdata->mdl_cmex = NULL;
      break;
   }

   default:
      error("[GAMS/GEV] ERROR: unknown GEV status/log callback %d\n", cbtype);
      return Error_RuntimeError;
   }

   return OK;
}
