#ifndef RESHOP_PRIV_H
#define RESHOP_PRIV_H

#include "reshop.h"
#include "reshop_gams_common.h"

#if defined(__cplusplus)
extern "C" {
#endif

void printgams(void *env, unsigned mode, const char *str);
void flushgams(void* env);

#if defined(__cplusplus)
}
#endif

#define xstr(s) str(s)
#define str(s) #s
#define CAT(a,b) a##b
#define _APIVER(_GMS_PREFIX) CAT(_GMS_PREFIX,APIVERSION) 


/* This is used to check that the library versions are compatible in both the
 * GAMS driver and reshop */
#define CHK_CORRECT_LIBVER(_gms_prefix, _GMS_PREFIX) \
   if (!_gms_prefix ## CorrectLibraryVersion(msg, sizeof(msg))) { \
      gevLogStat(jh->eh, "[WARNING] " # _GMS_PREFIX  " API version differ: ReSHOP compiled with" \
      " " xstr(_APIVER(_GMS_PREFIX)) ". Error message follows:\n"); \
      gevLogStat(jh->eh, msg); \
      gevLogStat(jh->eh, "This may lead to runtime failures. Continue at your own risk, or try to update ReSHOP\n"); \
   }

#endif /* RESHOP_PRIV_H */
