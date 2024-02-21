#ifndef FOOC_H
#define FOOC_H

#include "rhp_fwd.h"
#include "empdag_data.h"

/** @file fooc.h
 *
 * @brief First-Order Optimality Condition functions
 */

/** MCP statistics */
typedef struct mcp_stats {
   size_t mcp_size;            /**< size of the MCP */
   size_t n_primalvars;       /**< number of primal variable */
   size_t n_constraints;       /**< Number of constraints/multipliers */
   size_t n_lincons;           /**< Number of affine constraints */
   size_t n_nlcons;            /**< Number of NL constraints */
   size_t n_vifuncs;           /**< Number of VI functions   */
   size_t n_vizerofuncs;       /**< Number of VI zero functions */
} McpStats;

typedef struct mcp_def {
   daguid_t uid;               /**< uid of the subdag to be considered  */
//   Fops *fops_vars;            /**< If non-null, the fops for variables */
} McpDef;

int fooc_create_mcp(Model *mdl, McpStats * restrict mcpdata) NONNULL;
int fooc_create_vi(Model *mdl, McpStats * restrict mcpdata) NONNULL;
int fooc_mcp(Model *mdl_mcp, McpDef *mcpdef, McpStats * restrict mcpstats) NONNULL;
int fooc_postprocess(Model *mdl, unsigned n_primal) NONNULL;

#endif /* FOOC_H  */
