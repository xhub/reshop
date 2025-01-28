#ifndef RHP_LA_SPARSETOOLS
#define RHP_LA_SPARSETOOLS

#include "rhp_LA.h"

void csr_tocsc(RHP_INT n_row,
               RHP_INT n_col,
               const RHP_INT Ap[VMT(static n_row+1)],
               const RHP_INT Aj[],
               const double Ax[],
               RHP_INT Bp[VMT(static n_col+1)],
               RHP_INT Bi[],
               double Bx[]);

#endif
