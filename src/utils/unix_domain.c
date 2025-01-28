
#include <errno.h>
#include <fcntl.h>
#include <string.h>
#include <sys/poll.h>
#include <sys/socket.h>
#include <sys/un.h>
#include <unistd.h>

#include "macros.h"
#include "printout.h"
#include "rhp_ipc.h"
#include "rhp_ipc_protocol.h"
#include "rhp_socket_server.h"
#include "status.h"

tlsvar int gui_fd = -1;
tlsvar int data_fd = -1;
tlsvar int ctrl_fd = -1;

static void write_err(void)
{
   error_errno("[IPC] ERROR while calling 'write': '%s'\n");
}

static void read_err(void)
{
   error_errno("[IPC] ERROR while calling 'read': '%s'\n");
}

#define CHK_WRITE(EXPR) if ((EXPR) == -1) { write_err(); }
#define CHK_READ(EXPR) if ((EXPR) == -1) { read_err(); }

int fd_setup(int fd)
{
   int flags = fcntl(fd, F_GETFL, 0);
   if (flags == -1) {
      error_errno("[IPC] ERROR while on getting flags (F_GETFL) via fnctl on fd %d: '%s'", fd);
      return Error_SystemError;
}
   if (fcntl(fd, F_SETFL, flags | O_NONBLOCK) == -1) {
      error_errno("[IPC] ERROR while on setting flags (F_GETFL) via fnctl on fd %d: '%s'", fd);
      return Error_SystemError;
   }

   int size = 2 * 1024 * 1024;
   if (setsockopt(fd, SOL_SOCKET, SO_SNDBUF, &size, sizeof(size)) == -1) {
      error_errno("[IPC] ERROR: call to 'setsockopt' failed with msg: '%s'\n");
      return -1;
   }

   if (setsockopt(fd, SOL_SOCKET, SO_RCVBUF, &size, sizeof(size)) == -1) {
      error_errno("[IPC] ERROR: call to 'setsockopt' failed with msg: '%s'\n");
      return -1;
   }

   return OK;
}

static int unix_domain_getfd(const char *sockpath)
{
   struct sockaddr_un addr;
   int fd;

   // Create the Unix domain socket
   if ((fd = socket(AF_UNIX, SOCK_STREAM, 0)) == -1) {
      error_errno("[IPC] ERROR: call to 'socket' failed with msg: '%s'\n");
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
      error_errno("[IPC] ERROR: call to 'connect' failed with msg: '%s'\n");
      return -1;
   }

   int size = 2 * 1024 * 1024;
   if (setsockopt(fd, SOL_SOCKET, SO_SNDBUF, &size, sizeof(size)) == -1) {
      error_errno("[IPC] ERROR: call to 'setsockopt' failed with msg: '%s'\n");
      return -1;
   }

   if (setsockopt(fd, SOL_SOCKET, SO_RCVBUF, &size, sizeof(size)) == -1) {
      error_errno("[IPC] ERROR: call to 'setsockopt' failed with msg: '%s'\n");
      return -1;
   }

   return fd;
}

int unix_domain_client_init(const char *sockpath)
{
   gui_fd = unix_domain_getfd(sockpath);
   if (gui_fd <= 0) { 
      return Error_SystemError;
   }

   // Set socket to non-blocking
   if (fd_setup(gui_fd)) {
      error("[IPC] ERROR: could not set fd=%d as nonblocking\n", gui_fd);
      return Error_SystemError;
   }

   data_fd = unix_domain_getfd(sockpath);
   if (data_fd <= 0) { 
      return Error_SystemError;
   }

   debug("[IPC] Succesfully connected to %s: gui_fd=%d; data_fd=%d\n", sockpath, gui_fd, data_fd);

   return OK;
}

void unix_domain_send1(MessageType mtype)
{
   assert(gui_fd >= 0);

   MessageHeader header = { 0, mtype, {0} };
   CHK_WRITE(write(gui_fd, &header, sizeof(header)))
}

void unix_domain_send_str(MessageType mtype, const char* message)
{
   assert(gui_fd >= 0);
   // Send request
   size_t msglen = strlen(message)+1;
   MessageHeader header = { msglen, mtype, {0} };
   CHK_WRITE(write(gui_fd, &header, sizeof(header)))
   CHK_WRITE(write(gui_fd, message, msglen))

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

}
