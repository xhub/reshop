#ifndef EMPPARSER_EDGE_BUILDER_H
#define EMPPARSER_EDGE_BUILDER_H

#include <stdint.h>

#include "compat.h"
#include "empdag_common.h"
#include "empdag_data.h"
#include "empinterp_fwd.h"

void linklabels_free(LinkLabels *link);
LinkLabels * linklabels_new(LinkType type, const char *label, unsigned label_len,
                           uint8_t dim, uint8_t num_vars, unsigned size) MALLOC_ATTR(linklabels_free, 1);
LinkLabels * linklabels_dup(const LinkLabels * link_src) MALLOC_ATTR(linklabels_free);
LinkLabel * linklabels_dupaslabel(const LinkLabels * link_src, double coeff, rhp_idx vi) NONNULL;
int linklabels_add(LinkLabels * link, int *uels, double coeff, rhp_idx vi) NONNULL_AT(1);

void linklabel_free(LinkLabel *link);
MALLOC_ATTR(linklabel_free, 1) NONNULL
LinkLabel* linklabel_new(const char *label, unsigned label_len, uint8_t dim);

/* The underscore '_' is needed to avoid a name clash */
void dualslabel_free(DualsLabel *dual_labels);
MALLOC_ATTR(dualslabel_free, 1) NONNULL
DualsLabel* dualslabel_new(const char *label, unsigned label_len, uint8_t dim,
                           uint8_t num_vars, DualOperatorData *opdat);
int dualslabel_add(DualsLabel* dualslabel, int *uels, uint8_t nuels, mpid_t mpid_dual) NONNULL_AT(1);

MALLOC_ATTR(free, 1) NONNULL
DualLabel* dual_label_new(const char *label, unsigned label_len, uint8_t dim, mpid_t mpid_dual);

#endif
