#ifndef RHP_SOCKET_SERVER_H
#define RHP_SOCKET_SERVER_H

#ifdef __cplusplus
extern "C" {
#endif

#include "rhp_defines.h"

rhpfd_t server_init_socket(const char *socket_path, int pid);
void server_fini_socket(rhpfd_t socket_fd, const char *socket_path);
int fd_setup(rhpfd_t fd);

#ifdef __cplusplus
}
#endif

#endif
