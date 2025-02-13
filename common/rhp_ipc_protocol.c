
#include <stddef.h>
#include <string.h>

#ifdef HAS_UNISTD
#include <unistd.h>
#endif

#include "rhp_ipc_protocol.h"

#define MSGTYPE_STR() \
DEFSTR(CarcGuiData,"CarcGuiData") \
DEFSTR(CarcGuiDataEnd,"CarcGuiDataEnd") \
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
DEFSTR(NarcGuiData,"NarcGuiData") \
DEFSTR(NarcGuiDataEnd,"NarcGuiDataEnd") \
DEFSTR(NashGuiData,"NashGuiData") \
DEFSTR(NashGuiDataEnd,"NashGuiDataEnd") \
DEFSTR(NashGuiName,"NashGuiName") \
DEFSTR(NewEmpDagStart,"NewEmpDagStart") \
DEFSTR(NewEmpDagEnd,"NewEmpDagEnd") \
DEFSTR(NewModelStart,"NewModelStart") \
DEFSTR(NewModelName,"NewModelName") \
DEFSTR(NewModelEnd,"NewModelEnd") \
DEFSTR(MathPrgmGuiData,"MathPrgmGuiData") \
DEFSTR(MathPrgmGuiDataEnd,"MathPrgmGuiDataEnd") \
DEFSTR(MathPrgmGuiName,"MathPrgmGuiName") \
DEFSTR(RarcGuiData,"RarcGuiData") \
DEFSTR(RarcGuiDataEnd,"RarcGuiDataEnd") \
DEFSTR(RequestCoeffValue,"RequestCoeffValue") \
DEFSTR(RequestEquById,"RequestEquById") \
DEFSTR(RequestEquName,"RequestEquName") \
DEFSTR(RequestMpById,"RequestMpById") \
DEFSTR(RequestMpName,"RequestMpName") \
DEFSTR(RequestNashById,"RequestNashById") \
DEFSTR(RequestNashName,"RequestNashName") \
DEFSTR(RequestVarById,"RequestVarById") \
DEFSTR(RequestVarName,"RequestVarName") \
DEFSTR(VarcGuiData,"VarcGuiData") \
DEFSTR(VarcGuiDataEnd,"VarcGuiDataEnd") \


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
              "Check that all size of MessageType");


const char* msgtype2str(u8 mtyp)
{
   if (mtyp >= MessageTypeMaxValue) { return "ERROR invalid msg type"; }
   return msgnames.dummystr + msgnames_offsets[mtyp];
}

const MessageInfo msgtype[] = {
   [CarcGuiData]       = { .len = MessagePayloadArbitrary },
   [CarcGuiDataEnd]    = { .len = MessagePayloadArbitrary },
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
   [MathPrgmGuiData]   = { .len = MessagePayloadArbitrary },
   [MathPrgmGuiDataEnd]= { .len = MessagePayloadArbitrary },
   [MathPrgmGuiName]   = { .len = MessagePayloadArbitrary },
   [NarcGuiData]       = { .len = MessagePayloadArbitrary },
   [NarcGuiDataEnd]    = { .len = MessagePayloadArbitrary },
   [NewEmpDagStart]    = { .len = 0 },
   [NewEmpDagEnd]      = { .len = 0 },
   [NewModelStart]     = { .len = ModelGuiBasicDataSize },
   [NewModelName]      = { .len = MessagePayloadArbitrary },
   [NewModelEnd]       = { .len = 0 },
   [RarcGuiData]       = { .len = MessagePayloadArbitrary },
   [RarcGuiDataEnd]    = { .len = MessagePayloadArbitrary },
   [RequestCoeffValue] = { .len = sizeof(u32) },
   [RequestEquById]    = { .len = sizeof(rhp_idx) },
   [RequestEquName]    = { .len = sizeof(rhp_idx) },
   [RequestMpById]     = { .len = sizeof(mpid_t) },
   [RequestMpName]     = { .len = sizeof(mpid_t) },
   [RequestNashById]   = { .len = sizeof(nashid_t) },
   [RequestNashName]   = { .len = sizeof(nashid_t) },
   [RequestVarById]    = { .len = sizeof(rhp_idx) },
   [RequestVarName]    = { .len = sizeof(rhp_idx) },
   [VarcGuiData]       = { .len = MessagePayloadArbitrary },
   [VarcGuiDataEnd]    = { .len = MessagePayloadArbitrary },
};

/* Payloads
 * EventInit:     str:model_name
 * EventFini:     none
 * EventReset:    none
 * EventNewModel: modelstats str:model_name 
 */


