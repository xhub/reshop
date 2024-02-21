#ifndef NLTREE_H
#define NLTREE_H

/**
 *  @file nltree.h
 *
 *  @brief expression tree for nonlinear equations
 */

#include <stdbool.h>
#include <stddef.h>

#include "compat.h"
#include "rhp_fwd.h"
#include "instr.h"

typedef struct vartree VarTree;

typedef struct bucket_node {
   unsigned in_filledbuckets;  /**< Number of items in filled buckets  */
   unsigned bucket_idx;
   unsigned bucket_max;
   unsigned pool_idx;
   unsigned pool_max;
   NlNode **pool;
} NlNodeArena;

typedef struct bucket_children {
   unsigned in_filledbuckets;      /**< Number of items in filled buckets   */
   unsigned bucket_idx;
   unsigned bucket_max;
   unsigned pool_idx;
   unsigned pool_max;
   NlNode ***pool;
} NlChildrenArena;

/* TODO(xhub) kill this and replace it with rhp_idx_array  */
struct vlist {
   unsigned idx;
   unsigned max;
   int *pool;
};

/** @brief expression tree */
typedef struct rhp_nltree {
   NlNode *root;             /**< root node */
   rhp_idx idx;                          /**< index of the equation */
//   struct dynpool *additional_pool;
   VarTree *vt;               /**< list of variables in the tree */
   struct vlist *v_list;             /**< variable list (workspace)*/
   struct bucket_node nodes;         /**< node bucket */
   struct bucket_children children; /**< children bucket */
} NlTree;

/* -------------------------------------------------------------------------
 * Nlnode functions
 * ------------------------------------------------------------------------- */

void nlnode_dealloc(NlNode* node);
NlNode* nlnode_alloc(NlTree* tree, unsigned len) MALLOC_ATTR(nlnode_dealloc,1);
NlNode* nlnode_alloc_fixed(NlTree* tree, unsigned len) MALLOC_ATTR(nlnode_dealloc,1);
NlNode* nlnode_alloc_nochild(NlTree* tree) MALLOC_ATTR(nlnode_dealloc,1);
int nlnode_findfreechild(NlTree *tree, NlNode *node,
                          unsigned len, unsigned *idx);
int nlnode_reserve(NlTree* tree, NlNode* node, size_t len);
//int nlnode_replace_child(NlNode* node, NlNode* c, unsigned indx);

/* -------------------------------------------------------------------------
 * Equtree creation and destruction
 * ------------------------------------------------------------------------- */

void nltree_dealloc(NlTree* tree);
NlTree* nltree_alloc(size_t len) MALLOC_ATTR(nltree_dealloc,1);
int nltree_bootstrap(Equ *e, unsigned est_size, unsigned est_add);

/* -------------------------------------------------------------------------
 * Equtree modification: add node
 * ------------------------------------------------------------------------- */

NONNULL
int rctr_nltree_add_bilin(Container *ctr, NlTree* tree,
                          NlNode ** restrict *node, double coeff, rhp_idx v1, rhp_idx v2);
int rctr_nltree_add_bilin_vars(Container *ctr, NlTree *tree,
                           NlNode *node, unsigned offset,
                           Avar * restrict v1, Avar * restrict v2);
int nltree_add_cst(Container *ctr, NlTree* tree, NlNode ***node,
                    double cst);
int nltree_add_nlexpr(NlTree *tree, NlNode *node, NlPool *pool, double cst) NONNULL;
int rctr_equ_add_nlexpr_full(Container *ctr, NlTree *tree, const NlNode *node,
                             double coeff, const rhp_idx* rosetta);
int rctr_nltree_add_lin_term(Container *ctr, NlTree* tree,
                         NlNode ** restrict *node, Lequ *lequ,
                         rhp_idx vi_no, double coeffp);
int nltree_add_var(Container *ctr, NlTree *tree,
                    NlNode ***node, int vi, double coeff);
NONNULL
int nltree_add_var_tree(Container *ctr, NlTree *tree, rhp_idx vi, double val);

/* -------------------------------------------------------------------------
 * Equtree modification: MUL node
 * ------------------------------------------------------------------------- */

int nltree_mul_cst(NlTree* tree, NlNode ***node,  NlPool *pool, double coeff);
int nltree_mul_cst_add_node(NlTree* tree, NlNode ***node, NlPool *pool,
                            double coeff, unsigned size, unsigned *idx);
int nltree_mul_cst_x(NlTree* tree, NlNode ***node, NlPool *pool, double coeff,
                     bool *new_node);

int rctr_nltree_opcall1(Container *ctr, NlTree* tree, NlNode ***node,
                        rhp_idx vi, unsigned fnidx);

int nltree_scal(Container *ctr, NlTree* tree, double coeff) NONNULL;
int nltree_scal_umin(Container *ctr, NlTree *tree) NONNULL;
int nltree_umin(NlTree *tree, NlNode ** restrict *node);

int rctr_nltree_var(Container *ctr, NlTree* tree, NlNode ***node, rhp_idx vi,
                    double coeff);


int rctr_nltree_copy_to(Container *ctr, NlTree *tree, NlNode **dstnode,
                        NlNode *srcnode, double cst) NONNULL;
/* -------------------------------------------------------------------------
 * Checking functions
 * ------------------------------------------------------------------------- */

int nltree_check_add(NlNode *node);

/* -------------------------------------------------------------------------
 * Conversion from nltree to other format (GAMS, AMPL)
 * ------------------------------------------------------------------------- */

NlTree* nltree_buildfromgams(unsigned codelen, const int *instrs,
                             const int *args) NONNULL;
int nltree_buildopcode(Container *mdl, const Equ * e, int **instrs,
                        int **args, int *codelen) NONNULL;

/* -------------------------------------------------------------------------
 * Equtree misc
 * ------------------------------------------------------------------------- */

NlTree* nltree_dup(const NlTree *tree, const unsigned *var_indices, unsigned nb_var);
NlTree* nltree_dup_rosetta(const NlTree *tree, const int *rosetta);

int nltree_apply_rosetta(NlTree *tree, const rhp_idx * restrict rosetta) NONNULL;


int nltree_alloc_var_list(NlTree* tree) NONNULL;
int nltree_reset_var_list(NlTree* tree) NONNULL;
int nltree_eval(Container *ctr, NlTree *tree, double *val);
int nltree_evalat(NlTree *tree, const double *x, double *arr, double *val) NONNULL;
int nltree_find_add_node(NlTree *tree, NlNode ***raddr, NlPool *pool, double *coeff);

void nltree_print_dot(const NlTree* tree, const char* filename, const Container *ctr);
int nltree_replacevarbycst(NlTree* tree, rhp_idx vi, unsigned pool_idx) NONNULL;
int nltree_replacevarbytree(NlTree* tree, rhp_idx vi, const NlTree* subtree) NONNULL;
int nltree_reserve_add_node(NlTree *tree, NlNode **addr,
                             unsigned size, unsigned *offset);


/* -------------------------------------------------------------------------
 * High-level interface
 * ------------------------------------------------------------------------- */

int nltree_get_add_node(Model* mdl, rhp_idx ei, unsigned n_children,
                        NlNode **add_node, unsigned *offset, double *coeff) NONNULL;

/* -------------------------------------------------------------------------
 * Static inline functions
 * ------------------------------------------------------------------------- */

/**
 *  @brief Add a square term to the tree
 *
 * @ingroup EquSafeEditing
 *
 *  @param ctr   the container object
 *  @param tree  the tree to modify
 *  @param node  the node
 *  @param vidx  the variable index
 *
 *  @return      the error code
 */
static inline int rctr_nltree_add_sqr(Container *ctr, NlTree* tree, NlNode ***node,
                                 rhp_idx vi)
{
   return rctr_nltree_opcall1(ctr, tree, node, vi, fnsqr);
}

NONNULL static inline unsigned nltree_numnodes(const NlTree *tree)
{
   return tree->nodes.in_filledbuckets + tree->nodes.pool_idx;
}

NONNULL static inline unsigned nltree_numchildren(const NlTree *tree)
{
   return tree->children.in_filledbuckets + tree->children.pool_idx;
}

#endif /* NLTREE_H */
