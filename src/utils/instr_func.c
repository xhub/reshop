#include <errno.h>
#include <math.h>
#include <stdarg.h>

#include "instr.h"
#include "printout.h"

static double _notimpl(const char *fn, ...)
{
   error("%s is not yet implemented\n", fn);
   return NAN;
}

#define NOTIMPL_PREFIX _ni_

#define _NOTIMPL(X)  static double RHP_FXPTR_CALLCONV \
   NOTIMPL_PREFIX ## X(double x, ...) { \
   va_list ap; va_start(ap, x); double res = _notimpl(#X, ap); va_end(ap); return res;}

_NOTIMPL(mapval)
_NOTIMPL(dunfm)
_NOTIMPL(dnorm)
_NOTIMPL(jdate)
_NOTIMPL(jtime)
_NOTIMPL(jstart)
_NOTIMPL(jnow)
_NOTIMPL(error)
_NOTIMPL(gyear)
_NOTIMPL(gmonth)
_NOTIMPL(gday)
_NOTIMPL(gdow)
_NOTIMPL(gleap)
_NOTIMPL(ghour)
_NOTIMPL(gminute)
_NOTIMPL(gsecond)
_NOTIMPL(curseed)
_NOTIMPL(timest)
_NOTIMPL(timeco)
_NOTIMPL(timeex)
_NOTIMPL(timecl)
_NOTIMPL(errorl)
_NOTIMPL(heaps)
_NOTIMPL(fact)
_NOTIMPL(unfmi)
_NOTIMPL(pi)
_NOTIMPL(ncpf)
_NOTIMPL(ncpcm)
_NOTIMPL(entropy)
_NOTIMPL(sigmoid)
_NOTIMPL(boolnot)
_NOTIMPL(booland)
_NOTIMPL(boolor)
_NOTIMPL(boolxor)
_NOTIMPL(boolimp)
_NOTIMPL(booleqv)
_NOTIMPL(relopeq)
_NOTIMPL(relopgt)
_NOTIMPL(relopge)
_NOTIMPL(reloplt)
_NOTIMPL(relople)
_NOTIMPL(relopne)
_NOTIMPL(ifthen)
_NOTIMPL(edist)
_NOTIMPL(div)
_NOTIMPL(div0)
_NOTIMPL(sllog10)
_NOTIMPL(sqlog10)
_NOTIMPL(slexp)
_NOTIMPL(sqexp)
_NOTIMPL(slrec)
_NOTIMPL(sqrec)
_NOTIMPL(cvpower)
_NOTIMPL(vcpower)
_NOTIMPL(centropy)
_NOTIMPL(gmillisec)
_NOTIMPL(maxerror)
_NOTIMPL(timeel)
_NOTIMPL(beta)
_NOTIMPL(logbeta)
_NOTIMPL(gammareg)
_NOTIMPL(betareg)
_NOTIMPL(mathlastrc)
_NOTIMPL(mathlastec)
_NOTIMPL(mathoval)
_NOTIMPL(signpower)
_NOTIMPL(handle)
_NOTIMPL(ncpvusin)
_NOTIMPL(ncpvupow)
_NOTIMPL(binomial)
_NOTIMPL(rehandle)
_NOTIMPL(gamsver)
_NOTIMPL(delhandle)
_NOTIMPL(sleep)
_NOTIMPL(heapf)
_NOTIMPL(cohandle)
_NOTIMPL(gamsrel)
_NOTIMPL(poly)
_NOTIMPL(licensestatus)
_NOTIMPL(licenselevel)
_NOTIMPL(heaplimit)
_NOTIMPL(dummy)


static double RHP_FXPTR_CALLCONV _frac(double x1)
{
   return fmod(x1, 1);
}

static double RHP_FXPTR_CALLCONV _sign(double x1)
{
   if (x1 > 0.) {
      return 1.;
   }

   if (x1 < 0.) {
      return -1.;
   }

   return 0.;
}

static double RHP_FXPTR_CALLCONV _min(double x1, double x2)
{
   return x1 < x2 ? x1 : x2;
}

static double RHP_FXPTR_CALLCONV _max(double x1, double x2)
{
   return x1 > x2 ? x1 : x2;
}

static double RHP_FXPTR_CALLCONV _sqr(double x1)
{
   return x1*x1;
}

static double RHP_FXPTR_CALLCONV _rpower(double x, double exponent)
{
   /* error checking is done elsewhere*/
   return pow(x,exponent);
}

#undef _NOTIMPL
#define _NOTIMPL(X) NOTIMPL_PREFIX ## X


/* -------------------------------------------------------------------------
 * MSVC seems to struggle compiling func_call with C2099 error.
 * Since the latter is related to floating-point, we have to reverse-engineer
 * its struggles ... 
 *
 * It is most likely related to the factor that the libc/libm functions are
 * obtained from DLL. So let's wrap from them for MSVC.
 *
 * Clang-cl seems to do just fine here
 * ------------------------------------------------------------------------- */

#if defined(_MSC_VER) && !defined(__clang__)
#define LIBM_WRAPPER(X)  wrapped_ ## (X)

#define MSVC_WRAPPER1(X) static double RHP_FXPTR_CALLCONV \
   wrapped_ ## X(double x) { return X(x); }

#define MSVC_WRAPPER2(X) static double RHP_FXPTR_CALLCONV \
   wrapped_ ## X(double x, double y) { return X(x, y); }

MSVC_WRAPPER1(ceil)
MSVC_WRAPPER1(floor)
MSVC_WRAPPER1(round)
MSVC_WRAPPER1(trunc)

MSVC_WRAPPER1(exp)
MSVC_WRAPPER1(log)
MSVC_WRAPPER1(log10)
MSVC_WRAPPER1(sqrt)
MSVC_WRAPPER1(fabs)
MSVC_WRAPPER1(cos)
MSVC_WRAPPER1(sin)
MSVC_WRAPPER1(atan)
MSVC_WRAPPER1(erf)

MSVC_WRAPPER1(log2)

MSVC_WRAPPER1(tgamma)
MSVC_WRAPPER1(lgamma)

MSVC_WRAPPER1(sinh)
MSVC_WRAPPER1(cosh)
MSVC_WRAPPER1(tanh)
MSVC_WRAPPER1(tan)
MSVC_WRAPPER1(acos)
MSVC_WRAPPER1(asin)


MSVC_WRAPPER2(fmod)
MSVC_WRAPPER2(pow)
MSVC_WRAPPER2(atan2)

#undef MSVC_WRAPPER1
#undef MSVC_WRAPPER2

#else
#define LIBM_WRAPPER(X) (X)
#endif

/* -------------------------------------------------------------------------
 * This is super unlikely to compile as C++ code since many math function are
 * overloaded in C++. This would need to be changed for a possible compilation
 * in C++ mode.
 * ------------------------------------------------------------------------- */

const reshop_fxptr func_call[fndummy+1] = {
   (reshop_fxptr)&_NOTIMPL(mapval),
   (reshop_fxptr)&LIBM_WRAPPER(ceil),
   (reshop_fxptr)&LIBM_WRAPPER(floor),
   (reshop_fxptr)&LIBM_WRAPPER(round),
   (reshop_fxptr)&LIBM_WRAPPER(fmod),
   (reshop_fxptr)&LIBM_WRAPPER(trunc),
   (reshop_fxptr)&_sign,
   (reshop_fxptr)&_min,
   (reshop_fxptr)&_max,
   (reshop_fxptr)&_sqr,
   (reshop_fxptr)&LIBM_WRAPPER(exp),
   (reshop_fxptr)&LIBM_WRAPPER(log),
   (reshop_fxptr)&LIBM_WRAPPER(log10),
   (reshop_fxptr)&LIBM_WRAPPER(sqrt),
   (reshop_fxptr)&LIBM_WRAPPER(fabs),
   (reshop_fxptr)&LIBM_WRAPPER(cos),
   (reshop_fxptr)&LIBM_WRAPPER(sin),
   (reshop_fxptr)&LIBM_WRAPPER(atan),
   (reshop_fxptr)&LIBM_WRAPPER(erf),
   (reshop_fxptr)&_NOTIMPL(dunfm),
   (reshop_fxptr)&_NOTIMPL(dnorm),
   (reshop_fxptr)&LIBM_WRAPPER(pow),
   (reshop_fxptr)&_NOTIMPL(jdate),
   (reshop_fxptr)&_NOTIMPL(jtime),
   (reshop_fxptr)&_NOTIMPL(jstart),
   (reshop_fxptr)&_NOTIMPL(jnow),
   (reshop_fxptr)&_NOTIMPL(error),
   (reshop_fxptr)&_NOTIMPL(gyear),
   (reshop_fxptr)&_NOTIMPL(gmonth),
   (reshop_fxptr)&_NOTIMPL(gday),
   (reshop_fxptr)&_NOTIMPL(gdow),
   (reshop_fxptr)&_NOTIMPL(gleap),
   (reshop_fxptr)&_NOTIMPL(ghour),
   (reshop_fxptr)&_NOTIMPL(gminute),
   (reshop_fxptr)&_NOTIMPL(gsecond),
   (reshop_fxptr)&_NOTIMPL(curseed),
   (reshop_fxptr)&_NOTIMPL(timest),
   (reshop_fxptr)&_NOTIMPL(timeco),
   (reshop_fxptr)&_NOTIMPL(timeex),
   (reshop_fxptr)&_NOTIMPL(timecl),
   (reshop_fxptr)&_frac,
   (reshop_fxptr)&_NOTIMPL(errorl),
   (reshop_fxptr)&_NOTIMPL(heaps),
   (reshop_fxptr)&_NOTIMPL(fact),
   (reshop_fxptr)&_NOTIMPL(unfmi),
   (reshop_fxptr)&_NOTIMPL(pi),
   (reshop_fxptr)&_NOTIMPL(ncpf),
   (reshop_fxptr)&_NOTIMPL(ncpcm),
   (reshop_fxptr)&_NOTIMPL(entropy),
   (reshop_fxptr)&_NOTIMPL(sigmoid),
   (reshop_fxptr)&LIBM_WRAPPER(log2),
   (reshop_fxptr)&_NOTIMPL(boolnot),
   (reshop_fxptr)&_NOTIMPL(booland),
   (reshop_fxptr)&_NOTIMPL(boolor),
   (reshop_fxptr)&_NOTIMPL(boolxor),
   (reshop_fxptr)&_NOTIMPL(boolimp),
   (reshop_fxptr)&_NOTIMPL(booleqv),
   (reshop_fxptr)&_NOTIMPL(relopeq),
   (reshop_fxptr)&_NOTIMPL(relopgt),
   (reshop_fxptr)&_NOTIMPL(relopge),
   (reshop_fxptr)&_NOTIMPL(reloplt),
   (reshop_fxptr)&_NOTIMPL(relople),
   (reshop_fxptr)&_NOTIMPL(relopne),
   (reshop_fxptr)&_NOTIMPL(ifthen),
   (reshop_fxptr)&_rpower,
   (reshop_fxptr)&_NOTIMPL(edist),
   (reshop_fxptr)&_NOTIMPL(div),
   (reshop_fxptr)&_NOTIMPL(div0),
   (reshop_fxptr)&_NOTIMPL(sllog10),
   (reshop_fxptr)&_NOTIMPL(sqlog10),
   (reshop_fxptr)&_NOTIMPL(slexp),
   (reshop_fxptr)&_NOTIMPL(sqexp),
   (reshop_fxptr)&_NOTIMPL(slrec),
   (reshop_fxptr)&_NOTIMPL(sqrec),
   (reshop_fxptr)&_NOTIMPL(cvpower),
   (reshop_fxptr)&_NOTIMPL(vcpower),
   (reshop_fxptr)&_NOTIMPL(centropy),
   (reshop_fxptr)&_NOTIMPL(gmillisec),
   (reshop_fxptr)&_NOTIMPL(maxerror),
   (reshop_fxptr)&_NOTIMPL(timeel),
   (reshop_fxptr)&LIBM_WRAPPER(tgamma),
   (reshop_fxptr)&LIBM_WRAPPER(lgamma),
   (reshop_fxptr)&_NOTIMPL(beta),
   (reshop_fxptr)&_NOTIMPL(logbeta),
   (reshop_fxptr)&_NOTIMPL(gammareg),
   (reshop_fxptr)&_NOTIMPL(betareg),
   (reshop_fxptr)&LIBM_WRAPPER(sinh),
   (reshop_fxptr)&LIBM_WRAPPER(cosh),
   (reshop_fxptr)&LIBM_WRAPPER(tanh),
   (reshop_fxptr)&_NOTIMPL(mathlastrc),
   (reshop_fxptr)&_NOTIMPL(mathlastec),
   (reshop_fxptr)&_NOTIMPL(mathoval),
   (reshop_fxptr)&_NOTIMPL(signpower),
   (reshop_fxptr)&_NOTIMPL(handle),
   (reshop_fxptr)&_NOTIMPL(ncpvusin),
   (reshop_fxptr)&_NOTIMPL(ncpvupow),
   (reshop_fxptr)&_NOTIMPL(binomial),
   (reshop_fxptr)&_NOTIMPL(rehandle),
   (reshop_fxptr)&_NOTIMPL(gamsver),
   (reshop_fxptr)&_NOTIMPL(delhandle),
   (reshop_fxptr)&LIBM_WRAPPER(tan),
   (reshop_fxptr)&LIBM_WRAPPER(acos),
   (reshop_fxptr)&LIBM_WRAPPER(asin),
   (reshop_fxptr)&LIBM_WRAPPER(atan2),
   (reshop_fxptr)&_NOTIMPL(sleep),
   (reshop_fxptr)&_NOTIMPL(heapf),
   (reshop_fxptr)&_NOTIMPL(cohandle),
   (reshop_fxptr)&_NOTIMPL(gamsrel),
   (reshop_fxptr)&_NOTIMPL(poly),
   (reshop_fxptr)&_NOTIMPL(licensestatus),
   (reshop_fxptr)&_NOTIMPL(licenselevel),
   (reshop_fxptr)&_NOTIMPL(heaplimit),
   (reshop_fxptr)&_NOTIMPL(dummy),
};
