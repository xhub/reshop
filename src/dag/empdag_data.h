#ifndef EMPDAG_DATA_H
#define EMPDAG_DATA_H

#include <assert.h>
#include <limits.h>
#include <stdbool.h>

#include "rhp_fwd.h"

typedef unsigned daguid_t;
typedef unsigned dagid_t;
typedef unsigned mpid_t;
typedef unsigned nashid_t;

typedef UIntArray DagUidArray;
typedef UIntArray MpIdArray;

/* use macro since C requires enum to fit into an int ... */
#define MpId_NA UINT_MAX
#define MpeId_NA UINT_MAX

#define MpId_MaxRegular (MpId_NA >> 1)

#define MpId_SpecialValueMask  (UINT_MAX & (~MpId_MaxRegular))
#define MpId_SharedEquMask     (MpId_SpecialValueMask | (MpId_SpecialValueMask >> 1))
#define MpId_OvfDataMask       (MpId_SpecialValueMask | (MpId_SpecialValueMask >> 3))
#define MpId_SharedVarMask     (MpId_SpecialValueMask | (MpId_SpecialValueMask >> 2))

#define MpId_SpecialValueTag (MpId_SharedEquMask | MpId_SharedVarMask | MpId_OvfDataMask)

#define MpId_SpecialValuePayload (MpId_NA & ~(MpId_SpecialValueMask | MpId_SpecialValueTag))

static inline bool valid_mpid(mpid_t mpid)
{
   return mpid < MpId_NA;
}

static inline bool mpid_regularmp(mpid_t mpid)
{
   return mpid <= MpId_MaxRegular;
}

const char *mpid_specialvalue(mpid_t mpid);

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

RESHOP_STATIC_ASSERT(MpId_SpecialValueMask > MpId_MaxRegular, "Error in mpid_t")
RESHOP_STATIC_ASSERT(MpId_SharedEquMask > MpId_MaxRegular, "Error in mpid_t")
RESHOP_STATIC_ASSERT(MpId_SharedVarMask > MpId_MaxRegular, "Error in mpid_t")
RESHOP_STATIC_ASSERT(MpId_OvfDataMask > MpId_MaxRegular, "Error in mpid_t")

#endif
