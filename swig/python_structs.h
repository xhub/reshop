#ifndef RHP_PYTHON_STRUCTS
#define RHP_PYTHON_STRUCTS

#include <stdint.h>

#include "reshop.h"
#include "rhpidx.h"
#include "rhp_defines.h"

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

/** Python EMPDAG object */
typedef struct rhp_empdag {
   const rhp_mdl_t *mdl;
} EmpDag;

typedef struct rhp_tuple_empnodes {
   const rhp_mdl_t *mdl;
} EmpNodesTuple;

typedef struct rhp_tuple_empmps {
   const rhp_mdl_t *mdl;
} EmpMathPrgmTuple;

typedef struct rhp_tuple_empnashs {
   const rhp_mdl_t *mdl;
} EmpNashEquilibriumTuple;

typedef struct rhp_tuple_emparcs {
   const rhp_mdl_t *mdl;
   u32        nidx;
} EmpArcsTuple;

#endif
