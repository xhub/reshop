#ifndef MATHPRGM_DATA_H
#define MATHPRGM_DATA_H

/**
 *  @file mathprgm_data.h
 *
 *  @brief data to describe a mathematical programm 
 */

#include <stdbool.h>

/** MathPrgm type */
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

/** MathPrgm status */
typedef enum {
   MpStatusUnset                  = 0x0,  /**< No status set                   */
   MpFinalized                    = 0x1,  /**< MP has been finalized           */
   MpIsHidden                     = 0x2,  /**< Hidden MP                       */
   MpIsHidableAsDual              = 0x4,  /**< Hiddable MP: is used as dual    */
   MpIsHidableAsKkt               = 0x8,  /**< Hiddable MP: is used as kkt     */
   MpIsHidableAsInstance          = 0x10, /**< Hiddable MP: is used as kkt     */
   MpIsHidableAsSmooth            = 0x20, /**< Hiddable MP: smoothing operator */
   MpCcflibNeedsFullInstantiation = 0x40,
   MpDelayedFinalization          = 0x80, /**< MP could not be finalized yet   */
} MpStatus;

#define MpIsHidable  (MpIsHidableAsDual | MpIsHidableAsKkt | MpIsHidableAsInstance)

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

typedef struct dual_operator_data {
   DualOperatorScheme scheme;
   DualOperatorDomain domain;
} DualOperatorData;

#endif
