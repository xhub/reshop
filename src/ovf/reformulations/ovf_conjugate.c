#include <assert.h>
#include <float.h>
#include <math.h>
#include <stdbool.h>
#include <string.h>

#include "cmat.h"
#include "compat.h"
#include "consts.h"
#include "container.h"
#include "ctr_rhp.h"
#include "equ_modif.h"
#include "mdl_timings.h"
#include "nltree.h"
#include "nltree_priv.h"
#include "filter_ops.h"
#include "generators.h"
#include "lequ.h"
#include "macros.h"
#include "mathprgm.h"
#include "mdl.h"
#include "ctrdat_rhp.h"
#include "ctr_rhp_add_vars.h"
#include "open_lib.h"
#include "ovf_common.h"
#include "ovf_conjugate.h"
#include "ovf_options.h"
#include "ovfinfo.h"
#include "printout.h"
#include "rhp_LA.h"
#include "status.h"
#include "timings.h"
#include "win-compat.h"

//#define CONJ_DEBUG(str, ...) printf("[conjugate] DEBUG: " str "\n", __VA_ARGS__)
#define CONJ_DEBUG(...)
//#define DEBUG_OVF_CONJUGATE

/* TODO(xhub) no enum CONES, but char  */
tlsvar int (*compute_v_repr)(void *, double *, enum cone *, void*) = NULL;
tlsvar void* libvrepr_handle = NULL;

#if defined(_WIN32) && defined(_MSC_VER)
#define WIN32_LEAN_AND_MEAN
#define VC_EXTRALEAN
#include <windows.h>

  void cleanup_vrepr(void)
  {
      if (libvrepr_handle)
      {
        FreeLibrary(libvrepr_handle);
        libvrepr_handle = NULL;
      }
  }

#elif defined(__GNUC__) & !defined(__APPLE__)
#include <dlfcn.h>

 __attribute__ ((destructor)) static void cleanup_vrepr(void)
  {
    if (libvrepr_handle)
    {
       dlclose(libvrepr_handle);
       libvrepr_handle = NULL;
    }
  }
#endif

static int _load_vrepr(void)
{
   if (!compute_v_repr) {
      if (!libvrepr_handle) {
         libvrepr_handle = open_library(DLL_FROM_NAME("vrepr"), 0);

         if (!libvrepr_handle) {
            const char *ldpath = getenv("RHP_LDPATH");
            if (ldpath) {
               size_t ldpath_len = strlen(ldpath);
               size_t dll_len = strlen(DLL_FROM_NAME("vrepr"));

               char *dll_fullname = malloc(sizeof(char) * (1 + dll_len + ldpath_len));
               if (dll_fullname) {
                  strcpy(dll_fullname, ldpath);
                  strcat(dll_fullname, DLL_FROM_NAME("vrepr"));

                  libvrepr_handle = open_library(dll_fullname, 0);
                  free(dll_fullname);
               }
            }
         }

         if (!libvrepr_handle) {
            errormsg("[conjugate] ERROR: could not find "DLL_FROM_NAME("vrepr")". "
                     "Some functionalities may not be available\n");
            return Error_SystemError;
         }
      }

      *(void **)(&compute_v_repr) = get_function_address(libvrepr_handle, "compute_V_repr");
      if (!compute_v_repr) {
         errormsg("%s :: Could not find function named compute_V_repr in "
                  DLL_FROM_NAME("vrepr")". Some functionalities may not "
                  "be available\n");
         return Error_SystemError;
      }
   }

   return OK;
}


static int _add_gen_type(struct gen_data * restrict dat, rhp_idx vi, enum cone cone,
                         Lequ **les, NlTree **trees, Container *ctr,
                         MathPrgm *mp, const double *csts, unsigned size)
{

   /* -----------------------------------------------------------------------
    * Iterate over the generators and add
    *   <gen_i, F(x)> - vi   in   cone   if vi is valid
    *   │<gen_i, F(x)>       in   cone   otherwise
    * ----------------------------------------------------------------------- */

   for (unsigned i = 0, len = dat->size; i < len; ++i) {
      Equ *e;
      rhp_idx ei_new = IdxNA;

      S_CHECK(rctr_add_equ_empty(ctr, &ei_new, &e, ConeInclusion, cone));

      if (mp) {
         S_CHECK(mp_addconstraint(mp, ei_new));
      }

      if (valid_vi(vi)) {
         S_CHECK(lequ_add(e->lequ, vi, -1.));
      }

      double * restrict vec = dat->val[i];

      /* -------------------------------------------------------------------
       * For each component of the generator vec, get the corresponding
       * component of the argument
       * ------------------------------------------------------------------- */

      /* TODO(xhub) merge this with equ_add_dot_prod_cst_x  */
      for (unsigned j = 0; j < size; ++j) {
         double v = vec[j];

         if (fabs(v) > DBL_EPSILON) {

            /* Add the constant term if it exists */
            assert(!csts || isfinite(csts[j]));

            if (csts && fabs(csts[j]) > DBL_EPSILON) {
               equ_add_cst(e, v*csts[j]);
            }

            /*  Linear part: le could be NULL ... */
            Lequ *le = les[j];
            for (unsigned k = 0, lenle = le ? le->len : 0; k < lenle; ++k) {
               S_CHECK(lequ_add_unique(e->lequ, le->vis[k], v*le->coeffs[k]));
            }

            /*  Non-linear part */
            /*  \TODO(xhub) this should be optional*/
            if (!trees[j] || !trees[j]->root) { continue; }

            if (!e->tree) {
               S_CHECK(nltree_bootstrap(e, 4, 3));
            }

            /* TODO(xhub) this should be in a dedicated function  */
            NlNode *mul_node = NULL;
            NlNode **addr = &mul_node;
            S_CHECK(nltree_mul_cst(trees[j], &addr, ctr->pool, v));

            if (mul_node) {
               mul_node->children[0] = trees[j]->root;
            } else {
               mul_node = trees[j]->root;
            }
            assert(mul_node);
            /* TODO(xhub) minus madness */
            S_CHECK(rctr_equ_add_nlexpr(ctr, ei_new, mul_node, NAN));

         }
      }
      /* TODO: for t >= ... check that at at least one variable, besides t,
       * is present in the constraint. If not, this could be transformed
       * into a lower/upper  bound*/
      /*  TODO(xhub) provide an easier way to maintain consistency */
      S_CHECK(cmat_sync_lequ(ctr, e));
   }

   return OK;
}

int ovf_conjugate(Model *mdl, enum OVF_TYPE type, union ovf_ops_data ovfd)
{
   double start = get_thrdtime();

   const struct ovf_ops *op;
   int status = OK;

   /* Init data that would be free in the exit case  */
   Lequ **les = NULL;
   NlTree **trees = NULL;
   rhp_idx *equ_idx = NULL;
   double *coeffs = NULL, *csts = NULL, *s = NULL, *b = NULL;

   struct generators *gen = NULL;

   SpMat A, D, J, B;
   rhpmat_null(&A);
   rhpmat_null(&D);
   rhpmat_null(&J);
   rhpmat_null(&B);

   unsigned n_constr, n_u = 0;

   Container *ctr = &mdl->ctr;
   RhpContainerData *cdat = (RhpContainerData *)ctr->data;

   switch (type) {
   case OvfType_Ovf:
      op = &ovfdef_ops;
      break;
   default:
      TO_IMPLEMENT("user-defined OVF is not implemented");
   }

   int ovf_vidx = op->get_ovf_vidx(ovfd);
   if (!valid_vi(ovf_vidx)) {
      error("%s :: the OVF variable is not set! Value = %d\n", __func__, ovf_vidx);
      return Error_InvalidValue;
   }

   char ovf_name[SHORT_STRING];
   unsigned ovf_namelen;
   S_CHECK_EXIT(ctr_copyvarname(ctr, ovf_vidx, ovf_name, SHORT_STRING));
   ovf_namelen = strlen(ovf_name);

   MathPrgm *mp = NULL;
   RhpSense sense;

   S_CHECK_EXIT(ovf_get_mp_and_sense(mdl, ovf_vidx, &mp, &sense));

   bool has_set;
   struct ovf_ppty ovf_ppty;
   op->get_ppty(ovfd, &ovf_ppty);

   /* ---------------------------------------------------------------------
    * Test compatibility between OVF and problem type
    * --------------------------------------------------------------------- */

   S_CHECK_EXIT(ovf_compat_types(op->get_name(ovfd), ovf_name, sense, ovf_ppty.sense));

   /* ---------------------------------------------------------------------
    * 1. Inject new variables: t, z ≥ 0, w free
    *
    * 2. Replace the rho variable by t
    *
    * 3. Add constraints
    *                      t ≥ <v_i, F(x)>      with v_i any vertex
    *                      <r_i, F(x)> ≤ 0      r_i any ray
    *                      <l_i, F(x)> = 0      l_i any line
    *   of the set Y.
    * --------------------------------------------------------------------- */

   /* We look for A and s such that Ax - s belongs to K */
   S_CHECK_EXIT(op->get_set(ovfd, &A, &s, false));
   S_CHECK_EXIT(op->get_D(ovfd, &D, &J));

   if (A.ppty) {
      has_set = true;
      S_CHECK_EXIT(rhpmat_get_size(&A, &n_u, &n_constr));
      struct ctrmem CTRMEM working_mem = {.ptr = NULL, .ctr = ctr};
      A_CHECK(working_mem.ptr, ctr_getmem(ctr, n_constr * sizeof(enum cone)));
      enum cone *c = (enum cone *)working_mem.ptr;
      void *cone_data;
      /*  TODO(xhub) specialized call? */
      for (unsigned i = 0; i < n_constr; ++i) {
         S_CHECK_EXIT(op->get_cone(ovfd, i, &c[i], &cone_data));
      }
      S_CHECK_EXIT(_load_vrepr());
      double start_PPL = get_thrdtime();

      S_CHECK_EXIT(compute_v_repr(&A, s, c, &gen));

      simple_timing_add(&mdl->timings->reformulation.CCF.conjugate.PPL, get_thrdtime() - start_PPL);

      // generators_savetxt(gen);
#ifdef DEBUG_OVF_CONJUGATE
      generators_print(gen);
#endif
      if (gen->vertices.size == 0 && gen->rays.size == 0 &&
         gen->lines.size == 0) {
         error("%s :: no generator found for the set U\n",
                  __func__);
         status = Error_UnExpectedData;
         goto _exit;
      }
   } else {
      has_set = false;
      if (D.ppty) {
         unsigned dummy;
         S_CHECK_EXIT(rhpmat_get_size(&D, &dummy, &n_u));
      } else {
         error("%s :: Fatal Error: no OVF set given and no quadratic"
                 "part -> the OVF function is unbounded!\n", __func__);
         status = Error_EMPIncorrectInput;
         goto _exit;
      }
   }

   unsigned nb_vars = 0;
   if (has_set) {
      /* t is only necessary whenever we have at least one vertex  */
      nb_vars += gen->vertices.size > 0 ? 1 : 0;
   }
   if (ovf_ppty.quad) {
      nb_vars += n_u;
   }

   S_CHECK_EXIT(rctr_reserve_vars(ctr, nb_vars));

   /* ---------------------------------------------------------------------
    * If we want to initialize the values of the new variables, this is the
    * start for the new variables
    * --------------------------------------------------------------------- */
   unsigned start_new_vars = cdat->total_n;

   /* Add the t variable */
   rhp_idx t_idx = IdxNA;

   if (has_set && gen->vertices.size > 0) {
      char *t_name;
      NEWNAME(t_name, ovf_name, ovf_namelen, "_t");
      cdat_varname_start(cdat, t_name);
      Avar v;
      S_CHECK_EXIT(rctr_add_free_vars(ctr, 1, &v));
      t_idx = avar_fget(&v, 0);
      if (mp) {
         S_CHECK_EXIT(mp_addvar(mp, t_idx));
      }
      cdat_varname_end(cdat);
   }

   /* Add the w variable if needed  */
   rhp_idx w_start = IdxNA; /* init just to silence compiler warning */
   Avar w_var;
   if (ovf_ppty.quad) {
      char *w_name;
      NEWNAME(w_name, ovf_name, ovf_namelen, "_w");
      cdat_varname_start(cdat, w_name);
      S_CHECK_EXIT(rctr_add_free_vars(ctr, n_u, &w_var));
      if (mp) {
         S_CHECK_EXIT(mp_addvars(mp, &w_var));
      }
      cdat_varname_end(cdat);
      S_CHECK_EXIT(avar_get(&w_var, 0, &w_start));
   }

   /* Reserve equations (new obj function + constraints) */
   /* TODO(xhub) off by 1 here */
   S_CHECK_EXIT(rctr_reserve_equs(ctr, 2 + (gen ? gen->vertices.size + gen->rays.size + gen->lines.size : 0)));

   /* ---------------------------------------------------------------------
    * 2. Replace the rho variable by   0.5 <w, w> + t 
    * --------------------------------------------------------------------- */

   void *ptr = NULL;
   unsigned counter = 0;

   do {
      rhp_idx ei_new;
      double ovfvar_coeff;

      S_CHECK_EXIT(ovf_replace_var(mdl, ovf_vidx, &ptr, &ovfvar_coeff, &ei_new, 1));
      Equ *e_new = &ctr->equs[ei_new];

      /* Replace var by lin */
      if (valid_vi(t_idx)) {
         S_CHECK_EXIT(rctr_equ_addnewvar(ctr, e_new, t_idx, ovfvar_coeff));
      }

      /* Patch the opcode to add \| w \|^2_2 */
      if (ovf_ppty.quad) {
         S_CHECK_EXIT(rctr_equ_add_quadratic(ctr, e_new, &J, &w_var, ovfvar_coeff));
      }

      counter++;
   } while(ptr);

   S_CHECK_EXIT(rctr_reserve_equs(ctr, counter));

   /* ---------------------------------------------------------------------
    * 2.2 Add an equation to evaluate rho
    * --------------------------------------------------------------------- */

   /* ---------------------------------------------------------------------
    * If we want to initialize the values of the new variable, this is the
    * start for the new equations
    * --------------------------------------------------------------------- */
   unsigned start_new_equ = cdat->total_m;

   /* TODO(xhub) reuse the code before to also store that*/
   Equ *e_rho;
   rhp_idx ei_rho = IdxNA;
   S_CHECK_EXIT(rctr_add_equ_empty(ctr, &ei_rho, &e_rho, Mapping, CONE_NONE));

   if (valid_vi(t_idx)) {
      S_CHECK_EXIT(equ_add_newlvar(e_rho, t_idx, 1.));
   }
   /* Patch the opcode  */
   if (ovf_ppty.quad) {
      S_CHECK_EXIT(rctr_equ_add_quadratic(ctr, e_rho, &J, &w_var, 1.));
   }
   S_CHECK_EXIT(equ_add_newlvar(e_rho, ovf_vidx, -1.));
   S_CHECK_EXIT(cmat_sync_lequ(ctr, e_rho));

   S_CHECK_EXIT(rctr_add_eval_equvar(ctr, ei_rho, ovf_vidx));

   /* \TODO(xhub) this is a HACK to remove this equation from the main model.
    */
   S_CHECK_EXIT(rctr_deactivate_var(ctr, ovf_vidx));
   S_CHECK_EXIT(rctr_deactivate_equ(ctr, ei_rho));

   /* ---------------------------------------------------------------------
    * 3. Add constraints
    * 3.1 <v_i, B*F(x)+b - Ds>  ≤  t  for all vertices (v_i)
    * 3.2 <r_i, B*F(x)+b - Ds>  ≤  0  for all rays (r_i)
    * 3.3 <l_i, B*F(x)+b - Ds>  =  0  for all lines (l_i)
    * --------------------------------------------------------------------- */

    /* ---------------------------------------------------------------------
     * 3.0 Build B*F(x)+b - Ds as an array of lequ/tree
     * --------------------------------------------------------------------- */

   CALLOC_EXIT(les, Lequ *, n_u);
   CALLOC_EXIT(trees, NlTree *, n_u);
   MALLOC_EXIT(csts, double, n_u);

   /* Check whether we have B or b  */
   S_CHECK_EXIT(op->get_affine_transformation(ovfd, &B, &b));

   /* Get the indices of F(x)  */
   Avar *ovf_args;
   S_CHECK_EXIT(op->get_args(ovfd, &ovf_args));
   unsigned n_args = avar_size(ovf_args);

   S_CHECK_EXIT(op->get_mappings(ovfd, &equ_idx));
   S_CHECK_EXIT(ovf_process_indices(mdl, ovf_args, equ_idx));
   S_CHECK_EXIT(op->get_coeffs(ovfd, &coeffs));

   if (B.ppty) {
      unsigned n_u2, n_args2;
      S_CHECK_EXIT(rhpmat_get_size(&B, &n_args2, &n_u2));

      if (n_u != n_u2) {
         error("%s :: incompatible size: the number of row of B and the ambiant"
               " space of U should have the same there are %u rows in B and U is"
               " in R^%u\n", __func__, n_u2, n_u);
         status = Error_Inconsistency;
         goto _exit;
      }

      if (n_args2 != n_args) {
         error("%s :: incompatible size: the number of arguments (%u) and the "
               "number of columns in B (%u) should be the same\n", __func__,
               n_args, n_args2);
         status = Error_Inconsistency;
         goto _exit;
      }
   }

   Lequ *lequ;
   NlTree *eqtree;

   /* ---------------------------------------------------------------------
    * Build BF(x) + b - Ds.
    * output: array of lequ (les), array of tree and rhs
    */
   for (unsigned i = 0; i < n_u; ++i) {

      /* Add the sizes */
      size_t size_d = 0;

      if (ovf_ppty.quad) {
         EMPMAT_GET_CSR_SIZE(D, i, size_d);
      }

      unsigned *arg_indx;
      unsigned args_indx_len;
      unsigned single_idx;
      double single_val;
      double *Brow = NULL;
      S_CHECK_EXIT(rhpmat_row_needs_update(&B, i, &single_idx, &single_val, &args_indx_len,
                         &arg_indx, &Brow));

      if (args_indx_len == 0) {
         printout(PO_DEBUG, "[Warn] %s :: row %d is empty\n", __func__, i);
         csts[i] = NAN;
         continue;
      }
      size_t size_new_lequ = size_d;

      for (unsigned j = 0; j < args_indx_len; ++j) {
         rhp_idx eqidx = equ_idx[arg_indx[j]];
         if (!valid_ei(eqidx)) { size_new_lequ++; continue; }
         lequ = ctr->equs[eqidx].lequ;
         if (lequ) { size_new_lequ += lequ->len - 1; }
      }

      Lequ *le;
      A_CHECK_EXIT(les[i], lequ_new(size_new_lequ));
      le = les[i];

      NlTree *tree;
      /* TODO: tune constant */
      A_CHECK_EXIT(trees[i], nltree_alloc(4));
      tree = trees[i];

      size_t offset = 0;

      /* ---------------------------------------------------------------------
       * First add -Dw
       * ---------------------------------------------------------------------*/

      if (ovf_ppty.quad) {
         COPY_VALS(D, i, le->coeffs, le->vis, offset, size_d, w_start);
         /* \TODO(xhub) is there a better way to apply the minus */
         for (unsigned j = offset; j < offset+size_d; ++j) {
            le->coeffs[j] = -le->coeffs[j];
         }
         offset += size_d;
      }
      le->len = offset;

      /* Store the constant term */
      if (b) { csts[i] = b[i]; } else { csts[i] = 0.; }

      /* --------------------------------------------------------------------
       *  Deal with B*F(x) + b     row by row
       * -------------------------------------------------------------------- */

      for (unsigned j = 0; j < args_indx_len; ++j) {
         CONJ_DEBUG("row %d with argument #%d (pos %d) and value %e\n", i, arg_indx[j], j, Brow ? Brow[j] : 1.);
         rhp_idx eqidx = equ_idx[arg_indx[j]];
         rhp_idx vidx = avar_fget(ovf_args, arg_indx[j]);
         /* If the argument is a variable  */
         if (!valid_ei(eqidx)) {
            CONJ_DEBUG("variable %d in argument #%d\n", vidx, i);
            assert(!lequ_debug_hasvar(le, vidx));
            le->coeffs[offset] = 1.;
            le->vis[offset] = vidx;
            le->len++;
            offset++;
            continue;
         }

         /* The argument refers to an equation that we will copy  */
         lequ = ctr->equs[eqidx].lequ;

         CONJ_DEBUG("equation %d in argument #%d\n", eqidx, i);
         CONJ_DEBUG("placeholder variable coeff = %e; B_ij = %e\n",
                   coeffs[arg_indx[j]], Brow ? Brow[j] : 1.);
         double coeff = coeffs[arg_indx[j]];
         double coeffp;

         if (Brow) { coeffp = -Brow[j]/coeff; }
         else { coeffp = -1./coeff; }

         /* copy the constant */
         double cst = equ_get_cst(&ctr->equs[eqidx]);
         if (!isfinite(cst)) {
            error("[conjugate] ERROR: equation %s has non-finite constant",
                  ctr_printequname(ctr, eqidx));
            return Error_RuntimeError;
         }

         csts[i] += coeffp*cst;

         if (fabs(coeffp - 1.) < DBL_EPSILON) {
            /* Could use memcpy or BLAS if we know where the argument is */
            for (unsigned l = 0, k = offset; l < lequ->len; ++l) {
               if (lequ->vis[l] == vidx) continue;
               assert(!lequ_debug_hasvar(le, lequ->vis[l]));
               le->coeffs[k] = lequ->coeffs[l];
               le->vis[k] = lequ->vis[l];
               k++;
            }
         } else {

            if (fabs(coeffp) < DBL_EPSILON) {
               error("%s :: Error: the coefficient of variable %s "
                        "(%d) in equation %s (%d) is too small : %e", __func__,
                        ctr_printvarname(ctr, vidx), vidx,
                        ctr_printequname(ctr, eqidx), eqidx, coeffp);
            }

            /* \TODO(xhub) option to use BLAS */
            for (unsigned l = 0, k = offset, len = lequ->len; l < len; ++l) {
               /*  Do not use the placeholder variable */
               if (lequ->vis[l] == vidx) continue;
               assert(!lequ_debug_hasvar(le, lequ->vis[l]));
               le->coeffs[k] = coeffp*lequ->coeffs[l];
               le->vis[k] = lequ->vis[l];

               CONJ_DEBUG("Adding coefficient %e for variable %s (%d) at index %u",
                        le->values[k], ctr_printvarname(ctr, le->indices[k]), le->indices[k], k);
               k++;
            }
         }

         offset += lequ->len - 1;
         le->len += lequ->len - 1;

         /* If there is a NL part */
         S_CHECK_EXIT(rctr_getnl(ctr, &ctr->equs[eqidx]));
         eqtree  = ctr->equs[eqidx].tree;

         if (eqtree && eqtree->root) {

            S_CHECK_EXIT(nltree_add_nlexpr(tree, eqtree->root, ctr->pool, coeffp));

         }
      }
   }

   /* ---------------------------------------------------------------------
    * Main loop to add the equations:
    *
    * --------------------------------------------------------------------- */
   char *eqn_name;
   NEWNAME(eqn_name, ovf_name, ovf_namelen, "_set");
   cdat_equname_start(cdat, eqn_name);

   /* ---------------------------------------------------------------------
    * if OVF is sup, then   < v_i, u > ≤ t;   < r_i, u > ≤ 0;   < l_i, u > = 0
    * otherwise             < v_i, u > ≥ t;   < r_i, u > ≥ 0;   < l_i, u > = 0
    * --------------------------------------------------------------------- */

   enum cone cone_vert_rays;
   switch (ovf_ppty.sense) {
   case RhpMax:
     cone_vert_rays = CONE_R_MINUS; break;
   case RhpMin:
     cone_vert_rays = CONE_R_PLUS; break;
   default: status = error_runtime(); goto _exit;
   }

   /*  ---------------------------------------------------------------------
    *  if the only generator is 1 vertex, put an equality constraint.
    *  This kludge is needed because of the expected value OVF
    *
    *  TODO(xhub) remove this hack and substitute t_idx with the expression
    *  --------------------------------------------------------------------- */

   if (has_set) {
      if (gen->vertices.size == 1 && gen->rays.size == 0 && gen->lines.size == 0) {
        cone_vert_rays = CONE_0;
      }

      S_CHECK_EXIT(_add_gen_type(&gen->vertices, t_idx, cone_vert_rays, les, trees, ctr, mp, csts, n_u));
      S_CHECK_EXIT(_add_gen_type(&gen->rays, IdxNA, cone_vert_rays, les, trees, ctr, mp, csts, n_u));
      S_CHECK_EXIT(_add_gen_type(&gen->lines, IdxNA, CONE_0, les, trees, ctr, mp, csts, n_u));
   }

   /* All the equation have been added  */
   cdat_equname_end(cdat);

   /* ---------------------------------------------------------------------
    * Deallocate memory
    * --------------------------------------------------------------------- */

   unsigned nb_equs = cdat->total_m - start_new_equ;

   if (O_Ovf_Init_New_Variables) {
      Avar ovf_vars, ovfvar;
      avar_setcompact(&ovf_vars, nb_vars, start_new_vars);
      avar_setcompact(&ovfvar, 1, ovf_vidx);
      Aequ ovf_equs;
      aequ_setcompact(&ovf_equs, nb_equs, start_new_equ);

      for (unsigned i = 0; i < n_args; ++i) {
         if (!valid_ei(equ_idx[i])) {
            printout(PO_INFO, "%s :: precomputing the value of the new variables"
                             " is only implemented when the arguments of the OVF"
                             " can be substituted\n", __func__);
            goto _exit;
         }
      }

      struct filter_subset* fs;
      Avar var_c[] = { ovf_vars, ovfvar };
      Aequ eqn_c[] = { ovf_equs };
      size_t vlen = sizeof(var_c)/sizeof(Avar);
      size_t elen = sizeof(eqn_c)/sizeof(Aequ);

      /* TODO(Xhub) OVF_SUP: check RHP_MIN */
      struct mp_descr descr = { .mdltype = MdlType_qcp, .sense = RhpMin,
                                .objvar = ovf_vidx, .objequ = ei_rho };

      A_CHECK_EXIT(fs, filter_subset_new(vlen, var_c, elen, eqn_c, &descr));

      S_CHECK_EXIT(cdat_add_subctr(cdat, fs));
   }


_exit:
   FREE(csts);

   op->trimmem(ovfd);

   /* This has been allocated, what about OVF?*/
   FREE(s);
   FREE(b);
   rhpmat_free(&A);
   rhpmat_free(&D);
   rhpmat_free(&J);
   rhpmat_free(&B);

   generators_dealloc(gen);

   if (les) {
     for (unsigned i = 0; i < n_u; ++i) {
       lequ_free(les[i]);
       /*  TODO(xhub) qequ */
     }
   }

   if (trees) {
     for (unsigned i = 0; i < n_u; ++i) {
       nltree_dealloc(trees[i]);
     }
   }

   FREE(les);
   FREE(trees);

   simple_timing_add(&mdl->timings->reformulation.CCF.conjugate.stats, get_thrdtime() - start);

   return status;
}

