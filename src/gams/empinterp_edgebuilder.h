#ifndef EMPPARSER_EDGE_BUILDER_H
#define EMPPARSER_EDGE_BUILDER_H

#include <stdint.h>

#include "compat.h"
#include "empdag_data.h"

typedef struct dag_labels DagLabels;
typedef struct dag_label DagLabel;
typedef struct dual_label DualLabel;
typedef struct fooc_label FoocLabel;

void dag_labels_free(DagLabels *dagl);
DagLabels * dag_labels_new(const char *basename, unsigned basename_len,
                           uint8_t dim, uint8_t num_vars, unsigned size) MALLOC_ATTR(dag_labels_free, 1);
DagLabels * dag_labels_dup(const DagLabels * dagl_src) MALLOC_ATTR(dag_labels_free);
DagLabel * dag_labels_dupaslabel(const DagLabels * dagl_src) NONNULL;
int dag_labels_add(DagLabels * dagl, int *uels) NONNULL;

void dag_label_free(DagLabel *dagl);
MALLOC_ATTR(dag_label_free, 1) NONNULL
DagLabel* dag_label_new(const char *identname, unsigned identname_len, uint8_t dim);

void dual_label_free(DualLabel *dual);
MALLOC_ATTR(dual_label_free, 1) NONNULL
DualLabel* dual_label_new(const char *identname, unsigned identname_len, uint8_t dim, mpid_t mpid_dual);

void fooc_label_free(FoocLabel *dual);
MALLOC_ATTR(fooc_label_free, 1) NONNULL
FoocLabel* fooc_label_new(const char *identname, unsigned identname_len, uint8_t dim, daguid_t uid);

#endif
