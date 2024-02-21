#ifndef OVF_GENERATOR_H
#define OVF_GENERATOR_H

#include "cones.h"
#include "rhp_fwd.h"
#include "rhp_LA.h"
/*  For var_genops */
#include "var.h"

#define xstr(s) str(s)
#define str(s) #s

enum SPEYE_MAT_PPTY {FORCE_ALLOC, FILL_DATA, INIT_DATA};

typedef int (*gen_mat)(unsigned n, const void* env, SpMat* mat) NONNULL;
typedef int (*gen_vec)(unsigned m, const void* env, double** vals) NONNULL;

typedef int (*gen_variable)(Container* ctr, unsigned nb_vars,
                            const void* env) NONNULL;
typedef int (*gen_set_b_0)(unsigned n, const void* env, SpMat* mat,
                           double *u_shift, double **vals) NONNULL;
typedef int (*gen_k)(unsigned n, const void* env, Equ *e) NONNULL;
typedef enum cone (*gen_cones)(unsigned n, const void* env, unsigned idx,
                                void **cone_data) NONNULL;

typedef struct ovf_genops {
   gen_vec b;
   gen_k k;
   gen_mat B;
   gen_mat D;
   gen_mat J;
   gen_mat M;
   gen_mat set_A;
   gen_mat set_A_nonbox;
   gen_vec set_b;
   gen_vec set_b_nonbox;
   gen_set_b_0 set_b_0;
   gen_cones set_cones;
   gen_cones set_cones_nonbox;
   size_t (*size_u)(size_t n_args);
   gen_vec u_shift;
   gen_variable var;
   const struct var_genops *var_ppty;
   const char *name;
} OvfGenOps;

struct ovf_data {
   struct sp_matrix* b;
   struct sp_matrix* B;
   struct sp_matrix* D;
   struct sp_matrix* M;
};

int ovf_gen_id_matrix(unsigned n, const void *env, SpMat *mat) NONNULL;
struct sp_matrix* ovf_speye_mat_x(unsigned n, unsigned m, double value,
                                  bool ppty[]);

/** Generate a matrix with value on the diagonal. If m and n have different
 * size, then they should be multiple of one another. The returned matrix is
 * then the concatenation of matrices
 * \param n number of columns
 * \param m number of rows
 * \param value value of the diagonal
 * \return a CSR matrix if success. Otherwise, NULL
 */
static inline struct sp_matrix* ovf_speye_mat(unsigned n, unsigned m,
                                              double value)
{
   bool ppty[] = {false, false, false};
   return ovf_speye_mat_x(n, m, value, ppty);
}

int ovf_box(Container* ctr, unsigned nb_vars, const void* env,
            const struct var_genops* varfill) NONNULL;
#endif /* OVF_GENERATOR_H */
