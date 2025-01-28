#include "reshop_config.h"

#include <signal.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>


#ifdef _WIN32
#define WIN32_LEAN_AND_MEAN
#define VC_EXTRALEAN
#include <windows.h>
#include <objbase.h>

// For AF_UNIX
#include <winsock2.h>
#include <afunix.h>
#include <io.h>
#else
#include <fcntl.h>
#include <sys/socket.h>
#include <sys/un.h>
#endif

#ifdef HAS_UNISTD
#include <unistd.h>
#endif

#include "rhp_ipc_protocol.h"
#include "rhp_socket_server.h"
#include "rhp_gui_data.h"

#include <errno.h>

#define DEBUG_MSG(...) logfn(__VA_ARGS__)


int fd_setup(int fd)
{
#ifdef _WIN32
   u_long mode = 1;
   if (ioctlsocket (fd, FIONBIO, &mode) == SOCKET_ERROR) {
      loggerfmt(ErrorGui, "[IPC] ERROR while on setting flags on fd %d: '%s'", fd, strerror(errno));
   }

#else

   int flags = fcntl(fd, F_GETFL, 0);
   if (flags == -1) {
      loggerfmt(ErrorGui, "[IPC] ERROR while on getting flags (F_GETFL) via fnctl on fd %d: '%s'", fd, strerror(errno));
      return -1;
   }
   if (fcntl(fd, F_SETFL, flags | O_NONBLOCK) == -1) {
      loggerfmt(ErrorGui, "[IPC] ERROR while on setting flags (F_GETFL) via fnctl on fd %d: '%s'", fd, strerror(errno));
      return -1;
   }
#endif
   return 0;
}

int server_init_socket(const char *sockpath, int pid)
{
   int server_fd;
   struct sockaddr_un addr;

   // Create the Unix domain socket
   if ((server_fd = socket(AF_UNIX, SOCK_STREAM, 0)) == -1) {
      perror("socket");
      exit(EXIT_FAILURE);
   }

   // Bind the socket to a path
   memset(&addr, 0, sizeof(addr));
   addr.sun_family = AF_UNIX;

#ifdef __linux__
   addr.sun_path[0] = '\0';
   memcpy(&addr.sun_path[1], sockpath, strlen(sockpath)+1);
#else
   strncpy(addr.sun_path, sockpath, sizeof(addr.sun_path) - 1);
   unlink(sockpath); // Remove existing socket
#endif

   if (bind(server_fd, (struct sockaddr*)&addr, sizeof(addr)) == -1) {
      perror("bind");
      exit(EXIT_FAILURE);
   }

   // Set server socket to non-blocking mode
   int rc = fd_setup(server_fd);
   if (rc) { return rc; }

   loggerfmt(InfoGui, "ReSHOP gui listening on %s\n", sockpath);

#ifndef _WIN32
   if (pid > 1) {
      (void)kill(pid, SIGUSR1);
   }
#endif

   return server_fd;
}

void server_fini_socket(int socket_fd, UNUSED const char *socket_path)
{

   if (socket_fd < 0) {
      return;
   }

   close(socket_fd);

#ifndef __linux__
   if (socket_path && strlen(socket_path) > 0) {
      unlink(socket_path);
   }
#endif
}

