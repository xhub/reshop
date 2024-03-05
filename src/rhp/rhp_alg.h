#ifndef RMDL_ALG_H
#define RMDL_ALG_H

#include "rhp_fwd.h"


int rctr_compress_check_equ(const Container *ctr_src, Container *ctr_dst, size_t skip_equ) NONNULL;
int rctr_compress_check_equ_x(const Container *ctr_src, Container *ctr_dst,
                              size_t skip_equ, const Fops *fops);
int rctr_compress_check_var(size_t ctr_n, size_t total_n, size_t skip_var);
int rctr_compress_vars(const Container *ctr_src, Container *ctr_dst) NONNULL;
int rctr_compress_vars_x(const Container *ctr_src, Container *ctr_dst, Fops *fops) NONNULL_AT(1,2);
int rmdl_presolve(Model *mdl, unsigned backend) NONNULL;

#endif
