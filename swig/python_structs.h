#ifndef RHP_PYTHON_STRUCTS
#define RHP_PYTHON_STRUCTS

#include <stdint.h>

#include "reshop.h"
#include "rhpidx.h"

/** Model status */
typedef struct {
   unsigned code;
   struct rhp_mdl *mdl;
} ModelStatus;

/** Solve status */
typedef struct {
   unsigned code;
   struct rhp_mdl *mdl;
} SolveStatus;

/** Basis status */
typedef struct {
   enum rhp_basis_status status;
} BasisStatus;

/** Model backend */
typedef struct {
   enum rhp_backendtype backend;
} RhpBackendType;

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

/** Reference to an equation */
typedef struct rhp_linear_equation {
   unsigned len;
   rhp_idx_typed *vis;
   double *vals;
   struct rhp_mdl *mdl;
} LinearEquation;
#endif
