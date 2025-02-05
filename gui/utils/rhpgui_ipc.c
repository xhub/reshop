#include "reshop_config.h"

#include <errno.h>
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

/* Using this macro requires the label neg_bytes_read to be defined */
#define READ_MSG(msg, errmsg, ...) { \
   ssize_t bytes_read = read(fd, (msg)->buf, sizeof((msg)->buf)); \
   if (bytes_read == -1) { \
      if (errno == EAGAIN || errno == EWOULDBLOCK) { \
         sockerr_log2("[IPC] 'read' on client %d", fd); \
         return 1; \
      } \
\
      goto neg_bytes_read; \
   } \
\
   if (bytes_read == 0) { \
      sockerr_log2("[IPC] 'read' on client %d returned 0, " errmsg, \
                fd, __VA_ARGS__); \
      return 1; \
   } \
}


#define READ_LOAD(load_, read_cnt, msg_len, ACTION) { \
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
   }  while (rem_sz && (read_cnt) > 0); \
\
   if (total_read_cnt != (msg_len)) { \
      sockerr_log2("[IPC] ERROR: expected %u bytes, read %u\n", \
               msg_len, total_read_cnt); \
   } \
}


#define DEBUG_MSG(...) loggerfmt(0, __VA_ARGS__)

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
   MessageHeader res_header = { 0, GuiAckReq, {reqtype, 0, 0} };
   if (write(fd, &res_header, sizeof(res_header)) == -1) {
      sockerr_log2("[IPC] ERROR while sending ackreq to client %d and request %u",
                fd, reqtype);
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
      loggerfmt(ErrorGui, "[IPC] System ERROR: could not set fd %d as blocking\n", fd);
      return 1;
   }

   EmpDagGui * restrict load = &msg->simple.empdag;

   u32 mdlid = load->mdlid;

   ModelGui * restrict mdlgui;
   if (RHP_UNLIKELY(mdlid >= guidat->models->len)) {
      loggerfmt(ErrorGui, "[IPC] ERROR: while adding new EMPDAG: model ID %u not in [0,%u)\n",
                fd, mdlid, guidat->models->len);
      return 1;
   }

   mdlgui = &guidat->models->arr[mdlid]; assert(mdlgui);

   arr_add(mdlgui->empdags, *load);

   

   if (fd_set_nonblocking(fd)) {
      loggerfmt(ErrorGui, "[IPC] System ERROR: could not set fd %d as blocking\n", fd);
      return 1;
   }

   return 0;
}


static int newmodel(rhpfd_t fd, MessagePayload * restrict msg,
                    GuiData * restrict guidat)
{

   assert(NewModelStart == msg->simple.header.type);
   if (fd_set_blocking(fd)) {
      loggerfmt(ErrorGui, "[IPC] System ERROR: could not set fd %d as blocking\n", fd);
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

   arr_add(guidat->models, msg->simple.mdl);
   ModelGui *mdl = &guidat->models->arr[guidat->models->len-1];
   arr_init_size(mdl->empdags, 2);

   if (send_ackreq(fd, NewModelName)) { return 1; }

   READ_MSG(msg, ", while expecting the model name (ID=%u)\n", mdl->id)

   char * restrict mdlname = mdl->name;
   u8 * restrict load = msg->simple.load;
   u32 namelen = msg->simple.header.length;
   MALLOC_GUI(mdl->name, char, namelen);
   ssize_t read_cnt = 0;
   READ_LOAD(load, read_cnt, namelen,
             {memcpy(mdlname, load, read_cnt); mdlname += read_cnt; }); 

   if (send_ack(fd)) { return 1; }

   /* Get either end of transmission or empdag */
   READ_MSG(msg, ", while expecting the end of transmission or an EMPDAG for model '%s'\n",
            mdl->name)

   MessageType type = msg->simple.header.type;
   switch (type) {
   case NewModelEnd:
      break;
   case NewEmpDagStart:
      return newempdag(fd, msg, guidat);
   default: 
      loggerfmt(ErrorGui, "[IPC] ERROR while reading fd %d: expecting a message "
                "of type '%s' or '%s', got '%s'\n", fd, msgtype2str(NewModelEnd),
                msgtype2str(NewEmpDagStart), msgtype2str(type));
      return 1;
   }

   if (fd_set_nonblocking(fd)) {
      loggerfmt(ErrorGui, "[IPC] System ERROR: could not set fd %d as blocking\n", fd);
   }

   return 0;

neg_bytes_read:
   sockerr_log("[IPC] ERROR while reading fd");
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
      bytes_read = read(fd, msg.buf, sizeof(msg.buf));
      if (bytes_read == -1) {
         if (errno == EAGAIN || errno == EWOULDBLOCK) {
            sockerr_log2("[IPC] 'read' on client %d", fd)
            return false;
         }

         goto neg_bytes_read;
      }

      if (bytes_read == 0) { return false; }

      u32 msg_len = msg.simple.header.length;
      MessageType msg_type = msg.simple.header.type;

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
      case DataMp:
      case DataNash:
      case DataVarc:
      case DataCarc:
         loggerfmt(ErrorGui, "[IPC] ERROR: invalid message type '%s' outside of a "
                  "new empdag definition", msgtype2str(msg_type));
         return false;

      case LogMsgSolver: {
         MessageHeader header = msg.simple.header;
         u8 lvl = header.reserved[0], *load = msg.simple.load;
         READ_LOAD(load, bytes_read, msg_len,
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

neg_bytes_read:
   sockerr_log2("[IPC] ERROR while reading fd %d: '%s'", fd)
   return true; // No data or client disconnected
}
