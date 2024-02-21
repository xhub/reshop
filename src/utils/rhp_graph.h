#ifndef RHP_GRAPH_H
#define RHP_GRAPH_H

#include "rhp_fwd.h"

/** @file graph.h
 *
 *  @brief graph-related functions 
 */

struct rhp_graph_gen;

struct rhp_graph_child {
   rhp_idx idx;
   struct rhp_graph_gen *child;
};

struct rhp_graph_gen* rhp_graph_gen_alloc(void *obj);
void rhp_graph_gen_free(struct rhp_graph_gen* node);
int rhp_graph_gen_set_children(struct rhp_graph_gen *node,
                               struct rhp_graph_child *children,
                               unsigned n_children) NONNULL;
int rhp_graph_gen_dfs(struct rhp_graph_gen ** restrict nodes, unsigned n_nodes,
                      ObjArray * restrict dat) NONNULL;


int ovfgraph_dot(const struct ovfinfo *ovf_info, struct rhp_graph_gen **nodes,
                 unsigned n_nodes, const Model *mdl, const char* fname)
                 NONNULL;
#endif /* RHP_GRAPH_H  */
