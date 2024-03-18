#ifndef FOOC_H
#define FOOC_H

#include "empdag_data.h"
#include "mdl_data.h"
#include "rhp_fwd.h"

/** @file fooc.h
 *
 * @brief First-Order Optimality Condition functions
 */


typedef struct mcp_def {
   daguid_t uid;               /**< uid of the subdag to be considered  */
//   Fops *fops_vars;            /**< If non-null, the fops for variables */
} McpDef;

int fooc_create_mcp(Model *mdl) NONNULL;
int fooc_create_vi(Model *mdl) NONNULL;
int fooc_mcp(Model *mdl_mcp) NONNULL;

#endif /* FOOC_H  */
