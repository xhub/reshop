#ifndef RMDL_POOL_H
#define RMDL_POOL_H

#include <stdbool.h>
#include <stddef.h>

#include "compat.h"

/** 
 * @file pool.h
 *
 * @brief elementary pool support
 * */

/** @brief pool object */
typedef struct nltree_pool {
   double *data;             /**< array for the numbers */
   size_t len;               /**< current count of numbers stored in data */
   size_t max;               /**< size of the array data */
   unsigned  type;           /**< type of container owning this pool */
   unsigned char cnt;        /**< reference counter */
   bool own;                 /**< true if the array is managed by the program */
} NlPool;

void pool_release(NlPool* pool);
NlPool* pool_new(void) MALLOC_ATTR(pool_release,1);

NlPool* pool_copy_and_own(NlPool* pool) NONNULL;
NlPool* pool_new_gams(void);
NlPool* pool_get(NlPool* pool);

int pool_copy_and_own_data(NlPool* pool, size_t size) NONNULL;
unsigned pool_getidx(NlPool *pool, double val) NONNULL;

#endif
