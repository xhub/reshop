#include "empdag.h"
#include "empinfo.h"
#include "macros.h"
#include "mdl.h"
#include "ovfinfo.h"
#include "printout.h"
#include "status.h"
#include "reshop.h"

//#define DEBUG_GAMS_OUTPUT

int empinfo_init(Model *mdl)
{
   EmpInfo *empinfo = &mdl->empinfo;
 
   empinfo->equvar.num_implicit = 0;
   empinfo->equvar.num_deffn = 0;
   empinfo->equvar.num_explicit = 0;


   empinfo->equvar.disjunctions = (Disjunctions){0};
   empinfo->equvar.marginalVars = (IdxArray){0};
   empdag_init(&empinfo->empdag, mdl);

   return OK;
}

void empinfo_dealloc(EmpInfo *empinfo)
{
   if (empinfo->ovf) {
      ovfinfo_dealloc(empinfo);
   }

   empdag_rel(&empinfo->empdag);
}

/**
 * @brief Initialize empinfo from an upstream model
 *
 * @param mdl  the model
 *
 * @return     the error code
 */
int empinfo_initfromupstream(Model *mdl)
{
   if (!mdl_is_rhp(mdl)) {
      error("%s :: model must be of RHP subtype\n", __func__);
      return Error_WrongModelForFunction;
   }

   const Model *mdl_up = mdl->mdl_up;
   if (!mdl_up) {
      error("%s ERROR: The upstream model is NULL!\n", __func__);
      return Error_NullPointer;
   }

   const EmpInfo *empinfo_up = &mdl_up->empinfo;

   EmpInfo *empinfo = &mdl->empinfo;
   S_CHECK(empinfo_init(mdl));

   /* -----------------------------------------------------------------------
    * Copy basic information
    * ----------------------------------------------------------------------- */

   empinfo->equvar.num_implicit = empinfo_up->equvar.num_implicit;
   empinfo->equvar.num_explicit = empinfo_up->equvar.num_explicit;
   empinfo->equvar.num_deffn = empinfo_up->equvar.num_deffn;

   S_CHECK(rhp_idx_copy(&empinfo->equvar.marginalVars, &empinfo_up->equvar.marginalVars));
   S_CHECK(disjunctions_copy(&empinfo->equvar.disjunctions, &empinfo_up->equvar.disjunctions));

   /* -----------------------------------------------------------------------
    * Borrow processable information
    * ----------------------------------------------------------------------- */
   if (empinfo_up->ovf) {
      empinfo->ovf = ovfinfo_borrow(empinfo_up->ovf);
   }

   /* -----------------------------------------------------------------------
    * Init EMPDAG
    * ----------------------------------------------------------------------- */
   if (empinfo_up->empdag.mps.len == 0) {
      assert(empdag_isempty(&empinfo_up->empdag));
      S_CHECK(empdag_initFromUpstream(&empinfo->empdag, mdl_up));
   } else {
      S_CHECK(empdag_dup(&empinfo->empdag, &empinfo_up->empdag, mdl));
   }

   return empdag_fini(&empinfo->empdag);
}


bool empinfo_is_hop(const EmpInfo *empinfo)
{
   const EmpDag *empdag = &empinfo->empdag;
   assert(empinfo_hasempdag(empinfo));

   return !empdag_isempty(empdag) && !empdag_singleprob(empdag);
}

bool empinfo_is_vi(const EmpInfo *empinfo)
{
   const EmpDag *empdag = &empinfo->empdag;
   assert(empinfo_hasempdag(empinfo));

   return empdag->type == EmpDag_Single_Vi;
}

bool empinfo_is_opt(const EmpInfo *empinfo)
{
   const EmpDag *empdag = &empinfo->empdag;
   assert(empinfo_hasempdag(empinfo));

   return empdag->type == EmpDag_Single_Opt;
}

/**
 * @brief Print the EMP information
 *
 * @ingroup publicAPI
 *
 * @param mdl  the model
 */
void rhp_print_emp(const Model *mdl)
{
   if (!mdl) return;
   const EmpInfo *empinfo = &mdl->empinfo;

   /* ---------------------------------------------------------------------
    * Print the identifiers in the EMP file first.
    * --------------------------------------------------------------------- */
   const EmpDag *empdag = &empinfo->empdag;
   if (empdag->mps.len > 0) {
      empdag_print(empdag);
   }


   /* ---------------------------------------------------------------------
    * Print OV/OVF information.
    * --------------------------------------------------------------------- */

   if (empinfo->ovf) {
      ovf_print(empinfo->ovf, mdl);
   }
}


