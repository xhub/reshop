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
LinkLabel * linklabels_dupaslabel(const LinkLabels * link_src) NONNULL;
int linklabels_add(LinkLabels * link, int *uels, double coeff, rhp_idx vi) NONNULL_AT(1);

void linklabel_free(LinkLabel *link);
MALLOC_ATTR(linklabel_free, 1) NONNULL
LinkLabel* linklabel_new(const char *label, unsigned label_len, uint8_t dim);

void dual_label_free(DualLabel *dual);
MALLOC_ATTR(dual_label_free, 1) NONNULL
DualLabel* dual_label_new(const char *label, unsigned label_len, uint8_t dim, mpid_t mpid_dual);

#endif
