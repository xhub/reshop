#include "reshop_config.h"
#include "asnan.h"
#include "asprintf.h"

#include <string.h>

#include "cmat.h"
#include "container.h"
#include "compat.h"
#include "ctr_gams.h"
#include "ctrdat_rhp_priv.h"
#include "ctr_rhp.h"
#include "empdag.h"
#include "empdag_uid.h"
#include "empinfo.h"
#include "equ.h"
#include "equ_modif.h"
#include "nltree.h"
#include "equvar_helpers.h"
#include "equvar_metadata.h"
#include "filter_ops.h"
#include "fooc.h"
#include "fooc_data.h"
#include "fooc_priv.h"
#include "lequ.h"
#include "macros.h"
#include "mathprgm.h"
#include "mdl.h"
#include "mdl_data.h"
#include "ctrdat_rhp.h"
#include "rhp_alg.h"
#include "rmdl_debug.h"
#include "printout.h"
#include "reshop_data.h"
#include "sd_tool.h"
#include "status.h"
#include "timings.h"
#include "toplayer_utils.h"

/** @file  fooc.c */


#ifndef NDEBUG
#define FOOC_DEBUG(str, ...) trace_fooc("[fooc] DEBUG: " str "\n", __VA_ARGS__)
#else
#define FOOC_DEBUG(...)
#endif


struct equ_inh_names {
   IdxArray cons_src;
};


/* TODO: LOW rethink this */
UNUSED static const char* empdagc_err(unsigned len)
{
  switch (len) {
  case IdxEmpDagChildNotFound:
    return "EMPDAG child not found";
  case IdxInvalid:
    return "Invalid index";
  default:
    return "Unknown error";
  }
}

static inline int alloc_memory_(Model *mdl, const size_t nl_cons_size,
                                const MathPrgm *mp, struct sd_tool ***sd_cequ,
                                bool **var_in_mp) {
  if (nl_cons_size > 0) {
    /* CALLOC prevents some weird freeing  */
    CALLOC_(*sd_cequ, struct sd_tool *, nl_cons_size);
  }

  if (mp) {
    size_t total_n = ctr_nvars_total(&mdl->ctr);
    CALLOC_(*var_in_mp, bool, total_n)
  }

  return OK;
}

static void err_objvar_(const Model *mdl, const MathPrgm *mp, rhp_idx objvar,
                        rhp_idx objequ)
{
   const EmpDag *empdag = &mdl->empinfo.empdag;
   error("[FOOC] ERROR in %s model '%.*s' #%u: the MP(%s) has both an objective "
         "variable '%s' and an objective function '%s'. This is unsupported, "
         "there must be only one of these.\n", mdl_fmtargs(mdl),
         empdag_getmpname(empdag, mp->id), mdl_printvarname(mdl, objvar),
         mdl_printequname(mdl, objequ));
}

static NONNULL
int getequ_curidx(Model *mdl_src, rhp_idx ei_src, const Rosettas *r, Equ *e)
{
   
   Model *mdl = mdl_src;
   rhp_idx ei = ei_src;
   unsigned depth = 0;

   NlTree *tree = NULL;

   while (mdl) {
      switch (mdl->backend) {
      case RHP_BACKEND_RHP: {
         rhp_idx ei_up = cdat_equ_inherited(mdl->ctr.data, ei);

         if (valid_ei(ei_up)) {
            ei = ei_up;
            mdl = mdl->mdl_up;
            assert(valid_ei_(ei_up, ctr_nequs_total(&mdl->ctr), __func__));
            depth++;
            assert(mdl);
         } else {
            assert(ei_up == IdxNotFound);
            goto end;
         }

         break;
      }

      case RHP_BACKEND_GAMS_GMO: {
         int len, *instrs, *args;

         S_CHECK(gctr_getopcode(&mdl->ctr, ei, &len, &instrs, &args));
 
         if (len > 0) {
            A_CHECK(tree, nltree_buildfromgams(len, instrs, args));
         }

         /* No need to free instrs or args, it comes from a container workspace */
         goto end;
      }
      case RHP_BACKEND_JULIA:
      case RHP_BACKEND_AMPL:
         goto end;
         break;
      default:
         error("%s :: ERROR: unsupported backend %s", __func__,
               backend_name(mdl->backend));
      }
   }

end:

   assert(depth < r->mdls.len);
   rhp_idx * restrict rosetta_vars = &r->data[r->rosetta_starts.arr[depth]];

   Equ *e_up = &mdl->ctr.equs[ei];

   memcpy(e, e_up, sizeof(Equ));

   /* TODO GITLAB #105 */
   e->lequ = lequ_dup_rosetta(e_up->lequ, rosetta_vars);

   if (tree) {
      S_CHECK(nltree_apply_rosetta(tree, rosetta_vars));
      e->tree = tree;
   } else {
      e->tree = nltree_dup_rosetta(e_up->tree, rosetta_vars);
      assert(!e_up->tree || e->tree);
   }

   e->idx = IdxNA;

   return OK;
}

static inline
int setup_equvarnames(struct equ_inh_names * equnames, FoocData *fooc_dat,
                      RhpContainerData *cdat_mcp, const Model* mdl_fooc)
{
   rhp_idx_init(&equnames->cons_src);

   size_t n_cons = fooc_dat->info->n_constraints;
   S_CHECK(rhp_idx_reserve(&equnames->cons_src, n_cons));

   Aequ e, e_src;
   aequ_setcompact(&e, n_cons, fooc_dat->ei_cons_start);
   S_CHECK(aequ_extendandown(&cdat_mcp->equname_inherited.e, &e));
   aequ_setandownlist(&e_src, n_cons, equnames->cons_src.arr);
   S_CHECK(aequ_extendandown(&cdat_mcp->equname_inherited.e_src, &e_src));
 

   struct vnames *vnames_var = &cdat_mcp->var_names.v;
   vnames_var->type = VNAMES_MULTIPLIERS;
   vnames_var->start = fooc_dat->vi_mult_start;
   vnames_var->end = fooc_dat->vi_mult_start + n_cons - 1;

   struct vnames *vnames_equ = &cdat_mcp->equ_names.v;
   assert(vnames_equ->type == VNAMES_UNSET);

   vnames_equ->type = VNAMES_FOOC_LOOKUP;
   vnames_equ->start = fooc_dat->ei_F_start;
   vnames_equ->end = fooc_dat->ei_cons_start-1;

   A_CHECK(vnames_equ->fooc_lookup, vnames_lookup_new(fooc_dat->info->n_primalvars,
                                                      mdl_fooc,
                                                      fooc_dat->ei_F2vi_primal));

   return OK;
}

#if 0
static inline int nl_copy_(const Model *mdl_src, const Fops * const fops,
                           Equ *esrc, Equ *edst, Model *mdl_dst)
{
  if (fops) {
    S_CHECK(fops->transform_nltree(fops->data, esrc, edst));
    /* Keep the model representation consistent */

    assert(edst->tree && edst->tree->v_list && edst->tree->v_list->pool);
    Avar v;
    avar_setlist(&v, edst->tree->v_list->idx, edst->tree->v_list->pool);

    assert(valid_vi(edst->idx));
    S_CHECK(cmat_add_vars_to_equ(&mdl_dst->ctr, edst->idx, &v, NULL, true));

  } else {
    /* This hack is necessary since we change the equation index */
    rhp_idx ei_bck = edst->idx;
    edst->idx = esrc->idx;
    /* TODO(Xhub) lazy loading of tree ? */
    S_CHECK(rctr_getnl(&mdl_src->ctr, edst));
    edst->idx = ei_bck;
  }

  return OK;
}


static inline int copy_functional_from_mdl_(Model *restrict ctr_mcp,
                                            Model *restrict ctr_src) {
  TO_IMPLEMENT("Copy from VI model not yet implemented");
}

static inline int copy_functional_from_mp_(Model *restrict mdl_mcp,
                                           Model *restrict mdl_src,
                                           MathPrgm *restrict mp) {

   Container * restrict ctr_mcp = &mdl_mcp->ctr;
   Container * restrict ctr_src = &mdl_src->ctr;
   rhp_idx *rosetta_equs = ctr_src->rosetta_equs;
   Fops *fops;

   if (mdl_is_rhp(mdl_src)) {
      struct ctrdata_rhp *ctrdat_src = (struct ctrdata_rhp *)ctr_src->data;
      fops = ctrdat_src->fops;
   } else {
      fops = NULL;
   }

   rhp_idx * restrict mp_equs = mp->equs.list;
   size_t mp_equs_len = mp->equs.len;
   const EquMeta * restrict equmeta = ctr_src->equmeta;
   for (size_t i = 0; i < mp_equs_len; ++i) {
      rhp_idx ei;
      rhp_idx ei_src = mp_equs[i];

      Equ *restrict edst;
      Equ *restrict esrc = &ctr_src->equs[ei_src];

      if (esrc->object != EQ_MAPPING) {
         error("[fooc] ERROR: Unconsistency: the type of the equation in the "
               "functional is %d, should be %d\n", esrc->object, EQ_MAPPING);
         return Error_Unconsistency;
      }

      rhp_idx vi = equmeta[ei_src].dual;
      assert(valid_vi(vi));
      ei = vi;
      edst = &ctr_mcp->equs[ei];
      edst->idx = ei;

      rosetta_equs[ei_src] = ei;

      if (esrc->tree && esrc->tree->root) {
         S_CHECK(nl_copy_(mdl_src, fops, esrc, edst, mdl_mcp));
      } else {
         edst->tree = NULL;
      }

      S_CHECK(equ_copymetadata(edst, esrc, ei));
      /*  TODO(xhub) see where to put this */
      // DEL:
      // ctr_mcp->m++;

      Lequ *elequ = esrc->lequ;
      if (elequ && elequ->len > 0) {
         assert(!edst->lequ);
         A_CHECK(edst->lequ, lequ_alloc(elequ->len));

         if (fops) {
            S_CHECK(fops->transform_lequ(fops->data, esrc, edst));
         } else {
            edst->lequ = elequ;
         }

      } else {
         edst->lequ = NULL;
      }
   }

   return OK;
}
#endif

/* ------------------------------------------------------------------------
 * This just gets all the objective equations and the number of perp mappings
 * ------------------------------------------------------------------------ */

static void print_objequs_vifuncs_stats(const Model *mdl_src, FoocData *fooc_dat)
{
   unsigned n_objequ = aequ_size(&fooc_dat->objequs);
   trace_fooc("[fooc] Found %u objective equations\n", n_objequ);
   for (unsigned i = 0; i < n_objequ; ++i) {
      trace_fooc("%*c %s\n", 6, ' ', mdl_printequname(mdl_src, aequ_fget(&fooc_dat->objequs, i)));
   }
   trace_fooc("[fooc] Found %zu VI functions\n", fooc_dat->info->n_vifuncs);
   trace_fooc("[fooc] Found %zu zero VI functions\n", fooc_dat->info->n_vizerofuncs);

}

static int fill_objequs_and_get_vifuncs(const Model *mdl_src, FoocData *fooc_dat)
{
   size_t lvi_funcs = 0, lvi_zerofuncs = 0;
   rhp_idx *mps2objequs;
   rhp_idx *objequs2mps;
   const Container *ctr_src = &mdl_src->ctr;
   const EmpDag *empdag = &mdl_src->empinfo.empdag;
   size_t total_m = fooc_dat->src.total_m;

   unsigned mps_len = fooc_dat->mps.len;

  /* No HOP structure  */

   if (mps_len == 0) {
    rhp_idx objvar, objequ;
    RhpSense sense;
    S_CHECK(mdl_getobjvar(mdl_src, &objvar));
    S_CHECK(mdl_getobjequ(mdl_src, &objequ));
    S_CHECK(mdl_getsense(mdl_src, &sense));

    if (valid_vi(objvar) && valid_ei(objequ)) {
      error("[fooc] ERROR in %s model '%.*s' (#%u): the variable '%s' is the "
            "objective variable and '%s' is the objective function."
            "This is unsupported, the model must have exactly one of these.\n",
            mdl_fmtargs(mdl_src), ctr_printvarname(ctr_src, objvar),
            ctr_printequname(ctr_src, objequ));
      return Error_Inconsistency;
    }

      if (valid_ei(objequ)) {
         aequ_setcompact(&fooc_dat->objequs, 1, objequ);
      } else if (!valid_vi(objvar) && sense != RhpFeasibility) {
         error("[fooc] ERROR in %s model '%.*s' #%u: sense is %s, but neither "
               "an objective variable nor an objective equation have been given\n",
               mdl_fmtargs(mdl_src), sense2str(sense));
         return Error_Inconsistency;
      } 

    if (sense == RhpFeasibility) {
         ModelType probtype;
         S_CHECK(mdl_gettype(mdl_src, &probtype));
          if (probtype == MdlType_vi) {
            VarMeta *vmd = mdl_src->ctr.varmeta;

            for (unsigned i = 0, len = ctr_nvars(&mdl_src->ctr); i < len; ++i) {
               unsigned basictype = vmd_basictype(vmd[i].ppty);
               if (basictype == VarPerpToViFunction) { lvi_funcs++; }
               if (basictype == VarPerpToZeroFunctionVi) { lvi_zerofuncs++; }
            }
         }
      }
      fooc_dat->info->n_vifuncs = lvi_funcs;
      fooc_dat->info->n_vizerofuncs = lvi_zerofuncs;

      if (O_Output & PO_TRACE_FOOC) {
         print_objequs_vifuncs_stats(mdl_src, fooc_dat);
      }
      return OK;
   }

   rhp_idx *objequs_list;
   MALLOC_(objequs_list, rhp_idx, mps_len);
   aequ_setandownlist(&fooc_dat->objequs, UINT_MAX, objequs_list);

   unsigned nb_objequs = 0;
   MALLOC_(mps2objequs, rhp_idx, 2 * mps_len);
   memset(mps2objequs, IdxNA,  (2 * mps_len)*sizeof(rhp_idx));
   fooc_dat->mp2objequ = mps2objequs;
   objequs2mps = &mps2objequs[mps_len];

   MpIdArray *mps = &fooc_dat->mps;

   for (unsigned i = 0; i < mps_len; i++) {
      MathPrgm *mp;
      S_CHECK(empdag_getmpbyid(empdag, mpidarray_at(mps, i), &mp));

      switch (mp->type) {
      case MpTypeOpt: {

         rhp_idx objvar = mp_getobjvar(mp);
         rhp_idx objequ = mp_getobjequ(mp);
         bool valid_objvar = valid_vi(objvar), valid_objequ = valid_ei(objequ);

         if (valid_objvar && valid_objequ) {
           err_objvar_(mdl_src, mp, objvar, objequ);
           return Error_EMPIncorrectInput;
         }

         if (!(valid_objvar || valid_objequ)) {
            mp_err_noobjdata(mp);
            return Error_EMPIncorrectInput;
         }

         if (valid_objequ) {
            S_CHECK(ei_inbounds(objequ, total_m, __func__));
            /* ----------------------------------------------------------
             * Perform a sorted insertion.
             * This is needed since we want to iterate over the equations in
             * an interval fashion: we go from one objective equation to the
             * other, removing the objective equations from the processing.
             *
             * One could implement a secant-type search and use memmove
             * TODO: GITLAB #111
             * ---------------------------------------------------------- */

            size_t j = nb_objequs;
            while (j > 0) {
               rhp_idx cur = objequs_list[j - 1];
               if (cur > objequ) {
                  objequs_list[j] = cur;
                  rhp_idx mp_idx = objequs2mps[j - 1];
                  mps2objequs[mp_idx] = j;
                  objequs2mps[j] = mp_idx;
               } else {
                  break;
               }
               j--;
            }
            objequs_list[j] = objequ;
            mps2objequs[i] = j;
            objequs2mps[j] = i;
            nb_objequs++;
         }

         break;
      }

      /* ----------------------------------------------------------------
      * The number of constraints in the total system should not account
      * for the GE functionals
      * ---------------------------------------------------------------- */

      /* See GITLAB #83 */
      //      case MpTypeMcp:
      case MpTypeVi: {
         unsigned mp_zerofuncs = mp_getnumzerofunc(mp);
         lvi_zerofuncs += mp_zerofuncs;
         lvi_funcs += mp_getnumvars(mp) - mp_zerofuncs;
         mps2objequs[i] = IdxNA;
         break;
      }

      default:
         error("%s ERROR: unsupported MP of type %s\n", __func__, mp_gettypestr(mp));
         return Error_NotImplemented;
      }

   }


   aequ_setsize(&fooc_dat->objequs, nb_objequs);
  fooc_dat->info->n_vifuncs = lvi_funcs;
  fooc_dat->info->n_vizerofuncs = lvi_zerofuncs;


   if (O_Output & PO_TRACE_FOOC) {
      print_objequs_vifuncs_stats(mdl_src, fooc_dat);
   }

   return OK;
}

static int add_nonlinear_normal_cone_term(Model *restrict mdl_mcp,
                                          Aequ *restrict cons_nl,
                                          struct sd_tool **restrict sd_cequ,
                                          bool *restrict var_in_mp,
                                          rhp_idx *vi2ei_dLdx) {

  /* ----------------------------------------------------------------------
   * Compute the derivative of each nonlinear constraint and add -μᵢ ∇ⱼ gᵢ^NL(x)
   * to the primal part of the GE
   *
   * Note that g^NL is already in the MCP model, and there has already been
   * translated with the new variable space.
   *
   * Note that we need the model to have been prefilled.
   *
   * TODO(xhub) right now we can't parallelize because of the model
   * representation update
   * ---------------------------------------------------------------------- */

   Container *ctr_mcp = &mdl_mcp->ctr;

  size_t nl_cons_size = cons_nl->size;
  for (size_t i = 0; i < nl_cons_size; ++i) {

    rhp_idx ei = aequ_fget(cons_nl, i);
    rhp_idx vi_mult = ctr_mcp->equmeta[ei].dual;
    struct sd_tool *sd_equ = sd_cequ[i];

    void *iterator = NULL;
    do {
      double jacval;
      int nlflag;
      rhp_idx vi;

      /* Get the equation from the new model  */
      S_CHECK(ctr_equ_itervars(ctr_mcp, ei, &iterator, &jacval, &vi, &nlflag));
      assert(valid_vi(vi));

      /* ----------------------------------------------------------------
       * We have the guarantee that we are already in the new varspace
       * ---------------------------------------------------------------- */

      /* ---------------------------------------------------------------
       * Filter the variable if there is an EMP structure
       * --------------------------------------------------------------- */

      if (var_in_mp && !var_in_mp[vi]) { continue; }

      /* ---------------------------------------------------------------
       * Compute the derivative in ediff and add λᵢ ∇ⱼ gᵢ(x) to the right
       * equation.
       *
       * TODO(xhub) some tape-like technology would be good
       * --------------------------------------------------------------- */

      Equ ediff;
      memset(&ediff, 0, sizeof(struct equ));
      equ_basic_init(&ediff);

      /* Dummy trick to avoid an error later  */
      ediff.idx = IdxNA;
      ediff.object = Mapping;

      /* Compute ∇ⱼ gᵢ(x) */
      S_CHECK(sd_tool_deriv(sd_equ, vi, &ediff));

      rhp_idx ei_dLdx = vi2ei_dLdx ? vi2ei_dLdx[vi] : vi;
      assert(valid_ei_(ei_dLdx, ctr_mcp->m, __func__));

      /* Add -λᵢ ∇ⱼ gᵢ(x) to the functional part for xⱼ */
      S_CHECK(rctr_equ_submulv_equ(ctr_mcp, &ctr_mcp->equs[ei_dLdx], &ediff, vi_mult));

      equ_free(&ediff);

    } while (iterator);
  }

  return OK;
}

static int add_polyhedral_normal_cone_term_(Model *restrict mdl,
                                            Aequ *restrict cequ_lin,
                                            bool *restrict var_in_mp,
                                            rhp_idx * restrict vi2ei_dLdx) {
  /* ----------------------------------------------------------------------
   * Add -<λ, A> to the primal part of a GE
   *
   * - the index of λ is the index of the equation (eidx)
   * - each linear constraint contains as value a row of A
   * - the term -λⱼ Aᵢⱼ is added to the i-th equation in the GE if the i-th
   *   variable belongs to the MP
   * ---------------------------------------------------------------------- */

  for (unsigned i = 0, clen = cequ_lin->size; i < clen; ++i) {

    rhp_idx ei = aequ_fget(cequ_lin, i);
    rhp_idx vi_mult = mdl->ctr.equmeta[ei].dual;
    Lequ *le = mdl->ctr.equs[ei].lequ; assert(le);

    rhp_idx *restrict idx = le->vis;
    double *restrict vals = le->coeffs;

    for (size_t j = 0, lelen = le->len; j < lelen; ++j) {

      rhp_idx vi = idx[j]; assert(valid_vi(vi));

      if (var_in_mp && !var_in_mp[vi]) {
        continue;
      }

      double val = vals[j];
      rhp_idx ei_dLdx = vi2ei_dLdx ? vi2ei_dLdx[vi] : vi;
      assert(valid_ei_(ei_dLdx, mdl->ctr.m, __func__));

      S_CHECK(equ_add_newlvar(&mdl->ctr.equs[ei_dLdx], vi_mult, -val));
    }
  }

  return OK;
}

static void identify_vars_in_mp(bool *restrict var_in_mp,
                                MathPrgm *restrict mp,
                                rhp_idx *restrict rosetta_vars)
{
  /* -------------------------------------------------------------------
   * This identifies the variables indices that belong to an MP
   * This also inits the equations that belong to this MP
   *
   * WARNING: this needs var_in_mp to be zeroed on input (with memset for
   * instance)
   *
   * Not so sure about the deleted variables, but it should be fine
   * ------------------------------------------------------------------- */

   rhp_idx * restrict mp_vars = mp->vars.arr;
   unsigned len = mp->vars.len;
   if (rosetta_vars) {

      for (size_t i = 0; i < len; ++i) {

         rhp_idx vi = rosetta_vars[mp->vars.arr[i]];

         if (valid_vi(vi)) {
            var_in_mp[vi] = true;
         }
      }

   } else {

      for (size_t i = 0; i < len; ++i) {
         assert(valid_vi(mp_vars[i]));
         var_in_mp[mp_vars[i]] = true;
      }
   }
}

static NONNULL int add_cons_mdl(Model *mdl_mcp, Equ *e) 
{
   Container *ctr_mcp = &mdl_mcp->ctr;
   Equ *edst = &ctr_mcp->equs[e->idx];
   memcpy(edst, e, sizeof(Equ));

   if (!edst->tree) {
      return OK;
   }

   assert(edst->tree->v_list && edst->tree->v_list->pool);
   Avar v;
   avar_setlist(&v, edst->tree->v_list->idx, edst->tree->v_list->pool);

   assert(valid_vi(edst->idx));
   S_CHECK(cmat_equ_add_vars(ctr_mcp, edst->idx, &v, NULL, true));

   return OK;
}

/* TODO: GITLAB #81 */
static inline void copy_values_equ2mult(Var *var, Equ *equ)
{
   assert(var->type == VAR_X && !var->is_conic);

   /* Trust the multiplier value ... */
   if (isfinite(equ->multiplier)) {
      var->value = equ->multiplier;
   } else {
      var->value = var->bnd.lb == 0 ? 1 : -1;
   }

   FOOC_DEBUG("equ #%u has basis status %s, with cone %s, val=%e, mult=%e",
              equ->idx, basis_name(equ->basis), cone_name(equ->cone),
              equ->value, equ->multiplier);

   /* TODO: see if one can improve values here */
   BasisStatus equ_basis = equ->basis;
   switch (equ_basis) {
   case BasisLower:
   case BasisUpper:
      var->basis = BasisBasic;
      var->multiplier = 0;
      
      break;
   case BasisBasic:
      if (isfinite(var->bnd.ub)) {
         var->basis = BasisUpper;
         var->multiplier = -1;
      } else {
         var->basis = BasisLower;
         var->multiplier = 1;
      }
      break;
   case BasisUnset:
   case BasisSuperBasic:
   case BasisFixed:
      var->basis = BasisUnset;
      break;
   default:
      error("%s :: unknown basis status %d", __func__, equ_basis);
   }
}

static int inject_vifunc_and_cons(Model *mdl_src, Model *mdl_mcp, FoocData *fooc_dat,
                                  const Fops * const fops_equ,
                                  struct equ_inh_names *equ_inh_names,
                                  const Rosettas * restrict rosettas_vars)
{
   Container * restrict ctr_mcp = &mdl_mcp->ctr;
   Container * restrict ctr_src = &mdl_src->ctr;
   RhpContainerData *cdat_mcp = (RhpContainerData *)ctr_mcp->data;

   size_t ei_lin    = fooc_dat->ei_lincons_start;
   size_t ei_start  = fooc_dat->ei_cons_start;
   size_t ei_nl     = ei_start;
   size_t vi_mult   = fooc_dat->vi_mult_start;
   rhp_idx *vi2ei_F = fooc_dat->vi_primal2ei_F;

   /* We need to evaluate the VI functions when reporting the solution */
   unsigned n_vifuncs = fooc_dat->info->n_vifuncs;
   rhp_idx *vifuncs_list = NULL;
   if (n_vifuncs > 0) {
      MALLOC_(vifuncs_list, rhp_idx, n_vifuncs);
      Aequ vifuncs;
      aequ_setandownlist(&vifuncs, n_vifuncs, vifuncs_list);
      S_CHECK(aequ_extendandown(ctr_src->func2eval, &vifuncs));
   }

   /* ---------------------------------------------------------------------
    * We set the inherited structure to transfer ownsership of rev_rosetta_cons
    * TODO GITLAB #77
    * Marking the constraints as inherited breaks the export to GMO of NL cons.
    * The translation to indices can only happen once.
    * The model representation might be used in the GMO creation and its unclear
    * what happens there.
    * --------------------------------------------------------------------- */

  /* ----------------------------------------------------------------------
   * Start copying the constraints equations
   * ---------------------------------------------------------------------- */

   rhp_idx *rosetta_equs = ctr_src->rosetta_equs;
   rhp_idx *rosetta_vars = ctr_src->rosetta_vars;

   struct vnames *vnames_equ = &cdat_mcp->equ_names.v;
   assert(vnames_equ->type == VNAMES_FOOC_LOOKUP);
   VecNamesFoocLookup *lookup_dat = vnames_equ->fooc_lookup;
   VecNamesLookupTypes *names_lookup_types = vnames_lookup_gettypes(lookup_dat);


   unsigned n_mappings = 0;
   size_t ei_src_start = 0;
   size_t i_objequs = 0;
   size_t total_m_src = fooc_dat->src.total_m;
   unsigned skipped_equ = fooc_dat->skipped_equ;

   Aequ *objequs = &fooc_dat->objequs;

   do {
      size_t ei_src_end =
         i_objequs < objequs->size ? aequ_fget(objequs, i_objequs) : total_m_src;

      assert(ei_src_end <= total_m_src);

      for (size_t i = ei_src_start; i < ei_src_end; ++i) {

         if (fops_equ && !fops_equ->keep_equ(fops_equ->data, i)) {
            /* This equation is not kept  */
            skipped_equ++;
            rosetta_equs[i] = IdxNA;
            continue;
         }

         Equ equ;
         S_CHECK(getequ_curidx(mdl_src, i, rosettas_vars, &equ));
         rhp_idx ei, vi;

        /* -------------------------------------------------------------
         * If the source equation is mapping, we consider that the equation
         * is a VI mapping. Then it gets copied at the right place, that is
         * the position given by vi.
         * ------------------------------------------------------------- */

         if (equ.object == Mapping) {
            assert(ctr_src->equmeta && vifuncs_list);

            rhp_idx vi_src = ctr_src->equmeta[i].dual; assert(valid_vi(vi_src));
            vi = rosetta_vars[vi_src]; assert(valid_vi(vi));
            ei = vi2ei_F ? vi2ei_F[vi] : vi; assert(valid_ei(ei));

            if (n_mappings >= n_vifuncs) {
               error("[fooc] ERROR: processed %u mappings, but only %u VI funcs"
                     "have been detected\n", n_mappings+1, n_vifuncs);
               return Error_RuntimeError;
            }

            names_lookup_types[ei] = VNL_VIfunc;
            vifuncs_list[n_mappings] = i;
            n_mappings++;

            FOOC_DEBUG("adding VI mapping '%s' at pos %zu",
                       mdl_printequname(mdl_src, i), (size_t)ei);

         /* --------------------------------------------------------------
          *  The equation is a constraint and we copy it as such
          * -------------------------------------------------------------- */
         } else if (equ.object == ConeInclusion) {

            /* rev_rosetta_cons must be updated before nl_copy_ to avoid issues
           * with tracing container changes */
            if (equ.tree && equ.tree->root) {
               FOOC_DEBUG("adding NL constraint '%s' at pos %zu",
                          mdl_printequname(mdl_src, i), ei_nl);
               ei = ei_nl++;

            } else { /* This is an affine equation  */

               FOOC_DEBUG("adding linear constraint '%s' at pos %zu",
                          mdl_printequname(mdl_src, i), ei_lin);
               ei = ei_lin++;
            }

            assert(ei >= ei_start && ei < rctr_totalm(ctr_mcp));
            S_CHECK(rhp_idx_set(&equ_inh_names->cons_src, ei-ei_start, i));

            /* Copy the equations values to multiplier values */
            S_CHECK(rctr_var_setasdualmultiplier(ctr_mcp, &ctr_src->equs[i], vi_mult));
            copy_values_equ2mult(&mdl_mcp->ctr.vars[vi_mult], &ctr_src->equs[i]);

            vi = vi_mult++;

         } else {
            error("%s :: equation '%s' is a %s rather than a constraint or mapping\n",
                  __func__, ctr_printequname(&mdl_src->ctr, i), equtype_name(equ.object));
            return Error_UnExpectedData;
         }

         rosetta_equs[i] = ei;

         equ.idx = ei;
         S_CHECK(add_cons_mdl(mdl_mcp, &equ));
         Equ *edst = &mdl_mcp->ctr.equs[ei];

         /* -------------------------------------------------------------
         * Define the dual multipliers of the constraint
         *
         * We define the multipliers to be in the dual cone, not the polar.
         * This yields g ⊥ (μ,λ) ∈ Y and the constraints can be directly copied.
         * However, the Lagrangian is
         *                 f(x)  -  < μ, g^NL(x) >  -  < λ, Ax + b >
         * and the corresponding primal GE part is
         *                  ∇ₓf  -  (∇ₓg^NL)ᵀ μ     -  (Aₓ)ᵀ λ
         *
         * If the multipliers are in the polar, the part of the generalized
         * equation corresponding to the multipliers is  -g ⊥ (μ,λ) ∈ Y
         * ------------------------------------------------------------- */

         if (edst->object == ConeInclusion) {

            /*  TODO(xhub) add prox pert? */
            S_CHECK(cmat_sync_lequ(ctr_mcp, edst));

            /* -------------------------------------------------------------
           * Now we can change the constraint to a mapping
           * ------------------------------------------------------------- */

            edst->object = Mapping;

            S_CHECK(rctr_setequvarperp(ctr_mcp, ei, vi));

         } else if (edst->object == Mapping) {

            // No need to call rctr_finalize_add_equ, this is done for all the
            // non-constraints equations at once.
            
            // TODO: DELETE and cleanup
            //S_CHECK(rctr_finalize_add_equ(ctr_mcp, edst));
            /* XXX What is happening here?  */
            S_CHECK(rctr_setequvarperp(ctr_mcp, ei, vi));
            //
         } else {
            error("%s :: unsupported type %d for equation '%s'\n.",
                  __func__, edst->object, ctr_printequname(ctr_mcp, edst->idx));
            return Error_UnExpectedData;
         }

        /* TODO(Xhub) most likely to delete, already done in
         * model_finalize_add_equ  */
#if 0
            /* -------------------------------------------------------------
             * Fill the model representation for the constraints
             *
             * TODO(xhub) this should be unnecessary if we can inherit the model
             * representation, and if there is no translation
             * ------------------------------------------------------------- */

            void *jacptr = NULL;
            size_t indx = 0;

            do {
              double jacval;
              int nlflag;
              int vidx;

              S_CHECK(ctr_getrowjacinfo(ctr_src, i, &jacptr, &jacval, &vidx, &nlflag));

              nlflags[indx] = nlflag > 0;
              jac_vals[indx] = jacval;
              var_in_equ[indx++] = rosetta_vars ? rosetta_vars[vidx] : vidx;
              assert(valid_vi(var_in_equ[indx-1]));

            } while (jacptr);

            Avar vars = { .type = EquVar_List, .size = indx, .list = var_in_equ };

            S_CHECK(model_update_repr(ctr_mcp, &vars, midx, nlflags, jac_vals));
#endif
      } /*  end for loop over equation interval */

      /* Go right after the objective equation */
      ei_src_start = ei_src_end + 1;
      rosetta_equs[ei_src_end] = IdxNA;
      i_objequs++;
   } while (i_objequs <= objequs->size);

   /* Basic sanity check */
   if (n_mappings != n_vifuncs) {
      error("[fooc] ERROR: Expected to add %zu VI mappings, but only %u were!\n",
            (size_t)n_vifuncs, n_mappings);
      return Error_RuntimeError;
   }

   /* ----------------------------------------------------------------------
   * Add the VI zerofunc
   * ---------------------------------------------------------------------- */

   size_t n_vizerofunc = fooc_dat->info->n_vizerofuncs;

   if (n_vizerofunc > 0) {
      rhp_idx vi = 0;
      size_t total_n = ctr_nvars_total(ctr_src);
      VarMeta * restrict varmeta = ctr_src->varmeta;
      assert(varmeta);

      while (n_vizerofunc > 0 && vi < total_n) {
         if (vmd_basictype(varmeta[vi].ppty) == VarPerpToZeroFunctionVi) {
            rhp_idx vi_new = rosetta_vars[vi];
            if (!valid_vi(vi_new)) { vi++; continue; }

            rhp_idx ei = vi2ei_F ? vi2ei_F[vi_new] : vi_new;
            S_CHECK(rctr_init_equ_empty(ctr_mcp, ei, Mapping, CONE_NONE));
            FOOC_DEBUG(" VI zero func added at pos %u for VI var '%s'", ei,
                       ctr_printvarname(ctr_mcp, vi_new));
            names_lookup_types[ei] = VNL_VIzerofunc;
            S_CHECK(rctr_setequvarperp(ctr_mcp, ei, vi_new));
            n_vizerofunc--;
         }
         vi++;
      }
   }

   if (n_vizerofunc > 0) {
      error("[fooc] ERROR: %zu VI zero function could not be added!\n", n_vizerofunc);
      return Error_RuntimeError;
   }

   /* Do not add n_vizerofuncs here as they are not present in the source model */
   //ctr_mcp->m = mcpstats->n_vifuncs + mcpstats->n_constraints;
   //ctr_mcp->m += mcpds->stats->n_vifuncs + mcpds->stats->n_constraints;
   //S_CHECK(rctr_compress_check_equ_x(&mdl_src->ctr, ctr_mcp, skipped_equ, fops_equ));

   /* ----------------------------------------------------------------------
   * Important: this has to be after the rmdl_compress_check_equ call!
   *
   * - There are at least var_size more "equations"
   * - total_m is also known
   * - we just added cons_size multipliers
   * ---------------------------------------------------------------------- */

   fooc_dat->skipped_equ = skipped_equ;
   ctr_mcp->m = fooc_dat->n_equs_orig + fooc_dat->info->mcp_size;

   /* Just 2 consistency checks  */
   assert(ei_lin == (fooc_dat->info->mcp_size + fooc_dat->n_equs_orig));
   assert(ei_nl == (ei_lin - fooc_dat->info->n_lincons));

   return OK;
}

/**
 * @brief Build first-order optimality conditions w.r.t primal variables for an
 * optimization problem
 *
 * Add the equations \f$ ∇ₓf(x)  - (∇ₓg^NL)ᵀ μ  - (Aₓ)ᵀ λ \f$ into the MCP
 *
 * @param ctr_mcp      the destination container
 * @param mp           the mathematical programm (optional)
 * @param cequ_nl      the nonlinear constraints (only those belonging to the
 * MP, if any)
 * @param cequ_lin     the linear constraints (only those belonging to the MP,
 * if any)
 * @param ctr_src      the source container
 * @param objequ       the objective equation, to be found in ctr_src
 *
 * @return         the error code
 */
static int fooc_mcp_primal_opt(Model *restrict mdl_mcp,
                               MathPrgm *restrict mp,
                               Aequ *restrict cons_nl,
                               Aequ *restrict cons_lin,
                               Model *restrict mdl_src,
                               FoocData *restrict fooc_dat,
                               rhp_idx objequ) {
  /* ----------------------------------------------------------------------
   * A few assumptions:
   * - the multiplier are all in the model and their indices matches the once
   *   from equation (that is the multiplier for equation i has index i)
   * - no useless objective variables in the model: all that is unnecessary has
   *   been removed. This ensures that the variable indices are the final ones
   * - cons_nl and cons_lin are in the MCP index space
   * ---------------------------------------------------------------------- */

  /* ----------------------------------------------------------------------
   * Function description: we need to transform the OPT problem into an MCP.
   * We need to compute the (partial) derivative of the objective functional
   * with respect to the decision variables.
   * Also, any constraints must be added to the functional part.
   *
   *                     | x |   primal variable
   * The variables are   | μ |   multipliers for the NL constraints
   *                     | λ |   multipliers for the polyhedral constraints
   *
   * The functional part is   |  ±∇ₓf  - (∇ₓg^NL)ᵀ μ  - (Aₓ)ᵀ λ       |
   *                          |                 g^NL                  |
   *                          |                Ax + b                 |
   *
   * For min problem we have ∇ₓf and for max problem -∇ₓf
   *
   * Hence we are going to have 3 stages:
   * 1) Compute and add    ∇ₓf
   * 2) Compute and add   - (∇ₓg^NL)ᵀ μ
   * 3) Compute and add   - (Aₓ)ᵀ λ
   *
   * The equations  g^NL  and   Ax + b  are already in the model
   * ---------------------------------------------------------------------- */

   int status = OK;

   Container * restrict ctr_src = &mdl_src->ctr;
   Container * restrict ctr_mcp = &mdl_mcp->ctr;

   struct sd_tool *sd_objequ;
   if (valid_ei(objequ)) {
      A_CHECK(sd_objequ, sd_tool_alloc(SDT_ANY, ctr_src, objequ));
   } else {
      sd_objequ = NULL;
   }

   size_t cons_nl_size = cons_nl->size;
   struct sd_tool **sd_cequ = NULL;
   /* cache that */
   bool *var_in_mp = NULL;

   S_CHECK_EXIT(alloc_memory_(mdl_src, cons_nl_size, mp, &sd_cequ, &var_in_mp))

  /* ----------------------------------------------------------------------
   * The objective equation and the MP will be in the old varspace, if any
   * ---------------------------------------------------------------------- */

   rhp_idx *rosetta_vars = ctr_src->rosetta_vars;

  /* ----------------------------------------------------------------------
   * Fill var_in_mp to get which variables belongs to the MP
   * This is used for computing the gradient of f w.r.t. to the MP variables
   * ---------------------------------------------------------------------- */

   if (mp) {
      assert(var_in_mp);
      identify_vars_in_mp(var_in_mp, mp, rosetta_vars);
   }

   /*  TODO(xhub) parallelize and cache that */
   for (size_t i = 0; i < cons_nl_size; ++i) {
      rhp_idx ei = aequ_fget(cons_nl, i);
      assert(ei < rctr_totalm(ctr_mcp));
      Equ* e = &ctr_mcp->equs[ei];
      /* TODO: ensure that this is not needed here ... */

      A_CHECK_EXIT(sd_cequ[i], sd_tool_alloc_fromequ(SDT_ANY, ctr_mcp, e));
   }

   rhp_idx *vi_primal2ei_F = fooc_dat->vi_primal2ei_F;

  /* ----------------------------------------------------------------------
   * Compute the derivative of the objective w.r.t primal variables ∇ₖf(x)
   * ---------------------------------------------------------------------- */

   RhpSense sense;
   if (mp) {
      sense = mp_getsense(mp);
   } else {
      S_CHECK_EXIT(mdl_getsense(mdl_src, &sense));
   }

  /* ----------------------------------------------------------------------
   * If there is no objequ, then it is implicitely defined as the objvar.
   * Take the derivative there
   * ---------------------------------------------------------------------- */

   if (objequ == IdxNA) {
      rhp_idx objvar;

      if (mp) {
         objvar = mp_getobjvar(mp);
      } else {
         S_CHECK_EXIT(mdl_getobjvar(mdl_src, &objvar));
         assert(!vi_primal2ei_F);
      }

      if (!valid_vi(objvar)) {
         error("%s :: no valid objvar and no valid objequ\n", __func__);
         status = Error_UnExpectedData;
         goto _exit;
      }

      rhp_idx objvar_dst = rosetta_vars ? rosetta_vars[objvar] : objvar;

      rhp_idx ei = vi_primal2ei_F ? vi_primal2ei_F[objvar_dst] : objvar_dst;
      S_CHECK(rctr_setequvarperp(ctr_mcp, ei, objvar_dst));

      Equ *e = &ctr_mcp->equs[ei];
      switch (sense) {
      case RhpMin:
         equ_set_cst(e, 1.);
         break;
      case RhpMax:
         equ_set_cst(e, -1.);
         break;
      default:
         error("%s :: unsupported sense %s\n", __func__, sense2str(sense));
         status = Error_InvalidValue;
         goto _exit;
      }

   

      goto add_multiplier_terms;
   }

   assert(valid_ei(objequ));

   void *iterator = NULL;
   do {
      double jacval;
      int nlflag;
      rhp_idx vi_src;

      S_CHECK_EXIT(
         ctr_equ_itervars(ctr_src, objequ, &iterator, &jacval, &vi_src, &nlflag));

    /* -------------------------------------------------------------------
     * ctr_src is still in the original namespace
     * ------------------------------------------------------------------- */

    rhp_idx vi = rosetta_vars ? rosetta_vars[vi_src] : vi_src;
    assert(valid_vi(vi));

    /* ------------------------------------------------------------------
     * Filter the variable if there is an EMP structure: we do not compute the
     * derivative with respect to the other MPs primal variables
     * ------------------------------------------------------------------ */

    if (var_in_mp && !var_in_mp[vi]) { continue; }

    Equ ediff;
    memset(&ediff, 0, sizeof(struct equ));
    equ_basic_init(&ediff);
    /* Dummy trick to avoid an error later  */
    ediff.idx = IdxNA;
    ediff.object = Mapping;

    /* --------------------------------------------------------------------
     * Compute ∇ₖf(x) and inject that in the equation with index k.
     * Remember the structure of the functional in the VI/CP.
     *
     * TODO(Xhub) is it really equ_add_equ? or we just want to copy ediff?
     * TODO(xhub) We also need a umin in the MAX case
     * -------------------------------------------------------------------- */

    S_CHECK_EXIT(sd_tool_deriv(sd_objequ, vi_src, &ediff));

    /*  TODO(Xhub) check that the derivative is not zero */

      rhp_idx ei = vi_primal2ei_F ? vi_primal2ei_F[vi] : vi;

      assert(ei >= fooc_dat->ei_F_start && ei < fooc_dat->ei_cons_start);

      S_CHECK(rctr_setequvarperp(ctr_mcp, ei, vi));
    Equ *e = &ctr_mcp->equs[ei];
    assert(e && e->idx == ei);

    /* -------------------------------------------------------------------
     * If the model is a maximization problem, we need to add -∇ₖf(x)
     * ------------------------------------------------------------------- */

    switch (sense) {
    case RhpMin:
      S_CHECK_EXIT(rctr_equ_add_equ_rosetta(ctr_mcp, e, &ediff, rosetta_vars));
      break;
    case RhpMax:
      S_CHECK_EXIT(rctr_equ_min_equ_rosetta(ctr_mcp, e, &ediff, rosetta_vars));
      break;
    default:
      error("%s :: unsupported sense %s\n", __func__, sense2str(sense));
      status = Error_InvalidValue;
      goto _exit;
    }

    equ_free(&ediff);

  } while (iterator);

  /* ----------------------------------------------------------------------
   * Phase 2:  Compute and add   - < μ, (∇ₓg^NL)^T >
   * ---------------------------------------------------------------------- */

add_multiplier_terms:
  S_CHECK_EXIT(
      add_nonlinear_normal_cone_term(mdl_mcp, cons_nl, sd_cequ, var_in_mp,
                                      vi_primal2ei_F));

  /* ----------------------------------------------------------------------
   * Phase 3:  Compute and add   - <λ, Aₓ^T>
   * ---------------------------------------------------------------------- */

  S_CHECK_EXIT(add_polyhedral_normal_cone_term_(mdl_mcp, cons_lin, var_in_mp,
                                                vi_primal2ei_F));

_exit:
  sd_tool_dealloc(sd_objequ);

  if (sd_cequ) {
    for (size_t i = 0; i < cons_nl_size; ++i) {
      sd_tool_dealloc(sd_cequ[i]);
    }
  }

  FREE(sd_cequ);
  FREE(var_in_mp);

  return status;
}

/**
 * @brief Build first-order optimality conditions w.r.t primal variables for an
 * MCP
 *
 * Add the equations \f$ F \f$ into the container
 *
 * @param ctr_mcp      the destination container
 * @param mp           the mathematical programm (optional)
 * @param ctr_src      the source container
 *
 * @return         the error code
 */
static int fooc_mcp_primal_mcp(Model *ctr_mcp, MathPrgm *mp,
                               Model *ctr_src) {
  /* ----------------------------------------------------------------------
   * A few assumptions:
   * - the multiplier are all in the model and their indices matches the once
   *   from equation (that is the multiplier for equation i has index i)
   * - no useless objective variables in the model: all that is unnecessary has
   *   been removed. This ensures that the variable indices are the final ones
   * ---------------------------------------------------------------------- */
  int status = Error_NotImplemented;

  return status;
}
/**
 * @brief Build first-order optimality conditions w.r.t primal variables for a
 * VI
 *
 * Add the equations \f$ 0\in F(x) + N_C(x) \f$ into the MCP
 *
 * @param mdl_mcp      the destination container
 * @param mp           the mathematical programm (optional)
 * @param cequ_nl      the nonlinear constraints (only those belonging to the
 * MP, if any)
 * @param cequ_lin     the linear constraints (only those belonging to the MP,
 * if any)
 * @param mdl_src      the source container
 *
 * @return             the error code
 */
static int fooc_mcp_primal_vi(Model *mdl_mcp, MathPrgm *mp,
                              Aequ *cequ_nl, Aequ *cequ_lin,
                              Model *mdl_src, FoocData *fooc_dat)
{
  /* ----------------------------------------------------------------------
   * A few assumptions:
   * - the multiplier are all in the model and their indices matches the once
   *   from equation (that is the multiplier for equation i has index i)
   * - no useless objective variables in the model: all that is unnecessary has
   *   been removed. This ensures that the variable indices are the final ones
   * - The VI functions, namely F, have already been copied in the model
   * ---------------------------------------------------------------------- */

  /* ----------------------------------------------------------------------
   * Function description: we need to transform the VI into an MCP.
   * This entails that any constraint must be added to the functional part.
   * Let us write the VI as 0  ∈  F(x) + Nₓ(x)  and X described by constraints
   *     g^NL(x) ∈ K^NL   and   Ax + b ∈ K^L
   *
   *                     | x |   primal variable
   * The variables are   | μ |   multipliers for the NL constraints
   *                     | λ |   multipliers for the polyhedral constraints
   *
   * The functional part is   |  F  - < μ, (∇ₓg^NL)^T > - < λ, A^T >  |
   *                          |                 g^NL                  |
   *                          |                Ax + b                 |
   *
   * Hence we are going to have 3 stages:
   * 1) Add F
   * 2) Compute and add   - < μ, (∇ₓg^NL)^T >
   * 3) Add   - < λ, A^T >
   *
   * The equations  g^NL  and   Ax + b  are already in the model
   * ---------------------------------------------------------------------- */

  int status = OK;

   Container * restrict ctr_src = &mdl_src->ctr;
   Container * restrict ctr_mcp = &mdl_mcp->ctr;

  /* ----------------------------------------------------------------------
   * Acquire memory
   * ---------------------------------------------------------------------- */

  const size_t nl_cons_size = cequ_nl->size;
  struct sd_tool **sd_cequ = NULL;
  /* TODO(xhub) cache that */
  bool *var_in_mp = NULL;

  S_CHECK_EXIT(alloc_memory_(mdl_src, nl_cons_size, mp, &sd_cequ, &var_in_mp))

  /* ----------------------------------------------------------------------
   * The data from the VI will likely be in the old varspace, if any
   * ---------------------------------------------------------------------- */

  rhp_idx *rosetta_vars = ctr_src->rosetta_vars;

  /* ----------------------------------------------------------------------
   * Fill var_in_mp to get which variables belongs to the MP
   * This is used for computing the gradients w.r.t. to the MP variables
   * ---------------------------------------------------------------------- */

  if (mp) {
    assert(var_in_mp);
    identify_vars_in_mp(var_in_mp, mp, rosetta_vars);
  }

  /*  TODO(xhub) parallelize and cache that */
  for (size_t i = 0; i < nl_cons_size; ++i) {
    rhp_idx ei = aequ_fget(cequ_nl, i);
    assert(ei < rctr_totalm(ctr_mcp));
    A_CHECK_EXIT(sd_cequ[i], sd_tool_alloc(SDT_ANY, ctr_mcp, ei));
  }

   rhp_idx *vi_primal2ei_F = fooc_dat->vi_primal2ei_F;

  /* ----------------------------------------------------------------------
   * Phase 2:  Compute and add   - < μ, (∇ₓg^NL)^T >
   * ---------------------------------------------------------------------- */

  S_CHECK_EXIT(
      add_nonlinear_normal_cone_term(mdl_mcp, cequ_nl, sd_cequ, var_in_mp,
                                      vi_primal2ei_F));

  /* ----------------------------------------------------------------------
   * Phase 3:  Compute and add d - <λ, Aₓ^T>
   * ---------------------------------------------------------------------- */

  S_CHECK_EXIT(add_polyhedral_normal_cone_term_(mdl_mcp, cequ_lin, var_in_mp,
                                                vi_primal2ei_F));

_exit:

  FREE(sd_cequ);
  FREE(var_in_mp);

  return status;
}


/**
 * @brief Build the first order condition of an optimization problem or an
 * equilibrium
 *
 * @param  mdl_mcp    the MCP model to be created, with the problem to be
 *                    transformed into an MCP as source problem
 * 
 * @return            the error code
 */
int fooc_mcp(Model *mdl_mcp)
{

  /* ----------------------------------------------------------------------
   * TODO: how much is this still true?
   * A few assumptions on the input data:
   * - the container must be RHP (could be fixed by making the Var and
   *   Equ really modeling language independent
   *
   * On output:
   *                     | x |   primal variable
   * The variables are   | μ |   multipliers for the NL constraints
   *                     | λ |   multipliers for the polyhedral constraints
   *
   * The functional part is   |  ∇ₓf  - < μ, (∇ₓg^NL)^T > - < λ, A^T >      |
   *                          |                 g^NL(x)                     |
   *                          |                 Ax + b                      |
   * ---------------------------------------------------------------------- */

  int status = OK;
  Container *ctr_mcp = &mdl_mcp->ctr;
  RhpContainerData *cdat_mcp = (RhpContainerData *)ctr_mcp->data;

   McpInfo * restrict mcpstats;
   S_CHECK(mdl_settype(mdl_mcp, MdlType_mcp));
   A_CHECK(mcpstats, mdl_getmcpinfo(mdl_mcp));

   FoocData fooc_dat;
   fooc_data_init(&fooc_dat, mcpstats);

   /* This is a HACK */
   if (cdat_mcp->total_m == 0) {
      ctr_mcp->m = 0;
   } else if (cdat_mcp->total_m != ctr_mcp->m) {
      error("[fooc] ERROR: the MCP model has a different number of total and "
            "active equations: %zu vs %u\n", cdat_mcp->total_m, ctr_mcp->m);
      return Error_Inconsistency;
   }

   if (cdat_mcp->total_n == 0) {
      ctr_mcp->n = 0;
   }

  Rosettas rosettas_vars;
  rosettas_init(&rosettas_vars);
  rhp_idx *cons_idxs = NULL;

   Model *mdl_src = mdl_mcp->mdl_up;
   if (!mdl_src) {
      error("%s ERROR: the source container is missing!\n", __func__);
      return Error_NullPointer;
   }

   Container *ctr_src = &mdl_src->ctr;
   EmpDag *empdag = &mdl_src->empinfo.empdag;

  /* ----------------------------------------------------------------------
   * Fill fooc_dat.mps if needed
   * ---------------------------------------------------------------------- */

   Fops *fops = mdl_src->ctr.fops;
   if (fops) {
      switch (fops->type) {
      case FopsEmpDagSubDag: {
         daguid_t uid_root = fops_subdag_getrootuid(fops);
         S_CHECK_EXIT(fooc_data_empdag(&fooc_dat, empdag, uid_root, fops));
         break;
      }
      case FopsEmpDagSingleMp: {
         mpid_t mpid = fops_singleMP_getmpid(fops);
         mpidarray_add(&fooc_dat.mps, mpid);
         break;
      }
      case FopsEmpDagNash: {
         const MpIdArray *mpids = fops_Nash_getmpids(fops);
         S_CHECK_EXIT(mpidarray_copy(&fooc_dat.mps, mpids));
         break;
      }
      default: ;
      }
   }

   if (fooc_dat.mps.len == 0 && empdag->mps.len > 0) {

      unsigned n_roots = empdag->roots.len;

      if (n_roots != 1) {
         error("[fooc] ERROR in %s model '%.*s' #%u: %u root(s) detected, need "
               "be to given a unique root!\n", mdl_fmtargs(mdl_src), n_roots);
         return Error_EMPRuntimeError;
      }

      S_CHECK_EXIT(fooc_data_empdag(&fooc_dat, empdag, empdag->roots.arr[0], fops));
   }

   /* Check that we can compute the FOOC */
   S_CHECK(fooc_check_childless_mps(mdl_mcp, &fooc_dat));

   size_t n_primalvars, n_equs4mcp;

   if (fops) {
      fops->get_sizes(fops->data, &n_primalvars, &n_equs4mcp);
   } else {
      n_primalvars = ctr_src->n; 
      n_equs4mcp = ctr_src->m; 
   }

  
   fooc_dat.n_equs_orig = ctr_nequs(ctr_mcp);
   fooc_dat.src.total_m = ctr_nequs_total(ctr_src);
   fooc_dat.src.total_n = ctr_nvars_total(ctr_src);
   fooc_dat.vi_mult_start = n_primalvars;

   /* ----------------------------------------------------------------------
   * Init the MCP as a square system.
   *
   * - Fill objequs as the objective variable(s) or equation(s) shall not be
   *   copied
   * - Get the number of VI functions and VI zero functions
   * ---------------------------------------------------------------------- */

   S_CHECK_EXIT(fill_objequs_and_get_vifuncs(mdl_src, &fooc_dat));

  /* ----------------------------------------------------------------------
   * The total number of constraints is obtain from the number of incoming
   * equations minus:
   * - the number of objective variables
   * - the number of VI relations
   * ---------------------------------------------------------------------- */

   Aequ * restrict objequs = &fooc_dat.objequs;

  size_t n_vifuncs = mcpstats->n_vifuncs;
  assert(n_equs4mcp >= objequs->size + n_vifuncs);
  const size_t cons_size = n_equs4mcp - (objequs->size + n_vifuncs);


   if (mcpstats->n_foocvars != SSIZE_MAX) {
      if (mcpstats->n_foocvars + mcpstats->n_auxvars != n_primalvars) {
         error("[fooc] ERROR in %s model '%.*s' #%u: inconsistency in the "
               "number of variable: total is %zu, but %zu are tagged for the "
               "derivative (first-order optimality conditions) and %zu are tagged "
               "as auxiliaries (that is present in equations, but not in the "
               "first set). The second total is %zu\n", mdl_fmtargs(mdl_src),
               n_primalvars, mcpstats->n_foocvars, mcpstats->n_auxvars,
               mcpstats->n_foocvars + mcpstats->n_auxvars);
         return Error_Inconsistency;
      }
   } else if (mcpstats->n_auxvars != SSIZE_MAX) {
      error("[fooc] ERROR in %s model '%.*s' #%u: no FOOC vars, but %zu aux vars\n",
            mdl_fmtargs(mdl_src), mcpstats->n_auxvars);
         return Error_Inconsistency;
   } else { /* if n_foocvars was not set, it is assumed to be all vars */
      mcpstats->n_foocvars = n_primalvars;
      mcpstats->n_auxvars = 0;
   }

   size_t n_foocvars = mcpstats->n_foocvars;

   /* We do not count the auzliary variables in mcp_size*/
   size_t mcp_size = cons_size + mcpstats->n_foocvars;

   mcpstats->mcp_size = mcp_size;
   mcpstats->n_primalvars = n_primalvars;
   mcpstats->n_constraints = cons_size;

  /* ----------------------------------------------------------------------
   * Reserve all the variables and equations.
   * ATTENTION: we reserve more space for the variables (ctr_src->n + ncons),
   * as we could have "extra" variables. This is the case when computing an
   * MPMCP, as the variables of the upper level problem can be present
   * ---------------------------------------------------------------------- */

   /* TODO GITLAB #108*/
   S_CHECK_EXIT(ctr_resize(ctr_mcp, n_primalvars+cons_size, mcp_size));
   ctr_mcp->n = ctr_mcp->m = 0;

   /* ATTENTION: set total_m already here to allow for some debugging info */
   cdat_mcp->total_m = mcp_size;

   /* TODO: delete? This should already be done when creating the ctr_mcp? */
   S_CHECK_EXIT(ctr_borrow_nlpool(ctr_mcp, ctr_src));

   REALLOC_EXIT(ctr_src->rosetta_vars, rhp_idx, fooc_dat.src.total_n);

   /*  We allocate + 1 since it simplifies the logic */
   REALLOC_EXIT(ctr_src->rosetta_equs, rhp_idx, fooc_dat.src.total_m + 1);

   rhp_idx *rosetta_equ = ctr_src->rosetta_equs;

   if (fops) { /* With filtering, all equation data is copied */
      rctr_inherited_equs_are_not_borrowed(ctr_mcp);
   }

   /* ----------------------------------------------------------------------
   * 1. Take care of variables in the MCP model:
   *   - Compress primal variables and introduce them in the MCP model.
   *   - Set multiplier names
   * ---------------------------------------------------------------------- */

   S_CHECK_EXIT(rctr_compress_vars(ctr_src, ctr_mcp));

   S_CHECK_EXIT(compute_all_rosettas(mdl_mcp, &rosettas_vars));

  /* ----------------------------------------------------------------------
   * 2. Inject constraints into the MCP model
   *  - Set equation names
   *  - Sort constraints as NL or affine
   * ---------------------------------------------------------------------- */

  /* ----------------------------------------------------------------------
   * Equations cannot be copied, since we rearrange them in NL/aff groups
   * ---------------------------------------------------------------------- */

  /* ----------------------------------------------------------------------
   * - Sort the constraints: NL first, then affine
   * - Translate the variable into new space, if needed
   * - Add the equations starting at index var_size
   *
   * IMPORTANT: the objective equation(s) shall not be copied
   * ---------------------------------------------------------------------- */

   size_t ei_src_start = 0;
   size_t n_lincons = 0;
   unsigned n_objequs = objequs->size, idx = 0;
   const EquMeta * restrict equmeta = ctr_src->equmeta;
   do {
      size_t ei_src_end =
         idx < n_objequs ? aequ_fget(objequs, idx) : fooc_dat.src.total_m;

      assert(ei_src_end <= fooc_dat.src.total_m);

      for (size_t i = ei_src_start; i < ei_src_end; ++i) {
         if (fops && !fops->keep_equ(fops->data, i)) { continue; }

         EquExprType etype;
         S_CHECK_EXIT(ctr_getequexprtype(ctr_src, i, &etype));
         if (etype == EquExprLinear && (!equmeta || equmeta[i].role == EquConstraint)) {
            n_lincons++;
         }
      }

      ei_src_start = ei_src_end + 1;
      idx++;
   } while (idx <= n_objequs);

   assert(n_lincons <= cons_size);
   mcpstats->n_lincons = n_lincons;

   /* skip_equ is now the number of equations from the source model not expected
   * to be in the MCP */
   fooc_dat.skipped_equ = objequs->size;

  /* NL equs start at n_foocvars, LIN equs start at mcp_size - nb_lin */
   if (mcp_size < n_lincons + n_foocvars) {
      error("[fooc] ERROR in %s model '%.*s' #%u: Number of linear constraint (%zu) "
            "is greater than the number of multipliers (%zu)!\n", mdl_fmtargs(mdl_src),
            n_lincons, mcp_size-n_foocvars);
      status = Error_RuntimeError;
      goto _exit;
   }

   mcpstats->n_nlcons = cons_size - n_lincons;

   fooc_dat.ei_F_start = fooc_dat.n_equs_orig;
   fooc_dat.ei_cons_start = fooc_dat.n_equs_orig + fooc_dat.info->n_foocvars;
   fooc_dat.ei_lincons_start = fooc_dat.ei_cons_start + fooc_dat.info->n_nlcons;

   int offset;
   trace_fooc("[fooc] %n%s model '%.*s' #%u has an MCP of size %zu.\n", &offset,
              mdl_fmtargs(mdl_src), mcp_size);
   trace_fooc("%*s%zu primal variables: %zu for FOOC; %zu auxiliary\n", offset, "",
              n_primalvars, mcpstats->n_foocvars,
              mcpstats->n_auxvars != SSIZE_MAX ? mcpstats->n_auxvars : 0);
   trace_fooc("%*s%zu VI function(s); %zu zero VI function(s); %zu constraint(s)"
              " (%zu linear, %zu non-linear)\n", offset, "", n_vifuncs,
              mcpstats->n_vizerofuncs, cons_size, n_lincons, mcpstats->n_nlcons);

  /* ----------------------------------------------------------------------
   * We need to perform the filtering here since the rosetta_equ array
   * created by rmdl_compress_equs() would not be correct.
   * Moreover, extreme care would have to be taken when copying the equations
   * ---------------------------------------------------------------------- */

   struct equ_inh_names equ_inh_names;
   S_CHECK_EXIT(setup_equvarnames(&equ_inh_names, &fooc_dat, cdat_mcp, mdl_mcp));

   cdat_mcp->total_n += cons_size;

   S_CHECK_EXIT(inject_vifunc_and_cons(mdl_src, mdl_mcp, &fooc_dat, fops,
                                       &equ_inh_names, &rosettas_vars));

  /* ----------------------------------------------------------------------
   * This is needed to set the proper index values. It is a bit inefficient
   * as with many VI functions, it is going to write the same value.
   * ---------------------------------------------------------------------- */

  for (size_t i = fooc_dat.ei_F_start, end = fooc_dat.ei_cons_start; i < end; ++i) {
    ctr_mcp->equs[i].idx = i;
    ctr_mcp->equs[i].object = Mapping;
  }

  /* ----------------------------------------------------------------------
   * 3. Add the functions corresponding to the primal part of the MCP
   *   - First, collect all the NL and linear constraints in the equation
   *     containers cons_nl and fooc_dat.cons_lin with indices in the MCP space
   *   - Then, perform the construction of all dLdx functions
   *
   *  If there is no constraint, we just move on and set the aequ sizes to 0
   * ---------------------------------------------------------------------- */

   MpIdArray *mps = &fooc_dat.mps;
   unsigned mps_len = mps->len;

   if (mps_len > 0 ) {
    if (!fooc_dat.mp2objequ) {
      error("%s ERROR: Need allocated mp2objequ\n", __func__);
      status = Error_RuntimeError;
      goto _exit;
    }
      // expected size cons_size - nb_lin
      // expected size nb_lin
      S_CHECK_EXIT(aequ_setblock(&fooc_dat.cons_nl, mps_len));
      S_CHECK_EXIT(aequ_setblock(&fooc_dat.cons_lin, mps_len));
      aequ_setsize(&fooc_dat.cons_nl, cons_size - n_lincons);
      aequ_setsize(&fooc_dat.cons_lin, n_lincons);

      AequBlock *restrict cons_nl_blk = aequ_getblocks(&fooc_dat.cons_nl);
      AequBlock *restrict cons_lin_blk = aequ_getblocks(&fooc_dat.cons_lin);
      assert(cons_nl_blk && cons_lin_blk);

      rhp_idx * restrict nl_list_mp = NULL;
      rhp_idx * restrict lin_list_mp = NULL;
      if (cons_size > 0) {                 /* Avoids an allocation of size 0 */
         MALLOC_EXIT(cons_idxs, rhp_idx, cons_size);
         nl_list_mp = cons_idxs;
         lin_list_mp = &cons_idxs[mcpstats->n_nlcons];
      }

      /* Now the real linear equation start in the FOOC index space */
      rhp_idx ei_lincons_start = fooc_dat.ei_lincons_start;
      rhp_idx ei_cons_start = fooc_dat.ei_cons_start;

      for (size_t i = 0; i < mps_len; ++i) {
         if (cons_size == 0) {
            assert(n_lincons == 0);
            aequ_setcompact(&cons_nl_blk->e[i], 0, IdxInvalid);
            aequ_setcompact(&cons_lin_blk->e[i], 0, IdxInvalid);
            continue;
         }

         MathPrgm *mp;
         S_CHECK_EXIT(empdag_getmpbyid(empdag, mpidarray_at(mps, i), &mp));
         assert(mp);

         IdxArray *restrict equs_mp = &mp->equs; assert(equs_mp);
         unsigned n_nl_mp = 0, n_lin_mp = 0;

         for (size_t j = 0, len = equs_mp->len; j < len; ++j) {

            rhp_idx ei = equs_mp->arr[j]; assert(valid_ei(ei));
            rhp_idx ei_mcp = rosetta_equ[ei];

            if (!valid_ei(ei_mcp) || ei_mcp < ei_cons_start) {
               /* Skip this equation: deleted or is a VI function  */

            } else if (ei_mcp >= ei_lincons_start) {    /* linear constraint */
               assert(n_lin_mp < fooc_dat.info->n_lincons);
               lin_list_mp[n_lin_mp++] = ei_mcp;

            } else {                                        /* NL constraint */
               assert(n_nl_mp < fooc_dat.info->n_nlcons);
               nl_list_mp[n_nl_mp++] = ei_mcp;
            }
         }

         aequ_setlist(&cons_nl_blk->e[i], n_nl_mp, nl_list_mp);
         aequ_setlist(&cons_lin_blk->e[i], n_lin_mp, lin_list_mp);
         nl_list_mp = &nl_list_mp[n_nl_mp];
         lin_list_mp = &lin_list_mp[n_lin_mp];
      }
      assert(nl_list_mp - cons_idxs == (ei_lincons_start - fooc_dat.info->n_foocvars));
      assert(lin_list_mp - cons_idxs == cons_size);

      rhp_idx * restrict mp2objequ = fooc_dat.mp2objequ;

      /* -------------------------------------------------------------------
       * Compute all the dLdx expressions and add the normal cones contributions
       * ------------------------------------------------------------------- */

      for (size_t i = 0; i < mps_len; i++) {
         MathPrgm *mp;
         S_CHECK_EXIT(empdag_getmpbyid(empdag, mpidarray_at(mps, i), &mp));
         assert(mp);

         Aequ *cons_nl_mp = &cons_nl_blk->e[i];
         Aequ *cons_lin_mp = &cons_lin_blk->e[i];

         switch (mp->type) {
         case MpTypeOpt: {
            rhp_idx objequ;
            unsigned objequ_idx = mp2objequ[i];

            if (valid_idx(objequ_idx)) {
               assert(objequ_idx < objequs->size);
               objequ = objequs->list[objequ_idx];
            } else {
               objequ = IdxNA;
            }

            S_CHECK_EXIT(fooc_mcp_primal_opt(mdl_mcp, mp, cons_nl_mp, cons_lin_mp,
                                             mdl_src, &fooc_dat, objequ));
            break;
         }

         /* See GITLAB #83 */
         //      case RHP_MP_MCP:
         //        S_CHECK_EXIT(fooc_mcp_primal_mcp(mdl_mcp, mp_, mdl_src));
         //        break;

         case MpTypeVi:
            S_CHECK_EXIT(
               fooc_mcp_primal_vi(mdl_mcp, mp, cons_nl_mp, cons_lin_mp, mdl_src, &fooc_dat));
            break;

         default:
            error("%s ERROR: unsupported MP of type %s\n", __func__, mp_gettypestr(mp));
            status = Error_NotImplemented;
            goto _exit;
         }
      }
   } else { /* No HOP structure  */
      aequ_setcompact(&fooc_dat.cons_nl, cons_size - n_lincons, fooc_dat.ei_cons_start);
      aequ_setcompact(&fooc_dat.cons_lin, fooc_dat.info->n_lincons, fooc_dat.ei_lincons_start);

      if (objequs->size > 0) {

         assert(objequs->size == 1 && objequs->type == EquVar_Compact);
         rhp_idx objequ = aequ_fget(objequs, 0);
         S_CHECK_EXIT(fooc_mcp_primal_opt(mdl_mcp, NULL, &fooc_dat.cons_nl,
                                          &fooc_dat.cons_lin, mdl_src, &fooc_dat, objequ));

      } else {

         ModelType mdltype;
         S_CHECK_EXIT(mdl_gettype(mdl_src, &mdltype));

         if (mdltype == MdlType_mcp) {
            S_CHECK_EXIT(fooc_mcp_primal_mcp(mdl_mcp, NULL, mdl_src));

         } else if (mdltype == MdlType_vi ||
            (mdltype == MdlType_emp && mdl_src->empinfo.empdag.type == EmpDag_Single_Vi)) {
            S_CHECK_EXIT(
               fooc_mcp_primal_vi(mdl_mcp, NULL, &fooc_dat.cons_nl, &fooc_dat.cons_lin, mdl_src, &fooc_dat));

         } else { /* TODO: we could have a feasibility problem here */
            error("%s ERROR: unsupported model of type %s\n", __func__,
                  mdl_getprobtypetxt(mdltype));
            status = Error_WrongModelForFunction;
            goto _exit;
         }
      }
   }

  /* ----------------------------------------------------------------------
   * If needed, set the matching information. This could be absent if the
   * variable is not present in the Lagrangian 
   * ---------------------------------------------------------------------- */

   rhp_idx * restrict ei_F2vi_primal = fooc_dat.ei_F2vi_primal;
   const EquMeta * restrict emeta = ctr_mcp->equmeta;

   for (size_t i = fooc_dat.ei_F_start; i < fooc_dat.ei_cons_start; ++i) {
      if (!valid_vi(emeta[i].dual)) { 
         rhp_idx vi = ei_F2vi_primal ? ei_F2vi_primal[i-fooc_dat.ei_F_start] : i;
         S_CHECK_EXIT(rctr_setequvarperp(ctr_mcp, i, vi));
      }
      
      S_CHECK_EXIT(cmat_sync_lequ(ctr_mcp, &ctr_mcp->equs[i]));
      S_CHECK_EXIT(rctr_fix_equ(ctr_mcp, i));
   }

  /* ------------------------------------------------------------------------
   * Perform some final checks
   * - number of active variables could be larger than expected, as in the MPEC case
   * ------------------------------------------------------------------------ */

   size_t nvars_expected = fooc_dat.info->n_primalvars + fooc_dat.info->n_constraints;
   if (ctr_mcp->n < nvars_expected) {
      error("[fooc] ERROR: the number of active variables %zu is smaller than the "
            "expected size %zu\n", (size_t)ctr_mcp->n, nvars_expected);
      rmdl_debug_active_vars(ctr_mcp);
      status = Error_RuntimeError;
      goto _exit;
   }

   if (ctr_mcp->m != cdat_mcp->total_m) {
    error("[fooc] ERROR: the container has %zu active equations, but %zu are "
          "expected.\n", (size_t)ctr_mcp->m, cdat_mcp->total_m);
    rctr_debug_active_equs(ctr_mcp);
    status = Error_RuntimeError;
    goto _exit;
  }

_exit:

   if (status == OK) {
      if (objequs->size > 0) {
         status = aequ_extendandown(ctr_src->func2eval, objequs);
      }
   }

   rosettas_free(&rosettas_vars);
   fooc_data_fini(&fooc_dat);
   FREE(cons_idxs);

   return status;
}


/**
 * @brief Compute the first order optimality conditions as an MCP
 *
 * @param mdl      the model to solve
 * @param mcpdata  the jacobian data
 *
 * @return         the error code
 */
int fooc_create_mcp(Model *mdl)
{
   double start = get_thrdtime();

   assert(mdl->mdl_up && mdl_is_rhp(mdl->mdl_up));

   mdl->empinfo.empdag.type = EmpDag_Empty;

   S_CHECK(fooc_mcp(mdl));

   mdl->timings->solve.fooc += get_thrdtime() - start;

   return OK;
}

/**
 * @brief Compute the first order optimality conditions as a VI
 *
 * @param mdl      the model
 * @param mcpdata  the jacobian data
 *
 * @return         the error code
 */
int fooc_create_vi(Model *mdl) {
  return Error_NotImplemented;
}
