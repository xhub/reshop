#include "reshop_config.h"

/* For M_PI and such  */
#define _USE_MATH_DEFINES

#include <math.h>
#include <float.h>

#include "instr.h"
#include "macros.h"
#include "pool.h"
#include "printout.h"
#include "reshop.h"


/** 
 *  @brief allocate a pool
 *
 *  @return the allocated pool
 */
NlPool* pool_new(void)
{
   NlPool *pool;
   MALLOC_NULL(pool, NlPool, 1);

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
void pool_release(NlPool* pool)
{
   if (!pool) {
      return;
   }

   if (pool->cnt > 0) {
      pool->cnt--;
   } else {
      error("%s ERROR: cnt is already 0!\n", __func__);
      return;
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
NlPool* pool_copy_and_own(NlPool* p)
{
   assert(p->data);

   NlPool* pcopy; 
   AA_CHECK(pcopy, pool_new());

   MALLOC_EXIT_NULL(pcopy->data, double, p->max);
   memcpy(pcopy->data, p->data, p->len*sizeof(double));
   pcopy->len = p->len;
   pcopy->max = p->max;
   pcopy->type = 0;
   pcopy->own = true;

   return pcopy;

_exit:
   pool_release(pcopy);
   return NULL;
}


/** 
 *  @brief Create a GAMS pool
 *
 *  @return a GAMS pool or NULL if there is an error
 */
NlPool* pool_create_gams(void)
{
   NlPool *p;
   AA_CHECK(p, pool_new());

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
   pool_release(p);
   return NULL;
}

/** 
 *  @brief  get a pointer to the pool and increment the reference counter
 *
 *  @param p  the pool
 *
 *  @return   a pointer to the pool
 */
NlPool* pool_get(NlPool* p)
{
   if (p) {
      p->cnt++;
   }

   return p;
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

             NlPool *p = pool_new();
             p->max = MAX(2*pool->max, pool->len + 10);

             MALLOC_(p->data, double, p->max);
             memcpy(p->data, pool->data, pool->len*sizeof(double));
             p->own = true;
             p->len = pool->len;

            /* Copy counter value and decrease the counter of the original one
             * We do not need to free here has the data was not owned and the
             * ownership of the NlPool object is transfered
             */
             p->cnt = pool->cnt;
             pool->cnt--;

             memcpy(pool, p, sizeof(*p));

            FREE(p);
          }
      }

       pool->data[pool->len++] = val;
       /*  tree is 1-based --xhub*/
       pool_idx = pool->len;
   }

   return pool_idx;
}
