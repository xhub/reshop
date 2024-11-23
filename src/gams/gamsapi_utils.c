#include <stdarg.h>

#include "gamsapi_utils.h"

int gmderr(gmdHandle_t gmdh)
{
   char msg[GMS_SSSIZE];
   gmdGetLastError(gmdh, msg);

   error("[GMD] the last error message from the library is: %s\n", msg);

   return Error_GamsCallFailed;
}

