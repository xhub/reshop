#ifndef OVF_PARAMETERS_H
#define OVF_PARAMETERS_H

#include <stddef.h>
#include <stdbool.h>

#include "compat.h"
#include "ovfinfo.h"

struct ovf_param;


typedef struct ovf_param_def {
   const char *name;
   const char *help;
   bool mandatory;
   bool allow_data;
   bool allow_var;
   bool allow_fun;
   bool allow_scalar;
   bool allow_vector;
   bool allow_matrix;
   size_t (*get_vector_size)(size_t arg_size);
   int (*default_val)(struct ovf_param *p, size_t arg_size);
} OvfParamDef;


typedef struct ovf_param {
   const OvfParamDef *def;
   enum ovf_argtype type;
   unsigned size_vector;

   union {
      double val;
      double* vec;
      struct emp_mat* mat;
//      rhp_idx* var_list;
//      rhp_idx* eqn_list;
   };
} OvfParam;

typedef struct ovf_param_list {
    unsigned size;
    struct ovf_param p[];
} OvfParamList;

int ovf_fill_params(OvfParamList **params, size_t ovf_idx) NONNULL;
void ovf_param_print(const struct ovf_param *p, unsigned mode) NONNULL;
void ovf_param_dealloc(OvfParam *p);

size_t ovf_same_as_argsize(size_t argsize);

#endif /* OVF_PARAMETERS_H */
