#ifndef EMPINTERP_UTILS_H
#define EMPINTERP_UTILS_H

#include "compat.h"
#include "empinterp.h"

#include "dctmcc.h"

enum DctResolveDataType {
   GmsSymIteratorTypeImm,
   GmsSymIteratorTypeVm,
};

typedef struct {
   TokenType toktype;
   int symidx;
   int uels[GMS_MAX_INDEX_DIM];
   GmsSymIterator *symiter;
} GmsSymIteratorImm;

struct vm_gms_sym_iterator;

typedef struct  {
   enum DctResolveDataType type;
   union {
      GmsSymIteratorImm imm;
      struct vm_gms_sym_iterator *vm;
   } symiter;
   IntScratch *scratch;
   union {
      Avar *v;
      Aequ *e;
   } payload;
} DctResolveData;

int dct_resolve(dctHandle_t dct, DctResolveData * restrict data) NONNULL;
void dct_printuel(dctHandle_t dct, int uel, unsigned mode, int *offset);
void dct_printuelwithidx(dctHandle_t dct, int uel, unsigned mode) NONNULL;

int genlabelname(DagRegisterEntry * restrict entry, dctHandle_t dcth,
                 char **labelname) NONNULL;
OWNERSHIP_RETURNS
DagRegisterEntry* regentry_new(const char *basename, unsigned basename_len, 
                               uint8_t dim) NONNULL;
OWNERSHIP_RETURNS
DagRegisterEntry* regentry_dup(DagRegisterEntry *regentry) NONNULL;

int runtime_error(unsigned linenr);
int interp_ops_is_imm(Interpreter *interp) NONNULL;
int has_longform_solve(Interpreter *interp) NONNULL;

#endif // !EMPINTERP_UTILS_H
