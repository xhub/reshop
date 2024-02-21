#include "instr.h"
#include "reshop_config.h"
#include <float.h>
/* For M_PI and such  */
#define _USE_MATH_DEFINES

#include <math.h>

#include "macros.h"
#include "pool.h"
#include "printout.h"
#include "reshop.h"


/** 
 *  @brief allocate a pool
 *
 *  @return the allocated pool
 */
struct nltree_pool* pool_alloc(void)
{
   struct nltree_pool *pool;
   MALLOC_NULL(pool, struct nltree_pool, 1);

   pool->data = NULL;
   pool->cnt = 1;
   pool->len = 0;
   pool->max = 0;
   pool->type = 0;
   pool->own = false;

   return pool;
}

/**
 *  @brief deallocate a pool
 *
 *  @param  pool  the pool to deallocate
 */
void pool_dealloc(struct nltree_pool* pool)
{
   if (!pool) {
      return;
   }

   if (pool->cnt > 0) {
      pool->cnt--;
   } else {
      error("%s :: cnt is already 0!\n", __func__);
   }

   if (pool->cnt == 0) {
      if (pool->data && pool->own) {
         FREE(pool->data);
         pool->data = NULL;
      }
      FREE(pool);
   }
}

/**
 *  @brief copy a pool in a managed object
 *
 *  The data from the original pool is copied into a new pool object which is
 *  managed by this software
 *
 *  @param p  the pool to copy
 *
 * @return    the newly created pool
 */
struct nltree_pool* pool_copy_and_own(struct nltree_pool* p)
{
   assert(p->data);

   struct nltree_pool* pcopy; 
   AA_CHECK(pcopy, pool_alloc());

   MALLOC_EXIT_NULL(pcopy->data, double, p->max);
   memcpy(pcopy->data, p->data, p->len*sizeof(double));
   pcopy->len = p->len;
   pcopy->max = p->max;
   pcopy->type = 0;
   pcopy->own = true;

   return pcopy;

_exit:
   pool_dealloc(pcopy);
   return NULL;
}


/** 
 *  @brief Create a GAMS pool
 *
 *  @return a GAMS pool or NULL if there is an error
 */
struct nltree_pool* pool_create_gams(void)
{
   struct nltree_pool *p;
   AA_CHECK(p, pool_alloc());

   MALLOC_EXIT_NULL(p->data, double, 20);

   double *pool = p->data;

   pool[0] = 1.;
   pool[1] = 10.;
   pool[2] = .1;
   pool[3] = .25;
   pool[4] = .5;
   pool[5] = 2.;
   pool[6] = 4.;
   pool[7] = 0.;
   pool[8] = 1/sqrt(2*M_PI);
   pool[9] = 1/log(10);
   pool[10] = 1/log(2);
   pool[11] = M_PI;
   pool[12] = M_PI/2;
   pool[13] = sqrt(2);
   pool[14] = 3.;
   pool[15] = 5.;
   pool[16] = 0.;

   p->len = 16;
   p->max = 20;
   p->own = true;
   p->type = RHP_BACKEND_GAMS_GMO;

   return p;

_exit:
   pool_dealloc(p);
   return NULL;
}

/** 
 *  @brief  get a pointer to the pool and increment the reference counter
 *
 *  @param p  the pool
 *
 *  @return   a pointer to the pool
 */
struct nltree_pool* pool_get(struct nltree_pool* p)
{
   if (p) {
      p->cnt++;
   }

   return p;
}


/** 
 *  @brief  decrease the reference counter
 *
 *  @warning  this function should be used only by experts
 *
 */
void pool_rel(struct nltree_pool* p)
{
   if (p) {
      assert(p->cnt > 0);
      p->cnt--;
   }
}

unsigned pool_getidx(NlPool *pool, double val)
{
   /* This is a bit gams specific */
   unsigned pool_idx = 0;

   /* ----------------------------------------------------------------------
    * Try to use one of the well-known value
    * ---------------------------------------------------------------------- */

   if (val > 0. - DBL_EPSILON || val < 10. + DBL_EPSILON) {
      if (fabs(val - 1.) < DBL_EPSILON) {
         pool_idx = nlconst_one;
      } else if (fabs(val - 10.) < 10*DBL_EPSILON) {
         pool_idx = nlconst_ten;
      } else if (val <= 5. + DBL_EPSILON) {
         if (fabs(val) < DBL_EPSILON) {
            pool_idx = nlconst_zero;
         } else if (fabs(val - 5.) < 5*DBL_EPSILON) {
            pool_idx = nlconst_five;
         } else if(fabs(val - 4.) < 4*DBL_EPSILON) {
            pool_idx = nlconst_four;
         } else if(fabs(val - 3.) < 3*DBL_EPSILON) {
            pool_idx = nlconst_three;
         } else if(fabs(val - 2.) < 2*DBL_EPSILON) {
            pool_idx = nlconst_two;
         } else if (fabs(val -1./2.) < DBL_EPSILON) {
            pool_idx = nlconst_half;
         } else if (fabs(val - 1./4.) < DBL_EPSILON) {
            pool_idx = nlconst_quater;
         }
      }
   }

   /* ----------------------------------------------------------------------
    * Add the value to the pool
    *
    * TODO(Xhub) compare w.r.t last injected value and a MRU vector
    * ---------------------------------------------------------------------- */

   if (!pool_idx) {

       if (pool->len >= pool->max) {
          if (pool->own) {
             pool->max = MAX(2*pool->max, pool->len + 10);
             REALLOC_(pool->data, double, pool->max);
          } else {
             double *orig_data = pool->data;
             struct nltree_pool *p = pool_alloc();
             p->max = MAX(2*pool->max, pool->len + 10);
             p->len = pool->len;

             MALLOC_(p->data, double, p->max);
             memcpy(p->data, orig_data, p->len*sizeof(double));
             p->own = true;
             p->cnt = pool->cnt;

             /* TODO(xhub) why not use pool_dealloc? */
             pool_rel(pool);

             memcpy(pool, p, sizeof(*p));
          }
      }

       pool->data[pool->len++] = val;
       /*  tree is 1-based --xhub*/
       pool_idx = pool->len;
   }

   return pool_idx;
}
