#ifndef EMPINTERP_UTILS_H
#define EMPINTERP_UTILS_H

#include "compat.h"
#include "empinterp.h"

#include "dctmcc.h"
#include "gmdcc.h"

enum GmsResolveDataType {
   GmsSymIteratorTypeImm,
   GmsSymIteratorTypeVm,
};

typedef struct {
   TokenType toktype;
   int symidx;
   int uels[GMS_MAX_INDEX_DIM];
   GmsSymIterator *symiter;
} GmsSymIteratorImm;

typedef struct vm_gms_sym_iterator VmGmsSymIterator;

typedef struct {
   enum GmsResolveDataType type;
   union {
      GmsSymIteratorImm imm;
      VmGmsSymIterator *vm;
   } symiter;
   IntScratch *iscratch;
   DblScratch *dscratch;
   int itmp;
   double dtmp;
   unsigned nrecs;
   bool allrecs;
   union {
      Avar *v;
      Aequ *e;
   } payload;
} GmsResolveData;

int dct_resolve(dctHandle_t dct, GmsResolveData * restrict data) NONNULL;
void dct_printuel(dctHandle_t dct, int uel, unsigned mode, int *offset);
void dct_printuelwithidx(dctHandle_t dct, int uel, unsigned mode) NONNULL;

int gmd_resolve(gmdHandle_t gmd, GmsResolveData * restrict data) NONNULL;
void gmd_printuel(gmdHandle_t gmd, int uel, unsigned mode, int *offset);
void gmd_printuelwithidx(gmdHandle_t gmd, int uel, unsigned mode) NONNULL;

int genlabelname(DagRegisterEntry * restrict entry, Interpreter *interp,
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
