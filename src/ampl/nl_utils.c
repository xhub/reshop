#include <stdio.h>

#include "cmat.h"
#include "ctrdat_rhp.h"
#include "filter_ops.h"
#include "macros.h"
#include "mdl.h"
#include "nl_utils.h"
#include "printout.h"

static int nl_write_header(Model *mdl, FILE *stream, Model *pmdl)
{
   int objequ;
   unsigned n_ineq = 0;
   unsigned n_eq = 0;
   unsigned n_log = 0;
   unsigned nl_obj = 0;
   unsigned nl_constr = 0;

   if (pmdl->backend != RHP_BACKEND_RHP && pmdl->backend != RHP_BACKEND_JULIA) {
      error("%s :: the container type must be either of JULIA or RHP, rather "
            "than %s\n", __func__, backend_name(pmdl->backend));
      return Error_Unconsistency;
   }

   /*  TODO(Xhub) find more about that */
   fputs("g3 1 1 0", stream);
   S_CHECK(mdl_getobjequ(mdl, &objequ));
   int nobj = valid_ei(objequ) ? 1 : 0;

   for (unsigned i = 0; i < mdl->ctr.m; ++i) {

      if (mdl->ctr.equs[i].object == EQ_CONE_INCLUSION) {
         switch (mdl->ctr.equs[i].cone) {
         case CONE_0:
            n_eq++;
            break;
         case CONE_R_PLUS:
         case CONE_R_MINUS:
            n_ineq++;
            break;
         default:
            error("%s :: unsupported cone %s (%d)\n", __func__, 
                     cone_name(mdl->ctr.equs[i].cone), mdl->ctr.equs[i].cone);
            return Error_InvalidValue;
         }
      } else if (mdl->ctr.equs[i].object == EQ_BOOLEAN_RELATION) {
         n_log++;
      } else {

         error("%s :: unsupported relation of type %d\n", __func__,
                  mdl->ctr.equs[i].object);
         return Error_InvalidValue;
      }
   }

   struct ctrdata_rhp *cdat = (struct ctrdata_rhp *)pmdl->ctr.data;
   Fops *fops = cdat->fops;

   for (size_t i = 0; i < cdat->total_m; ++i) {
      if (fops && !fops->keep_equ(fops->data, i)) continue;

      bool isNL;
      S_CHECK(ctr_isequNL(&pmdl->ctr, i, &isNL));
      if (isNL) nl_constr++;
   }

   rhp_idx old_objequ;
   S_CHECK(mdl_getobjequ(pmdl, &old_objequ));
   if (nobj) {
      bool isobjNL;

      S_CHECK(ctr_isequNL(&pmdl->ctr, old_objequ, &isobjNL));

      if (isobjNL) {
          nl_constr--;
          nl_obj = 1;
      }
   }

   unsigned nlvo = 0;
   unsigned nlvc = 0;
   unsigned nlvb = 0;
   unsigned nlvoi = 0;
   unsigned nlvci = 0;
   unsigned nlvbi = 0;
   unsigned nbv = 0;
   unsigned niv = 0;

   for (size_t i = 0; i < cdat->total_n; ++i) {
      if (fops && !fops->keep_var(fops->data, i)) continue;

      bool has_seen_obj = false;
      bool isNLc = false;
      bool isNLo = false;
      bool isBin = (pmdl->ctr.vars[i].type == VAR_B || pmdl->ctr.vars[i].type == VAR_I);

      struct ctr_mat_elt *me = cdat->vars[i];
      assert(me);

      /* -------------------------------------------------------------------
       * Iterate through all the equations for this variable and update the
       * number of NL variables
       * ------------------------------------------------------------------- */

      /*  TODO(xhub) remove access to model_elt and add an API to walk through
       *  all the equations where the variable appear */
      /*  XXX: Very important! why do we stop as soon as has_seen_obj? A
       *  variable could be linear in the obj, but NL in a constraint*/

      while (!has_seen_obj && !isNLc && me) {
         if (me->isNL) {
            if (old_objequ == me->ei) {
               isNLo = true;
               nlvo++;
               if (isBin) nlvoi++;
            } else {
               isNLc = true;
               nlvc++;
               if (isBin) nlvci++;
            }
         }

         if (old_objequ == me->ei) {
            has_seen_obj = true;
         }

         me = me->next_equ;
      }

      if (isNLo && isNLc) {
         nlvb++;
         if (isBin) nlvbi++;
      } else if (isBin && !(isNLo || isNLc)) {
         if (pmdl->ctr.vars[i].type == VAR_B) nbv++;
         else if (pmdl->ctr.vars[i].type == VAR_I) niv++;
      }
   }

   /* vars, constraints, objectives, ranges, eqns, logical constraints */
   fprintf(stream, " %u %u %d %u %u %u\n", mdl->ctr.n, mdl->ctr.m, nobj, n_ineq, n_eq, n_log);

   /* nonlinear constraints, objectives */
   fprintf(stream, " %u %u\n", nl_constr, nl_obj);

   /* network constraints: nonlinear, linear */
   fputs(" 0 0\n", stream);

   /* nonlinear vars in constraints, objectives, both */
   fprintf(stream, " %u %u %u\n", nlvc, nlvo, nlvb);

   /* linear network variables; functions; arith, flags */
   fputs(" 0 0 0 1\n", stream);

   /* discrete variables: binary, integer, nonlinear (b,c,o) */
   fprintf(stream, " %u %u %u %u %u\n", nbv, niv, nlvbi, nlvci, nlvoi);

   /* nonzeros in Jacobian, gradients */
   fputs(" 0 0\n", stream);

   /* common exprs: b,c,o,c1,o1  */
   fputs(" 0 0 0 0 0\n", stream);

   return OK;
}

static int nl_write_nlexpr(Container *ctr, FILE *stream, Container *pctr)
{
   
   return OK;
}
