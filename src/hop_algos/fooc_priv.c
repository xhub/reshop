#include <assert.h>
#include "fooc_priv.h"
#include "macros.h"
#include "mdl.h"
#include "printout.h"
#include "status.h"

void rosettas_init(Rosettas *r)
{
   rhp_uint_init(&r->rosetta_starts);
   rhp_uint_init(&r->mdl_n);
   rhp_obj_init(&r->mdls);

   r->data = NULL;
}

void rosettas_free(Rosettas *r)
{
   rhp_uint_empty(&r->rosetta_starts);
   rhp_uint_empty(&r->mdl_n);
   rhp_obj_empty(&r->mdls);

   FREE(r->data);
}

int compute_all_rosettas(Model *mdl_mcp, Rosettas *r)
{
   rhp_uint_init(&r->mdl_n);
   rhp_uint_init(&r->rosetta_starts);
   rhp_obj_init(&r->mdls);

   unsigned total_n = 0, num_mdl = 0;
   Model *up = mdl_mcp->mdl_up;

   while (up) {
      num_mdl++;
      unsigned mdl_total_n = ctr_nvars_total(&up->ctr);

      rhp_uint_add(&r->mdl_n, mdl_total_n);
      rhp_uint_add(&r->rosetta_starts, total_n);
      rhp_obj_add(&r->mdls, up);

      total_n += mdl_total_n;
      up = up->mdl_up;
   }

   if (total_n == 0) {
      error("ERROR: no variable defined for %s model '%.*s' #%u",
            mdl_fmtargs(mdl_mcp));
      return Error_RuntimeError;
   }

   MALLOC_(r->data, rhp_idx, total_n);
   unsigned cur_idx = r->mdl_n.list[0];
   memcpy(r->data, mdl_mcp->mdl_up->ctr.rosetta_vars, cur_idx*sizeof(rhp_idx));

   for (unsigned i = 1; i < num_mdl; ++i) {
      rhp_idx *rosetta_cur = &r->data[cur_idx];

      unsigned mdl_total_n = r->mdl_n.list[i];

      const Model * restrict mdl = r->mdls.list[i];
      rhp_idx * restrict rosetta_vars = mdl->ctr.rosetta_vars;

      rhp_idx *rosetta_prev = &r->data[r->rosetta_starts.list[i-1]];

      /* If we have a rosetta, we copy it and update it */
      if (rosetta_vars) {

         memcpy(rosetta_cur, rosetta_vars, mdl_total_n * sizeof(rhp_idx));

         for (unsigned j = 0; j < mdl_total_n; ++j) {

            rhp_idx vi = rosetta_cur[j];
            if (valid_vi(vi)) {
               rosetta_cur[j] = rosetta_prev[vi];
            }
         }

      } else { /* If we had no rosetta_var, just */ 
         assert(r->mdl_n.list[i] >= mdl_total_n);
         memcpy(rosetta_cur, rosetta_prev, mdl_total_n * sizeof(rhp_idx));
      } 

      cur_idx += mdl_total_n;
   }

   return OK;
}
