#ifndef RESHOP_GUI_DATA_H
#define RESHOP_GUI_DATA_H

#include <stdint.h>

#ifdef __cplusplus
extern "C" {
#endif

#include "empdag_data.h"
#include "rhp_ipc_protocol.h"

void loggerfmt(uint8_t level, const char *fmt, ...);
void error_init(const char *fmt, ...);


typedef enum {
   DebugGui,
   InfoGui,
   WarnGui,
   ErrorGui,
} LogLevelGui;


typedef struct {
   bool connected;
   struct {
      int server_fd;
      void *data;
      const char *name;
   } ipc;
   ModelGuiArray *models;
} GuiData;

void modelgui_free(ModelGui *mgui);

static inline GuiData* guidata_new(void)
{
   GuiData *gdat = (GuiData *)mymalloc(sizeof(GuiData));
   gdat->models = NULL;
   return gdat;
}

void guidata_fini(GuiData* gdat);

#ifdef __cplusplus
}
#endif

#endif
