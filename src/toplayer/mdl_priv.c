#include "mdl_priv.h"  
#include "mdl.h"

/**
 * @brief Set up the parent-child relationship between two models
 *
 * @param mdl_src The source model
 * @param mdl_dst The destination model
 */
void mdl_linkmodels(Model *mdl_src, Model *mdl_dst)
{
   assert(!mdl_dst->mdl_up);
   mdl_dst->mdl_up = mdl_borrow(mdl_src);
   mdl_dst->ctr.ctr_up = &mdl_src->ctr;
   mdl_timings_rel(mdl_dst->timings);
   mdl_dst->timings = mdl_timings_borrow(mdl_src->timings);
}
