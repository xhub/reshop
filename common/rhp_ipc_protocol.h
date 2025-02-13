#ifndef RESHOP_SOCKET_PROTOCOL
#define RESHOP_SOCKET_PROTOCOL

#include "reshop_config.h"

#include <stddef.h>
#include <stdint.h>
#include <stdlib.h>
#include <string.h>

#ifdef __cplusplus
extern "C" {
#endif

#include "empdag_data.h"
#include "rhp_defines.h"

typedef enum {
   CarcGuiData,
   CarcGuiDataEnd,
   EventInit,
   EventFini,
   EventReset,
   GuiIsReady,
   GuiAck,
   GuiAckReq,
   GuiRaiseError,
   GuiIsClosing,
   LogMsgSolver,
   LogMsgSubSolver,
   MathPrgmGuiData,
   MathPrgmGuiDataEnd,
   MathPrgmGuiName,     // FIXME: delete once proper name in empdag has been implemented
   NarcGuiData,
   NarcGuiDataEnd,
   NashGuiData,
   NashGuiDataEnd,
   NashGuiName,         // FIXME: delete once proper name in empdag has been implemented
   NewEmpDagStart,
   NewEmpDagEnd,
   NewModelStart,
   NewModelName,
   NewModelEnd,
   RarcGuiData,
   RarcGuiDataEnd,
   RequestCoeffValue,
   RequestEquById,
   RequestEquName,
   RequestMpById,
   RequestMpName,
   RequestNashById,
   RequestNashName,
   RequestVarById,
   RequestVarName,
   VarcGuiData,
   VarcGuiDataEnd,
   MessageTypeMaxValue,
} MessageType;

typedef struct {
   uint32_t len;

} MessageInfo;

#define MessagePayloadArbitrary UINT32_MAX

#define DEF_ARRAY(obj) \
typedef struct { \
  u32 len; \
  u32 max; \
  obj arr[]; \
} obj ## Array

typedef struct {
   u32 basename_idx;
} EmpDagName;

typedef struct {
   uint32_t id;
   uint8_t  sense;
   uint8_t  type;
   uint8_t  status;
   uint8_t  type_payload[3];
   uint32_t nparents;
   uint32_t nchildren;
   char *name;
} MathPrgmGui;

#define MpGuiBasicDataSize offsetof(MathPrgmGui, name)

typedef struct {
   uint32_t id;
   uint32_t nparents;
   uint32_t nchildren;
   char *name;
} NashGui;

#define NashGuiBasicDataSize offsetof(NashGui, name)

typedef struct {
   uint32_t id_child;
   int32_t vi;
   double cst;
} VarcBasicDatGui;

typedef struct {
   u32 max;
   u32 len;
   VarcBasicDatGui *darr; 
} VarcBasicDatGuiDArray;

typedef struct {
   uint8_t  type;
   uint32_t id_parent;
   union {
      VarcBasicDatGui basicdat;
      VarcBasicDatGuiDArray basicdat_arr;
   };
} VarcGui;

typedef struct {
   daguid_t daguid_child;
} CarcGui;

typedef struct {
   mpid_t mpid_child;
} NarcGui;

typedef struct {
   daguid_t daguid_parent;
} RarcGui;

DEF_ARRAY(MathPrgmGui);
DEF_ARRAY(NashGui);
DEF_ARRAY(VarcGui);
DEF_ARRAY(CarcGui);
DEF_ARRAY(NarcGui);
DEF_ARRAY(RarcGui);

typedef struct {
   uint32_t mps_len;
   uint32_t nashs_len;
} EmpDagSizes;

typedef struct {
   uint32_t  mdlid;
   uint8_t   type;
   uint8_t   stage;
   EmpDagNodeStats nodestats;
   EmpDagEdgeStats arcstats;
   MathPrgmGuiArray  *mps;
   NashGuiArray      *nashs;
   VarcGuiArray      *Varcs;
   CarcGuiArray      *Carcs;
   NarcGuiArray      *Narcs;
   RarcGuiArray      *Rarcs;
} EmpDagGui;

DEF_ARRAY(EmpDagGui);

#define EmpDagGuiBasicDataSize offsetof(EmpDagGui, mps)

typedef struct {
   uint8_t  backend;
   uint8_t  type;
   uint32_t id;
   uint32_t nvars;
   uint32_t nequs;
   char *name;
   EmpDagGuiArray* empdags;
} ModelGui;

#define ModelGuiBasicDataSize offsetof(ModelGui, name)

DEF_ARRAY(ModelGui);

#define MessagePayloadSize 4096
#define DesiredAlignement 32
#define SimpleLoadSize (MessagePayloadSize-DesiredAlignement)

#define MathPrgmGuiMax (SimpleLoadSize/MpGuiBasicDataSize)
#define NashGuiMax     (SimpleLoadSize/NashGuiBasicDataSize)
#define VarcGuiMax     (SimpleLoadSize/sizeof(VarcGui))
#define CarcGuiMax     (SimpleLoadSize/sizeof(CarcGui))
#define NarcGuiMax     (SimpleLoadSize/sizeof(NarcGui))
#define RarcGuiMax     (SimpleLoadSize/sizeof(RarcGui))
#define StrMaxLen      (SimpleLoadSize/sizeof(char))



typedef struct {
   uint32_t length;     // Length of the payload
   uint8_t type;        // Request type
   uint8_t reserved[3]; // Reserved bytes
   uint8_t pad[24]; /**< Padding to make simple.load aligned */
} MessageHeader;

RESHOP_STATIC_ASSERT(sizeof(MessageHeader) == DesiredAlignement, "Adapt padding in MessageHeader")

/*
 * For better performance, we want simple.load to be 32 or bits aligned.
 */
typedef union {
   uint8_t buf[MessagePayloadSize];
   struct {
      MessageHeader header;
      union {
         uint8_t load[SimpleLoadSize];
         char name[StrMaxLen];
 
         ModelGui mdl;
         EmpDagGui empdag;
         MathPrgmGui mps[MathPrgmGuiMax];
         NashGui nashs[NashGuiMax];
         VarcGui varcs[VarcGuiMax];
         CarcGui carcs[CarcGuiMax];
         NarcGui narcs[NarcGuiMax];
         RarcGui rarcs[RarcGuiMax];
      };
   } load;

} MessagePayload;

RESHOP_STATIC_ASSERT(offsetof(MessagePayload, load.load) % DesiredAlignement == 0, "Check padding for load field in MessagePayload")

RESHOP_STATIC_ASSERT(offsetof(MessagePayload, load.mdl) == sizeof(MessageHeader), "Check MessagePayload")

RESHOP_STATIC_ASSERT(CHAR_BIT == 8, "MessagePayload needs porting to an arch where CHAR_BIT != 8")

#define MessageSimplePayloadSize (offsetof(MessagePayload, mps))

#define NewModelStartPayload (offsetof(MessagePayload,  load.mdl) + ModelGuiBasicDataSize)
#define NewEmpDagStartPayload (offsetof(MessagePayload, load.empdag) + EmpDagGuiBasicDataSize)

extern const MessageInfo msgtype[MessageTypeMaxValue];
const char* msgtype2str(uint8_t mtyp);

#ifdef __cplusplus
}
#endif

#endif
