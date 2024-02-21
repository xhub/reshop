#ifndef MATHPRGM_DATA_H
#define MATHPRGM_DATA_H

/**
 *  @file mathprgm_data.h
 *
 *  @brief data to describe a mathematical programm 
 */

#include <stdbool.h>

struct emptree_node;

struct mp_parents {
   unsigned nb;
   unsigned max;
   struct emptree_node *list;
};

typedef enum {
   MpTypeUndef = 0,
   MpTypeOpt,
   MpTypeVi,
   /* TODO: All these types could be added, see GITLAB #83*/
//   MpTypeMcp,
//   MpTypeQvi,
//   MpTypeMpcc,
//   MpTypeMpec,
   MpTypeCcflib,
   MpTypeLast = MpTypeCcflib,
} MpType;

#endif
