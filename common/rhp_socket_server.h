#ifndef RHP_SOCKET_SERVER_H
#define RHP_SOCKET_SERVER_H

#ifdef __cplusplus
extern "C" {
#endif

int server_init_socket(const char *socket_path, int pid);
void server_fini_socket(int socket_fd, const char *socket_path);
int fd_setup(int fd);

#ifdef __cplusplus
}
#endif

#endif
