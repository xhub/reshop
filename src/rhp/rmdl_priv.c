#include "reshop_config.h"

#include <string.h>

#include "empdag.h"
#include "empdag_mp.h"
#include "mdl.h"
#include "mdl_rhp.h"
#include "rmdl_priv.h"

/**
 * @brief Get the objequ, without performing any empdag check
 *
 * When transforming a model, the empdag type might not be set.
 * This just checks if any objequ is defined
 *
 * @param       mdl     the model
 * @param[out]  objequ  the objective equation 
 */
void rmdl_getobjequ_nochk(const Model *mdl, rhp_idx *objequ)
{
   const EmpDag *empdag = &mdl->empinfo.empdag;

   switch (empdag->type) {
      case EmpDag_Empty:
         *objequ = empdag->simple_data.objequ;
         return;
      default:
         *objequ = IdxNA;
         return;
   }

}

int rmdl_get_editable_mdl(Model *mdl, Model **mdl_dst, const char *name)
{
   if (mdl_is_rhp(mdl) && rmdl_iseditable(mdl)) {
      *mdl_dst = mdl_borrow(mdl);
      mdl_unsetmetachecked(mdl);
      mdl_unsetchecked(mdl);
      mdl_unsetfinalized(mdl);
      return OK;
   }

   Model *mdl_dst_ = rhp_mdl_new(RhpBackendReSHOP);
   S_CHECK(mdl_setname(mdl_dst_, strdup(name)));

   S_CHECK(rmdl_initctrfromfull(mdl_dst_, mdl));

   *mdl_dst = mdl_dst_;

   const EmpDag * empdag_src = &mdl->empinfo.empdag;
   EmpDag * empdag_dst = &mdl_dst_->empinfo.empdag;
   unsigned num_mp = empdag_src->mps.len;

   /* Prepare new EMPDAG: we copy the existing one. The big MPs will be added at the end */
   S_CHECK(dagmp_array_reserve(&empdag_dst->mps, num_mp*1.2));
   S_CHECK(empdag_dup(empdag_dst, empdag_src, mdl_dst_));

   return OK;
}
