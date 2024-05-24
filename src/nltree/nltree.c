#include "reshop_config.h"

#include <fenv.h>
#include <float.h>
#include <inttypes.h>
#include <math.h>
#include <stdbool.h>
#include <stdio.h>
#include <string.h>

#include "cmat.h"
#include "compat.h"
#include "container.h"
#include "nltree.h"
#include "nltree_priv.h"
#include "equ.h"
#include "macros.h"
/* For ctr_printvarname (in debug mode )  */
#include "pool.h"
#include "printout.h"
#include "status.h"
#include "var.h"


//#define DEBUG_TREE

typedef double(*fnarg1)(double x1);
typedef double(*fnarg2)(double x1, double x2);


const char * const opcode_names[] = { "CST", "VAR", "ADD", "SUB", "MUL", "DIV",
                                     "CALL1", "CALL2", "CALLN", "UMIN", "UNKNOWN"};

const char * const oparg_names[] = { "NONE", "CST", "VAR", "FMA", "LINEAR" };

RESHOP_STATIC_ASSERT(sizeof(opcode_names)/sizeof(char*) == __OPCODE_LEN + 1,
                     "OPCODE_CLASS and opcode_names must be synchronized")

void nlnode_print(const NlNode *node, unsigned mode,
                    bool print_children)
{
   if (!node) return;
   printout(mode, "node property:\n"
                  "op = %s (%d) \n"
                  "oparg = %s (%d)\n"
                  "ppty = %d\n"
                  "value = %d\n"
                  "children_max = %d\n"
                  "Child(ren): \n",
                  node->op < __OPCODE_LEN ? opcode_names[node->op] : NULL,
                  node->op,
                  node->oparg < __OPTYPE_LEN ? oparg_names[node->oparg] : NULL,
                  node->oparg,
                  node->ppty,
                  node->value,
                  node->children_max);
   for (unsigned i = 0; i < node->children_max; ++i) {
     if (print_children && node->children[i]) {
        nlnode_print(node->children[i], mode, true);
     } else {
        printout(mode, "child %d: %" PRIxPTR "\n", i, (uintptr_t)node->children[i]);
     }
   }
}


static VarTree* _vartree_alloc(unsigned nb_var, const unsigned* var_indices)
{
   VarTree* vt;
   MALLOCBYTES(vt, VarTree, sizeof(VarTree) + nb_var*sizeof(VarTreeList));
   if (!vt) return NULL;

   vt->len = nb_var;
   vt->done = false;

   for (unsigned i = 0; i < nb_var; ++i) {
      vt->vars[i].idx = var_indices[i];
      vt->vars[i].max = 2;
      vt->vars[i].len = 0;
      MALLOC_EXIT_NULL(vt->vars[i].nodes, NlNode *, vt->vars[i].max);
   }

   return vt;

_exit:
   FREE(vt);
   return NULL;
}

static void _vartree_dealloc(VarTree** vtp)
{
   assert(vtp);
   VarTree* vt = *vtp;
   if (!vt) return;

   for (unsigned i = 0; i < vt->len; ++i)
   {
      if (vt->vars[i].nodes) {
         FREE(vt->vars[i].nodes);
         vt->vars[i].nodes = NULL;
      }
   }

   FREE(vt);
}

#define TREE_ARENA_GROWTH_FACTOR 1.4

NONNULL static NlNode* _nltree_getnode(NlTree* tree)
{
   if (tree->nodes.pool_idx >= tree->nodes.pool_max) {

      tree->nodes.bucket_idx++;
      tree->nodes.in_filledbuckets += tree->nodes.pool_max;

      if (tree->nodes.bucket_idx >= tree->nodes.bucket_max) {
         tree->nodes.bucket_max = TREE_ARENA_GROWTH_FACTOR * tree->nodes.bucket_max;
         REALLOC_NULL(tree->nodes.pool, NlNode *, tree->nodes.bucket_max);
      }

      tree->nodes.pool_max = TREE_ARENA_GROWTH_FACTOR * tree->nodes.pool_max;
      MALLOC_NULL(tree->nodes.pool[tree->nodes.bucket_idx], NlNode, tree->nodes.pool_max);

      tree->nodes.pool_idx = 0;
   }

   return &tree->nodes.pool[tree->nodes.bucket_idx][tree->nodes.pool_idx++];
}

NONNULL static NlNode** _nltree_getnode_children(NlTree* tree, unsigned len)
{
   if (tree->children.pool_idx + len >= tree->children.pool_max)
   {
      tree->children.bucket_idx++;
      tree->nodes.in_filledbuckets += tree->nodes.pool_idx;

      tree->children.pool_max = MAX(TREE_ARENA_GROWTH_FACTOR*tree->children.pool_max, len+10);

      if (tree->children.bucket_idx >= tree->children.bucket_max) {
         tree->children.bucket_max *= 2;
         REALLOC_NULL(tree->children.pool, NlNode **,
               tree->children.bucket_max);
      }

      MALLOC_NULL(tree->children.pool[tree->children.bucket_idx], NlNode*,
            tree->children.pool_max);

      tree->children.pool_idx = 0;
   }

   size_t indx = tree->children.pool_idx;
   tree->children.pool_idx += len;

   return &tree->children.pool[tree->children.bucket_idx][indx];
}


NlNode* nlnode_alloc(NlTree* tree, unsigned len)
{
   NlNode *node;
   AA_CHECK(node, _nltree_getnode(tree));
   AA_CHECK_EXIT(node->children, _nltree_getnode_children(tree, len + 2));
   node->children[len] = NULL;
   node->children[len+1] = NULL;

   node->ppty = 0;
   node->value = 0;
   node->children_max = len;

   return node;

_exit:
   FREE(node);
   return NULL;
}

NlNode* nlnode_alloc_fixed(NlTree* tree, unsigned len)
{
   NlNode *node;
   AA_CHECK(node, _nltree_getnode(tree));
   AA_CHECK_EXIT(node->children, _nltree_getnode_children(tree, len));

   node->ppty = 0;
   node->value = 0;
   node->children_max = len;

   return node;

_exit:
   FREE(node);
   return NULL;
}

NlNode* nlnode_alloc_nochild(NlTree* tree)
{
   NlNode *node;
   AA_CHECK(node, _nltree_getnode(tree));
   node->ppty = 0;
   node->value = 0;
   node->children_max = 0;
   return node;
}

/**
 * @brief Find the first free child of a node and make sure there is enough space
 *
 * @param tree  the tree
 * @param node  the node
 * @param len   the number of children needed
 *
 * @return      the error code
 */
int nlnode_findfreechild(NlTree *tree, NlNode *node, unsigned len, unsigned *idx)
{
  unsigned i = 0;
  unsigned max  = node->children_max;

  while (i < max && !node->children[i]) { i++; };

  unsigned free_space = max - i;
  if (free_space < len) {
    S_CHECK(nlnode_reserve(tree, node, len - free_space));
  }

  *idx = i;
  return OK;
}

/** 
 *  @brief add space in the children of a node.
 *
 *  @param tree  the tree
 *  @param node  the node to extend
 *  @param len   the number of children to add
 *
 *  @return      the exit code
 */
int nlnode_reserve(NlTree *tree, NlNode *node, size_t len)
{
  /*  \TODO(xhub) we should be able to notify that we don't need the memory
   *  space */
   NlNode **c;
   assert(node);
   A_CHECK(c, _nltree_getnode_children(tree, node->children_max+len));
   memcpy(c, node->children, node->children_max * sizeof(NlNode *));
   node->children = c;

   for (unsigned i = node->children_max; i < node->children_max + len; ++i) {
      node->children[i] = NULL;
   }

   node->children_max += len;

   return OK;
}

/** 
 *  @brief add a umin to an expression tree
 *
 *  @param tree  the expression tree
 *  @param node  the current node
 *
 *  @return      the error code
 */
int nltree_umin(NlTree *tree, NlNode ** restrict *node)
{
   NlNode *lnode;
   assert(tree && node && *node && !(**node));

   A_CHECK(lnode, nlnode_alloc_fixed_init(tree, 1));
   (**node) = lnode;
   nlnode_default(lnode, NLNODE_UMIN);

   *node = &lnode->children[0];

   return OK;
}

/**
 * @brief Allocate a nltree
 *
 * @param n_nodes  estimation of the number of nodes
 *
 * @return         the nltree
 */
NlTree* nltree_alloc(size_t n_nodes)
{
   NlTree* t;
   CALLOC_NULL(t, NlTree, 1);

   /*  TODO(xhub) this is bad */
   if (n_nodes < 3) {
      n_nodes = 3;
   }

   MALLOC_EXIT_NULL(t->nodes.pool, NlNode *, 3);
   MALLOC_EXIT_NULL(t->nodes.pool[0], NlNode, n_nodes);
   MALLOC_EXIT_NULL(t->children.pool, NlNode **, 3);
   MALLOC_EXIT_NULL(t->children.pool[0], NlNode *, 2*n_nodes);
   t->nodes.in_filledbuckets = 0;
   t->nodes.bucket_idx = 0;
   t->nodes.bucket_max = 3;
   t->nodes.pool_idx = 0;
   t->nodes.pool_max = n_nodes;

   t->children.in_filledbuckets = 0;
   t->children.bucket_idx = 0;
   t->children.bucket_max = 3;
   t->children.pool_idx = 0;
   t->children.pool_max = 2*n_nodes;

   t->idx = IdxInvalid;

   return t;

_exit:
   if (t->nodes.pool) {
     FREE(t->nodes.pool[0]);
     FREE(t->nodes.pool);
   }

   if (t->children.pool) {
     FREE(t->children.pool[0]);
     FREE(t->children.pool);
   }

   FREE(t);
   return NULL;
}

/**
 * @brief Allocate a nltree
 *
 * @param n_nodes  estimation of the number of nodes
 *
 * @return         the nltree
 */
static int nltree_reserve(NlTree *tree, unsigned n_nodes, unsigned n_children)
{
   /* if n_nodes >= 2/3 * (available + next bucket), then we resize the current
    * bucket and */
   NlNodeArena *node_arena = &tree->nodes;
   if (n_nodes >= 2./3 * (node_arena->pool_max - node_arena->pool_idx + node_arena->pool_max * TREE_ARENA_GROWTH_FACTOR)) {
      // TODO: valgrind complains that realloc moves the memory even if the size should fit. Check that ths is true
      //REALLOC_(node_arena->pool[node_arena->bucket_idx], NlNode, node_arena->pool_idx);
      node_arena->pool_idx = 0;
      node_arena->pool_max = n_nodes * 1.2;

      node_arena->bucket_idx++;
      MALLOC_(node_arena->pool[node_arena->bucket_idx], NlNode, node_arena->pool_max);
   }

   NlChildrenArena *children_arena = &tree->children;
   if (n_children >= 2./3 * (children_arena->pool_max - children_arena->pool_idx + children_arena->pool_max * TREE_ARENA_GROWTH_FACTOR)) {
      // TODO: valgrind complains that realloc moves the memory even if the size should fit. Check that ths is true
      //REALLOC_(children_arena->pool[children_arena->bucket_idx], NlNode *, children_arena->pool_idx);
      children_arena->pool_idx = 0;
      children_arena->pool_max = n_children * 1.2;

      children_arena->bucket_idx++;
      MALLOC_(children_arena->pool[children_arena->bucket_idx], NlNode *, children_arena->pool_max);
   }

   return OK;
}

/**
 * @brief Allocate a nltree
 *
 * @param n_nodes  estimation of the number of nodes
 *
 * @return         the nltree
 */
static NlTree* nltree_alloc2(unsigned n_nodes, unsigned n_children)
{
   NlTree* t;
   CALLOC_NULL(t, NlTree, 1);

   /*  TODO(xhub) this is bad */
   if (n_nodes < 3) {
      n_nodes = 3;
   }

   MALLOC_EXIT_NULL(t->nodes.pool, NlNode *, 3);
   MALLOC_EXIT_NULL(t->nodes.pool[0], NlNode, n_nodes);
   MALLOC_EXIT_NULL(t->children.pool, NlNode **, 3);
   MALLOC_EXIT_NULL(t->children.pool[0], NlNode *, n_children);
   t->nodes.in_filledbuckets = 0;
   t->nodes.bucket_idx = 0;
   t->nodes.bucket_max = 3;
   t->nodes.pool_idx = 0;
   t->nodes.pool_max = n_nodes;

   t->children.in_filledbuckets = 0;
   t->children.bucket_idx = 0;
   t->children.bucket_max = 3;
   t->children.pool_idx = 0;
   t->children.pool_max = n_children;

   t->idx = IdxInvalid;

   return t;

_exit:
   if (t->nodes.pool) {
     FREE(t->nodes.pool[0]);
     FREE(t->nodes.pool);
   }

   if (t->children.pool) {
     FREE(t->children.pool[0]);
     FREE(t->children.pool);
   }

   FREE(t);
   return NULL;
}
void nltree_dealloc(NlTree* tree)
{
   if (tree) {
      for (size_t i = 0; i <= tree->nodes.bucket_idx; ++i) {
         FREE(tree->nodes.pool[i]);
      }
      FREE(tree->nodes.pool);

      for (size_t i = 0; i <= tree->children.bucket_idx; ++i) {
         FREE(tree->children.pool[i]);
      }
      FREE(tree->children.pool);

      _vartree_dealloc(&tree->vt);
      if (tree->v_list) { FREE(tree->v_list->pool); FREE(tree->v_list); }

      FREE(tree);
   }
}


static int _nlnode_replacevarbycst(NlNode* node, rhp_idx vi,
                                    unsigned pool_idx)
{
   int info;
   if (!node) return OK;
   rhp_idx lvi = VIDX(vi);

   enum NLNODE_OPARG oparg = node->oparg;
   if (oparg == NLNODE_OPARG_VAR && node->value == lvi) {
     node->oparg = NLNODE_OPARG_CST;
     node->value = pool_idx;
   }

   for (size_t i = 0; i < node->children_max; ++i) {
      if (!node->children[i]) continue;

      NlNode *child =  node->children[i];
      unsigned op = child->op;

      if (op == NLNODE_VAR && child->value == lvi) {
          child->op = NLNODE_CST;
          child->value = pool_idx;
      } else if (op == NLNODE_VAR || op == NLNODE_CST) {
        continue;
      } else {
         info = _nlnode_replacevarbycst(child, vi, pool_idx);
         if (info != OK) { return info; }
      }
   }

   return OK;
}

int nltree_replacevarbycst(NlTree* tree, rhp_idx vi, unsigned pool_idx)
{
   return _nlnode_replacevarbycst(tree->root, vi, pool_idx);
}

static int _nlnode_replacevarbytree(NlNode* node, rhp_idx vidx,
                               NlNode* subnode, NlTree* tree)
{
   int info;
   if (!node) return OK;
   rhp_idx lvidx = VIDX(vidx);

   for (size_t i = 0; i < node->children_max; ++i) {
      if (!node->children[i]) continue;

      unsigned op = node->children[i]->op;

      if (op == NLNODE_VAR) {
         if (node->children[i]->value == lvidx) {
           NlNode *lnode = node->children[i];
           for (unsigned j = 0; j < lnode->children_max; ++j) {
              lnode->children[j] = NULL;
           }
           int len = subnode->children_max - lnode->children_max;
           if (len > 0) {
              nlnode_reserve(tree, lnode, len);
           }
           nlnode_attr_copy(lnode, subnode);
           /* TODO(xhub) when we will support refcnt in node, do that instead
            * of copying  */
           for (unsigned j = 0; j < subnode->children_max; ++j) {
              S_CHECK(nlnode_copy(&lnode->children[j], subnode->children[j], tree));
           }
         } else { continue; }
      } else if (op == NLNODE_CST) { continue;
      } else if (node->oparg == NLNODE_OPARG_VAR) {
         TO_IMPLEMENT("_nlnode_replacevar with variable attached to node is not yet supported");
      } else {
         if ((info = _nlnode_replacevarbytree(node->children[i], vidx, subnode, tree)) != OK) {
            return info;
         }
      }
   }

   return OK;
}

int nltree_replacevarbytree(NlTree* tree, rhp_idx vi, const NlTree* subtree)
{
   assert(tree->root); // TODO check tree->root->op
   /*  Let the party begin */
   NlNode * subnode = subtree->root;
   
   S_CHECK(nltree_reserve(tree, nltree_numnodes(tree), nltree_numchildren(tree)));

   return _nlnode_replacevarbytree(tree->root, vi, subnode, tree);
}

/** @brief Copy the tree and perform some analysis if asked
 *
 *  @param tree         the expression tree to copy
 *  @param var_indices  indices of variable to keep track off. If this is a
 *                      valid pointer, the nodes that has the variable as child
 *                      are saved
 *  @param nb_var       size of var_indices
 *
 *  @return an expression tree, copy of the one given as parameter
 */
NlTree* nltree_dup(const NlTree *tree, const unsigned *var_indices, unsigned nb_var)
{
   if (!tree) return NULL;

   NlTree* copy;
   AA_CHECK(copy, nltree_alloc2(nltree_numnodes(tree), nltree_numchildren(tree)));

   if (!tree->root) return copy;

   if (nb_var > 0 && var_indices) {
      AA_CHECK_EXIT(copy->vt, _vartree_alloc(nb_var, var_indices));
   }

   SN_CHECK_EXIT(nlnode_copy(&copy->root, tree->root, copy))

   if (copy->vt) copy->vt->done = true;

   return copy;

_exit:
   nltree_dealloc(copy);
   return NULL;
}

/** @brief Copy the tree and keep a list of variables for latter analysis.
 *
 *  @param tree         the expression tree to copy
 *  @param rosetta      the translation between the variable indices
 *
 *  @return an expression tree, copy of the one given as parameter
 */
NlTree* nltree_dup_rosetta(const NlTree* restrict tree, const rhp_idx* restrict rosetta)
{
   if (!tree) return NULL;

   NlTree* tree_copy;
   AA_CHECK(tree_copy, nltree_alloc2(nltree_numnodes(tree), nltree_numchildren(tree)));

   if (!tree->root) return tree_copy;

  /* TODO(xhub) TREE this should better handled. free the memory? borrow the memory? */
   SN_CHECK_EXIT(nltree_reset_var_list(tree_copy));

   SN_CHECK_EXIT(nlnode_copy_rosetta(&tree_copy->root, tree->root, tree_copy, rosetta))

   return tree_copy;

_exit:
   nltree_dealloc(tree_copy);
   return NULL;
}


int nltree_apply_rosetta(NlTree *tree, const rhp_idx * restrict rosetta)
{
   if (!tree->root) { return OK; }

   S_CHECK(nltree_reset_var_list(tree));

   return nlnode_apply_rosetta(tree->root, tree->v_list, rosetta);
}

static void _print_node(const NlNode* node, FILE* f, const Container *ctr)
{
   unsigned op = node->op;
   char oparg[512];
   char nodestyle[256];
   const char *varname;

   switch (op) {
      case NLNODE_CST:
         snprintf(oparg, sizeof(oparg), "%u\\n%.2e", node->value,
                  ctr ? ctr->pool->data[CIDX_R(node->value)] : NAN);
         nodestyle[0] = 0;
         break;
      case NLNODE_VAR:
         varname = ctr ? ctr_printvarname(ctr, node->value-1) : "??";
         snprintf(oparg, sizeof(oparg), "%u\\n %s", node->value, varname);
         snprintf(nodestyle, 255, ",style=filled,color=lightblue1");
         break;
      case NLNODE_CALL1:
      case NLNODE_CALL2:
         snprintf(oparg, sizeof(oparg), "(%u): %s", node->value, func_code_name[node->value]);
         snprintf(nodestyle, 255, ",style=filled,color=lightseagreen");
         break;
      case NLNODE_MUL:
         if (node->oparg == NLNODE_OPARG_FMA) {
            snprintf(oparg, sizeof(oparg), "%u\\n%.2e", node->value,
                     ctr ? ctr->pool->data[CIDX_R(node->value)] : NAN);
            snprintf(nodestyle, 255, ",style=filled,color=lightsalmon1");
            break;
         }
      FALLTHRU
      case NLNODE_ADD:
      case NLNODE_SUB:
      case NLNODE_DIV:
         switch (node->oparg) {
         case NLNODE_OPARG_CST:
            snprintf(oparg, sizeof(oparg), "%u\\n%.2e", node->value,
                     ctr ? ctr->pool->data[CIDX_R(node->value)] : NAN);
            nodestyle[0] = 0;
            goto print;
         case NLNODE_OPARG_VAR:
            varname = ctr ? ctr_printvarname(ctr, node->value-1) : "??";
            snprintf(oparg, sizeof(oparg), "%u\\n %s", node->value, varname);
            snprintf(nodestyle, 255, ",style=filled,color=lightblue1");
            goto print;
         case NLNODE_OPARG_FMA:
            snprintf(oparg, sizeof(oparg), "%u", node->value);
            snprintf(nodestyle, 255, ",style=filled,color=lightsalmon1");
            goto print;
         default: ;
         }
      FALLTHRU
      default:
         nodestyle[0] = 0;
         snprintf(oparg, sizeof(oparg), "%u", node->value);
   }
print:
   fprintf(f, " A%p [label=\"%s %s\" %s];\n", (void*)node, opcode_names[op], oparg, nodestyle);
}

static void print_edges(const NlNode* node, FILE* f)
{
   if (!node || node->children_max == 0) return;

   fprintf(f, " A%p -> {", (void*)node);
   bool first = true;
   for (size_t i = 0; i < node->children_max; ++i)
   {
      if (!node->children[i]) continue;
      if (!first) { fputs(", ", f); } else { first = false; }
      fprintf(f, "A%p", (void*)node->children[i]);
   }
   fputs("}\n", f);

   for (size_t i = 0; i < node->children_max; ++i)
   {
      if (!node->children[i]) continue;
      print_edges(node->children[i], f);
   }
}

static void print_node_graph(const NlNode* node, FILE* f, const Container *ctr)
{
   _print_node(node, f, ctr);

   for (size_t i = 0; i < node->children_max; ++i)
   {
      if (!node->children[i]) continue;
      print_node_graph(node->children[i], f, ctr);
   }
}

void nltree_print_dot(const NlTree* tree, const char* fname, const Container *ctr)
{
   UNUSED int status = OK;
   if (!tree || !tree->root) return;

   FILE* f = fopen(fname, "w");

   if (!f) {
      error("%s :: Could not create file named '%s'\n", __func__, fname);
      return;
   }

   IO_CALL_EXIT(fputs("digraph structs {\n node [shape=record];\n", f));

   print_node_graph(tree->root, f, ctr);

   print_edges(tree->root, f);

   IO_CALL_EXIT(fputs("label=\"NlTree for equation ", f));
   IO_CALL_EXIT(fputs(ctr_printequname(ctr, tree->idx), f));
   IO_CALL_EXIT(fputs("\"\n", f));

   IO_CALL_EXIT(fputs("}", f));

_exit:
   SYS_CALL(fclose(f));
}

int nltree_alloc_var_list(NlTree* tree)
{
#define INITIAL_MAX 10

   if (!tree->v_list) {
      MALLOC_(tree->v_list, struct vlist, 1);
      tree->v_list->idx = 0;
      tree->v_list->max = INITIAL_MAX;
      MALLOC_(tree->v_list->pool, int, INITIAL_MAX);
   }

   return OK;
}

/** 
 *  @brief Reset the variable list 
 *
 *  This is useful if we want to keep track of the variables added by copying
 *  nodes into a tree (via nlnode_copy)
 *
 *  @param tree  the tree
 *
 *  @return      the error code
 */
int nltree_reset_var_list(NlTree* tree)
{
#define INITIAL_MAX 10

   if (!tree->v_list) {
      MALLOC_(tree->v_list, struct vlist, 1);
      tree->v_list->max = INITIAL_MAX;
      MALLOC_(tree->v_list->pool, int, INITIAL_MAX);
   }

   /* ----------------------------------------------------------------------
    *  Reset it to zero since we are going to use that for adding variables to
    *  the equation in the model representation 
    * ---------------------------------------------------------------------- */

   tree->v_list->idx = 0;

   return OK;
}

/**
 *  @brief Add a multiplicative constant.
 *
 *   This is a wrapper around nltree_mul_cst_x, which can be use is it is 
 *   important to know whether a node has been added.
 *
 *  @param ctr            the container
 *  @param tree           the equation tree
 *  @param[in,out] node   the current node
 *  @param[in,out] coeff  the coefficient to use in the multiplication
 *
 *  @return               the error code
 */
int nltree_mul_cst(NlTree* tree, NlNode ***node, NlPool *pool, double coeff)
{
   bool dummy;
   return nltree_mul_cst_x(tree, node, pool, coeff, &dummy);
}

/**
 *  @brief Add a multiplicative constant
 *
 *  Node must point to an empty object
 *
 *  @param         ctr       the container
 *  @param         tree      the equation tree
 *  @param[in,out] node      the current node
 *  @param[in,out] coeff     the coefficient to use in the multiplication
 *  @param[out]    new_node  on exit, set to true if a new node has been created.
 *                           No new node are created if coeff is set to 1.
 *
 *  @return                  the error code
 */
int nltree_mul_cst_x(NlTree* tree, NlNode ***node, NlPool *pool, double coeff,
                     bool *new_node)
{
   NlNode *lnode;
   assert(node && *node);
   assert(isfinite(coeff));

   /* TODO(xhub) remove that and use assert  */
   if (**node) {
      error("%s :: unexpected non-null node\n", __func__);
      nlnode_print(**node, PO_ERROR, true);
      *new_node = false;
      return Error_UnExpectedData;
   }

   if (fabs(coeff - 1.) < DBL_EPSILON) {
     *new_node = false;
      return OK;
   }

   if (fabs(coeff + 1.) < DBL_EPSILON) {
     *new_node = true;
      S_CHECK(nltree_umin(tree, node));
      return OK;
   }

   /* ----------------------------------------------------------------------
    * Here we have a non trivial constant
    * ---------------------------------------------------------------------- */

   A_CHECK(lnode, nlnode_alloc_fixed_init(tree, 1));
   (**node) = lnode;
   lnode->op = NLNODE_MUL;
   lnode->oparg = NLNODE_OPARG_CST;
   unsigned idx = pool_getidx(pool, coeff);
   if (idx == UINT_MAX) {
      return -Error_InsufficientMemory;
   }

   lnode->value = idx;

   *node = &lnode->children[0];

   *new_node = true;

   assert(*node);
   return OK;
}

/**
 *  @brief Multiply the whole tree by a constant
 *
 *  @param ctr            the container
 *  @param tree           the equation tree
 *
 *  @return               the error code
 */
int nltree_scal(Container *ctr, NlTree* tree, double coeff)
{
   assert(isfinite(coeff));
   assert(tree->root);

   if (fabs(coeff - 1.) < DBL_EPSILON) { return OK; }

   if (fabs(coeff + 1.) < DBL_EPSILON) {
     return nltree_scal_umin(ctr, tree);
   }

   NlNode * restrict root = tree->root;

   /* ----------------------------------------------------------------------
    * We do not aggressively pursue doing the minimal change in the 
    * ---------------------------------------------------------------------- */

   switch (root->op) {
   case NLNODE_UMIN:
      if (root->oparg == NLNODE_OPARG_UNSET) {
         nlnode_default(root, NLNODE_MUL);
         unsigned pidx = rctr_poolidx(ctr, -coeff);
         if (pidx == UINT_MAX) { return -Error_InsufficientMemory; }
         root->value = pidx;
         return OK;
      }
   FALLTHRU
   default:
      A_CHECK(tree->root, nlnode_alloc_fixed(tree, 1));
      nlnode_default(tree->root, NLNODE_MUL);
      unsigned pidx = rctr_poolidx(ctr, coeff);
      if (pidx == UINT_MAX) { return -Error_InsufficientMemory; }
      tree->root->value = pidx;
      tree->root->children[0] = root;
   }

   return OK;
}

int nltree_scal_umin(Container *ctr, NlTree *tree)
{
   NlNode * restrict root = tree->root;

   switch (root->op) {
   case NLNODE_UMIN:
      if (root->oparg == NLNODE_OPARG_VAR) {
         root->op = NLNODE_VAR;
         root->oparg = NLNODE_OPARG_UNSET;
      } else {
         tree->root = root->children[0];
      }
      return OK;
   case NLNODE_VAR:
      root->op = NLNODE_UMIN;
      root->oparg = NLNODE_OPARG_VAR;
      return OK;
   default:
      A_CHECK(tree->root, nlnode_alloc_fixed(tree, 1));
      nlnode_default(tree->root, NLNODE_UMIN);
      tree->root->children[0] = root;
   }

   return OK;
}

/**
 *  @brief Add a multiplicative constant to a sum inside a sum, without
 *         introducing unnecessary ADD node.
 *
 *  @param         ctr    the container
 *  @param         tree   the equation tree
 *  @param[in,out] node   the current ADD node
 *  @param[in,out] coeff  the coefficient to use in the multiplication
 *  @param         size   the estimate on the number of children
 *  @param[in,out] idx    on input, the index of the first children of the node
 *                        that is free. On output, the index of the first children
 *                        of the node that is available.
 *
 *  @return              the error code
 */
int nltree_mul_cst_add_node(NlTree* tree, NlNode ***node, NlPool *pool,
                            double coeff, unsigned size,
                             unsigned *idx)
{
   assert(node && *node && **node
          && "the node argument must point to a valid node object");
   assert((**node)->op == NLNODE_ADD);

   NlNode **caddr = &(**node)->children[*idx];
   assert(!*caddr);

   bool new_node;
   S_CHECK(nltree_mul_cst_x(tree, &caddr, pool, coeff, &new_node));

   if (new_node) {
      *node = caddr;
      S_CHECK(nltree_reserve_add_node(tree, caddr, size, idx));
   } else {
      unsigned dummy_offset;
      S_CHECK(nltree_reserve_add_node(tree, *node, size-1, &dummy_offset));
   }

   return OK;
}

/** 
 *  @brief Add a bilinear term \f$ c v_1 v_2\f$.
 *
 * @ingroup EquSafeEditing
 *
 *  @param          ctr    the container object
 *  @param          tree   the expression tree
 *  @param[in,out]  node   the pointer to a node address. Modified to point on
 *                         the second node of the mul if v2 is not variable index.
 *  @param          coeff  the coefficient
 *  @param          v1     the first variable index. If invalid, v2 must also be invalid.
 *  @param          v2     If a variable index, this function completes the bilinear
 *                         product. Otherwise, the second term is left untouched.
 *
 *  @return                the error code
 */
int rctr_nltree_add_bilin(Container *ctr, NlTree* tree, NlNode ** restrict *node,
                          double coeff, rhp_idx v1, rhp_idx v2)
{
   NlNode *lnode, **addr;

   bool v2valid = valid_vi(v2);
   if (!valid_vi(v1)) {
      if (v2valid) {
         errormsg("[nltree] ERROR in nltree_add_bilin, the first variable argument"
                  "is not valid, but the second one is. This is not supported.\n");
         return Error_RuntimeError;
      }

      A_CHECK(lnode, nlnode_alloc_fixed_init(tree, 2));
      nlnode_default(lnode, NLNODE_MUL);

      if (coeff == -1.) {
         S_CHECK(nltree_umin(tree, node));
         (**node)->children[0] = lnode;
      } else if (coeff != 1.) {
         S_CHECK(nlnode_mulcst(ctr, lnode, coeff));
      }

      **node = lnode;

      return OK;
   }

   /* Add the mul for two variables */
   if (coeff == 1.) {
      A_CHECK(lnode, nlnode_alloc_fixed_init(tree, 1));
      /* IMPORTANT: the node assignment must happen as soon as possible  */
      (**node) = lnode;
      lnode->op = NLNODE_MUL;
      lnode->oparg = NLNODE_OPARG_VAR;
      lnode->value = VIDX(v1);
      /* We need to manually add the variable in the model */
      S_CHECK(cmat_equ_add_nlvar(ctr, tree->idx, v1, NAN));
   } else {
      A_CHECK(lnode, nlnode_alloc_fixed_init(tree, 2));
      /* TODO(xhub) if coeff != 1., store it there?  */
      nlnode_default(lnode, NLNODE_MUL);
      /* IMPORTANT: the node assignment must happen as soon as possible  */
      (**node) = lnode;

      /* Add the first variable  */
      addr = &lnode->children[1];
      S_CHECK(rctr_nltree_var(ctr, tree, &addr, v1, coeff));
   }

   /* Add the second variable if it is given, and the node is null. Otherwise modify node */
   if (v2valid) {
      addr = &lnode->children[0];
      S_CHECK(rctr_nltree_var(ctr, tree, &addr, v2, 1.));
      *node = NULL;
   } else {
      *node = &lnode->children[0];
   }

   return OK;
}

/**
 * @brief Add a bilinear term <v1, v2>
 *
 * @param ctr     the model
 * @param tree    the tree
 * @param node    an ADD node
 * @param offset  the offset of the first available children in the node
 * @param v1      the first variable
 * @param v2      the second variable
 *
 * @return        the error code
 */
int rctr_nltree_add_bilin_vars(Container *ctr, NlTree *tree, NlNode *node,
                               unsigned offset, Avar * restrict v1, Avar * restrict v2)
{
   /* ------------------------------------------------------------------
    * We already have an ADD node in node
    *
    * 1. Reserve the space of all the elements
    * 2. Add all the x_i * x_j terms
    * ------------------------------------------------------------------*/

   size_t vlen = v1->size;
   if (v2->size != vlen) {
      error("%s :: inconsistent size for the variables: %u vs %u\n",
                         __func__, v1->size, v2->size);
      return Error_DimensionDifferent;
   }

   for (size_t i = 0, cur_idx = offset; i < vlen; ++i, ++cur_idx) {
      NlNode **addr = &node->children[cur_idx];
      rhp_idx vi1 = avar_fget(v1, i);
      rhp_idx vi2 = avar_fget(v2, i);

      /* ---------------------------------------------------------------
       * If v1 == v2 the term is v_ii x_i^2
       * ---------------------------------------------------------------*/

      if (vi1 == vi2) {
         S_CHECK(rctr_nltree_add_sqr(ctr, tree, &addr, vi1));
         S_CHECK(nlnode_print_now(node->children[cur_idx]));
      } else {
      /* ---------------------------------------------------------------
       * This term is v_ij x_i x_j
       * ---------------------------------------------------------------*/

         S_CHECK(rctr_nltree_add_bilin(ctr, tree, &addr, 1., vi1, vi2));
      }
   }

   return OK;
}

int rctr_nltree_opcall1(Container *ctr, NlTree* tree,
                    NlNode ***node, rhp_idx vi, unsigned fnidx)
{
   NlNode *lnode;
   A_CHECK(lnode, nlnode_alloc_fixed(tree, 1));
   (**node) = lnode;
   lnode->op = NLNODE_CALL1;
   lnode->value = fnidx;
   lnode->oparg = NLNODE_OPARG_UNSET;

   NlNode * lnode_child;
   A_CHECK(lnode_child, nlnode_alloc_nochild(tree));
   nlnode_setvar(lnode_child, vi);

   lnode->children[0] = lnode_child;

   *node = NULL;

   S_CHECK(cmat_equ_add_nlvar(ctr, tree->idx, vi, NAN));

   return OK;
}

/**
 * @brief  Add a linear expression c * ( sum a_i x_i )
 *
 * @ingroup EquSafeEditing
 *
 * @param ctr      the container object
 * @param tree     the expression tree
 * @param node     the node to start from
 * @param lequ     the linear equation
 * @param vi_no    if valid, index variable that should not be added to the tree
 * @param coeffp   the multiplicative coefficient
 *
 * @return           the error code
 */
int rctr_nltree_add_lin_term(Container *ctr, NlTree* tree,
                             NlNode ** restrict *node, Lequ *lequ,
                             rhp_idx vi_no, double coeffp)
{
    /* Add the node */
   NlNode *lnode;
   unsigned offset = 0;
   unsigned size = valid_vi(vi_no) ? lequ->len : lequ->len + 1;

   S_CHECK(nltree_reserve_add_node(tree, *node, size, &offset));
   lnode = **node;

   /* TODO(xhub) reintrooduce FMA?*/
   unsigned k = offset;

    for (unsigned l = 0; l < lequ->len; ++l) {
       /*  Do not use the placeholder variable */
       if (lequ->vis[l] == vi_no || !isfinite(lequ->coeffs[l])) continue;

       NlNode **addr = &lnode->children[k];
       S_CHECK(rctr_nltree_var(ctr, tree, &addr, lequ->vis[l], coeffp*lequ->coeffs[l]));

      k++;
    }

    assert(k < lnode->children_max);
//   *node = &lnode->children[k];

    /*  TODO(xhub) this should be unnecessary */
    for (; k < lnode->children_max; ++k) {
       lnode->children[k] = NULL;
    }

    Avar v;
    avar_setlist(&v, lequ->len, lequ->vis);

    /*  TODO(Xhub) should we also pass coeffp? */
    S_CHECK(cmat_equ_add_vars_excpt(ctr, tree->idx, &v, vi_no, lequ->coeffs, true));


   /* Make sure that we don't have just one child for the node  */
   /* TODO(xhub) do we need this?  */
//   S_CHECK(nltree_check_add(lnode));

   return OK;
}

/**
 *  @brief  add a constant to an expression
 *
 *  - If node points to an existing object, it as to be of the ADD type. Then we
 *    try to transform the node into an ADDi. If this is not possible, then we
 *    add a child
 *
 *  - If node points to a null object, create a ADDi node 
 *
 *  On exit, if node was empty on input, it points to the new object
 *
 *  @param ctr   the container
 *  @param tree  the tree
 *  @param node  the current node
 *  @param cst   the constant
 *
 *  @return      the error code
 */
int nltree_add_cst(Container *ctr, NlTree* tree, NlNode ***node,
                    double cst)
{
   assert(tree && node && *node);
   NlNode *lnode;
   unsigned pool_idx = rctr_poolidx(ctr, cst);

   if (pool_idx == UINT_MAX) { return -Error_InsufficientMemory; }

   /* If **node exists (and is of the right type, then we */
   if (**node) {
      lnode = **node;
      assert(lnode->op == NLNODE_ADD || lnode->op == __OPCODE_LEN);

      if (lnode->op == __OPCODE_LEN) {
         nlnode_default(lnode, NLNODE_ADD);
      }

      if (lnode->op != NLNODE_ADD) {
         error("%s :: node is of type %d, which is not OPCODE_ADD\n",
                  __func__, lnode->op);
         return Error_Inconsistency;
      }

     /* ---------------------------------------------------------------------
      * First try to put the constant as a property. If this fails, look for a
      * child
      * --------------------------------------------------------------------- */

      if (lnode->oparg == NLNODE_OPARG_UNSET) {
         assert(lnode->value == 0);
         lnode->oparg = NLNODE_OPARG_CST;
         lnode->value = pool_idx;
      } else {
         unsigned offset = 0;
         /* TODO(xhub) improve the search  */
         while (offset < lnode->children_max && lnode->children[offset]) {
           offset++;
         }

         if (offset == lnode->children_max) {
           /*  TODO(xhub) check constant */
            S_CHECK(nlnode_reserve(tree, lnode, 3));
         }

         A_CHECK(lnode->children[offset], nlnode_alloc_nochild(tree));

         lnode->children[offset]->op = NLNODE_CST;
         lnode->children[offset]->oparg = NLNODE_OPARG_CST;
         /* TODO(Xhub) this node is not of the right type, and we loose some
          * memory, but oh well  */
   //      lnode->children[offset]->children_max = 0 ;
         lnode->value = pool_idx;
      }
   } else {
      A_CHECK(lnode, nlnode_alloc_fixed_init(tree, 1));
      (**node) = lnode;
      lnode->op = NLNODE_ADD;
      lnode->oparg = NLNODE_OPARG_CST;
      lnode->value = pool_idx;

      *node = &lnode->children[0];
   }

   assert((lnode->op == NLNODE_CST && lnode->children_max == 0) || lnode->oparg == NLNODE_OPARG_CST);

   return OK;

}

int nltree_add_var(Container *ctr, NlTree* tree, NlNode ***node, rhp_idx vi,
                   double coeff)
{
   NlNode *lnode;
   assert(valid_vi(vi));
   assert(tree && node && *node && !**node);
   bool need_umin;
   bool need_mulc;

   if (coeff == 1.) {
      need_umin = false;
      need_mulc = false;
   } else if (coeff == -1.) {
      need_umin = true;
      need_mulc = false;
   } else {
      need_umin = false;
      need_mulc = true;
   }

   /* TODO: URG this looks quite wrong: the above assert implies that we want *node to be NULL ... */
//   if (**node) {
//      lnode = **node;
//      assert(lnode->op == OPCODE_ADD);
//      if (!need_umin && !need_mulc && lnode->optype == OPTYPE_OTHER
//          && lnode->value == 0) {
//         lnode->optype = NLNODE_OPARG_VAR;
//         lnode->value = VIDX(vi);
//         /* TODO(xhub) This is super inefficient if this is the last term to be
//          * added */
//         unsigned offset = 0;
//         while (offset < lnode->children_max && lnode->children[offset]) {
//            offset++;
//         }
//         if (offset == lnode->children_max) {
//            S_CHECK(nlnode_reserve(tree, lnode, 3));
//         }
//         *node = &lnode->children[offset];;
//         /* We just added the var, no need to allocate  */
//         goto _add_var_in_model;
//      } else {
//         unsigned offset = 0;
//         /* TODO(xhub) improve the search  */
//         while (offset < lnode->children_max && lnode->children[offset]) {
//            offset++;
//         }
//         if (offset == lnode->children_max) {
//           /*  TODO(xhub) check constant */
//            S_CHECK(nlnode_reserve(tree, lnode, 3));
//         }
//         *node = &lnode->children[offset];;
//         TO_IMPLEMENT("This requires some checking");
//      }
//   } else {
   /* ----------------------------------------------------------------------
    * The node does not exists. Hence, we create one
    * ---------------------------------------------------------------------- */

   A_CHECK(lnode, nlnode_alloc_init(tree, 1));
   (**node) = lnode;
   if (!need_umin && !need_mulc) {
      lnode->op = NLNODE_ADD;
      lnode->oparg = NLNODE_OPARG_VAR;
      lnode->value = VIDX(vi);
      goto _chain_node;
   }
   //   }

   if (need_umin) {
     TO_IMPLEMENT("This requires some checking");
     //NlNode **addr = &lnode;
     //S_CHECK(nltree_umin(tree, &addr));
     //lnode = *addr;
   } else if (need_mulc) {
     TO_IMPLEMENT("This requires some checking");
     //NlNode **addr = &lnode;
     //S_CHECK(nltree_mul_cst(ctr, tree, &addr, coeff));
     //lnode = *addr;
   }

   /* Add the variable  */
   assert(lnode);
   lnode->op = NLNODE_VAR;
   lnode->oparg = NLNODE_OPARG_UNSET;
   lnode->value = VIDX(vi);

_chain_node:
   assert(lnode->op == NLNODE_ADD && lnode->children_max > 0 && !lnode->children[0]);
   *node = &lnode->children[0];

   S_CHECK(cmat_equ_add_nlvar(ctr, tree->idx, vi, NAN));

   return OK;
}

/** 
 *  @brief  Add a variable in the tree
 *
 *  - If node points to an existing object, it as to be of the ADD type. Then we
 *    try to transform the node into an ADDi. If this is not possible, then we
 *    add a child
 *
 *  - If node points to a null object, create a ADDi node 
 *
 *  On exit, if node was empty on input, it points to the new object
 *
 * @ingroup EquSafeEditing
 *
 *  @param ctr    the container
 *  @param tree   the tree
 *  @param node   the current node
 *  @param vi     the variable
 *  @param coeff  the constant
 *
 *  @return       the error code
 */
int rctr_nltree_var(Container *ctr, NlTree* tree, NlNode ***node, rhp_idx vi,
               double coeff)
{
   NlNode *lnode;
   assert(valid_vi(vi));
   assert(tree && node && *node && !(**node));

   S_CHECK(nltree_mul_cst(tree, node, ctr->pool, coeff));

   A_CHECK(lnode, nlnode_alloc_nochild(tree));
   (**node) = lnode;
   lnode->op = NLNODE_VAR;
   lnode->oparg = NLNODE_OPARG_UNSET;
   lnode->value = VIDX(vi);

   *node = NULL;

   double val = NAN;
   S_CHECK(cmat_equ_add_nlvar(ctr, tree->idx, vi, val));

   return OK;
}

/** @brief ensure that the node is of the ADD type and has enough space for
 *         a number of children nodes. Returns also the offset for the first
 *         available child
 *
 *  @param         tree    the tree
 *  @param[in,out] node    On input, the address of the current node. On output,
 *                         the address of the ADD node
 *  @param         size    the number of additional children for the node
 *  @param[out]    offset  the index of the first available child
 *
 *
 */
int nltree_reserve_add_node(NlTree *tree, NlNode **node,
                             unsigned size, unsigned *offset)
{
   NlNode *lnode = *node;
   if (lnode) {
      if (lnode->op == __OPCODE_LEN) {
         nlnode_default(lnode, NLNODE_ADD);
         S_CHECK(nlnode_reserve(tree, lnode, size));
         *offset = 0;
      } else if (lnode->op == NLNODE_ADD) {
         /* ----------------------------------------------------------------
          * This is a bit dangerous since we are going to change a node that we
          * may want to reference later. This requires a thoro solution of not
          * asking for children directly,  via the array, put via a function
          * that return the next available child. This could be implemented for
          * dynamic node, while static node (with fixed number of children,
          * would still be allowed to have direct access of their children via
          * the array
          * ---------------------------------------------------------------- */

         *offset = lnode->children_max;
         S_CHECK(nlnode_reserve(tree, lnode, size));
      } else {
         NlNode *add_node;
         A_CHECK(add_node, nlnode_alloc_init(tree, size+1));
         nlnode_default(add_node, NLNODE_ADD);
         add_node->children[0] = lnode;
         *offset = 1;
         *node = add_node;
      }
   } else {
      A_CHECK(*node, nlnode_alloc_init(tree, size));
      nlnode_default(*node, NLNODE_ADD);
      *offset = 0;
   }

   return OK;
}

/** 
 *  @brief add a variable to a NL expression
 *
 *  @param ctr   the container
 *  @param tree  the expression tree
 *  @param vi    the variable
 *  @param val   the coefficient for the variable
 *
 *  @return      the error code
 */
int nltree_add_var_tree(Container *ctr, NlTree *tree, rhp_idx vi, double val)
{
   NlNode **addr;

   double lval = val;
   S_CHECK(nltree_find_add_node(tree, &addr, ctr->pool, &lval));

   unsigned offset;
   S_CHECK(nltree_reserve_add_node(tree, addr, 1, &offset));

   addr = &(*addr)->children[offset];

   /* Put the VAR node in */
   S_CHECK(rctr_nltree_var(ctr, tree, &addr, vi, lval));

   return OK;
}

/* -----------------------------------------------------------------------
 * Deal with the linear part
 *
 *  We add -F(x), so either -var if a variable was given, or -<c,x>
 * ----------------------------------------------------------------------- */

static int nltree_add_expr_common(NlTree *tree, const NlNode *node,
                                  NlPool *pool, NlNode **out_node,
                                  unsigned *node_pos, unsigned *offset, double cst)
{

   /* TODO(xhub) fix this: it is suboptimal */
   if (!tree->root) {
      A_CHECK(tree->root, nlnode_alloc_init(tree, 1));
      tree->root->op = __OPCODE_LEN;
      tree->root->oparg = NLNODE_OPARG_UNSET;
      tree->root->value = 0;
   }

   S_CHECK(nltree_reset_var_list(tree));

   NlNode **add_node = &tree->root;
   double lcst = cst;
   unsigned children_from_node;

   S_CHECK(nltree_find_add_node(tree, &add_node, pool, &lcst));

   if (node->op == NLNODE_ADD) {
      /* TODO(xhub) fix it! children_max is not what we want */
      children_from_node = node->children_max;
   } else {
      children_from_node = 1;
   }

   S_CHECK(nlnode_child_getoffset(tree, *add_node, offset));

   /* ----------------------------------------------------------------------
    * Take into account the coefficient that defined F(x). If it is not finite,
    * it indicates that it does not exists, so skip that part
    * ---------------------------------------------------------------------- */

   if (isfinite(lcst)) {
     S_CHECK(nltree_mul_cst_add_node(tree, &add_node, pool, lcst,
                                     children_from_node, offset));
   } else {
     S_CHECK(nltree_reserve_add_node(tree, add_node, children_from_node, offset));
   }

   *out_node = *add_node;
   *node_pos = children_from_node;

   return OK;
}


/**
 *  @brief Add a nonlinear expression to a tree
 *
 *  @param ctr   the container
 *  @param tree  the expression to modify
 *  @param node  the root node of the expression
 *  @param cst   a multiplicative coefficient for the nonlinear expression
 *
 *  @return      the error code
 */
int nltree_add_nlexpr(NlTree *tree, NlNode *node, NlPool *pool, double cst)
{
   NlNode *lnode;
   unsigned children_from_node, offset = 0;

   S_CHECK(nltree_add_expr_common(tree, node, pool, &lnode, &children_from_node, &offset, cst));

   /* ----------------------------------------------------------------------
    * If the new node is ADD, then we can just add the new node
    * TODO(Xhub) SUB would also work with a umin on the children
    * ---------------------------------------------------------------------- */

   if (node->op == NLNODE_ADD) {
      for (unsigned i = 0; i < children_from_node; ++i) {
         if (!node->children[i]) { continue; }
         S_CHECK(nlnode_copy(&lnode->children[offset], node->children[i], tree));
         offset++;
      }
   } else {
      S_CHECK(nlnode_copy(&lnode->children[offset], node, tree));
   }

   return nltree_check_add(lnode);
}

/**
 *  @brief Add a nonlinear expression to an equation
 *
 *  This function updates the container matrix
 *
 *  @param ctr   the container
 *  @param tree  the expression to modify
 *  @param node  the root node of the expression
 *  @param cst   a multiplicative coefficient for the nonlinear expression
 *
 *  @return      the error code
 */
int rctr_equ_add_nlexpr(Container *ctr, rhp_idx ei, NlNode *node, double cst)
{
   if (!ctr->equs[ei].tree) {
      S_CHECK(nltree_bootstrap(&ctr->equs[ei], 10, 30));
   }

   NlTree *tree = ctr->equs[ei].tree;

   S_CHECK(nltree_add_nlexpr(tree, node, ctr->pool, cst));

   /* update the container matrix */
   Avar v;
   avar_setlist(&v, tree->v_list->idx, tree->v_list->pool);
   S_CHECK(cmat_equ_add_vars(ctr, tree->idx, &v, NULL, true));

   return OK;
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
static inline int nlnode_reserve_and_getoffset(NlTree *tree,
                                                NlNode *node,
                                                unsigned nchildren,
                                                unsigned *offset)
{
   assert(node && tree);
   unsigned loffset = 0;

   /* TODO(xhub) improve the search  */
   while (loffset < node->children_max && node->children[loffset]) {
      loffset++;
   }

   unsigned free_slots = node->children_max - loffset;
   if (free_slots < nchildren) {
      S_CHECK(nlnode_reserve(tree, node, nchildren-free_slots));
   }

   *offset = loffset;

   return OK;
}

/**
 *  @brief Add an expression to a tree
 *
 *  This function updates the container matrix
 *
 * @ingroup EquSafeEditing
 *
 *  @param ctr   the container
 *  @param tree  the expression to modify
 *  @param node  the root node of the expression
 *  @param cst   if finite, coefficient of the variable that defines the equation:
 *               \f$ cst v + F(x) =R= rhs \f$; otherwise assume that node points
 *               to the root node of the expression tree for F.
 *
 *  @return      the error code
 */
int rctr_nltree_copy_to(Container *ctr, NlTree *tree, NlNode **dstnode, NlNode *srcnode,
                        double cst)
{
   /* TODO: unused */

   unsigned offset = 0;

   if (!*dstnode) {
      S_CHECK(nlnode_copy(dstnode, srcnode, tree));

   } else {

   /* ----------------------------------------------------------------------
    * If srcnode is ADD, then we can just add the new node
    * TODO(Xhub) SUB would also work with a umin on the children
    * ---------------------------------------------------------------------- */
      NlNode *lnode = *dstnode;
      assert(lnode->op == NLNODE_ADD);

      unsigned nchildren = srcnode->op == NLNODE_ADD ? srcnode->children_max : 1;

      /* ----------------------------------------------------------------------
       * Take into account the coefficient that defined F(x). If it is not finite,
       * it indicates that it does not exists, so skip that part
       * ---------------------------------------------------------------------- */

      if (srcnode->op == NLNODE_ADD) {

         NlNode **add_node = dstnode;
         if (isfinite(cst)) {
            S_CHECK(nltree_mul_cst_add_node(tree, &add_node, ctr->pool, cst,
                                             nchildren, &offset));
         } else {
            S_CHECK(nltree_reserve_add_node(tree, add_node, nchildren, &offset));
         }

         lnode = *add_node;
         for (unsigned i = 0, len = nchildren; i < len; ++i) {
            if (!srcnode->children[i]) { continue; }
            S_CHECK(nlnode_copy(&lnode->children[offset], srcnode->children[i], tree));
            offset++;
         }

      } else {

         S_CHECK(nlnode_reserve_and_getoffset(tree, lnode, nchildren, &offset));

         S_CHECK(nlnode_copy(&lnode->children[offset], srcnode, tree));
      }

      S_CHECK(nltree_check_add(lnode));

   }

   /* keep the model representation consistent */
   Avar v;
   avar_setlist(&v, tree->v_list->idx, tree->v_list->pool);
   S_CHECK(cmat_equ_add_vars(ctr, tree->idx, &v, NULL, true));

   return OK;
}

/**
 *  @brief Add an expression to a tree, with a possible translation.
 *
 *  This function keeps the model representation consistent
 *
 *  @param ctr      the container
 *  @param tree     the expression to modify
 *  @param node     the root node of the expression
 *  @param rosetta  translate the variable index
 *
 *  @return         the error code
 */
int rctr_equ_add_nlexpr_full(Container *ctr, NlTree *tree, const NlNode *node,
                          double coeff, const rhp_idx* rosetta)
{
   NlNode *lnode;
   unsigned children_from_node, offset = 0;

   S_CHECK(nltree_add_expr_common(tree, node, ctr->pool, &lnode,
                                  &children_from_node, &offset, coeff));

   /* ----------------------------------------------------------------------
    * If the new node is ADD, then we can just add the new node
    * TODO(Xhub) SUB would also work with a umin on the children
    * ---------------------------------------------------------------------- */

   if (node->op == NLNODE_ADD) {
      for (unsigned i = 0; i < children_from_node; ++i) {
         if (!node->children[i]) { continue; }
         S_CHECK(nlnode_copy_rosetta(&lnode->children[offset], node->children[i], tree, rosetta));
         offset++;
      }
   } else {
      S_CHECK(nlnode_copy_rosetta(&lnode->children[offset], node, tree, rosetta));
   }

   /* keep the model representation consistent */
   if (ctr) {
      Avar v;
      avar_setlist(&v, tree->v_list->idx, tree->v_list->pool);
      S_CHECK(cmat_equ_add_vars(ctr, tree->idx, &v, NULL, true));
   }

   S_CHECK(nltree_check_add(lnode));

   return OK;
}

/**
 *  @brief create a empty tree with an ADD root node 
 *
 *  @param e                         the equation
 *  @param n_nodes_estimate          estimation of the tree size
 *  @param n_add_children_estimate   estimation of the number of children for the root node
 *
 *  @return          the error code
 */
int nltree_bootstrap(Equ *e, unsigned n_nodes_estimate,
                     unsigned n_add_children_estimate)
{
   if (e->tree) {
      errormsg("[nltree] ERROR: bootstrapping an existing nltree!\n");
      return Error_RuntimeError;
   }

   A_CHECK(e->tree, nltree_alloc(n_nodes_estimate));

   e->tree->idx = e->idx;
   e->tree->root = nlnode_alloc_init(e->tree, n_add_children_estimate);
   nlnode_default(e->tree->root, NLNODE_ADD);

   return OK;
}


static int _check_math_error2(unsigned fn_code, double x1, double x2)
{
   if (errno || fetestexcept(FE_INVALID | FE_DIVBYZERO | FE_OVERFLOW 
         | FE_UNDERFLOW)) {
      if (errno == EDOM || fetestexcept(FE_INVALID)) {
         error("[nlequ eval] Domain error for function %s and argument (%e,%e)\n",
                  func_code_name[fn_code], x1, x2);
      } else if (fetestexcept(FE_DIVBYZERO)) {
         error("[nlequ eval] Pole error for function %s and argument (%e,%e)\n",
                  func_code_name[fn_code], x1, x2);
      } else if (fetestexcept(FE_OVERFLOW)) {
         error("[nlequ eval] Overflow error for function %s and argument (%e,%e)\n",
                  func_code_name[fn_code], x1, x2);
      } else if (fetestexcept(FE_UNDERFLOW)) {
         error("[nlequ eval] Underflow error for function %s and argument (%e,%e)\n",
                  func_code_name[fn_code], x1, x2);
      } else if (errno == ERANGE) {
         error("[nlequ eval] Range error for function %s and argument (%e,%e)\n",
                  func_code_name[fn_code], x1, x2);
      } else {
         error("%s :: Unexpected value of errno: %d\n", __func__,
                  errno);
         return Error_SystemError;
      }
      return Error_MathError;
   }
   return OK;
}

static int _check_math_error1(unsigned fn_code, double x1)
{
   if (errno || fetestexcept(FE_INVALID | FE_DIVBYZERO | FE_OVERFLOW |
                             FE_UNDERFLOW)) {
      if (errno == EDOM || fetestexcept(FE_INVALID)) {
         error("[nlequ eval] Domain error for function %s and argument %e\n",
               func_code_name[fn_code], x1);
      } else if (fetestexcept(FE_DIVBYZERO)) {
         error("[nlequ eval] Pole error for function %s and argument %e\n",
               func_code_name[fn_code], x1);
      } else if (fetestexcept(FE_OVERFLOW)) {
         error("[nlequ eval] Overflow error for function %s and argument %e\n",
               func_code_name[fn_code], x1);
      } else if (fetestexcept(FE_UNDERFLOW)) {
         error("[nlequ eval] Underflow error for function %s and argument %e\n",
               func_code_name[fn_code], x1);
      } else if (errno == ERANGE) {
         error("[nlequ eval] Range error for function %s and argument %e\n",
               func_code_name[fn_code], x1);
      } else {
         error("%s :: Unexpected value of errno: %d\n", __func__, errno);
         return Error_SystemError;
      }
      return Error_MathError;
   }
   return OK;
}

#undef EVAL_NAME
#undef GETVALUE_NAME
#undef EVAL_DEF
#undef GETVALUE_DEF
#undef POOL_VAL
#undef VAR_VAL
#undef GET_VALUE
#undef EVAL
#undef _DPRINT_VAR

#define EVAL_NAME _evalctr
#define GETVALUE_NAME _get_valuectr

#define EVAL_DEF(NODE, VAL) \
  static int EVAL_NAME(NODE, const Container * restrict ctr, VAL)
#define GETVALUE_DEF(NODE, VAL) \
  static inline int GETVALUE_NAME(NODE, const Container * restrict ctr, VAL)

#define POOL_VAL(IDX) ctr->pool->data[CIDX_R(IDX)]
#define VAR_VAL(IDX) ctr->vars[VIDX_R(IDX)].value
#define GET_VALUE(NODE, VAL) GETVALUE_NAME(NODE, ctr, VAL)
#define EVAL(NODE, VAL) EVAL_NAME(NODE, ctr, VAL)

#define _DPRINT_VAR(IDX, VAL) \
  DPRINT("%s :: var %s (%d) = %e\n", __func__, \
  ctr_printvarname(ctr, VIDX_R(IDX)), VIDX_R(IDX), VAL)

#include "nltree_eval.inc"

/**
 *  @brief Evaluate an expression tree
 *
 *  @param ctr   the container
 *  @param tree  the expression tree
 *  @param val   the result of the computation
 *
 *  @return      the error code
 */
int nltree_eval(Container *ctr, NlTree *tree, double *val)
{
   *val = 0.;

   /*  TODO(xhub) change whenever umin is done */
   if (tree->root && tree->root->op < __OPCODE_LEN) {
      return EVAL_NAME(tree->root, ctr, val);
   } 

   return OK;
}

#undef EVAL_NAME
#undef GETVALUE_NAME
#undef EVAL_DEF
#undef GETVALUE_DEF
#undef POOL_VAL
#undef VAR_VAL
#undef GET_VALUE
#undef EVAL
#undef _DPRINT_VAR

#define EVAL_NAME _evalat
#define GETVALUE_NAME _get_valueat

#define EVAL_DEF(NODE, VAL) \
  static int EVAL_NAME(NODE, const double * restrict x, double * restrict arr, VAL)
#define GETVALUE_DEF(NODE, VAL) \
  static inline int GETVALUE_NAME(NODE, const double * restrict x, double * restrict arr, VAL)

#define POOL_VAL(IDX) arr[CIDX_R(IDX)]
#define VAR_VAL(IDX) x[VIDX_R(IDX)]
#define GET_VALUE(NODE, VAL) GETVALUE_NAME(NODE, x, arr, VAL)
#define EVAL(NODE, VAL) EVAL_NAME(NODE, x, arr, VAL)

#define _DPRINT_VAR(IDX, VAL) \
  DPRINT("%s :: var #%d = %e\n", __func__, \
  VIDX_R(IDX), VAL)

#include "nltree_eval.inc"

/**
 *  @brief Evaluate an expression tree at a given point
 *
 *  @param tree  the expression tree
 *  @param x     the values of the variables
 *  @param arr   the data array for constant
 *  @param val   the result of the computation
 *
 *  @return      the error code
 */
int nltree_evalat(NlTree * restrict tree, const double * restrict x,
                  double * restrict arr, double *val)
{
   *val = 0.;

   /*  TODO(xhub) change whenever umin is done */
   if (tree->root && tree->root->op < __OPCODE_LEN) {
      return EVAL_NAME(tree->root, x, arr, val);
   } 

   return OK;
}

/** 
 *  @brief Insert an ADD node in the expression tree
 *
 *  @param         tree  the expression tree
 *  @param[in,out] addr  on input, address where to put the ADD node. On output
 *                       points to the add node.
 *
 *  @return              the error code
 */
static int _nltree_put_add_node(NlTree *tree, NlNode **addr)
{
   NlNode *add_node;

   /* ----------------------------------------------------------------------
    * First create the new ADD node
    * ---------------------------------------------------------------------- */

   /* \TODO(xhub) add an optional param to reserve more node if needed */
   A_CHECK(add_node, nlnode_alloc(tree, 2));
   nlnode_default(add_node, NLNODE_ADD);
   add_node->children[0] = *addr;
   add_node->children[1] = NULL;

   *addr = add_node;

   return OK;
}

/*  TODO(xhub) make a nice function to find the top
 *  add node, put a mul node and reserve N children*/
/**
 * @brief Find the top ADD node an expression tree.
 *
 * It creates an ADD node if there is none
 *
 *
 * @param         ctr    the container
 * @param         tree   the expression tree
 * @param[out]    raddr  the ADD node
 * @param[in,out] coeff  the coefficient of an expression
 *
 * @return               the error code
 */
int nltree_find_add_node(NlTree *tree, NlNode *** restrict raddr, NlPool *pool,
                         double * restrict coeff)
{
   NlNode * restrict node  = tree->root;
   NlNode ** restrict addr = &tree->root;

   if (!node) {
      A_CHECK(tree->root, nlnode_alloc_init(tree, 1));
      (*raddr) = &tree->root;
      nlnode_default(tree->root, NLNODE_ADD);
      return OK;
   }

   /* ----------------------------------------------------------------------
    *
    * ---------------------------------------------------------------------- */

   while(node) {
      switch (node->op) {
      case NLNODE_UMIN:
         if (isfinite(*coeff)) {
            *coeff *= -1.;
            addr = &node->children[0];
            node = node->children[0];
            break;
         } else {
            S_CHECK(_nltree_put_add_node(tree, addr));
            goto _end;
         }
      case NLNODE_ADD:
         /* TODO(xhub) check that this is OK  */
         goto _end;
      case __OPCODE_LEN:
         /*  TODO(xhub) do something here */
         goto _to_be_reworked;
      case NLNODE_MUL:
         if (isfinite(*coeff) && node->oparg == NLNODE_OPARG_CST
                && node->children_max == 1) {
           assert(node->children[0]);
           *coeff /= pool->data[CIDX_R(node->value)];
           addr = &node->children[0];
           node = node->children[0];
           break;
         }
         FALLTHRU
      case NLNODE_CALL1:
      case NLNODE_CALL2:
      case NLNODE_CALLN:
      /* -------------------------------------------------------------------- 
       * This is possible and correct whenever we have a NL expression
       * with a linear part. If the latter is injected before any NL term, we
       * end up with the root node being a VAR node
       * -------------------------------------------------------------------- */
      case NLNODE_VAR:

         /* -----------------------------------------------------------------
          * In this case we have no choice but to insert an ADD node
          * ----------------------------------------------------------------- */

         S_CHECK(_nltree_put_add_node(tree, addr));
         goto _end;

      case NLNODE_SUB:

         /* -----------------------------------------------------------------
          * Switch the node to an ADD node
          * @warning: we are changing an existing node
          * ----------------------------------------------------------------- */

         node->op = NLNODE_ADD;

         switch (node->oparg) {
         case NLNODE_OPARG_CST:
         {
            size_t loffset = node->children_max;
            S_CHECK(nlnode_reserve(tree, node, 1));
            NlNode ** restrict laddr = &node->children[loffset];
            S_CHECK(nltree_umin(tree, &laddr));

            A_CHECK(*laddr, nlnode_alloc_nochild(tree));
            NlNode * restrict lnode = *laddr;
            lnode->op = NLNODE_CST;
            lnode->oparg = NLNODE_OPARG_UNSET;
            lnode->value = node->value;

            node->value = 0;
            goto _end;
         }
         case NLNODE_OPARG_VAR:
         {
            /* \TODO(xhub) use uminv */
            size_t loffset = node->children_max;
            S_CHECK(nlnode_reserve(tree, node, 1));
            NlNode ** restrict laddr = &node->children[loffset];
            S_CHECK(nltree_umin(tree, &laddr));

            A_CHECK(*laddr, nlnode_alloc_nochild(tree));
            NlNode * restrict lnode = *laddr;
            lnode->op = NLNODE_VAR;
            lnode->oparg = NLNODE_OPARG_UNSET;
            lnode->value = node->value;

            node->value = 0;
            goto _end;
         }
         case NLNODE_OPARG_UNSET:
         {
            assert(node->children[0] && node->children_max >= 2);
            if (node->children_max > 2) {
               error("%s :: untested case: OPCODE_SUB has more than"
                                 "two children: %d\n", __func__,
                                 node->children_max);
            }

            NlNode * restrict lnode = node->children[0];
            NlNode ** restrict laddr = &node->children[0];
            node->children[0] = NULL;
            S_CHECK(nltree_umin(tree, &laddr));
            *laddr = lnode;

            goto _end;
         }
         default:
            error("%s :: unsupported case", __func__);
         }
         FALLTHRU
      default:
         /* \TODO(xhub) this requires thorough testing  */
         error("%s :: going into untested territory: node->op = %d\n",
               __func__, node->op);
         assert(node->op <= __OPCODE_LEN);
         S_CHECK(_nltree_put_add_node(tree, addr));
         goto _end;
      }
   }

_to_be_reworked:
   /* This means that the tree is empty :(  */
   if (isfinite(*coeff) && fabs(*coeff + 1.) < DBL_EPSILON
          && (!node || node->op == __OPCODE_LEN)) {
      if (!node) {
         A_CHECK(node, nlnode_alloc_fixed_init(tree, 1));
         (*addr) = node;
      }
      nlnode_default(node, NLNODE_UMIN);
      A_CHECK(node->children[0], nlnode_alloc_init(tree, 2));
      (*raddr) = &node->children[0];
      nlnode_default(node->children[0], NLNODE_ADD);
      *coeff = 1.;
      return OK;
   }

   /* ------------------------------------------------------------------------
    * This case is not dealt with properly
    * TODO(xhub) fix that
    * ------------------------------------------------------------------------*/

   if (*addr) {
      if ((*addr)->op == __OPCODE_LEN) {
         nlnode_default(*addr, NLNODE_ADD);
         S_CHECK(nlnode_reserve(tree, *addr, 2));
      } else {
      /* ---------------------------------------------------------------------
       * We have a node that is not an ADD node
       * -------------------------------------------------------------------- */
         TO_IMPLEMENT("")
      }
   } else {
      A_CHECK(*addr, nlnode_alloc_fixed_init(tree, 2));
      nlnode_default(*addr, NLNODE_ADD);
   }

_end:
   assert(*addr && (*addr)->op == NLNODE_ADD);
   (*raddr) = addr;

   return OK;
}

/* \TODO(Xhub) Maybe expand this for other types of nodes  */

/** 
 *  @brief Check if the ADD node is valid, and try to rectify if needed
 *
 *  @param node  the node to check
 *
 *  @return      the error code
 */
int nltree_check_add(NlNode *node)
{
   assert(node->op == NLNODE_ADD);
   unsigned nb_operand;
   unsigned valid_child_idx = 0;
   bool has_oparg_cst = false;
   bool has_oparg_var = false;
   switch (node->oparg) {
   case NLNODE_OPARG_CST:
      has_oparg_cst = true;
      nb_operand = 1;
      break;
   case NLNODE_OPARG_VAR:
      has_oparg_var = true;
      nb_operand = 1;
      break;
   default:
      nb_operand = 0;
   }

   for (unsigned i = 0; i < node->children_max; ++i) {
      if (node->children[i]) {
        nb_operand++;
        valid_child_idx = i;

        /* -----------------------------------------------------------------
         * If there are more than 2 operands, then we are good
         * ----------------------------------------------------------------- */

        if (nb_operand > 1) { return OK; }
      }
   }

   /* ----------------------------------------------------------------------
    * Only one operand
    * ---------------------------------------------------------------------- */

   if (nb_operand == 1) {
      if (has_oparg_cst) {
         assert(node->value > 0);
         node->op = NLNODE_CST;
         node->oparg = NLNODE_OPARG_UNSET;

         /* TODO(Xhub) this node is not of the right type, and we loose some
          * memory, but oh well  */
         node->children_max = 0;
      } else if (has_oparg_var) {
         assert(node->value > 0);
         node->op = NLNODE_VAR;
         node->oparg = NLNODE_OPARG_UNSET;

         /* TODO(Xhub) this node is not of the right type, and we loose some
          * memory, but oh well  */
         node->children_max = 0;
      } else {
        /* TODO(xhub) improve this part  */
         memmove(node, node->children[valid_child_idx], sizeof(NlNode));
      }
   } else {
      error("%s :: invalid node: there is no child!\n", __func__);
      return Error_InvalidValue;
   }

   return OK;
}


