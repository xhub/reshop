#include "reshop_config.h"

#include <errno.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>

#ifdef _WIN32
#include <winsock2.h>
#include <ws2tcpip.h>

#define poll WSAPoll
#define close closesocket

#pragma comment(lib, "ws2_32.lib")  // Link against Winsock library

#else
#include <sys/socket.h>
#include <poll.h>
#endif

#ifdef HAS_UNISTD
#include <unistd.h>
#endif

#define MAX_CLIENTS 10

#include "rhp_socket_server.h"
#include "rhpgui_error.h"
#include "rhpgui_ipc.h"
#include "rhp_gui_data.h"
#include "rhpgui_ipc_poll.h"

typedef struct {
   u8 max;
   u8 nclients;
   struct pollfd fds[] __counted_by(nclients);
} PollData;

void ipc_poll_gui_init(GuiData *guidat)
{
   // Polling setup
   PollData *polldat = malloc(sizeof(PollData) + sizeof(struct pollfd)*(1+MAX_CLIENTS));
   polldat->fds[0].fd = guidat->ipc.server_fd;
   polldat->fds[0].events = POLLIN; // Monitor for incoming connections
   //
   guidat->ipc.data = polldat;

   polldat->max = MAX_CLIENTS;
   polldat->nclients = 0;

   // Listen for connections
   if (listen(guidat->ipc.server_fd, MAX_CLIENTS) == -1) {
      sockerr_fatal("Call to 'listen' failed");
   }
}

int ipc_pool_process(GuiData *guidat)
{
   PollData *polldat = guidat->ipc.data;
   struct pollfd *fds = polldat->fds;
   int server_fd = guidat->ipc.server_fd;
   u8 nclients = polldat->nclients;


   // NOTE change timeout
   int poll_count = poll(fds, nclients + 1, 0);
   if (poll_count == -1) {
      sockerr_log("[IPC] ERROR in 'poll'");
      return 1;
   }

   // Handle new connections
   if (polldat->fds[0].revents & POLLIN) {
      rhpfd_t client_fd;
      do {
         client_fd = accept(server_fd, NULL, NULL);
         if (client_fd < 0) {
            if (errno != EWOULDBLOCK) {
               sockerr_log("[IPC] ERROR in 'accept'");
               return -1;
            }
            break;
         }

         fd_setup(client_fd);
         assert(nclients < MAX_CLIENTS);
         fds[++nclients].fd = client_fd;
         fds[nclients].events = POLLIN;
         loggerfmt(DebugGui, "New client connected (fd=" FDP")\n", client_fd);

      } while (client_fd >= 0);
   }

   // Handle client messages
   int nclients_end = 1;
   for (u8 i = 1; i <= nclients; i++) {
      // TODO handle other cases
      if (fds[i].revents & POLLIN) {
         rhpfd_t client_fd = fds[i].fd;
         bool closeconn = gui_ipc_handle_client(client_fd, guidat);
         if (closeconn) {
            loggerfmt(DebugGui, "[IPC] client %u disconnected (fd=" FDP ")\n",
                      i, client_fd);
            close(fds[i].fd);
            fds[i].fd = -1;
            continue;
         } 
      }
      fds[nclients_end++] = fds[i];
      assert(nclients_end <= MAX_CLIENTS);
   }

   polldat->nclients = nclients_end-1;

   return 0;

}
