
#include "empinterp_linkbuilder.h"

#include <stddef.h>
#include "empdag_uid.h"
#include "empinterp.h"
#include "macros.h"
#include "printout.h"

LinkLabels * linklabels_new(LinkType type, const char *label, unsigned label_len,
                           uint8_t dim, uint8_t num_vars, unsigned max_children)
{
   LinkLabels *link = NULL;
   CALLOCBYTES_EXIT_NULL(link, LinkLabels, sizeof(LinkLabels) + sizeof(int) * (dim+num_vars));

   if (max_children > 0) {
      MALLOC_EXIT_NULL(link->uels_var, int, (size_t)max_children*num_vars); 
      MALLOC_EXIT_NULL(link->vi, rhp_idx, (size_t)max_children); 
      MALLOC_EXIT_NULL(link->coeff, double, (size_t)max_children); 
   } else {
      link->uels_var = NULL;
      link->vi = NULL;
      link->coeff = NULL;
   }

   link->linktype = type;

   switch (type) {
   case LinkObjAddMapSmoothed:

   default: ;
      
   }
   link->extras = NULL;

   link->dim = dim;
   link->num_var = num_vars;
   link->label_len = label_len;

   link->num_children = 0;
   link->max_children = max_children;
   link->daguid_parent = EMPDAG_UID_NONE;
   link->label = label;

   return link;
_exit:
   free(link->uels_var);
   free(link->vi);
   free(link->coeff);
   free(link);
   return NULL;
}

LinkLabels * linklabels_dup(const LinkLabels * link_src)
{
   LinkLabels *link_cpy = NULL;
   uint8_t dim = link_src->dim;
   uint8_t num_var = link_src->num_var;
   unsigned max_children = link_src->max_children;

   size_t data_size = sizeof(int) * (dim + num_var);
   MALLOCBYTES_NULL(link_cpy, LinkLabels, sizeof(LinkLabels) + data_size);

   memcpy(link_cpy, link_src, sizeof(LinkLabels) + data_size);

   if (max_children > 0) {
      MALLOC_EXIT_NULL(link_cpy->uels_var, int, (size_t)num_var*max_children);
      MALLOC_EXIT_NULL(link_cpy->vi, rhp_idx, max_children);
      MALLOC_EXIT_NULL(link_cpy->coeff, double, max_children);

   } else {
      link_cpy->uels_var = NULL;
      link_cpy->vi = NULL;
      link_cpy->coeff = NULL;
   }

   return link_cpy;
_exit:
   FREE(link_cpy);
   return NULL;
}

LinkLabel * linklabels_dupaslabel(const LinkLabels * link_src)
{
   LinkLabel *link;
   uint8_t dim = link_src->dim;
   assert(link_src->num_children <= 1);
   MALLOCBYTES_NULL(link, LinkLabel, sizeof(LinkLabel) + sizeof(int) * dim);

   link->dim = dim;
   link->label_len = link_src->label_len;
   link->label = link_src->label;
   memcpy(link->uels, link_src->data, sizeof(int) * dim);

   link->daguid_parent = link_src->daguid_parent;
   link->vi = IdxNA;
   link->coeff = 1.;
   link->linktype = link_src->linktype;

   return link;
}

int linklabels_add(LinkLabels *link, int *uels, double coeff, rhp_idx vi)
{
   uint8_t num_var = link->num_var;
   unsigned num_children = link->num_children, max_children = link->max_children;

   if (num_children >= max_children) {
      unsigned size = link->max_children = MAX(2*max_children, num_children + 10);
      if (num_var > 0) {
         size_t size_data = (num_var*size);
         REALLOC_(link->uels_var, int, size_data);
      }
      REALLOC_(link->coeff, double, size);
      REALLOC_(link->vi, rhp_idx, size);
   }

   if (uels) {
      memcpy(&link->uels_var[(size_t)num_var*num_children], uels, sizeof(int)*num_var);
   }

   link->coeff[num_children] = coeff;
   link->vi[num_children] = vi;

   link->num_children++;

   return OK;
}

void linklabels_free(LinkLabels *link)
{
   if (!link) return;

   FREE(link->uels_var);
   FREE(link->vi);
   FREE(link->coeff);
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
   link->coeff = 1;

   return link;
}

void linklabel_free(LinkLabel *link)
{
   FREE(link);
}

DualLabel* dual_label_new(const char *identname, unsigned identname_len, uint8_t dim, mpid_t mpid_dual)
{
   DualLabel *dual;
   MALLOCBYTES_NULL(dual, DualLabel, sizeof(DualLabel) + (sizeof(int) * dim));

   dual->dim = dim;
   dual->label_len = identname_len;
   dual->mpid_dual = mpid_dual;
   dual->label = identname;

   return dual;
}

void dual_label_free(DualLabel *dual)
{
   free(dual);
}
