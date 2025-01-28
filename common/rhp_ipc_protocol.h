#ifndef RESHOP_SOCKET_PROTOCOL
#define RESHOP_SOCKET_PROTOCOL

#include "reshop_config.h"

#include <stddef.h>
#include <stdint.h>
#include <string.h>

#ifdef __cplusplus
extern "C" {
#endif

#include "empdag_data.h"
#include "rhp_defines.h"

typedef enum {
   DataMp,
   DataNash,
   DataVarc,
   DataCarc,
   DataNarc,
   EventInit,
   EventFini,
   EventReset,
   GuiIsReady,
   GuiAck,
   GuiRaiseError,
   GuiIsClosing,
   LogMsgSolver,
   LogMsgSubSolver,
   NewEmpDagStart,
   NewEmpDagEnd,
   NewModelStart,
   NewModelName,
   NewModelEnd,
   RequestMpById,
   RequestMpName,
   RequestNashById,
   RequestNashName,
   RequestVarById,
   RequestVarName,
   RequestEquById,
   RequestEquName,
   RequestCoeffValue,
   MessageTypeMaxValue,
} MessageType;

typedef struct {
    uint32_t length;     // Length of the payload
    uint8_t type;        // Request type
    uint8_t reserved[3]; // Reserved bytes
} MessageHeader;

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

#define arr_nbytes(a, n) (sizeof(*(a)) + (sizeof(*((a)->arr))) * n)

#define arr_add(a, elt) { \
   if ((a)->len >= (a)->max) { \
      (a)->max = MAX(2*(a)->max, (a)->len+10); \
      (a) = myrealloc((a)->arr, arr_nbytes((a), (a)->max)); \
      if (!(a)) { \
         fprintf(stderr, "FATAL ERROR: allocation failure\n"); \
         exit(EXIT_FAILURE); \
      } \
   } \
   (a)->arr[(a)->len++] = elt; \
}

#define mymalloc malloc

#define arr_init_size(a, n) { \
   (a) = mymalloc(arr_nbytes(a, n))) ; \
   if (!(a)) { \
      fprintf(stderr, "FATAL ERROR: allocation failure\n"); \
      exit(EXIT_FAILURE); \
   } \
   (a)->len = 0; (a)->max = n; \
}

#define arr_add_nochk(a, elt) { \
   assert(a->len < a->max); \
   a->arr[a->len++] = elt; \
}

#define arr_getnext(a) { \
   a->arr[a->len++] = elt; \
   assert(a->len <= a->max); \
}

#define arr_free(a, fn) if (a) { \
   for (u32 i = 0, len = (a)->len; i < len; ++i) { \
      fn(&(a)->arr[i]); \
   } \
}

#define darr_free(da, fn) if (da) { \
   for (u32 i = 0, len = (da)->len; i < len; ++i) { \
      fn(&(da)->darr[i]); \
   } \
   free((da)->darr); \
}
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
#define SimpleLoadSize (MessagePayloadSize-sizeof(MessageHeader))

#define MathPrgmGuiMax (MessagePayloadSize/MpGuiBasicDataSize)
#define NashGuiMax (MessagePayloadSize/NashGuiBasicDataSize)
#define VarcGuiMax (MessagePayloadSize/MpGuiBasicDataSize)
#define CarcGuiMax (MessagePayloadSize/MpGuiBasicDataSize)
#define NarcGuiMax (MessagePayloadSize/MpGuiBasicDataSize)

typedef union {
   uint8_t buffer[MessagePayloadSize];
   struct {
      MessageHeader header;
      union {
         uint8_t load[SimpleLoadSize];
         ModelGui mdl;
         EmpDagGui empdag;
      };
   } simple;
   MathPrgmGui mps[MathPrgmGuiMax];
   NashGui nashs[NashGuiMax];
   VarcGui varcs[VarcGuiMax];
   CarcGui carcs[CarcGuiMax];
   NarcGui narcs[NarcGuiMax];
} MessagePayload;


#define MessageSimplePayloadSize (offsetof(MessagePayload, mps))

RESHOP_STATIC_ASSERT(sizeof(MessageHeader) == offsetof(MessagePayload, simple.load), "review MessagePayload")

#define NewModelStartPayload (offsetof(MessagePayload, simple.mdl) + ModelGuiBasicDataSize)
#define NewEmpDagStartPayload (offsetof(MessagePayload, simple.empdag) + EmpDagGuiBasicDataSize)

extern const MessageInfo msgtype[MessageTypeMaxValue];
const char* msgtype2str(uint8_t mtyp);

#ifdef __cplusplus
}
#endif

#endif
