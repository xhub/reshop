#include <assert.h>
#include <float.h>
#include <math.h>

#include "allocators.h"
#include "macros.h"
#include "printout.h"
#include "rhp_LA.h"
#include "rhp_LA_sparsetools.h"
#include "rhp_defines.h"
#include "status.h"
#include "lequ.h" /* for some debug info  */

/** @file rhp_LA.c */

/** @brief Free a matrix
 *
 *  @param m  the matrix to free
 */
void rhpmat_free(SpMat *m)
{
   if (m->csr) { rhpmat_spfree(m->csr); }
   if (m->csc) { rhpmat_spfree(m->csc); }
   if (m->triplet) { rhpmat_spfree(m->triplet); }
   if (m->block) {
      struct block_spmat *bmat = m->block;
      if (bmat->blocks) {
         for (size_t i = 0; i < bmat->number; ++i) {
            rhpmat_spfree(bmat->blocks[i]);
         }
      }
      FREE(bmat->blocks);
      FREE(bmat->row_starts);
      FREE(bmat->col_starts);
      FREE(m->block);
   }
}

/** @brief build a triplet matrix
 *
 *  @param n       the number of columns
 *  @param m       the number of rows
 *  @param nnz     the number of non-zero elements
 *  @param rowidx  the row indices
 *  @param colidx  the column indices
 *  @param data    the values
 *
 *  @return        the matrix 
 */
SpMat* rhpmat_triplet(unsigned n, unsigned m, unsigned nnz, int *rowidx, int *colidx, double *data)
{
   DPRINT("%s :: row:", __func__);
   DPRINTF(for (unsigned i = 0; i < nnz; ++i) { printout(PO_DEBUG, " %d", rowidx[i]); } );
   DPRINT("\n%s :: col:", __func__);
   DPRINTF(for (unsigned i = 0; i < nnz; ++i) { printout(PO_DEBUG, " %d", colidx[i]); } );
   DPRINT("\n%s :: vals:", __func__);
   DPRINTF(for (unsigned i = 0; i < nnz; ++i) { printout(PO_DEBUG, " %e", val[i]); } );
   DPRINT("%s", "\n"); /* This is ugly but we need to provide at least one VAR arg --xhub */
   SpMat* mat;
   CALLOC_NULL(mat, SpMat, 1);
   rhpmat_set_coo(mat);

   MALLOC_EXIT_NULL(mat->triplet, SparseMatrix, 1);

   SparseMatrix* csm = mat->triplet;

   csm->nnzmax = nnz;
   csm->m = m;
   csm->n = n;
   csm->nnz = nnz;

   if (nnz == 0) {
      csm->p = NULL;
      csm->i = NULL;
      csm->x = NULL;
      return mat;
   }

   MALLOC_EXIT_NULL(csm->p, RHP_INT, nnz);
   MALLOC_EXIT_NULL(csm->i, RHP_INT, nnz);
   MALLOC_EXIT_NULL(csm->x, double, nnz);

   /*  We don't use memcpy to allow a possible conversion here. sizeof(RHP_INT) may
    *  not be the same as sizeof(int)*/
   for (unsigned i = 0; i < nnz; ++i) {
      csm->p[i] = colidx[i];
      csm->i[i] = rowidx[i];
   }
   memcpy(csm->x, data, nnz * sizeof(double));

   return mat;

_exit:
   rhpmat_free(mat);
   FREE(mat);
   return NULL;
}

SparseMatrix* rhpmat_allocA(M_ArenaLink *arena, RHP_INT m, RHP_INT n, RHP_INT nnzmax, unsigned char type)
{
   SparseMatrix *mat = NULL;

   u64 lenp = type == RHP_TRIPLET ? 1+nnzmax : 1+n;
   RHP_INT *i = NULL, *p = NULL;
   double *x = NULL;

   void *mems[] = {mat, i, p, x};
   u64 sizes[] = {sizeof(SparseMatrix), sizeof(RHP_INT)*nnzmax, sizeof(RHP_INT)*lenp, sizeof(double)*nnzmax};

   RESHOP_STATIC_ASSERT(ARRAY_SIZE(mems) == ARRAY_SIZE(sizes), "");

   SN_CHECK_EXIT(arenaL_alloc_blocks(arena, ARRAY_SIZE(sizes), mems, sizes));

   assert(mat && i && p && x);

   mat->m = m;
   mat->n = n;
   mat->nnz = 0;
   mat->nnzmax = nnzmax;
   mat->i = i;
   mat->p = p;
   mat->x = x;

   return mat;

_exit:
   return NULL;
}

SparseMatrix* rhpmat_spalloc(RHP_INT m, RHP_INT n, RHP_INT nnzmax, unsigned char type)
{
   SparseMatrix *mat;
   CALLOC_NULL(mat, SparseMatrix, 1);

   mat->m = m;
   mat->n = n;
   mat->nnz = 0;
   mat->nnzmax = nnzmax;
   MALLOC_EXIT_NULL(mat->i, RHP_INT, nnzmax);
   MALLOC_EXIT_NULL(mat->p, RHP_INT, type == RHP_TRIPLET ? 1+nnzmax : 1+n);
   MALLOC_EXIT_NULL(mat->x, double, nnzmax);

   return mat;

_exit:
   FREE(mat->i);
   FREE(mat->p);
   FREE(mat->x);
   FREE(mat);
   return NULL;
}

/** @brief Free a sparse matrix
 *
 * @param m  the sparse matrix to free
 */
void rhpmat_spfree(SparseMatrix *m)
{
   if (!m) { return; }

   FREE(m->i);
   FREE(m->p);
   FREE(m->x);
   free(m);
}

void rhpmat_copy_row_neg(SpMat* M, unsigned i, double *vals,
                         int *indx, unsigned *offset, unsigned offset_var)
{
   unsigned loffset = *offset;
   if (M->ppty & EMPMAT_EYE) {
      if (!(M->ppty & EMPMAT_BLOCK)) {
         if (M->csr->nnzmax == 0) {
            vals[loffset] = -1.;
         } else if (fabs(M->csr->x[0]) > DBL_EPSILON) {
            vals[loffset] = -M->csr->x[0];
         } else {
            return;
         }

         assert(lequ_debug_quick_chk(i + offset_var, loffset, indx, vals));
         indx[loffset] = i + offset_var;
      } else {
         unsigned block = M->block->number-1;
         for (unsigned kk = 0; kk < M->block->number-1; ++kk) {
            if (i < M->block->row_starts[kk+1]) {
               block = kk;
            }
         }
         if (M->block->blocks[block]->nnzmax == 0) {
            vals[loffset] = -1.;
         } else if (fabs(M->block->blocks[block]->x[0]) > 0.) {
            vals[loffset] = -M->block->blocks[block]->x[0];
         } else {
            return;
         }
         assert(lequ_debug_quick_chk(i + offset_var, loffset, indx, vals));
         indx[loffset] = i + offset_var;
      }
      *offset = loffset+1;
   } else {
      SparseMatrix *spm = M->csr;
      assert(spm->p && spm->i && spm->x);
      for (RHP_INT j = spm->p[i], k = 0; j < spm->p[i+1]; ++j, ++k) {
         if (fabs(spm->x[j]) > DBL_EPSILON) {
            assert(lequ_debug_quick_chk(spm->i[j] + offset_var, loffset, indx, vals));
            indx[loffset] = spm->i[j] + offset_var;
            vals[loffset] = -spm->x[j];
            loffset++;
         }
      }
      *offset = loffset;
   }


}

/** @brief return the size of the matrix
 *
 *  @param     mat   the matrix
 *  @param[out] n    the number of columns
 *  @param[out] m    the number of rows
 *
 *  @return          the error code
 */
int rhpmat_get_size(const SpMat* mat, unsigned *n, unsigned *m)
{
   if (mat->ppty & EMPMAT_CSR) {
      *n = (unsigned)mat->csr->n;
      *m = (unsigned)mat->csr->m;
   } else if (mat->ppty & EMPMAT_CSC) {
      *n = (unsigned)mat->csc->n;
      *m = (unsigned)mat->csc->m;
   } else if (mat->ppty & EMPMAT_COO) {
      *n = (unsigned)mat->triplet->n;
      *m = (unsigned)mat->triplet->m;
   } else if (mat->ppty & EMPMAT_EYE) {
      if (!(mat->ppty & EMPMAT_BLOCK)) {
         *n = (unsigned)mat->csr->n;
         *m = (unsigned)mat->csr->m;
      } else {
         *n = (unsigned)mat->block->n;
         *m = (unsigned)mat->block->m;
      }
   } else {
     return Error_InvalidValue;
   }

   return OK;
}

#ifdef __ICL
#pragma optimize("", off)
#endif

/**
 * @brief General interface to get the row of a matrix 
 *
 * This function fill in the a vector corresponding to a row in a matrix
 * The signature is quite long, and some arguments are just input in case the
 * matrix is empty or has no data associated with it
 *
 * @param m                 the matrix
 * @param i                 the index of the row to get
 * @param[in]  single_idx   memory space for the index (no matrix data case)
 * @param[in]  single_val   memory space for the value (no matrix data case)
 * @param[out] col_idx_len  the length of the row vector
 * @param[out] col_idx      the column indices for the row vector
 * @param[out] vals         the entries for the row vector
 *
 * @return                  the error code
 */
int rhpmat_row_needs_update(const SpMat* m, unsigned i, unsigned* restrict single_idx,
               double* restrict single_val, unsigned* restrict col_idx_len,
               unsigned** restrict col_idx, double** restrict vals)
{
   SpMatColRowWorkingMem wrkmem = {.single_val = NAN, .single_idx = UINT_MAX};
   S_CHECK(rhpmat_row(m, i, &wrkmem, col_idx_len, col_idx, vals));
   if (*col_idx == &wrkmem.single_idx) {
      *single_idx = **col_idx;
      *single_val = **vals;

      *col_idx = single_idx;
      *vals = single_val;
   }

   return OK;
}

/**
 * @brief General interface to get the row of a matrix 
 *
 * This function fill in the a vector corresponding to a row in a matrix
 * The signature is quite long, and some arguments are just input in case the
 * matrix is empty or has no data associated with it
 *
 * @param m                the matrix
 * @param i                the index of the row to get
 * @param[in]  wrkmem      working memory space when the matrix contains no data
 * @param[out] row_len     the length of the row vector
 * @param[out] row_idxs    the indices for the row vector
 * @param[out] row_vals    the entries for the row vector
 *
 * @return                  the error code
 */
int rhpmat_row(const SpMat* m, unsigned i, SpMatColRowWorkingMem *wrkmem,
               unsigned* restrict row_len, unsigned** restrict row_idxs,
               double** restrict row_vals)
{
#if defined(__GNUC__) && !defined(__clang__) && (__GNUC__ >= 14)
#pragma GCC diagnostic ignored "-Wdangling-pointer"
#endif
   if (m->ppty) {
      if (!(m->ppty & EMPMAT_CSR) || m->ppty & EMPMAT_BLOCK) {
         error("%s :: only CSR matrices are supported\n", __func__);
         return Error_NotImplemented;
      }

      if (m->ppty & EMPMAT_EYE) {
         assert(m->csr);
         *row_idxs = &wrkmem->single_idx;
         wrkmem->single_idx = i;
         *row_len = 1;
         if (m->csr->nnzmax == 1) {
            *row_vals = m->csr->x;
         } else {
            *row_vals = &wrkmem->single_val;
            wrkmem->single_val = 1.;
         }

      } else {
         RHP_INT row_idx = m->csr->p[i];
         *row_len = m->csr->p[i+1] - row_idx;
         *row_idxs = &m->csr->i[row_idx];
         *row_vals = &m->csr->x[row_idx];
      }
   } else { /* No m->given -> this is taken as the identity */
      /*  TODO(xhub) this is a bit specific, see if we can move this out */
      *row_idxs = &wrkmem->single_idx;
      *row_vals = &wrkmem->single_val;
      wrkmem->single_idx = i;
      wrkmem->single_val = 1.;
      *row_len = 1;
   }

   return OK;
#if defined(__GNUC__) && !defined(__clang__) && (__GNUC__ > 11)
#pragma GCC diagnostic pop
#endif
}
#ifdef __ICL
#pragma optimize("", on)
#endif


int rhpmat_ensure_cscA(M_ArenaLink *arena, SpMat *m)
{
   if (!m->ppty) { return OK; } // nothing to do for empty (Id) matrix

   assert(m->csr && !m->csc);
   SparseMatrix * restrict csr = m->csr, *csc;

   RHP_INT mrow = csr->m, nrow = csr->n;
   A_CHECK(m->csc, rhpmat_allocA(arena, mrow, nrow, csr->nnz, RHP_CS));
   csc = m->csc;
   csr_tocsc(csr->m, csr->n, csr->p, csr->i, csr->x, csc->p, csc->i, csc->x);

   return OK;

}

/**
 * @brief General interface to get the col of a matrix 
 *
 * This function fill in the vector corresponding to a column in a matrix
 * The signature is quite long, and some arguments are just input in case the
 * matrix is empty or has no data associated with it
 *
 * @param m                 the matrix
 * @param i                 the index of the row to get
 * @param[in]  wrkmem      working memory space when the matrix contains no data
 * @param[out] col_len      the length of the row vector
 * @param[out] col_idxs     the column indices for the row vector
 * @param[out] col_vals     the entries for the row vector
 *
 * @return                  the error code
 */
int rhpmat_col(SpMat* m, unsigned i, SpMatColRowWorkingMem *wrkmem,
               unsigned* restrict col_len, unsigned** restrict col_idxs,
               double** restrict col_vals)
{
   if (m->ppty) {
      bool csc_or_csr = m->ppty & (EMPMAT_CSR | EMPMAT_CSC);
      if (!(csc_or_csr) || m->ppty & EMPMAT_BLOCK) {
         error("%s :: only CSR or CSC matrices are supported\n", __func__);
         return Error_NotImplemented;
      }

      if (m->ppty & EMPMAT_EYE) { goto eye_mat; }

      if (csc_or_csr) {
         if (!(m->ppty & EMPMAT_CSC)) {

            assert(m->csr && !m->csc);
            SparseMatrix * restrict csr = m->csr, *csc;

            RHP_INT mrow = csr->m, nrow = csr->n;
            A_CHECK(m->csc, rhpmat_spalloc(mrow, nrow, csr->nnz, RHP_CS));
            csc = m->csc;
            csr_tocsc(csr->m, csr->n, csr->p, csr->i, csr->x, csc->p, csc->i, csc->x);

         }

         SparseMatrix * restrict csc = m->csc;
         RHP_INT row_start = csc->p[i];
         *col_len = csc->p[i+1] - row_start;
         *col_idxs = &csc->i[row_start];
         *col_vals = &csc->x[row_start];

         return OK;
      }

      return error_runtime();

   }

eye_mat:
   *col_idxs = &wrkmem->single_idx;
   wrkmem->single_idx = i;
   *col_len = 1;
   *col_vals = &wrkmem->single_val;
   wrkmem->single_val = 1.;

   return OK;
}

/**
 * @brief Compute y += Ax where A is a diagonal matrix
 *
 * @param spm  the sparse diagonal matrix
 * @param x    the vector
 * @param y    the output
 */
static void _rhpmat_eye_axpy(const SparseMatrix *spm, const double * restrict x, double * restrict y)
{
   double coeff;
   if (spm->nnzmax == 1) {
      coeff = spm->x[0];
   } else {
      coeff = 1.;
   }
   for (size_t i = 0; i < (size_t)spm->m; ++i) {
      y[i] += coeff*x[i];
   }
}

/**
 * @brief Compute y = A^Tx where A is a general sparse
 *
 * @param spm  the sparse diagonal matrix
 * @param x    the vector
 * @param y    the output
 */
static void _cs_gatxpy(const SparseMatrix *spm, const double * restrict x, double * restrict y)
{
   for (size_t j = 0; j < (size_t)spm->m; ++j) {
      double xj = x[j];
      for (size_t k = spm->p[j]; k < (size_t)spm->p[j+1]; ++k) {
         /*  This is the row for the transpose matrix */
         RHP_INT idx = spm->i[k];
         y[idx] += spm->x[k]*xj;
      }
   }
}

static void _cs_gaxpy(const SparseMatrix *spm, const double * restrict x, double * restrict y)
{
   for (size_t j = 0; j < (size_t)spm->m; ++j) {
      for (size_t k = spm->p[j]; k < (size_t)spm->p[j+1]; ++k) {
         RHP_INT idx = spm->i[k];
         y[j] += spm->x[k]*x[idx];
      }
   }
}

/**
 * @brief computes y = A x
 *
 * @param A  the matrix
 * @param x  the vector
 * @param y  the output
 *
 * @return   the error code
 */
int rhpmat_axpy(const SpMat *A, const double * restrict x, double * restrict y)
{
   if (A->ppty) {
      if (!(A->ppty & EMPMAT_CSR || A->ppty & EMPMAT_BLOCK)) {
         error("%s :: only CSR matrices are supported\n", __func__);
         return Error_NotImplemented;
      }

      if (A->ppty & EMPMAT_BLOCK) {

         struct block_spmat* bmat = A->block;
         /* cs_gaxpy or _rhpmat_eye_axpy does y += Ax */
         memset(y, 0, sizeof(double)*bmat->m);

         if (A->ppty & EMPMAT_EYE) {
            for (size_t i = 0; i < bmat->number; ++i) {
               unsigned start = bmat->row_starts[i];
               assert(!bmat->col_starts || start == bmat->col_starts[i]);
               _rhpmat_eye_axpy(bmat->blocks[i], &x[start], &y[start]);
            }

         } else {
            for (size_t i = 0; i < bmat->number; ++i) {
               SparseMatrix *spm = bmat->blocks[i];
               unsigned rowstart = bmat->row_starts[i];
               unsigned colstart = bmat->row_starts[i];
               _cs_gaxpy(spm, &x[colstart], &y[rowstart]);
            }

         }

      } else {
         SparseMatrix *spm = A->csr;

         /* cs_gaxpy or _rhpmat_eye_axpy does y += Ax */
         memset(y, 0, sizeof(double)*spm->m);

         if (A->ppty & EMPMAT_EYE) {
            _rhpmat_eye_axpy(spm, x, y);
         } else {
            _cs_gaxpy(spm, x, y);
         }
      }
   }

   return OK;
}

/**
 * @brief computes y = A^T x
 *
 * @param A  the matrix
 * @param x  the vector
 * @param y  the output
 *
 * @return   the error code
 */
int rhpmat_atxpy(const SpMat *A, const double * restrict x, double * restrict y)
{
   if (A->ppty) {
      if (!(A->ppty & EMPMAT_CSR || A->ppty & EMPMAT_BLOCK)) {
         error("%s :: only CSR matrices are supported\n", __func__);
         return Error_NotImplemented;
      }

      if (A->ppty & EMPMAT_BLOCK) {

         struct block_spmat* bmat = A->block;

         /* _cs_gatxpy or _rhpmat_eye_axpy does y += Ax */
         memset(y, 0, sizeof(double)*bmat->m);

         if (A->ppty & EMPMAT_EYE) {
            for (size_t i = 0; i < bmat->number; ++i) {
               unsigned start = bmat->row_starts[i];
               assert(!bmat->col_starts || start == bmat->col_starts[i]);
               _rhpmat_eye_axpy(bmat->blocks[i], &x[start], &y[start]);
            }

         } else {
            for (size_t i = 0; i < bmat->number; ++i) {
               SparseMatrix *spm = bmat->blocks[i];
               unsigned rowstart = bmat->row_starts[i];
               unsigned colstart = bmat->row_starts[i];
               _cs_gatxpy(spm, &x[colstart], &y[rowstart]);
            }

         }


      } else {
         SparseMatrix *spm = A->csr;
         /* _cs_gatxpy or _rhpmat_eye_axpy does y += Ax */
         memset(y, 0, sizeof(double)*spm->m);

         if (A->ppty & EMPMAT_EYE) {
            _rhpmat_eye_axpy(spm, x, y);
         } else {

            /* -----------------------------------------------------------------
             * We can use cs_gaxpy since we have the transpose
             * ----------------------------------------------------------------- */

            _cs_gatxpy(spm, x, y);
         }
      }
   }

   return OK;
}

static inline double csr_evalquad(const SparseMatrix * restrict spm,
                                  const double * restrict x)
{
   double res = 0.;

   if (spm->nnzmax == 1) {
      if (fabs(spm->x[0]) < DBL_EPSILON) {
         return 0.;
      }
      for (size_t i = 0; (size_t)spm->m; ++i) {
         res += spm->x[0] * x[i] * x[i];
      }
      return res;
   }

   for (size_t i = 0; i < (size_t)spm->m; ++i) {
      for (RHP_INT j = spm->p[i]; j < spm->p[i+1]; ++j) {
         res += spm->x[j] * x[spm->i[j]] * x[i];
      }
   }

   return res;
}

static inline double speye_evalquad(const double *x, unsigned len_x)
{
   double res = 0.;

   for (unsigned i = 0; i < len_x; ++i) {
      res += x[i]*x[i];
   }

   return res;
}

/**
 * @brief Evaluates the quadratic form at x
 *
 * @param m  the matrix M
 * @param x  the vector x
 *
 * @return   the value <Mx, x>, or NAN if an error occured
 */
double rhpmat_evalquad(const SpMat *m, const double *x)
{
   double res = 0.;
   if (!m->ppty) {
      return NAN;
   }

   if (m->ppty & EMPMAT_BLOCK) {

      struct block_spmat* bmat = m->block;
      if (m->ppty & EMPMAT_EYE) {
         for (size_t i = 0; i < bmat->number; ++i) {
            SparseMatrix *spm = bmat->blocks[i];
            if (spm->nnzmax == 0) {
               res += speye_evalquad(x, spm->m);
            } else {
               assert(spm->nnzmax == 1);
               res += spm->x[0]*speye_evalquad(x, spm->m);
            }

         }

         goto _end;
      }

      for (size_t i = 0; i < bmat->number; ++i) {
         SparseMatrix *spm = bmat->blocks[i];
         unsigned start = bmat->row_starts[i];
         assert(!bmat->col_starts || start == bmat->col_starts[i]);
         res += csr_evalquad(spm, &x[start]);
      }
   } else if (m->ppty & EMPMAT_EYE) {

      assert(m->ppty & EMPMAT_CSR || m->ppty & EMPMAT_BLOCK);
      if (m->csr->nnzmax == 0) {
         res = speye_evalquad(x, m->csr->m);
         goto _end;
      } else {
         assert(m->csr->nnzmax == 1);
         res = m->csr->x[0]*speye_evalquad(x, m->csr->m);
         goto _end;
      }

   } else if (m->ppty & EMPMAT_CSR) {

      res = csr_evalquad(m->csr, x);

   } else {

      error("%s :: only CSR or EYE matrices are supported\n", __func__);
      return NAN;
   }

_end:
   return res;
}

bool rhpmat_is_square(SpMat* m)
{
   enum EMPMAT_PPTY type = rhpmat_get_type(m);
   switch (type) {
   case EMPMAT_CSR:
      return m->csr && (m->csr->m == m->csr->n);
   case EMPMAT_CSC:
      return m->csc && (m->csc->m == m->csc->n);
   case EMPMAT_COO:
      return m->triplet && (m->triplet->m == m->triplet->n);
   default:
      if (m->ppty & EMPMAT_BLOCK) {
         return m->block && (m->block->m == m->block->n);
      } else if (m->ppty & EMPMAT_EYE) {
         return true;
      }
      return false;
   }
}
