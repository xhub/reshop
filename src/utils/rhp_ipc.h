#ifndef RHP_IPC_H
#define RHP_IPC_H

#include "rhp_ipc_protocol.h"
#include "tlsdef.h"

extern tlsvar int gui_fd;
extern tlsvar int data_fd;
extern tlsvar int ctrl_fd;

#define valid_fd(x) (x) >= 0

int unix_domain_client_init(const char *sockpath);
const char* ipc_unix_domain_init(void);
int ipc_send_mdl(Model *mdl, int fd) NONNULL;
int ipc_wait(int fd) NONNULL;

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
