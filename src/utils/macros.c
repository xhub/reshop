#include <stdio.h>
#include <stdlib.h>

#include "macros.h"
#include "status.h"

void backtrace_(const char *expr, int status)
{
   if (getenv("RHP_NO_BACKTRACE")) return;

   printf("Call to %s failed!\tError is %s (%d)\n", expr, status_descr(status), status);

   if (!getenv("RHP_NO_STOP")) { GDB_STOP();} //NOLINT(bugprone-unused-return-value,cert-err33-c)

}
