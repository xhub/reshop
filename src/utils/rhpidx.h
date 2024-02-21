#ifndef RHP_IDX_H
#define RHP_IDX_H

#include <limits.h>
#include <stdbool.h>

#ifndef rhp_idx
#include "compat.h"
#endif

/* TODO(xhub) change for real value */
#define MAX_VAL_IDX INT_MAX

/** Special values for indices */
enum rhp_special_idx {
   IdxInvalid    = MAX_VAL_IDX,
   IdxNA         = MAX_VAL_IDX-1,
   IdxNotFound   = MAX_VAL_IDX-2,
   IdxDeleted    = MAX_VAL_IDX-3,
   IdxOutOfRange = MAX_VAL_IDX-4,
   IdxError      = MAX_VAL_IDX-5,    /**< An error occurred                  */
   IdxDuplicate  = MAX_VAL_IDX-6,    /**< A duplicate value was found        */
   IdxNone       = MAX_VAL_IDX-7,    /**< No value passed                    */
   IdxCcflib     = MAX_VAL_IDX-8,     /**< Special value for CCFLIB MP data  */
   IdxEmpDagChildNotFound = MAX_VAL_IDX-11, /**< Child not found in empdagc  */
   IdxMaxValid   = MAX_VAL_IDX-100,  /**< Biggest valid indx                 */
};

static inline bool valid_vi(rhp_idx vi) {
   return (vi >= 0 && vi <= IdxMaxValid);
}

static inline bool valid_ei(rhp_idx ei) {
   return (ei >= 0 && ei <= IdxMaxValid);
}

const char * badidx_str(rhp_idx idx);

static inline bool valid_idx(unsigned idx) {
   return idx <= IdxMaxValid;
}

#endif
