#ifndef RHP_GDX_READER_H
#define RHP_GDX_READER_H

#include "compat.h"
#include "empinterp_data.h"
#include "empinterp_fwd.h"
#include "reshop_data.h"
#include "rhp_fwd.h"

#include "gclgms.h"

struct vm_gms_sym_iterator;

typedef struct {
   int idx;
   int dim;
   int type;
   int alias_idx;                 /**< Alias index     */
   int alias_gdx_type;            /**< Alias type       */
   char alias_name[GMS_SSSIZE];   /**< name of the alias */
} GdxSymData; 

typedef struct gdx_reader {
   const char *fname;
   void*       gdxh;
   void*       dcth;
   GdxSymData symdat;
   IntArray    setobj; 
   GdxMultiSet    multiset;
   double      scalar;
   Lequ * restrict vector;
} GdxReader;

typedef enum gdxSyType GdxSymType;

GdxReader* gdx_readers_new(Interpreter* restrict interp) NONNULL;
GdxReader* gdx_reader_last(Interpreter* restrict interp) NONNULL;

void gdx_reader_free(struct gdx_reader *reader);
NONNULL OWNERSHIP_TAKES(3)
int gdx_reader_init(GdxReader *reader, Model *mdl, const char *fname);

int gdx_reader_readsym(GdxReader *reader, const char *symname) NONNULL;
int gdx_reader_find(GdxReader *reader, const char *symname) NONNULL;
int gdx_reader_getsubset(GdxReader * restrict reader, GdxMultiSet * restrict set,
                         unsigned pos, IntArray * restrict res) NONNULL;
int gdx_reader_boolean_test(GdxReader * restrict reader, struct vm_gms_sym_iterator *filter,
                            bool *res) NONNULL;
void print_vector(const Lequ * restrict vector, unsigned mode, void *gmd) NONNULL;

#endif // !RHP_GDX_READER_H
