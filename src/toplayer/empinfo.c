#include "empdag.h"
#include "empinfo.h"
#include "macros.h"
#include "mdl.h"
#include "ovfinfo.h"
#include "printout.h"
#include "status.h"
#include "reshop.h"

//#define DEBUG_GAMS_OUTPUT

int empinfo_alloc(EmpInfo *empinfo, Model *mdl)
{
   empinfo->num_dualvar = 0;
   empinfo->num_implicit = 0;
   empinfo->num_deffn = 0;

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
   S_CHECK(empinfo_alloc(empinfo, mdl));

   /* -----------------------------------------------------------------------
    * Copy basic information
    * ----------------------------------------------------------------------- */

   empinfo->num_dualvar = empinfo_up->num_dualvar;
   empinfo->num_implicit = empinfo_up->num_implicit;

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
      S_CHECK(empdag_initfrommodel(&empinfo->empdag, mdl_up));
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


   /* ---------------------------------------------------------------------
    * Print OV/OVF information.
    * --------------------------------------------------------------------- */

   if (empinfo->ovf) {
      ovf_print(empinfo->ovf, mdl);
   }
}


