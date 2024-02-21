
#include "checks.h"
#include "reshop.h"

#include "empdag.h"
#include "empdag_common.h"
#include "empinfo.h"
#include "mathprgm.h"
#include "mdl.h"
#include "mdl_rhp.h"

/** This ensure that the model is of EMP type.
 * If the model type is unset, we set it to EMP.
 * If it is already set to a different type than EMP,
 * then we error
 */
static int ensure_EMP_mdl_type(Model *mdl)
{
   ProbType probtype;
   S_CHECK(mdl_getprobtype(mdl, &probtype));

   if (probtype == MdlProbType_none) {
      S_CHECK(mdl_setprobtype(mdl, MdlProbType_emp));
   } else if (probtype !=  MdlProbType_emp) {

      error("%s :: ERROR Model type is %s, should be %s\n", __func__,
            probtype_name(probtype), probtype_name(MdlProbType_emp));
      return Error_EMPIncorrectInput;
  }

   /*Coming from GAMS we might have not varmeta or equmeta */
   Container *ctr = &mdl->ctr;
   if (!ctr->varmeta) {
      rhp_idx total_n = ctr_nvars_total(ctr);
      MALLOC_(ctr->varmeta, struct var_meta, total_n);
      for (size_t i = 0; i < (size_t)total_n; ++i) {
         varmeta_init(&ctr->varmeta[i]);
      }
   }

   if (!ctr->equmeta) {
      rhp_idx total_m = ctr_nvars_total(ctr);
      MALLOC_(ctr->equmeta, struct equ_meta, total_m);
      for (size_t i = 0; i < (size_t)total_m; ++i) {
         equmeta_init(&ctr->equmeta[i]);
      }
   }

   return OK;
}

/**
 * @brief Set the root of the EMP dag to a mathematical program
 *
 * @ingroup publicAPI
 *
 * @param mdl  the model
 * @param mp   the mathematical program
 *
 * @return     the error code
 */
int rhp_empdag_rootsaddmp(Model *mdl, MathPrgm *mp)
{
   S_CHECK(chk_mdl(mdl, __func__));

   if (!mp) {
      error("%s :: MP object is NULL\n", __func__);
      return Error_NullPointer;
   }

   return empdag_rootsaddmp(&mdl->empinfo.empdag, mp->id);
}

/**
 * @brief Set the root of the EMP dag to an equilibrium
 *
 * @ingroup publicAPI
 *
 * @param mdl  the model
 * @param mpe  the equilibrium
 *
 * @return     the error code
 */
int rhp_empdag_rootsaddmpe(Model *mdl, Mpe *mpe)
{
   S_CHECK(chk_mdl(mdl, __func__));

   if (!mpe) {
      error("%s :: equilibrium object is NULL\n", __func__);
      return Error_NullPointer;
   }

   ProbType probtype;
   rmdl_getprobtype(mdl, &probtype);
   if (probtype == MdlProbType_none) {
      mdl_setprobtype(mdl, MdlProbType_emp);
   }

   return empdag_rootsaddmpe(&mdl->empinfo.empdag, mpe->id);
}

/**
 * @brief Allocate an mathprgm object
 *
 * @param  mdl    the overall model
 * @param  sense  the sense of the MP
 *
 * @return the equilibrium object
 */
MathPrgm *rhp_empdag_newmp(Model *mdl, unsigned sense)
{
   SN_CHECK(chk_mdl(mdl, __func__));

   SN_CHECK(ensure_EMP_mdl_type(mdl));

   if (sense >= RHP_SENSE_LEN) {
      error("[empdag] ERROR: invalid sense argument for MP! The value must be "
               "less than %d\n", RHP_SENSE_LEN);
      return NULL;
   }

  return empdag_newmp(&mdl->empinfo.empdag, sense);
}

/**
 * @brief Allocate an equilibrium object
 *
 * @param mdl  the overall model

 * @return the equilibrium object
 */
Mpe* rhp_empdag_newmpe(Model *mdl)
{
  if (!mdl) {
      error("%s :: The model object is NULL\n", __func__);
      return NULL;
   }

   SN_CHECK(ensure_EMP_mdl_type(mdl));

  return empdag_newmpe(&mdl->empinfo.empdag);
}

/** 
 *  @brief add a mathematical programm into the equilibrium
 *
 *  @param mpe  the equilibrium structure
 *  @param mp   the mathematical programm
 *
 *  @return     the error code
 */
int rhp_empdag_mpeaddmp(Model *mdl, Mpe* mpe, MathPrgm *mp)
{
   S_CHECK(chk_mdl(mdl, __func__));

   if (!mpe) {
      error("%s :: The equilibrium object is NULL\n", __func__);
      return Error_NullPointer;
   }
   if (!mp) {
      error("%s :: The mathematical programm object is NULL\n",
               __func__);
      return Error_NullPointer;
   }

  EmpDag *empdag = &mdl->empinfo.empdag;
  return empdag_mpeaddmpbyid(empdag, mpe->id, mp->id);
}

/** 
 *  @brief reserve space for mathematical programms
 *
 *  This is the first function to call if there are such EMP structure
 *
 *  @param  mdl       the reshop model
 *  @param  reserve   the number of MP that should have space
 *
 *  @return           the error code
 */
int rhp_ensure_mp(Model *mdl, unsigned reserve)
{
   S_CHECK(chk_mdl(mdl, __func__));

   empdag_reserve_mp(&mdl->empinfo.empdag, reserve);

   return OK;
}

EdgeVF * rhp_edgeVF_new(void)
{
   EdgeVF *edgeVF;
   MALLOC_NULL(edgeVF, EdgeVF, 1);
   return edgeVF;
}

int rhp_edgeVF_init(EdgeVF *edgeVF, rhp_idx ei)
{
   S_CHECK(chk_arg_nonnull(edgeVF, 1, __func__));
   edgeVFb_init(edgeVF, ei);
   return OK;
}

int rhp_edgeVF_free(EdgeVF *edgeVF)
{
   FREE(edgeVF);
   return OK;
}

int rhp_edgeVF_setvar(EdgeVF *edgeVF, rhp_idx vi)
{
   S_CHECK(chk_arg_nonnull(edgeVF, 1, __func__));
   edgeVFb_setvar(edgeVF, vi);
   return OK;
}

int rhp_edgeVF_setcst(EdgeVF *edgeVF, double cst)
{
   S_CHECK(chk_arg_nonnull(edgeVF, 1, __func__));
   edgeVFb_setcst(edgeVF, cst);
   return OK;
}

int rhp_empdag_mpaddmpVF(Model *mdl, MathPrgm *mp,
                         MathPrgm *mp_child, EdgeVF *edgeVF)
{
   S_CHECK(chk_rmdldag(mdl, __func__));
   S_CHECK(chk_arg_nonnull(mp, 2, __func__));
   S_CHECK(chk_arg_nonnull(mp_child, 3, __func__));
   S_CHECK(chk_arg_nonnull(edgeVF, 4, __func__));

   unsigned id_parent = mp->id;
   unsigned id_child = mp_child->id;
   edgeVF->child_id = id_child;

   return empdag_mpVFmpbyid(&mdl->empinfo.empdag, id_parent, edgeVF);
}

int rhp_empdag_mpaddmpCTRL(Model *mdl, MathPrgm *mp, MathPrgm *mp_child)
{
   S_CHECK(chk_rmdldag(mdl, __func__));
   S_CHECK(chk_arg_nonnull(mp, 2, __func__));
   S_CHECK(chk_arg_nonnull(mp_child, 3, __func__));

   unsigned id_parent = mp->id;
   unsigned id_child = mp_child->id;

   return empdag_mpCTRLmpbyid(&mdl->empinfo.empdag, id_parent, id_child);
}

