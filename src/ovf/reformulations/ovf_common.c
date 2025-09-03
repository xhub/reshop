#include "asprintf.h"
#include "asnan.h"

#include <math.h>
#include <stddef.h>

#include "container.h"
#include "container_helpers.h"
#include "ctr_rhp.h"
#include "empdag_uid.h"
#include "empinfo.h"
#include "equ_modif.h"
#include "equvar_metadata.h"
#include "lequ.h"
#include "macros.h"
#include "mathprgm.h"
#include "mdl.h"
#include "ctrdat_rhp.h"
#include "mdl_rhp.h"
#include "ovf_common.h"
#include "ovf_generator.h"
#include "printout.h"
#include "toplayer_utils.h"

/** 
 *  @brief prepare the replacement an OVF variable
 *
 *  @param      mdl         the model (must be RHP)
 *  @param      ovf_vidx    OVF variable index
 *  @param[out] jacptr      pointer to the next equation where the OVF variable
 *                          appears. Must be set to NULL on the first call for a
 *                          given OVF variable
 *  @param[out] ovf_coeff   coefficient of the OVF variable
 *  @param[out] ei          the index for the transformed equation
 *  @param      extra_vars  the (estimated) number of additional variables
 *
 *  @return               the error code
 */
int ovf_replace_var(Model *mdl, rhp_idx ovf_vidx, void **jacptr,
                    double *ovf_coeff, rhp_idx *ei, unsigned extra_vars)
{
   rhp_idx eidx;
   int nlflag;
   Container *ctr = &mdl->ctr;

   S_CHECK(ctr_var_iterequs(ctr, ovf_vidx, jacptr, ovf_coeff, &eidx, &nlflag));

   EquInfo equinfo;
   S_CHECK(rctr_get_equation(ctr, eidx, &equinfo));

   if (nlflag) {
      error("%s :: OVF variable %s (#%d) appears non-linearly",
                         __func__, ctr_printvarname(ctr, ovf_vidx), ovf_vidx);
      TO_IMPLEMENT("The case when the OVF variable appears non-linearly is not yet supported");
   }

   /*  Oops the equation has been expanded */
   if (equinfo.expanded) {
      TO_IMPLEMENT("The case when a variable has been expanded");
   }

   rhp_idx ei_new = equinfo.ei;

   /* Copy if needed */
   if (equinfo.copy_if_modif) {

      S_CHECK(rmdl_equ_dup_except(mdl, &ei_new, extra_vars, ovf_vidx));
      DPRINT("%s :: copy equation %d in %d\n", __func__, eidx, ei_new);

   } else {

      /* -------------------------------------------------------------------
       * Remove the variable from the equation
       * ------------------------------------------------------------------- */

      S_CHECK(equ_rm_var(ctr, &ctr->equs[ei_new], ovf_vidx));
   }

   *ei = ei_new;

   return OK;
}

int ovf_process_indices(Model *mdl, Avar *args, rhp_idx *eis)
{
   Container *ctr = &mdl->ctr;
   unsigned nargs = avar_size(args);

   S_CHECK(rctr_reserve_eval_equvar(ctr, nargs));

   for (unsigned i = 0; i < nargs; ++i) {
      rhp_idx ei_old = eis[i];
      if (!valid_ei(ei_old)) { continue; }

      EquInfo equinfo;
      S_CHECK(rctr_get_equation(ctr, ei_old, &equinfo));

      rhp_idx ei = equinfo.ei;
      eis[i] = ei;

      S_CHECK(rmdl_equ_rm(mdl, ei));
      S_CHECK(rctr_add_eval_equvar(&mdl->ctr, ei, avar_fget(args, i)));
   }

   return OK;
}

int ovf_remove_mappings(Model *mdl, OvfDef *ovf_def)
{
   const Avar *args = ovf_def->args;
   unsigned nargs = avar_size(args);

   if (!ovf_def->eis) {
      error("[OVF] ERROR in OVF %s: cannot remove mappings before identifying them!\n",
            ovf_getname(ovf_def));
      return Error_RuntimeError;
   }

   rhp_idx *eis = ovf_def->eis;
   Container *ctr = &mdl->ctr;

   for (unsigned i = 0; i < nargs; ++i) {
      EquInfo equinfo;
      S_CHECK(rctr_get_equation(ctr, eis[i], &equinfo));

      //rhp_idx vi = avar_fget(args, i);
      //rhp_idx ei = equinfo.ei;

   }

   TO_IMPLEMENT("OVF mapping removal");
}

int ovf_equil_init(Model *mdl, struct ovf_basic_data *ovf_data, MathPrgm **mp_ovf)
{
   Container *ctr = &mdl->ctr;
   EmpInfo *empinfo = &mdl->empinfo;
   EmpDag *empdag = &empinfo->empdag;

   MathPrgm *mp_ovf_, *mp = NULL; /* init just to silence compiler */
   Nash *nash;

   const DagUidArray *mp_parents = NULL;

   rhp_idx vi_ovf = ovf_data->idx;
   bool mp_rm_vi_ovf = true;
   
   /* ----------------------------------------------------------------------
    * Get the MP and its parents, if they exists
    * ---------------------------------------------------------------------- */

   if (empdag_exists(empdag)) {
      mp = mdl_getmpforvar(mdl, vi_ovf);

      if (!mp) {
         error("%s :: no mp found for variable %s (%d), and there "
                  "are %d MP", __func__, ctr_printvarname(ctr, vi_ovf),
                  vi_ovf, empdag_num_mp(empdag));
         return Error_NullPointer;
      }

      S_CHECK(empdag_getmpparents(empdag, mp, &mp_parents));

   } else {
      /* -------------------------------------------------------------------
       * We reset the EMPDAG type
       * ------------------------------------------------------------------- */
      Avar v;
      avar_setcompact(&v, 1, vi_ovf);
      S_CHECK(empdag_initDAGfrommodel(mdl, &v));
      empdag_reset_type(empdag);
      S_CHECK(empdag_getmpbyid(empdag, 0, &mp));

      mp_rm_vi_ovf = false;
   }

   /* ----------------------------------------------------------------------
    * 3 cases here:
    * - The MP has more than 1 parents
    * - The MP has a parent (only an equilibrium parent is supported)
    * - The MP is either a root or there is no EMPDAG (whole problem)
    * ---------------------------------------------------------------------- */

   if (!mp_parents || mp_parents->len == 0) {
      /* -------------------------------------------------------------------
       * No Nash owns the MP where the OVF is. Then, we create an Nash for the
       * MP and the auxiliary OVF problem.
       * ------------------------------------------------------------------- */
      assert(!mp_parents || empdag_isroot(empdag, mpid2uid(mp->id)));
      S_CHECK(rhp_ensure_mp(mdl, 2));

      char *nash_ovf_name;
      IO_PRINT(asprintf(&nash_ovf_name, "%s", ovf_data->name));
      A_CHECK(nash, empdag_newnashnamed(empdag, nash_ovf_name));
      free(nash_ovf_name);

      S_CHECK(empdag_nashaddmpbyid(empdag, nash->id, mp->id));

      S_CHECK(empdag_setroot(empdag, nashid2uid(nash->id)));

      /* --------------------------------------------------------------------
       * The objective variable of the original problem won't have the correct
       * value, because we take a shortcut. Add an evaluation here
       *
       * TODO(xhub) check how this is handled by other reformulation scheme and
       * make things consistent.
       * -------------------------------------------------------------------- */

      rhp_idx mp_objequ = mp_getobjequ(mp);
      rhp_idx mp_objvar = mp_getobjvar(mp);
      if (valid_ei(mp_objequ) && valid_vi(mp_objvar)) {
         S_CHECK(rctr_add_eval_equvar(ctr, mp_objequ, mp_objvar));
      }

   } else if (mp_parents->len > 1) {
      error("%s :: the OVF function is in an MP with more than"
            "one parent (%d). This is not yet supported\n",
            __func__, mp_parents->len);
      return Error_NotImplemented;

   } else {

      unsigned parent_uid = mp_parents->arr[0];
      if (!uidisNash(parent_uid)) {
         TO_IMPLEMENT("The MP has an MP parent; only Nash parent are currently supported.");
      }

      empdag_getnashbyid(empdag, uid2id(parent_uid), &nash);

      assert(mp && nash); /* Just to silence a warning */

      S_CHECK(rhp_ensure_mp(mdl, empdag_num_mp(empdag) + 1));

   } 

   /* --------------------------------------------------------------------
    * Remove the ovf variable from the first agent since we reuse it for
    *  the objective variable of the second agent
    * -------------------------------------------------------------------- */

   if (mp_rm_vi_ovf) {
      S_CHECK(mp_rm_var(mp, vi_ovf));
   }

   /* ----------------------------------------------------------------------
    * Create the new MP and add it to the equilibrium
    * ---------------------------------------------------------------------- */

   A_CHECK(mp_ovf_, empdag_newmpnamed(empdag, ovf_data->sense, ovf_data->name));

   S_CHECK(empdag_nashaddmpbyid(empdag, nash->id, mp_ovf_->id));

   *mp_ovf = mp_ovf_;


   return OK;
}

/**
 * @brief Check that the OVF type (inf or sup) is compatible with the
 * optimization problem type. This should only be used when the OVF variable is
 * in the objective function.
 *
 * @param ovf_name     name of the OVF function
 * @param ovf_varname  name of the OVF variable
 * @param mp_sense     true if the objective function/variable is to be
 *                     minimized
 * @param ovf_sense      true if the OVF is of sup type
 *
 * @return             the error code
 */
int ovf_compat_types(const char *ovf_name, const char *ovf_varname, RhpSense mp_sense,
                     RhpSense ovf_sense)
{
   /* MpMin and OVF sup or MpMax and OVF min are valid */
   if (!((mp_sense == RhpMin && ovf_sense == RhpMax) ||
         (mp_sense == RhpMax && ovf_sense == RhpMin))) {
      size_t i = 0;

      /* -------------------------------------------------------------------
       * There are some OVF that have no compatility issues, namely when
       * the maximizer/minimizer of the OVF problem is always the same.
       * In that case, the OVF problem is reduced to a simple expression.
       *
       * Nothing actually needs to be done, but that's another story ...
       * ------------------------------------------------------------------- */

      while (ovf_always_compatible[i]) {
         if (!strcasecmp(ovf_name, ovf_always_compatible[i])) {
            return OK;
         }
         ++i;
      }

      error("[OVF] ERROR: unsupported problem types: the OVF %s (var name %s)"
            " of type %s is used in a %s optimization problem.\n"
            "This is unsupported for now", ovf_name, ovf_varname,
            sense2str(ovf_sense), sense2str(mp_sense));
      return Error_EMPIncorrectSyntax;
   }

   return OK;
}


int ovf_get_mp_and_sense(const Model *mdl, rhp_idx vi_ovf, MathPrgm **mp, RhpSense *sense)
{
   if (empdag_exists(&mdl->empinfo.empdag)) {
      MathPrgm *mp_;
      A_CHECK(mp_, mdl_getmpforvar(mdl, vi_ovf));
      *mp = mp_;
      *sense = mp_getsense(mp_);
   } else {
      S_CHECK(rmdl_getsense(mdl, sense));
      assert(valid_optsense(*sense));
      *mp = NULL;
   }

   return OK;
}
