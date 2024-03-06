#include "asprintf.h"

#include "empdag.h"
#include "empdag_uid.h"
#include "empdag_priv.h"
#include "empinfo.h"
#include "mdl.h"
#include "printout.h"
#include "tlsdef.h"

static tlsvar char bufMPE[3*sizeof(unsigned)*CHAR_BIT/8+5];
static tlsvar char bufMP[3*sizeof(unsigned)*CHAR_BIT/8+5];
static tlsvar char bufMP2[3*sizeof(unsigned)*CHAR_BIT/8+5];
static tlsvar char msg[3*sizeof(unsigned)*CHAR_BIT/8+41];

const char *daguid_type2str(unsigned uid)
{
   return uidisMP(uid) ? "MP" : "MPE";
}

bool valid_uid_(const EmpDag *empdag, daguid_t uid, const char *fn)
{
   if (!valid_uid(uid)) {
      error("[empdag] ERROR: invalid uid %u in function %s", uid, fn);
      return false;
   }

   if (uidisMP(uid)) {
      mpid_t mpid = uid2id(uid);
      if (mpid >= empdag->mps.len) {
         error("[empdag] ERROR in function %s: MP id %u is outside of [0, %u)",
               fn, mpid,  empdag->mps.len);
         return  false;
      }

      return true;
   }

   assert(uidisMPE(uid));

   mpeid_t mpeid = uid2id(uid);
   if (mpeid >= empdag->mpes.len) {
      error("[empdag] ERROR in function %s: MPE id %u is outside of [0, %u)",
            fn, mpeid, empdag->mpes.len);
      return  false;
   }

      return true;
}

bool empdag_mphasname(const EmpDag *empdag, unsigned mp_id)
{
   assert(mp_id < empdag->mps.len);
   return ((mp_id < empdag->mps.len) && empdag->mps.names[mp_id]);
}

const char* empdag_getmpname(const EmpDag *empdag, mpid_t mpid)
{
   int status;

   if (mpid >= MpId_MaxRegular) {
      return mpid_specialvalue(mpid);
   }

   if (mpid >= empdag->mps.len) {
      IO_CALL_EXIT(snprintf(msg, sizeof msg, "ERROR: MP index %u is out of bound",
                            mpid));
      return msg;
   }

   const char *mp_name = empdag->mps.names[mpid];
   if (mp_name) { return mp_name; }

   IO_CALL_EXIT(snprintf(bufMP, sizeof bufMP, "ID %u", mpid));

   return bufMP;

_exit:
   return "RUNTIME ERROR";
}

const char* empdag_getmpname2(const EmpDag *empdag, mpid_t mpid)
{
   int status;

   if (mpid >= MpId_MaxRegular) {
      return mpid_specialvalue(mpid);
   }

   if (mpid >= empdag->mps.len) {
      IO_CALL_EXIT(snprintf(msg, sizeof msg, "ERROR: MP index %u is out of bound",
                            mpid));
      return msg;
   }

   const char *mp_name = empdag->mps.names[mpid];
   if (mp_name) { return mp_name; }

   IO_CALL_EXIT(snprintf(bufMP2, sizeof bufMP2, "ID %u", mpid));

   return bufMP2;

_exit:
   return "RUNTIME ERROR";
}

const char* empdag_getmpename(const EmpDag *empdag, unsigned id)
{
   int status;

   if (id >= empdag->mpes.len) {
      IO_CALL_EXIT(snprintf(msg, sizeof msg, "ERROR: MPE index %u is out of bound",
                            id));
      return msg;
   }

   const char *mpe_name = empdag->mpes.names[id];
   if (mpe_name) { return mpe_name; }
   IO_CALL_EXIT(snprintf(bufMPE, sizeof bufMPE, "ID %u", id));

   return bufMPE;

_exit:
   return "RUNTIME ERROR";
}

/**
 * @brief Return the name of an EMP DAG element
 *
 *        If the element has its name set, then returns it
 *        Otherwise return a string representation of its ID.
 *
 * @param empdag  the EMPDAG
 * @param uid     the UID of the object
 *
 * @return        the string
 */
const char *empdag_getname(const EmpDag *empdag, unsigned uid)
{

   if (!valid_uid(uid)) {
      return "ERROR: invalid UID!";
   }

   /* ---------------------------------------------------------------------
    * Deals with MP case first and then MPE (which is almost identical)
    * --------------------------------------------------------------------- */

   unsigned id = uid2id(uid);
   if (uidisMP(uid)) {
      return empdag_getmpname(empdag, id);
   } 

   assert(uidisMPE(uid));
   return empdag_getmpename(empdag, id);
}



int empdag_exportasdot(Model *mdl)
{
   int status = OK;
   static unsigned cnt = 0;
   char *fname = NULL;

   cnt++;

   const char *latex_dir = mygetenv("RHP_EXPORT_LATEX");
   if (latex_dir) {
      IO_CALL(asprintf(&fname, "%s" DIRSEP "empdag_%u.dot", latex_dir, cnt));
      S_CHECK(empdag2dotfile(&mdl->empinfo.empdag, fname));
   }
   myfreeenvval(latex_dir);

   const char *empdag_dotdir = mygetenv("RHP_EMPDAG_DOTDIR");
   
   if (empdag_dotdir) {
      IO_CALL(asprintf(&fname, "%s" DIRSEP "empdag_%u.dot", empdag_dotdir, cnt));
      S_CHECK(empdag2dotfile(&mdl->empinfo.empdag, fname));
   }
   myfreeenvval(empdag_dotdir);

   if (optvalb(mdl, Options_Display_EmpDag) || optvalb(mdl, Options_Export_EmpDag)) {
      if (mdl_ensure_exportdir(mdl) != OK) {
         error("%s ERROR: could not create an export dir", __func__);
      } else {
         const char *export_dir = mdl->commondata.exports_dir;
         IO_CALL(asprintf(&fname, "%s" DIRSEP "empdag_%u.dot", export_dir, cnt));
         S_CHECK(empdag2dotfile(&mdl->empinfo.empdag, fname));
      }
   }

   return OK;

_exit:
   FREE(fname);
   return status;
}
