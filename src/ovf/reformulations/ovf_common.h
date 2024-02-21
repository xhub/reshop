#ifndef OVF_COMMON_H
#define OVF_COMMON_H

#include <stdbool.h>

#include "cones.h"
#include "container.h"
#include "mathprgm_data.h"
#include "ovfinfo.h"

/** @file ovf_common.h
 *
 *  @brief common part for OVF reformulation
 */

/** Some property of OVF*/
struct ovf_ppty {
   bool quad;  /**< true if the OVF is quadratic */
   RhpSense sense;   /**< true if the OVF is in SUP form */
};

/** Quick access to some OVF data */
struct ovf_basic_data {
   RhpSense sense;
   rhp_idx idx;
   const char *name;
};

int ovf_equil_init(Model *mdl, struct ovf_basic_data *ovf_dat,
                    MathPrgm **mp_ovf) NONNULL;

int ovf_compat_types(const char *ovf_name, const char *ovf_varname, RhpSense mp_sense,
                     bool ovf_sup) NONNULL;

int ovf_replace_var(Model *mdl, rhp_idx ovf_vidx, void **jacptr,
                    double *jacval, rhp_idx *ei, unsigned extra_vars) NONNULL;

int ovf_get_mp_and_sense(const Model *mdl, rhp_idx vi_ovf,
                         MathPrgm **mp, RhpSense *sense) NONNULL;

int ovf_process_indices(Model *mdl, Avar *args, rhp_idx *eis) NONNULL;

#define NEWNAME(VAR, OVFVAR_NAME, OVFVAR_NAME_LEN, SUFFIX) \
      MALLOC_EXIT(VAR, char, 1 + OVFVAR_NAME_LEN + strlen(SUFFIX)); \
      strcpy(VAR, OVFVAR_NAME); \
      strcat(VAR, SUFFIX);

#define EMPMAT_GET_CSR_SIZE(M, i, size) \
if (M.ppty & EMPMAT_EYE) { size += 1; } else { \
   size += M.csr->p[i+1] - M.csr->p[i]; }

#define COPY_VALS(M, i, vals, indx, offset, size,  offset_var) { \
   if (M.ppty & EMPMAT_EYE) { if (!(M.ppty & EMPMAT_BLOCK)) { if (M.csr->nnzmax == 0) { vals[offset] = 1.; } else {\
   vals[offset] = M.csr->x[0]; } indx[offset] = i + offset_var; assert(size == 1 && M.csr->nnzmax == 1); } \
   else { unsigned block = M.block->number-1; for (unsigned kk = 0; kk < M.block->number-1; ++kk) { if (i < M.block->row_starts[kk+1]) block = kk; } \
   if (M.block->blocks[block]->nnzmax == 0) { vals[offset] = 1.; } else { vals[offset] = M.block->blocks[block]->x[0]; } indx[offset] = i + offset_var; } \
   } else { memcpy(&vals[offset], &M.csr->x[M.csr->p[i]], size*sizeof(double)); \
   for (RHP_INT j = M.csr->p[i], k = 0; j < M.csr->p[i+1]; ++j, ++k) { indx[offset+k] = M.csr->i[j] + offset_var; } } }

#define COPY_VALS_NEG(M, i, vals, indx, offset, size,  offset_var) { \
   if (M.ppty & EMPMAT_EYE) { if (!(M.ppty & EMPMAT_BLOCK)) { if (M.csr->nnzmax == 0) { vals[offset] = -1.; } else {\
   vals[offset] = -M.csr->x[0]; } indx[offset] = i + offset_var; assert(size == 1 && M.csr->nnzmax == 1); } \
   else { unsigned block = M.block->number-1; for (unsigned kk = 0; kk < M.block->number-1; ++kk) { if (i < M.block->row_starts[kk+1]) block = kk; } \
   if (M.block->blocks[block]->nnzmax == 0) { vals[offset] = -1.; } else { vals[offset] = -M.block->blocks[block]->x[0]; } indx[offset] = i + offset_var; } \
   } else { for (unsigned i = 0; i < size; ++i) { vals[offset] = -M.csr->x[M.csr->p[i]]; }; \
   for (RHP_INT j = M.csr->p[i], k = 0; j < M.csr->p[i+1]; ++j, ++k) { indx[offset+k] = M.csr->i[j] + offset_var; } } }

typedef struct ovf_ops {
   int (*add_k)(union ovf_ops_data ovfd, Model *mdl, Equ *e,
                Avar *y, unsigned n_args);
   int (*get_args)(union ovf_ops_data ovfd, Avar **v);
   int (*get_mappings)(union ovf_ops_data ovfd, rhp_idx **eis);
   int (*get_coeffs)(union ovf_ops_data ovfd, double **coeffs);
   int (*get_cone)(union ovf_ops_data ovfd, unsigned idx, enum cone *cone, void **cone_data);
   int (*get_cone_nonbox)(union ovf_ops_data ovfd, unsigned idx, enum cone *cone,
                   void **cone_data);
   int (*get_D)(union ovf_ops_data ovfd, SpMat *D, SpMat *J);
   int (*get_lin_transformation)(union ovf_ops_data ovfd,  SpMat *B, double **b);
   int (*get_M)(union ovf_ops_data ovfd, SpMat *M);
   const char* (*get_name)(union ovf_ops_data ovfd);
   rhp_idx (*get_ovf_vidx)(union ovf_ops_data ovfd);
   int (*get_set)(union ovf_ops_data ovfd, SpMat *At, double** b, bool trans);
   int (*get_set_nonbox)(union ovf_ops_data ovfd, SpMat *A, double** b, bool trans);
   int (*get_set_0)(union ovf_ops_data ovfd, SpMat *At, double** b, double ** u_shift);
   int (*create_uvar)(union ovf_ops_data ovfd, Container *ctr,
                   char *name, Avar *uvar);
   double (*get_var_lb)(union ovf_ops_data ovfd, size_t vidx);
   double (*get_var_ub)(union ovf_ops_data ovfd, size_t vidx);
   void   (*get_ppty)(union ovf_ops_data ovfd, struct ovf_ppty *ovf_ppty);
   size_t (*size_u)(union ovf_ops_data ovfd, size_t n_args);
   void (*trimmem)(union ovf_ops_data ovfd);
} OvfOps;

extern const OvfOps ovfdef_ops;
extern const OvfOps ccflib_ops;

#endif /* OVF_COMMON_H */
