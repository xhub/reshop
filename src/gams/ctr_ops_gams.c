#include <stdio.h>
#include <stdlib.h>
#include <string.h>

#include "container.h"
#include "container_ops.h"
#include "ctrdat_gams.h"
#include "equ.h"
#include "equvar_helpers.h"
#include "gams_macros.h"
#include "gams_option.h"
#include "gams_rosetta.h"
#include "macros.h"
#include "printout.h"
#include "status.h"
#include "tlsdef.h"
#include "var.h"

tlsvar char gams_bufvar[GMS_SSSIZE];
tlsvar char gams_bufequ[GMS_SSSIZE];
tlsvar char gams_errbuf[GMS_SSSIZE];

/* ------------------------------------------------------------------------
 * Locally defined functions.
 * ------------------------------------------------------------------------ */
static int gams_evalfunc(const Container *ctr, int si, double *x,
                         double *f, int *numerr);
static int gams_evalgrad(const Container *ctr, int si, double *x,
                         double *f, double *g, double *gx, int *numerr);
static int gams_evalgradobj(const Container *ctr, double *x, double *f,
                            double *g, double *gx, int *numerr);
static int gams_getequval(const Container *ctr, rhp_idx ei, double *value);
static int gams_getequmult(const Container *ctr, rhp_idx ei, double *mult);
static int gams_getequperp(const Container *ctr, rhp_idx ei, int *match);
static int gams_getequbasis(const Container *ctr, rhp_idx ei, int *basis);
static int gams_getcoljacinfo(const Container *ctr, int colidx,
                              void **jacptr, double *jacval, int *rowidx,
                              int *nlflag);
static int gams_getrowjacinfo(const Container *ctr, int rowidx,
                              void **jacptr, double *jacval, int *colidx,
                              int *nlflag);
static int gams_getnnz(const Container *ctr, int *nnz);
static int gams_getvarval(const Container *ctr, rhp_idx vi, double *level);
static int gams_getvarmult(const Container *ctr, rhp_idx vi, double *mult);
static int gams_getvarperp(const Container *ctr, rhp_idx vi, int *match);
static int gams_getvarbasis(const Container *ctr, rhp_idx vi, int *basis);
static int gams_setequbasis(Container *ctr, rhp_idx ei, int basis);
static int gams_setvarval(Container *ctr, rhp_idx vi, double val);
static int gams_setvarmult(Container *ctr, rhp_idx vi, double mult);
static int gams_setvarbasis(Container *ctr, rhp_idx vi, int basis);

static int gams_notimpl3(Container *ctr, rhp_idx ei, double val)
{
   errormsg("Functionality not implemented/supported\n");
   return Error_NotImplemented;
}

static int gams_notimpl3char(const Container *ctr, const char *name, int *vi)
{
   errormsg("Functionality not implemented/supported\n");
   return Error_NotImplemented;
}

static int gams_allocdata(Container *ctr)
{
   CALLOC_(ctr->data, struct ctrdata_gams, 1);

   return OK;
}

static void gams_deallocdata(Container *ctr)
{
   struct ctrdata_gams *gms = ctr->data;

   if (!gms) { return; }

   if (!gms->initialized) {
      FREE(gms);
      ctr->data = NULL;
      return;
   }

   if (gms->owning_handles) {
      gcdat_rel(gms);
   }


   FREE(gms->sos_group);
   FREE(gms->rhsdelta);
   FREE(gms->equvar_eval.pairs);
   FREE(gms);

   ctr->data = NULL;
}

static int gams_equvarcounts(Container *ctr)
{
   int status = OK;
   const GmsContainerData *gms = ctr->data;
   gmoHandle_t gmo = gms->gmo;

   enum gmoVarType vtypes[] = {
      gmovar_X ,
      gmovar_B ,
      gmovar_I ,
      gmovar_S1,
      gmovar_S2,
      gmovar_SC,
      gmovar_SI
   };

   enum gmoEquType etypes[] = {
      gmoequ_E,
      gmoequ_G,
      gmoequ_L,
      gmoequ_N,
      gmoequ_X,
      gmoequ_C,
      gmoequ_B,
   };

   int vtypes_cnt[ARRAY_SIZE(vtypes)] = { 0 };
   int etypes_cnt[ARRAY_SIZE(etypes)] = { 0 };



   for (unsigned i = 0, len = ARRAY_SIZE(vtypes); i < len; ++i) {
      vtypes_cnt[i] = gmoGetVarTypeCnt(gmo, vtypes[i]);
   }

   for (unsigned i = 0, len = ARRAY_SIZE(etypes); i < len; ++i) {
      etypes_cnt[i] = gmoGetEquTypeCnt(gmo, etypes[i]);
   }

   enum gmoEquOrder equorders[] = {
      gmoorder_ERR, gmoorder_L, gmoorder_Q, gmoorder_NL
   };

   unsigned equorders_cnt[ARRAY_SIZE(equorders)] = { 0 };

   for (int i = 0, len = gmoM(gmo); i < len; ++i) {
      int idx = gmoGetEquOrderOne(gmo, i);

      if (idx < 0 || idx >= ARRAY_SIZE(equorders)) {
         error("[gams] ERROR: equation '%s' has unsupported order %d",
               ctr_printequname(ctr, i), idx);
         status = Error_GamsCallFailed;
      }

      equorders_cnt[idx]++;
   }

   if (equorders_cnt[gmoorder_ERR] > 0) {
      error("[gams] ERROR: got %d errors from gmoGetEquOrderOne()\n",
            equorders_cnt[gmoorder_ERR]);
   }

   if (etypes_cnt[gmoequ_X] > 0) {
      TO_IMPLEMENT("GAMS/GMO model with =X= equations");
   }

   if (etypes_cnt[gmoequ_C] > 0) {
      TO_IMPLEMENT("GAMS/GMO model with =C= equations");
   }

   if (etypes_cnt[gmoequ_B] > 0) {
      TO_IMPLEMENT("GAMS/GMO model with =B= equations");
   }

  /* ----------------------------------------------------------------------
   * Report values in container
   * ---------------------------------------------------------------------- */

   EquVarTypeCounts *equvarstats = &ctr->equvarstats;

   equvarstats->vartypes[VAR_X]    = vtypes_cnt[gmovar_X];
   equvarstats->vartypes[VAR_B]    = vtypes_cnt[gmovar_B];
   equvarstats->vartypes[VAR_I]    = vtypes_cnt[gmovar_I];
   equvarstats->vartypes[VAR_SOS1] = vtypes_cnt[gmovar_S1];
   equvarstats->vartypes[VAR_SOS2] = vtypes_cnt[gmovar_S2];
   equvarstats->vartypes[VAR_SC]   = vtypes_cnt[gmovar_SC];
   equvarstats->vartypes[VAR_SI]   = vtypes_cnt[gmovar_SI];

   equvarstats->equs.cones[CONE_0]       = etypes_cnt[gmoequ_E];
   equvarstats->equs.cones[CONE_R_PLUS]  = etypes_cnt[gmoequ_G];
   equvarstats->equs.cones[CONE_R_MINUS] = etypes_cnt[gmoequ_L];

   equvarstats->equs.types[ConeInclusion]        = etypes_cnt[gmoequ_E] + etypes_cnt[gmoequ_G] + etypes_cnt[gmoequ_L];

   equvarstats->equs.exprtypes[EquExprLinear]    = equorders_cnt[gmoorder_L];
   equvarstats->equs.exprtypes[EquExprQuadratic] = equorders_cnt[gmoorder_Q];
   equvarstats->equs.exprtypes[EquExprNonLinear] = equorders_cnt[gmoorder_NL];

   return status;
}

static int gams_evalequvar(Container *ctr)
{
   struct ctrdata_gams *gms = ctr->data;

   if (gms->equvar_eval.len == 0) { return OK; }

   REALLOC_(gms->rhsdelta, double, ctr->n);
   double * restrict x = gms->rhsdelta;

   gmoGetVarL(gms->gmo, x);

   for (size_t i = 0; i < gms->equvar_eval.len; ++i) {
      rhp_idx vi = gms->equvar_eval.pairs[i].var;
      rhp_idx ei = gms->equvar_eval.pairs[i].equ;

      void *ptr;
      rhp_idx ei_gmo;
      int nlflag;
      double coeff;
      S_CHECK(gams_getcoljacinfo(ctr, ei, &ptr, &coeff, &ei_gmo, &nlflag));

      if (ei != ei_gmo-1) {
         error("%s :: implementation only supports variable appearing in 1 "
               "equation, mostly objective variables. Here the equation was "
               "'%s' #%u, but the variable '%s' #%u also appears in '%s' #%u\n",
               __func__, ctr_printequname(ctr, ei), ei,
               ctr_printvarname(ctr, vi), vi,
               ctr_printequname(ctr, ei_gmo-1), ei_gmo-1);
         return Error_RuntimeError;
      }

      x[vi] = 0.;
      double fval;
      int dummyint;
      S_CHECK(gams_evalfunc(ctr, ei, x, &fval, &dummyint));
      double vval = fval/coeff;
      x[vi] = vval;
      gmoSetVarLOne(gms->gmo, vi, vval);
   }

   return OK;
}

static int gams_evalfunc(const Container *ctr, rhp_idx ei, double *x,
                         double *f, int *numerr)
{
   const struct ctrdata_gams *gms = ctr->data;

   gmoEvalFunc(gms->gmo, ei, x, f, numerr);

   if ((*numerr) > 0) {
      return Error_EvaluationFailed;
   }

   return OK;
}

static int gams_evalgrad(const Container *ctr, int si, double *x,
                         double *f, double *g, double *gx, int *numerr)
{
   const struct ctrdata_gams *gms = ctr->data;

   gmoEvalGrad(gms->gmo, si, x, f, g, gx, numerr);

   if ((*numerr) > 0) {
      return Error_EvaluationFailed;
   }

   return OK;
}

static int gams_evalgradobj(const Container *ctr, double *x, double *f,
                            double *g, double *gx, int *numerr)
{
   const struct ctrdata_gams *gms;

   gms = ctr->data;

   gmoEvalGradObj(gms->gmo, x, f, g, gx, numerr);

   if ((*numerr) > 0) {
      return Error_EvaluationFailed;
   }

   return OK;
}

static int gams_getaequbasis(const Container *ctr, Aequ *v, int *basis)
{
   assert(v && basis);
   for (size_t i = 0; i < v->size; ++i) {
      rhp_idx ei = aequ_fget(v, i);
      assert(valid_ei(ei));
      S_CHECK(gams_getequbasis(ctr, ei, &basis[i]));
   }
   return OK;
}

static int gams_getaequmult(const Container *ctr, Aequ *v, double *mult)
{
   assert(v && mult);
   for (size_t i = 0; i < v->size; ++i) {
      rhp_idx ei = aequ_fget(v, i);
      assert(valid_ei(ei));
      S_CHECK(gams_getequmult(ctr, ei, &mult[i]));
   }
   return OK;
}

static int gams_getaequval(const Container *ctr, Aequ *v, double *vals)
{
   assert(v && vals);
   for (size_t i = 0; i < v->size; ++i) {
      rhp_idx ei = aequ_fget(v, i);
      assert(valid_ei(ei));
      gams_getequval(ctr, ei, &vals[i]);
   }
   return OK;
}

static int gams_getavarbasis(const Container *ctr, Avar *v, int *basis)
{
   assert(v && basis);
   for (size_t i = 0; i < v->size; ++i) {
      rhp_idx vi = avar_fget(v, i);
      assert(valid_vi(vi));
      S_CHECK(gams_getvarbasis(ctr, vi, &basis[i]));
   }
   return OK;
}

static int gams_getavarmult(const Container *ctr, Avar *v, double *mult)
{
   assert(v && mult);
   for (size_t i = 0; i < v->size; ++i) {
      rhp_idx vi = avar_fget(v, i);
      assert(valid_vi(vi));
      S_CHECK(gams_getvarmult(ctr, vi, &mult[i]));
   }
   return OK;
}

static int gams_getavarval(const Container *ctr, Avar *v, double *vals)
{
   assert(v && vals);
   for (size_t i = 0; i < v->size; ++i) {
      rhp_idx vi = avar_fget(v, i);
      assert(valid_vi(vi));
      gams_getvarval(ctr, vi, &vals[i]);
   }
   return OK;
}

static int gams_getequval(const Container *ctr, rhp_idx ei, double *value)
{
   assert(ctr->backend == RHP_BACKEND_GAMS_GMO);
   gmoHandle_t gmo = ((struct ctrdata_gams *) ctr->data)->gmo;
   assert(gmo);

   S_CHECK(ei_inbounds(ei, gmoM(gmo), __func__));

   double gms_pinf = gmoPinf(gmo), gms_minf = gmoMinf(gmo), gms_na = gmoValNA(gmo);

   double gms_val = gmoGetEquLOne(gmo, ei);
   (*value) = dbl_from_gams(gms_val, gms_pinf, gms_minf, gms_na) - gmoGetRhsOne(gmo, ei);

   return OK;
}

static int gams_getequmult(const Container *ctr, rhp_idx ei, double *mult)
{
   assert(ctr->backend == RHP_BACKEND_GAMS_GMO);
   gmoHandle_t gmo = ((struct ctrdata_gams *) ctr->data)->gmo;
   assert(gmo);

   S_CHECK(ei_inbounds(ei, gmoM(gmo), __func__));

   double gms_pinf = gmoPinf(gmo), gms_minf = gmoMinf(gmo), gms_na = gmoValNA(gmo);

   double gms_val = gmoGetEquMOne(gmo, ei);
   bool flip = gmoSense(gmo) == gmoObj_Max;
   
   double marginal = dbl_from_gams(gms_val, gms_pinf, gms_minf, gms_na);
   (*mult) = flip ? -marginal : marginal;

   return OK;
}

static int gams_getequperp(const Container *ctr, rhp_idx ei, int *match)
{
   assert(ctr->backend == RHP_BACKEND_GAMS_GMO);
   gmoHandle_t gmo = ((struct ctrdata_gams *) ctr->data)->gmo;
   assert(gmo);
   S_CHECK(ei_inbounds(ei, gmoM(gmo), __func__));

   (*match) = gmoGetEquMatchOne(gmo, ei);
   return OK;
}


static int gams_getequbasis(const Container *ctr, rhp_idx ei, int *basis)
{
   assert(ctr->backend == RHP_BACKEND_GAMS_GMO);
   gmoHandle_t gmo = ((struct ctrdata_gams *) ctr->data)->gmo;
   assert(gmo);
   S_CHECK(ei_inbounds(ei, gmoM(gmo), __func__));

   (*basis) = basis_from_gams(gmoGetEquStatOne(gmo, ei));
   return OK;
}

static int gams_getequsmult(const Container *ctr, double *mult)
{
   assert(ctr->backend == RHP_BACKEND_GAMS_GMO);

   gmoHandle_t gmo = ((struct ctrdata_gams *) ctr->data)->gmo;
   assert(gmo);

   double gms_pinf = gmoPinf(gmo), gms_minf = gmoMinf(gmo), gms_na = gmoValNA(gmo);

   GMSCHK(gmoGetEquM(gmo, mult));

   bool flip = gmoSense(gmo) == gmoObj_Max;

   if (flip) {
      for (int i = 0, len = gmoM(gmo); i < len; ++i) {
         mult[i] = -dbl_from_gams(mult[i], gms_pinf, gms_minf, gms_na); 
      }
      
   } else {
      for (int i = 0, len = gmoM(gmo); i < len; ++i) {
         mult[i] = dbl_from_gams(mult[i], gms_pinf, gms_minf, gms_na); 
      }
   }

   return OK;
}
static int gams_getequsval(const Container *ctr, double *vals)
{
   assert(ctr->backend == RHP_BACKEND_GAMS_GMO);
   gmoHandle_t gmo = ((struct ctrdata_gams *) ctr->data)->gmo;
   assert(gmo);

   double gms_pinf = gmoPinf(gmo), gms_minf = gmoMinf(gmo), gms_na = gmoValNA(gmo);

   GMSCHK(gmoGetEquL(gmo, vals));

   for (int i = 0, len = gmoM(gmo); i < len; ++i) {
      vals[i] = dbl_from_gams(vals[i], gms_pinf, gms_minf, gms_na) - gmoGetRhsOne(gmo, i); 
   }

   return OK;
}

static int gams_getcoljacinfo(const Container *ctr, rhp_idx vi,
                              void **jacptr, double *jacval, rhp_idx *ei,
                              int *nlflag)
{
   const struct ctrdata_gams *gms = ctr->data;

   int colidx = vi;
   int rowidx;
   gmoGetColJacInfoOne(gms->gmo, colidx, jacptr, jacval, &rowidx, nlflag);

   *ei = rowidx < 0 ? IdxNA : rowidx;

   return OK;
}

static int gams_getrowjacinfo(const Container *ctr, rhp_idx ei,
                              void **jacptr, double *jacval, rhp_idx *vi,
                              int *nlflag)
{
   const struct ctrdata_gams *gms;

   gms = ctr->data;

   int rowidx = ei;
   int colidx;
   gmoGetRowJacInfoOne(gms->gmo, rowidx, jacptr, jacval, &colidx, nlflag);

   *vi = colidx < 0 ? IdxNA : colidx;

   return OK;
}

static int gams_copyvarname(const Container *ctr, rhp_idx vi, char *name,
                           unsigned len)
{
   const struct ctrdata_gams *gms;
   char quote, sname[GMS_SSSIZE];
   int sindex, sdim, suels[GMS_MAX_INDEX_DIM];

   gms = ctr->data;
   if (!gms->dct) {
      error("%s :: no dictionary in the gms object!\n", __func__);
      return Error_NullPointer;
   }
   unsigned i = 0;

   /* ------------------------------------------------------------------
    * Get dictionary information about variable vi
    * ------------------------------------------------------------------ */

   if (dctColUels(gms->dct, vi, &sindex, suels, &sdim)) {
      name[i] = '\0';
      error("%s :: call to dctColUels() failed\n", __func__);
      return Error_GamsCallFailed;
   }

   /* ------------------------------------------------------------------
    * Get column i's name.
    * ------------------------------------------------------------------ */

   if (dctSymName(gms->dct, sindex, sname, sizeof(sname))) {
      name[i] = '\0';
      error("%s :: call to dctSymName failed\n", __func__);
      return Error_GamsCallFailed;
   }

   while (i < (len - 1) && sname[i] != '\0') {
      name[i] = sname[i];
      i++;
   }

   if (i >= (len - 1) && sname[i] != '\0') {
      name[i] = '\0';
      return Error_SizeTooSmall;
   }

   if (sdim == 0) {
      name[i] = '\0';
      return OK;
   }

   /* ------------------------------------------------------------------
    * Get the name of each element.
    * ------------------------------------------------------------------ */

   name[i] = '(';
   i++;

   if (i >= (len - 1)) {
      name[i] = '\0';
      return Error_SizeTooSmall;
   }

   for (size_t dim = 0; dim < (size_t)sdim; dim++) {
      if (dctUelLabel(gms->dct, suels[dim], &quote, sname, sizeof(sname))) {
         name[i] = '\0';
         return Error_InvalidValue;
      }

      if (' ' != quote) {
         name[i] = quote;
         i++;

         if (i >= (len - 1)) {
            name[i] = '\0';
            return Error_SizeTooSmall;
         }
      }

      unsigned k = 0;
      while (i < (len - 1) && sname[k] != '\0') {
         name[i] = sname[k];
         i++;
         k++;
      }

      if (i >= (len - 1)) {
            name[i] = '\0';
            return Error_SizeTooSmall;
      }

      if (' ' != quote) {
         name[i] = quote;
         i++;

         if (i >= (len - 1)) {
            name[i] = '\0';
            return Error_SizeTooSmall;
         }
      }

      name[i] = ',';
      i++;
      if (i >= (len - 1)) {
         name[i] = '\0';
         return Error_SizeTooSmall;
      }
   }

   name[i-1] = ')';
   name[i] = '\0';

   return OK;
}

static int gams_copyequname(const Container *ctr, rhp_idx ei, char *name,
                            unsigned len)
{
   const struct ctrdata_gams *gms;
   char quote, sname[GMS_SSSIZE];

   gms = ctr->data;
   if (!gms->dct) {
      error("%s :: no dictionary in the gms object!\n", __func__);
      return Error_NullPointer;
   }
   unsigned j = 0;

   if (ei >= ctr->m) {
      error("%s :: the requested equation index %u is larger than the total "
            "number of equations %u.\n", __func__, ei, ctr->m);
      return Error_IndexOutOfRange;
   }

   /* ------------------------------------------------------------------
    * Get dictionary information about equation ei
    * ------------------------------------------------------------------ */

   int sindex, sdim, suels[GMS_MAX_INDEX_DIM];
   if (dctRowUels(gms->dct, ei, &sindex, suels, &sdim)) {
      error("%s :: call to dctRowUels() failed with index %d\n", __func__, ei);
      name[j] = '\0';
      return Error_GamsCallFailed;
   }

   /* ------------------------------------------------------------------
    * Get the name of equation ei
    * ------------------------------------------------------------------ */

   if (dctSymName(gms->dct, sindex, sname, sizeof(sname))) {
      error("%s :: call to dctSymName failed with index %d\n",
                         __func__, sindex);
      name[j] = '\0';
      return Error_GamsCallFailed;
   }

   STRNCPY(name, sname, len);
   while (j < (len - 1) && sname[j] != '\0') {
      name[j] = sname[j];
      j++;
   }

   if (j >= (len - 1) && sname[j] != '\0') {
      name[j] = '\0';
      return Error_SizeTooSmall;
   }

   if (sdim == 0) {
      name[j] = '\0';
      return OK;
   }

   /* ------------------------------------------------------------------
    * Get the name of each element.
    * ------------------------------------------------------------------ */

   name[j] = '(';
   j++;

   if (j >= (len - 1)) {
      name[j] = '\0';
      return Error_SizeTooSmall;
   }

   for (size_t dim = 0; dim < (size_t)sdim; dim++) {
      if (dctUelLabel(gms->dct, suels[dim], &quote, sname, sizeof(sname))) {
         error("%s :: call to dctUelLabel failed with index %d\n",
                         __func__, suels[dim]);
         name[j] = '\0';
         return Error_InvalidValue;
      }

      if (' ' != quote) {
         name[j] = quote;
         j++;

         if (j >= (len - 1)) {
            name[j] = '\0';
            return Error_SizeTooSmall;
         }
      }

      unsigned k = 0;
      while (j < (len - 1) && sname[k] != '\0') {
         name[j] = sname[k];
         j++;
         k++;
      }

      if (j >= (len - 1)) {
            name[j] = '\0';
            return Error_SizeTooSmall;
      }

      if (' ' != quote) {
         name[j] = quote;
         j++;

         if (j >= (len - 1)) {
            name[j] = '\0';
            return Error_SizeTooSmall;
         }
      }

      name[j] = ',';
      j++;
      if (j >= (len - 1)) {
         name[j] = '\0';
         return Error_SizeTooSmall;
      }
   }

   name[j-1] = ')';
   name[j] = '\0';

   return OK;
}


UNUSED static int gams_getnnz(const Container *ctr, int *nnz)
{
   (*nnz) = gmoNZ(((const struct ctrdata_gams *) ctr->data)->gmo);
   return OK;
}

static int gams_getcst(const Container *ctr, rhp_idx eidx, double *val)
{
   assert(ctr->backend == RHP_BACKEND_GAMS_GMO);
   *val = -gmoGetRhsOne(((const struct ctrdata_gams *) ctr->data)->gmo, eidx);
   return OK;
}


static int gams_getspecialfloats(const Container *ctr, double *minf, double *pinf, double* na)
{
   assert(ctr->backend == RHP_BACKEND_GAMS_GMO);
   gmoHandle_t gmo = ((const struct ctrdata_gams *) ctr->data)->gmo;
   (*pinf) = gmoPinf(gmo);
   (*minf) = gmoMinf(gmo);
   (*na)  = gmoValNA(gmo);

   return OK;
}

static int gams_getvarbounds(const Container *ctr, rhp_idx vi, double *lb, double *ub)
{
   assert(ctr->backend == RHP_BACKEND_GAMS_GMO);
   gmoHandle_t gmo = ((struct ctrdata_gams *) ctr->data)->gmo;
   assert(gmo);

   double gms_pinf = gmoPinf(gmo), gms_minf = gmoMinf(gmo), gms_na = gmoValNA(gmo);

   S_CHECK(vi_inbounds(vi, gmoN(gmo), __func__));

   double gms_lo = gmoGetVarLowerOne(gmo, vi);
   double gms_up = gmoGetVarUpperOne(gmo, vi);

   (*lb) = dbl_from_gams(gms_lo, gms_pinf, gms_minf, gms_na);
   (*ub) = dbl_from_gams(gms_up, gms_pinf, gms_minf, gms_na);

   return OK;
}

static int gams_getvarlb(const Container *ctr, rhp_idx vi, double *lb)
{
   assert(ctr->backend == RHP_BACKEND_GAMS_GMO);
   gmoHandle_t gmo = ((struct ctrdata_gams *) ctr->data)->gmo;
   assert(gmo);

   double gms_pinf = gmoPinf(gmo), gms_minf = gmoMinf(gmo), gms_na = gmoValNA(gmo);

   S_CHECK(vi_inbounds(vi, gmoN(gmo), __func__));

   double gms_lo = gmoGetVarLowerOne(gmo, vi);

   (*lb) = dbl_from_gams(gms_lo, gms_pinf, gms_minf, gms_na);

   return OK;
}

static int gams_getvarub(const Container *ctr, rhp_idx vi, double *ub)
{
   assert(ctr->backend == RHP_BACKEND_GAMS_GMO);
   gmoHandle_t gmo = ((struct ctrdata_gams *) ctr->data)->gmo;
   assert(gmo);

   double gms_pinf = gmoPinf(gmo), gms_minf = gmoMinf(gmo), gms_na = gmoValNA(gmo);

   S_CHECK(vi_inbounds(vi, gmoN(gmo), __func__));

   double gms_up = gmoGetVarUpperOne(gmo, vi);

   (*ub) = dbl_from_gams(gms_up, gms_pinf, gms_minf, gms_na);

   return OK;
}

static int gams_getvarval(const Container *ctr, rhp_idx vi, double *level)
{
   assert(ctr->backend == RHP_BACKEND_GAMS_GMO);
   gmoHandle_t gmo = ((struct ctrdata_gams *) ctr->data)->gmo;
   assert(gmo);

   double gms_pinf = gmoPinf(gmo), gms_minf = gmoMinf(gmo), gms_na = gmoValNA(gmo);


   S_CHECK(vi_inbounds(vi, gmoN(gmo), __func__));

   double gmo_val = gmoGetVarLOne(gmo, vi);
   (*level) = dbl_from_gams(gmo_val, gms_pinf, gms_minf, gms_na);

   return OK;
}

static int gams_getvarmult(const Container *ctr, rhp_idx vi, double *multiplier)
{
   assert(ctr->backend == RHP_BACKEND_GAMS_GMO);
   gmoHandle_t gmo = ((struct ctrdata_gams *) ctr->data)->gmo;
   assert(gmo);

   double gms_pinf = gmoPinf(gmo), gms_minf = gmoMinf(gmo), gms_na = gmoValNA(gmo);

   S_CHECK(vi_inbounds(vi, gmoN(gmo), __func__));

   double gmo_val = gmoGetVarMOne(gmo, vi);
   bool flip = gmoSense(gmo) == gmoObj_Max;

   double marginal = dbl_from_gams(gmo_val, gms_pinf, gms_minf, gms_na);
   (*multiplier) = flip ? -marginal : marginal;

   return OK;
}

static int gams_getvarperp(const Container *ctr, rhp_idx vi, int *match)
{
   assert(ctr->backend == RHP_BACKEND_GAMS_GMO);
   gmoHandle_t gmo = ((struct ctrdata_gams *) ctr->data)->gmo;
   assert(gmo);
   S_CHECK(vi_inbounds(vi, gmoN(gmo), __func__));

   (*match) = gmoGetVarMatchOne(((struct ctrdata_gams *) ctr->data)->gmo, vi);
   return OK;
}

static int gams_getvarbasis(const Container *ctr, rhp_idx vi, int *basis)
{
   assert(ctr->backend == RHP_BACKEND_GAMS_GMO);
   gmoHandle_t gmo = ((struct ctrdata_gams *) ctr->data)->gmo;
   assert(gmo);
   S_CHECK(vi_inbounds(vi, gmoN(gmo), __func__));

   (*basis) = basis_from_gams(gmoGetVarStatOne(gmo, vi));
   return OK;
}

UNUSED static int gams_getvartype(const Container *ctr, rhp_idx vi, int *type)
{
   assert(ctr->backend == RHP_BACKEND_GAMS_GMO);
   gmoHandle_t gmo = ((struct ctrdata_gams *) ctr->data)->gmo;
   assert(gmo);
   S_CHECK(vi_inbounds(vi, gmoN(gmo), __func__));

   (*type) = gmoGetVarTypeOne(((struct ctrdata_gams *) ctr->data)->gmo, vi);
   return OK;
}

static int gams_getvarsmult(const Container *ctr, double *mult)
{
   assert(ctr->backend == RHP_BACKEND_GAMS_GMO);

   gmoHandle_t gmo = ((struct ctrdata_gams *) ctr->data)->gmo;
   assert(gmo);

   double gms_pinf = gmoPinf(gmo), gms_minf = gmoMinf(gmo), gms_na = gmoValNA(gmo);

   GMSCHK(gmoGetVarM(gmo, mult));

   bool flip = gmoSense(gmo) == gmoObj_Max;

   if (flip) {
      for (int i = 0, len = gmoN(gmo); i < len; ++i) {
         mult[i] = -dbl_from_gams(mult[i], gms_pinf, gms_minf, gms_na); 
      }

   } else {
      for (int i = 0, len = gmoN(gmo); i < len; ++i) {
         mult[i] = dbl_from_gams(mult[i], gms_pinf, gms_minf, gms_na); 
      }
   }

   return OK;
}

static int gams_getvarsval(const Container *ctr, double *vals)
{
   assert(ctr->backend == RHP_BACKEND_GAMS_GMO);

   gmoHandle_t gmo = ((struct ctrdata_gams *) ctr->data)->gmo;
   assert(gmo);

   double gms_pinf = gmoPinf(gmo), gms_minf = gmoMinf(gmo), gms_na = gmoValNA(gmo);

   GMSCHK(gmoGetVarL(gmo, vals));

   for (int i = 0, len = gmoN(gmo); i < len; ++i) {
      vals[i] = dbl_from_gams(vals[i], gms_pinf, gms_minf, gms_na); 
   }

   return OK;
}

static int gams_resize(Container *ctr, unsigned n, unsigned m)
{
   /*  we could call gmoinitdata here? */
   return OK;
}

static int gams_setequval(Container *ctr, rhp_idx ei, double level)
{
   assert(ctr->backend == RHP_BACKEND_GAMS_GMO);
   gmoHandle_t gmo = ((struct ctrdata_gams *) ctr->data)->gmo;
   assert(gmo);

   double gms_pinf = gmoPinf(gmo), gms_minf = gmoMinf(gmo), gms_na = gmoValNA(gmo);

   S_CHECK(vi_inbounds(ei, gmoM(gmo), __func__));

   double gms_val = dbl_to_gams(level + gmoGetRhsOne(gmo, ei), gms_pinf, gms_minf, gms_na);
   gmoSetEquLOne(gmo, ei, gms_val);
   return OK;
}

static int gams_setequname(Container *ctr, rhp_idx ei, const char *name)
{
   error("%s :: GAMS model does not support setting an equation name"
                      " after its creation.\n", __func__);

   return Error_UnsupportedOperation;
}

static int gams_setequmult(Container *ctr, rhp_idx ei, double mult)
{
   return Error_NotImplemented;
}

static int gams_setequbasis(Container *ctr, rhp_idx ei, int basis)
{
   return Error_NotImplemented;
}

static int gams_setequtype(Container *ctr, rhp_idx eidx, unsigned type, unsigned cone)
{
   return Error_NotImplemented;
}

static int gams_setequvarperp(Container *ctr, rhp_idx ei, rhp_idx vi)
{
   return Error_NotImplemented;
}

static int gams_getequtype(const Container *ctr, rhp_idx eidx, unsigned *type, unsigned *cone)
{
   return Error_NotImplemented;
}

static int gams_getequexprtype(const Container *ctr, rhp_idx ei, EquExprType *type)
{
   assert(ctr->backend == RHP_BACKEND_GAMS_GMO);
   gmoHandle_t gmo = ((struct ctrdata_gams *) ctr->data)->gmo;
   assert(gmo);

   int gmsequexprtype = gmoGetEquOrderOne(gmo, ei);


   switch (gmsequexprtype) {
   /*  TODO(xhub) support quadratic expression */
   case gmoorder_Q:
      *type = EquExprQuadratic;
      break;
   case gmoorder_NL:
      *type = EquExprNonLinear;
      break;
   case gmoorder_L:
      *type = EquExprLinear;
      break;
   case gmoorder_ERR:
      error("%s ERROR: while probing for the type of expression in equation '%s'\n",
            __func__, ctr_printequname(ctr, ei));
      return Error_GamsCallFailed;

   default:
      error("%s ERROR: unsupported value %d from gmoGetEquOrderOne() when probing "
            "for the type of expression in equation '%s'\n", __func__, gmsequexprtype,
            ctr_printequname(ctr, ei));
      return Error_GamsCallFailed;
   }

   return OK;
}

static int gams_setcst(Container *ctr, rhp_idx ei, double val)
{
   error("%s :: Not implemented\n", __func__);
   return Error_NotImplemented;
}


static int gams_setvarval(Container *ctr, rhp_idx vi, double varl)
{
   if (vi < ctr->n) {
      ctr->vars[vi].value = varl;
      gmoSetVarLOne(((struct ctrdata_gams *) ctr->data)->gmo, vi, varl);
      return OK;
   }

   return Error_IndexOutOfRange;
}

UNUSED static int gams_setvarname(Container *ctr, rhp_idx vi, const char *name)
{
   error("%s :: GAMS model does not support setting a variable name"
                      " after its creation.\n", __func__);

   return Error_UnsupportedOperation;
}

static int gams_setvarmult(Container *ctr, rhp_idx vi, double varm)
{
   if (vi < ctr->n) {
      ctr->vars[vi].multiplier = varm;
      gmoSetVarMOne(((struct ctrdata_gams *) ctr->data)->gmo, vi, varm);
      return OK;
   }

   return Error_IndexOutOfRange;
}

static int gams_setvarbasis(Container *ctr, rhp_idx vi, int basis)
{
   assert(ctr->backend == RHP_BACKEND_GAMS_GMO);
   gmoHandle_t gmo = ((struct ctrdata_gams *) ctr->data)->gmo;
   assert(gmo);

   S_CHECK(vi_inbounds(vi, gmoN(gmo), __func__));

   gmoSetVarStatOne(gmo, vi, basis_to_gams(basis));
   return OK;
}

static int gams_setvartype(Container *ctr, rhp_idx vi, unsigned type)
{
   error("%s :: GAMS does not support setting variable types\n", __func__);
   return Error_NotImplemented;
}

const struct container_ops ctr_ops_gams = {
   .allocdata      = gams_allocdata,
   .deallocdata    = gams_deallocdata,
   .copyequname    = gams_copyequname,
   .copyvarname    = gams_copyvarname,
   .equvarcounts   = gams_equvarcounts,
   .evalequvar     = gams_evalequvar,
   .evalfunc       = gams_evalfunc,
   .evalgrad       = gams_evalgrad,
   .evalgradobj    = gams_evalgradobj,
   .getequsbasis   = gams_getaequbasis,
   .getequsmult    = gams_getaequmult,
   .getequsval     = gams_getaequval,
   .getvarsbasis   = gams_getavarbasis,
   .getvarsmult    = gams_getavarmult,
   .getvarsval     = gams_getavarval,
   .getequcst      = gams_getcst,
   .getequbyname   = gams_notimpl3char,
   .getequval      = gams_getequval,
   .getequmult     = gams_getequmult,
   .getequperp     = gams_getequperp,
   .getallequsmult = gams_getequsmult,
   .getequbasis    = gams_getequbasis,
   .getallequsval  = gams_getequsval,
   .getequtype     = gams_getequtype,
   .getequexprtype = gams_getequexprtype,
   .getcoljacinfo  = gams_getcoljacinfo, /* RENAME */
   .getrowjacinfo  = gams_getrowjacinfo, /* RENAME */
   .getspecialfloats = gams_getspecialfloats,
   .getvarbounds   = gams_getvarbounds,
   .getvarbyname   = gams_notimpl3char,
   .getvarlb       = gams_getvarlb,
   .getvarval      = gams_getvarval,
   .getvarmult     = gams_getvarmult,
   .getvarperp     = gams_getvarperp,
   .getallvarsmult = gams_getvarsmult,
   .getvarbasis    = gams_getvarbasis,
   .getvarub       = gams_getvarub,
   .getallvarsval  = gams_getvarsval,
   .resize         = gams_resize,
   .setequval      = gams_setequval,
   .setequname     = gams_setequname,
   .setequmult     = gams_setequmult,
   .setequbasis    = gams_setequbasis,
   .setequtype     = gams_setequtype,
   .setequvarperp  = gams_setequvarperp,
   .setequcst      = gams_setcst,
   .setvarlb       = gams_notimpl3,
   .setvarval      = gams_setvarval,
   .setvarmult     = gams_setvarmult,
   .setvarbasis    = gams_setvarbasis,
   .setvartype     = gams_setvartype,
   .setvarub       = gams_notimpl3,
};
