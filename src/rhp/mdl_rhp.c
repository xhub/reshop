#include "reshop_config.h"
#include "asnan.h"

#include <float.h>
#include <limits.h>
#include <string.h>

#include "cmat.h"
#include "cones.h"
#include "container.h"
#include "ctr_rhp.h"
#include "ctrdat_rhp.h"
#include "empinfo.h"
#include "equvar_helpers.h"
#include "equvar_metadata.h"
#include "filter_ops.h"
#include "internal_model_common.h"
#include "lequ.h"
#include "macros.h"
#include "mathprgm.h"
#include "mdl.h"
#include "mdl_priv.h"
#include "mdl_rhp.h"
#include "mdl_timings.h"
#include "nltree.h"
#include "printout.h"
#include "reshop.h"
#include "rmdl_data.h"
#include "timings.h"

#ifndef NDEBUG
#define RMDL_DEBUG(str, ...) trace_fooc("[rmdl] DEBUG: " str "\n", __VA_ARGS__)
#else
#define RMDL_DEBUG(...)
#endif


int rmdl_checkobjequvar(const Model *mdl, rhp_idx objvar, rhp_idx objequ)
{
   S_CHECK(ei_inbounds(objequ, ctr_nequs_total(&mdl->ctr), __func__));
   S_CHECK(vi_inbounds(objvar, ctr_nvars_total(&mdl->ctr), __func__));

   Equ *e = &mdl->ctr.equs[objequ];
   EquObjectType otype = e->object;
   if (otype != Mapping && otype != DefinedMapping) {
      error("[model/rhp] ERROR: the objective equation '%s' has type %s it should"
            " be %s\n", mdl_printequname(mdl, objequ), equtype_name(e->object),
            equtype_name(Mapping));
      return Error_RuntimeError;
   }

   if (e->cone != CONE_NONE) {
      error("[model/rhp] ERROR: the objective equation '%s' is not an equality "
            "constraint (cone type %s)", mdl_printequname(mdl, objequ),
            cone_name(e->cone));
      return Error_RuntimeError;
   }

   bool objvar_in_objequ;
   S_CHECK(rctr_var_in_equ(&mdl->ctr, objvar, objequ, &objvar_in_objequ));
   if (!objvar_in_objequ) {
      error("[model/rhp] ERROR in %s model '%.*s' #%u: the objective variable %s"
            " does not appear in the objective equation %s\n", mdl_fmtargs(mdl),
            mdl_printvarname(mdl, objvar), mdl_printequname(mdl, objequ));
      return Error_RuntimeError;
   }

   return OK;
}


static char * get_mdlname_new(Model *mdl_up)
{
   char *mdlname;
   const char *mdlname_src = mdl_getname(mdl_up);
   size_t src_model_name_len = strlen(mdlname_src);

   size_t model_name_len = src_model_name_len + 5;

   MALLOC_NULL(mdlname, char, model_name_len);

   memcpy(mdlname, mdlname_src, src_model_name_len*sizeof(char));
   memcpy(&mdlname[src_model_name_len], "_rhp", 4*sizeof(char));
   mdlname[model_name_len-1] = '\0';

   return mdlname;
}

int rmdl_initctrfromfull(Model *mdl, Model *mdl_up)
{
   Container *ctr = &mdl->ctr;
   Container *ctr_up = &mdl_up->ctr;

   if (mdl->mdl_up) {
      error("[process] ERROR: %s model '%.*s' #%u already has a source model!\n",
            mdl_fmtargs(mdl));
      return Error_RuntimeError;
   }

   if (ctr_up->n == 0) {
      error("[process] ERROR: %s model '%.*s' #%u is empty, cannot initialize "
            "from it\n", mdl_fmtargs(mdl_up));
      return Error_RuntimeError;
   }

   assert(mdl_is_rhp(mdl) && (mdl_up->backend == RHP_BACKEND_GAMS_GMO || mdl_is_rhp(mdl_up)));

   trace_process("[process] %s model %.*s #%u: initializing from %s model %.*s #%u\n",
                 mdl_fmtargs(mdl), mdl_fmtargs(mdl_up));


   mdl_linkmodels(mdl_up, mdl);
  
   /* Not always necessary  */
   S_CHECK(ctr_borrow_nlpool(ctr, ctr_up));

   /* ----------------------------------------------------------------------
    * Copy the options
    *
    * TODO(xhub) this is terrible
    * ---------------------------------------------------------------------- */

   S_CHECK(mdl_copysolveoptions(mdl, mdl_up));

   /* ----------------------------------------------------------------------
    * Get the name of the model right. This could already be set
    * ---------------------------------------------------------------------- */
   if (!mdl->commondata.name) {
      A_CHECK(mdl->commondata.name, get_mdlname_new(mdl_up));
   }

   /* \TODO(xhub) implement proper model type detection. */
   S_CHECK(mdl_copyprobtype(mdl, mdl_up));

   /* ----------------------------------------------------------------------
    * Setup the container and model size
    *
    * Warning: do that after setting the modeltype
    * ---------------------------------------------------------------------- */

   S_CHECK(ctr_resize(ctr, ctr_up->n, ctr_up->m));

   RhpContainerData *cdat = (RhpContainerData *)ctr->data;

   /* ----------------------------------------------------------------------
    * Copy the variable, equations and mark them as inherited
    * ---------------------------------------------------------------------- */

   unsigned nvars_up = ctr_up->n, nequs_up = ctr_up->m;

   memcpy(ctr->vars, ctr_up->vars, nvars_up * sizeof(Var));
   memcpy(ctr->equs, ctr_up->equs, nequs_up * sizeof(Equ));

   aequ_setcompact(&cdat->equ_inherited.e, nequs_up, 0);
   aequ_setcompact(&cdat->equ_inherited.e_src, nequs_up, 0);

   avar_setcompact(&cdat->var_inherited.v, nvars_up, 0);
   avar_setcompact(&cdat->var_inherited.v_src, nvars_up, 0);


   /* \TODO(xhub) need to run an estimate of the number of rows/cols */
   ctr->m = nequs_up;
   ctr->n = 0;

   cdat->total_m = nequs_up;
   cdat->total_n = nvars_up;

   /* ---------------------------------------------------------------------
    * Fill the model representation matrix row by row
    * TODO(xhub) this is a duplicate with code in fooc.c
    * TODO(xhub) MED remove this, and just query it from the upstream model
    * --------------------------------------------------------------------- */

   struct ctrmem CTRMEM working_mem = {.ptr = NULL, .ctr = ctr};
   size_t mem_size = nvars_up * (sizeof(bool) + sizeof(int) + sizeof(double));
   A_CHECK(working_mem.ptr, ctr_getmem(ctr, mem_size));

   double* values = (double*)working_mem.ptr;
   int *var_in_equ = (int*)&values[nvars_up];
   bool *nlflags = (bool*)&var_in_equ[nvars_up];

   /* TODO improvement: use cmat_append_equs if we start from RHP? */
   for (rhp_idx ei = 0, len = ctr->m; ei < len; ++ei) {
      void *iterator = NULL;
      double jacval;
      int nlflag;
      rhp_idx vi;
      size_t indx = 0;

      do {
         S_CHECK(ctr_equ_itervars(ctr_up, ei, &iterator, &jacval, &vi, &nlflag));

         if (!valid_vi(vi)) {
            assert(indx == 0);
            S_CHECK(rctr_set_equascst(ctr, ei));
            goto next_equ;

         }

         nlflags[indx] = nlflag;
         values[indx] = jacval;
         var_in_equ[indx++] = vi;
      } while (iterator);

      Avar vars;
      avar_setlist(&vars, indx, var_in_equ);

      S_CHECK(cmat_fill_equ(ctr, ei, &vars, values, nlflags));
   next_equ: ;
   }

   /* ---------------------------------------------------------------------
    * Copy the variable and eqation metadata
    * --------------------------------------------------------------------- */

   ModelType mdltype_up;
   S_CHECK(mdl_gettype(mdl_up, &mdltype_up));

   if (mdltype_hasmetadata(mdltype_up)) {
      if (!ctr_up->varmeta || !ctr->varmeta) { return Error_NullPointer; }
      if (!ctr_up->equmeta || !ctr->equmeta) { return Error_NullPointer; }
      memcpy(ctr->varmeta, ctr_up->varmeta, nvars_up*sizeof(struct var_meta));
      memcpy(ctr->equmeta, ctr_up->equmeta, ctr_up->m*sizeof(struct equ_meta));
   }

   if (mdl_is_rhp(mdl_up)) {
      ((RhpModelData*)mdl->data)->solver = ((RhpModelData*)mdl_up->data)->solver;
   }


   return OK;
}

/**
 * @brief Initialize the data of the model
 *
 * @param mdl      the model to populate
 * @param mdl_up   the upstream model
 *
 * @return         the error code
 */
int rmdl_initfromfullmdl(Model *mdl, Model *mdl_up)
{
   double start = get_thrdtime();

    S_CHECK(rmdl_initctrfromfull(mdl, mdl_up));

   /* ---------------------------------------------------------------------
    *  Copy the empinfo
    * --------------------------------------------------------------------- */

   S_CHECK(empinfo_initfromupstream(mdl));

   S_CHECK(mdl_analyze_modeltype(mdl, NULL));

  /* ----------------------------------------------------------------------
   * Final adjustements (after EMPDAG is set):
   * - objequ must be a mapping (this is not the case coming from GAMS)
   * ---------------------------------------------------------------------- */
   rhp_idx objequ;
   rmdl_getobjequ(mdl, &objequ);
   if (valid_ei(objequ)) {
      Equ * restrict eobj = &mdl->ctr.equs[objequ];
      EquObjectType equtype = eobj->object;
      if (equtype == ConeInclusion) {
         assert(mdl_up->backend == RHP_BACKEND_GAMS_GMO);

         rhp_idx objvar;
         rmdl_getobjvar(mdl, &objvar);
         if (valid_vi(objvar)) {
            eobj->object = DefinedMapping;
         } else {
            eobj->object = Mapping;
         }
         eobj->cone = CONE_NONE;
      }
      EquObjectType otype = mdl->ctr.equs[objequ].object;
      if (otype != Mapping && otype != DefinedMapping) {
         error("[model] %s model '%.*s' #%u inherited an objective equation from "
               "%s model '%.*s' #%u, but the latter has type %s, expected %s\n",
               mdl_fmtargs(mdl), mdl_fmtargs(mdl_up),
               equtype_name(mdl->ctr.equs[objequ].object), equtype_name(Mapping));
         return Error_InvalidModel;
      }
   }

   RhpModelData *mdldat = mdl->data;
   mdldat->status = Rmdl_Editable;

   mdl_timings_rel(mdl->timings);
   mdl->timings = mdl_timings_borrow(mdl_up->timings);

   simple_timing_add(&mdl->timings->rhp.mdl_creation, start);

   return OK;
}

#if 0
static int rmdl_appendvars(Container *ctr, Container *ctr_up,
                           const Avar * restrict v)
{
   RhpContainerData *cdat = (RhpContainerData *)ctr->data;
   unsigned n_curvars = cdat->total_n;
   unsigned n_newvars = v->size;

   S_CHECK(rctr_reserve_vars(ctr, n_newvars));
   rhp_idx * restrict rosetta_vars = ctr_up->rosetta_vars;
   rhp_idx *rev_rosetta_newvars;
   MALLOC_(rev_rosetta_newvars, rhp_idx, n_newvars);

   Var * restrict vars_dst = ctr->vars; 
   Var * restrict vars_src = ctr_up->vars; 

   rhp_idx vi_new = n_curvars;
   struct v_inh newvars_inherited;
   avar_setcompact(&newvars_inherited.v, n_newvars, n_curvars);
   avar_setandownlist(&newvars_inherited.v_src, n_newvars, rev_rosetta_newvars);

   for (unsigned i = 0; i < n_newvars; ++i, ++vi_new) {

      rhp_idx vi_src = avar_fget(v, i);
      assert(valid_vi_(vi_src, ctr_nvars_total(ctr_up), __func__));

      assert(!valid_vi(rosetta_vars[vi_src]));
      rosetta_vars[vi_src] = vi_new;
      rev_rosetta_newvars[i] = vi_src;

      vars_dst[vi_new] = vars_src[vi_src];
      vars_dst[vi_new].idx = vi_new;

      assert(!vars_dst[vi_new].is_deleted);
   }

   if (ctr_up->varmeta) {

      vi_new = n_curvars;

      if (!ctr->varmeta) { return Error_NullPointer; }
      
      VarMeta * restrict vmeta_dst = ctr->varmeta;
      VarMeta * restrict vmeta_src = ctr_up->varmeta;

      for (unsigned i = 0, len = v->size; i < len; ++i) {

         rhp_idx vi_src = avar_fget(v, i);
         assert(valid_vi_(vi_src, ctr_nvars_total(ctr_up), __func__));

         VarMeta * restrict vmeta = &vmeta_dst[vi_new];
         vmeta->type = vmeta_src[vi_src].type;
         vmeta->ppty = vmeta_src[vi_src].ppty;

         rhp_idx ei_dual = vmeta_dst[vi_new].dual;
         if (valid_vi(ei_dual)) {
            vmeta->dual = ctr_getcurrent_ei(ctr_up, ei_dual);
         }

         /* TODO GITLAB #106 */
         vmeta->mp_id = UINT_MAX;
      }
   }

   return OK;
}
#endif

int rmdl_appendequs(Model *mdl, const Aequ *e)
{
   Container *ctr = &mdl->ctr;
   RhpContainerData *cdat = (RhpContainerData *)ctr->data;
   Model *mdl_up = mdl->mdl_up;
   const Container *ctr_up = &mdl_up->ctr;

   if (!mdl_up) {
      error("[process] ERROR: %s model '%.*s' #%u has a no source model!\n",
            mdl_fmtargs(mdl));
      return Error_RuntimeError;
   }

   assert(mdl_is_rhp(mdl) && mdl_is_rhp(mdl_up));

   unsigned n_curequs = cdat->total_m;

   unsigned n_newequs = e->size;
   assert(n_newequs > 0);

   trace_process("[process] %s model %.*s #%u: copying %u equs from "
                 "%s model %.*s #%u\n", mdl_fmtargs(mdl), n_newequs,
                 mdl_fmtargs(mdl_up));


 
   /* ----------------------------------------------------------------------
    * Setup the container and model size
    *
    * Warning: do that after setting the modeltype
    * ---------------------------------------------------------------------- */

   S_CHECK(rctr_reserve_equs(ctr, n_newequs));

   /* ----------------------------------------------------------------------
    * Copy the variable, equations and mark them as inherited
    * ---------------------------------------------------------------------- */

   rhp_idx * restrict rosetta_equs = ctr_up->rosetta_equs;
   rhp_idx * restrict rosetta_vars = ctr_up->rosetta_vars;

   rhp_idx *rev_rosetta_newequs;
   MALLOC_(rev_rosetta_newequs, rhp_idx, n_newequs);


   Equ * restrict equs_dst = ctr->equs; 
   Equ * restrict equs_src = ctr_up->equs;

   /* TODO GITLAB #110 */
   RhpContainerData *cdat_up = ctr_up->data;

   rhp_idx ei_new = n_curequs;

   for (unsigned i = 0; i < n_newequs; ++i) {

      rhp_idx ei_src = aequ_fget(e, i);
      assert(valid_ei_(ei_src, ctr_nequs_total(ctr_up), __func__));

      assert(!valid_ei(rosetta_equs[ei_src]));
      
      /* TODO GITLAB #110 */
      if (!cdat_up->equs[ei_src]) { continue; }

      rosetta_equs[ei_src] = ei_new;
      rev_rosetta_newequs[i] = ei_src;
      Equ *esrc = &equs_src[ei_src];
      Equ * restrict edst = &equs_dst[ei_new];

      *edst = *esrc;
      edst->idx = ei_new;

      assert(edst->object != ConeInclusion || cone_1D_polyhedral(edst->cone));

      S_CHECK(rctr_getnl(ctr_up, esrc));

      if (esrc->tree) {
         A_CHECK(edst->tree, nltree_dup_rosetta(esrc->tree, rosetta_vars));
         edst->tree->idx = ei_new;
      } else {
         edst->tree = NULL;
      }

      /* TODO GITLAB #105 */
      A_CHECK(edst->lequ, lequ_dup_rosetta(esrc->lequ, rosetta_vars));

      RMDL_DEBUG("Adding equation '%s' at pos #%u (ei_src %u)",
                 mdl_printequname(mdl_up, ei_src), ei_new, ei_src);

      ei_new++;
   }

   unsigned n_addedequs = ei_new-n_curequs;

   Aequ e_, e_src;
   aequ_setcompact(&e_, n_addedequs, n_curequs);
   S_CHECK(aequ_extendandown(&cdat->equname_inherited.e, &e_));
   aequ_setandownlist(&e_src, n_addedequs, rev_rosetta_newequs);
   S_CHECK(aequ_extendandown(&cdat->equname_inherited.e_src, &e_src));

    /* ---------------------------------------------------------------------
    * Fill the container matrix
    * --------------------------------------------------------------------- */
   S_CHECK(cmat_append_equs(ctr, ctr_up, e, n_curequs));

    /* ---------------------------------------------------------------------
    * Copy the variable and equation metadata
    * --------------------------------------------------------------------- */

   if (ctr_up->equmeta) {

      ei_new = n_curequs;

      if (!ctr->equmeta) { return Error_NullPointer; }
      
      EquMeta * restrict emeta_dst = ctr->equmeta;
      EquMeta * restrict emeta_src = ctr_up->equmeta;

      for (unsigned i = 0, len = e->size; i < len; ++i) {

         rhp_idx ei_src = aequ_fget(e, i);
         assert(valid_vi_(ei_src, ctr_nequs_total(ctr_up), __func__));

         /* TODO GITLAB #110 */
         if (!cdat_up->equs[ei_src]) {
            assert(emeta_src[ei_src].ppty & EquPptyIsDeleted);
            continue;
         }

         EquMeta * restrict emeta = &emeta_dst[ei_new];
         emeta->role = emeta_src[ei_src].role;
         emeta->ppty = emeta_src[ei_src].ppty;

         rhp_idx vi_dual = emeta_dst[ei_new].dual;
         if (valid_vi(vi_dual)) {
            emeta->dual = ctr_getcurrent_vi(ctr_up, vi_dual);
         }

         /* TODO GITLAB #106 */
         emeta->mp_id = UINT_MAX;

         ei_new++;
      }
   }

   return OK;
}


/**
 * @brief If needed, fix the objective function value to the objective variable
 *
 * @param mdl  the model
 *
 * @return     the error code
 */
int rmdl_fix_objequ_value(Model *mdl)
{
   RhpContainerData *cdat = (RhpContainerData *)mdl->ctr.data;

   rhp_idx objequ, objvar;
   rmdl_getobjvar(mdl, &objvar);
   rmdl_getobjequ(mdl, &objequ);

   if (valid_vi(objequ) && cdat->objequ_val_eq_objvar) {
      if (!valid_vi(objvar)) {
         error("%s :: Expecting a valid objective variable\n", __func__);
         return Error_Inconsistency;
      }

      mdl->ctr.equs[objequ].value = mdl->ctr.vars[objvar].value;
      double val = mdl->ctr.vars[objvar].value;
      trace_solreport("[solreport] %s model '%.*s' #%u: setting objequ value to %e.\n",
                      mdl_fmtargs(mdl), val);
   }

   struct mp_namedarray *mps = &mdl->empinfo.empdag.mps;

   for (unsigned i = 0, len = mps->len; i < len; ++i) {
      MathPrgm *mp = mps->arr[i];
      if (!mp) continue;

      S_CHECK(mp_fixobjequval(mp));
   }

   return OK;
}


int rmdl_remove_fixedvars(Model *mdl)
{
   const Avar *fv = mdl->ctr.fixed_vars;
   struct var_meta *varmeta = mdl->ctr.varmeta;
   if (!varmeta) return OK;

  for (size_t i = 0, len = avar_size(fv); i < len; ++i) {
    rhp_idx vi = avar_fget(fv, i);
    if (!valid_vi(vi)) continue; /* Nothing to do*/

    Var *v = &mdl->ctr.vars[vi];

    /* -------------------------------------------------------------------------
     * If vi has a perpendicular equation, copy the later into a constraint
     * - If vi.val == vi.lb, then Fi >= 0
     * - If vi.val == vi.ub, then Fi <= 0
     * - Else Fi == 0
     * ------------------------------------------------------------------------- */

    if (varmeta) {
      rhp_idx ei = varmeta[vi].dual;
      if (!valid_vi(ei)) continue;

      const Equ *e = &mdl->ctr.equs[ei];

      double lb = -INFINITY, ub = INFINITY;
      //bool has_bounds = true; TODO delete?

      if (v->is_conic) {
        switch (v->cone.type) {
        case CONE_R_PLUS:
          lb = 0.;
          break;
        case CONE_R_MINUS:
          ub = 0.;
          break;
        case CONE_R:
          break;
        default:
          error("%s:: unsupported cone type %s", __func__, cone_name(e->cone));
        }
      } else {
        assert(v->type == VAR_X);
        lb = v->bnd.lb;
        ub = v->bnd.ub;

        if (lb == ub) {
          error("%s :: ERROR: fixed variable '%s' (⟂ '%s') with same lower "
                "and upper bounds %e\n", __func__, ctr_printvarname(&mdl->ctr, vi),
                ctr_printequname(&mdl->ctr, ei), lb);
          return Error_UnsupportedOperation;
        }
      }

      rhp_idx ei_new = ei;
      S_CHECK(rmdl_dup_equ_except(mdl, &ei_new, 0, vi));
      Equ *e_new = &mdl->ctr.equs[ei_new]; 

      if (v->value == ub) {
        e_new->cone = CONE_R_MINUS; 
      } else if (v->value == lb) {
        e_new->cone = CONE_R_PLUS; 
      } else {
        e_new->cone = CONE_0; 

      }

      trace_process("[process] %s model %.*s #%u: fixed variable %s is dual to "
                    "equation %s\n ** Introducing constraint %s\n", mdl_fmtargs(mdl),
                    ctr_printvarname(&mdl->ctr, v->idx),
                    ctr_printequname(&mdl->ctr, ei),
                    ctr_printvarname(&mdl->ctr, ei_new));

    }
  }

  return OK;
}

int rmdl_incstage(Model *mdl)
{
   RhpContainerData *cdat = (RhpContainerData *)mdl->ctr.data;

   if (cdat->current_stage == UCHAR_MAX) {
      error("%s :: maximum number of stages %d reached!\n",
                         __func__, cdat->current_stage);
      return Error_RuntimeError;
   }

   cdat->current_stage++;

   unsigned stage = cdat->current_stage;
   REALLOC_(cdat->stage_auxmdl, struct auxmdl, stage+1);
   cdat->stage_auxmdl[stage].len = 0;
   cdat->stage_auxmdl[stage].max = 0;
   cdat->stage_auxmdl[stage].filter_subset = NULL;

   if (stage >= cdat->equvar_evals_size) {
      unsigned size = MAX(2*cdat->equvar_evals_size, stage+1);
      unsigned diff = size - cdat->equvar_evals_size;
      REALLOC_(cdat->equvar_evals, struct equvar_eval, size);

      memset(&cdat->equvar_evals[cdat->equvar_evals_size], 0, sizeof(struct equvar_eval)*diff);
      cdat->equvar_evals_size = size;
   }

   return OK;

}

Fops* rmdl_getfops(const Model *mdl)
{
   return mdl->ctr.fops;
}

int rmdl_ensurefops_activedefault(Model *mdl)
{
   Container *ctr = &mdl->ctr;
   if (ctr->fops) { return OK; }

   MALLOC_(ctr->fops, Fops, 1);

   return fops_active_init(ctr->fops, &mdl->ctr);
}

int rmdl_set_simpleprob(Model *mdl, const MpDescr *descr)
{
   EmpDag *empdag = &mdl->empinfo.empdag;
   S_CHECK(empdag_simple_init(empdag));
   assert(valid_mdltype(descr->mdltype));

   S_CHECK(rmdl_setobjsense(mdl, descr->sense));
   S_CHECK(rmdl_setobjfun(mdl, descr->objequ));
   S_CHECK(rmdl_setobjvar(mdl, descr->objvar));
   S_CHECK(mdl_settype(mdl, descr->mdltype));

   return OK;
}
