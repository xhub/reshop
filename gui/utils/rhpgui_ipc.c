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

#define DEBUG_MSG(...) loggerfmt(0, __VA_ARGS__)

#include "rhp_gui_data.h"
#include "rhp_ipc_protocol.h"
#include "rhpgui_ipc.h"

static void newmodel(int client_fd, GuiData *guidat)
{
   return;
}

static void newempdag(int client_fd, GuiData *guidat)
{
   return;
}

static inline void send_guiack(int client_fd)
{
   MessageHeader res_header = { 0, GuiAck, {0} };
   if (write(client_fd, &res_header, sizeof(res_header)) == -1) {
      loggerfmt(ErrorGui, "[IPC] ERROR while sending ack to client %d: '%s'\n",
               client_fd, strerror(errno));
   }
}

/**
 * @brief Handles a incoming message on the socket
 *
 * @param client_fd  the client fd
 * @param guidat     the GUI data
 *
 * @return           true if the connection is to be closed
 */
bool gui_ipc_handle_client(int fd, GuiData *guidat)
{
   u8 buffer[4096];
   ssize_t bytes_read;

   while (true) {
      MessageHeader header;
      bytes_read = read(fd, &header, sizeof(header));
      if (bytes_read == -1) {
         if (errno == EAGAIN || errno == EWOULDBLOCK) {
            return false;
         }
         goto neg_bytes_read;
      }

      if (bytes_read == 0) {
         return false;
      }

      u32 msg_len = header.length;
      MessageType msg_type = header.type;

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
         send_guiack(fd);
         DEBUG_MSG("[IPC] Connected to solver on fd=%d\n", fd);
         break;
      case EventFini:
         guidat->connected = false;
         loggerfmt(InfoGui, "[IPC] Disconnected from solver\n");
         break;
      case EventReset:
         break;
      case NewModelStart:
         newmodel(fd, guidat);
         break;
      case NewEmpDagStart:
         newempdag(fd, guidat);
         break;
      case DataMp:
      case DataNash:
      case DataVarc:
      case DataCarc:
         loggerfmt(ErrorGui, "[IPC] ERROR: invalid message type '%s' outside of a "
                  "new empdag definition", msgtype2str(msg_type));
         return false;

      case LogMsgSolver: {
         u8 lvl = header.reserved[0];
         u32 total_bytes_read = 0, remaining_bytes = msg_len;
         do {
             u32 read_sz = remaining_bytes <= sizeof(buffer) ? remaining_bytes : sizeof(buffer);
             bytes_read = read(fd, buffer, read_sz);
             if (bytes_read <= 0) { goto neg_bytes_read; }
             buffer[bytes_read] = '\0';
             loggerfmt(lvl, (const char*)buffer);
             remaining_bytes -= read_sz;
             total_bytes_read += read_sz;
         }  while (remaining_bytes && bytes_read > 0);

         if (total_bytes_read != msg_len) {
            loggerfmt(ErrorGui, "[IPC] ERROR: expected %u bytes, read %u\n",
                     msg_len, total_bytes_read);
         }

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
   loggerfmt(ErrorGui, "[IPC] ERROR got rc = %ld from reading fd %d: '%s'",
            bytes_read, fd, strerror(errno));
   return true; // No data or client disconnected
}
