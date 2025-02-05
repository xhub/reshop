#ifndef RHP_IPC_H
#define RHP_IPC_H

#include "rhp_ipc_protocol.h"
#include "tlsdef.h"
#include "rhp_socket_server.h"

// WARNING: this needs to be above the valid_fd part
#define RHP_SOCKET_FATAL(...)  error(__VA_ARGS__)
#define RHP_SOCKET_LOGGER(...) error(__VA_ARGS__)

#include "rhp_socket_error.h"

extern tlsvar rhpfd_t gui_fd;
extern tlsvar rhpfd_t data_fd;
extern tlsvar rhpfd_t ctrl_fd;

#ifdef _WIN32

#ifndef INVALID_SOCKET
#error "INVALID_SOCKET must be defined for valid()"
#endif

#define valid_fd(x) ((x) != INVALID_SOCKET)

#else

#define valid_fd(x) ((x) >= 0)

#endif

int unix_domain_client_init(const char *sockpath);
const char* ipc_unix_domain_init(void);
int ipc_send_mdl(Model *mdl, rhpfd_t fd) NONNULL;
int ipc_wait(rhpfd_t fd) NONNULL;

void unix_domain_send1(MessageType mtype);
void unix_domain_send_str(MessageType mtype, const char* message);

// Dispatchers
#define UD_DISPATCH_2(mtype, x) _Generic((x), \
    const char*: unix_domain_send_str \
)(x)

#define UD_DISPATCH_1(x) unix_domain_send1(x)

// Main macro
#define unix_domain_send(...) CONCAT(UD_DISPATCH_, PP_NARG(__VA_ARGS__))(__VA_ARGS__)


#endif 
