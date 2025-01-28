#ifndef RHPGUI_IPC_POLL_H
#define RHPGUI_IPC_POLL_H

#include "rhp_gui_data.h"

#ifdef __cplusplus
extern "C" {
#endif

void ipc_poll_gui_init(GuiData *guidat);
int ipc_pool_process(GuiData *guidat);

#ifdef __cplusplus
}
#endif


#endif
