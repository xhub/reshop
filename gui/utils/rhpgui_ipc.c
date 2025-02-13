#include "reshop_config.h"

#include <errno.h>
#include <inttypes.h>
#include <string.h>

#ifdef HAS_UNISTD
#include <unistd.h>
#endif

#ifdef _WIN32
#include <io.h>
#include <BaseTsd.h>
typedef SSIZE_T ssize_t;
#endif

#include "rhp_gui_data.h"
#include "rhp_ipc_protocol.h"
#include "rhp_socket_server.h"
#include "rhpgui_error.h"
#include "rhpgui_ipc.h"
#include "rhpgui_array_utils.h"

#define DEBUG_MSG(...) loggerfmt(0, __VA_ARGS__)

/* Using this macro requires the label neg_bytes_read to be defined */
#define READ_MSG(msg, errmsg, ...) { \
   ssize_t bytes_read = read(fd, (msg)->buf, sizeof((msg)->buf)); \
   if (bytes_read == -1) { \
      if (errno == EAGAIN || errno == EWOULDBLOCK) { \
         sockerr_log2("[IPC] 'read' on client "FDP, fd); \
         return 1; \
      } \
\
      goto neg_bytes_read; \
   } \
\
   if (bytes_read == 0) { \
      sockerr_log2("[IPC] 'read' on client "FDP" returned 0, " errmsg, \
                fd, __VA_ARGS__); \
      return 1; \
   } \
}


#define READ_LOGMSG(load_, read_cnt, msg_len, ACTION) { \
   u32 total_read_cnt = 0, rem_sz = msg_len; \
   do { \
       u32 expected_sz = rem_sz <= MessagePayloadSize ? rem_sz : MessagePayloadSize; \
       (read_cnt) = read(fd, (load_), expected_sz); \
\
       if ((read_cnt) <= 0) { goto neg_bytes_read; } \
\
      ACTION \
\
       rem_sz -= expected_sz; \
       total_read_cnt += expected_sz; \
\
   } while (rem_sz && (read_cnt) > 0); \
\
   if (total_read_cnt != (msg_len)) { \
      sockerr_log2("[IPC] ERROR: expected %u bytes, read %u", \
               msg_len, total_read_cnt); \
   } \
}

#define READ_LOAD(load_, read_cnt, msg_len, ACTION) { \
   u32 total_read_cnt = (msg_len); \
   u32 rem_sz = (msg_len) > SimpleLoadSize ? (msg_len) - SimpleLoadSize : 0; \
   ACTION \
   while (rem_sz && (read_cnt) > 0) { \
       u32 expected_sz = rem_sz <= MessagePayloadSize ? rem_sz : MessagePayloadSize; \
       (read_cnt) = read(fd, (load_), expected_sz); \
\
       if ((read_cnt) <= 0) { goto neg_bytes_read; } \
\
      ACTION \
\
       rem_sz -= expected_sz; \
       total_read_cnt += expected_sz; \
\
   } \
\
   if (total_read_cnt != (msg_len)) { \
      sockerr_log2("[IPC] ERROR: expected %u bytes, read %u", \
               msg_len, total_read_cnt); \
   } \
}

#define READ_FD(fd, msg) { \
   ssize_t bytes_read42 = read(fd, (msg)->buf, sizeof((msg)->buf)); \
   if (bytes_read42 == -1) { \
      if (errno == EAGAIN || errno == EWOULDBLOCK) { \
         sockerr_log2("[IPC] 'read' on client " FDP, fd) \
         return 1; \
      } \
\
      goto neg_bytes_read; \
   } \
}

#define READ_FD_NZ(fd, msg) { \
   ssize_t bytes_read42 = read(fd, (msg)->buf, sizeof((msg)->buf)); \
   if (bytes_read42 == -1) { \
      if (errno == EAGAIN || errno == EWOULDBLOCK) { \
         sockerr_log2("[IPC] 'read' on client " FDP, fd) \
         return 1; \
      } \
\
      goto neg_bytes_read; \
   } \
\
   if (bytes_read42 == 0) { \
      sockerr_log2("[IPC] 'read' of size 0 on client " FDP, fd) \
      return 1; \
   } \
}

#define READ_FD_SIZED(fd, load, sz) { \
   ssize_t bytes_read42 = read(fd, load, sz); \
   if (bytes_read42 == -1) { \
      if (errno == EAGAIN || errno == EWOULDBLOCK) { \
         sockerr_log2("[IPC] 'read' on client " FDP, fd) \
         return 1; \
      } \
\
      goto neg_bytes_read; \
   } \
\
   if (bytes_read42 == 0) { \
      sockerr_log2("[IPC] 'read' of size 0 on client " FDP, fd) \
      return 1; \
   } \
\
   if (bytes_read42 != (sz)) { \
      sockerr_log2("[IPC] 'read' of size %u expected, got %zd on client " FDP, sz, bytes_read42, fd) \
      return 1; \
   } \
}

#define read_expect(fd, msg, msgtype) { \
   READ_FD(fd, msg); \
   if ((msg)->load.header.type != (msgtype)) { \
      loggerfmt(ErrorGui, "[IPC] ERROR: expected a message of type '%s', got '%s'\n", \
                   msgtype2str(msgtype), msgtype2str((msg)->load.header.type)); \
   } \
}

static inline unsigned send_ack(rhpfd_t fd)
{
   MessageHeader res_header = { 0, GuiAck, {0} };
   if (write(fd, &res_header, sizeof(res_header)) == -1) {
      sockerr_log("[IPC] ERROR while sending ack to client");
   }

   return 0;
}

static inline unsigned send_ackreq(rhpfd_t fd, MessageType reqtype)
{
   MessageHeader res_header = { 0, GuiAckReq, {reqtype, 0, 0}, {0} };
   if (write(fd, &res_header, sizeof(res_header)) == -1) {
      sockerr_log2("[IPC] ERROR while sending ackreq to client " FDP " and request %u",
                fd, reqtype);
      return 1;
   }

   return 0;
}

static inline unsigned send_guierr(rhpfd_t fd)
{
   MessageHeader res_header = { 0, GuiRaiseError, {0}, {0} };
   if (write(fd, &res_header, sizeof(res_header)) == -1) {
      sockerr_log2("[IPC] ERROR while sending %s to client " FDP, fd);
      return 1;
   }

   return 0;
}

static int newempdag(rhpfd_t fd, MessagePayload * restrict msg,
                     GuiData * restrict guidat)
{
  /* ----------------------------------------------------------------------
   * Protocol:
   * --->    ReSHOP sends a EmpDagGui struct
   * <---    GUI ACKs and ask for a model name
   * --->    ReSHOP sends the model name
   * <---    GUI ACKs
   *
   * If there is an EMPDAG:
   * --->    ReSHOP sends a basic EmpDagGui
   * The function newempdag is called to receive it
   *
   * If there is no EMPDAG:
   * --->    ReSHOP sends an end of transmission
   * The processing is done and we return to the callee
   * ---------------------------------------------------------------------- */

   if (fd_set_blocking(fd)) {
      loggerfmt(ErrorGui, "[IPC] System ERROR: could not set fd "FDP" as blocking\n", fd);
      return 1;
   }

   EmpDagGui * restrict load = &msg->load.empdag;

   u32 mdlid = load->mdlid;

   ModelGui * restrict mdlgui;
   if (RHP_UNLIKELY(mdlid >= guidat->models->len)) {
      loggerfmt(ErrorGui, "[IPC] ERROR: while adding new EMPDAG: model ID %u not in [0,%u)\n",
                fd, mdlid, guidat->models->len);
      send_guierr(fd);
      return 1;
   }

   mdlgui = &guidat->models->arr[mdlid]; assert(mdlgui);

   arr_add(mdlgui->empdags, *load);

   if (send_ack(fd)) { return 1; }

   EmpDagGui *empdag = arr_getlast(mdlgui->empdags);

   u32 num_mps = empdag->nodestats.num_mps;
   u32 num_nashs = empdag->nodestats.num_nashs;
   arr_init_size(empdag->mps, num_mps);
   arr_init_size(empdag->nashs, num_nashs);
   arr_init_size(empdag->Varcs, num_mps);
   arr_init_size(empdag->Carcs, num_mps);
   arr_init_size(empdag->Narcs, num_nashs);
   arr_init_size(empdag->Rarcs, num_mps + num_nashs);

   DEBUG_MSG("[newempdag] empdag with %u MPs and %u Nash nodes\n", num_mps, num_nashs);

  /* ----------------------------------------------------------------------
   * Get all the MPs
   * ---------------------------------------------------------------------- */

   MathPrgmGui * restrict mpgui_arr = empdag->mps->arr; assert(mpgui_arr);
   do {
      READ_FD_NZ(fd, msg);
      u32 nmps = msg->load.header.length;
      MathPrgmGui * restrict mpgui_msg = msg->load.mps;
 
      // FIXME: once we have proper name, see if one mempcy call is sufficient
      for (u32 i = 0; i < nmps; ++i) {
         memcpy(mpgui_arr++, mpgui_msg++, MpGuiBasicDataSize);
      }
      if (send_ack(fd)) { return 1; }
   } while (msg->load.header.type == MathPrgmGuiData);

   if (msg->load.header.type != MathPrgmGuiDataEnd) {
      loggerfmt(ErrorGui, "[IPC] ERROR while receiving MPs: expecting msg type "
                "'%s', got '%s'\n", msgtype2str(MathPrgmGuiDataEnd),
                msgtype2str(msg->load.header.type));
      return 1;
   }

   empdag->mps->len = num_mps;
   mpgui_arr = empdag->mps->arr;

   // FIXME: Change this once we have proper naming
   for (u32 i = 0; i < num_mps; ++i, mpgui_arr++) {
      READ_FD_NZ(fd, msg);

      if (msg->load.header.type != MathPrgmGuiName) {
         loggerfmt(ErrorGui, "[IPC] ERROR while receiving MP name "PRIu32": "
                   "expecting msg type '%s', got '%s'\n", i,
                   msgtype2str(MathPrgmGuiDataEnd), msgtype2str(msg->load.header.type));
         return 1;
      }

      mpgui_arr->name = strdup(msg->load.name);

      DEBUG_MSG("[newempdag] MP #%u is '%s'\n", i, mpgui_arr->name);
      if (send_ack(fd)) { return 1; } 
   }

  /* ----------------------------------------------------------------------
   * Get all the Nashs
   * ---------------------------------------------------------------------- */

   if (num_nashs > 0) {
      NashGui * restrict nashgui_arr = empdag->nashs->arr; assert(nashgui_arr);
      do {
         READ_FD_NZ(fd, msg);
         u32 nnashs = msg->load.header.length;
         NashGui * restrict nashgui_msg = msg->load.nashs;
 
         // FIXME: once we have proper name, see if one mempcy call is sufficient
         for (u32 i = 0; i < nnashs; ++i) {
            memcpy(nashgui_arr++, nashgui_msg++, NashGuiBasicDataSize);
         }
         if (send_ack(fd)) { return 1; }
      } while (msg->load.header.type == NashGuiData);

      if (msg->load.header.type != NashGuiDataEnd) {
         loggerfmt(ErrorGui, "[IPC] ERROR while receiving MPs: expecting msg type "
                   "'%s', got '%s'\n", msgtype2str(NashGuiDataEnd),
                   msgtype2str(msg->load.header.type));
         return 1;
      }

      assert(num_nashs == nashgui_arr - empdag->nashs->arr);
      nashgui_arr = empdag->nashs->arr;
      empdag->nashs->len = num_nashs;

      // FIXME: Change this once we have proper naming
      for (u32 i = 0; i < num_nashs; ++i, nashgui_arr++) {
         READ_FD_NZ(fd, msg);

         if (msg->load.header.type != NashGuiName) {
            loggerfmt(ErrorGui, "[IPC] ERROR while receiving MP name "PRIu32": "
                      "expecting msg type '%s', got '%s'\n", i,
                      msgtype2str(NashGuiDataEnd), msgtype2str(msg->load.header.type));
            return 1;
         }

         nashgui_arr->name = strdup(msg->load.name);

         DEBUG_MSG("[newempdag] Nash #%u is '%s'\n", i, nashgui_arr->name);
         if (send_ack(fd)) { return 1; } 
      }
   }

   /* End of transmission */
   read_expect(fd, msg, NewModelEnd);
   if (send_ack(fd)) { return 1; }

   if (fd_set_nonblocking(fd)) {
      loggerfmt(ErrorGui, "[IPC] System ERROR: could not set fd " FDP " as blocking\n", fd);
      return 1;
   }

   return 0;

neg_bytes_read:
   sockerr_log2("[IPC] ERROR while reading fd " FDP, fd);
   return 1; // No data or client disconnected
}


static int newmodel(rhpfd_t fd, MessagePayload * restrict msg,
                    GuiData * restrict guidat)
{

   assert(NewModelStart == msg->load.header.type);
   if (fd_set_blocking(fd)) {
      loggerfmt(ErrorGui, "[IPC] System ERROR: could not set fd "FDP" as blocking\n", fd);
   }
 
  /* ----------------------------------------------------------------------
   * Protocol:
   * --->    ReSHOP sends a ModelGui struct
   * <---    GUI ACKs and ask for a model name
   * --->    ReSHOP sends the model name
   * <---    GUI ACKs
   *
   * If there is an EMPDAG:
   * --->    ReSHOP sends a basic EmpDagGui
   * The function newempdag is called to receive it
   *
   * If there is no EMPDAG:
   * --->    ReSHOP sends an end of transmission
   * The processing is done and we return to the callee
   * ---------------------------------------------------------------------- */

   READ_FD_SIZED(fd,  &msg->load.mdl, ModelGuiBasicDataSize);

   arr_add(guidat->models, msg->load.mdl);
   ModelGui *mdl = &guidat->models->arr[guidat->models->len-1];
   arr_init_size(mdl->empdags, 2);

   if (send_ackreq(fd, NewModelName)) { return 1; }

   READ_MSG(msg, ", while expecting the model name (ID=%u)\n", mdl->id)

   assert(msg->load.header.type == NewModelName);

   u8 * restrict load = msg->load.load;
   u32 namelen = msg->load.header.length;
   MALLOC_GUI(mdl->name, char, namelen);
   char * restrict mdlname = mdl->name;
   ssize_t read_cnt = namelen;
   READ_LOAD(load, read_cnt, namelen,
             {memcpy(mdlname, load, read_cnt); mdlname += read_cnt;}); 

   DEBUG_MSG("[newmodel] model '%s' #%u\n", mdl->name, mdl->id);

   if (send_ack(fd)) { return 1; }

   /* Get either end of transmission or empdag */
   READ_MSG(msg, ", while expecting the end of transmission or an EMPDAG for model '%s'\n",
            mdl->name)

   MessageType type = msg->load.header.type;
   switch (type) {
   case NewModelEnd:
      break;
   case NewEmpDagStart:
      return newempdag(fd, msg, guidat);
   default: 
      loggerfmt(ErrorGui, "[IPC] ERROR while reading fd "FDP": expecting a message "
                "of type '%s' or '%s', got '%s'\n", fd, msgtype2str(NewModelEnd),
                msgtype2str(NewEmpDagStart), msgtype2str(type));
      return 1;
   }

   if (send_ack(fd)) { return 1; }

   if (fd_set_nonblocking(fd)) {
      loggerfmt(ErrorGui, "[IPC] System ERROR: could not set fd "FDP" as nonblocking\n", fd);
   }

   return 0;

neg_bytes_read:
   sockerr_log2("[IPC] ERROR while reading fd " FDP, fd);
   return 1; // No data or client disconnected
}


/**
 * @brief Handles a incoming message on the socket
 *
 * @param fd         the client fd
 * @param guidat     the GUI data
 *
 * @return           true if the connection is to be closed
 */
bool gui_ipc_handle_client(rhpfd_t fd, GuiData *guidat)
{
   // FIXME: msg should have the same alignement as DesiredAlignement
   MessagePayload msg;
   ssize_t bytes_read;

   while (true) {
      bytes_read = read(fd, &msg.load.header, sizeof(msg.load.header));
      if (bytes_read == -1) {
         if (errno == EAGAIN || errno == EWOULDBLOCK) {
            //sockerr_log2("[IPC] 'read' on client "FDP, fd)
            return false;
         }

         goto neg_bytes_read;
      }

      if (bytes_read == 0) { return false; }

      u32 msg_len = msg.load.header.length;
      MessageType msg_type = msg.load.header.type;

      if (msg_type >= MessageTypeMaxValue) {
         loggerfmt(ErrorGui, "[IPC] ERROR: received message with type %u, larger "
                  "than max value %u", msg_type, MessageTypeMaxValue-1);
         return false;
      }

      u32 msgtype_expected_len = msgtype[msg_type].len;
      if (msgtype_expected_len < UINT32_MAX && msg_len != msgtype_expected_len) {
         loggerfmt(ErrorGui, "[IPC] ERROR: msg type '%s' expected payload has size %u, "
                   "received %u", msgtype2str(msg_type), msgtype_expected_len,
                   msg_len);
         return false;
      }

      //DEBUG_MSG("[IPC] Received request '%s' of length=%u\n", msgtype2str(msg_type), msg_len);

      switch (msg_type) {
      case EventInit:
         guidat->connected = true;
         send_ack(fd);
         DEBUG_MSG("[IPC] Connected to solver on fd=" FDP "\n", fd);
         break;
      case EventFini:
         guidat->connected = false;
         loggerfmt(InfoGui, "[IPC] Disconnected from solver\n");
         break;
      case EventReset:
         break;
      case NewModelStart:
         newmodel(fd, &msg, guidat);
         break;
      case NewEmpDagStart:
         newempdag(fd, &msg, guidat);
         break;
      case MathPrgmGuiData:
      case MathPrgmGuiDataEnd:
      case MathPrgmGuiName:
      case NashGuiData:
      case NashGuiDataEnd:
      case NashGuiName:
      case CarcGuiData:
      case CarcGuiDataEnd:
      case NarcGuiData:
      case NarcGuiDataEnd:
      case RarcGuiData:
      case RarcGuiDataEnd:
      case VarcGuiData:
      case VarcGuiDataEnd:
         loggerfmt(ErrorGui, "[IPC] ERROR: invalid message type '%s' outside of a "
                  "new empdag definition", msgtype2str(msg_type));
         return false;

      case LogMsgSolver: {
         MessageHeader header = msg.load.header;
         u8 lvl = header.reserved[0], *load = msg.load.load;
         READ_LOGMSG(load, bytes_read, msg_len,
                     {load[bytes_read] = '\0'; loggerfmt(lvl, (const char*)load);})
         break;
      }
      /* these are invalid */
      case GuiIsReady:
      case GuiAck:
      case GuiRaiseError:
      case GuiIsClosing:
      case RequestMpById:
      case RequestMpName:
      case RequestNashById:
      case RequestNashName:
      case RequestVarById:
      case RequestVarName:
      case RequestEquById:
      case RequestEquName:
      case RequestCoeffValue:

         loggerfmt(ErrorGui, "[IPC] ERROR: invalid message type '%s'",
                     msgtype2str(msg_type));
         return false;

      case LogMsgSubSolver:
      default:
         loggerfmt(ErrorGui, "[IPC] ERROR: unhandled message type '%s'",
                     msgtype2str(msg_type));
         return false;
      }

   }

   return false;

neg_bytes_read:
   sockerr_log2("[IPC] ERROR while reading fd "FDP, fd)
   return true; // No data or client disconnected
}
