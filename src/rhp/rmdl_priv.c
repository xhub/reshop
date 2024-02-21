
#include "empdag.h"
#include "mdl.h"
#include "rmdl_priv.h"

/**
 * @brief Get the objequ, without performing any empdag check
 *
 * When transforming a model, the empdag type might not be set.
 * This just checks if any objequ is defined
 *
 * @param       mdl     the model
 * @param[out]  objequ  the objective equation 
 * @return 
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


