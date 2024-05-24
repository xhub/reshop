
#include "filter_ops.h"
#include "fooc_data.h"
#include "empdag.h"
#include "mdl.h"
#include "mdl_rhp.h"

void fooc_data_init(FoocData *fooc_dat, McpInfo *info)
{
   fooc_dat->info = info;

   /* This is important as we check whether this has been set to another value */
   info->n_foocvars = SSIZE_MAX;
   info->n_auxvars = SSIZE_MAX;

   fooc_dat->vi_primal2ei_F = NULL;
   fooc_dat->ei_F2vi_primal = NULL;
   fooc_dat->mp2objequ      = NULL;

   aequ_init(&fooc_dat->objequs);
   aequ_init(&fooc_dat->cons_nl);
   aequ_init(&fooc_dat->cons_lin);

   mpidarray_init(&fooc_dat->mps);
}

void fooc_data_fini(FoocData *fooc_dat)
{
   if (!fooc_dat) { return; }

   aequ_empty(&fooc_dat->objequs);
   aequ_empty(&fooc_dat->cons_nl);
   aequ_empty(&fooc_dat->cons_lin);

   mpidarray_empty(&fooc_dat->mps);
   FREE(fooc_dat->mp2objequ);
}

/**
 * @brief Populate the list of MPs in FoocData
 *
 * @param fooc_dat  the fooc data struct
 * @param empdag    the empdag
 * @param uid_root  the root of the EMPDAG
 * @param fops      the filter
 *
 * @return          the error code
 */
int fooc_data_empdag(FoocData *fooc_dat, EmpDag *empdag, daguid_t uid_root, Fops *fops)
{
   //assert(!fops || fops->type == FopsEmpDagSubDag);
   assert(valid_uid(uid_root));

   dagid_t id = uid2id(uid_root);
   MpIdArray *mplist = &fooc_dat->mps;

   if (uidisNash(uid_root)) {
      assert(id < empdag->nashs.len);
      DagUidArray *mps = &empdag->nashs.arcs[id];
      S_CHECK(mpidarray_reserve(&fooc_dat->mps, mps->len));
      for (unsigned i = 0, len = mps->len; i < len; ++i) {
         S_CHECK(mpidarray_addsorted(mplist, uid2id(mps->arr[i])));
      }
   } else {
      S_CHECK(mpidarray_add(mplist, id));
   }


/*
 * Create a permutation list from variable indices in the source model
 * to a subset based on a subdag.
 */

   if (!empdag_isroot(empdag, uid_root)) {
      Model *mdl_src = empdag->mdl;
      VarMeta * restrict vmeta = mdl_src->ctr.varmeta;
      assert(vmeta);

      rhp_idx nvars_src_total = ctr_nvars_total(&mdl_src->ctr);
      size_t nvars_primal, dummy;
      if (fops) {
         fops->get_sizes(fops->data, &nvars_primal, &dummy);
      } else {
         nvars_primal = nvars_src_total;
         assert(nvars_src_total == mdl_src->ctr.n);
      }

      rhp_idx *vi2ei_F, *ei_F2vi;
      MALLOC_(vi2ei_F, rhp_idx, nvars_src_total);
      MALLOC_(ei_F2vi, rhp_idx, nvars_primal);
      fooc_dat->vi_primal2ei_F = vi2ei_F;
      fooc_dat->ei_F2vi_primal = ei_F2vi;

      /*TODO GITLAB #109*/
      rhp_idx ei_F = 0;
      size_t n_auxvars = 0;
      for (rhp_idx i = 0, vi = 0; i < nvars_src_total; ++i) {
         if (fops && !fops->keep_var(fops->data, i)) {
            continue;
         }

         if (mpidarray_findsorted(mplist, vmeta[i].mp_id) != UINT_MAX) {
            ei_F2vi[ei_F] = vi;
            vi2ei_F[vi] = ei_F++;
         } else { /* auxiliary variable */
            vi2ei_F[vi] = IdxInvalid;
            n_auxvars++;
         }

         vi++;
      }

      fooc_dat->info->n_foocvars = ei_F;
      fooc_dat->info->n_auxvars = n_auxvars;
   }

   return OK;
}
