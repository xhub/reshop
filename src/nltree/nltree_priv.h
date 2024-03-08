#ifndef NLTREE_PRIV_H
#define NLTREE_PRIV_H

#include <assert.h>
#include <stdbool.h>
#include <stddef.h>
#include <stdint.h>

#include "ctr_rhp.h"
#include "rhp_fwd.h"

#include "compat.h"
#include "nltree.h"
#include "macros.h"
#include "printout.h"
#include "status.h"
#include "rhpidx.h"

/** Subtype of opcode */
typedef enum NLNODE_OPARG {
   NLNODE_OPARG_UNSET = 0, /**< Undetermined type*/
   NLNODE_OPARG_CST,       /**< Constant that get pull to the arithmetic operation */
   NLNODE_OPARG_VAR,       /**< Variable that get pull to the arithmetic operation */
   NLNODE_OPARG_FMA,       /**< Fused multiply add */
   __OPTYPE_LEN,
} NlNodeOpArg;

/** Type of operation */
typedef enum NLNODE_OP {
   NLNODE_CST,      /**< numerical constant */
   NLNODE_VAR,      /**< variable */
   NLNODE_ADD,      /**< Addition */
   NLNODE_SUB,      /**< Substitution */
   NLNODE_MUL,      /**< Multiplication */
   NLNODE_DIV,      /**< Division */
   NLNODE_CALL1,    /**< function call with 1 argument */
   NLNODE_CALL2,    /**< function call with 2 arguments  */
   NLNODE_CALLN,    /**< function call with an arbitrary number of argument */
   NLNODE_UMIN,     /**< unitary minus */
   __OPCODE_LEN
} NlNodeOp;

/** Additional property of a node */
enum NODE_PPTY {
   NODE_NONE = 0,      /**< No property */ 
   NODE_PRINT_NOW = 1  /**< the node is immediately injected in the opcode */
};

/** @brief node definition */
typedef struct rhp_nlnode {
   NlNodeOp op;                   /**< type of operation */
   NlNodeOpArg oparg;             /**< argument of operation */
   enum NODE_PPTY ppty;           /**< node property */
   unsigned value;                /**< value of the operation */
   unsigned children_max;         /**< maximum number of children */
   union {
      NlNode **children;
      double val;
      double *vals;
      SpMat *mat;
   };                             /**< payload of the node */
} NlNode;



/* TODO: This is just food for thought, unused for now*/

/** @brief node definition */
typedef struct rhp_nlnode0 {
   unsigned value;     /* Lower 1 bit: CST or VAR, rest: index*/
} NlNode0;

/* UINT8_MAX: umin; UINT8_MAX-1: uminV; UINT8_MAX-2: uminCst*/
typedef uint8_t NlNode1Type;

/* UINT8_MAX: umin; UINT8_MAX-1: uminV; UINT8_MAX-2: uminCst*/
typedef uint8_t NlNode2Type;

/** @brief node definition */
typedef struct rhp_nlnode1 { /* UMIN / CALL 1*/
   NlNode1Type type;  /* If a valid func_code <= fndummy, then it is a call node, otherwise  a umin node */
   unsigned child;
} NlNode1;

/** @brief node definition */
typedef struct rhp_nlnode2 { /* SUB / DIV / CALL2*/
   NlNode2Type type;  /* If a valid func_code <= fndummy, then it is a call node, otherwise  a SUB or DIV node */
   unsigned child1;
   unsigned child2;
} NlNode2;

/** @brief node definition */
typedef struct rhp_nlnodeN { /* ADD / MUL / CALLN */
   NlNode2Type type;  /* If a valid func_code <= fndummy, then it is a call node, otherwise  a ADD/MUL node */
   unsigned *children;
} NlNodeN;

/** @brief node for nltree
 */
//struct nlnode {
//   unsigned id;                   /**< The id of this node       */
//   unsigned value;                /**< value of the operation    */
//   enum OPCODE_CLASS op;          /**< type of operation         */
//   enum OPCODE_TYPE optype;       /**< subtype of operation      */
//   enum NODE_PPTY ppty;           /**< node property             */
//};

typedef struct vartree_list {
   unsigned idx;
   unsigned len;
   unsigned max;
   NlNode **nodes;
} VarTreeList;


typedef struct vartree {
   unsigned len;
   bool done;
   VarTreeList vars[] __counted_by(len);
} VarTree;

extern const char * const opcode_names[];

#define VIDX(X)      ((X)+1)
#define VIDX_R(X)    ((X)-1)

/*  CIDX should not be used */
#define _CIDX_R(X)   ((X)-1)
#define CIDX_R(X) assert((X) > 0), _CIDX_R(X)

int nlnode_add_child(NlNode* node, NlNode* c, size_t indx);
int nlnode_copy(NlNode** new_node, const NlNode* node, NlTree* tree);
int nlnode_copy_rosetta(NlNode** new_node, const NlNode* node,
                        NlTree* tree, const rhp_idx* rosetta);
int nlnode_apply_rosetta(NlNode *node, struct vlist *v_list,
                         const rhp_idx * restrict rosetta) NONNULL;
int nltree_getallvars(NlTree *tree) NONNULL;
enum NLNODE_OPARG gams_get_optype(int opcode);
void nlnode_print(const NlNode *node, unsigned mode,
                    bool print_children);

static inline bool _print_now(const uint8_t ppty)
{
   return (ppty & NODE_PRINT_NOW) != 0 ? true : false;
}

static inline int nlnode_print_now(NlNode* node)
{
   if (!node) { return Error_NullPointer; }
   node->ppty = NODE_PRINT_NOW;
   return OK;
}

static inline NlNode* nlnode_alloc_fixed_init(NlTree* tree, unsigned len)
{
   NlNode *lnode;
   AA_CHECK(lnode, nlnode_alloc_fixed(tree, len));
   for (unsigned i = 0; i < len; ++i) {
      lnode->children[i] = NULL;
   }
   return lnode;
}

static inline NlNode* nlnode_alloc_init(NlTree* tree, unsigned len)
{
   NlNode *lnode;
   AA_CHECK(lnode, nlnode_alloc(tree, len));
   for (unsigned i = 0; i < len; ++i) {
      lnode->children[i] = NULL;
   }
   return lnode;
}

/**
 * @brief Get the offset in a node with children
 *
 * @param       tree    the tree
 * @param       node    the node
 * @param[out]  offset  the offset
 *
 * @return              the error code
 */
static inline int nlnode_child_getoffset(NlTree *tree,
                                         NlNode *node,
                                         unsigned *offset)
{
   assert(node && tree);
   unsigned loffset = 0;
   /* TODO(xhub) improve the search  */
   while (loffset < node->children_max && node->children[loffset]) {
      loffset++;
   }

   if (loffset == node->children_max) {
      /*  TODO(xhub) check constant */
      S_CHECK(nlnode_reserve(tree, node, 3));
   }

   *offset = loffset;

   return OK;
}

static inline int nlnode_next_child(NlTree *tree, NlNode*** node)
{
   assert(node && *node && **node);
   unsigned offset = 0; /* init just for compiler warnings */
   if ((**node)->op != NLNODE_ADD && (**node)->op != NLNODE_SUB
         && (**node)->op != NLNODE_MUL && (**node)->op != NLNODE_DIV) {
      error("%s :: unsupported node of type %d", __func__,
               (**node)->op);
      return Error_Inconsistency;
   }
   S_CHECK(nlnode_child_getoffset(tree, **node, &offset));
   (*node) = &(**node)->children[offset];

   return OK;
}

static inline void nlnode_default(NlNode *node, enum NLNODE_OP op)
{
   node->op = op;
   node->oparg = NLNODE_OPARG_UNSET;
   node->value = 0;
}

static inline void nlnode_setvar(NlNode *node, rhp_idx vi)
{
   assert(node && valid_vi(vi));

   node->op = NLNODE_VAR;
   node->oparg = NLNODE_OPARG_UNSET;
   node->value = VIDX(vi);
}

NONNULL static inline
int nlnode_mulcst(Container *ctr, NlNode *node, double cst)
{
   assert(node->op == NLNODE_MUL);
   node->oparg = NLNODE_OPARG_CST;
   unsigned idx = rctr_poolidx(ctr, cst);
   if (idx == UINT_MAX) {
      return Error_InsufficientMemory;
   }

   node->value = idx;
   return OK;
}

/**
 *  @brief  Copy the attributes of a node (everything except the children info) 
 *
 *  @param dst   the destination node
 *  @param src   the source node
 *
 */
static inline NONNULL void nlnode_attr_copy(NlNode* dst, const NlNode* src)
{
   dst->op = src->op;
   dst->oparg = src->oparg;
   dst->ppty = src->ppty;
   dst->value = src->value;
}

/**
 *  @brief  Copy the attributes of a node (everything except the children info) 
 *
 *  @param dst      the destination node
 *  @param src      the source node
 *  @param rosetta  the conversion of variable indices
 *
 */
static inline NONNULL
void nlnode_attr_copy_rosetta(NlNode* dst, const NlNode* src, const rhp_idx* rosetta)
{
   dst->op = src->op;
   dst->oparg = src->oparg;
   dst->ppty = src->ppty;
   if (src->op == NLNODE_VAR || src->oparg == NLNODE_OPARG_VAR) {
      dst->value = VIDX(rosetta[VIDX_R(src->value)]);
   } else {
      dst->value = src->value;
   }
}

static inline NONNULL int vlist_add(struct vlist *vlist, rhp_idx vidx)
{
   for (size_t i = 0; i < vlist->idx; ++i) {
      if (vidx == vlist->pool[i]) {
         return OK;
      }
   }

   if (vlist->idx >= vlist->max) {
      vlist->max = MAX(vlist->max*2, 2);
      REALLOC_(vlist->pool, int, vlist->max);
   }
   vlist->pool[vlist->idx++] = vidx;

   return OK;
}

#endif
