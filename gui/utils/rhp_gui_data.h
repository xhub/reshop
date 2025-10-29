#ifndef RESHOP_GUI_DATA_H
#define RESHOP_GUI_DATA_H

#include <stdint.h>

#ifdef __cplusplus
extern "C" {
#endif

#include "empdag_data.h"
#include "rhp_ipc_protocol.h"

void loggerfmt(uint8_t level, const char *fmt, ...);
NO_RETURN void error_init(const char *fmt, ...);

// FIXME: get proper support for custom allocator
#define mymalloc malloc

typedef enum {
   DebugGui,
   InfoGui,
   WarnGui,
   ErrorGui,
} LogLevelGui;


typedef struct {
   bool connected;
   struct {
      rhpfd_t server_fd;
      void *data;
      char *name;
   } ipc;
   ModelGuiArray *models;
} GuiData;

void modelgui_free(ModelGui *mgui);

static inline GuiData* guidata_new(void)
{
   GuiData *gdat = (GuiData *)mymalloc(sizeof(GuiData));
   gdat->connected = false;
   gdat->ipc.server_fd =
#ifdef _WIN32
   INVALID_SOCKET;
#else
   -1;
#endif

   gdat->ipc.data = NULL;
   gdat->ipc.name = NULL;
   gdat->models = NULL;

   return gdat;
}

void guidata_fini(GuiData* gdat);

#ifdef __cplusplus
}
#endif

#endif
