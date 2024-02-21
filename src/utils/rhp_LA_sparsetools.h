#ifndef RHP_LA_SPARSETOOLS
#define RHP_LA_SPARSETOOLS

#include "rhp_LA.h"

void csr_tocsc(const RHP_INT n_row,
               const RHP_INT n_col,
               const RHP_INT Ap[],
               const RHP_INT Aj[],
               const double Ax[],
               RHP_INT Bp[],
               RHP_INT Bi[],
               double Bx[]);

#endif
