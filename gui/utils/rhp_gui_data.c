#include "rhp_gui_data.h"
#include "rhp_socket_server.h"

static void mpgui_free(MathPrgmGui *mp)
{
   if (!mp) { return; }
   free(mp->name);
}

static void nashgui_free(NashGui *nash)
{
   if (!nash) { return; }
   free(nash->name);
}

static void Varcs_mb_free(VarcBasicDatGuiDArray *arcdat)
{
}

#define noop(X) /* X */

static void Varcs_free(VarcGui *arc)
{
   if (!arc) { return; }
   ArcVFType type = arc->type;
   switch (type) {
   case ArcVFBasic:
      return;
   case ArcVFMultipleBasic:
      darr_free(&arc->basicdat_arr, noop);
      return;
   default:
      fprintf(stderr, "Fatal Error: unhandled Varc type %u", type);
      assert(0 && "Fatal Error: unhandled Varc type");
      exit(EXIT_FAILURE);
   }
}

static void empdaggui_free(EmpDagGui* empdag)
{
   if (!empdag) { return; }
   arr_free(empdag->mps, mpgui_free);
   arr_free(empdag->nashs, nashgui_free);
   arr_free(empdag->Varcs, Varcs_free);
   free(empdag->Carcs);
   free(empdag->Narcs);
}

void modelgui_free(ModelGui *mgui)
{
   if (!mgui) { return; }
   free(mgui->name);
   arr_free(mgui->empdags, empdaggui_free)
}

void guidata_fini(GuiData* gdat)
{
   server_fini_socket(gdat->ipc.server_fd, gdat->ipc.name);
   free(gdat->ipc.data);
   arr_free(gdat->models, modelgui_free);
}


