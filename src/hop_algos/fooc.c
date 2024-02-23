#include "reshop_config.h"
#include "asnan.h"

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
#include "fooc_priv.h"
#include "lequ.h"
#include "macros.h"
#include "mathprgm.h"
#include "mdl.h"
#include "mdl_rhp.h"
#include "ctrdat_rhp.h"
#include "rhp_alg.h"
#include "rmdl_data.h"
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

typedef struct {
   rhp_idx *vi_primal2ei_F;
   rhp_idx *ei_F2vi_primal;
   rhp_idx ei_F_start;
   rhp_idx ei_cons_start;
   rhp_idx ei_lincons_start;
   rhp_idx vi_mult_start;
   unsigned n_equs_orig;
   struct {
      unsigned total_m;
      unsigned total_n;
   } src;
   unsigned skipped_equ;
   McpStats *stats;
} McpInfo;


/* TODO: LOW rethink this */
static const char* empdagc_err(unsigned len)
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
   error("[FOOC] ERROR: the variable '%s' is the objective variable in MP(%s),"
         " and its objective function is '%s'. This is unsupported, the MP must"
         " have exactly one of these\n",
         mdl_printvarname(mdl, objvar), mdl_printequname(mdl, objequ),
         empdag_getmpname(empdag, mp->id));
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
int setup_equvarnames(struct equ_inh_names * equnames, McpInfo *mcpinfo,
                      RhpContainerData *cdat_mcp, const Model* mdl_fooc)
{
   rhp_idx_init(&equnames->cons_src);

   size_t n_cons = mcpinfo->stats->n_constraints;
   S_CHECK(rhp_idx_reserve(&equnames->cons_src, n_cons));

   Aequ e, e_src;
   aequ_setcompact(&e, n_cons, mcpinfo->ei_cons_start);
   S_CHECK(aequ_extendandown(&cdat_mcp->equname_inherited.e, &e));
   aequ_setandownlist(&e_src, n_cons, equnames->cons_src.arr);
   S_CHECK(aequ_extendandown(&cdat_mcp->equname_inherited.e_src, &e_src));
 

   struct vnames *vnames_var = &cdat_mcp->var_names.v;
   vnames_var->type = VNAMES_MULTIPLIERS;
   vnames_var->start = mcpinfo->vi_mult_start;
   vnames_var->end = mcpinfo->vi_mult_start + n_cons - 1;

   struct vnames *vnames_equ = &cdat_mcp->equ_names.v;
   assert(vnames_equ->type == VNAMES_UNSET);

   vnames_equ->type = VNAMES_FOOC_LOOKUP;
   vnames_equ->start = mcpinfo->ei_F_start;
   vnames_equ->end = mcpinfo->ei_cons_start-1;

   A_CHECK(vnames_equ->fooc_lookup, vnames_lookup_new(mcpinfo->stats->n_primalvars,
                                                      mdl_fooc,
                                                      mcpinfo->ei_F2vi_primal));

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

static int fill_objequs_and_get_vifuncs(const Model *mdl_src, const MathPrgm *mp,
                                        const Mpe *mpe, McpInfo *mcpinfo,
                                        Aequ *objequs, rhp_idx **mp2objequ)
{
  size_t lvi_funcs = 0, lvi_zerofuncs = 0;
  rhp_idx *mps2objequs;
  rhp_idx *objequs2mps;
  const Container *ctr_src = &mdl_src->ctr;
   size_t total_m = mcpinfo->src.total_m;

  if (mp) {
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
         aequ_setcompact(objequs, 1, objequ);
      }
      break;
    }
    case MpTypeVi:
/* See GITLAB #83 */
//    case MpTypeMcp:
       lvi_zerofuncs = mp_getnumzerofunc(mp);
       lvi_funcs = mp_getnumvars(mp) - lvi_zerofuncs;
       break;
    default:
       error("%s :: not implemented for MP type %s", __func__, mp_gettypestr(mp));
       return Error_NotImplemented;
    }

  } else if (mpe) {

    bool do_exit = false;
    rhp_idx *objequs_list;
    const EmpInfo *empinfo_src = &mdl_src->empinfo;
    const EmpDag *empdag = &empinfo_src->empdag;

    UIntArray mps;
    S_CHECK(empdag_mpe_getchildren(empdag, mpe->id, &mps));
    unsigned mpe_len = mps.len;
    if (!valid_idx(mpe_len)) {
      error("%s :: invalid MPE %s, error is ``%s''", __func__,
               empdag_getmpename(empdag, mpe->id), empdagc_err(mpe_len));
      return Error_EMPIncorrectInput;
    }
    MALLOC_(objequs_list, rhp_idx, mpe_len);
    aequ_setandownlist(objequs, UINT_MAX, objequs_list);
    unsigned nb_objequs = 0;
    MALLOC_(mps2objequs, rhp_idx, 2 * mpe_len);
    memset(mps2objequs, IdxNA,  (2 * mpe_len)*sizeof(rhp_idx));
    *mp2objequ = mps2objequs;
    objequs2mps = &mps2objequs[mpe_len];

    for (unsigned i = 0; i < mpe_len; i++) {
      MathPrgm *_mp;
      S_CHECK(empdag_getmpbyuid(empdag, rhp_uint_at(&mps, i), &_mp));

      switch (_mp->type) {
      case MpTypeOpt: {

        rhp_idx objvar = mp_getobjvar(_mp);
        rhp_idx objequ = mp_getobjequ(_mp);
        bool valid_objvar = valid_vi(objvar), valid_objequ = valid_ei(objequ);

        /* with a valid objvar, we do nothing  */
        if (valid_objvar) {
          if (valid_objequ) {
            err_objvar_(mdl_src, _mp, objvar, objequ);
            do_exit = true;
          } 
        } else if (!valid_objequ) { /* error if no objequ */
            mp_err_noobjdata(_mp);
            do_exit = true;
        } else {
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
         unsigned mp_zerofuncs = mp_getnumzerofunc(_mp);
         lvi_zerofuncs += mp_zerofuncs;
         lvi_funcs += mp_getnumvars(_mp) - mp_zerofuncs;
         mps2objequs[i] = IdxNA;
      }
      break;

      default:
        error("%s :: unsupported MP of type %s\n", __func__, mp_gettypestr(_mp));
        return Error_NotImplemented;
      }

      /* If one of more fatal error were encountered  */
      if (do_exit) {
        return Error_EMPIncorrectInput;
      }
    }

    aequ_setsize(objequs, nb_objequs);

  } else { /* No HOP structure  */

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
      return Error_Unconsistency;
    }

      if (valid_ei(objequ)) {
         aequ_setcompact(objequs, 1, objequ);
      } else if (!valid_vi(objvar) && sense != RhpFeasibility) {
         error("[fooc] ERROR in %s model '%.*s' #%u: sense is %s, but neither "
               "an objective variable nor an objective equation have been given\n",
               mdl_fmtargs(mdl_src), sense2str(sense));
         return Error_Unconsistency;
      } 

    if (sense == RhpFeasibility) {
         ProbType probtype;
         S_CHECK(mdl_getprobtype(mdl_src, &probtype));
          if (probtype == MdlProbType_vi) {
            VarMeta *vmd = mdl_src->ctr.varmeta;

            for (unsigned i = 0, len = ctr_nvars(&mdl_src->ctr); i < len; ++i) {
               unsigned basictype = vmd_basictype(vmd[i].ppty);
               if (basictype == VarPerpToViFunction) { lvi_funcs++; }
               if (basictype == VarPerpToZeroFunctionVi) { lvi_zerofuncs++; }
            }
         }
      }
   }

  mcpinfo->stats->n_vifuncs = lvi_funcs;
  mcpinfo->stats->n_vizerofuncs = lvi_zerofuncs;


   if (O_Output & PO_TRACE_FOOC) {
      unsigned n_objequ = aequ_size(objequs);
      trace_fooc("[fooc] Found %u objective equations\n", n_objequ);
      for (unsigned i = 0; i < n_objequ; ++i) {
         trace_fooc("%*c %s\n", 6, ' ', mdl_printequname(mdl_src, aequ_fget(objequs, i)));
      }
      trace_fooc("[fooc] Found %zu VI functions\n", lvi_funcs);
      trace_fooc("[fooc] Found %zu zero VI functions\n", lvi_zerofuncs);
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
      ediff.object = EQ_MAPPING;

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

static int inject_vifunc_and_cons(Model *mdl_src, Model *mdl_mcp, McpInfo *mcpinfo,
                                  Aequ *objequs, const Fops * const fops_equ,
                                  struct equ_inh_names *equ_inh_names,
                                  const Rosettas * restrict rosettas_vars)
{
   Container * restrict ctr_mcp = &mdl_mcp->ctr;
   Container * restrict ctr_src = &mdl_src->ctr;
   RhpContainerData *cdat_mcp = (RhpContainerData *)ctr_mcp->data;

   //size_t ei_lin    = mcpstats->mcp_size - mcpstats->n_lincons;
   //size_t ei_start  = mcpstats->n_primal_vars;
   size_t ei_lin    = mcpinfo->ei_lincons_start;
   size_t ei_start  = mcpinfo->ei_cons_start;
   size_t ei_nl     = ei_start;
   size_t vi_mult   = mcpinfo->vi_mult_start;
   rhp_idx *vi2ei_F = mcpinfo->vi_primal2ei_F;

   /* We need to evaluate the VI functions when reporting the solution */
   unsigned n_vifuncs = mcpinfo->stats->n_vifuncs;
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

  /* TODO(xhub) URG this seems unused, why? */
  /*
  int *var_in_equ = (int*)working_mem.ptr;
  double *jac_vals = (double*)&var_in_equ[var_size];
  bool *nlflags = (bool*)&jac_vals[var_size];
  */

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
   size_t total_m_src = mcpinfo->src.total_m;
   unsigned skipped_equ = mcpinfo->skipped_equ;

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

         if (equ.object == EQ_MAPPING) {
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
         } else if (equ.object == EQ_CONE_INCLUSION) {

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

         if (edst->object == EQ_CONE_INCLUSION) {

            /*  TODO(xhub) add prox pert? */
            S_CHECK(cmat_sync_lequ(ctr_mcp, edst));

            /* -------------------------------------------------------------
           * Now we can change the constraint to a mapping
           * ------------------------------------------------------------- */

            edst->object = EQ_MAPPING;

            S_CHECK(rctr_setequvarperp(ctr_mcp, ei, vi));

         } else if (edst->object == EQ_MAPPING) {

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

   size_t n_vizerofunc = mcpinfo->stats->n_vizerofuncs;

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
            S_CHECK(rctr_init_equ_empty(ctr_mcp, ei, EQ_MAPPING, CONE_NONE));
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

   mcpinfo->skipped_equ = skipped_equ;
   ctr_mcp->m = mcpinfo->n_equs_orig + mcpinfo->stats->mcp_size;

   /* Just 2 consistency checks  */
   assert(ei_lin == (mcpinfo->stats->mcp_size + mcpinfo->n_equs_orig));
   assert(ei_nl == (ei_lin - mcpinfo->stats->n_lincons));

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
                               McpInfo *restrict mcpinfo,
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

   rhp_idx *vi_primal2ei_F = mcpinfo->vi_primal2ei_F;

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
    ediff.object = EQ_MAPPING;

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

      assert(ei >= mcpinfo->ei_F_start && ei < mcpinfo->ei_cons_start);

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
 * @param ctr_mcp      the destination container
 * @param mp           the mathematical programm (optional)
 * @param cequ_nl      the nonlinear constraints (only those belonging to the
 * MP, if any)
 * @param cequ_lin     the linear constraints (only those belonging to the MP,
 * if any)
 * @param ctr_src      the source container
 *
 * @return             the error code
 */
static int fooc_mcp_primal_vi(Model *mdl_mcp, MathPrgm *mp,
                              Aequ *cequ_nl, Aequ *cequ_lin,
                              Model *mdl_src, McpInfo *mcpinfo) {
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

   rhp_idx *vi_primal2ei_F = mcpinfo->vi_primal2ei_F;

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
 * @brief Create a permutation list from variable indices in the source model
 * to a subset based on a subdag.
 *
 * @param mdl_src 
 * @param subdag_root 
 * @param vi_primal2ei_F 
 * @return 
 */
static int subdag_create_permutations(Model *mdl_src, daguid_t subdag_root,
                                      rhp_idx **vi_primal2ei_F,
                                      rhp_idx **ei_F2vi_primal)
{
   const EmpDag *empdag = &mdl_src->empinfo.empdag;

   UIntArray mplist;
   rhp_uint_init(&mplist);

   S_CHECK(empdag_subdag_getmplist(empdag, subdag_root, &mplist));

   assert(mplist.len > 0);

   VarMeta * restrict vmeta = mdl_src->ctr.varmeta;
   assert(vmeta);

   Fops *fops_ctr = rmdl_getfops(mdl_src);

   rhp_idx nvars_src = ctr_nvars_total(&mdl_src->ctr);
   size_t nvars_primal, dummy;
   if (fops_ctr) { fops_ctr->get_sizes(fops_ctr->data, &nvars_primal, &dummy); }
   else { nvars_primal = nvars_src; }

   rhp_idx *vi2ei_F, *ei_F2vi;
   MALLOC_(vi2ei_F, rhp_idx, nvars_src);
   MALLOC_(ei_F2vi, rhp_idx, nvars_primal);
   *vi_primal2ei_F = vi2ei_F;
   *ei_F2vi_primal = ei_F2vi;

      /*TODO GITLAB #109*/
   rhp_idx ei_F = 0;
   for (rhp_idx i = 0, vi = 0; i < nvars_src; ++i) {
      if (fops_ctr && !fops_ctr->keep_var(fops_ctr->data, i)) {
         continue;
      }

      if (rhp_uint_findsorted(&mplist, vmeta[i].mp_id) != UINT_MAX) {
         ei_F2vi[ei_F] = vi;
         vi2ei_F[vi] = ei_F++;
      } else {
         vi2ei_F[vi] = IdxInvalid;
      }

      vi++;
   }

   return OK;
}

/**
 * @brief Build the first order condition of an optimization problem or an
 * equilibrium
 *
 * @param  mdl_mcp    the MCP model to be created, with the problem to be
 *                    transformed into an MCP as source problem
 * @param  mcpdef     Data needed to define the MCP
 * @param  mcpstats   Statistics about the MCP
 * 
 * @return            the error code
 */
int fooc_mcp(Model *mdl_mcp, McpDef *mcpdef, McpStats * restrict mcpstats)
{

  /* ----------------------------------------------------------------------
   * TODO: how much is this still true?
   * A few assumptions on the input data:
   * - the container must be RHP (could be fixed by making the Var and
   *   Equ really modeling language independent
   * - the given model is already compressed: no inactive variables or
   *   equations
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

   McpInfo mcpinfo = {.stats = mcpstats};

   if (cdat_mcp->total_m != ctr_mcp->m) {
      error("[fooc] ERROR: the MCP model has a different number of total and "
            "active equations: %zu vs %u\n", cdat_mcp->total_m, ctr_mcp->m);
      return Error_Unconsistency;
   }

  Aequ objequs, cons_nl, cons_lin;
  aequ_init(&objequs);
  aequ_init(&cons_nl);
  aequ_init(&cons_lin);

  Rosettas rosettas_vars;
  rosettas_init(&rosettas_vars);
  rhp_idx *cons_idxs = NULL, *mp2objequ = NULL;

   Model *mdl_src = mdl_mcp->mdl_up;
   if (!mdl_src) {
      error("%s :: the source container is missing!\n", __func__);
      return Error_NullPointer;
   }

   Container *ctr_src = &mdl_src->ctr;
   EmpDag *empdag = NULL;

   if (ctr_src->rosetta_equs) {
      error("%s :: a rosetta array is already present, this is not allowed", __func__);
      status = Error_UnExpectedData;
      goto _exit;
   }

   size_t var_size, equ_size;

   MathPrgm *mp = NULL;
   Mpe *mpe = NULL;
   Fops *fops_subdag = NULL, *fops_ctr = NULL; //mcpdef->fops_vars;
   bool is_subdag, delete_fops_subdag = false;

   daguid_t empdag_uid = mcpdef->uid;
   if (empdag_uid != EMPDAG_UID_NONE) {

      unsigned id = uid2id(empdag_uid);

      A_CHECK(empdag, &mdl_src->empinfo.empdag);
      if (uidisMP(empdag_uid)) {
         S_CHECK_EXIT(empdag_getmpbyid(empdag, id, &mp));
      } else {
         S_CHECK_EXIT(empdag_getmpebyid(empdag, id, &mpe));
      }

      if (!empdag_isroot(empdag, empdag_uid)) {
         is_subdag = delete_fops_subdag = true;
         MALLOC_(fops_subdag, Fops, 1);
         S_CHECK(fops_subdag_init(fops_subdag, mdl_src, empdag_uid));
         S_CHECK(subdag_create_permutations(mdl_src, empdag_uid,
                                            &mcpinfo.vi_primal2ei_F,
                                            &mcpinfo.ei_F2vi_primal));

      } else {
         mcpinfo.vi_primal2ei_F = NULL;
         mcpinfo.ei_F2vi_primal = NULL;
         is_subdag = false;
//         if (mcpdef->fops_vars) {
//            errormsg("[fooc] ERROR: MCP definition contained fops for variables,"
//                  " but the first-order optimality condition for the whole DAG "
//                  "are going to be created. This doesn't make sense,\n");
//            return Error_Unconsistency;
//         }
      }
   } else {
      mcpinfo.vi_primal2ei_F = NULL;
      mcpinfo.ei_F2vi_primal = NULL;
   }

  /* ----------------------------------------------------------------------
   * TODO(xhub) this is a bit smelly
   * ---------------------------------------------------------------------- */

   if (!fops_ctr && mdl_is_rhp(mdl_src)) {
      fops_ctr = rmdl_getfops(mdl_src);
   }

   mcpinfo.src.total_m = ctr_nequs_total(ctr_src);
   mcpinfo.n_equs_orig = ctr_nequs(ctr_mcp);
   mcpinfo.src.total_n = ctr_nvars_total(ctr_src);
   mcpinfo.vi_mult_start = ctr_nvars(ctr_src);

   size_t n_primalvars, n_equs4mcp;

   if (fops_ctr) {
      fops_ctr->get_sizes(fops_ctr->data, &var_size, &equ_size);
   } else {
      var_size = ctr_src->n;
      equ_size = ctr_src->m;
   }

   if (fops_subdag) {
      fops_subdag->get_sizes(fops_subdag->data, &n_primalvars, &n_equs4mcp);
   } else {
      n_primalvars = var_size; 
      n_equs4mcp = equ_size; 
   }

   Fops *fops_equ = fops_subdag ? fops_subdag : fops_ctr;

   /* ----------------------------------------------------------------------
   * Init the MCP as a square system.
   *
   * - Fill objequs as the objective variable(s) or equation(s) shall not be
   *   copied
   * - Get the number of VI functions and VI zero functions
   * ---------------------------------------------------------------------- */

   S_CHECK_EXIT(fill_objequs_and_get_vifuncs(mdl_src, mp, mpe, &mcpinfo,
                                             &objequs, &mp2objequ));

  /* ----------------------------------------------------------------------
   * The total number of constraints is obtain from the number of incoming
   * equations minus:
   * - the number of objective variables
   * - the number of VI relations
   * ---------------------------------------------------------------------- */

  size_t n_vifuncs = mcpstats->n_vifuncs;
  assert(n_equs4mcp >= objequs.size + n_vifuncs);
  const size_t cons_size = n_equs4mcp - (objequs.size + n_vifuncs);
  size_t mcp_size = cons_size + n_primalvars;

   mcpstats->mcp_size = mcp_size;
   mcpstats->n_primalvars = n_primalvars;
   mcpstats->n_constraints = cons_size;

  /* ----------------------------------------------------------------------
   * Reserve all the variables and equations
   * ATTENTION: set total_m already here to allow for some debugging info
   * ---------------------------------------------------------------------- */

   /* TODO GITLAB #108*/
   S_CHECK_EXIT(ctr_resize(ctr_mcp, ctr_src->n+cons_size, mcp_size));
//  S_CHECK_EXIT(rctr_reserve_equs(ctr_mcp, mcp_size));
//  S_CHECK_EXIT(rctr_reserve_vars(ctr_mcp, ctr_src->n+cons_size));

  cdat_mcp->total_m = mcp_size;

  S_CHECK_EXIT(mdl_setprobtype(mdl_mcp, MdlProbType_mcp));

   /* TODO: delete? This should already be done when creating the ctr_mcp? */
  S_CHECK_EXIT(rctr_inherit_pool(ctr_mcp, ctr_src));

   if (!ctr_src->rosetta_vars) {
      MALLOC_EXIT(ctr_src->rosetta_vars, rhp_idx, ctr_nvars_total(ctr_src));
   }

  /*  We allocate + 1 since it simplifies the logic */
  MALLOC_EXIT(ctr_src->rosetta_equs, rhp_idx, mcpinfo.src.total_m + 1);
  rhp_idx *rosetta_equ = ctr_src->rosetta_equs;

  if (fops_subdag || fops_ctr) { /* With filtering, all equation data is copied */
    rctr_inherited_equs_are_not_borrowed(ctr_mcp);
  }

  /* ----------------------------------------------------------------------
   * 1. Take care of variables in the MCP model:
   *   - Compress primal variables and introduce them in the MCP model.
   *   - Set multiplier names
   * ---------------------------------------------------------------------- */

  //S_CHECK_EXIT(rctr_compress_vars_x(ctr_src, ctr_mcp, fops_ctr));
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
  size_t i_objequs = 0;
  size_t n_lincons = 0;
  const EquMeta * restrict equmeta = ctr_src->equmeta;
  do {
    size_t ei_src_end =
        i_objequs < objequs.size ? aequ_fget(&objequs, i_objequs) : mcpinfo.src.total_m;

    assert(ei_src_end <= mcpinfo.src.total_m);

    for (size_t i = ei_src_start; i < ei_src_end; ++i) {
      if (fops_equ && !fops_equ->keep_equ(fops_equ->data, i)) { continue; }

      Equ *e = &ctr_src->equs[i];
      S_CHECK_EXIT(rctr_getnl(ctr_src, e));

         /* TODO: this is fragile */
      if (!(e->tree && e->tree->root) &&
          (!equmeta || equmeta[i].role == EquConstraint)) {
       n_lincons++;
      }
    }

    ei_src_start = ei_src_end + 1;
    i_objequs++;
  } while (i_objequs <= objequs.size);

   assert(n_lincons <= cons_size);
   mcpstats->n_lincons = n_lincons;

  /* skip_equ is now the number of equations from the source model not expected
   * to be in the MCP */
  mcpinfo.skipped_equ = objequs.size;

  /* NL equs start at var_size, LIN equs start at mcp_size - nb_lin */
   if (mcp_size < n_lincons + n_primalvars) {
      error("[fooc] ERROR in %s model '%.*s' #%u: Number of linear constraint (%zu)"
            "is greater than the number of multipliers (%zu)!\n", mdl_fmtargs(mdl_src),
            n_lincons, mcp_size-n_primalvars);
      status = Error_RuntimeError;
      goto _exit;
   }

   mcpstats->n_nlcons = cons_size - n_lincons;

   mcpinfo.ei_F_start = mcpinfo.n_equs_orig;
   mcpinfo.ei_cons_start = mcpinfo.n_equs_orig + mcpinfo.stats->n_primalvars;
   mcpinfo.ei_lincons_start = mcpinfo.ei_cons_start + mcpinfo.stats->n_nlcons;

   trace_fooc("[fooc] MCP of size %zu has %zu primal variables; %zu VI function(s); %zu zero"
              " VI function(s); %zu constraint(s) (%zu linear, %zu non-linear)\n",
              var_size + cons_size, var_size, n_vifuncs, mcpstats->n_vizerofuncs,
              cons_size, n_lincons, mcpstats->n_nlcons);

  /* ----------------------------------------------------------------------
   * We need to perform the filtering here since the rosetta_equ array
   * created by rmdl_compress_equs() would not be correct.
   * Moreover, extreme care would have to be taken when copying the equations
   * ---------------------------------------------------------------------- */

   struct equ_inh_names equ_inh_names;
   S_CHECK_EXIT(setup_equvarnames(&equ_inh_names, &mcpinfo, cdat_mcp, mdl_mcp));

   cdat_mcp->total_n += cons_size;

   S_CHECK_EXIT(inject_vifunc_and_cons(mdl_src, mdl_mcp, &mcpinfo, &objequs,
                                       fops_equ, &equ_inh_names, &rosettas_vars));

  /* ----------------------------------------------------------------------
   * This is needed to set the proper index values. It is a bit inefficient
   * as with many VI functions, it is going to write the same value.
   * ---------------------------------------------------------------------- */

  for (size_t i = mcpinfo.ei_F_start, end = mcpinfo.ei_cons_start; i < end; ++i) {
    ctr_mcp->equs[i].idx = i;
    ctr_mcp->equs[i].object = EQ_MAPPING;
  }

  /* ----------------------------------------------------------------------
   * 3. Add the functions corresponding to the primal part of the MCP
   *   - First, collect all the NL and linear constraints in the equation
   *     containers cons_nl and cons_lin with indices in the MCP space
   *   - Then, perform the construction of all dLdx functions
   *
   *  If there is no constraint, we just move on and set the aequ sizes to 0
   * ---------------------------------------------------------------------- */

   if (mpe) {
      UIntArray mps;
      S_CHECK_EXIT(empdag_mpe_getchildren(empdag, mpe->id, &mps));
      unsigned nb_mp = mps.len;
      // expected size cons_size - nb_lin
      // expected size nb_lin
      S_CHECK_EXIT(aequ_setblock(&cons_nl, nb_mp));
      S_CHECK_EXIT(aequ_setblock(&cons_lin, nb_mp));
      aequ_setsize(&cons_nl, cons_size - n_lincons);
      aequ_setsize(&cons_lin, n_lincons);

      AequBlock *restrict cons_nl_blk = aequ_getblocks(&cons_nl);
      AequBlock *restrict cons_lin_blk = aequ_getblocks(&cons_lin);
      assert(cons_nl_blk && cons_lin_blk);

      rhp_idx * restrict nl_list_mp = NULL;
      rhp_idx * restrict lin_list_mp = NULL;
      if (cons_size > 0) {                 /* Avoids an allocation of size 0 */
         MALLOC_EXIT(cons_idxs, rhp_idx, cons_size);
         nl_list_mp = cons_idxs;
         lin_list_mp = &cons_idxs[mcpstats->n_nlcons];
      }

      /* Now the real linear equation start in the FOOC index space */
      rhp_idx ei_lincons_start = mcpinfo.ei_lincons_start;
      rhp_idx ei_cons_start = mcpinfo.ei_cons_start;

      for (size_t i = 0; i < nb_mp; ++i) {
         if (cons_size == 0) {
            assert(n_lincons == 0);
            aequ_setcompact(&cons_nl_blk->e[i], 0, IdxInvalid);
            aequ_setcompact(&cons_lin_blk->e[i], 0, IdxInvalid);
            continue;
         }

         MathPrgm *mp_;
         S_CHECK_EXIT(empdag_getmpbyuid(empdag, rhp_uint_at(&mps, i), &mp_));
         assert(mp_);

         IdxArray *restrict equs_mp = &mp_->equs; assert(equs_mp);
         unsigned n_nl_mp = 0, n_lin_mp = 0;

         for (size_t j = 0, len = equs_mp->len; j < len; ++j) {

            rhp_idx ei = equs_mp->arr[j]; assert(valid_ei(ei));
            rhp_idx ei_mcp = rosetta_equ[ei];

            if (!valid_ei(ei_mcp) || ei_mcp < ei_cons_start) {
               /* Skip this equation: deleted or is a VI function  */

            } else if (ei_mcp >= ei_lincons_start) {    /* linear constraint */
               assert(n_lin_mp < mcpinfo.stats->n_lincons);
               lin_list_mp[n_lin_mp++] = ei_mcp;

            } else {                                        /* NL constraint */
               assert(n_nl_mp < mcpinfo.stats->n_nlcons);
               nl_list_mp[n_nl_mp++] = ei_mcp;
            }
         }

         aequ_setlist(&cons_nl_blk->e[i], n_nl_mp, nl_list_mp);
         aequ_setlist(&cons_lin_blk->e[i], n_lin_mp, lin_list_mp);
         nl_list_mp = &nl_list_mp[n_nl_mp];
         lin_list_mp = &lin_list_mp[n_lin_mp];
      }
      assert(nl_list_mp - cons_idxs == (ei_lincons_start - mcpinfo.stats->n_primalvars));
      assert(lin_list_mp - cons_idxs == cons_size);

   } else {
      aequ_setcompact(&cons_nl, cons_size - n_lincons, mcpinfo.ei_cons_start);
      aequ_setcompact(&cons_lin, mcpinfo.stats->n_lincons, mcpinfo.ei_lincons_start);
   }

  /* ------------------------------------------------------------------------
   * Now compute all the dLdx expressions and add the normal cones contributions
   * ------------------------------------------------------------------------ */

  if (mp) {

    switch (mp->type) {
    case MpTypeOpt: {
      rhp_idx objequ = objequs.size > 0 ? aequ_fget(&objequs, 0) : IdxNA;
      S_CHECK_EXIT(fooc_mcp_primal_opt(mdl_mcp, mp, &cons_nl, &cons_lin,
                                       mdl_src, &mcpinfo, objequ));
      break;
      }
/* See GITLAB #83 */
//    case RHP_MP_MCP:
//      S_CHECK_EXIT(fooc_mcp_primal_mcp(mdl_mcp, mp, mdl_src));
//      break;

    case MpTypeVi:
      S_CHECK_EXIT(
          fooc_mcp_primal_vi(mdl_mcp, mp, &cons_nl, &cons_lin, mdl_src, &mcpinfo));
      break;

    default:
      error("[fooc] ERROR: unsupported MP of type %s\n", mp_gettypestr(mp));
      status = Error_NotImplemented;
      goto _exit;
    }

  } else if (mpe) {
    if (!mp2objequ) {
      error("%s :: Need allocated mp2objequ\n", __func__);
      status = Error_RuntimeError;
      goto _exit;
    }

    AequBlock *restrict cons_nl_blk = aequ_getblocks(&cons_nl);
    AequBlock *restrict cons_lin_blk = aequ_getblocks(&cons_lin);
    assert(cons_nl_blk && cons_lin_blk);

    UIntArray mps;
    S_CHECK_EXIT(empdag_mpe_getchildren(empdag, mpe->id, &mps));

    for (size_t i = 0, n_mp = mps.len; i < n_mp; i++) {
      MathPrgm *mp_;
      S_CHECK_EXIT(empdag_getmpbyuid(empdag, rhp_uint_at(&mps, i), &mp_));
      assert(mp_);

      Aequ *cons_nl_mp = &cons_nl_blk->e[i];
      Aequ *cons_lin_mp = &cons_lin_blk->e[i];

      switch (mp_->type) {
      case MpTypeOpt: {
         rhp_idx objequ;
         unsigned idx = mp2objequ[i];

         if (valid_idx(idx)) {
            assert(idx < objequs.size);
            objequ = objequs.list[idx];
         } else {
            objequ = IdxNA;
         }

        S_CHECK_EXIT(fooc_mcp_primal_opt(mdl_mcp, mp_, cons_nl_mp, cons_lin_mp,
                                         mdl_src, &mcpinfo, objequ));
        break;
      }

/* See GITLAB #83 */
//      case RHP_MP_MCP:
//        S_CHECK_EXIT(fooc_mcp_primal_mcp(mdl_mcp, mp_, mdl_src));
//        break;

      case MpTypeVi:
        S_CHECK_EXIT(
            fooc_mcp_primal_vi(mdl_mcp, mp_, cons_nl_mp, cons_lin_mp, mdl_src, &mcpinfo));
        break;

      default:
        error("%s :: unsupported MP of type %s\n", __func__, mp_gettypestr(mp_));
        status = Error_NotImplemented;
        goto _exit;
      }
    }

  } else { /* No HOP structure  */
    if (objequs.size > 0) {

      assert(objequs.size == 1 && objequs.type == EquVar_Compact);
      rhp_idx objequ = aequ_fget(&objequs, 0);
      S_CHECK_EXIT(fooc_mcp_primal_opt(mdl_mcp, NULL, &cons_nl, &cons_lin,
                                       mdl_src, &mcpinfo, objequ));

    } else {

      ProbType probtype;
      S_CHECK_EXIT(mdl_getprobtype(mdl_src, &probtype));

      if (probtype == MdlProbType_mcp) {
        S_CHECK_EXIT(fooc_mcp_primal_mcp(mdl_mcp, NULL, mdl_src));

      } else if (probtype == MdlProbType_vi ||
            (probtype == MdlProbType_emp && mdl_src->empinfo.empdag.type == EmpDag_Single_Vi)) {
        S_CHECK_EXIT(
            fooc_mcp_primal_vi(mdl_mcp, NULL, &cons_nl, &cons_lin, mdl_src, &mcpinfo));

      } else { /* TODO: we could have a feasibility problem here */
        error("%s :: ERROR: unsupported model of type %s\n", __func__,
                 mdl_getprobtypetxt(probtype));
        status = Error_WrongModelForFunction;
        goto _exit;
      }
    }
  }

  /* ----------------------------------------------------------------------
   * If needed, set the matching information. This could be absent if the
   * variable is not present in the Lagrangian 
   * ---------------------------------------------------------------------- */

   rhp_idx * restrict ei_F2vi_primal = mcpinfo.ei_F2vi_primal;
   const EquMeta * restrict emeta = ctr_mcp->equmeta;
   for (size_t i = mcpinfo.ei_F_start; i < mcpinfo.ei_cons_start; ++i) {
      if (!valid_vi(emeta[i].dual)) { 
         rhp_idx vi = ei_F2vi_primal ? ei_F2vi_primal[i-mcpinfo.ei_F_start] : i;
         S_CHECK_EXIT(rctr_setequvarperp(ctr_mcp, i, vi));
      }
      
      S_CHECK_EXIT(cmat_sync_lequ(ctr_mcp, &ctr_mcp->equs[i]));
      S_CHECK_EXIT(rctr_fix_equ(ctr_mcp, i));
   }

  /* ------------------------------------------------------------------------
   * Perform some final checks
   * - number of active variables could be larger than expected, as in the MPEC case
   * ------------------------------------------------------------------------ */

   size_t nvars_expected = mcpinfo.stats->n_primalvars + mcpinfo.stats->n_constraints;
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
      if (objequs.size > 0) {
         status = aequ_extendandown(ctr_src->func2eval, &objequs);
      }
   }

   rosettas_free(&rosettas_vars);
   aequ_empty(&cons_nl);
   aequ_empty(&cons_lin);
   FREE(cons_idxs);
   FREE(mp2objequ);

   return status;
}

static bool childless_mp(const EmpDag *empdag, mpid_t mpid)
{
   bool res = true;

   if (empdag->mps.Varcs[mpid].len > 0) {
      error("[fooc] ERROR in %s model '%.*s' #%u: MP(%s) has %u VF children. "
            "Computing the first-order optimality conditions is not possible\n",
            mdl_fmtargs(empdag->mdl), empdag_getmpname(empdag, mpid), 
            empdag->mps.Varcs[mpid].len);
      res = false;
   }

   if (empdag->mps.Carcs[mpid].len > 0) {
      error("[fooc] ERROR in %s model '%.*s' #%u: MP(%s) has %u CTRL children. "
            "Computing the first-order optimality conditions is not possible\n",
            mdl_fmtargs(empdag->mdl), empdag_getmpname(empdag, mpid), 
            empdag->mps.Varcs[mpid].len);
      res = false;
   }

   return res;
}
/**
 * @brief Analyse the EMP structure to see if we can compute the first order
 * optimality conditions
 *
 * @param mdl  the model
 *
 * @return         the error code
 */
static NONNULL int fooc_mcp_analyze_emp(Model *mdl, daguid_t *uid) 
{
  int status = OK;

   const EmpInfo *empinfo_src = &mdl->mdl_up->empinfo;
   const EmpDag *empdag = &empinfo_src->empdag;

   assert(empinfo_hasempdag(empinfo_src));

   rhp_idx _uid;
   const UIntArray *roots = &empdag->roots;
   unsigned roots_len = roots->len;

   if (roots_len == 1) {

      _uid = roots->arr[0];

   } else if (roots_len == 0) {
      error("[fooc] ERROR: No root in the EMPDAG for %s model '%.*s' #%u\n",
            mdl_fmtargs(mdl));
      return Error_EMPIncorrectInput;

   } else {
      error("[fooc] ERROR: %u roots in EMPDAG for %s model '%.*s' #%u, only one "
            "is expected\n", roots_len, mdl_fmtargs(mdl));
      return Error_EMPIncorrectInput;
   }

   MathPrgm *mp = NULL;
   Mpe *mpe = NULL;
   if (_uid != EMPDAG_UID_NONE) {
      unsigned id = uid2id(_uid);

      if (uidisMP(_uid)) {
         S_CHECK(empdag_getmpbyid(empdag, id, &mp));
      } else {
         S_CHECK(empdag_getmpebyid(empdag, id, &mpe));
      }
   }

   *uid = _uid;

   if (mpe) {
      UIntArray mps;
      S_CHECK(empdag_mpe_getchildren(empdag, mpe->id, &mps));
      unsigned nb_mp = mps.len;
      if (nb_mp == 0) {
         error("%s :: empty equilibrium %d ``%s''\n", __func__,
               mpe->id, empdag_getmpename(empdag, mpe->id));
         return Error_EMPRuntimeError;
      }

      for (unsigned i = 0; i < nb_mp; ++i) {
         mpid_t mpid = uid2id(mps.arr[i]);
         if (!childless_mp(empdag, mpid)) { status = Error_OperationNotAllowed; }
      }

   } else {

      if (!mp) {
         error("[fooc] ERROR in %s model '%.*s' #%u: Empdag root is neither an "
               "MP or an MPE\n", mdl_fmtargs(mdl));
         return Error_RuntimeError;
      }

      if (!childless_mp(empdag, mp->id)) { status = Error_OperationNotAllowed; }
   }

   return status;
}

/**
 * @brief Compute the first order optimality conditions as an MCP for an EMP
 *        problem
 *
 * @param mdl      the model to solve
 * @param mcpdata  data for the jacobian computation
 *
 * @return         the error code
 */
static int fooc_mcp_emp(Model *mdl, McpStats *restrict mcpdata) {
   daguid_t uid;
   S_CHECK(fooc_mcp_analyze_emp(mdl, &uid));

   McpDef mcpdef = {.uid = uid}; //, .fops_vars = NULL};
   return fooc_mcp(mdl, &mcpdef, mcpdata);
}

/**
 * @brief Compute the first order optimality conditions as an MCP
 *
 * @param mdl      the model to solve
 * @param mcpdata  the jacobian data
 *
 * @return         the error code
 */
int fooc_create_mcp(Model *mdl, McpStats *restrict mcpdata)
{
   double start = get_thrdtime();

   assert(mdl->mdl_up && mdl_is_rhp(mdl->mdl_up));

  RhpModelData *mdldata_up = (RhpModelData *)mdl->mdl_up->data;

  enum mdl_probtype probtype = mdldata_up->probtype;

  if (probtype == MdlProbType_mcp) {
    error("%s :: the problem type is MCP, which already represents optimality "
          "conditions\n", __func__);
    return Error_UnExpectedData;
  }

  switch (probtype) {
  case MdlProbType_lp:
  case MdlProbType_qcp:
  case MdlProbType_nlp: {
    McpDef mcpdef = {.uid = EMPDAG_UID_NONE}; //, .fops_vars = NULL};
    S_CHECK(fooc_mcp(mdl, &mcpdef, mcpdata));
    break;
    }

  case MdlProbType_dnlp:
    error("%s :: ERROR: nonsmooth NLP are not supported\n", __func__);
    return Error_NotImplemented;

  case MdlProbType_cns:
    error("%s :: ERROR: constraints systems are not supported\n", __func__);
    return Error_NotImplemented;

  case MdlProbType_mip:
  case MdlProbType_minlp:
  case MdlProbType_miqcp:
    error("%s :: ERROR: Model with integer variables are not yet supported\n", __func__);
    return Error_NotImplemented;

  case MdlProbType_emp:
    S_CHECK(fooc_mcp_emp(mdl, mcpdata));
    break;

  default:
    error("%s :: ERROR: unknown/unsupported container type %s\n", __func__,
          probtype_name(probtype));
    return Error_InvalidValue;
  }

   mdl->empinfo.empdag.type = EmpDag_Empty;

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
int fooc_create_vi(Model *mdl, McpStats *restrict mcpdata) {
  return Error_NotImplemented;
}

/**
 * @brief Post-process the result from MCP
 *
 * @param mdl      the model
 *
 * @return         the error code
 */
int fooc_postprocess(Model *mdl, unsigned n_primal)
{
  return OK;
}
