#include "reshop_priv.h"
#include "reshop.h"

#if defined(__GNUC__) || defined(__clang__) || defined(__INTEL_LLVM_COMPILER)
#   define UNUSED __attribute__((unused))
#else
#   define UNUSED
#endif

/** @brief Print out log or status message to the screen.
 *         It strips off one new line if it exists.
 *
 *  @param  env          ReSHOP GAMS record as an opaque object
 *  @param  reshop_mode  mode indicating log, status, or both.
 *  @param  str          the actual string
 */
void printgams(void* env, UNUSED unsigned reshop_mode, const char *str)
{
   rhpRec_t *jh = (rhpRec_t *)env;
   /* TODO(Xhub) support status and all ...  */
   int mode = LOGMASK;

   switch (mode & ALLMASK) {
   case LOGMASK:
      gevLogPChar(jh->eh, str);
      break;
   case STATUSMASK:
      gevStatPChar(jh->eh, str);
      break;
   case ALLMASK:
      gevLogStatPChar(jh->eh, str);
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
   rhpRec_t *jh = (rhpRec_t *)env;
   gevLogStatFlush(jh->eh);

}
