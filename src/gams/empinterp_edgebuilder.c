
#include "empinterp_edgebuilder.h"

#include <stddef.h>
#include "empdag_uid.h"
#include "empinterp.h"
#include "macros.h"
#include "printout.h"

DagLabels * dag_labels_new(const char *basename, unsigned basename_len,
                           uint8_t dim, uint8_t num_vars, unsigned size)
{
   DagLabels *dagl = NULL;
   MALLOCBYTES_EXIT_NULL(dagl, DagLabels, sizeof(DagLabels) + sizeof(int) * (dim+num_vars));
   MALLOC_EXIT_NULL(dagl->uels_var, int, (size_t)size*num_vars); 

   dagl->dim = dim;
   dagl->num_var = num_vars;
   dagl->nodename_len = basename_len;

   dagl->num_children = 0;
   dagl->max_children = size;
   dagl->daguid_parent = EMPDAG_UID_NONE;
   dagl->nodename = basename;

   return dagl;
_exit:
   FREE(dagl);
   return NULL;
}

DagLabels * dag_labels_dup(const DagLabels * dagl_src)
{
   DagLabels *dagc_cpy = NULL;
   uint8_t dim = dagl_src->dim;
   uint8_t num_var = dagl_src->num_var;
   unsigned max_children = dagl_src->max_children;

   size_t data_size = sizeof(int) * (dim + num_var);
   MALLOCBYTES_NULL(dagc_cpy, DagLabels, sizeof(DagLabels) + data_size);

   memcpy(dagc_cpy, dagl_src, sizeof(DagLabels) + data_size);

   MALLOC_EXIT_NULL(dagc_cpy->uels_var, int, (size_t)num_var*max_children);

   return dagc_cpy;
_exit:
   FREE(dagc_cpy);
   return NULL;
}

DagLabel * dag_labels_dupaslabel(const DagLabels * dagl_src)
{
   DagLabel *dagc;
   uint8_t dim = dagl_src->dim;
   assert(dagl_src->num_children <= 1);
   MALLOCBYTES_NULL(dagc, DagLabel, sizeof(DagLabel) + sizeof(int) * dim);

   dagc->dim = dim;
   dagc->nodename_len = dagl_src->nodename_len;
   dagc->nodename = dagl_src->nodename;
   memcpy(dagc->uels, dagl_src->data, sizeof(int) * dim);

   dagc->daguid_parent = dagl_src->daguid_parent;

   return dagc;
}

int dag_labels_add(DagLabels *dagl, int *uels)
{
   uint8_t num_var = dagl->num_var;
   unsigned num_children = dagl->num_children;

   if (num_children >= dagl->max_children) {
      dagl->max_children = MAX(2*dagl->max_children, dagl->num_children + 10);
      size_t size_data = (num_var*dagl->max_children);
      REALLOC_(dagl->uels_var, int, size_data);
   }

   memcpy(&dagl->uels_var[(size_t)num_var*num_children], uels, sizeof(int)*num_var);

   dagl->num_children++;

   return OK;
}

void dag_labels_free(DagLabels *dagl)
{
   if (!dagl) return;

   FREE(dagl->uels_var);
   FREE(dagl);
}

DagLabel* dag_label_new(const char *identname, unsigned identname_len, uint8_t dim)
{
   DagLabel *dagl;
   MALLOCBYTES_NULL(dagl, DagLabel, sizeof(DagLabel) + sizeof(int) * dim);

   dagl->dim = dim;
   dagl->nodename_len = identname_len;
   dagl->daguid_parent = EMPDAG_UID_NONE;
   dagl->nodename = identname;

   return dagl;
}

void dag_label_free(DagLabel *dagl)
{
   FREE(dagl);
}

DualLabel* dual_label_new(const char *identname, unsigned identname_len, uint8_t dim, mpid_t mpid_dual)
{
   DualLabel *dual;
   MALLOCBYTES_NULL(dual, DualLabel, sizeof(DualLabel) + sizeof(int) * dim);

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

FoocLabel* fooc_label_new(const char *identname, unsigned identname_len, uint8_t dim, daguid_t uid)
{
   FoocLabel *fooc;
   MALLOCBYTES_NULL(fooc, FoocLabel, sizeof(FoocLabel) + sizeof(int) * dim);

   fooc->dim = dim;
   fooc->label_len = identname_len;
   fooc->daguid_fooc = uid;
   fooc->label = identname;

   return fooc;
}

void fooc_label_free(FoocLabel *fooc)
{
   free(fooc);
}


