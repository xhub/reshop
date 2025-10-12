#include "reshop_config.h"

#include <errno.h>
#include <fcntl.h>
#include <string.h>

#ifdef _WIN32
#define WIN32_LEAN_AND_MEAN
#include <winsock2.h>
#include <io.h>
#include <afunix.h>

// See https://learn.microsoft.com/en-us/windows/win32/winsock/error-codes-errno-h-errno-and-wsagetlasterror-2
#undef errno
#define errno WSAGetLastError()

#ifdef _MSC_VER
#pragma comment(lib, "Ws2_32.lib")
#endif
#else

#include <sys/poll.h>
#include <sys/socket.h>
#include <sys/un.h>
#include <unistd.h>

#endif

#include "macros.h"
#include "printout.h"
#include "rhp_ipc.h"
#include "rhp_ipc_protocol.h"
#include "rhp_socket_server.h"
#include "status.h"


#ifdef _WIN32

tlsvar rhpfd_t gui_fd  = INVALID_SOCKET;
tlsvar rhpfd_t data_fd = INVALID_SOCKET;
tlsvar rhpfd_t ctrl_fd = INVALID_SOCKET;

#else

tlsvar int gui_fd  = -1;
tlsvar int data_fd = -1;
tlsvar int ctrl_fd = -1;

#endif

static void write_err(void)
{
   sockerr_log("[IPC] ERROR while calling 'write'");
}
#define CHK_WRITE(EXPR) if ((EXPR) == -1) { write_err(); }

// FIXME
#if 0
static void read_err(void)
{
   error_errno("[IPC] ERROR while calling 'read': '%s'\n");
}

#define CHK_READ(EXPR) if ((EXPR) == -1) { read_err(); }
#endif

int fd_setup(rhpfd_t fd)
{
#ifdef _WIN32

   u_long mode = 1;
   if (ioctlsocket (fd, FIONBIO, &mode) == SOCKET_ERROR) {
      sockerr_log2("[IPC] ERROR while on setting flags on fd %zu", fd);
   }

#else
   int flags = fcntl(fd, F_GETFL, 0);
   if (flags == -1) {
      sockerr_log2("[IPC] ERROR while on getting flags (F_GETFL) via fnctl on fd %d", fd);
      return Error_SystemError;
}
   if (fcntl(fd, F_SETFL, flags | O_NONBLOCK) == -1) {
      sockerr_log2("[IPC] ERROR while on setting flags (F_GETFL) via fnctl on fd %d", fd);
      return Error_SystemError;
   }

   int size = 2 * 1024 * 1024;
   if (setsockopt(fd, SOL_SOCKET, SO_SNDBUF, &size, sizeof(size)) == -1) {
      sockerr_log("[IPC] ERROR: call to 'setsockopt' failed");
      return -1;
   }

   if (setsockopt(fd, SOL_SOCKET, SO_RCVBUF, &size, sizeof(size)) == -1) {
      sockerr_log("[IPC] ERROR: call to 'setsockopt' failed with msg");
      return -1;
   }
#endif

   return OK;
}

static int unix_domain_getfd(const char *sockpath)
{
   struct sockaddr_un addr;
   int fd;

   // Create the Unix domain socket
   if ((fd = socket(AF_UNIX, SOCK_STREAM, 0)) == -1) {
      sockerr_log("[IPC] ERROR: call to 'socket' failed with msg");
      return -1;
   }

   // Connect to the server
   memset(&addr, 0, sizeof(addr));
   addr.sun_family = AF_UNIX;

#ifdef __linux__  /* We use abstract namespaces */
   addr.sun_path[0] = '\0';
   memcpy(&addr.sun_path[1], sockpath, strlen(sockpath)+1);
#else
   strncpy(addr.sun_path, sockpath, sizeof(addr.sun_path) - 1);
#endif

   if (connect(fd, (struct sockaddr*)&addr, sizeof(addr)) == -1) {
      sockerr_log("[IPC] ERROR: call to 'connect' failed with msg");
      return -1;
   }

#ifndef _WIN32
   int size = 2 * 1024 * 1024;
   if (setsockopt(fd, SOL_SOCKET, SO_SNDBUF, &size, sizeof(size)) == -1) {
      sockerr_log("[IPC] ERROR: call to 'setsockopt' failed with msg");
      return -1;
   }

   if (setsockopt(fd, SOL_SOCKET, SO_RCVBUF, &size, sizeof(size)) == -1) {
      sockerr_log("[IPC] ERROR: call to 'setsockopt' failed with msg");
      return -1;
   }
#endif

   return fd;
}

int unix_domain_client_init(const char *sockpath)
{
   gui_fd = unix_domain_getfd(sockpath);
   if (!valid_fd(gui_fd)) { 
      return Error_SystemError;
   }

   // Set socket to non-blocking
   if (fd_setup(gui_fd)) {
      error("[IPC] ERROR: could not set fd=" FDP " as nonblocking\n", gui_fd);
      return Error_SystemError;
   }

   data_fd = unix_domain_getfd(sockpath);
   if (!valid_fd(data_fd)) { 
      return Error_SystemError;
   }

   pr_debug("[IPC] Succesfully connected to %s: gui_fd=" FDP "; data_fd=" FDP "\n",
            sockpath, gui_fd, data_fd);

   return OK;
}

void unix_domain_send1(MessageType mtype)
{
   assert(valid_fd(gui_fd));

   MessageHeader header = { 0, mtype, {0}, {0} };
   CHK_WRITE(write(gui_fd, &header, sizeof(header)))
}

void unix_domain_send_str(MessageType mtype, const char* message)
{
   assert(valid_fd(gui_fd));
   // Send request
   size_t msglen = strlen(message)+1;
   MessageHeader header = { msglen, mtype, {0}, {0} };
   CHK_WRITE(write(gui_fd, &header, sizeof(header)))
   CHK_WRITE(write(gui_fd, message, msglen))

#if 0
   // Poll for response
   struct pollfd fds;
   fds.fd = gui_fd;
   fds.events = POLLIN;

   while (poll(&fds, 1, 5000) > 0) { // 5-second timeout
      if (fds.revents & POLLIN) {
         MessageHeader res_header;
         CHK_READ(read(gui_fd, &res_header, sizeof(res_header)))

         char* response = malloc(res_header.length);
         CHK_READ(read(gui_fd, response, res_header.length))

         printf("Response: %.*s\n", res_header.length, response);
         free(response);
         break;
      }
   }
   #endif
}
