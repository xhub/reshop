
#ifndef GAMSAPI_UTILS_H
#define GAMSAPI_UTILS_H

#include "dctmcc.h"
#include "gdxcc.h"
#include "printout.h"

#define gdxerror(func, gdxh) { \
   char msg42[GMS_SSSIZE];\
   int rc42 = gdxGetLastError(gdxh);\
   gdxErrorStr(gdxh, rc42, msg42);\
   error("%s :: Call to %s failed with rc %d and msg '%s'\n", __func__, #func, rc42, msg42);\
}

#define dcterror(func, gdxh) { \
   error("%s :: Call to %s failed\n", __func__, #func);\
}

#define dct_call_rc(func, dcth, ...) \
  { if (func(dcth, __VA_ARGS__)) { dcterror(func, dcth); } }

#define gdx_call_rc(func, gdxh, ...) \
  { if (!func(gdxh, __VA_ARGS__)) { gdxerror(func, gdxh); } }

#define gdx_call0_rc(func, gdxh) \
  { if (func(gdxh)) { gdxerror(func, gdxh); } }

#define gdx_call0_rc0(func, gdxh) \
  { if (!func(gdxh)) { gdxerror(func, gdxh); } }

#define GMD_CHK(func, gmdh, ...) \
  if (!func(gmdh, __VA_ARGS__)) { \
      int offset42; \
      error("[gmd] %nERROR: call to %s failed!\n", &offset42, #func); \
      char msg42[GMS_SSSIZE]; \
      gmdGetLastError(gmdh, msg42); \
      error("%*s%s\n", offset42, "", msg42); \
   }

#define GMD_FIND_CHK(func, gmdh, ...) \
   if (!func(gmdh, __VA_ARGS__)) { \
      GMD_CHK(gmdSymbolInfo, gmd, symptr, GMD_NAME, NULL, NULL, buf); \
      error("[empinterp] ERROR: in the GMD, could not find record for symbol %s", buf); \
      if (dim > 1 || (dim == 1 && uels[0] > 0)) { \
         errormsg("("); \
         for (unsigned i = 0; i < dim; ++i) { \
            char quote42 = '\''; \
            int uel42 = uels[i]; \
            if (uel42 > 0) { GMD_CHK(gmdGetUelByIndex, gmdh, uel42, buf); \
            } else { strcpy(buf, "'*'"); } \
            if (i > 0) { errormsg(","); } \
               error("%c%s%c", quote42, buf, quote42); \
            } \
            errormsg(")"); \
         } \
         errormsg("\n"); \
      \
      int offset42; \
      error("[gmd] GMD errors: %n\n", &offset42); \
      char msg42[GMS_SSSIZE]; \
      gmdGetLastError(gmdh, msg42); \
      error("%*s%s\n", offset42, "", msg42); \
      return Error_SymbolNotInTheGamsRim; \
   }

static inline int chk_dbl2int(double val, const char *fn)
{
   if (val >= GMS_SV_NAINT) {
      error("ERROR in %s: double value %e larger than max %d", fn, val, GMS_SV_NAINT);
      return Error_GamsCallFailed;
   }

   return OK;
}

#endif
