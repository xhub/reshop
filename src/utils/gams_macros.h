#ifndef GAMS_MACROS_H
#define GAMS_MACROS_H

#include "printout.h"
#include "status.h"

#define GMSCHK(EXPR) { int status42 = EXPR; if (RHP_UNLIKELY(status42)) { \
  error("%s ERROR: call " #EXPR " failed with error = %d\n", __func__, \
           status42); \
  return Error_GamsCallFailed; } }

#define GMSCHK_EXIT(EXPR) { int status42 = EXPR; if (RHP_UNLIKELY(status42)) { \
  error("%s ERROR: call " #EXPR " failed with error = %d\n", __func__, \
           status42); status = Error_GamsCallFailed; goto _exit; } }

#define GMSCHK_BUF_EXIT(FN, obj, buf) { int status42 = FN(obj, buf); \
  if (RHP_UNLIKELY(status42)) { BACKTRACE(#FN, status42); \
    error("%s ERROR: call to " #FN " failed with error = %d\n" \
        "Gams msg is: %s\n", __func__, status42, buf); \
  status = Error_GamsCallFailed; goto _exit;} }


#define GMSCHK_ERRMSG(EXPR, gmo, buf) { int status42 = EXPR; if (RHP_UNLIKELY(status42)) { \
  BACKTRACE(#EXPR, Error_GamsCallFailed); gmoErrorMessage(gmo, buf); \
  error("%s ERROR: call to " #EXPR " failed with error %d\n" \
      "Gams msg is: %s\n", __func__, status42, buf); \
  return Error_GamsCallFailed; } }

#define GAMS_CHECK1(FN, obj, buf) { int status42 = FN(obj, buf, sizeof(buf)); \
  if (RHP_UNLIKELY(!status42)) { BACKTRACE(#FN, !status42); \
    error("%s ERROR: call to " #FN " failed with error = %d\n" \
        "Gams msg is: %s\n", __func__, status42, buf); \
  return Error_GamsCallFailed; } }


#endif
