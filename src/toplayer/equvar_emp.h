#ifndef EMPVAR_EMP_H
#define EMPVAR_EMP_H

#include <stdbool.h>

#include "compat.h"
#include "reshop_data.h"
#include "rhp_fwd.h"

/** Disjunction piece */
typedef struct {
   bool var_not;   /**< If true, piece is active when variable is off       */
   bool equ_not;   /**< If true, equations are inactive when variable is on */
   rhp_idx vi;     /**< Binary variable controlling the activation          */
   IdxArray equs;  /**< Equations controlled by the variable                */
} DisjunctionPiece;

/** Disjunction: union of controlled parts */
typedef struct {
   unsigned npieces;                                /**< number of pieces */
   DisjunctionPiece pieces[] __counted_by(npieces); /**< array of pieces  */
} Disjunction;

/** Array of disjunction */
typedef struct {
   unsigned len;
   unsigned max;
   Disjunction **arr;
} Disjunctions;

/** EMP annotation involving only collections of variables and equations */
typedef struct {
   unsigned num_deffn;          /**< number of mappings                       */
   unsigned num_explicit;       /**< number of explicit mapping               */
   unsigned num_implicit;       /**< number of implicit variable              */

   Disjunctions disjunctions;   /**< Disjunctions                             */
   IdxArray marginalVars;       /**< marginal variables "dualvar"             */ 
} EquVarEmp;


Disjunction* disjunction_new(unsigned npieces) NONNULL;

void disjunctions_init(Disjunctions* disj_arr) NONNULL;
void disjunctions_empty(Disjunctions* disj_arr) NONNULL;
int disjunctions_add(Disjunctions* disj_arr, Disjunction* disj) NONNULL;
int disjunctions_copy(Disjunctions* disj_arr_dst, const Disjunctions* disj_arr_src) NONNULL;


#endif /* EMPVAR_EMP_H */
