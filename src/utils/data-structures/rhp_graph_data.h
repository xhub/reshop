#ifndef RHP_GRAPH_DATA_H
#define RHP_GRAPH_DATA_H

typedef enum dfs_state {
   NotExplored  = 0,    /* This MUST be 0 to be initialized via calloc */
   InProgress   = 1,
   Processed    = 2,
   CycleStart   = 3,
   ErrorState   = 4,

} DfsState;

#endif
