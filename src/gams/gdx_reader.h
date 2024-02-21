#ifndef RHP_GDX_READER_H
#define RHP_GDX_READER_H

#include "compat.h"
#include "empinterp_data.h"
#include "reshop_data.h"
#include "rhp_fwd.h"

#include "gclgms.h"

struct vm_gms_sym_iterator;

typedef struct gdx_reader {
   const char *fname;
   void*       gdxh;
   void*       dcth;
   GamsSymData cursym;
   IntArray    setobj; 
   MultiSet    multiset;
   double      scalar;
   Lequ * restrict vector;
} GdxReader;

typedef enum gdxSyType GdxSymType;

void gdx_reader_free(struct gdx_reader *reader);
NONNULL OWNERSHIP_TAKES(3)
int gdx_reader_init(GdxReader *reader, Model *mdl, const char *fname);

int gdx_reader_readsym(GdxReader *reader, const char *symname) NONNULL;
int gdx_reader_find(GdxReader *reader, const char *symname) NONNULL;
int gdx_reader_getsubset(GdxReader * restrict reader, MultiSet * restrict set,
                         unsigned pos, IntArray * restrict res) NONNULL;
int gdx_reader_boolean_test(GdxReader * restrict reader, struct vm_gms_sym_iterator *filter,
                            bool *res) NONNULL;
void print_vector(const Lequ * restrict vector, unsigned mode, void *dct) NONNULL;

#endif // !RHP_GDX_READER_H
