#ifndef GAMS_LIBVER_H
#define GAMS_LIBVER_H

/* This is used to check that the library versions are compatible in both the
 * GAMS driver and reshop */
#define CAT(a,b) a##b
#define _APIVER(_GMS_PREFIX) CAT(_GMS_PREFIX,APIVERSION) 
#define xstr(s) str(s)
#define str(s) #s

#define CHK_CORRECT_LIBVER(_gms_prefix, _GMS_PREFIX, _msg) /* */

/* TODO: expose xxxXAPIVersion for a better check
#define CHK_CORRECT_LIBVER(_gms_prefix, _GMS_PREFIX, _msg) \
   if (!_gms_prefix ## CorrectLibraryVersion(_msg, sizeof(_msg))) { \
      printstr(PO_ALLDEST, "[WARNING] " # _GMS_PREFIX  " API version differ: ReSHOP compiled with version " \
               xstr(_APIVER(_GMS_PREFIX))". Error message follows:\n"); \
      printout(PO_ALLDEST, "          %s\n", _msg); \
      printstr(PO_ALLDEST, "          This may lead to runtime failures. Continue at your own risk, or try to update ReSHOP\n"); \
   }
*/
#endif
