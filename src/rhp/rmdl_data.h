#ifndef RMDL_DATA_H
#define RMDL_DATA_H

/** @file rmdl_data.h
 *
 *  @brief ReSHOP model data
 */

#include "gams_option.h"

typedef enum rmdl_solver {
   RMDL_SOLVER_UNSET,
   RMDL_SOLVER_PATHVI_GE,
   RMDL_SOLVER_PATHVI_MCP,
   RMDL_SOLVER_PATH,
   RMDL_SOLVER_GAMS,
   __RMDL_SOLVER_SIZE,
} RhpSolver;

typedef enum rmdl_status {
   Rmdl_NoStatus = 0,
   Rmdl_Editable = 1, /**< Model can be edited, no need to copy              */
} RmdlStatus;

typedef struct rmdl_data {
   enum gams_modelstatus modelstat;
   enum gams_solvestatus solvestat;
   RmdlStatus status;
   RhpSolver solver;               /**< Solver for this model                */
   struct rmdl_option *options;    /**< Options                              */
} RhpModelData;

#endif
