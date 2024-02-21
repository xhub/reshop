#ifndef RHP_PYTHON_STRUCTS
#define RHP_PYTHON_STRUCTS

#include "reshop.h"
#include "rhpidx.h"

/** Basis status */
typedef struct {
   enum rhp_basis_status status;
} BasisStatus;

/** Reference to a variable */
typedef struct rhp_variable_ref {
   rhp_idx_typed vi;
   struct rhp_mdl *mdl;
} VariableRef;

/** Reference to an equation */
typedef struct rhp_equation_ref {
   rhp_idx_typed ei;
   struct rhp_mdl *mdl;
} EquationRef;


/* SWIG does not deal well with opaque pointer ... */


#endif
