#include "asprintf.h"

#include "empdag.h"
#include "empdag_uid.h"
#include "empdag_priv.h"
#include "empinfo.h"
#include "mdl.h"
#include "printout.h"
#include "tlsdef.h"

static tlsvar char bufNash[3*sizeof(unsigned)*CHAR_BIT/8+5];
static tlsvar char bufNash2[3*sizeof(unsigned)*CHAR_BIT/8+5];
static tlsvar char bufMP[3*sizeof(unsigned)*CHAR_BIT/8+5];
static tlsvar char bufMP2[3*sizeof(unsigned)*CHAR_BIT/8+5];
static tlsvar char msg[3*sizeof(unsigned)*CHAR_BIT/8+41];
static tlsvar char msg2[3*sizeof(unsigned)*CHAR_BIT/8+41];

const char *daguid_type2str(daguid_t uid)
{
   return uidisMP(uid) ? "MP" : "Nash";
}

const char * rarclinktype2str(daguid_t uid)
{
   if (uidisNash(uid))  { return "Nash link"; }
   if (rarcTypeVF(uid)) { return "valFn link"; }
   return "CTRL link";
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

   assert(uidisNash(uid));

   nashid_t nash = uid2id(uid);
   if (nash >= empdag->nashs.len) {
      error("[empdag] ERROR in function %s: Nash id %u is outside of [0, %u)",
            fn, nash, empdag->nashs.len);
      return  false;
   }

      return true;
}

bool empdag_mp_hasname(const EmpDag *empdag, unsigned mpid)
{
   assert(mpid < empdag->mps.len);
   return ((mpid < empdag->mps.len) && empdag->mps.names[mpid]);
}

bool empdag_mp_hasobjfn_modifiers(const EmpDag *empdag, mpid_t mpid)
{
   assert(mpid < empdag->mps.len);
   if (mpid >= empdag->mps.len) { 
      return false;
   }

   const VarcArray * Varcs = &empdag->mps.Varcs[mpid];
   const Varc *arr = Varcs->arr;

   for (unsigned i = 0, len = Varcs->len; i < len; ++i) {
      if (arcVF_in_objfunc(&arr[i], empdag->mdl)) { return true; }
   }

   /* There could be an objfn added */
   unsigned idx = mpidarray_find(&empdag->objfn.dst, mpid);
   if (idx < UINT_MAX) { return true; }

   idx = mpidarray_find(&empdag->objfn_maps.mps, mpid);
   return idx < UINT_MAX;
}

const char* empdag_getmpname(const EmpDag *empdag, mpid_t mpid)
{
   UNUSED int status;

   if (mpid >= MpId_MaxRegular) {
      return mpid_specialvalue(mpid);
   }

   if (mpid >= empdag->mps.len) {
      IO_PRINT_EXIT(snprintf(msg, sizeof msg, "ERROR: MP index %u is out of bound",
                            mpid));
      return msg;
   }

   const char *mp_name = empdag->mps.names[mpid];
   if (mp_name) { return mp_name; }

   IO_PRINT_EXIT(snprintf(bufMP, sizeof bufMP, "ID %u", mpid));

   return bufMP;

_exit:
   return "RUNTIME ERROR";
}

const char* empdag_getmpname2(const EmpDag *empdag, mpid_t mpid)
{
   UNUSED int status;

   if (mpid >= MpId_MaxRegular) {
      return mpid_specialvalue(mpid);
   }

   if (mpid >= empdag->mps.len) {
      IO_PRINT_EXIT(snprintf(msg, sizeof msg, "ERROR: MP index %u is out of bound",
                            mpid));
      return msg;
   }

   const char *mp_name = empdag->mps.names[mpid];
   if (mp_name) { return mp_name; }

   IO_PRINT_EXIT(snprintf(bufMP2, sizeof bufMP2, "ID %u", mpid));

   return bufMP2;

_exit:
   return "RUNTIME ERROR";
}

const char* empdag_getnashname(const EmpDag *empdag, unsigned id)
{
   UNUSED int status;

   if (id >= empdag->nashs.len) {
      IO_PRINT_EXIT(snprintf(msg, sizeof msg, "ERROR: Nash index %u is out of bound",
                            id));
      return msg;
   }

   const char *nash_name = empdag->nashs.names[id];
   if (nash_name) { return nash_name; }
   IO_PRINT_EXIT(snprintf(bufNash, sizeof bufNash, "ID %u", id));

   return bufNash;

_exit:
   return "RUNTIME ERROR";
}

const char* empdag_getnashname2(const EmpDag *empdag, unsigned id)
{
   UNUSED int status;

   if (id >= empdag->nashs.len) {
      IO_PRINT_EXIT(snprintf(msg2, sizeof msg2, "ERROR: Nash index %u is out of bound",
                            id));
      return msg;
   }

   const char *nash_name = empdag->nashs.names[id];
   if (nash_name) { return nash_name; }
   IO_PRINT_EXIT(snprintf(bufNash2, sizeof bufNash2, "ID %u", id));

   return bufNash2;

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

   assert(uidisNash(uid));
   return empdag_getnashname(empdag, id);
}

/**
 * @brief Return the name of an EMP DAG element
 *
 *        If the element has its name set, then returns it
 *        Otherwise return a string representation of its ID.
 *
 *        This function uses a different buffer to empdag_getname()
 *
 * @param empdag  the EMPDAG
 * @param uid     the UID of the object
 *
 * @return        the string
 */
const char *empdag_getname2(const EmpDag *empdag, unsigned uid)
{

   if (!valid_uid(uid)) {
      return "ERROR: invalid UID!";
   }

   /* ---------------------------------------------------------------------
    * Deals with MP case first and then MPE (which is almost identical)
    * --------------------------------------------------------------------- */

   unsigned id = uid2id(uid);
   if (uidisMP(uid)) {
      return empdag_getmpname2(empdag, id);
   } 

   assert(uidisNash(uid));
   return empdag_getnashname2(empdag, id);
}



int empdag_export(Model *mdl)
{
   int status = OK;
   static unsigned cnt = 0;
   char *fname = NULL;

   cnt++;

   const char *latex_dir = mygetenv("RHP_EXPORT_LATEX");
   if (latex_dir) {
      IO_PRINT(asprintf(&fname, "%s" DIRSEP "empdag_%u.dot", latex_dir, cnt));
      S_CHECK_EXIT(empdag2dotfile(&mdl->empinfo.empdag, fname));
      free(fname); fname = NULL;
   }
   myfreeenvval(latex_dir);

   const char *empdag_dotdir = mygetenv("RHP_EMPDAG_DOTDIR");

   if (empdag_dotdir) {
      IO_PRINT(asprintf(&fname, "%s" DIRSEP "empdag_%u.dot", empdag_dotdir, cnt));
      S_CHECK_EXIT(empdag2dotfile(&mdl->empinfo.empdag, fname));
      free(fname); fname = NULL;
   }
   myfreeenvval(empdag_dotdir);

   if (optvalb(mdl, Options_Display_EmpDag) || optvalb(mdl, Options_Save_EmpDag)) {
      if (mdl_ensure_exportdir(mdl) != OK) {
         error("[empdag] ERROR: could not create an export directory for %s model '%*s' #%u\n",
               mdl_fmtargs(mdl));
         return Error_SystemError;
      }

      const char *export_dir = mdl->commondata.exports_dir;
      IO_PRINT(asprintf(&fname, "%s" DIRSEP "empdag_%u.dot", export_dir, cnt));
      S_CHECK_EXIT(empdag2dotfile(&mdl->empinfo.empdag, fname));
      free(fname);
   }

   return OK;

_exit:
   free(fname);
   return status;
}
