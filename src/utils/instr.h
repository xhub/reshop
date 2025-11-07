#ifndef INSTR_H
#define INSTR_H

enum nlopcode {
   nlNoOp    = 0,    /**< no operation                              */
   nlPushV,          /**< push variable                             */
   nlPushI,          /**< push immediate (constant)                 */
   nlStore,          /**< store row                                 */
   nlAdd,            /**< add                                       */
   nlAddV,           /**< add variable                              */
   nlAddI,           /**< add immediate                             */
   nlSub,            /**< subtract                                  */
   nlSubV,           /**< subtract variable                         */
   nlSubI,           /**< subtract immediate                        */
   nlMul,            /**< multiply                                  */
   nlMulV,           /**< multiply variable                         */
   nlMulI,           /**< multiply immediate                        */
   nlDiv,            /**< divide                                    */
   nlDivV,           /**< divide variable                           */
   nlDivI,           /**< divide immediate                          */
   nlUMin,           /**< unary minus                               */
   nlUMinV,          /**< unary minus variable                      */
   nlHeader,         /**< header                                    */
   nlEnd,            /**< end of instruction list                   */
   nlCallArg1,       /**< function call with 1 argument             */
   nlCallArg2,       /**< function call with 2 arguments            */
   nlCallArgN,       /**< function call with N arguments            */
   nlFuncArgN,       /**< number of arguments (before nlCallArgN )  */
   nlMulIAdd,        /**< multiply by constant and add (FMA-like)   */
   nlPushZero,       /**< Push a zero constant                      */
   nlChk,
   nlAddO,
   nlPushO,
   nlInvoc,
   nlStackIn,
   nlOpcode_size
};

enum func_code {
   fnmapval=0,fnceil,fnfloor,fnround,
   fnmod,fntrunc,fnsign,fnmin,
   fnmax,fnsqr,fnexp,fnlog,
   fnlog10,fnsqrt,fnabs,fncos,
   fnsin,fnarctan,fnerrf,fndunfm,
   fndnorm,fnpower,fnjdate,fnjtime,
   fnjstart,fnjnow,fnerror,fngyear,
   fngmonth,fngday,fngdow,fngleap,
   fnghour,fngminute,fngsecond,
   fncurseed,fntimest,fntimeco,
   fntimeex,fntimecl,fnfrac,fnerrorl,
   fnheaps,fnfact,fnunfmi,fnpi,
   fnncpf,fnncpcm,fnentropy,fnsigmoid,
   fnlog2,fnboolnot,fnbooland,
   fnboolor,fnboolxor,fnboolimp,
   fnbooleqv,fnrelopeq,fnrelopgt,
   fnrelopge,fnreloplt,fnrelople,
   fnrelopne,fnifthen,fnrpower,
   fnedist,fndiv,fndiv0,fnsllog10,
   fnsqlog10,fnslexp,fnsqexp,fnslrec,
   fnsqrec,fncvpower,fnvcpower,
   fncentropy,fngmillisec,fnmaxerror,
   fntimeel,fngamma,fnloggamma,fnbeta,
   fnlogbeta,fngammareg,fnbetareg,
   fnsinh,fncosh,fntanh,fnmathlastrc,
   fnmathlastec,fnmathoval,fnsignpower,
   fnhandle,fnncpvusin,fnncpvupow,
   fnbinomial,fnrehandle,fngamsver,
   fndelhandle,fntan,fnarccos,
   fnarcsin,fnarctan2,fnsleep,fnheapf,
   fncohandle,fngamsrel,fnpoly,
   fnlicensestatus,fnlicenselevel,fnheaplimit,
   fnlinear,fntriangle,fnforceerror,
   fnforceerrorcount,fnrandbinomial,fnjobhandle,
   fnjobstatus,fnjobkill,fnjobterminate,
   fnnumcores,fnEmbeddedHandle,fnPlatformCode,
   fnLogit,
   fnlsemax, fnlsemaxsc, fnlsemin, fnlseminsc,
   fndummy
};

extern const char * const func_code_name[fndummy+1];


/* Due to microsoft calling convention peculiarities, we specify
 * the calling convention here. It seems that the libc/libm functions
 * on windows are __cdecl (and not __stdcall) */

#ifdef _WIN32
#define RHP_FXPTR_CALLCONV __cdecl
#else
#define RHP_FXPTR_CALLCONV 
#endif

typedef double (RHP_FXPTR_CALLCONV *reshop_fxptr)(void);

extern const reshop_fxptr func_call[fndummy+1];

enum nlconst {
   nlconst_one       = 1,
   nlconst_ten       = 2,
   nlconst_tenth     = 3,
   nlconst_quarter   = 4,
   nlconst_half      = 5,
   nlconst_two       = 6,
   nlconst_four      = 7,
   nlconst_zero      = 8,
   nlconst_oosqrt2pi = 9,    /**< 1/sqrt(2*pi) */
   nlconst_ooln10    = 10,   /**< 1/ln(10)     */
   nlconst_ooln2     = 11,   /**< 1/ln(2)      */
   nlconst_pi        = 12,   /**< pi           */
   nlconst_pihalf    = 13,   /**< pi/2         */
   nlconst_sqrt2     = 14,   /**< sqrt(2)      */
   nlconst_three     = 15,
   nlconst_five      = 16,
   nlconst_size      = nlconst_five,
};

const char* nlopcode_argfunc2str(int ifunc);
const char* nlinstr2str(int instr);
void nlopcode_print(unsigned mode, const int *instrs, const int *args, int len);

#endif /* INSTR_H */
