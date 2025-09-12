
#include "empinterp_linkbuilder.h"

#include <stddef.h>
#include "empdag_uid.h"
#include "empinterp.h"
#include "macros.h"
#include "mathprgm_data.h"
#include "printout.h"

bool linklabels_valid(const LinkLabels *l)
{
   if (l->nvaridxs > l->dim) return false;
   return true;
}

LinkLabels * linklabels_new(LinkType type, const char *label, unsigned label_len,
                            uint8_t dim, uint8_t nvaridxs, unsigned maxrecs)
{
   LinkLabels *link = NULL;
   assert(dim >= nvaridxs);
   CALLOCBYTES_EXIT_NULL(link, LinkLabels, sizeof(LinkLabels) + sizeof(int) * (dim+nvaridxs));

   // HACK: check that uels_var needs to be of size num_vars.
   // That is not obvious as OP_LINKLABELS_SETFROM_LOOPVAR touches .data ...
   if (maxrecs > 0) {
      MALLOC_EXIT_NULL(link->uels_var, int, (size_t)maxrecs*nvaridxs); 
      MALLOC_EXIT_NULL(link->vi, rhp_idx, (size_t)maxrecs); 
      MALLOC_EXIT_NULL(link->coeff, double, (size_t)maxrecs); 
   } else {
      link->uels_var = NULL;
      link->vi = NULL;
      link->coeff = NULL;
   }

   link->linktype = type;

   switch (type) {
   case LinkObjAddMapSmoothed:
      // HACK: what was our thought process here?

   default: ;
   }

   link->extras = NULL;

   link->dim = dim;
   link->nvaridxs = nvaridxs;
   link->label_len = label_len;

   link->nrecs = 0;
   link->maxrecs = maxrecs;
   link->daguid_parent = EMPDAG_UID_NONE;
   link->label = label;

   assert(linklabels_valid(link));

   return link;
_exit:
   free(link->uels_var);
   free(link->vi);
   free(link->coeff);
   free(link);
   return NULL;
}

LinkLabels * linklabels_dup(const LinkLabels *link_src)
{
   assert(linklabels_valid(link_src));

   LinkLabels *link_cpy = NULL;
   uint8_t dim = link_src->dim;
   uint8_t nvaridxs = link_src->nvaridxs;
   unsigned max_children = link_src->maxrecs;

   size_t data_size = sizeof(int) * (dim + nvaridxs);
   MALLOCBYTES_NULL(link_cpy, LinkLabels, sizeof(LinkLabels) + data_size);

   memcpy(link_cpy, link_src, sizeof(LinkLabels) + data_size);

   if (max_children > 0) {

      MALLOC_EXIT_NULL(link_cpy->uels_var, int, (size_t)nvaridxs*max_children);
      MALLOC_EXIT_NULL(link_cpy->vi, rhp_idx, max_children);
      MALLOC_EXIT_NULL(link_cpy->coeff, double, max_children);

   } else {
      link_cpy->uels_var = NULL;
      link_cpy->vi = NULL;
      link_cpy->coeff = NULL;
   }

   return link_cpy;

_exit:
   free(link_cpy);
   return NULL;
}

LinkLabel * linklabels_dupaslabel(const LinkLabels *link_src, double coeff, rhp_idx vi)
{
   assert(linklabels_valid(link_src));

   LinkLabel *link;
   uint8_t dim = link_src->dim;
   assert(link_src->nrecs <= 1);
   MALLOCBYTES_NULL(link, LinkLabel, sizeof(LinkLabel) + sizeof(int) * dim);

   link->dim = dim;
   link->label_len = link_src->label_len;
   link->label = link_src->label;
   memcpy(link->uels, link_src->data, sizeof(int) * dim);

   link->daguid_parent = link_src->daguid_parent;
   link->vi = vi;
   link->coeff = coeff;
   link->linktype = link_src->linktype;

   return link;
}

int linklabels_add(LinkLabels *link, int *uels, double coeff, rhp_idx vi)
{
   assert(linklabels_valid(link));

   uint8_t num_var = link->nvaridxs;
   unsigned nrecs = link->nrecs, maxrecs = link->maxrecs;

   if (nrecs >= maxrecs) {
      unsigned size = link->maxrecs = MAX(2*maxrecs, nrecs + 10);
      if (num_var > 0) {
         size_t size_data = (num_var*size);
         REALLOC_(link->uels_var, int, size_data);
      }
      REALLOC_(link->coeff, double, size);
      REALLOC_(link->vi, rhp_idx, size);
      REALLOC_(link->extras, void*, size);
   }

   if (uels) {
      memcpy(&link->uels_var[(size_t)num_var*nrecs], uels, sizeof(*uels)*num_var);
   }

   link->coeff[nrecs] = coeff;
   link->vi[nrecs] = vi;

   link->nrecs++;

   return OK;
}

void linklabels_free(LinkLabels *link)
{
   if (!link) return;

   FREE(link->uels_var);
   FREE(link->vi);
   FREE(link->coeff);
   FREE(link->extras);
   FREE(link);
}

LinkLabel* linklabel_new(const char *label, unsigned label_len, uint8_t dim)
{
   LinkLabel *link;
   MALLOCBYTES_NULL(link, LinkLabel, sizeof(LinkLabel) + (sizeof(int) * dim));

   link->dim = dim;
   link->label_len = label_len;
   link->daguid_parent = EMPDAG_UID_NONE;
   link->label = label;
   link->vi = IdxNA;
   link->coeff = 1.;

   return link;
}

void linklabel_free(LinkLabel *link)
{
   FREE(link);
}

DualsLabel* dualslabel_new(const char *label, unsigned label_len, uint8_t dim,
                           uint8_t num_vars, DualOperatorData *opdat)
{
   DualsLabel* dualslabel;
   CALLOCBYTES_EXIT_NULL(dualslabel, DualsLabel, sizeof(*dualslabel) + sizeof(int) * (dim+num_vars));

   dualslabel->dim = dim;
   dualslabel->nvaridxs = num_vars;
   dualslabel->label = label;
   dualslabel->label_len = label_len;

   unsigned max_children = 3;

   MALLOC_EXIT_NULL(dualslabel->uels_var, int, (size_t)max_children*num_vars); 

   mpidarray_init(&dualslabel->mpid_duals);
   mpidarray_reserve(&dualslabel->mpid_duals, max_children);

   dualslabel->opdat = *opdat;
 
   return dualslabel;

_exit:
   IGNORE_DEALLOC_MISMATCH(dualslabel_free(dualslabel));

   return NULL;
}

int dualslabel_add(DualsLabel *dualslabel, int *uels, uint8_t nuels, mpid_t mpid_dual)
{
   uint8_t num_var = dualslabel->nvaridxs;
   /* The MpIdArray len and max is used to keep track of the other data arrays as well */
   unsigned num_children = dualslabel->mpid_duals.len, max_children = dualslabel->mpid_duals.max;

   if (num_children >= max_children) {
      unsigned size = MAX(2*max_children, num_children + 10);
      if (num_var > 0) {
         size_t size_data = (num_var*size);
         REALLOC_(dualslabel->uels_var, int, size_data);
      }
      mpidarray_reserve(&dualslabel->mpid_duals, size);
   }

   if (uels) {
      // HACK
      assert(nuels == num_var);
      memcpy(&dualslabel->uels_var[(size_t)(num_children*num_var)], uels, num_var*sizeof(*uels));
   }
   mpidarray_add(&dualslabel->mpid_duals, mpid_dual);

   return OK;
}

void dualslabel_free(DualsLabel *dualslabel)
{
   if (!dualslabel) { return; }

   free(dualslabel->uels_var);
   mpidarray_empty(&dualslabel->mpid_duals);
   free(dualslabel);
}

DualLabel* dual_label_new(const char *label, unsigned label_len, uint8_t dim, mpid_t mpid_dual)
{
   DualLabel *dual;
   MALLOCBYTES_NULL(dual, DualLabel, sizeof(DualLabel) + (sizeof(int) * dim));

   dual->dim = dim;
   dual->label_len = label_len;
   dual->mpid_dual = mpid_dual;
   dual->label = label;

   return dual;
}
