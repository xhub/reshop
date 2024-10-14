#ifndef EMPINTERP_SYMBOL_RESOLVER_H
#define EMPINTERP_SYMBOL_RESOLVER_H

#include "empinterp.h"
#include "empparser_data.h"
#include "reshop_data.h"
#include "rhp_fwd.h"

#include "gclgms.h"

typedef struct dctRec *dctHandle_t;
typedef struct gmdRec *gmdHandle_t;

typedef enum {
   GmsSymIteratorTypeImm,
   GmsSymIteratorTypeVm,
} GmsResolveDataType;

typedef struct {
   TokenType toktype;
   int symidx;
   int uels[GMS_MAX_INDEX_DIM];
   GmsSymIterator *symiter;
} ImmGmsSymIterator;

typedef struct vm_gms_sym_iterator VmGmsSymIterator;

typedef struct {
   GmsResolveDataType type;
   union {
      ImmGmsSymIterator imm;
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

int dct_read_equvar(dctHandle_t dct, GmsResolveData * restrict data) NONNULL;
void dct_printuel(dctHandle_t dct, int uel, unsigned mode, int *offset);
void dct_printuelwithidx(dctHandle_t dct, int uel, unsigned mode) NONNULL;

int gmd_read(gmdHandle_t gmd, GmsResolveData * restrict data, const char *symname) NONNULL;
void gmd_printuel(gmdHandle_t gmd, int uel, unsigned mode, int *offset);
void gmd_printuelwithidx(gmdHandle_t gmd, int uel, unsigned mode) NONNULL;

#endif
