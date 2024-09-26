#include "asprintf.h"

#include "ctrdat_gams.h"
#include "empinterp_vm_utils.h"
#include "gdx_reader.h"
#include "gevmcc.h"
#include "lequ.h"
#include "macros.h"
#include "mdl.h"
#include "printout.h"
#include "reshop_data.h"

#include "gamsapi_utils.h"

static tlsvar int gdxerr = OK;

static tlsvar bool test_result = false;
static tlsvar Lequ* vector_global = NULL;
static tlsvar unsigned vector_pos = UINT_MAX;
static tlsvar IntArray *vector_filter = NULL;


static tlsvar IntArray *subset = NULL;
static tlsvar unsigned subset_pos = UINT_MAX;

/* function of type TDataStoreProc_t */
static void GDX_CALLCONV _test_membership(UNUSED const int *Indx,
                                          UNUSED const double *Vals)
{
   test_result = true;
}

UNUSED static void GDX_CALLCONV store_vector(const int *Indx, const double *Vals)
{
   assert(vector_pos < GMS_MAX_INDEX_DIM && vector_global);
   if (gdxerr != OK) return;

   gdxerr = lequ_add(vector_global, Indx[vector_pos], Vals[GMS_VAL_LEVEL]);
}

static void GDX_CALLCONV store_subset(const int Indx[], UNUSED const double Vals[])
{
   assert(subset_pos < GMS_MAX_INDEX_DIM && subset);
   if (gdxerr != OK) return;

   gdxerr = rhp_int_addsorted(subset, Indx[subset_pos]);
}

UNUSED static void GDX_CALLCONV store_vector_filt(const int Indx[], const double Vals[])
{
   assert(vector_pos < GMS_MAX_INDEX_DIM);
   assert(vector_filter);

   if (gdxerr != OK) return;

   int idx = Indx[vector_pos];
   int * restrict indices = vector_filter->arr;
   assert(indices);

   for (unsigned i = 0, len = vector_filter->len; i < len; ++i) {
      if (idx == indices[i]) {
         gdxerr = lequ_add(vector_global, idx, Vals[GMS_VAL_LEVEL]);
         return;
      }
   }
}
/* END TDataStoreProc_t functions */

GdxReader* gdx_readers_new(Interpreter* restrict interp)
{
   GdxReaders * restrict gdxreaders = &interp->gdx_readers;
   if (gdxreaders->len >= gdxreaders->max) {
      gdxreaders->max = MAX(2*gdxreaders->max, 2);
      REALLOC_NULL(gdxreaders->list, GdxReader, gdxreaders->max);
   }

   return &gdxreaders->list[gdxreaders->len++];
}

GdxReader* gdx_reader_last(Interpreter* restrict interp)
{
   GdxReaders * restrict gdxreaders = &interp->gdx_readers;
   return gdxreaders->len > 0 ? &gdxreaders->list[gdxreaders->len-1] : NULL;
}


void gdx_reader_free(GdxReader *reader)
{
   if (!reader) return;

   FREE(reader->fname);

   if (reader->gdxh) {
      gdx_call0_rc(gdxClose, reader->gdxh);
      gdxHandle_t gdxh = reader->gdxh;
      gdxFree(&gdxh);
      reader->gdxh = NULL;
   }

   FREE(reader->vector); /* Lequ content is owned by a namedlist */
}

int gdx_reader_init(GdxReader *reader, Model *mdl, const char *fname)
{
   char msg[GMS_SSSIZE];
   reader->vector = NULL;

   reader->fname = fname;
   if (!gdxLibraryLoaded()) {
      error("%s :: gdx library not loaded!\n", __func__);
      return Error_RuntimeError;
   }

   gdxHandle_t gdxh = reader->gdxh;
   if (!gdxCreate(&gdxh, msg, sizeof(msg))) {
      error("%s :: Could not create gdx handle\n", __func__);
      return Error_RuntimeError;
   }
   reader->gdxh = gdxh;

   if (O_Output & PO_TRACE_EMPINTERP) {
      trace_empparser("[empinterp] opening file '%s'\n", fname);
   }

   int rc;
   gdxOpenRead(reader->gdxh, fname, &rc);
   if (rc) {
      gdxerror(gdxOpenRead, reader->gdxh);
      error("[empinterp] Could not open gdx file '%s'\n", fname);
      return Error_RuntimeError;
   }

   reader->dcth = ((struct ctrdata_gams*)mdl->ctr.data)->dct;

   if (!reader->dcth) {
      error("%s :: no DCT handle in model!\n", __func__);
      return Error_RuntimeError;
   }

   return OK;
}

int gdx_reader_find(GdxReader *reader, const char *symname)
{
   assert(reader->gdxh);
   GdxSymData *symdat = &reader->symdat;

   int symidx, symdim, symtype;
   if (!gdxFindSymbol(reader->gdxh, symname, &symidx) || symidx < 0) {
      error("[empinterp] ERROR: couldn't find symbol '%s' in file '%s'\n",
                      symname, reader->fname);
      //reader->cursym.idx = -2;
      return Error_NotFound;
   }
   symdat->idx = symidx;

   char buf[GMS_SSSIZE];
   gdxSymbolInfo(reader->gdxh, symidx, buf, &symdim, &symtype);

   symdat->dim = symdim;
   symdat->type = symtype;


   if (symtype > GMS_DT_MAX) {
      error("[empinterp] ERROR gdx file '%s': symbol '%s' has type %u > %u (max)\n",
            reader->fname, symname, symtype, GMS_DT_MAX);
      return Error_RuntimeError;
   }

   if (O_Output & PO_TRACE_EMPINTERP) {
      trace_empparser("[empinterp] In gdx file '%s', found symbol '%s' of type '%s', "
                      "dimension %d and index %d\n", reader->fname, symname,
                      gmsGdxTypeText[symtype], symdim, symidx);
   }
   return OK;
}

static int gdx_reader_readset(GdxReader * restrict reader, const char *setname)
{
   gdxHandle_t gdxh = reader->gdxh;
   dctHandle_t dct  = reader->dcth;
   gdxValues_t        values;
   gdxStrIndex_t      uels;
   gdxStrIndexPtrs_t  uels_str;

   assert(gdxh);
   assert(reader->symdat.dim == 1);

   rhp_int_init(&reader->setobj);

   /* ---------------------------------------------------------------------
    * We use gdxDataReadStr to get the string representation of the UELs
    * and then lookup its index via the DCT interface
    *
    * If we have the GUARANTEE that the universal set in the gdx is the same
    * as the dct one, then we could just use gdxDataReadRaw
    * --------------------------------------------------------------------- */

   int nrecs;
   gdx_call_rc(gdxDataReadStrStart, gdxh, reader->symdat.idx, &nrecs);

   if (nrecs <= 0) {
      error("%s :: Set '%s': invalid record size %d", __func__, setname, nrecs);
      return Error_RuntimeError;
   }

   if (O_Output & PO_TRACE_EMPINTERP) {
      trace_empparser("[empinterp] Reading set '%s' with %d records\n", setname, nrecs);
   }

   S_CHECK(rhp_int_reserve(&reader->setobj, nrecs));

   int dummyint;
   /* "magic" stuff from gdx */
   GDXSTRINDEXPTRS_INIT(uels,uels_str);

   for (int i = 0; i < nrecs; ++i) {
      gdx_call_rc(gdxDataReadStr, gdxh, uels_str, values, &dummyint);
      int uel = dctUelIndex(dct, uels_str[0]);
      rhp_int_add(&reader->setobj, uel);

      if (O_Output & PO_TRACE_EMPINTERP) {
         trace_empparser("[empinterp] Adding uel [%5d] '%s'\n", uel, uels_str[0]);
      }

   }

   gdxDataReadDone(gdxh);

   return OK;
}

static int gdx_reader_readscalar(GdxReader *reader, const char *symname)
{
   gdxHandle_t gdxh = reader->gdxh;
   gdxValues_t        values;
   int dummyuels[GMS_MAX_INDEX_DIM] = {0};

   assert(gdxh);
   assert(reader->symdat.dim == 0);

   int nrecs;
   gdx_call_rc(gdxDataReadRawStart, gdxh, reader->symdat.idx, &nrecs);
   assert(nrecs == 1);

   int dummyint;
   gdx_call_rc(gdxDataReadRaw, gdxh, dummyuels, values, &dummyint);
   reader->scalar = values[GMS_VAL_LEVEL];

   if (O_Output & PO_TRACE_EMPINTERP) {
      trace_empparser("[empinterp] Scalar parameter '%s' has value %e\n", symname,
                      reader->scalar);
   }

   gdxDataReadDone(gdxh);


   return OK;
}

static int gdx_reader_readvector(GdxReader *reader, const char *symname)
{
   gdxHandle_t gdxh = reader->gdxh;
   dctHandle_t dct  = reader->dcth;
   gdxValues_t        values;
   gdxStrIndex_t      uels;
   gdxStrIndexPtrs_t  uels_str;

   assert(gdxh);
   assert(reader->symdat.dim == 1);

   int nrecs;
   gdx_call_rc(gdxDataReadStrStart, gdxh, reader->symdat.idx, &nrecs);
   assert(nrecs >= 1);

   A_CHECK(reader->vector, lequ_alloc(nrecs));

   /* ---------------------------------------------------------------------
    * We use gdxDataReadStr to get the string representation of the UELs
    * and then lookup its index via the DCT interface
    *
    * If we have the GUARANTEE that the universal set in the gdx is the same
    * as the dct one, then we could just use gdxDataReadRaw
    * --------------------------------------------------------------------- */

   int dummyint;
   /* "magic" stuff from gdx */
   GDXSTRINDEXPTRS_INIT(uels,uels_str);

   for (int i = 0; i < nrecs; ++i) {
      gdx_call_rc(gdxDataReadStr, gdxh, uels_str, values, &dummyint);
      int uel = dctUelIndex(dct, uels_str[0]);
      reader->vector->vis[i] = uel;
      reader->vector->coeffs[i] = values[GMS_VAL_LEVEL];
   }
    reader->vector->len = nrecs;

   if (O_Output & PO_TRACE_EMPINTERP) {
      trace_empparser("[empinterp] 1D parameter '%s' has %d entries:\n", symname,
                      nrecs);
      print_vector(reader->vector, PO_TRACE_EMPINTERP, dct);

   }

   gdxDataReadDone(gdxh);


   return OK;
}

int gdx_reader_readsym(GdxReader *reader, const char *symname)
{
   S_CHECK(gdx_reader_find(reader, symname));

   int type = reader->symdat.type;
   switch (type) {
   case GMS_DT_SET:
      if (reader->symdat.dim == 1) {
         return gdx_reader_readset(reader, symname);
      } else { /* The callee gets the data and handles it */
         return OK;
      }
      break;
   case GMS_DT_ALIAS: {
      int dummyint, idx_alias, idx = reader->symdat.idx, symdim;
      char buf[GMS_SSSIZE];
      gdxSymbolInfoX(reader->gdxh, idx, &dummyint, &idx_alias, buf);

      trace_empdag("[empinterp] In gdx '%s': symbol '%s' is an alias to %d.\n",
                   reader->fname, symname, idx_alias);

      gdxSymbolInfo(reader->gdxh, idx_alias, reader->symdat.alias_name, &symdim,
                    &reader->symdat.alias_gdx_type);

      reader->symdat.alias_idx = idx_alias;
      reader->symdat.type = GMS_DT_ALIAS;
      reader->symdat.dim = symdim;

      return OK;
   }
   case GMS_DT_PAR: {
      int dim  = reader->symdat.dim;
      if (dim == 0) { return gdx_reader_readscalar(reader, symname); }
      if (dim == 1) { return gdx_reader_readvector(reader, symname); }

      error("[empinterp] In gdx '%s': parameter '%s' has dimension %d, only"
            "0 (scalar) and 1 are currently supported", reader->fname,
            symname, dim);
      return Error_NotImplemented;
   }
   default:
      error("[empinterp] ERROR in gdx file '%s': symbol '%s' is neither a set "
            "or a parameter\n", reader->fname, symname);
      return Error_NotImplemented;
   }
}

int gdx_reader_boolean_test(GdxReader * restrict reader, VmGmsSymIterator *filter,
                            bool *res)
{
   gdxUelIndex_t elt_cnt;
   gdxStrIndex_t UELfilter;
   const char * UELfilter_ptr[GMS_MAX_INDEX_DIM];
   gdxHandle_t gdxh = reader->gdxh;
   dctHandle_t dcth = reader->dcth;

   assert(filter->ident.origin == IdentOriginGdx);
   /* reset tlsvar */
   test_result = false;

   gdx_call_rc(gdxDataReadSliceStart, gdxh, filter->ident.idx, elt_cnt);

   for (unsigned i = 0, len = filter->ident.dim; i < len; ++i) {
      int uel = filter->uels[i];
      if (uel > 0) {
         char dummyquote = ' ';
         dct_call_rc(dctUelLabel, dcth, uel, &dummyquote, UELfilter[i], sizeof(UELfilter[i]));
      } else {
         UELfilter[i][0] = ' ';
         UELfilter[i][1] = '\0';
      }
   }

   int dummyint;
   GDXSTRINDEXPTRS_INIT(UELfilter, UELfilter_ptr);
   gdx_call_rc(gdxDataReadSlice, gdxh, UELfilter_ptr, &dummyint, _test_membership);

   gdx_call0_rc0(gdxDataReadDone, gdxh);

   *res = test_result;

   if (O_Output & PO_TRACE_EMPINTERP) {
      char symname[GMS_SSSIZE];
      int symdim, symtype;
      gdx_call_rc(gdxSymbolInfo,reader->gdxh, filter->ident.idx, symname, &symdim, &symtype);
      trace_empparser("[empinterp] Testing if '%.*s' has an entry for indices (%s",
                      (int)sizeof(symname), symname, UELfilter[0]);
      for (unsigned i = 1, len = filter->ident.dim; i < len; ++i) {
         trace_empparser(", %s", UELfilter[i]);
      }
      trace_empparser("): result = %s\n", test_result ? "TRUE" : "FALSE");
   }
   return OK;
}

int gdx_reader_getsubset(GdxReader * restrict reader, GdxMultiSet * restrict set,
                         unsigned pos, IntArray * restrict res)
{
   int status = OK;
   gdxUelIndex_t elt_cnt;
   gdxStrIndex_t UELfilter;
   const char * UELfilter_ptr[GMS_MAX_INDEX_DIM];
   gdxHandle_t gdxh = reader->gdxh;

   if (pos >= set->dim) {
      char symname[GMS_SSSIZE];
      int symdim, symtype;
      gdxSymbolInfo(reader->gdxh, set->idx, symname, &symdim, &symtype);
      error("[empinterp] In gdx file '%s', the set '%s' has dimension %d and we "
            "seek the subset at position %d\n", reader->fname, symname, set->dim,
            pos);
      return Error_EMPRuntimeError;
   }

   /* reset tlsvar */
   subset = res;
   subset_pos = pos;


   for (unsigned i = 0, len = set->dim; i < len; ++i) {
      int uel = set->uels[i];
      if (uel > 0) {
         gdx_call_rc(gdxGetUEL, gdxh, uel, UELfilter[i]);
      } else {
         UELfilter[i][0] = ' ';
         UELfilter[i][1] = '\0';
      }

   }

   gdx_call_rc(gdxDataReadSliceStart, gdxh, set->idx, elt_cnt);

   int subset_upperbnd = elt_cnt[pos];
   if (subset_upperbnd <= 0) {
      char symname[GMS_SSSIZE];
      int symdim, symtype;
      gdxSymbolInfo(reader->gdxh, set->idx, symname, &symdim, &symtype);

      error("[empinterp] In gdx file '%s', in the set '%s', the number of elements "
            "at the position %u is %d\n", reader->fname, symname, pos, subset_upperbnd);
      status = Error_EMPRuntimeError;
      goto _exit;
   }

   S_CHECK_EXIT(rhp_int_reserve(res, subset_upperbnd));

   int dummyint;
   GDXSTRINDEXPTRS_INIT(UELfilter, UELfilter_ptr);
   gdx_call_rc(gdxDataReadSlice, gdxh, UELfilter_ptr, &dummyint, store_subset);

   gdx_call0_rc0(gdxDataReadDone, gdxh);

_exit:
   subset = NULL;
   subset_pos = UINT_MAX;

   return status;
}

void print_vector(const Lequ * restrict vector, unsigned mode, void *dct)
{
   char quote = ' ';
   char buf[GMS_SSSIZE];
   for (unsigned i = 0, len = vector->len; i < len; ++i) {
      int idx = vector->vis[i];
      dctUelLabel(dct, idx, &quote, buf, sizeof(buf));
      printout(mode, "[%5d] %e %s\n", idx, vector->coeffs[i], buf);
   }
}
