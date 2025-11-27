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
 * @param      ctr          the container
 * @param      e            the abstract equation
 * @param[out] ebasis_info  the basis array
 *
 * @return       the error code
 */
int ctr_getequsbasis(const Container *ctr, Aequ *e, int *ebasis_info)
{
   return ctr->ops->getequsbasis(ctr, e, ebasis_info);
}

/**
 * @brief Get the multipliers (if available) for an abstract equation
 *
 * @param      ctr    the container
 * @param      e      the abstract equation
 * @param[out] edual  the dual multiplier array
 *
 * @return       the error code
 */
int ctr_getequsdual(const Container *ctr, Aequ *e, double *edual)
{
   return ctr->ops->getequsdual(ctr, e, edual);
}

/**
 * @brief Get the values for an abstract equation
 *
 * @param      ctr     the container
 * @param      e       the abstract equation
 * @param[out] elevel  the values array
 *
 * @return       the error code
 */
int ctr_getequslevel(const Container *ctr, Aequ *e, double *elevel)
{
   return ctr->ops->getequslevel(ctr, e, elevel);
}

/**
 * @brief Get the basis status (if available) for an abstract variable
 *
 * @param       ctr         the container
 * @param       v           the abstract var
 * @param[out]  vbasis_info  the basis array
 *
 * @return       the error code
 */
int ctr_getvarsbasis(const Container *ctr, Avar *v, int *vbasis_info)
{
   return ctr->ops->getvarsbasis(ctr, v, vbasis_info);
}

/**
 * @brief Get the multipliers (if available) for an abstract variable
 *
 * @param       ctr    the container
 * @param       v      the abstract var
 * @param[out]  vdual  the multiplier array
 *
 * @return       the error code
 */
int ctr_getvarsdual(const Container *ctr, Avar *v, double *vdual)
{
   return ctr->ops->getvarsdual(ctr, v, vdual);
}

/**
 * @brief Get the values for an abstract variable
 *
 * @param      ctr     the container
 * @param      v       the abstract var
 * @param[out] vlevel  the values array
 *
 * @return       the error code
 */
int ctr_getvarslevel(const Container *ctr, Avar *v, double *vlevel)
{
   return ctr->ops->getvarslevel(ctr, v, vlevel);
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
 * @param      ctr    the container
 * @param      ei     the equation index
 * @param[out] level  the value
 *
 * @return     the error code
 */
int ctr_getequlevel(const Container *ctr, rhp_idx ei, double *level)
{
   return ctr->ops->getequlevel(ctr, ei, level);
}

/**
 * @brief Get the multiplier of an equation
 *
 * @param      ctr   the container
 * @param      ei    the equation index
 * @param[out] dual  the dual multiplier
 *
 * @return           the error code
 */
int ctr_getequdual(const Container *ctr, rhp_idx ei, double *dual)
{
   return ctr->ops->getequdual(ctr, ei, dual);
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
 * @brief Get the basis status of an equation
 *
 * @param      ctr          the container
 * @param[out] basis_infos  the basis status
 *
 * @return            the error code
 */
int ctr_getallequsbasis(const Container *ctr, int *basis_infos)
{
   return ctr->ops->getallequsbasis(ctr, basis_infos);
}

/**
 * @brief Get all equations multiplier
 *
 * @param       ctr    the container
 * @param[out]  duals  the multiplier array, of appropriate size
 *
 * @return      the error code
 */
int ctr_getallequsdual(const Container *ctr, double *duals)
{
   return ctr->ops->getallequsdual(ctr, duals);
}

/** @brief Get all equations level values
 *
 * @param       ctr     the container
 * @param[out]  levels  the value array, of appropriate size
 *
 * @return      the error code
 */
int ctr_getallequslevel(const Container *ctr, double *levels)
{
   return ctr->ops->getallequslevel(ctr, levels);
}

/**
 * @brief Get the basis status of an equation
 *
 * @param      ctr         the container
 * @param      ei          the equation index
 * @param[out] basis_info  the basis status
 *
 * @return            the error code
 */
int ctr_getequbasis(const Container *ctr, rhp_idx ei, int *basis_info)
{
   return ctr->ops->getequbasis(ctr, ei, basis_info);
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
 * @param[out] cst the constant value
 *
 * @return         the error code
 */
int ctr_getequcst(const Container *ctr, rhp_idx ei, double *cst)
{
   return ctr->ops->getequcst(ctr, ei, cst);
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
 * @param      ctr    the container
 * @param      vi     the variable index
 * @param[out] level  the variable level value
 *
 * @return          the error code
 */
int ctr_getvarlevel(const Container *ctr, rhp_idx vi, double *level)
{
   return ctr->ops->getvarlevel(ctr, vi, level);
}

/** @brief Get the multiplier of a variable
 *
 * @param      ctr   the container
 * @param      vi    the variable index
 * @param[out] dual  the variable multiplier
 *
 * @return          the error code
 */
int ctr_getvardual(const Container *ctr, rhp_idx vi, double *dual)
{
   return ctr->ops->getvardual(ctr, vi, dual);
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
 * @param      ctr         the container
 * @param      vi          the variable index
 * @param[out] basis_info  the variable basis status
 *
 * @return          the error code
 */
int ctr_getvarbasis(const Container *ctr, rhp_idx vi, int *basis_info)
{
   return ctr->ops->getvarbasis(ctr, vi, basis_info);
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

/** @brief Get all variables basis info
 *
 * @param       ctr          the container
 * @param[out]  basis_infos  the value array, of appropriate size
 *
 * @return      the error code
 */
int ctr_getallvarsbasis(const Container *ctr, int *basis_infos)
{
   return ctr->ops->getallvarsbasis(ctr, basis_infos);
}

/** @brief Get all variables values
 *
 * @param       ctr     the container
 * @param[out]  levels  the value array, of appropriate size
 *
 * @return      the error code
 */
int ctr_getallvarslevel(const Container *ctr, double *levels)
{
   return ctr->ops->getallvarslevel(ctr, levels);
}

/** @brief Get all variables multipliers
 *
 * @param       ctr    the container
 * @param[out]  duals  the multipliers array, of appropriate size
 *
 * @return      the error code
 */
int ctr_getallvarsdual(const Container *ctr, double *duals)
{
   return ctr->ops->getallvarsdual(ctr, duals);
}

/**
 * @brief Set the basis status of an equation
 *
 * @param ctr         the container
 * @param ei          the equation index
 * @param basis_info  the basis status
 * 
 * @return       the error code
 */
int ctr_setequbasis(Container *ctr, rhp_idx ei, int basis_info)
{
   return ctr->ops->setequbasis(ctr, ei, basis_info);
}

/**
 * @brief Set the (level) value of an equation
 *
 * @param ctr    the container
 * @param ei     the equation index
 * @param level  the value
 * 
 * @return       the error code
 */
int ctr_setequlevel(Container *ctr, rhp_idx ei, double level)
{
   return ctr->ops->setequlevel(ctr, ei, level);
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

/** @brief Set the dual multiplier of an equation
 *
 *  @param ctr   the container
 *  @param ei    the equation index
 *  @param dual  the equation multiplier
 *
 *  @return      the error code
 */
int ctr_setequdual(Container *ctr, rhp_idx ei, double dual)
{
   return ctr->ops->setequdual(ctr, ei, dual);
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
 *  @param cst   the constant value
 *
 *  @return      the error code
 */
int ctr_setequcst(Container *ctr, rhp_idx ei, double cst)
{
   return ctr->ops->setequcst(ctr, ei, cst);
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
 *  @param ctr     the container
 *  @param vi      the variable
 *  @param level   the variable name
 *
 *  @return      the error code
 */
int ctr_setvarlevel(Container *ctr, rhp_idx vi, double level)
{
   return ctr->ops->setvarlevel(ctr, vi, level);
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

/** @brief Set the dual multiplier of a variable
 *
 *  @param ctr   the container
 *  @param vi    the variable
 *  @param dual  the variable multiplier
 *
 *  @return      the error code
 */
int ctr_setvardual(Container *ctr, rhp_idx vi, double dual)
{
   return ctr->ops->setvarlevel(ctr, vi, dual);
}

/** @brief Set the basis status of a variable
 *
 *  @param ctr         the container
 *  @param vi          the variable index
 *  @param basis_info  the basis status of the variable
 *
 *  @return      the error code
 */
int ctr_setvarbasis(Container *ctr, rhp_idx vi, int basis_info)
{
   return ctr->ops->setvarbasis(ctr, vi, basis_info);
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
