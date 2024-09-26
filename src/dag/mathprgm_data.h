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
   MpTypeOpt = 1,
   MpTypeVi = 2,
   /* TODO: All these types could be added, see GITLAB #83*/
//   MpTypeMcp,
//   MpTypeQvi,
//   MpTypeMpcc,
//   MpTypeMpec,
   MpTypeCcflib = 3,
   MpTypeDual = 4,
   MpTypeFooc = 5,
   MpTypeLast = MpTypeFooc,
} MpType;

typedef enum {
   FenchelScheme = 0,
   EpiScheme = 1,
   DualOperatorSchemeLast = EpiScheme,
} DualOperatorScheme;

typedef enum {
   SubDagDomain = 0,
   NodeDomain = 1,
   DualOperatorDomainLast = NodeDomain,
} DualOperatorDomain;

typedef struct {
   DualOperatorScheme scheme;
   DualOperatorDomain domain;
} DualOperatorData;

#endif
