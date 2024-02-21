
#ifndef GAMSAPI_UTILS_H
#define GAMSAPI_UTILS_H

#include "dctmcc.h"
#include "gdxcc.h"

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

#endif
