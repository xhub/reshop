#include "checks.h"
#include "container.h"
#include "gams_solve.h"
#include "mdl_data.h"
#include "container_ops.h"
#include "container_helpers.h"
#include "empdag.h"
#include "empinfo.h"
#include "gams_exportempinfo.h"
#include "macros.h"
#include "mdl.h"
#include "mdl_rhp.h"
#include "mdl_ops.h"
#include "rhp_alg.h"
#include "printout.h"
#include "reshop.h"
#include "rhp_model.h"
#include "status.h"

/** @file rhp_model.c */


/**
 * @brief Export a (subset of a) model into another one
 *
 * @param mdl         the source model
 * @param mdl_solver  the model to be populated
 * @param fops        the filtering structure. If NULL, the active variable and
 *                    equations are kept.
 *
 * @return            the error code
 */
int rmdl_exportmodel(Model *mdl, Model *mdl_solver, Fops *fops)
{
   if (!mdl_is_rhp(mdl)) {
      errormsg("[exportmodel] ERROR: the first argument (source model) must be "
               "a RHP-like model.\n");
      return Error_WrongModelForFunction;
   }

   /* ----------------------------------------------------------------------
    * First of all, some checks:
    * - on the source model
    * - on the metadata
    * 
    * First the empdag is finalized 
    * ---------------------------------------------------------------------- */

   S_CHECK(empdag_finalize(mdl));

   S_CHECK(mdl_check(mdl));
   S_CHECK(mdl_checkmetadata(mdl));

  /* ----------------------------------------------------------------------
   * Set the fops, but after the check
   * ---------------------------------------------------------------------- */

   if (fops) {
      S_CHECK(rmdl_setfops(mdl, fops));
   }

   S_CHECK(rmdl_prepare_export(mdl, mdl_solver));


  /* ----------------------------------------------------------------------
   * If we solve it via GAMS and it is a MOPEC, then create the MCP
   *
   * TODO: GITLAB #87
   * TODO GITLAB #88
   * ---------------------------------------------------------------------- */

#if 0
   /* We have to do this here  */
   ProbType probtype_src;
   mdl_getprobtype(mdl, &probtype_src);

   if (probtype_src == MdlProbType_emp && mdl_solver->backend == RHP_BACKEND_GAMS_GMO &&
      !getenv("RHP_NO_JAMS_EMULATION")) {

      const EmpDag *empdag = &mdl->empinfo.empdag;

      /* TODO this is a bit hackish */
      switch (empdag->type) {
      case EmpDag_Mopec:
      case EmpDag_Vi:
      case EmpDag_Simple_Vi:
         return mdl_exportasgmo(mdl, mdl_solver); 
      default: ;
      }

      error("[process] ERROR: don't know how to solve %s model '%.*s' #%u of "
            "type %s with empdag type %s\n", mdl_fmtargs(mdl),
            probtype_name(probtype_src), empdag_typename(empdag->type));

      return Error_NotImplemented;
   }
#endif

   S_CHECK(mdl_exportmodel(mdl, mdl_solver));

   /* We have to do this here  */
   ProbType probtype;
   mdl_getprobtype(mdl_solver, &probtype);

   if (probtype == MdlProbType_none) {
      S_CHECK(mdl_copyprobtype(mdl_solver, mdl));
   }

   return OK;
}

#if 0
static int _print_all_vars(struct empinfo_dat *file, IntArray *vars,
                           Container *ctr, int objvaridx)
{
   char buf[SHORT_STRING];
   char bufout[2*SHORT_STRING];

   for (size_t vi = 0; vi < vars->size; ++vi) {
      rhp_idx vidx = vars->list[vi];
      if (vidx != objvaridx) {
         assert(vidx < ctr->n);
         S_CHECK(_add_empinfo(file, bufout));
      }
   }

   return OK;
}

static int _print_all_equs(struct empinfo_dat *file, IntArray *equs,
                           Container *ctr)
{
   char buf[SHORT_STRING];
   char bufout[2*SHORT_STRING];

   for (size_t ei = 0; ei < equs->size; ++ei) {
      rhp_idx eidx = equs->list[ei];
      assert(eidx < ctr->m);
      GET_PROPER_ROWNAME(ctr, eidx, buf, bufout);
      S_CHECK(_add_empinfo(file, bufout));
   }

   return OK;
}

static int _print_vars(struct empinfo_dat *file, IntArray *vars,
                       struct model_repr *model, Container *ctr, int objvaridx)
{
   char buf[SHORT_STRING];
   char bufout[2*SHORT_STRING];

   for (size_t i = 0; i < vars->size; ++i) {
      rhp_idx vi = rmdl_getcurrent_vidx(model, vars->list[i]);
      if (valid_vi(vi) && vi != objvaridx) {
         assert((size_t)vi < model->total_n);
         GET_PROPER_COLNAME(ctr, vi, buf, bufout);
         S_CHECK(_add_empinfo(file, bufout));
      }
   }

   return OK;
}

static int _print_equs(struct empinfo_dat *file, IntArray *equs,
                       struct model_repr *model, Container *ctr)
{
   char buf[SHORT_STRING];
   char bufout[2*SHORT_STRING];

   for (size_t i = 0; i < equs->size; ++i) {
      rhp_idx ei = rmdl_getcurrent_eidx(model, equs->list[i]);
      if (valid_ei(ei)) {
         assert((size_t)ei < model->total_m);
         GET_PROPER_ROWNAME(ctr, ei, buf, bufout);
         S_CHECK(_add_empinfo(file, bufout));
      }
   }

   return OK;
}

static int _print_mp_opt(struct empinfo_dat *file, MathPrgm *mp,
                         struct model_repr *model, Container *ctr)
{
   /* ----------------------------------------------------------------------
    * For a optimization programm, the syntax is 
    *
    * (min|max) objvar all_vars all_equs
    *
    * ---------------------------------------------------------------------- */
   char buf[SHORT_STRING];
   char bufout[2*SHORT_STRING];

   int mp_dir = mp_getobjdir(mp);
   assert(mp_dir == RHP_MIN || mp_dir == RHP_MAX);

   fputs(mp_dir == RHP_MIN ? "\nmin" : "\nmax", file->empinfo_file);

   rhp_idx mp_objvar = mp_getobjvar(mp);

   if (!valid_vi(mp_objvar)) {
      error("%s :: MP %d has a negative objective variable "
            "%d\n", __func__, mp->id, mp_objvar);
      return Error_IndexOutOfRange;
   }

   rhp_idx objvaridx = model ? rmdl_getcurrent_vidx(model, mp_objvar) : mp_objvar;

   if (!valid_vi(objvaridx)) {
      error("%s :: optimization programm %d has no valid "
            "objective variable\n", __func__, mp->id);
      return Error_Unconsistency;
   }

   GET_PROPER_COLNAME(ctr, objvaridx, buf, bufout);

   fprintf(file->empinfo_file, " %s", bufout);
   file->line_len = strlen(buf) + 5;

   if (model) {
      S_CHECK(_print_vars(file, &mp->vars, model, ctr, objvaridx));

      S_CHECK(_print_equs(file, &mp->equs, model, ctr));
   } else {
      S_CHECK(_print_all_vars(file, &mp->vars, ctr, objvaridx));

      S_CHECK(_print_all_equs(file, &mp->equs, ctr));
   }

   return OK;

}

/* TODO(Xhub) create a version without the model  */
static int _print_mp_vi(struct empinfo_dat *file, MathPrgm *mp,
                         struct model_repr *model, Container *ctr)
{
   /* -------------------------------------------------------
    * For a VI programm, the syntax is 
    *
    * vi zero_func_vars {var eqn} constraints_equs
    *
    * where {var eqn} can be repeated multiples times
    *
    * ------------------------------------------------------- */

   IntArray *equs = &mp->equs;
   struct equ_meta *equ_md = *mp->equ_md;
   IntArray *vars = &mp->vars;
   struct var_meta *var_md = *mp->var_md;

   /* ----------------------------------------------------------------------
    * If there are zero functions, loop over all the variables to define those
    * ---------------------------------------------------------------------- */

   int nb_zerofunc = mp_getzerofunc(mp);

   if (nb_zerofunc > 0) {
      for (size_t vi = 0; vi < vars->size; ++vi) {
         rhp_idx vidx = vars->list[vi];
         if (var_md[vidx].subtype == Varmeta_SubZeroFunction) {
            rhp_idx new_vidx = model ? rmdl_getcurrent_vidx(model, vidx) : vidx;
            new_vidx
            S_CHECK(_add_empinfo(file, bufout));
         }
      }
   }

   /* ----------------------------------------------------------------------
    * Define the pairs ``equation variable''
    * ---------------------------------------------------------------------- */

   for (size_t vi = 0; vi < vars->size; ++vi) {
      rhp_idx vidx = vars->list[vi];
      rhp_idx eidx = var_md[vidx].dual;
       if (!valid_ei(eidx)) {
         if (var_md[vidx].subtype == Varmeta_SubZeroFunction) {
            continue;
         }

         error("%s :: Error in MP %d of type %s: the equation "
                            "associated with the variable %s (#%d) is not well "
                            "defined\n", __func__, mp_getid(mp),
                            mp_gettypestr(mp), ctr_printvarname(ctr, vidx),
                            vidx);
         return Error_Unconsistency;
      }

      rhp_idx new_eidx = model ? rmdl_getcurrent_eidx(model, eidx) : eidx;
      assert(valied_ei(new_eidx) && "new equation index is invalid");
      GET_PROPER_ROWNAME(ctr, new_eidx, buf, bufout);
      S_CHECK(_add_empinfo(file, bufout));
      rhp_idx new_vidx = model ? rmdl_getcurrent_vidx(model, vidx) : vidx;
      assert(valid_vi(new_vidx));
      GET_PROPER_COLNAME(ctr, new_vidx, buf, bufout);
      S_CHECK(_add_empinfo(file, bufout));
   }

   /* ----------------------------------------------------------------------
    * Define all the equations
    * ---------------------------------------------------------------------- */

   if (mp_getnumconstrs(mp) > 0) {
      for (size_t ei = 0; ei < equs->size; ++ei) {
         rhp_idx eidx = equs->list[ei];
         assert(valid_ei(eidx));
         if (equ_md[eidx].type == Equmeta_Constraint) {
            rhp_idx new_eidx = model ? rmdl_getcurrent_eidx(model, eidx) : eidx;
            GET_PROPER_ROWNAME(ctr, new_eidx, buf, bufout);
            S_CHECK(_add_empinfo(file, bufout));
         }
      }
   }

   return OK;
}

static inline int _print_mp(struct empinfo_dat *file, MathPrgm *mp,
                            struct model_repr *model, Container *ctr)
{
   switch (mp->type) {
   case MP_OPT:
      S_CHECK(_print_mp_opt(file, mp, model, ctr));
      break;
   case MP_VI:
      S_CHECK(_print_mp_vi(file, mp, model, ctr));
      break;
   default:
      error("%s :: declaration of a MP of type %s (#%d) in a "
            "GAMS empinfo file is not supported\n", __func__,
            mp_gettypestr(mp), mp_gettype(mp));
      return Error_NotImplemented;
   }

   return OK;
}
#endif
