#ifndef EMPPARSER_UTILS_H
#define EMPPARSER_UTILS_H

#include <stdbool.h>

#include "compat.h"
#include "empinterp_fwd.h"

typedef enum { OperatorKeywordString, OperatorKeywordScalar } OperatorKeywordType;

typedef void (*setter_operator_optchoice)(void *dat, unsigned idx);
typedef void (*setter_operator_double)(void *dat, double val);
typedef void (*setter_operator_uint)(void *dat, unsigned pos);

typedef struct {
   const char *name;
   bool optional;
   OperatorKeywordType type;
   unsigned position;
   union {
      const char **kwstrs;
   };
   union {
      setter_operator_optchoice strsetter;
      setter_operator_double dblsetter;
      setter_operator_uint uintsetter;
   };
} OperatorKeyword;

int parse_gmsindices(Interpreter * restrict interp, unsigned * restrict p,
                     GmsIndicesData * restrict idxdata) NONNULL;
int parse_labeldefindices(Interpreter * restrict interp, unsigned * restrict p,
                          GmsIndicesData * restrict idxdata);

int resolve_tokasident(Interpreter *interp, IdentData *ident) NONNULL;

NONNULL int parse_operator_kw_args(Interpreter * restrict interp, unsigned * restrict p,
                           unsigned op_kws_size, const OperatorKeyword *op_kws, void *dat);
#endif
