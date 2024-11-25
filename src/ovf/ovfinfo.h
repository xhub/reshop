#ifndef OVFINFO_H
#define OVFINFO_H

#include <stdbool.h>
#include <stddef.h>

#include "empdag_data.h"
#include "rhp_fwd.h"
#include "var.h"

/** @file ovfinfo.h
 *
 *  @brief Definitions for Optimal Value Functions (OVF)
 */

struct ovf_parameters;
typedef struct ovf_param_list OvfParamList;

typedef struct ovf_param_def_list {
    const struct ovf_param_def* const * p;
    const unsigned *s;
} OvfParamDefList;

extern const unsigned ovf_numbers;
extern const char* const ovf_names[];
extern const char* const ovf_synonyms[][2];
extern const char* const ovf_always_compatible[];

/** @brief type of arguments  */
typedef enum ovf_argtype {
   ARG_TYPE_UNSET = 0,
   ARG_TYPE_SCALAR,
   ARG_TYPE_VEC,
   ARG_TYPE_MAT,
   ARG_TYPE_VAR,
   ARG_TYPE_EQU,
   ARG_TYPE_NESTED,          /**< Special value for VM */
   __ARG_TYPE_SIZE
} OvfArgType;

typedef union {
   double val;
   double* vec;
   struct emp_mat* mat;
   unsigned gidx;
} OvfParamPayload;

const char *ovf_argtype_str(OvfArgType type);


typedef enum OVF_TYPE {
   OvfType_OV,
   OvfType_Ovf,
   OvfType_Ccflib,
   OvfType_Ccflib_Dual,
} OvfType;

typedef enum {
   OvfNoStatus  = 0,
   OvfChecked   = 1,
   OvfFinalized = 2
} OvfStatus;

typedef struct rhp_ovfdef {
   unsigned idx;                /**< Index of the generator for this OVF      */
   rhp_idx vi_ovf;              /**< index of the OVF variable                */

   Avar *args;                  /**< variable arguments                       */
   rhp_idx *eis;                /**< Equations which are arguments to the CCF */
   double *coeffs;              /**< Coefficients of the OVF variable arg     */
   unsigned num_empdag_children;/**< Number of EMPDAG children               */

   const OvfGenOps *generator;  /**< generator for this OVF */
   const char *name;            /**< name of this OVF */
   OvfDef *next;                /**< next OVF */
   OvfParamList *params;         /**< set of parameters associated with this OVF */
   unsigned char reformulation; /**< Reformulation scheme for this OVF */
   bool sense;                  /**< true if the OVF is in sup form (inf form otherwise) */
   OvfStatus status;            /**< status of the OVF def                               */
   unsigned refcnt;             /**< Reference counter                        */
} OvfDef;


typedef struct ovfinfo {
//   struct ovf_def * ov_def;
   OvfDef *ovf_def;
   unsigned num_ovf;
   unsigned refcnt;
} OvfInfo;

typedef struct {
   MathPrgm *mp_primal;
   mpid_t mpid_dual;
} CcflibPrimalDualData;

typedef union ovf_ops_data {
   OvfDef *ovf;
   CcflibPrimalDualData *ccfdat;
} OvfOpsData;

void ovfdef_free(OvfDef* def);
OvfDef* ovfdef_new_ovfinfo(struct ovfinfo *ovfinfo, unsigned ovf_idx) MALLOC_ATTR(ovfdef_free,1);
OvfDef* ovfdef_new(unsigned ovf_idx) MALLOC_ATTR(ovfdef_free,1);
OvfDef* ovfdef_borrow(OvfDef* ovfdef) NONNULL;

int ovfinfo_alloc(EmpInfo *empinfo);
void ovfinfo_dealloc(EmpInfo *empinfo);
struct ovfinfo *ovfinfo_borrow(struct ovfinfo *ovfinfo) NONNULL;

int ovf_addbyname(EmpInfo *empinfo, const char *name, OvfDef **ovfdef) NONNULL;
int ovf_params_sync(OvfDef * restrict ovf, OvfParamList * restrict params) NONNULL;
unsigned ovf_findbyname(const char *name) NONNULL;
unsigned ovf_findbytoken(const char *start, unsigned len) NONNULL;
int ovf_finalize(Model *mdl, OvfDef *ovf_def) NONNULL;
int ccflib_finalize(Model *mdl, OvfDef *ovf_def, MathPrgm *mp) NONNULL;
int ovf_remove_mappings(Model *mdl, OvfDef *ovf_def) NONNULL;
int ovf_forcefinalize(Model *mdl, OvfDef *ovf_def) NONNULL;
int ovf_check(OvfDef *ovf_def) NONNULL;

const char* ovf_getname(const OvfDef* ovf);

void ovf_print(const struct ovfinfo *ovf_info, const Model *mdl) NONNULL;
void ovf_def_print(const OvfDef *ovf, unsigned mode, const Model *mdl) NONNULL;

void ovf_print_usage(void);

const OvfParamDefList* ovf_getparamdefs(unsigned ovf_idx);

static inline unsigned ovf_argsize(const OvfDef *ovf)
{
   return ovf->num_empdag_children + avar_size(ovf->args);
}


#endif /* OVFINFO_H  */
