#if ! defined(_RHP_H_)
#define _RHP_H_

#include "gmomcc.h"
#include "optcc.h"
#include "dctmcc.h"

typedef struct rhpRec rhpRec_t;

#if defined(__cplusplus)
extern "C" {
#endif

  int    rhpCallSolver (rhpRec_t *jh);
  int    rhpReadyAPI (rhpRec_t *jh, gmoHandle_t gh, optHandle_t oh);
  void   rhpFree (rhpRec_t **jh);
  void   rhpCreate (rhpRec_t **jh, char *msgBuf, int msgBufLen);

#if defined(__cplusplus)
}
#endif

#endif /* ! defined(_RHP_H_) */
