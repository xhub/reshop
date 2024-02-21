#ifndef GENERATORS_H
#define GENERATORS_H

#include "compat.h"

/** Generator data (list of vectors)*/
struct gen_data {
   unsigned size; /**< current number of pointer stored  */
   unsigned max;  /**< maximum size of the array val */
   double **val;  /**< array of pointers to the data*/
};

/** List of generators for a convex set*/
struct generators {
   unsigned dim;             /**< dimension of the ambient space */
   struct gen_data vertices; /**< vertices */
   struct gen_data rays;     /**< rays */
   struct gen_data lines;     /**< lines */
};

void generators_dealloc(struct generators *gen);

/** @brief Allocate a generators data structure
 *
 * @param space_dim  dimension of the space
 * @return           a generators structure or NULL if there is an error
 */
struct generators* generators_alloc(unsigned space_dim) MALLOC_ATTR(generators_dealloc,1);

int generators_add_vertex(struct generators *gen, double *v ) NONNULL;
int generators_add_ray(struct generators *gen, double *r ) NONNULL;
int generators_add_line(struct generators *gen, double *l ) NONNULL;
void generators_print(struct generators *gen ) NONNULL;
void generators_savetxt(struct generators *gen) NONNULL;
/** Release memory*/
int generators_trim_memory(struct generators *gen ) NONNULL;

#endif
