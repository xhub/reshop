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

#ifndef NDEBUG
#define GMOEXPORT_DEBUG(str, ...) trace_stack("[GMOexport] " str "\n", __VA_ARGS__) \
//  { GDB_STOP(); }
#else
#define GMOEXPORT_DEBUG(...)
#endif

#ifndef NDEBUG
#define SOLREPORT_DEBUG(str, ...) trace_solreport("[solreport] " str "\n", __VA_ARGS__) \
//  { GDB_STOP(); }
#else
#define SOLREPORT_DEBUG(...)
#endif


static int _debug_check_nlcode(int opcode, int value, size_t nvars, size_t poolen)
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

static void _debug_lequ(const CMatElt *me, const Container *ctr)
{
#ifndef NDEBUG
   unsigned dummy;
   double lval;
   Lequ *lequ = ctr->equs[me->ei].lequ;
   if (!lequ || me->placeholder) {
      return;
   }
   assert(!lequ_find(lequ, me->vi, &lval, &dummy));
   assert(me->isNL || fabs(me->value - lval) < DBL_EPSILON);
   assert(me->isNL || ((isfinite(me->value) && isfinite(lval))
          || (!isfinite(me->value) && !isfinite(lval))));
   if (fabs(me->value - lval) > DBL_EPSILON) {
      printout(PO_DEBUG, "%s :: in %5d %e vs %e\n", __func__, me->ei,
                         me->value, lval);
   }
#endif
}


static inline bool _iscurrent_equ(Container *ctr, size_t eidx)
{
  assert(ctr->rosetta_equs);
  return !ctr->rosetta_equs || valid_ei(ctr->rosetta_equs[eidx]);
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
static void _track_NAN(int nb_equ, double *jacval, int new_idx, int old_idx,
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
                 buffer, vidx, v->idx, v->type, v->bnd.lb, v->level, v->bnd.ub);

         for (unsigned j = 0; j < (unsigned)indx; ++j) {
           DPRINT("Equation %d, jacval = %d; isNL = %d; ", equidx[j], jacval[j], isvarNL[j]);
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
   A_CHECK(working_mem.ptr, ctr_getmem(ctr, 3*n*sizeof(double)));
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

       SOLREPORT_DEBUG("VAR %-30s " SOLREPORT_FMT " using var %s",
                       ctr_printvarname(ctr, i), solreport_gms_v(&ctr->vars[i]), 
                       ctr_printvarname2(ctr_gms, j));

//       assert(isfinite(ctr->vars[i].value));
       j++;
     }
     assert(j <= n);
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
            SOLREPORT_DEBUG("EQU %-30s " SOLREPORT_FMT " using equ %s",
                            ctr_printequname(ctr, i), solreport_gms(&ctr->equs[i]), 
                            ctr_printequname2(ctr_gms, ei_new));

         } else {
            /* TODO(xhub) determine whether forgotten eqn is right */
            ctr->equs[i].value = SNAN;
            ctr->equs[i].multiplier = SNAN;
            ctr->equs[i].basis = BasisUnset;
         }

     } else {
         ctr->equs[i].value = dbl_from_gams(gmoGetEquLOne(gmo, j), gms_pinf, gms_minf, gms_na);
         double marginal = dbl_from_gams(gmoGetEquMOne(gmo, j), gms_pinf, gms_minf, gms_na);
         double multiplier = flip_marginal ? -marginal : marginal;
         ctr->equs[i].multiplier = multiplier;
         ctr->equs[i].basis = basis_from_gams(gmoGetEquStatOne(gmo, j));
//         assert(isfinite(ctr->equs[i].value));
         SOLREPORT_DEBUG("EQU %-30s " SOLREPORT_FMT " using equ %s",
                         ctr_printequname(ctr, i), solreport_gms(&ctr->equs[i]), 
                         ctr_printequname2(ctr_gms, j));

         j++;
     }
   }

   return OK;
}

static int _check_deleted_equ(struct ctrdata_rhp *cdat, rhp_idx i)
{
   if (cdat->deleted_equs && cdat->deleted_equs[i]) {
      return OK;
   }

   error("%s :: equation %u is in the model and marked as deleted "
         "in the container. However, it could not be found in the"
         " model representation.\n",
         __func__, i);
   return Error_Inconsistency;
}

/**
 * @brief Export an RHP model to a GAMS GMO object
 *
 * @param mdl      the RHP model
 * @param mdl_gms  the GAMS model
 *
 * @return         the error code
 */
int rmdl_exportasgmo(Model *mdl, Model *mdl_gms)
{
   double start = get_thrdtime();

   assert(mdl_gms->backend == RHP_BACKEND_GAMS_GMO && mdl_is_rhp(mdl));

   int status = OK;
   char buffer[2048];
   RESHOP_STATIC_ASSERT(sizeof(buffer) >= GMS_SSSIZE, "");

   Container * restrict ctr_src = &mdl->ctr;
   Container * restrict ctr_gms = &mdl_gms->ctr;
   RhpContainerData * restrict cdat = (RhpContainerData *)ctr_src->data;
   GmsContainerData * restrict gms_dst = (GmsContainerData *)ctr_gms->data;


   /* Setup all GAMS objects */
   S_CHECK(gmdl_cdat_setup(mdl_gms, mdl));


   gmoHandle_t gmo = gms_dst->gmo;
   dctHandle_t dct = gms_dst->dct;

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

   IdxArray objequs;
   rhp_idx_init(&objequs);

   /* -----------------------------------------------------------------------
    * Pointer definition
    * ---------------------------------------------------------------------- */

   bool *NLequs = NULL;
   int *equidx = NULL, *isvarNL = NULL;
   double *jacval = NULL;



   Fops *fops = ctr_src->fops; assert(fops);

   if (ctr_src->rosetta_vars || ctr_src->rosetta_equs) {
      error("[GMOexport] ERROR: in %s model '%.*s' #%u, rosetta arrays are "
            "already present\n", mdl_fmtargs(mdl));
      status = Error_UnExpectedData;
      goto _exit;
   }

   MALLOC_EXIT(ctr_src->rosetta_vars, rhp_idx, cdat->total_n);
   MALLOC_EXIT(ctr_src->rosetta_equs, rhp_idx, MAX(cdat->total_m, 1));

   rhp_idx *rosetta_equs = ctr_src->rosetta_equs;
   rhp_idx *rosetta_vars = ctr_src->rosetta_vars;

   /* ----------------------------------------------------------------------
    * Unfortunately we need to get a valid rosetta_var for the 
    * ---------------------------------------------------------------------- */

   size_t skip_var = 0;

   for (size_t i = 0; i < cdat->total_n; ++i) {

      if (fops->keep_var(fops->data, i)) {
         rhp_idx vi = i - skip_var;
         rosetta_vars[i] = vi;
      } else {
         skip_var++;
         rosetta_vars[i] = IdxDeleted;
      }
   }

   /* ----------------------------------------------------------------------
    * Now we go through the equations and add them
    * ---------------------------------------------------------------------- */
   size_t skip_equ = 0;

   ModelType mdltype;
   S_CHECK_EXIT(mdl_gettype(mdl, &mdltype));
   if (mdltype_isopt(mdltype)) {
      rhp_idx objequ;
      rmdl_getobjequ(mdl, &objequ);
      S_CHECK_EXIT(rhp_idx_add(&objequs, objequ));
   } else if (mdltype == MdlType_emp) {
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

   bool has_metadata = mdltype_hasmetadata(probtype);

   for (size_t i = 0; i < cdat->total_m; ++i) {

      /* deleted equations are easily handled */
      if (!fops->keep_equ(fops->data, i)) {
         rosetta_equs[i] = IdxDeleted;
         skip_equ++;
         continue;
      }

      rhp_idx ei = i - skip_equ;
      rosetta_equs[i] = ei;

      assert(ei < ctr_gms->m);
      Equ *e = &ctr_src->equs[i];

      /* This kludge maybe removed if we decide not to delete part of the
       * representation */
      /*  TODO(Xhub) should we skip creating the equation here? */
      if (!cdat->equs[i]) {
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
      case ConeInclusion:
         S_CHECK_EXIT(cone_to_gams_relation_type(e->cone, &equtype));
         break;
      case Mapping:
         equtype = gmoequ_N;
         break;
      default:
         error("%s :: equ '%s' has unsupported type %s\n", __func__,
               ctr_printequname(ctr_src, e->idx), equtype_name(e->object));
         status = Error_NotImplemented;
         goto _exit;
      }

      if (rhp_idx_findsorted(&objequs, i) != UINT_MAX && equtype != gmoequ_E) {
         /* a RHP source model would have objequ as mapping */
         if (equtype == gmoequ_N) { assert(mdl_is_rhp(mdl)); equtype = gmoequ_E; }
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

      G_CHK_EXIT(gmoAddRow(gmo,
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

   /* ----------------------------------------------------------------------
    * Do some bookkeeping
    *
    * if the number of skipped equation and the active one is not equal to
    * the original one, there are multiple plausible explanation
    *
    * - constant equation, with no variables
    * ---------------------------------------------------------------------- */


   S_CHECK_EXIT(rctr_compress_check_equ(ctr_src, ctr_gms, skip_equ));

   CALLOC_EXIT(NLequs, bool, ctr_gms->m);

   MALLOC_EXIT(equidx, int, ctr_gms->m);
   MALLOC_EXIT(jacval, double, ctr_gms->m);
   MALLOC_EXIT(isvarNL, int, ctr_gms->m);

   rhp_idx objvar;
   rmdl_getobjvar(mdl, &objvar);

   for (size_t i = 0; i < cdat->total_n; ++i) {

      rhp_idx vi = rosetta_vars[i];
      if (!valid_vi(vi)) { continue; }

      CMatElt *vtmp = cdat->vars[i];
      Var *v = &ctr_src->vars[i];

      S_CHECK(ctr_copyvarname(ctr_src, i, buffer, sizeof(buffer)));
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

      /*  TODO(xhub) this is quite an upper-bound */
      int nb_equ = 0;

      /* -----------------------------------------------------------------
       * Count the number of equation in which the variable may appear
       * ----------------------------------------------------------------- */

      while (vtmp) { nb_equ++; vtmp = vtmp->next_equ; }
      nb_equ++;

      /* -----------------------------------------------------------------
       * Realloc the temporary data if needed
       * ----------------------------------------------------------------- */


      /* -----------------------------------------------------------------
       * Walk through all the equation where the variable appears
       *
       * One exception is when the variable is in a matching relationship
       * with 
       * ----------------------------------------------------------------- */

      vtmp = cdat->vars[i];
      int indx = 0;
      while (vtmp) {
//         assert(vtmp->vidx == v->idx);
         assert(!vtmp->prev_equ || vtmp->prev_equ->vi == vtmp->vi);
         assert(!vtmp->prev_equ || vtmp->prev_equ->next_equ == vtmp);
         /* TODO(xhub) this assert is no longer valid with subset
          * assert(model->eqns[vtmp->eidx]); */

         if (_iscurrent_equ(ctr_src, vtmp->ei)) {

            /* ----------------------------------------------------------
             * If the variable is matched to an equation, GAMS needs to see
             * it in the equation ... Make sure we have this kludge
             * ---------------------------------------------------------- */

            if (vtmp->placeholder && ctr_src->varmeta
                  && ctr_src->varmeta[i].type == VarPrimal) {
               if (ctr_src->varmeta[i].ppty == VarPerpToViFunction) {
                  assert(vtmp->ei == ctr_src->varmeta[i].dual);

                  jacval[indx] = 0.;

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

            } else if (!vtmp->placeholder) {

               /* TODO(xhub) Use a not available?  */
               jacval[indx] = dbl_to_gams(vtmp->value, gms_pinf, gms_minf, gms_na);

            } else if (vtmp->placeholder && valid_vi(vtmp->vi)

                && !valid_ei(vtmp->ei) && vtmp->vi == objvar) {
               vtmp = vtmp->next_equ;
               continue;

            } else {
              error("[GMOexport] ERROR: variable '%s' is a placeholder but has type %d\n", 
                    ctr_printvarname(ctr_src, i), ctr_src->varmeta ? ctr_src->varmeta[i].type : -1);
              jacval[indx] = SNAN;
            }

            equidx[indx] = ctr_getcurrent_ei(ctr_src, vtmp->ei);

            assert(vtmp->isNL || isfinite(jacval[indx]));
            _debug_lequ(vtmp, ctr_src);
            assert(equidx[indx] < ctr_gms->m);

            isvarNL[indx] = vtmp->isNL ? 1 : 0;

            /* ----------------------------------------------------------
             * Update the NL status of the equation if the variable is
             * non-linear
             * TODO(Xhub) QUAD
             * ---------------------------------------------------------- */

            if (!NLequs[equidx[indx]] && vtmp->isNL) {
               NLequs[equidx[indx]] = true;
               DPRINT("Equation %d is NL due to variable #%d (#%d)\n",
                        equidx[indx], vidx, vtmp->vidx);
            }

            indx++;
         }
         vtmp = vtmp->next_equ;
      }

#ifdef A20240118
      /* TODO(xhub) factorize with previous */
      /* TODO(xhub) where does that crazyness come from? */
      for (size_t j = 0; j < idx_deleted; ++j) {
         CMatElt *me = deleted_equ[j];

         while (me) {
            if ((size_t)me->vi == i) {
               equidx[indx] = ctr_getcurrent_ei(ctr_src, me->ei);
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

      if (ctr_src->varmeta && ctr_src->varmeta[i].ppty == VarPerpToViFunction) {
        bool has_dual = false;
         assert(valid_ei(ctr_src->varmeta[i].dual));
         rhp_idx e_dual = ctr_getcurrent_ei(ctr_src, ctr_src->varmeta[i].dual);
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

      /*  this assert is not helpful with filtering */
//      assert((size_t)nb_equ >= indx);
      _debug_var_util(v, indx, jacval, vi, equidx, isvarNL, buffer);

      double multiplier = v->multiplier;
      double marginal = flip_marginal ? - multiplier : multiplier;
      double gms_marginal = dbl_to_gams(marginal, gms_pinf, gms_minf, gms_na);

      G_CHK_EXIT(gmoAddCol(gmo,
         /* type     */    v->type,
         /* lo       */    dbl_to_gams(v->bnd.lb, gms_pinf, gms_minf, gms_na),
         /* level    */    dbl_to_gams(v->value, gms_pinf, gms_minf, gms_na),
         /* up       */    dbl_to_gams(v->bnd.ub, gms_pinf, gms_minf, gms_na),
         /* marginal */    gms_marginal,
         /* basic    */    v->basis == BasisBasic ? 0 : 1,
         /* SOS      */    v->type == VAR_SOS1 || v->type == VAR_SOS2
                                      ? gms_dst->sos_group[vi] : 0,
         /* prior    */    0,
         /* scale    */    0.,
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

   S_CHECK_EXIT(rctr_compress_check_var(ctr_src->n, cdat->total_n, skip_var));

   for (unsigned j = 0; j < (unsigned) ctr_gms->m; ++j) {
       DPRINT("Equation %d: isNL = %s\n", j, NLequs[j] ? "true" : "false");
   }

   /* \TODO(xhub) this is a hack. It should be moved to another place when we
    * need to generate the pool. rmdl_compress should already have allocated
    * the pool ... */
   if (!ctr_src->pool) {
     A_CHECK_EXIT(ctr_src->pool, pool_new_gams());
   }

   /* ----------------------------------------------------------------------
    * Add the opcode to the GMO
    * ---------------------------------------------------------------------- */

   for (size_t i = 0; i < cdat->total_m; ++i) {

      if (_iscurrent_equ(ctr_src, i)) {

        int ei_gmo = ctr_getcurrent_ei(ctr_src, i);
        assert(!cdat->equs[i] || i == (unsigned)cdat->equs[i]->ei);
        assert(ei_gmo < ctr_gms->m);

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
           if (NLequs[ei_gmo]) {
              error("[GMOexport] ERROR: equation '%s' is declared as NL, but "
                    "has no nlcode\n", ctr_printequname(ctr_src, i));
              status = Error_Inconsistency;
              goto _exit;
          }
          continue;
       }

       /* ------------------------------------------------------------------
        * Filter the GAMS opcode: change variable index and/or insert variable
        * values
        * ------------------------------------------------------------------ */

        S_CHECK_EXIT(fops->transform_gamsopcode(fops->data, ei_gmo, instrs, args,
                                                codelen));


       /* TODO(xhub) this is a hack
        *
        * The reason for this bad piece of code is that the pool of the
        * original container may be modified
        */
       /* \TODO(xhub) rework this to use pool_get ?*/
       double* pool = ctr_src->pool->data;
       int poolen = (int)ctr_src->pool->len;

#ifndef NDEBUG
       for (unsigned j = 0; j < (unsigned)codelen; ++j) {
          _debug_check_nlcode(instrs[j], args[j], ctr_gms->n, poolen);
       }
#endif

       /* ------------------------------------------------------------------
        * The last instruction is (nlStore, eidx)  This value is used
        * inconsistently in GAMS, but we have to properly set the equation
        * index
        * ------------------------------------------------------------------ */

       assert(args[codelen-1] == 1 + ei_gmo && instrs[codelen-1] == nlStore);

       /* ------------------------------------------------------------------
        * Insert the opcode in the GMO
        * ------------------------------------------------------------------ */

       G_CHK_EXIT(gmoDirtySetRowFNLInstr(gmo, ei_gmo, codelen, instrs, args, NULL, pool, poolen));

      }

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
    S_CHECK_EXIT(vi_inbounds(objvar, cdat->total_n, __func__));
    assert(valid_vi(ctr_getcurrent_vi(ctr_src, objvar)));

    gmoObjVarSet(gmo, ctr_getcurrent_vi(ctr_src, objvar));
    RhpSense sense;
    S_CHECK_EXIT(rmdl_getsense(mdl, &sense));
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
   GAMS_CHECK1_EXIT(gmoCompleteData, gmo, buffer);

   /* Some checks  */
   assert(gmoM(gmo) == ctr_gms->m && gmoN(gmo) == ctr_gms->n);
   assert(!mdltype_isopt(probtype) || gmoGetObjName(gmo, buffer));

   bool do_expensive_check = optvalb(mdl, Options_Expensive_Checks);

   if (do_expensive_check) {

      if (ctr_gms->n > ctr_gms->m) {
         REALLOC_EXIT(equidx, int, ctr_gms->n);
         REALLOC_EXIT(jacval, double, ctr_gms->n);
         REALLOC_EXIT(isvarNL, int, ctr_gms->n);
      }

      for (int i = 0, m = ctr_gms->m, ei = 0; i < m; ++i, ++ei) {
         int nz, nlnz;
         gmoGetRowSparse(gmo, i, equidx, jacval, isvarNL, &nz, &nlnz);

         while (!valid_ei(ctr_src->rosetta_equs[ei])) { ei++; assert(ei < cdat->total_m); }
         CMatElt *ce = cdat->equs[ei];
         int nz_rhp = 0, nlnz_rhp = 0;
         while (ce) { nz_rhp++; if (ce->isNL) { nlnz_rhp++; } ce = ce->next_var; }

         if (nz != nz_rhp || nlnz != nlnz_rhp) {
            error("[GMOexport] ERROR for equation %s: GMO nz = %d, nlnz = %d; RHP "
                  "nz = %d, nlnz = %d\n", ctr_printequname(ctr_src, ei), nz, nlnz, nz_rhp,
                  nlnz_rhp);
            status = Error_RuntimeError;
         }
         printout(PO_VVV, "EQU %s involves %d variables (nl: %d)\n",
                  ctr_printequname(ctr_src, ei), nz, nlnz);

      }

      int * vidxs = equidx;

      for (int i = 0, n = ctr_gms->n, vi = 0; i < n; ++i, ++vi) {
         int nz, nlnz;
         gmoGetColSparse(gmo, i, vidxs, jacval, isvarNL, &nz, &nlnz);

         while (!valid_ei(ctr_src->rosetta_vars[vi])) { vi++; assert(vi < cdat->total_n); }
         CMatElt *ce = cdat->vars[vi];
         int nz_rhp = 0, nlnz_rhp = 0;
         while (ce) { nz_rhp++; if (ce->isNL) { nlnz_rhp++; } ce = ce->next_equ; }

         if (nz != nz_rhp || nlnz != nlnz_rhp) {

            error("[GMOexport] ERROR for variable %s: GMO nz = %d, nlnz = %d; RHP "
                  "nz = %d, nlnz = %d\n", ctr_printvarname(ctr_src, vi), nz, nlnz, nz_rhp,
                  nlnz_rhp);
            status = Error_RuntimeError;
         }
         printout(PO_VVV, "VAR %s is present in %d equations (nl: %d)\n",
                  ctr_printvarname(ctr_src, vi), nz, nlnz);

      }
   }

   if (status != OK) { goto _exit; }

   if (gmoM(gmo) != dctNRows(dct)) {
      error("[GMOexport] ERROR: There are %d equations in the DCT, but %d are "
            "expected\n", dctNRows(dct), gmoM(gmo));
      status = Error_RuntimeError;
      goto _exit;
   }

   if (gmoN(gmo) != dctNCols(dct)) {
      error("[GMOexport] ERROR: There are %d variables in the DCT, but %d are "
            "expected\n", dctNCols(dct), gmoN(gmo));
      status = Error_RuntimeError;
      goto _exit;
   }

   /* Complete the initialization of   */
   /*  TODO(Xhub) factorize code */
   gms_dst->owning_handles = true;
   MALLOC_EXIT(gms_dst->rhsdelta, double, ctr_src->m+1);
   gms_dst->initialized = true;

_exit:
   /*  FREE all */
   /* TODO(xhub) ave those 2?  */
   FREE(NLequs);
   FREE(equidx);
   FREE(jacval);
   FREE(isvarNL);
   rhp_idx_empty(&objequs);

   mdl->timings->gmo_creation += get_thrdtime() - start;

   return status;
}

int rmdl_resetequbasis(Container *ctr, double objmaxmin)
{
   unsigned objsense;
   struct ctrdata_rhp *cdat = (struct ctrdata_rhp *) ctr->data;

   for (unsigned i = 0; i < cdat->total_m; i++) {
      if (ctr->equs[i].basis != BasisBasic) {
         int gams_type;
         S_CHECK(cone_to_gams_relation_type(ctr->equs[i].cone, &gams_type));
         if (gams_type == gmoequ_E) {
            if (ctr->equs[i].multiplier * objmaxmin >= 0.) {
               ctr->equs[i].basis = BasisLower;
            } else {
               ctr->equs[i].basis = BasisUpper;
            }
         } else if (gams_type == gmoequ_G) {
            ctr->equs[i].basis = BasisLower;
         } else if (gams_type == gmoequ_L) {
            ctr->equs[i].basis = BasisUpper;
         } else {
            ctr->equs[i].basis = BasisSuperBasic;
         }
      }
   }

   return OK;
}

int rmdl_resetvarbasis(Container *ctr, double objmaxmin)
{
   unsigned objsense;
   double lev, lb, ub, minf, pinf, nan;
   struct ctrdata_rhp *model = (struct ctrdata_rhp *) ctr->data;

   ctr_getspecialfloats(ctr, &pinf, &minf, &nan);
   /* TODO: add option for that */
   double tol_bnd = 1e-8;

   for (size_t i = 0; i < model->total_n; i++) {
      if (ctr->vars[i].basis != BasisBasic) {
         lev = ctr->vars[i].value;
         lb = ctr->vars[i].bnd.lb;
         ub = ctr->vars[i].bnd.ub;

         if (lb != minf && ub != pinf) {
            if (fabs(lb - ub) < tol_bnd) {

               /* ---------------------------------------------------------
                * Fixed variable
                * --------------------------------------------------------- */

               if (ctr->vars[i].multiplier * objmaxmin >= 0.) {
                  ctr->vars[i].basis = BasisLower;
               } else {
                  ctr->vars[i].basis = BasisUpper;
               }
            } else {

               /* ---------------------------------------------------------
                * Doubly-bounded variable
                * --------------------------------------------------------- */

               if (fabs(lev - lb) < tol_bnd) {
                  ctr->vars[i].basis = BasisLower;
               } else if (fabs(lev - ub) < tol_bnd) {
                  ctr->vars[i].basis = BasisUpper;
               } else {
                  ctr->vars[i].basis = BasisSuperBasic;
               }
            }
         } else if (lb != minf) {

            /* ------------------------------------------------------------
             * Lower bounded
             * ------------------------------------------------------------ */
            if (fabs(lev - lb) < tol_bnd) {
               ctr->vars[i].basis = BasisLower;
            } else {
               ctr->vars[i].basis = BasisSuperBasic;
            }
         } else if (ub != pinf) {

            /* ------------------------------------------------------------
             * Upper bounded
             * ------------------------------------------------------------ */

            if (fabs(lev - ub) < tol_bnd) {
               ctr->vars[i].basis = BasisUpper;
            } else {
               ctr->vars[i].basis = BasisSuperBasic;
            }
         } else {

            /* ------------------------------------------------------------
             * Free
             * ------------------------------------------------------------ */
            ctr->vars[i].basis = BasisSuperBasic;
         }
      }
   }

   return OK;
}


