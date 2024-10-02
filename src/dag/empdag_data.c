
#include <stdio.h>

#include "empdag_data.h"
#include "macros.h"
#include "mdl.h"
#include "status.h"
#include "printout.h"

tlsvar char buf[128];

const char *mpid_specialvalue(mpid_t mpid)
{
   UNUSED int status = OK;

   if (!(mpid & MpId_SpecialValueMask)) {
      IO_CALL_EXIT(snprintf(buf, sizeof buf,
                            "mpid_specialvalue(): ERROR mp ID %u is not a special value", mpid));
      return buf;
   }

   if (mpid == MpId_NA) {
      return "MP not available";
   }

   mpid_t sv = mpid & MpId_SpecialValueTag;

   switch (sv) {
   case MpId_SharedVarMask: {
      mpid_t grpid = mpid & MpId_SpecialValuePayload;
      IO_CALL_EXIT(snprintf(buf, sizeof buf,
                            "Shared variable member of group %u", grpid));
      return buf;
      }
   case MpId_SharedEquMask: {
      mpid_t grpid = mpid & MpId_SpecialValuePayload;
      IO_CALL_EXIT(snprintf(buf, sizeof buf,
                            "Shared equation member of group %u", grpid));
      return buf;
      }
   case MpId_OvfDataMask: {
      mpid_t ovfid = mpid & MpId_SpecialValuePayload;
      IO_CALL_EXIT(snprintf(buf, sizeof buf,
                            "OVF equation or variable #%u", ovfid));
      return buf;
      }

   default: 
      IO_CALL_EXIT(snprintf(buf, sizeof buf,
                            "ERROR: Unknown MP ID special value %u", sv));
      return buf;
   }

_exit:
   return "mpid_specialvalue: runtime ERROR";
}

const char *mpid_getname(const Model *mdl, mpid_t mp_id)
{

   if (mpid_regularmp(mp_id)) {
      return empdag_getmpname(&mdl->empinfo.empdag, mp_id);
   }

   return mpid_specialvalue(mp_id);
}
