#ifndef RHP_LA_H
#define RHP_LA_H

#include <assert.h>
#include <limits.h>
#include <stdbool.h>
#include <stddef.h>
#include <stdint.h>

#include "compat.h"
#include "macros.h"

#define RHP_INT unsigned
#define RHP_INTMAX UINT_MAX

#define RHP_CS 0
#define RHP_TRIPLET 1

typedef struct sp_matrix {
   RHP_INT n;
   RHP_INT m;
   RHP_INT nnz;
   RHP_INT nnzmax;
   RHP_INT *i;
   RHP_INT *p;
   double *x;
} SparseMatrix;

struct block_spmat {
   unsigned number;              /**< number of blocks */
   RHP_INT n;
   RHP_INT m;
   unsigned *row_starts;
   unsigned *col_starts;
   SparseMatrix **blocks;
};

/*  TODO(xhub) replace ppty by booleans */

typedef struct rhp_spmat {
   SparseMatrix* csr;
   SparseMatrix* csc;
   SparseMatrix* triplet;
   struct block_spmat* block;
   uintptr_t ppty;
} SpMat;

enum EMPMAT_PPTY {
   EMPMAT_NUL = 0,
   EMPMAT_CSR = 1,
   EMPMAT_CSC = 2,
   EMPMAT_COO = 4,
   EMPMAT_SYM = 8,
   EMPMAT_EYE = 16,
   EMPMAT_BLOCK = 32,
};

static inline unsigned rhpmat_get_type(const SpMat* m) { return m->ppty & 0x7; }

static inline void rhpmat_set_csr(SpMat* m) { m->ppty |= EMPMAT_CSR; }

static inline void rhpmat_set_csc(SpMat* m) { m->ppty |= EMPMAT_CSC; }

static inline void rhpmat_set_coo(SpMat* m) { m->ppty |= EMPMAT_COO; }

static inline void rhpmat_set_block(SpMat* m) { m->ppty |= EMPMAT_BLOCK; }

static inline void rhpmat_set_sym(SpMat* m) { m->ppty |= EMPMAT_SYM; }

static inline void rhpmat_set_eye(SpMat* m) { m->ppty |= EMPMAT_EYE; }

static inline void rhpmat_reset(SpMat* m) { m->ppty = EMPMAT_NUL; }

static inline void rhpmat_null(SpMat* m)
{
   m->csr = NULL; m->csc = NULL; m->triplet = NULL; m->block = NULL; m->ppty = 0;
}

static inline bool rhpmat_nonnull(SpMat* m)
{
   return (m->csr || m->csc || m->triplet || m->block);
}

/* TODO: finish support */
static inline RHP_INT rhpmat_nnz(const SpMat* m) {

   if (m->ppty & EMPMAT_EYE) {
      if (m->csr) return MAX3(m->csr->m, m->csr->n, m->csr->nnz);
      if (m->csc) return MAX3(m->csc->m, m->csc->n, m->csc->nnz);
      if (m->triplet) return MAX3(m->triplet->m, m->triplet->n, m->triplet->nnz);
      if (m->block) {
         RHP_INT nnz = 0;
         for (unsigned i = 0, len = m->block->number; i < len; ++i) {
            const SparseMatrix *mm = m->block->blocks[i];
            nnz += MAX3(mm->n, mm->m, mm->nnz);
         }
         return nnz;
      }

      assert(false && "Unsupported case");
      return RHP_INTMAX;
   }

   if (m->ppty & EMPMAT_CSR) {
      assert(m->csr);
      return m->csr->nnz;
   }
   if (m->ppty & EMPMAT_CSC) {
      assert(m->csc);
      return m->csc->nnz;
   }
   if (m->ppty & EMPMAT_COO) {
      assert(m->csc);
      return m->triplet->nnz;
   }
   if (m->ppty & EMPMAT_BLOCK) {
      RHP_INT nnz = 0;
      for (unsigned i = 0, len = m->block->number; i < len; ++i) {
         nnz += m->block->blocks[i]->nnz;
      }
      return nnz;
   }

   assert(false && "Unsupported case");
   return RHP_INTMAX;
}

void rhp_spfree(struct sp_matrix *m);
struct sp_matrix* rhp_spalloc(RHP_INT m, RHP_INT n, RHP_INT nnzmax, unsigned char type) MALLOC_ATTR(rhp_spfree,1) CHECK_RESULT;

int rhpmat_axpy(const SpMat *A, const double *x, double *y) NONNULL;
int rhpmat_atxpy(const SpMat *A, const double *x, double *y) NONNULL;
void rhpmat_copy_row_neg(SpMat* M, unsigned i, double *vals,
                         int *indx, unsigned *offset, unsigned offset_var);
double rhpmat_evalquad(const SpMat *m, const double *x) NONNULL;

bool rhpmat_is_square(SpMat* m) NONNULL;
int rhpmat_get_size(const SpMat* mat, unsigned *n, unsigned *m)
   ACCESS_ATTR(write_only, 2) ACCESS_ATTR(write_only, 3) NONNULL;

int rhpmat_row(const SpMat* m, unsigned i, unsigned *single_idx, double *single_val,
               unsigned *col_idx_len, unsigned **col_idx, double **vals) NONNULL;

int rhpmat_col(SpMat* m, unsigned i, unsigned* single_idx,
               double* single_val, unsigned* col_idx_len,
               unsigned** col_idx, double** vals);
#define rhpmat_free rhp_mat_free
#define rhpmat_triplet rhp_mat_triplet

#endif /* RHP_LA_H  */
