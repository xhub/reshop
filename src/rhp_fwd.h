#ifndef EMP_FWD_H
#define EMP_FWD_H

#include "compat.h"

typedef struct rhp_ctr_filter_ops Fops;
typedef struct container Container;
typedef struct rhp_mdl Model;
typedef struct empinfo EmpInfo;
typedef struct empdag EmpDag;
typedef struct equ Equ;
typedef struct var Var;
typedef struct lequ Lequ;
typedef struct rhp_avar Avar;
typedef struct rhp_aequ Aequ;
typedef struct rhp_nltree NlTree;
typedef struct rhp_nlnode NlNode;
typedef struct nltree_pool NlPool;

typedef struct rhp_mathprgm MathPrgm;
typedef struct rhp_nash_equilibrium Nash;
typedef struct rhp_empdag_arcVF Varc;

typedef struct rhp_int_data   IntArray;
typedef struct rhp_uint_data  UIntArray;
typedef struct rhp_obj_data   ObjArray;
#define rhp_idx_data rhp_int_data
typedef struct rhp_idx_data  IdxArray;


typedef struct rhp_spmat SpMat;

typedef struct rhp_ovfdef OvfDef;
typedef struct ovf_ops OvfOps;
typedef struct ovf_genops OvfGenOps;

typedef struct mp_descr MpDescr;

typedef struct M_ArenaLink M_ArenaLink;

struct option;

#endif
