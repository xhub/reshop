#ifndef EMPDAG_UID_H
#define EMPDAG_UID_H


#include <assert.h>
#include <limits.h>
#include <stdbool.h>

#include "empdag_data.h"

#define EMPDAG_IDMAX (UINT_MAX >> 2) - 1
#define EMPDAG_UID_NONE UINT_MAX

/** @file empdag_uid.h */

/*******
 * EMPDAG uid documentation
 * The type of uid is unsigned and the idea is to store  metadata in the "lower"
 * parts, namely the first bit indicates the edge type and the second one the
 * type of child node.
 * 
 * Finally, the special value EMPDAG_UID_NONE is akin to "none": the uid refers
 * to no concrete node.
********/

/** @brief EMPDAG uid special values */
typedef enum {
  EmpNodeMP    = 0,    /**< Index is for a MP node              */
  EmpNodeNash  = 2,    /**< Index is for a Nash node            */
} EmpNodeType;

typedef enum {
  EmpArcMask   = 1,    /**< Arc mask                            */
  EmpNodeMask  = 2,    /**< Node type mask                      */
} EmpUidMask;

/** EmpDag MP2MP arc type */
typedef enum {
  EmpArcCtrl  = 0,    /**< Arc is a control one               */
  EmpArcVF    = 1,    /**< Arc is a value function (VF) one   */
} EmpArcType;


static inline bool valid_uid(daguid_t uid) { return uid != EMPDAG_UID_NONE; }

const char *daguid_type2str(unsigned uid);

static inline daguid_t nashid2uid(nashid_t id)
{
  assert(id <= EMPDAG_IDMAX);

  daguid_t uid = id << 2 | EmpNodeNash;
  return uid;
}

static inline daguid_t mpid2uid(mpid_t id)
{
  assert(id <= EMPDAG_IDMAX);

  daguid_t uid = id << 2 | EmpNodeMP;
  return uid;
}

static inline daguid_t rarcVFuid(daguid_t uid)
{
   return uid | EmpArcVF;
}

static inline daguid_t rarcCTRLuid(daguid_t uid)
{
   return uid | EmpArcCtrl;
}

static inline unsigned uid2id(daguid_t uid)
{
  return uid >> 2;
}

static inline bool uidisMP(daguid_t uid)
{
  return (uid & EmpNodeMask) == EmpNodeMP;
}

static inline bool uidisNash(daguid_t uid)
{
  return (uid & EmpNodeMask) == EmpNodeNash;
}

static inline bool rarcTypeVF(daguid_t uid)
{
  return (uid & EmpArcMask) == EmpArcVF; 
}

static inline bool rarcTypeCtrl(daguid_t uid)
{
  return (uid & EmpArcMask) == EmpArcCtrl; 
}

const char * rarclinktype2str(daguid_t uid);

#endif // !EMPDAG_UID_H

