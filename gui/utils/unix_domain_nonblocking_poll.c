#include "reshop_config.h"

#include <errno.h>
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

#define close closesocket

#else

#include <fcntl.h>
#include <sys/socket.h>
#include <sys/un.h>

#endif

#if defined(HAS_UNISTD) && !defined(_WIN32)
#include <unistd.h>
#endif

#include "rhp_ipc_protocol.h"
#include "rhp_socket_server.h"
#include "rhp_gui_data.h"
#include "rhpgui_error.h"

#define DEBUG_MSG(...) logfn(__VA_ARGS__)


int fd_set_blocking(rhpfd_t fd)
{
#ifdef _WIN32
   u_long mode = 0;
   if (ioctlsocket (fd, FIONBIO, &mode) == SOCKET_ERROR) {
      sockerr_log2("[IPC] ERROR while setting fd %d as blocking", fd);
      return 1;
   }

#else

   int flags = fcntl(fd, F_GETFL, 0);
   if (flags == -1) {
      sockerr_log2("[IPC] ERROR while on getting flags (F_GETFL) via fnctl on fd %d", fd);
      return -1;
   }
   if ((flags & O_NONBLOCK) && (fcntl(fd, F_SETFL, flags & ~O_NONBLOCK) == -1)) {
      sockerr_log2("[IPC] ERROR while on setting flags (F_GETFL) via fnctl on fd %d: '%s'", fd);
      return -1;
   }

#endif

   return 0;


}

int fd_set_nonblocking(rhpfd_t fd)
{
#ifdef _WIN32
   u_long mode = 1;
   if (ioctlsocket (fd, FIONBIO, &mode) == SOCKET_ERROR) {
      sockerr_log2("[IPC] ERROR while setting fd %d as blocking: '%s'", fd);
      return 1;
   }

#else

   int flags = fcntl(fd, F_GETFL, 0);
   if (flags == -1) {
      sockerr_log2("[IPC] ERROR while on getting flags (F_GETFL) via fnctl on fd %d: '%s'", fd);
      return -1;
   }
   if (fcntl(fd, F_SETFL, flags | O_NONBLOCK) == -1) {
      sockerr_log2("[IPC] ERROR while on setting flags (F_GETFL) via fnctl on fd %d: '%s'", fd);
      return -1;
   }

#endif

   return 0;

}

int fd_setup(rhpfd_t fd)
{
   return fd_set_nonblocking(fd);
}

rhpfd_t server_init_socket(const char *sockpath, int pid)
{
   rhpfd_t fd;
   struct sockaddr_un addr;

#ifdef _WIN32
       // Initialize Winsock
    WSADATA wsaData;
    if (WSAStartup(MAKEWORD(2, 2), &wsaData) != 0) {
        sockerr_fatal("[IPC] ERROR: WSAStartup failed\n");
    }
#endif

   // Create the Unix domain socket
   if ((fd = socket(AF_UNIX, SOCK_STREAM, 0)) == -1) {
        sockerr_fatal("[IPC] ERROR: AF_UNIX socket creation failed")
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

   if (bind(fd, (struct sockaddr*)&addr, sizeof(addr)) == -1) {
      sockerr_fatal2("[IPC] ERROR: could not bind AF_UNIX socket '%s'", sockpath)
   }

   // Set server socket to non-blocking mode
   int rc = fd_setup(fd);
   if (rc) { return 0; }

   loggerfmt(InfoGui, "ReSHOP gui listening on %s\n", sockpath);

#ifndef _WIN32
   if (pid > 1) {
      (void)kill(pid, SIGUSR1);
   }
#endif

   return fd;
}

void server_fini_socket(rhpfd_t fd, UNUSED const char *socket_path)
{

   if (fd > 0) {

      close(fd);

#ifndef __linux__
      if (socket_path && strlen(socket_path) > 0) {
         unlink(socket_path);
      }
#endif

   }

#ifdef _WIN32
   WSACleanup();
#endif
}
