#ifndef CTR_RHP_ADD_VARS_H
#define CTR_RHP_ADD_VARS_H

#include "rhp_fwd.h"

int rctr_add_free_vars(Container *ctr, unsigned nb, Avar *vout);
int rctr_add_box_vars(Container *ctr, unsigned nb, Avar *vout,
                       double *lbs, double *ubs);
int rctr_add_box_vars_id(Container *ctr, unsigned nb,
                          Avar *vout, double lb, double ub);
int rctr_add_neg_vars(Container *ctr, unsigned nb, Avar *vout);
int rctr_add_pos_vars(Container *ctr, unsigned nb, Avar *vout);

#endif // CTR_RHP_ADD_VARS_H


