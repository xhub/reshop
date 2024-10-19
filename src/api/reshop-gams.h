#ifndef RESHOP_GAMS_H
#define RESHOP_GAMS_H

#include "cfgmcc.h"
#include "dctmcc.h"
#include "gevmcc.h"
#include "gmomcc.h"
#include "optcc.h"
#include "gdxcc.h"

struct rhp_mdl;

struct rhp_gams_handles {
   optHandle_t oh;
   gmoHandle_t gh;
   gevHandle_t eh;
   dctHandle_t dh;
   cfgHandle_t ch;
};

#ifndef RHP_PUBLIB

#if defined _WIN32 || defined __CYGWIN__
  #ifdef reshop_EXPORTS
    #ifdef __GNUC__
      #define RHP_PUBLIB __attribute__ ((dllexport))
    #else
      #define RHP_PUBLIB __declspec(dllexport) // Note: actually gcc seems to also supports this syntax.
    #endif
  #else
    #ifdef __GNUC__
      #define RHP_PUBLIB __attribute__ ((dllimport))
    #else
      #define RHP_PUBLIB __declspec(dllimport) // Note: actually gcc seems to also supports this syntax.
    #endif
  #endif

#else

  #if __GNUC__ >= 4
    #define RHP_PUBLIB __attribute__ ((visibility ("default")))
  #else
    #define RHP_PUBLIB
  #endif
#endif

#endif /*  RHP_PUBLIB  */

RHP_PUBLIB
int rhp_gms_readempinfo(struct rhp_mdl *mdl, const char *fname);
RHP_PUBLIB
int rhp_gms_fillgmshandles(struct rhp_mdl *mdl, struct rhp_gams_handles *gmsh);
RHP_PUBLIB
int rhp_gms_loadlibs(const char* sysdir);
RHP_PUBLIB
int rhp_gms_fillmdl(struct rhp_mdl *mdl);
RHP_PUBLIB
int rhp_gms_set_gamsprintops(struct rhp_mdl *mdl);
RHP_PUBLIB
void* rhp_gms_getgmo(struct rhp_mdl *mdl);
RHP_PUBLIB
void* rhp_gms_getgev(struct rhp_mdl *mdl);
RHP_PUBLIB
const char* rhp_gms_getsysdir(struct rhp_mdl *mdl);

RHP_PUBLIB
int rhp_embcode(void *gmd, unsigned char scrdirlen, const char *scrdir,
                unsigned char codelen, const char *code, unsigned char argslen,
                const char *args);

RHP_PUBLIB
int rhp_gms_set_solvelink(struct rhp_mdl *mdl, unsigned solvelink);

/* -------------------------------------------------------------------------
 * Misc functions
 * ------------------------------------------------------------------------- */

RHP_PUBLIB
int rhp_rc2gmosolvestat(int rc);
RHP_PUBLIB
void rhp_printrcmsg(int rc, void *gev);


#undef RHP_PUBLIB

#endif /* RESHOP_GAMS_H */
