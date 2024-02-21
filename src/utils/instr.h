#ifndef INSTR_H
#define INSTR_H

enum instr_code {
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
   __instr_code_size
};

enum optype {
   opError    = 0,
   opIgnore,
   opStore,
   opAlpha,
   opFuncU,
   opFuncB,
   opMult,
   opAdd,
   opUnary,
   opNewFunc,
   opArg,
   opPushS,
   __optype_size
};

enum func_code {
   fnmapval   =  0,
   fnceil,
   fnfloor,
   fnround,
   fnmod,
   fntrunc,             /*  5 */
   fnsign,
   fnmin,
   fnmax,
   fnsqr,
   fnexp,               /* 10 */
   fnlog,
   fnlog10,
   fnsqrt,
   fnabs,
   fncos,               /* 15 */
   fnsin,
   fnarctan,
   fnerrf,
   fndunfm,
   fndnorm,             /* 20 */
   fnpower,
   fnjdate,
   fnjtime,
   fnjstart,
   fnjnow,              /* 25 */
   fnerror,
   fngyear,
   fngmonth,
   fngday,
   fngdow,              /* 30 */
   fngleap,
   fnghour,
   fngminute,
   fngsecond,
   fncurseed,           /* 35 */
   fntimest,
   fntimeco,
   fntimeex,
   fntimecl,
   fnfrac,              /* 40 */
   fnerrorl,
   fnheaps,
   fnfact,
   fnunfmi,
   fnpi,                /* 45 */
   fnncpf,
   fnncpcm,
   fnentropy,
   fnsigmoid,
   fnlog2,              /* 50 */
   fnboolnot,
   fnbooland,
   fnboolor,
   fnboolxor,
   fnboolimp,           /* 55 */
   fnbooleqv,
   fnrelopeq,
   fnrelopgt,
   fnrelopge,
   fnreloplt,           /* 60 */
   fnrelople,
   fnrelopne,
   fnifthen,
   fnrpower,
   fnedist,             /* 65 */
   fndiv,
   fndiv0,
   fnsllog10,
   fnsqlog10,
   fnslexp,             /* 70 */
   fnsqexp,
   fnslrec,
   fnsqrec,
   fncvpower,
   fnvcpower,           /* 75 */
   fncentropy,
   fngmillisec,
   fnmaxerror,
   fntimeel,
   fngamma,             /* 80 */
   fnloggamma,
   fnbeta,
   fnlogbeta,
   fngammareg,
   fnbetareg,           /* 85 */
   fnsinh,
   fncosh,
   fntanh,
   fnmathlastrc,
   fnmathlastec,        /* 90 */
   fnmathoval,
   fnsignpower,
   fnhandle,
   fnncpvusin,
   fnncpvupow,          /* 95 */
   fnbinomial,
   fnrehandle,
   fngamsver,
   fndelhandle,
   fntan,               /* 100 */
   fnarccos,
   fnarcsin,
   fnarctan2,
   fnsleep,
   fnheapf,             /* 105 */
   fncohandle,
   fngamsrel,
   fnpoly,
   fnlicensestatus,
   fnlicenselevel,      /* 110 */
   fnheaplimit,
   fndummy
};

extern const char * const nlopt_name[__instr_code_size];
extern const char * const func_code_name[fndummy+1];

typedef double (*reshop_fxptr)(void);

extern const reshop_fxptr func_call[fndummy+1];

enum nlconst {
   nlconst_one       = 1,
   nlconst_ten       = 2,
   nlconst_tenth     = 3,
   nlconst_quater    = 4,
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
   _gams_nlconst_size
};

const char* instr_getfuncname(unsigned ifunc);
const char* instr_code_name(enum instr_code instr);
void gams_opcode_print(unsigned mode, int *instrs, int *args, int len);

#endif /* INSTR_H */
