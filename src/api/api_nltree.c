#include "nltree.h"
#include "nltree_priv.h"
#include "macros.h"
#include "mdl.h"
#include "mdl_rhp.h"
#include "reshop.h"
#include "status.h"

/** 
 *  @brief Check the input (tree and node)
 *
 *  @param tree  the tree to check
 *  @param node  the node to check
 *
 *  @return      the error code
 *
 */
static int _chk_tree_node_v1(NlTree *tree, NlNode ** restrict *node, const char* fn)
{
   if (!tree) {
      error("%s :: the tree is NULL\n", fn);
      return Error_NullPointer;
   }

   if (!node) {
      error("%s :: the node is NULL\n", fn);
      return Error_NullPointer;
   }

   if (*node) {
      error("%s :: the node points to a non-null object\n", fn);
      if (**node) {
         nlnode_print(**node, PO_ERROR, true);
      }
      return Error_UnExpectedData;
   }

   return OK;
}

static int _chk_tree_node_v2(NlTree *tree, NlNode ** restrict *node, const char* fn)
{
   if (!tree) {
      error("%s :: the tree is NULL\n", fn);
      return Error_NullPointer;
   }

   if (!node) {
      error("%s :: the node is NULL\n", fn);
      return Error_NullPointer;
   }

   if (!*node) {
      error("%s :: the node points to a NULL object\n", fn);
      return Error_NullPointer;
   }

   if (**node) {
      error("%s :: the node points to a non-null object\n", fn);
      nlnode_print(**node, PO_ERROR, true);
      return Error_UnExpectedData;
   }

   return OK;
}

static int _chk_node_only(NlNode **restrict *node, const char* fn)
{
   if (!node) {
      error("%s :: the node is NULL\n", fn);
      return Error_NullPointer;
   }

   if (*node) {
      error("%s :: the node points to a non-null object\n", fn);
      if (**node) {
         nlnode_print(**node, PO_ERROR, true);
      }
      return Error_UnExpectedData;
   }

   return OK;
}

static int _chk_node(NlNode **node, const char* fn)
{
   if (!node) {
      error("%s :: the node is NULL\n", fn);
      return Error_NullPointer;
   }
   if (!*node) {
      error("%s :: the node points to a NULL object\n", fn);
      return Error_NullPointer;
   }

   return OK;
}

static int _chk_node2(NlNode ***node, const char* fn)
{
   if (!node) {
      error("%s :: the node is NULL\n", fn);
      return Error_NullPointer;
   }
   if (!*node) {
      error("%s :: the node points to a NULL object\n", fn);
      return Error_NullPointer;
   }
   if (!**node) {
      error("%s :: **node is a NULL object\n", fn);
      return Error_NullPointer;
   }

   return OK;
}

/** 
 *  @brief Get the address of the root node of an expression tree
 *
 *  If the root node does not exists, it gets created.
 *
 *  @ingroup publicAPI
 *
 *  @param       tree  the expression tree
 *  @param[out]  node  on output, contains the address of the root node
 *
 *  @return            the error code
 */
int rhp_nltree_getroot(NlTree *tree, NlNode ***node)
{
   S_CHECK(_chk_tree_node_v1(tree, node, __func__));

   *node = &tree->root;
   return OK;
}

/** 
 *  @brief Get the address of a child of a node
 *
 *  @ingroup publicAPI
 *
 *  @param       node   the node
 *  @param[out]  child  on output, contains the address of the root node
 *  @param       i      the index of the child
 *
 *  @return             the error code
 */
int rhp_nltree_getchild(NlNode **node, NlNode ***child, unsigned i)
{
   S_CHECK(_chk_node(node, __func__));
   S_CHECK(_chk_node_only(child, __func__));

   NlNode *lnode = *node;
   if (i >= lnode->children_max) {
      error("%s :: index %d >= %d the number of children\n",
               __func__, i, (lnode)->children_max);
      return Error_IndexOutOfRange;
   }

   *child = &lnode->children[i];

   return OK;
}

/** 
 *  @brief Get the address of a child of a node
 *
 *  @ingroup publicAPI
 *
 *  @param       node   the node
 *  @param[out]  child  on output, contains the address of the root node
 *  @param       i      the index of the child
 *
 *  @return             the error code
 */
int rhp_nltree_getchild2(NlNode ***node, NlNode ***child, unsigned i)
{
   S_CHECK(_chk_node2(node, __func__));
   S_CHECK(_chk_node_only(child, __func__));

   NlNode *lnode = **node;
   if (i >= lnode->children_max) {
      error("%s :: index %d >= %d the number of children\n",
               __func__, i, (lnode)->children_max);
      return Error_IndexOutOfRange;
   }

   *child = &lnode->children[i];

   return OK;
}
/** 
 *  @brief Add an arithmetic operation to the expression tree
 *
 *  @ingroup publicAPI
 *
 *  @param tree    the expression tree
 *  @param node    the node where to add the relation
 *  @param opcode  the opcode for the binary relation
 *  @param nb      the number of terms involved by the relation
 *
 *  @return        the error code
 */
int rhp_nltree_arithm(NlTree *tree, NlNode ***node,
                      unsigned opcode, unsigned nb)
{
   S_CHECK(_chk_tree_node_v2(tree, node, __func__));

   switch (opcode) {
   case NLNODE_ADD:
   case NLNODE_SUB:
   case NLNODE_MUL:
   case NLNODE_DIV:
   case NLNODE_UMIN:
   {
      NlNode *lnode;
      A_CHECK(lnode, nlnode_alloc_fixed_init(tree, nb));
      **node = lnode;
      nlnode_default(lnode, opcode);
      break;
   }
   default:
    error("%s :: unsupported opcode %s (%d)", __func__,
             opcode < __OPCODE_LEN ? opcode_names[opcode] : "invalid", opcode);
    return Error_InvalidValue;
   }

   return OK;
}

/** 
 *  @brief Add a function call to the expression tree
 *
 *  The problem type may be changed if function that is added is nonsmooth
 *
 *  @ingroup publicAPI
 *
 *  @param ctr     the container
 *  @param tree    the expression tree
 *  @param node    the current node
 *  @param opcode  the opcode corresponding to the function
 *  @param n_args  the number of arguments (see enum func_code)
 *
 *  @return        the error code
 */
int rhp_nltree_call(Model *mdl, NlTree *tree,
                    NlNode ***node, unsigned opcode, unsigned n_args)
{
   S_CHECK(chk_rmdl(mdl, __func__));
   S_CHECK(_chk_tree_node_v2(tree, node, __func__));

   if (opcode >= fndummy) {
      error("%s :: the function opcode is incorrect: %d and the "
               "max value is %d\n", __func__, opcode, fndummy);
      return Error_InvalidValue;
   }

   NlNode *lnode;
   A_CHECK(lnode, nlnode_alloc_fixed_init(tree, n_args));
   **node = lnode;
   lnode->oparg = NLNODE_OPARG_UNSET;
   lnode->value = opcode;
   switch (n_args) {
   case 1:
      lnode->op = NLNODE_CALL1;
      break;
   case 2:
      lnode->op = NLNODE_CALL2;
      break;
   case 0:
      error("%s :: the number of argument has to be non-zero\n",
               __func__);
      return Error_InvalidValue;
   default:
      lnode->op = NLNODE_CALLN;
   }

   /* ----------------------------------------------------------------------
    * If we inject some function, we know that we end up with a nonsmooth model
    * ---------------------------------------------------------------------- */

   if (opcode >= fnboolnot && opcode <= fnifthen) {
      S_CHECK(mdl_setprobtype(mdl, MdlProbType_dnlp));
   }

   return OK;
}

/**
 *  @brief  Put a constant in a node (CST node)
 *
 *  The node must point to a valid memory area that contains a NULL object
 *  (e.g. *node is non-null but **node is NULL)
 *
 *  @ingroup publicAPI
 *
 *  @param          ctr   the container
 *  @param          tree  the expression tree
 *  @param[in,out]  node  the current node
 *  @param          cst   the constant
 *
 *  @return              the error code
 */
int rhp_nltree_cst(Model *mdl, NlTree* tree, NlNode ***node,
                   double cst)
{
   NlNode *lnode;
   S_CHECK(chk_rmdl(mdl, __func__));
   S_CHECK(_chk_tree_node_v2(tree, node, __func__));

   unsigned pool_idx = rctr_poolidx(&mdl->ctr, cst);
   if (pool_idx == UINT_MAX) { return -Error_InsufficientMemory; }

   A_CHECK(lnode, nlnode_alloc_nochild(tree));
   (**node) = lnode;
   lnode->op = NLNODE_CST;
   lnode->oparg = NLNODE_OPARG_UNSET;
   lnode->value = pool_idx;


   return OK;
}

/** 
 *  @brief add a umin to an expression tree
 *
 *  @ingroup publicAPI
 *
 *  @param tree  the expression tree
 *  @param node  the current node
 *
 *  @return      the error code
 */
int rhp_nltree_umin(NlTree *tree, NlNode ** restrict *node)
{
   S_CHECK(_chk_tree_node_v2(tree, node, __func__));

   return nltree_umin(tree, node);
}

int rhp_nltree_var(Model *mdl, NlTree* tree,
                NlNode ***node, rhp_idx vi, double coeff)
{
   S_CHECK(chk_rmdl(mdl, __func__));
   S_CHECK(_chk_tree_node_v2(tree, node, __func__));

   return rctr_nltree_var(&mdl->ctr, tree, node, vi, coeff);
}
