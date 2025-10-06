#include "container.h"
#include "container_ops.h"
#include "mdl.h"
#include "printout.h"
#include "status.h"

/** @file container_ops.c
 *
 *  @brief This is the wrapper around the container ops
 */

int ctr_copyvarname(const Container *ctr, rhp_idx vi, char *name, unsigned len)
{
   return ctr->ops->copyvarname(ctr, vi, name, len);
}

int ctr_copyequname(const Container *ctr, rhp_idx ei, char *name, unsigned len)
{
   return ctr->ops->copyequname(ctr, ei, name, len);
}

/** @brief Evaluate the equations marked as such
 *
 *  This function reports the values from the solver to the internal model
 *  representation. Then it evaluates equations marked as such.
 *
 *  @param ctr         the container
 *
 *  @return           the error code
 */
int ctr_evalequvar(Container *ctr)
{
   return ctr->ops->evalequvar(ctr);
}

int ctr_evalfunc(const Container *ctr, rhp_idx ei, double *x, double *f, int *numerr)
{
   return ctr->ops->evalfunc(ctr, ei, x, f, numerr);
}

int ctr_evalgrad(const Container *ctr, rhp_idx ei, double *x, double *f,
                 double *g, double *gx, int *numerr)
{
   return ctr->ops->evalgrad(ctr, ei, x, f, g, gx, numerr);
}

int ctr_evalgradobj(const Container *ctr, double *x, double *f, double *g,
                    double *gx, int *numerr)
{
   return ctr->ops->evalgradobj(ctr, x, f, g, gx, numerr);
}

/**
 * @brief Get the basis status (if available) for an abstract equation
 *
 * @param      ctr         the container
 * @param      e           the abstract equation
 * @param[out] equs_basis  the basis array
 *
 * @return       the error code
 */
int ctr_getequsbasis(const Container *ctr, Aequ *e, int *equs_basis)
{
   return ctr->ops->getequsbasis(ctr, e, equs_basis);
}

/**
 * @brief Get the multipliers (if available) for an abstract equation
 *
 * @param      ctr        the container
 * @param      e          the abstract equation
 * @param[out] equs_mult  the multiplier array
 *
 * @return       the error code
 */
int ctr_getequsmult(const Container *ctr, Aequ *e, double *equs_mult)
{
   return ctr->ops->getequsmult(ctr, e, equs_mult);
}

/**
 * @brief Get the values for an abstract equation
 *
 * @param      ctr        the container
 * @param      e          the abstract equation
 * @param[out] equs_val   the values array
 *
 * @return       the error code
 */
int ctr_getequsval(const Container *ctr, Aequ *e, double *equs_val)
{
   return ctr->ops->getequsval(ctr, e, equs_val);
}

/**
 * @brief Get the basis status (if available) for an abstract variable
 *
 * @param       ctr         the container
 * @param       v           the abstract var
 * @param[out]  vars_basis  the basis array
 *
 * @return       the error code
 */
int ctr_getvarsbasis(const Container *ctr, Avar *v, int *vars_basis)
{
   return ctr->ops->getvarsbasis(ctr, v, vars_basis);
}

/**
 * @brief Get the multipliers (if available) for an abstract variable
 *
 * @param       ctr        the container
 * @param       v          the abstract var
 * @param[out]  vars_mult  the multiplier array
 *
 * @return       the error code
 */
int ctr_getvarsmult(const Container *ctr, Avar *v, double *vars_mult)
{
   return ctr->ops->getvarsmult(ctr, v, vars_mult);
}

/**
 * @brief Get the values for an abstract variable
 *
 * @param      ctr       the container
 * @param      v         the abstract var
 * @param[out] vars_val  the values array
 *
 * @return       the error code
 */
int ctr_getvarsval(const Container *ctr, Avar *v, double *vars_val)
{
   return ctr->ops->getvarsval(ctr, v, vars_val);
}

/**
 * @brief Get an equation by its name
 *
 * @param       ctr   the container
 * @param       name  the name of the equation
 * @param[out]  ei    the equation index
 *
 * @return      the error code
 */
int ctr_getequbyname(const Container *ctr, const char* name, rhp_idx *ei)
{
   return ctr->ops->getequbyname(ctr, name, ei);
}

/**
 * @brief Get the value of an equation
 *
 * @param      ctr  the container
 * @param      ei   the equation index
 * @param[out] val  the value
 *
 * @return     the error code
 */
int ctr_getequval(const Container *ctr, rhp_idx ei, double *val)
{
   return ctr->ops->getequval(ctr, ei, val);
}

/**
 * @brief Get the multiplier of an equation
 *
 * @param      ctr   the container
 * @param      ei    the equation index
 * @param[out] mult  the multiplier
 *
 * @return           the error code
 */
int ctr_getequmult(const Container *ctr, rhp_idx ei, double *mult)
{
   return ctr->ops->getequmult(ctr, ei, mult);
}

/**
 * @brief Get the matching variable of an equation
 *
 * If there is no matching variable, then a invalid index is returned
 *
 * @param      ctr   the container
 * @param      ei    the equation index
 * @param[out] vi    the matched variable index
 *
 * @return           the error code
 */
int ctr_getequperp(const Container *ctr, rhp_idx ei, rhp_idx *vi)
{
   return ctr->ops->getequperp(ctr, ei, vi);
}

/**
 * @brief Get all equations multiplier
 *
 * @param       ctr   the container
 * @param[out]  mult  the multiplier array, of appropriate size
 *
 * @return      the error code
 */
int ctr_getallequsmult(const Container *ctr, double *mult)
{
   return ctr->ops->getallequsmult(ctr, mult);
}

/**
 * @brief Get the basis status of an equation
 *
 * @param      ctr           the container
 * @param      ei            the equation index
 * @param[out] basis_status  the basis status
 *
 * @return            the error code
 */
int ctr_getequbasis(const Container *ctr, rhp_idx ei, int *basis_status)
{
   return ctr->ops->getequbasis(ctr, ei, basis_status);
}

/** @brief Get all equations values
 *
 * @param       ctr   the container
 * @param[out]  vals  the value array, of appropriate size
 *
 * @return      the error code
 */
int ctr_getallequsval(const Container *ctr, double *vals)
{
   return ctr->ops->getallequsval(ctr, vals);
}

/**
 * @brief Return the type of an equation
 *
 * @param      ctr  the container
 * @param      ei   the equation
 * @param[out] type the equation type
 * @param[out] cone the equation cone
 *
 * @return          the error code
 */
int ctr_getequtype(const Container *ctr, rhp_idx ei, unsigned *type, unsigned *cone)
{
   return ctr->ops->getequtype(ctr, ei, type, cone);
}

int ctr_getequexprtype(const Container *ctr, rhp_idx ei, EquExprType *type)
{
   return ctr->ops->getequexprtype(ctr, ei, type);
}

int ctr_var_iterequs(const Container *ctr, rhp_idx vi, void **jacptr,
                      double *jacval, rhp_idx *ei, int *nlflag)
{
   return ctr->ops->getcoljacinfo(ctr, vi, jacptr, jacval, ei, nlflag);
}

int ctr_equ_itervars(const Container *ctr, rhp_idx ei, void **jacptr,
                           double *jacval, rhp_idx *vi, int *nlflag)
{
   return ctr->ops->getrowjacinfo(ctr, ei, jacptr, jacval, vi, nlflag);
}

int ctr_equ_iscst(const Container *ctr, rhp_idx ei, bool *isCst)
{
   return ctr->ops->isequcst(ctr, ei, isCst);
}
int ctr_equvarcounts(Container *ctr)
{
  return ctr->ops->equvarcounts(ctr);
}


/**
 * @brief Get the constant of an equation
 *
 * @param      ctr the container
 * @param      ei  the equation index
 * @param[out] val the constant value
 *
 * @return         the error code
 */
int ctr_getequcst(const Container *ctr, rhp_idx ei, double *val)
{
   return ctr->ops->getequcst(ctr, ei, val);
}

/**
 * @brief Get special floating-point values (like the infinities and NaN)
 *
 * @param       ctr   the container
 * @param[out]  minf  the value of -INF
 * @param[out]  pinf  the value of +INF
 * @param[out]  nan   the value of NaN (Not a Number)
 *
 * @return            the error code
 */
int ctr_getspecialfloats(const Container *ctr, double *minf, double *pinf, double *nan)
{
   return ctr->ops->getspecialfloats(ctr, minf, pinf, nan);
}

/**
 * @brief Get a variable upper and lower bound
 *
 * @param       ctr  the container
 * @param       vi   the variable index
 * @param[out]  lb   the lower bound of the variable (could be -INF)
 * @param[out]  ub   the upper bound of the variable (could be +INF)
 *
 * @return           the error code
 */
int ctr_getvarbounds(const Container *ctr, rhp_idx vi, double *lb, double *ub)
{
   return ctr->ops->getvarbounds(ctr, vi, lb, ub);
}

/** @brief Get a variable by its name
 *
 * @param      ctr  the container
 * @param      name the variable name 
 * @param[out] vi   the variable index
 *
 * @return          the error code
 */
int ctr_getvarbyname(const Container *ctr, const char* name, rhp_idx *vi)
{
   return ctr->ops->getvarbyname(ctr, name, vi);
}

/** @brief Get the value of a variable
 *
 * @param      ctr  the container
 * @param      vi   the variable index
 * @param[out] val  the variable value
 *
 * @return          the error code
 */
int ctr_getvarval(const Container *ctr, rhp_idx vi, double *val)
{
   return ctr->ops->getvarval(ctr, vi, val);
}

/** @brief Get the multiplier of a variable
 *
 * @param      ctr   the container
 * @param      vi    the variable index
 * @param[out] mult  the variable multiplier
 *
 * @return          the error code
 */
int ctr_getvarmult(const Container *ctr, rhp_idx vi, double *mult)
{
   return ctr->ops->getvarmult(ctr, vi, mult);
}

/**
 * @brief Get the equation perpendicular (or matched) with a variable
 *
 * @param       ctr  the container
 * @param       vi   the variable index
 * @param[out]  ei   the equation index
 *
 * @return           the error code
 */
int ctr_getvarperp(const Container *ctr, rhp_idx vi, rhp_idx *ei)
{
   return ctr->ops->getvarperp(ctr, vi, ei);
}

/** @brief Get the basis status of a variable
 *
 * @param      ctr           the container
 * @param      vi            the variable index
 * @param[out] basis_status  the variable basis status
 *
 * @return          the error code
 */
int ctr_getvarbasis(const Container *ctr, rhp_idx vi, int *basis_status)
{
   return ctr->ops->getvarbasis(ctr, vi, basis_status);
}

/** @brief Get the type of a variable
 *
 * @param      ctr   the container
 * @param      vi    the variable index
 * @param[out] type  the variable basis status
 *
 * @return          the error code
 */
int ctr_getvartype(const Container *ctr, rhp_idx vi, unsigned *type)
{
   return ctr->ops->getvartype(ctr, vi, type);
}

/** @brief Get the lower bound of a variable
 *
 * @param      ctr  the container
 * @param      vi   the variable index
 * @param[out] lb   the variable lower bound
 *
 * @return          the error code
 */
int ctr_getvarlb(const Container *ctr, rhp_idx vi, double *lb)
{
   return ctr->ops->getvarlb(ctr, vi, lb);
}


/** @brief Get the value of a variable
 *
 * @param      ctr  the container
 * @param      vi   the variable index
 * @param[out] ub  the variable value
 *
 * @return          the error code
 */
int ctr_getvarub(const Container *ctr, rhp_idx vi, double *ub)
{
   return ctr->ops->getvarub(ctr, vi, ub);
}

/** @brief Get all variables values
 *
 * @param       ctr   the container
 * @param[out]  vals  the value array, of appropriate size
 *
 * @return      the error code
 */
int ctr_getallvarsval(const Container *ctr, double *vals)
{
   return ctr->ops->getallvarsval(ctr, vals);
}

/** @brief Get all variables multipliers
 *
 * @param       ctr   the container
 * @param[out]  mult  the multipliers array, of appropriate size
 *
 * @return      the error code
 */
int ctr_getallvarsmult(const Container *ctr, double *mult)
{
   return ctr->ops->getallvarsmult(ctr, mult);
}

/**
 * @brief Set the basis status of an equation
 *
 * @param ctr    the container
 * @param ei     the equation index
 * @param basis_status  the basis status
 * 
 * @return       the error code
 */
int ctr_setequbasis(Container *ctr, rhp_idx ei, int basis_status)
{
   return ctr->ops->setequbasis(ctr, ei, basis_status);
}

/**
 * @brief Set the (level) value of an equation
 *
 * @param ctr    the container
 * @param ei     the equation index
 * @param val    the value
 * 
 * @return       the error code
 */
int ctr_setequval(Container *ctr, rhp_idx ei, double val)
{
   return ctr->ops->setequval(ctr, ei, val);
}

/**
 *  @brief Set the name of an equation
 *
 *  @param ctr   the container
 *  @param ei    the equation
 *  @param name  the equation name
 *
 *  @return      the error code
 */
int ctr_setequname(Container *ctr, rhp_idx ei, const char *name)
{
   if (!name) {
      error("%s :: the equation name is NULL\n", __func__);
      return Error_NullPointer;
   }

   return ctr->ops->setequname(ctr, ei, name);
}

/** @brief Set the multiplier of an equation
 *
 *  @param ctr   the container
 *  @param ei    the equation index
 *  @param mult  the equation multiplier
 *
 *  @return      the error code
 */
int ctr_setequmult(Container *ctr, rhp_idx ei, double mult)
{
   return ctr->ops->setequmult(ctr, ei, mult);
}

/** @brief Set the type of an equation
 *
 *  @param ctr   the container
 *  @param ei    the equation index
 *  @param type  the equation type
 *  @param cone  the equation cone
 *
 *  @return      the error code
 */
int ctr_setequtype(Container *ctr, rhp_idx ei, unsigned type, unsigned cone)
{
   return ctr->ops->setequtype(ctr, ei, type, cone);
}

/** @brief Set the constant value of an equation
 *
 *  @param ctr   the container
 *  @param ei    the equation index
 *  @param val  the constant value
 *
 *  @return      the error code
 */
int ctr_setequcst(Container *ctr, rhp_idx ei, double val)
{
   return ctr->ops->setequcst(ctr, ei, val);
}


/** @brief Set the lower bound of a variable
 *
 *  @param ctr   the container
 *  @param vi    the variable
 *  @param lb    the variable lower bound
 *
 *  @return      the error code
 */
int ctr_setvarlb(Container *ctr, rhp_idx vi, double lb)
{
   return ctr->ops->setvarlb(ctr, vi, lb);
}

/** @brief Set the bounds of a variable
 *
 *  @param ctr   the container
 *  @param vi    the variable
 *  @param lb    the variable lower bound
 *  @param ub    the variable upper bound
 *
 *  @return      the error code
 */
int ctr_setvarbounds(Container *ctr, rhp_idx vi, double lb, double ub)
{
   return ctr->ops->setvarbounds(ctr, vi, lb, ub);
}

/** @brief Set the value of a variable
 *
 *  @param ctr   the container
 *  @param vi    the variable
 *  @param val   the variable name
 *
 *  @return      the error code
 */
int ctr_setvarval(Container *ctr, rhp_idx vi, double val)
{
   return ctr->ops->setvarval(ctr, vi, val);
}

/** @brief Set the name of a variable
 *
 *  @param ctr   the container
 *  @param vi    the variable
 *  @param name  the variable name
 *
 *  @return      the error code
 */
int ctr_setvarname(Container *ctr, rhp_idx vi, const char *name)
{
   return ctr->ops->setvarname(ctr, vi, name);
}

/** @brief Set the multiplier of a variable
 *
 *  @param ctr   the container
 *  @param vi    the variable
 *  @param mult  the variable multiplier
 *
 *  @return      the error code
 */
int ctr_setvarmult(Container *ctr, rhp_idx vi, double mult)
{
   return ctr->ops->setvarval(ctr, vi, mult);
}

/** @brief Set the basis status of a variable
 *
 *  @param ctr   the container
 *  @param vi    the variable index
 *  @param basis_status  the basis status of the variable
 *
 *  @return      the error code
 */
int ctr_setvarbasis(Container *ctr, rhp_idx vi, int basis_status)
{
   return ctr->ops->setvarbasis(ctr, vi, basis_status);
}

/** @brief Set the type of a variable
 *
 *  @param ctr   the container
 *  @param vi    the variable
 *  @param type  the variable type
 *
 *  @return      the error code
 */
int ctr_setvartype(Container *ctr, rhp_idx vi, unsigned type)
{
   return ctr->ops->setvartype(ctr, vi, type);
}

/** @brief Set the upper bound of a variable
 *
 *  @param ctr the container
 *  @param vi  the variable
 *  @param ub  the variable upper bound
 *
 *  @return      the error code
 */
int ctr_setvarub(Container *ctr, rhp_idx vi, double ub)
{
   return ctr->ops->setvarub(ctr, vi, ub);
}
