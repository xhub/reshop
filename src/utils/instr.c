#include <assert.h>
#include <stddef.h>

#include "instr.h"
#include "printout.h"

#define DEFINE_STR() \
DEFSTR(nlNoOp,"NoOp") \
DEFSTR(nlPushV,"PushV") \
DEFSTR(nlPushI,"PushI") \
DEFSTR(nlStore,"Store") \
DEFSTR(nlAdd,"Add") \
DEFSTR(nlAddV,"AddV") \
DEFSTR(nlAddI,"AddI") \
DEFSTR(nlSub,"Sub") \
DEFSTR(nlSubV,"SubV") \
DEFSTR(nlSubI,"SubI") \
DEFSTR(nlMul,"Mul") \
DEFSTR(nlMulV,"MulV") \
DEFSTR(nlMulI,"MulI") \
DEFSTR(nlDiv,"Div") \
DEFSTR(nlDivV,"DivV") \
DEFSTR(nlDivI,"DivI") \
DEFSTR(nlUMin,"UMin") \
DEFSTR(nlUMinV,"UMinV") \
DEFSTR(nlHeader,"Header") \
DEFSTR(nlEnd,"End") \
DEFSTR(nlCallArg1,"CallArg1") \
DEFSTR(nlCallArg2,"CallArg2") \
DEFSTR(nlCallArgN,"CallArgN") \
DEFSTR(nlFuncArgN,"FuncArgN") \
DEFSTR(nlMulIAdd,"MulIAdd") \
DEFSTR(nlPushZero,"PushZero") \
DEFSTR(nlChk,"Chk") \
DEFSTR(nlAddO,"AddO") \
DEFSTR(nlPushO,"PushO") \
DEFSTR(nlInvoc,"Invoc") \
DEFSTR(nlStackIn,"StackIn")


#define DEFSTR(id, str) char id[sizeof(str)];

typedef struct {
   union {
      struct {
      DEFINE_STR()
   };
   char dummystr[1];
   };
} OpcodeNames;
#undef DEFSTR

#define DEFSTR(id, str) .id = str,

static const OpcodeNames opnames = {
DEFINE_STR()
};
#undef DEFSTR

#define DEFSTR(id, str) offsetof(OpcodeNames, id),
static const unsigned opnames_offsets[] = {
DEFINE_STR()
};

static_assert(sizeof(opnames_offsets)/sizeof(opnames_offsets[0]) == __instr_code_size,
              "Incompatible sizes!");

const char * instr_code_name(enum instr_code instr)
{
   if (instr >= __instr_code_size) { return "unknown GAMS opcode"; }   

   return opnames.dummystr + opnames_offsets[instr];
}


const char * const nlopt_name[__instr_code_size] = {
   "NoOp",
   "PushV",
   "PushI",
   "Store",
   " + ",
   "AddV",
   "AddI",
   " - ",
   "SubV",
   "SubI",
   "*",
   "MulV",
   "MulI",
   "/",
   "DivV",
   "DivI",
   "-",
   "UMinV",
   "Header",
   "End",
   "CallArg1",
   "CallArg2",
   "CallArgN",
   "FuncArgN",
   "MulIAdd",
   "PushZero",
   "Chk",
   "AddO",
   "PushO",
   "Invoc",
   "StackIn"
};

const char * const func_code_name[fndummy+1] = {
   "mapval",
   "ceil",
   "floor",
   "round",
   "mod",
   "trunc",
   "sign",
   "min",
   "max",
   "sqr",
   "exp",
   "log",
   "log10",
   "sqrt",
   "abs",
   "cos",
   "sin",
   "arctan",
   "errf",
   "dunfm",
   "dnorm",
   "power",
   "jdate",
   "jtime",
   "jstart",
   "jnow",
   "error",
   "gyear",
   "gmonth",
   "gday",
   "gdow",
   "gleap",
   "ghour",
   "gminute",
   "gsecond",
   "curseed",
   "timest",
   "timeco",
   "timeex",
   "timecl",
   "frac",
   "errorl",
   "heaps",
   "fact",
   "unfmi",
   "pi",
   "ncpf",
   "ncpcm",
   "entropy",
   "sigmoid",
   "log2",
   "boolnot",
   "booland",
   "boolor",
   "boolxor",
   "boolimp",
   "booleqv",
   "relopeq",
   "relopgt",
   "relopge",
   "reloplt",
   "relople",
   "relopne",
   "ifthen",
   "rpower",
   "edist",
   "div",
   "div0",
   "sllog10",
   "sqlog10",
   "slexp",
   "sqexp",
   "slrec",
   "sqrec",
   "cvpower",
   "vcpower",
   "centropy",
   "gmillisec",
   "maxerror",
   "timeel",
   "gamma",
   "loggamma",
   "beta",
   "logbeta",
   "gammareg",
   "betareg",
   "sinh",
   "cosh",
   "tanh",
   "mathlastrc",
   "mathlastec",
   "mathoval",
   "signpower",
   "handle",
   "ncpvusin",
   "ncpvupow",
   "binomial",
   "rehandle",
   "gamsver",
   "delhandle",
   "tan",
   "arccos",
   "arcsin",
   "arctan2",
   "sleep",
   "heapf",
   "cohandle",
   "gamsrel",
   "poly",
   "licensestatus",
   "licenselevel",
   "heaplimit",
   "dummy"
};

const char *instr_getfuncname(unsigned ifunc)
{
   if (ifunc == fnpower) {
      return "POWER";
   }
   if (ifunc == fnrpower || ifunc == fncvpower || ifunc == fnvcpower) {
      return "**";
   }

   return func_code_name[ifunc];
}

void gams_opcode_print(unsigned mode, int *instrs, int *args, int len)
{
   for (int i = 0; i < len; ++i) {
      printout(mode, "[%5d]\t  Instr: %10s, Field: %5d\n",
               i, instr_code_name(instrs[i]), args[i]);

   }
}


