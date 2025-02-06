#ifndef EMPDAG_DATA_H
#define EMPDAG_DATA_H

#include <assert.h>
#include <limits.h>
#include <stdbool.h>
#include <stdint.h>

#include "rhp_defines.h"
#include "rhp_fwd.h"

/** @file empdag_data.h */

typedef u32 daguid_t;
typedef u32 dagid_t;
typedef u32 mpid_t;
typedef u32 nashid_t;

typedef UIntArray DagUidArray;
typedef UIntArray MpIdArray;

/* use macro since C requires enum to fit into an int ... */
#define MpId_NA  UINT32_MAX
#define MpeId_NA UINT32_MAX

#define MpId_MaxRegular        (MpId_NA >> 1)

#define MpId_SpecialValueMask  (UINT32_MAX & (~MpId_MaxRegular))
#define MpId_SharedEquMask     (MpId_SpecialValueMask | (MpId_SpecialValueMask >> 1))
#define MpId_OvfDataMask       (MpId_SpecialValueMask | (MpId_SpecialValueMask >> 3))
#define MpId_SharedVarMask     (MpId_SpecialValueMask | (MpId_SpecialValueMask >> 2))

#define MpId_SpecialValueTag (MpId_SharedEquMask | MpId_SharedVarMask | MpId_OvfDataMask)

#define MpId_SpecialValuePayload (MpId_NA & ~(MpId_SpecialValueMask | MpId_SpecialValueTag))

/** Stage of the EMPDAG */
typedef enum {
   EmpDagStage_Unset,
   EmpDagStage_Model,
   EmpDagStage_Transformed,
   EmpDagStage_Collapsed,
   EmpDagStage_Subset,
} EmpDagStage;

/** Type of VF arc  */
typedef enum {
   ArcVFUnset,          /**< Unset arc VF type  */
   ArcVFBasic,          /**< child VF appears in one equation with basic weight */
   ArcVFMultipleBasic,  /**< child VF appears in equations with basic weight */
   ArcVFLequ,           /**< child VF appears in one equation with linear sum weight */
   ArcVFMultipleLequ,   /**< child VF appears in equations with linear sum weight */
   ArcVFEqu,            /**< child VF appears in one equation with general weight */
   ArcVFMultipleEqu,    /**< child VF appears in equations with general weight */
   ArcVFLast = ArcVFMultipleEqu,
} ArcVFType;

typedef struct {
   u32 num_min;
   u32 num_max;
   u32 num_feas;
   u32 num_vi;
   u32 num_nash;
   u32 num_active;
} EmpDagNodeStats;

typedef struct {
   u32 num_ctrl;
   u32 num_equil;
   u32 num_vf;
} EmpDagEdgeStats;

static inline bool valid_mpid(mpid_t mpid)
{
   return mpid < MpId_NA;
}

static inline bool mpid_regularmp(mpid_t mpid)
{
   return mpid <= MpId_MaxRegular;
}

static inline mpid_t mpid2grpid(mpid_t mpid)
{
   assert(!mpid_regularmp(mpid) && 
          (((mpid & MpId_SpecialValueTag) == MpId_SharedVarMask) ||
           ((mpid & MpId_SpecialValueTag) == MpId_SharedEquMask)));

   return mpid & MpId_SpecialValuePayload;
}

static inline mpid_t mpid2ovfid(mpid_t mpid)
{
   assert(!mpid_regularmp(mpid) && 
          ((mpid & MpId_SpecialValueTag) == MpId_OvfDataMask));

   return mpid & MpId_SpecialValuePayload;
}

const char *mpid_getname(const Model *mdl, mpid_t mpid) NONNULL;
const char *mpid_specialvalue(mpid_t mpid);

RESHOP_STATIC_ASSERT(MpId_SpecialValueMask > MpId_MaxRegular, "Error in mpid_t")
RESHOP_STATIC_ASSERT(MpId_SharedEquMask > MpId_MaxRegular, "Error in mpid_t")
RESHOP_STATIC_ASSERT(MpId_SharedVarMask > MpId_MaxRegular, "Error in mpid_t")
RESHOP_STATIC_ASSERT(MpId_OvfDataMask > MpId_MaxRegular, "Error in mpid_t")

#endif
