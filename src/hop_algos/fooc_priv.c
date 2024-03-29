#include <assert.h>

#include "fooc_priv.h"
#include "fooc_data.h"
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
   Model *mdl_up = mdl_mcp->mdl_up;

   while (mdl_up) {
      num_mdl++;
      unsigned ctr_total_n = ctr_nvars_total(&mdl_up->ctr);

      S_CHECK(rhp_uint_add(&r->mdl_n, ctr_total_n));
      S_CHECK(rhp_uint_add(&r->rosetta_starts, total_n));
      S_CHECK(rhp_obj_add(&r->mdls, mdl_up));

      total_n += ctr_total_n;
      mdl_up = mdl_up->mdl_up;
   }

   if (total_n == 0) {
      error("ERROR: no variable defined for %s model '%.*s' #%u",
            mdl_fmtargs(mdl_mcp));
      return Error_RuntimeError;
   }

  /* ----------------------------------------------------------------------
   * Create a continuous array of all the 
   * ---------------------------------------------------------------------- */

   MALLOC_(r->data, rhp_idx, total_n);
   unsigned cur_idx = r->mdl_n.arr[0];
   memcpy(r->data, mdl_mcp->mdl_up->ctr.rosetta_vars, cur_idx*sizeof(rhp_idx));

   for (unsigned i = 1; i < num_mdl; ++i) {
      rhp_idx *rosetta_cur = &r->data[cur_idx];

      unsigned mdl_total_n = r->mdl_n.arr[i];

      const Model * restrict mdl = r->mdls.arr[i];
      rhp_idx * restrict rosetta_vars = mdl->ctr.rosetta_vars;

      rhp_idx *rosetta_prev = &r->data[r->rosetta_starts.arr[i-1]];

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
         assert(r->mdl_n.arr[i] >= mdl_total_n);
         memcpy(rosetta_cur, rosetta_prev, mdl_total_n * sizeof(rhp_idx));
      } 

      cur_idx += mdl_total_n;
   }

   return OK;
}

bool childless_mp(const EmpDag *empdag, mpid_t mpid)
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

int fooc_check_childless_mps(Model *mdl_mcp, FoocData *fooc_dat)
{
   const EmpDag *empdag = &mdl_mcp->mdl_up->empinfo.empdag;

   MpIdArray *mps = &fooc_dat->mps;
   unsigned n_mps = mps->len;

   for (unsigned i = 0; i < n_mps; ++i) {
      mpid_t mpid = mps->arr[i];
      if (!childless_mp(empdag, mpid)) { 
         return Error_OperationNotAllowed;
      }
   }

   return OK;
}

