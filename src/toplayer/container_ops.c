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

int ctr_evalfunc(const Container *ctr, rhp_idx ei, double *x, double *f,
                 int *numerr)
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
 * @ingroup publicAPI
 *
 * @param ctr    the model
 * @param e      the abstract equation
 * @param basis  the basis array
 *
 * @return       the error code
 */
int ctr_getequsbasis(const Container *ctr, Aequ *e, int *basis)
{
   return ctr->ops->getequsbasis(ctr, e, basis);
}

/**
 * @brief Get the multipliers (if available) for an abstract equation
 *
 * @ingroup publicAPI
 *
 * @param ctr   the model
 * @param e     the abstract equation
 * @param mult  the multiplier array
 *
 * @return       the error code
 */
int ctr_getequsmult(const Container *ctr, Aequ *e, double *mult)
{
   return ctr->ops->getequsmult(ctr, e, mult);
}

/**
 * @brief Get the values for an abstract equation
 *
 * @ingroup publicAPI
 *
 * @param ctr   the model
 * @param e     the abstract equation
 * @param vals  the values array
 *
 * @return       the error code
 */
int ctr_getequsval(const Container *ctr, Aequ *e, double *vals)
{
   return ctr->ops->getequsval(ctr, e, vals);
}

/**
 * @brief Get the basis status (if available) for an abstract variable
 *
 * @ingroup publicAPI
 *
 * @param ctr    the model
 * @param v      the abstract var
 * @param basis  the basis array
 *
 * @return       the error code
 */
int ctr_getvarsbasis(const Container *ctr, Avar *v, int *basis)
{
   return ctr->ops->getvarsbasis(ctr, v, basis);
}

/**
 * @brief Get the multipliers (if available) for an abstract variable
 *
 * @ingroup publicAPI
 *
 * @param ctr   the model
 * @param v     the abstract var
 * @param mult  the multiplier array
 *
 * @return       the error code
 */
int ctr_getvarsmult(const Container *ctr, Avar *v, double *mult)
{
   return ctr->ops->getvarsmult(ctr, v, mult);
}

/**
 * @brief Get the values for an abstract variable
 *
 * @ingroup publicAPI
 *
 * @param ctr   the model
 * @param v     the abstract var
 * @param vals  the values array
 *
 * @return       the error code
 */
int ctr_getvarsval(const Container *ctr, Avar *v, double *vals)
{
   return ctr->ops->getvarsval(ctr, v, vals);
}

/**
 * @brief Get an equation by its name
 *
 * @ingroup publicAPI
 *
 * @param       ctr   the model
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
 * @ingroup publicAPI
 *
 * @param      ctr  the model
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
 * @ingroup publicAPI
 *
 * @param      ctr   the model
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
 * @ingroup publicAPI
 *
 * @param      ctr   the model
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
 * @ingroup publicAPI
 *
 * @param ctr   the model
 * @param mult  the multiplier array, with size given by ctr_m
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
 * @ingroup publicAPI
 *
 * @param      ctr    the model
 * @param      ei     the equation index
 * @param[out] basis  the basis status
 *
 * @return            the error code
 */
int ctr_getequbasis(const Container *ctr, rhp_idx ei, int *basis)
{
   return ctr->ops->getequbasis(ctr, ei, basis);
}

/**
 * @brief Get all equations values
 *
 * @ingroup publicAPI
 *
 * @param ctr   the model
 * @param vals  the value array, with size given by ctr_m
 *
 * @return      the error code
 */
int ctr_getallequsval(const Container *ctr, double *vals)
{
   return ctr->ops->getallequsval(ctr, vals);
}

int ctr_getequtype(const Container *ctr, rhp_idx ei, unsigned *type, unsigned *cone)
{
   return ctr->ops->getequtype(ctr, ei, type, cone);
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



int ctr_getcst(const Container *ctr, rhp_idx ei, double *val)
{
   return ctr->ops->getequcst(ctr, ei, val);
}

int ctr_getspecialfloats(const Container *ctr, double *minf, double *pinf, double *nan)
{
   return ctr->ops->getspecialfloats(ctr, minf, pinf, nan);
}

int ctr_getvarbounds(const Container *ctr, rhp_idx vi, double *lb, double *ub)
{
   return ctr->ops->getvarbounds(ctr, vi, lb, ub);
}

int ctr_getvarbyname(const Container *ctr, const char* name, rhp_idx *vi)
{
   return ctr->ops->getvarbyname(ctr, name, vi);
}

int ctr_getvarval(const Container *ctr, rhp_idx vi, double *value)
{
   return ctr->ops->getvarval(ctr, vi, value);
}

int ctr_getvarmult(const Container *ctr, rhp_idx vi, double *multiplier)
{
   return ctr->ops->getvarmult(ctr, vi, multiplier);
}

int ctr_getvarperp(const Container *ctr, rhp_idx vi, rhp_idx *ei)
{
   return ctr->ops->getvarperp(ctr, vi, ei);
}

int ctr_getallvarsmult(const Container *ctr, double *mult)
{
   return ctr->ops->getallvarsmult(ctr, mult);
}

int ctr_getvarbasis(const Container *ctr, rhp_idx vi, int *basis)
{
   return ctr->ops->getvarbasis(ctr, vi, basis);
}

int ctr_getallvarsval(const Container *ctr, double *vals)
{
   return ctr->ops->getallvarsval(ctr, vals);
}

int ctr_getvartype(const Container *ctr, rhp_idx vi, unsigned *type)
{
   return ctr->ops->getvartype(ctr, vi, type);
}

int ctr_isequNL(const Container *ctr, rhp_idx ei, bool *isNL)
{
   return ctr->ops->isequNL(ctr, ei, isNL);
}

int ctr_setequbasis(Container *ctr, rhp_idx ei, int basis)
{
   return ctr->ops->setequbasis(ctr, ei, basis);
}

int ctr_setequvalue(Container *ctr, rhp_idx ei, double value)
{
   return ctr->ops->setequval(ctr, ei, value);
}

/**
 *  @brief Set the name of an equation
 *
 *  @ingroup publicAPI
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

int ctr_setequmult(Container *ctr, rhp_idx ei, double multiplier)
{
   return ctr->ops->setequmult(ctr, ei, multiplier);
}

int ctr_setequtype(Container *ctr, rhp_idx ei, unsigned type, unsigned cone)
{
   return ctr->ops->setequtype(ctr, ei, type, cone);
}

int ctr_setcst(Container *ctr, rhp_idx ei, double val)
{
   return ctr->ops->setequcst(ctr, ei, val);
}


int ctr_setvarlb(Container *ctr, rhp_idx vi, double lb)
{
   return ctr->ops->setvarlb(ctr, vi, lb);
}

int ctr_setvarvalue(Container *ctr, rhp_idx vi, double varl)
{
   return ctr->ops->setvarval(ctr, vi, varl);
}

/**
 *  @brief Set the name of a variable
 *
 *  @ingroup publicAPI
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

int ctr_setvarmult(Container *ctr, rhp_idx vi, double varm)
{
   return ctr->ops->setvarval(ctr, vi, varm);
}

int ctr_setvarbasis(Container *ctr, rhp_idx vi, int basis)
{
   return ctr->ops->setvarbasis(ctr, vi, basis);
}

int ctr_setvartype(Container *ctr, rhp_idx vi, unsigned type)
{
   return ctr->ops->setvartype(ctr, vi, type);
}

int ctr_setvarub(Container *ctr, rhp_idx vi, double ub)
{
   return ctr->ops->setvarub(ctr, vi, ub);
}


