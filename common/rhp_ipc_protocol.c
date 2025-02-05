
#include <stddef.h>
#include <string.h>

#ifdef HAS_UNISTD
#include <unistd.h>
#endif

#include "rhp_ipc_protocol.h"

#define MSGTYPE_STR() \
DEFSTR(DataMp,"DataMp") \
DEFSTR(DataNash,"DataNash") \
DEFSTR(DataVarc,"DataVarc") \
DEFSTR(DataCarc,"DataCarc") \
DEFSTR(DataNarc,"DataNarc") \
DEFSTR(EventInit,"EventInit") \
DEFSTR(EventFini,"EventFini") \
DEFSTR(EventReset,"EventReset") \
DEFSTR(GuiIsReady,"GuiIsReady") \
DEFSTR(GuiAck,"GuiAck") \
DEFSTR(GuiAckReq,"GuiAckReq") \
DEFSTR(GuiRaiseError,"GuiRaiseError") \
DEFSTR(GuiIsClosing,"GuiIsClosing") \
DEFSTR(LogMsgSolver,"LogMsgSolver") \
DEFSTR(LogMsgSubSolver,"LogMsgSubSolver") \
DEFSTR(NewEmpDagStart,"NewEmpDagStart") \
DEFSTR(NewEmpDagEnd,"NewEmpDagEnd") \
DEFSTR(NewModelStart,"NewModelStart") \
DEFSTR(NewModelName,"NewModelName") \
DEFSTR(NewModelEnd,"NewModelEnd") \
DEFSTR(RequestCoeffValue,"RequestCoeffValue") \
DEFSTR(RequestEquById,"RequestEquById") \
DEFSTR(RequestEquName,"RequestEquName") \
DEFSTR(RequestMpById,"RequestMpById") \
DEFSTR(RequestMpName,"RequestMpName") \
DEFSTR(RequestNashById,"RequestNashById") \
DEFSTR(RequestNashName,"RequestNashName") \
DEFSTR(RequestVarById,"RequestVarById") \
DEFSTR(RequestVarName,"RequestVarName") \


#define DEFSTR(id, str) char id[sizeof(str)];

typedef struct {
   union {
      struct {
      MSGTYPE_STR()
   };
   char dummystr[1];
   };
} MsgNames;
#undef DEFSTR

#define DEFSTR(id, str) .id = str,

static const MsgNames msgnames = {
MSGTYPE_STR()
};
#undef DEFSTR

#define DEFSTR(id, str) offsetof(MsgNames, id),
static const unsigned msgnames_offsets[] = {
MSGTYPE_STR()
};

static_assert(sizeof(msgnames_offsets)/sizeof(msgnames_offsets[0]) == MessageTypeMaxValue,
              "Check the sizze of rules and enum emptok_type");


const char* msgtype2str(u8 mtyp)
{
   if (mtyp >= MessageTypeMaxValue) { return "ERROR invalid msg type"; }
   return msgnames.dummystr + msgnames_offsets[mtyp];
}

const MessageInfo msgtype[] = {
   [DataMp]            = { .len = MpGuiBasicDataSize },
   [DataNash]          = { .len = NashGuiBasicDataSize },
   [DataVarc]          = { .len = MessagePayloadArbitrary}, 
   [DataCarc]          = { .len = MessagePayloadArbitrary},
   [DataNarc]          = { .len = MessagePayloadArbitrary},
   [EventInit]         = { .len = 0 },
   [EventFini]         = { .len = 0 },
   [EventReset]        = { .len = 0 },
   [GuiIsReady]        = { .len = 0 },
   [GuiAck]            = { .len = 0 },
   [GuiAckReq]         = { .len = 0 },
   [GuiRaiseError]     = { .len = MessagePayloadArbitrary },
   [GuiIsClosing]      = { .len = 0 },
   [LogMsgSolver]      = { .len = MessagePayloadArbitrary },
   [LogMsgSubSolver]   = { .len = MessagePayloadArbitrary },
   [NewEmpDagStart]    = { .len = 0 },
   [NewEmpDagEnd]      = { .len = 0 },
   [NewModelStart]     = { .len = 0 },
   [NewModelName]      = { .len = 0 },
   [NewModelEnd]       = { .len = 0 },
   [RequestCoeffValue] = { .len = sizeof(u32) },
   [RequestEquById]    = { .len = sizeof(rhp_idx) },
   [RequestEquName]    = { .len = sizeof(rhp_idx) },
   [RequestMpById]     = { .len = sizeof(mpid_t) },
   [RequestMpName]     = { .len = sizeof(mpid_t) },
   [RequestNashById]   = { .len = sizeof(nashid_t) },
   [RequestNashName]   = { .len = sizeof(nashid_t) },
   [RequestVarById]    = { .len = sizeof(rhp_idx) },
   [RequestVarName]    = { .len = sizeof(rhp_idx) },
};

/* Payloads
 * EventInit:     str:model_name
 * EventFini:     none
 * EventReset:    none
 * EventNewModel: modelstats str:model_name 
 */


