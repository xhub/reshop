#include <float.h>
#include <fcntl.h>
#include <sys/stat.h>
#include <sys/types.h>


#include "asnan.h"
#include "cmat.h"
#include "container.h"
#include "ctr_gams.h"
#include "ctr_rhp.h"
#include "ctrdat_gams.h"
#include "equvar_helpers.h"
#include "equvar_metadata.h"
#include "filter_ops.h"
#include "gams_macros.h"
#include "gams_rosetta.h"
#include "gams_utils.h"
#include "instr.h"
#include "lequ.h"
#include "macros.h"
#include "mathprgm.h"
#include "mdl.h"
#include "mdl_gams.h"
#include "ctrdat_rhp.h"
#include "mdl_rhp.h"
#include "rhp_alg.h"
#include "rmdl_gams.h"
#include "rmdl_options.h"
#include "pool.h"
#include "printout.h"
#include "status.h"
#include "timings.h"
#include "toplayer_utils.h"

#include "cfgmcc.h"
#include "dctmcc.h"
#include "gevmcc.h"
#include "gmomcc.h"

// compute the exponent padding
#define NUMANDEPAD(n) (n), (fabs(n)>=1e100 ? 1 : 2)

#ifndef NDEBUG
#include "nltree_priv.h"
#endif


UNUSED static int _debug_check_nlcode(int opcode, int value, size_t nvars, size_t poolen)
{
#ifndef NDEBUG
   switch (gams_get_optype(opcode)) {
   case NLNODE_OPARG_CST:
   case NLNODE_OPARG_FMA:
      if ((size_t)value > poolen) {
         error("%s :: constant value %d outside of the pool range %zu\n",
               __func__, value, poolen);
         return Error_InvalidValue;
      } else if (value == 0 && opcode != nlPushZero) {
         error("%s :: constant value is 0. It should be positive\n", __func__);
         return Error_InvalidValue;
      }
      return OK;

   case NLNODE_OPARG_VAR:
      if ((size_t)value > nvars || value <= 0) {
         error("%s :: variable %d outside of the range [1, %zu]\n", __func__,
               value, nvars);
         return Error_InvalidValue;
      }
      return OK;
   default:
      return OK;

   }
#endif
   return OK;
}

static void _debug_lequ(const CMatElt *cme, const Container *ctr)
{
#ifndef NDEBUG
   unsigned dummy;
   double lval;
   Lequ *lequ = ctr->equs[cme->ei].lequ;
   if (!lequ || cme_isplaceholder(cme)) {
      return;
   }
   assert(!lequ_find(lequ, cme->vi, &lval, &dummy));
   assert(cme_isNL(cme) || fabs(cme->value - lval) < DBL_EPSILON);
   assert(cme_isNL(cme) || ((isfinite(cme->value) && isfinite(lval))
          || (!isfinite(cme->value) && !isfinite(lval))));
   if (fabs(cme->value - lval) > DBL_EPSILON) {
      printout(PO_DEBUG, "%s :: in %5d %e vs %e\n", __func__, cme->ei,
                         cme->value, lval);
   }
#endif
}


#ifdef DEBUG_NAN
static void _track_NAN(int nb_equ, double *jacval, int new_idx, int old_idx,
                       int *equidx, int *isvarNL)
{
   bool print_var = false;
   for (unsigned j = 0; j < (unsigned)nb_equ; ++j) {
      if (!isfinite(jacval[j])) {
         print_var = false;
         break;
      }
   }
   if (print_var) {
      error("vidx %d (original index %d)\n", new_idx, old_idx);
      for (unsigned j = 0; j < (unsigned)nb_equ; ++j) {
         printout(PO_DEBUG, "eidx = %d, val = %e, isNL = %d\n", equidx[j],
                  jacval[j], isvarNL[j]);
      }

   }
}

#else /*  DEBUG_NAN */
UNUSED static void _track_NAN(int nb_equ, double *jacval, int new_idx, int old_idx,
                       int *equidx, int *isvarNL)
{
   return;
}
#endif

static void _debug_var_util(Var *v, int indx, double* jacval, int vidx,
                            int *equidx, int *isvarNL, char *buffer)
{
#ifndef NDEBUG
         assert(v->type < VAR_TYPE_LEN);
         _track_NAN(indx, jacval, vidx, v->idx, equidx, isvarNL);
         DPRINT("Adding variable %s #%d (original index = %d) of type %d; lo = %e; lvl = %e; up = %e\n",
                 buffer, vidx, v->idx, v->type, v->bnd.lb, v->value, v->bnd.ub);

         for (unsigned j = 0; j < (unsigned)indx; ++j) {
           DPRINT("Equation %d, jacval = %e; isNL = %d; ", equidx[j], jacval[j], isvarNL[j]);
         }

         // this is required to silence the warning
         // warning: ISO C99 requires at least one argument for the "..." in a
         // variadic macro
         DPRINT("%s", "\n");
#endif
}

int rmdl_copysolveoptions_gams(Model *mdl, const Model *mdl_gms)
{
  const struct ctrdata_gams *gms = (const struct ctrdata_gams *)mdl_gms->ctr.data;
      if (!gms->initialized) {
        error("%s :: GEV is not initialized!\n", __func__);
        return Error_NotInitialized;
      }

      union opt_t val;
      val.i = gmoOptFile(gms->gmo);
      rmdl_setoption(mdl, "solver_option_file_number", val);
      val.b = gevGetIntOpt(gms->gev, gevKeep) == 0 ? false : true;
      rmdl_setoption(mdl, "keep_files", val);
      val.d = gevGetDblOpt(gms->gev, gevOptCA);
      rmdl_setoption(mdl, "atol", val);
      val.d = gevGetDblOpt(gms->gev, gevOptCR);
      rmdl_setoption(mdl, "rtol", val);

   return OK;
}

/**
 * @brief Copy the variables values from a GAMS ctr to a RHP-like one
 *
 * @param ctr      the destination container
 * @param ctr_gms  the GAMS container with the values
 *
 * @return         the error code
 */
int rctr_reporvalues_from_gams(Container * restrict ctr, const Container * restrict ctr_gms)
{
   const struct ctrdata_gams *gms_src = (const struct ctrdata_gams *) ctr_gms->data;
   struct ctrdata_rhp *cdat = (struct ctrdata_rhp *) ctr->data;
   const Fops *fops = ctr->fops;

   gmoHandle_t gmo = gms_src->gmo;

   size_t n = gmoN(gmo);
   size_t m = gmoM(gmo);

   /* -----------------------------------------------------------------------
    * Get the special values
    * ---------------------------------------------------------------------- */

   double gms_pinf = gmoPinf(gmo);
   double gms_minf = gmoMinf(gmo);
   double gms_na = gmoValNA(gmo);

   /* TODO(Xhub) investigate this failure from JULIA  */
   if (ctr->n > n || ctr->m > m) {
      printout(PO_DEBUG, "%s :: the size of the destination gmo is larger then "
               "the source one: n = %d; m = %d vs n= %zu; m = %zu\n",
               __func__, ctr->n, ctr->m, n, m);
//      return Error_DimensionDifferent;
   }

   double *val_ptr, *marginals;
   struct ctrmem CTRMEM working_mem = {.ptr = NULL, .ctr = ctr};
   A_CHECK(working_mem.ptr, ctr_getmem_old(ctr, 3*n*sizeof(double)));
   val_ptr = (double*)working_mem.ptr;
   marginals = &val_ptr[n];
   int *b_pointer = (int*)&marginals[n];

   gmoGetVarL(gmo, val_ptr);
   gmoGetVarM(gmo, marginals);
   gmoGetVarStat(gmo, b_pointer);

   RhpSense objsense;
   S_CHECK(gctr_getsense(ctr_gms, &objsense));
   bool flip_marginal = objsense == RhpMax ? true : false;

   for (size_t i = 0, j = 0; i < cdat->total_n; ++i) {
     if (fops && !fops->keep_var(fops->data, i)) {
       ctr->vars[i].value = SNAN;
       ctr->vars[i].multiplier = SNAN;
       ctr->vars[i].basis = BasisUnset;
     } else {
       ctr->vars[i].value = dbl_from_gams(val_ptr[j], gms_pinf, gms_minf, gms_na);
       double multiplier = flip_marginal ? -marginals[j] : marginals[j];
       ctr->vars[i].multiplier = multiplier;
       ctr->vars[i].basis = basis_from_gams(b_pointer[j]);

       SOLREPORT_DEBUG("VAR %-30s " SOLREPORT_FMT " using var %s\n",
                       ctr_printvarname(ctr, i), solreport_gms_v(&ctr->vars[i]), 
                       ctr_printvarname2(ctr_gms, j));

//       assert(isfinite(ctr->vars[i].value));
       j++;
     }
   }

   /* ----------------------------------------------------------------------
    * Get the equations
    *
    * The logic here is non-trivial, so we don't use a big pointer, we rather
    * go equation by equation
    * ---------------------------------------------------------------------- */
   rhp_idx *rosetta_equs = ctr->rosetta_equs;
   assert(rosetta_equs);

   for (size_t i = 0, j = 0; i < cdat->total_m; ++i) {

     if (fops && !fops->keep_equ(fops->data, i)) {
         EquInfo equinfo;
         S_CHECK(rctr_get_equation(ctr, i, &equinfo));

         rhp_idx ei_new = equinfo.ei;
         assert(valid_ei(ei_new));

         ei_new = rosetta_equs[ei_new];

         if (!equinfo.expanded && valid_ei(ei_new)) {
            /*  TODO(xhub) is this the right thing to do?  */

            double value = dbl_from_gams(gmoGetEquLOne(gmo, ei_new), gms_pinf, gms_minf, gms_na);
            double marginal = dbl_from_gams(gmoGetEquMOne(gmo, ei_new), gms_pinf, gms_minf, gms_na);
            double multiplier = flip_marginal ? -marginal : marginal;
            BasisStatus basis = basis_from_gams(gmoGetEquStatOne(gmo, ei_new));

            if (RHP_LIKELY(!equinfo.flipped)) {
               ctr->equs[i].value = -value;
               ctr->equs[i].multiplier = -multiplier;

               if (basis == BasisLower) {
                  basis = BasisUpper;
               } else if (basis == BasisUpper) {
                  basis = BasisLower;
               }
               ctr->equs[i].basis = basis;

            } else {

               ctr->equs[i].value = value;
               ctr->equs[i].multiplier = multiplier;
               ctr->equs[i].basis = basis;
            }
//            assert(isfinite(ctr->equs[i].value));
            SOLREPORT_DEBUG("EQU %-30s " SOLREPORT_FMT " using equ %s\n",
                            ctr_printequname(ctr, i), solreport_gms(&ctr->equs[i]), 
                            ctr_printequname2(ctr_gms, ei_new));

         } else {
            /* TODO(xhub) determine whether forgotten eqn is right */
            ctr->equs[i].value = SNAN;
            ctr->equs[i].multiplier = SNAN;
            ctr->equs[i].basis = BasisUnset;
         }

     /* ----------------------------------------------------------------------
      * This equations was present in the downstrean model
      * ---------------------------------------------------------------------- */
     } else {
         ctr->equs[i].value = dbl_from_gams(gmoGetEquLOne(gmo, j), gms_pinf, gms_minf, gms_na);
         double marginal = dbl_from_gams(gmoGetEquMOne(gmo, j), gms_pinf, gms_minf, gms_na);
         double multiplier = flip_marginal ? -marginal : marginal;
         ctr->equs[i].multiplier = multiplier;
         ctr->equs[i].basis = basis_from_gams(gmoGetEquStatOne(gmo, j));
//         assert(isfinite(ctr->equs[i].value));
         SOLREPORT_DEBUG("EQU %-30s " SOLREPORT_FMT " using equ %s\n",
                         ctr_printequname(ctr, i), solreport_gms(&ctr->equs[i]), 
                         ctr_printequname2(ctr_gms, j));

         j++;
     }
   }

   return OK;
}

static int _check_deleted_equ(struct ctrdata_rhp *cdat, rhp_idx i)
{
   if (cdat->cmat.deleted_equs && cdat->cmat.deleted_equs[i]) {
      return OK;
   }

   error("%s :: equation %u is in the model and marked as deleted "
         "in the container. However, it could not be found in the"
         " model representation.\n",
         __func__, i);
   return Error_Inconsistency;
}

static int chk_nlpool(NlPool *pool, Model *mdl_src)
{
   if (!pool) {
      error("[GMOexport] ERROR in %s model '%.*s' #%u: nlpool is missing!",
            mdl_fmtargs(mdl_src));
      return Error_Inconsistency;
   }

   if (pool->type != RhpBackendGamsGmo) {
      error("[GMOexport] ERROR in %s model '%.*s' #%u: nlpool has type %s, but "
            "the expected type is %s\n", mdl_fmtargs(mdl_src),
            backend2str(pool->type), backend2str(RhpBackendGamsGmo));
      return Error_Inconsistency;
   }

   if (pool->len > INT_MAX) {
      error("[GMOexport] ERROR in %s model '%.*s' #%u: nlpool has size %zu, but "
            "GAMS has a maximum size limit of %d", mdl_fmtargs(mdl_src),
            pool->len, INT_MAX);
      return Error_SizeTooLarge;
   }

   return OK;
}

static int chk_newgmodct(gmoHandle_t gmo, dctHandle_t dct, Model *mdl_src, Container *ctr_gms)
{
   int status = OK;

   assert(gmoDictionary(gmo));
   assert(gmoDict(gmo));

   /* Some checks  */
   assert(gmoM(gmo) == ctr_gms->m && gmoN(gmo) == ctr_gms->n);

   bool do_expensive_check = optvalb(mdl_src, Options_Expensive_Checks);

   if (do_expensive_check) {
      Container *ctr_src = &mdl_src->ctr;
      RhpContainerData *cdat = ctr_src->data;

      size_t maxdim = MAX(ctr_gms->n, ctr_gms->m);

      M_ArenaTempStamp mstamp = mdl_memtmp_init(mdl_src);
      int *equidx = mdl_memtmp_get(mdl_src, sizeof(int)*maxdim);
      int *isvarNL = mdl_memtmp_get(mdl_src, sizeof(int)*maxdim);
      double *jacval = mdl_memtmp_get(mdl_src, sizeof(double)*maxdim);

      CMatElt * restrict * restrict cmat_equs = cdat->cmat.equs;

      for (int i = 0, m = ctr_gms->m, ei = 0; i < m; ++i, ++ei) {
         int nz, nlnz;
         gmoGetRowSparse(gmo, i, equidx, jacval, isvarNL, &nz, &nlnz);

         while (!valid_ei(ctr_src->rosetta_equs[ei])) { ei++; assert(ei < cdat->total_m); }

         CMatElt *cme = cmat_equs[ei];
         int nz_rhp = 0, nlnz_rhp = 0;
         while (cme) { nz_rhp++; if (cme_isNL(cme)) { nlnz_rhp++; } cme = cme->next_var; }

         if (nz != nz_rhp || nlnz != nlnz_rhp) {
            error("[GMOexport] ERROR for equation %s: GMO nz = %d, nlnz = %d; RHP "
                  "nz = %d, nlnz = %d\n", ctr_printequname(ctr_src, ei), nz, nlnz, nz_rhp,
                  nlnz_rhp);
            status = Error_RuntimeError;
         }
         printout(PO_VVV, "EQU %s involves %d variables (nl: %d)\n",
                  ctr_printequname(ctr_src, ei), nz, nlnz);

      }

      int *vidxs = equidx;
      CMatElt * restrict * restrict cmat_vars = cdat->cmat.vars;

      for (int i = 0, n = ctr_gms->n, vi = 0; i < n; ++i, ++vi) {
         int nz, nlnz;
         gmoGetColSparse(gmo, i, vidxs, jacval, isvarNL, &nz, &nlnz);

         while (!valid_ei(ctr_src->rosetta_vars[vi])) { vi++; assert(vi < cdat->total_n); }
         CMatElt *cme = cmat_vars[vi];
         int nz_rhp = 0, nlnz_rhp = 0;
         while (cme) { nz_rhp++; if (cme_isNL(cme)) { nlnz_rhp++; } cme = cme->next_equ; }

         if (nz != nz_rhp || nlnz != nlnz_rhp) {

            error("[GMOexport] ERROR for variable %s: GMO nz = %d, nlnz = %d; RHP "
                  "nz = %d, nlnz = %d\n", ctr_printvarname(ctr_src, vi), nz, nlnz, nz_rhp,
                  nlnz_rhp);
            status = Error_RuntimeError;
         }
         printout(PO_VVV, "VAR %s is present in %d equations (nl: %d)\n",
                  ctr_printvarname(ctr_src, vi), nz, nlnz);

      }

      ctr_memtmp_fini(mstamp);
   }

   if (status != OK) { return status; }

   if (gmoM(gmo) != dctNRows(dct)) {
      error("[GMOexport] ERROR: There are %d equations in the DCT, but %d are "
            "expected\n", dctNRows(dct), gmoM(gmo));
      return Error_RuntimeError;
   }

   if (gmoN(gmo) != dctNCols(dct)) {
      error("[GMOexport] ERROR: There are %d variables in the DCT, but %d are "
            "expected\n", dctNCols(dct), gmoN(gmo));
      return Error_RuntimeError;
   }

   return OK;

}
/**
 * @brief Export an RHP model to a GAMS GMO object
 *
 * @param mdl_src  the RHP model
 * @param mdl_gms  the GAMS model
 *
 * @return         the error code
 */
int rmdl_exportasgmo(Model *mdl_src, Model *mdl_gms)
{
   double start = get_thrdtime();

   assert(mdl_gms->backend == RhpBackendGamsGmo && mdl_is_rhp(mdl_src));

   int status = OK;
   char buffer[2048];
   RESHOP_STATIC_ASSERT(sizeof(buffer) >= GMS_SSSIZE, "");

   Container * restrict ctr_src = &mdl_src->ctr;
   Container * restrict ctr_gms = &mdl_gms->ctr;
   RhpContainerData * restrict cdat = (RhpContainerData *)ctr_src->data;
   GmsContainerData * restrict gms_dst = (GmsContainerData *)ctr_gms->data;

   if (!(mdl_gms->status & MdlInstantiable)) {
      error("[GMOexport] ERROR: %s model '%.*s' #%u is not instantiable\n",
            mdl_fmtargs(mdl_gms));
      return Error_RuntimeError;
   }

   assert(ctr_gms->n > 0);

  /* ----------------------------------------------------------------------
   * GAMS/GMO requires to have a objvar. This needs to be checked before
   * gmdl_cdat_setup()
   * ---------------------------------------------------------------------- */

   Fops *fops = ctr_src->fops;
   rhp_idx *rosetta_equs = ctr_src->rosetta_equs;
   rhp_idx *rosetta_vars = ctr_src->rosetta_vars;

  /* ----------------------------------------------------------------------
   * 2024.03.12: HACK to make this workable 
   * ---------------------------------------------------------------------- */
   if (!rosetta_equs) {
      unsigned nequs = ctr_nequs_total(ctr_src);
      MALLOC_(rosetta_equs, rhp_idx, ctr_nequs_total(ctr_src));
      ctr_src->rosetta_equs = rosetta_equs;
      for (unsigned i = 0; i < nequs; ++i) {
         rosetta_equs[i] = i;
      }
   }

   if (!rosetta_vars) {
      unsigned nvars = ctr_nvars_total(ctr_src);
      MALLOC_(rosetta_vars, rhp_idx, ctr_nvars_total(ctr_src));
      ctr_src->rosetta_vars = rosetta_vars;
      for (unsigned i = 0; i < nvars; ++i) {
         rosetta_vars[i] = i;
      }
   }

   ModelType probtype;
   S_CHECK(mdl_gettype(mdl_src, &probtype));

   bool inject_objvar = false;
   rhp_idx objvar, objequ = IdxNA;

   if (mdltype_isopt(probtype)) {
      mdl_getobjvar(mdl_src, &objvar);
      mdl_getobjequ(mdl_src, &objequ);

      if (!valid_vi(objvar) || !valid_vi(rosetta_vars[objvar])) {

         if (!valid_ei(objequ) || !valid_ei(rosetta_equs[objequ])) {
            error("[GMOexport] ERROR in %s model '%.*s' #%u has neither a valid "
                  "objective variable, or a valid objective equation.\n"
                  "This will result in an unbounded problem, not worth solving it\n",
                  mdl_fmtargs(mdl_src));
            return Error_ModelUnbounded;
         }


         S_CHECK(ctr_resize(ctr_gms, ctr_gms->n+1, ctr_gms->m));
         ctr_gms->n++;
         inject_objvar = true;

         /* ----------------------------------------------------------------
          * Equations cannot be copied, since we rearrange them in NL/aff groups
          * ---------------------------------------------------------------- */

         if (mdl_is_rhp(mdl_src)) {
            S_CHECK(rctr_func2eval_add(&mdl_src->ctr, objequ));
         }
      }
   }

   /* Setup all GAMS objects */
   S_CHECK(gmdl_cdat_create(mdl_gms, mdl_src));

   gmoHandle_t gmo = gms_dst->gmo;
   dctHandle_t dct = gms_dst->dct;
   assert(gms_dst->owndct);
   assert(dctNUels(dct) == 0 && dctNLSyms(dct) == 0);

   /* -----------------------------------------------------------------------
    * Get the special values
    * ---------------------------------------------------------------------- */

   double gms_pinf = gmoPinf(gmo);
   double gms_minf = gmoMinf(gmo);
   double gms_na = gmoValNA(gmo);

   bool flip_marginal = gmoSense(gmo) == gmoObj_Max;

   /* -----------------------------------------------------------------------
    * Start the allocation. This needs to be early enough so that we can free
    * it in the _exit.
    * ---------------------------------------------------------------------- */

   /* -----------------------------------------------------------------------
    * Pointer definition
    * ---------------------------------------------------------------------- */

   bool *NLequs = NULL;
   int *equidx = NULL, *isvarNL = NULL;
   double *jacval = NULL;

   /* ----------------------------------------------------------------------
    * Now we go through the equations and add them
    * ---------------------------------------------------------------------- */

#if 0
     else if (probtype == MdlProbType_emp) {
      const EmpDag *empdag = &mdl->empinfo.empdag;
      MathPrgm **mplist = empdag->mps.arr;

      if (empdag->mps.len == 0) {
         error("[export2gmo] ERROR while exporting %s model '%.*s' #%u: EMPDAG "
               "is empty\n", mdl_fmtargs(mdl));
      }

      for (unsigned i = 0, len = empdag->mps.len; i < len; ++i) {
         rhp_idx objequ = mp_getobjequ(mplist[i]);
         if (valid_ei(objequ)) { rhp_idx_addsorted(&objequs, objequ); }
      }

   }
#endif

   bool has_metadata = mdltype_hasmetadata(probtype);

   for (size_t i = 0, len = cdat->total_m; i < len; ++i) {

      rhp_idx ei = rosetta_equs[i];
      if (!valid_ei(ei)) { continue; }

      assert(ei < ctr_gms->m);
      Equ *e = &ctr_src->equs[i];

      /* This kludge maybe removed if we decide not to delete part of the
       * representation */
      /*  TODO(Xhub) should we skip creating the equation here? */
      if (!cdat->cmat.equs[i]) {
         S_CHECK_EXIT(_check_deleted_equ(cdat, i));
      }

      /* -----------------------------------------------------------------
       * Insert Symbol into dictionary
       * GAMS does not support scalar variables with the name myvar(myidx)
       * ----------------------------------------------------------------- */

      S_CHECK_EXIT(ctr_copyequname(ctr_src, i, buffer, sizeof(buffer)/sizeof(char)));
      gams_fix_equvar_names(buffer);

      /* TODO(xhub) LOW: try to expand that */

      /* dct, symName, symTyp, symDim, userInfo, symTxt  */
      dctAddSymbol(dct, buffer, dcteqnSymType, 0, 0, "");
      dctAddSymbolData(dct, NULL);

      int equtype;
      switch (e->object) {
      case DefinedMapping:
         equtype = gmoequ_E;
         break;
      case ConeInclusion:
         S_CHECK_EXIT(cone_to_gams_relation_type(e->cone, &equtype));
         break;
      case Mapping:
         equtype = gmoequ_N;
         break;
      default:
         error("[GMOexport] ERROR in %s model '%.*s' #%u: equ '%s' has "
               "unsupported type %s\n", mdl_fmtargs(mdl_src),
               ctr_printequname(ctr_src, e->idx), equtype_name(e->object));
         status = Error_NotImplemented;
         goto _exit;
      }

      if (objequ == i && equtype != gmoequ_E) {
         /* a RHP source model would have objequ as mapping */
         if (equtype == gmoequ_N) { assert(mdl_is_rhp(mdl_src)); equtype = gmoequ_E; }
         else {
            error("[GMOexport] ERROR: objective equation '%s' has type %s, it "
                  "should be %s\n", ctr_printequname(ctr_src, i),
                  gams_equtype_name(equtype), gams_equtype_name(gmoequ_E));
            status = Error_Inconsistency;
            goto _exit;
         }
      }

      /* match information is always 1-based, indep of the index base  */
      int match = 0;
      if (has_metadata) {
         assert(ctr_src->equmeta);
         rhp_idx vi_old = ctr_src->equmeta[i].dual;

         if (valid_vi(vi_old)) {
           assert(valid_vi(rosetta_vars[vi_old]) && equtype == gmoequ_N);

           match = rosetta_vars[vi_old] + 1;

         } else if (equmeta_hasdual(&ctr_src->equmeta[i])){
            error("%s :: equation %u has no valid dual\n", __func__, vi_old);
         }
      }

      double equ_cst = equ_get_cst(e);
      double multiplier = e->multiplier;
      double marginal = flip_marginal ? - multiplier : multiplier;
      double gms_marginal = dbl_to_gams(marginal, gms_pinf, gms_minf, gms_na);

      GMSCHK_EXIT(gmoAddRow(gmo,
       /* type      */     equtype,                         
       /* match     */     match,                                
                           /*  TODO(xhub) check correctness 
                            *  2023.10.20: used to be e->value + equ_cst*/
       /* slack     */     dbl_to_gams(e->value-equ_cst, gms_pinf, gms_minf, gms_na), 
      /* scale      */     1.,                                   
      /* rhs        */     -equ_cst,         
      /* marginal   */     gms_marginal,       
      /* is basic?  */     (e->basis == BasisBasic) ? 0 : 1,     
                           0,
                           NULL,
                           NULL,
                           NULL));

      GMOEXPORT_DEBUG("adding equ %-30s of type %15s with LHS = % 3.2e and RHS = % 3.2e; "
                      ".m = % 3.2e; %-10s and %-10s", buffer, gams_equtype_name(equtype),
                      dbl_to_gams(e->value-equ_cst, gms_pinf, gms_minf, gms_na),
                      dbl_to_gams(-equ_cst, gms_pinf, gms_minf, gms_na),
                      gms_marginal, e->basis == BasisBasic ? "basic" : "non-basic",
                      isvarNL ? "nonlinear" : "linear");

//         assert(gmoM(gmo) == dctNRows(dct));
   }

   CALLOC_EXIT(NLequs, bool, ctr_gms->m);

   size_t maxdim = MAX(ctr_gms->n, ctr_gms->m);
   MALLOC_EXIT(equidx, int, maxdim);
   MALLOC_EXIT(jacval, double, maxdim);
   MALLOC_EXIT(isvarNL, int, maxdim);

   for (size_t i = 0, len = cdat->total_n; i < len; ++i) {

      rhp_idx vi = rosetta_vars[i];
      if (!valid_vi(vi)) { continue; }

      CMatElt *vtmp = cdat->cmat.vars[i];
      Var *v = &ctr_src->vars[i];

      S_CHECK_EXIT(ctr_copyvarname(ctr_src, i, buffer, sizeof(buffer)));
      gams_fix_equvar_names(buffer);

      /* dct, symName, symTyp, symDim, userInfo, symTxt  */
      dctAddSymbol(dct, buffer, dctvarSymType, 0, 0, "");

      /* We don't want UELs yet --xhub */
      /*  TODO(xhub) change this to be functional. It looks like we can't
       *  use CPLEX because of that */
      dctAddSymbolData(dct, NULL);

      if (!vtmp) {
         error("[GMOexport] ERROR: variable '%s' is no longer in the container\n",
               ctr_printvarname(ctr_src, i));
         status = Error_Inconsistency;
         goto _exit;
      }

      /* -----------------------------------------------------------------
       * Walk through all the equation where the variable appears
       *
       * One exception is when the variable is in a matching relationship
       * with 
       * ----------------------------------------------------------------- */

      vtmp = cdat->cmat.vars[i];
      int indx = 0;
      while (vtmp) {
//         assert(vtmp->vidx == v->idx);
         assert(!vtmp->prev_equ || vtmp->prev_equ->vi == vtmp->vi);
         assert(!vtmp->prev_equ || vtmp->prev_equ->next_equ == vtmp);
         /* TODO(xhub) this assert is no longer valid with subset
          * assert(model->eqns[vtmp->eidx]); */

         if (!valid_ei(vtmp->ei) || !valid_ei(rosetta_equs[vtmp->ei])) {
            vtmp = vtmp->next_equ;
            continue;
         }

         /* ----------------------------------------------------------
             * If the variable is matched to an equation, GAMS needs to see
             * it in the equation ... Make sure we have this kludge
             * ---------------------------------------------------------- */

         bool isplaceholder = cme_isplaceholder(vtmp);
         if (isplaceholder && ctr_src->varmeta
            && ctr_src->varmeta[i].type == VarPrimal) {

            if (ctr_src->varmeta[i].ppty == VarPerpToViFunction) {
               assert(vtmp->ei == ctr_src->varmeta[i].dual);

               jacval[indx] = 0.; //TODO: CHECK if valid
               //jacval[indx] = GMS_SV_EPS;

            } else if (ctr_src->varmeta[i].ppty == VarPerpToZeroFunctionVi) {
               //vtmp = vtmp->next_equ;
               TO_IMPLEMENT_EXIT("Zero Func");
               //continue;
            } else {
               error("%s :: variable %s is a placeholder with type "
                     "'Primal', but has a subtype" "%d\n", __func__,
                     ctr_printvarname(ctr_src, i), ctr_src->varmeta[i].ppty);
               jacval[indx] = SNAN;
            }

         } else if (!isplaceholder) {

            /* TODO(xhub) Use a not available?  */
            jacval[indx] = dbl_to_gams(vtmp->value, gms_pinf, gms_minf, gms_na);

         } else if (isplaceholder && valid_vi(vtmp->vi)
            && !valid_ei(vtmp->ei) && vtmp->vi == objvar) {
            vtmp = vtmp->next_equ;
            continue;

         } else {
            error("[GMOexport] ERROR: variable '%s' is a placeholder but has type %d\n", 
                  ctr_printvarname(ctr_src, i), ctr_src->varmeta ? ctr_src->varmeta[i].type : -1);
            jacval[indx] = SNAN;
         }

         equidx[indx] = rosetta_equs[vtmp->ei];

         assert(cme_isNL(vtmp) || isfinite(jacval[indx]));
         _debug_lequ(vtmp, ctr_src);
         assert(equidx[indx] < ctr_gms->m);

         bool varNL = cme_isNL(vtmp);
         isvarNL[indx] = varNL ? 1 : 0;

         /* ----------------------------------------------------------
             * Update the NL status of the equation if the variable is
             * non-linear
             * TODO(Xhub) QUAD
             * ---------------------------------------------------------- */

         if (!NLequs[equidx[indx]] && varNL) {
            NLequs[equidx[indx]] = true;
            DPRINT("Equation %d is NL due to variable #%d (#%d)\n",
                   equidx[indx], vi, vtmp->vi);
         }

         indx++;
         vtmp = vtmp->next_equ;
      }

#ifdef A20240118
      /* TODO(xhub) factorize with previous */
      /* TODO(xhub) where does that crazyness come from? */
      for (size_t j = 0; j < idx_deleted; ++j) {
         CMatElt *me = deleted_equ[j];

         while (me) {
            if ((size_t)me->vi == i) {
               equidx[indx] = rosetta_equs[me->ei];
               jacval[indx] = dbl_to_gams(me->value, gms_pinf, gms_minf, gms_na);
               isvarNL[indx] = me->isNL ? 1 : 0;

               _debug_lequ(me, ctr_src);
               assert(me->isNL || isfinite(me->value));
               assert(equidx[indx] < ctr_gms->m);

               if (!NLequs[equidx[indx]] && me->isNL) {
                  NLequs[equidx[indx]] = true;
               }
               indx++;
               break;
            }
            me = me->next_var;
         }
      }
#endif

#ifdef TO_DELETE_2024_03_05
      if (ctr_src->varmeta && ctr_src->varmeta[i].ppty == VarPerpToViFunction) {
        bool has_dual = false;
         assert(valid_ei(ctr_src->varmeta[i].dual));
         rhp_idx e_dual = rosetta_equs[ctr_src->varmeta[i].dual];
         assert(valid_ei(e_dual));

         for (int j = 0; j < indx; ++j) {
            if (equidx[j] == e_dual) {
//               if (jacval[j] == 0.) {
//                  jacval[j] = GMS_SV_EPS;
//               }
               has_dual = true;
               break;
            }
         }

         /* TODO URG cleanup this */
         if (!has_dual) {
            //equidx[indx] = e_dual;
            //jacval[indx] = 0.;
            //jacval[indx] = GMS_SV_EPS;
            //isvarNL[indx++] = 0;
         }
      }
#endif

      /*  this assert is not helpful with filtering */
//      assert((size_t)nb_equ >= indx);
      _debug_var_util(v, indx, jacval, vi, equidx, isvarNL, buffer);

      double multiplier = v->multiplier;
      double marginal = flip_marginal ? - multiplier : multiplier;
      double gms_marginal = dbl_to_gams(marginal, gms_pinf, gms_minf, gms_na);

      GMSCHK_EXIT(gmoAddCol(gmo,
         /* type     */    v->type,
         /* lo       */    dbl_to_gams(v->bnd.lb, gms_pinf, gms_minf, gms_na),
         /* level    */    dbl_to_gams(v->value, gms_pinf, gms_minf, gms_na),
         /* up       */    dbl_to_gams(v->bnd.ub, gms_pinf, gms_minf, gms_na),
         /* marginal */    gms_marginal,
         /* basic    */    v->basis == BasisBasic ? 0 : 1,
         /* SOS      */    v->type == VAR_SOS1 || v->type == VAR_SOS2
                                      ? gms_dst->sos_group[vi] : 0,
         /* prior    */    1,
         /* scale    */    1.,
                           indx,
                           equidx,
                           jacval,
                           isvarNL));

      GMOEXPORT_DEBUG("adding var %-30s in [% 3.2e%-*s% 3.2e%*s .l = % 3.2e; .m = % 3.2e;"
                      " %-10s %-10s and found in %d equations", buffer,
                      NUMANDEPAD(dbl_to_gams(v->bnd.lb, gms_pinf, gms_minf, gms_na)),
                      ",",
                      NUMANDEPAD(dbl_to_gams(v->bnd.ub, gms_pinf, gms_minf, gms_na)),
                      "]",
                      dbl_to_gams(v->value, gms_pinf, gms_minf, gms_na),
                      gms_marginal, v->basis == BasisBasic ? "basic" : "non-basic",
                      isvarNL ? "nonlinear" : "linear", indx);

   }

   for (unsigned j = 0; j < (unsigned) ctr_gms->m; ++j) {
       DPRINT("Equation %d: isNL = %s\n", j, NLequs[j] ? "true" : "false");
   }

   S_CHECK_EXIT(chk_nlpool(ctr_src->pool, mdl_src));

   double *nlpool = ctr_src->pool->data;
   int nlpool_len = (int)ctr_src->pool->len;

   /* ----------------------------------------------------------------------
    * Add the opcode to the GMO
    * ---------------------------------------------------------------------- */

   for (size_t i = 0, len = cdat->total_m; i < len; ++i) {

      rhp_idx ei = rosetta_equs[i];
      if (!valid_ei(ei) || !NLequs[ei]) { continue; }

      assert(ei < ctr_gms->m);

      /* ------------------------------------------------------------------
        * Generate the GAMS opcode
        * ------------------------------------------------------------------ */

      int *instrs = NULL, *args = NULL, codelen;
      S_CHECK_EXIT(gctr_genopcode(ctr_src, i, &codelen, &instrs, &args));

      /* ----------------------------------------------------------------
          * The equation may have no opcode. We still check, for consistency,
          * that the NL flag is not set
          * ---------------------------------------------------------------- */

      if (codelen == 0) {
         /* We error here otherwise GMO is going to error*/
         if (NLequs[ei]) {
            error("[GMOexport] ERROR: equation '%s' is declared as NL, but "
                  "has no nlcode\n", ctr_printequname(ctr_src, i));
            status = Error_Inconsistency;
            goto _exit;
         }
         continue;
      }

      /* ------------------------------------------------------------------
        * Transform the GAMS opcode: change variable index and/or insert variable
        * values
        * ------------------------------------------------------------------ */

      if (fops) {
         S_CHECK_EXIT(fops->transform_gamsopcode(fops->data, ei, codelen, instrs, args));
      }


#ifndef NDEBUG
      for (unsigned j = 0; j < (unsigned)codelen; ++j) {
         _debug_check_nlcode(instrs[j], args[j], ctr_gms->n, nlpool_len);
      }
#endif

      int ei_gmo = ei;
      /* ------------------------------------------------------------------
        * The last instruction is (nlStore, eidx)  This value is used
        * inconsistently in GAMS, but we have to properly set the equation
        * index
        * ------------------------------------------------------------------ */

      assert(args[codelen-1] == 1 + ei_gmo && instrs[codelen-1] == nlStore);

      /* ------------------------------------------------------------------
        * Insert the opcode in the GMO
        * ------------------------------------------------------------------ */

      GMSCHK_EXIT(gmoDirtySetRowFNLInstr(gmo, ei_gmo, codelen, instrs, args, NULL,
                                        nlpool, nlpool_len));

      // HACK ARENA: we need to release memory here, just in case if was used
      ctr_relmem_recursive_old(ctr_src);

   }


   printout(PO_VVV, "[GMOexport] Model stat: # rows: %d (nl: %d); # cols: %d (nl: %d)\n",
            gmoM(gmo), gmoNLM(gmo), gmoN(gmo), gmoNLN(gmo));
   printout(PO_VVV, "[GMOexport] Model stat: NNZ: %d (nl: %d)\n",
            gmoNZ(gmo), gmoNLNZ(gmo));

   /*  \TODO(xhub) we need to get the resulting model_type somehow*/

   /* ----------------------------------------------------------------------
    * Set the objective variable value
    * ---------------------------------------------------------------------- */

  if (mdltype_isopt(probtype)) {

      int objvar_gmo;
      if (inject_objvar) {
         objvar_gmo = ctr_gms->n-1;

         /* TODO: ensure uniqueness ...*/
         strcat(buffer, "reshop_objvar");
         /* dct, symName, symTyp, symDim, userInfo, symTxt  */
         dctAddSymbol(dct, buffer, dctvarSymType, 0, 0, "");
         dctAddSymbolData(dct, NULL);

         rhp_idx ei_objequ = rosetta_equs[objequ];
         equidx[0] = (int) ei_objequ;
         jacval[0] = -1.;
         isvarNL[0] = 0;

         GMSCHK_EXIT(gmoAddCol(gmo,
         /* type     */       gmovar_X,
         /* lo       */       gms_minf,
         /* level    */       0., /* TODO: tune this*/
         /* up       */       gms_pinf,
         /* marginal */       0.,
         /* basic    */       0,
         /* SOS      */       0,
         /* prior    */       1,
         /* scale    */       1.,
                              1,
                              equidx,
                              jacval,
                              isvarNL));
      } else {
         S_CHECK_EXIT(vi_inbounds(objvar, cdat->total_n, __func__));
         assert(valid_vi(rosetta_vars[objvar]));
         objvar_gmo = rosetta_vars[objvar];
      }

    gmoObjVarSet(gmo, objvar_gmo);
    RhpSense sense;
    S_CHECK_EXIT(rmdl_getsense(mdl_src, &sense));
      switch (sense) {
      case RHP_MIN:
         gmoSenseSet(gmo, gmoObj_Min);
         break;
      case RHP_MAX:
         gmoSenseSet(gmo, gmoObj_Max);
         break;
      default:
         error("%s :: unsupported model sense %s. Only RHP_MIN and RHP_MAX are "
               "supported\n", __func__, sense2str(sense));
         status = Error_Inconsistency;
         goto _exit;
    }

  } else {
    gmoObjVarSet(gmo, -1);
  }

   /* ----------------------------------------------------------------------
    * Set dictionary and finalize the GMO
    * ---------------------------------------------------------------------- */

   gmoDictSet(gmo, dct);
   gmoDictionarySet(gmo, 1);

   GMSCHK_BUF_EXIT(gmoCompleteData, gmo, buffer);

   S_CHECK_EXIT(chk_newgmodct(gmo, dct, mdl_src, ctr_gms));

   /* This must be after setting the dictionary */
   assert(!mdltype_isopt(probtype) || gmoGetObjName(gmo, buffer));

   /* Complete the initialization of   */
   /*  TODO(Xhub) factorize code */
   gms_dst->owning_handles = true;
   REALLOC_EXIT(gms_dst->rhsdelta, double, ctr_src->m+1);
   gms_dst->initialized = true;

_exit:
   /*  FREE all */
   /* TODO(xhub) ave those 2?  */
   FREE(NLequs);
   FREE(equidx);
   FREE(jacval);
   FREE(isvarNL);

   mdl_src->timings->gmo_creation += get_thrdtime() - start;

   return status;
}
