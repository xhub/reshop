#include "reshop_priv.h"
#include "reshop.h"
#include <string.h>

/** @brief Print out log or status message to the screen.
 *
 *  @param  env          ReSHOP GAMS record as an opaque object
 *  @param  reshop_mode  mode indicating log, status, or both.
 *  @param  str          the actual string
 */
void printgams(void* env, unsigned reshop_mode, const char *str)
{
   gevHandle_t gev = (gevHandle_t)env;
   /* TODO(Xhub) support status and all ...  */

   int mode;
   if (reshop_mode == 0x3 || reshop_mode == 0x7) {
      mode = ALLMASK;
   } else {
      mode = LOGMASK;
   }

   switch (mode) {
   case STATUSMASK:
      gevStatPChar(gev, str);
      break;
   case ALLMASK: {
      char *newline = strchr(str, '\n');
      bool simple_copy = newline && newline[1] == '\0'; 
      const char *start = simple_copy ? "=C" : "=1\n";

      gevStatPChar(gev, start);

      gevLogStatPChar(gev, str);

      if (!simple_copy) {
         size_t sz = strlen(str);

         /* If the string does not end with '\n', add one before "=2" */
         const char *end = (sz >= 1 && str[sz-1] != '\n') ? "\n=2\n" : "=2\n";
         gevStatPChar(gev, end);
      }
      break;
   }
   default:
   case LOGMASK:
      gevLogPChar(gev, str);
      break;
   }

}

/**
 * @brief Flush the output streams
 *
 * @param env  ReSHOP GAMS record as an opaque object
 */
void flushgams(void* env)
{
   gevHandle_t gev = (gevHandle_t)env;
   gevLogStatFlush(gev);
}
