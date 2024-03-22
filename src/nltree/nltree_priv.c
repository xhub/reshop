#include <assert.h>

#include "compat.h"
#include "nltree.h"
#include "nltree_priv.h"
#include "macros.h"
#include "status.h"

static int _vartree_add(VarTree* vt, unsigned vt_idx, NlNode* var_node)
{
   if (vt->vars[vt_idx].max <= vt->vars[vt_idx].len)
   {
      vt->vars[vt_idx].max = MAX(2*vt->vars[vt_idx].max, 2);
      REALLOC_(vt->vars[vt_idx].nodes, NlNode *, vt->vars[vt_idx].max);
   }

   vt->vars[vt_idx].nodes[vt->vars[vt_idx].len++] = var_node;

   return OK;
}

/**
 * @brief Search for the variable vidx, and return the index in the list
 *
 * @param      vt      the vartree structure
 * @param      value   the variable index
 *
 * @return             the index of vidx in vt (if found), otherwise IdxNA
 *
 */
static NONNULL
unsigned _vartree_match(VarTree* vt, unsigned value)
{
   for (unsigned i = 0, len = vt->len; i < len; ++i) {

      if (vt->vars[i].idx == value) {
         return i;
      }
   }

   return UINT_MAX;
}

int nlnode_add_child(NlNode* node, NlNode* c, size_t indx)
{
   if (node->children_max <= indx) {
      node->children_max = MAX(2*node->children_max, 2);
      REALLOC_(node->children, NlNode*, node->children_max);
   }

   node->children[indx] = c;

   return OK;
}

/** @brief duplicate a node
 *
 *  @param[out] new_node  the destination node, must be empty
 *  @param      node      the source node
 *  @param      tree      the tree for the destination node
 *
 *  @return               the error code
 */
int nlnode_copy(NlNode** new_node, const NlNode* node, NlTree* tree)
{
   NlNode* node2;
   A_CHECK(node2, nlnode_alloc_fixed_init(tree, node->children_max));

   /*  new_node must be empty */
   assert(!*new_node);

   nlnode_attr_copy(node2, node);

   /* ----------------------------------------------------------------------
    * Keep a list of all the variables in the tree
    * TODO: GITLAB 74
    * ---------------------------------------------------------------------- */

   if (tree->v_list && (node->op == NLNODE_VAR || node->oparg == NLNODE_OPARG_VAR)) {
      rhp_idx vidx = VIDX_R(node->value);
      for (size_t i = 0; i < tree->v_list->idx; ++i) {
         if (vidx == tree->v_list->pool[i]) {
            goto _no_add;
         }
      }
      if (tree->v_list->idx >= tree->v_list->max) {
         tree->v_list->max = MAX(tree->v_list->max*2, 2);
         REALLOC_(tree->v_list->pool, int, tree->v_list->max);
      }
      tree->v_list->pool[tree->v_list->idx++] = vidx;
   }

_no_add: ;

//   if (debug) {
//   printf("nlnode_copy :: object %p op = %d; value = %d; children = %d\n",
//   node, node->op, node->value, node->children_max); }

   /* ----------------------------------------------------------------------
    * Iterate over the children (DFS)
    * ---------------------------------------------------------------------- */

   for (unsigned i = 0, len = node->children_max; i < len; ++i)
   {
      NlNode* child = node->children[i];
      if (!child) {
         node2->children[i] = NULL;
         continue;
      }
//         if (debug) {
//         printf("child %d :: object %p op = %d; value = %d; children = %d\n",
//         i, node->children[i], node->children[i]->op, node->children[i]->value,
//         node->children[i]->children_max);}


      /* ----------------------------------------------------------------
       * If the child is of the variable type, 
       * ---------------------------------------------------------------- */

      if (tree->vt && !tree->vt->done && child->op == NLNODE_VAR) {
         unsigned vt_idx = _vartree_match(tree->vt, child->value);

         if (vt_idx != UINT_MAX) {
            S_CHECK(_vartree_add(tree->vt, vt_idx, child));
         }
      }

      S_CHECK(nlnode_copy(&node2->children[i], child, tree));
   }

   (*new_node) = node2;

   return OK;
}

/** @brief duplicate a node with a translation of the variable index
 *
 *  @param[out] new_node  the destination node, must be empty
 *  @param      node      the source node
 *  @param      tree      the tree for the destination node
 *  @param      rosetta   the array for translating indices
 *
 *  @return               the error code
 */
int nlnode_copy_rosetta(NlNode** new_node, const NlNode* node,
                        NlTree* tree, const int* restrict rosetta)
{
   /*  new_node must be empty */
   assert(!*new_node);

   NlNode* node2;
   A_CHECK(node2, nlnode_alloc_fixed_init(tree, node->children_max));

   nlnode_attr_copy_rosetta(node2, node, rosetta);

   /* ----------------------------------------------------------------------
    * Keep a list of all the variables in the tree
    * TODO(xhub) make v_list a sorted avar
    * ---------------------------------------------------------------------- */

   if (tree->v_list && (node->op == NLNODE_VAR || node->oparg == NLNODE_OPARG_VAR)) {
      rhp_idx vi = rosetta[VIDX_R(node->value)]; assert(valid_vi(vi));
      S_CHECK(vlist_add(tree->v_list, vi));
   }

//   if (debug) {
//   printf("nlnode_copy :: object %p op = %d; value = %d; children = %d\n",
//   node, node->op, node->value, node->children_max); }

   /* ----------------------------------------------------------------------
    * Iterate over the children (DFS)
    * ---------------------------------------------------------------------- */

   for (unsigned i = 0, len = node->children_max; i < len; ++i)
   {
      if (!node->children[i]) {
         node2->children[i] = NULL;
         continue;
      }
//         if (debug) {
//         printf("child %d :: object %p op = %d; value = %d; children = %d\n",
//         i, node->children[i], node->children[i]->op, node->children[i]->value,
//         node->children[i]->children_max);}


      /* ----------------------------------------------------------------
       * If the child is of the variable type, 
       * ---------------------------------------------------------------- */

      if (tree->vt && !tree->vt->done && node->children[i]->op == NLNODE_VAR) {
         const NlNode* child = node->children[i];
         unsigned vt_idx = _vartree_match(tree->vt, child->value);

         if (vt_idx != UINT_MAX) {
            S_CHECK(_vartree_add(tree->vt, vt_idx, node->children[i]));
         }
      }

      S_CHECK(nlnode_copy_rosetta(&node2->children[i], node->children[i], tree, rosetta));
   }

   (*new_node) = node2;

   return OK;
}

static NONNULL int _nlnode_getallvars(NlTree *tree, NlNode *node)
{
   if (node->op == NLNODE_VAR || node->oparg == NLNODE_OPARG_VAR) {
      S_CHECK(vlist_add(tree->v_list, VIDX_R(node->value)));
   }

   /* ----------------------------------------------------------------------
    * Iterate over the children (DFS)
    * ---------------------------------------------------------------------- */

   for (size_t i = 0; i < node->children_max; ++i)
   {
      if (node->children[i]) {
         S_CHECK(_nlnode_getallvars(tree, node->children[i]));
      }
   }

   return OK;
}

int nltree_getallvars(NlTree *tree)
{
   S_CHECK(nltree_reset_var_list(tree));

   if (!tree->root) {
      return OK;
   }

   S_CHECK(_nlnode_getallvars(tree, tree->root));

   return OK;
}

int nlnode_apply_rosetta(NlNode *node, struct vlist *v_list, const rhp_idx * restrict rosetta)
{
   if (node->op == NLNODE_VAR || node->oparg == NLNODE_OPARG_VAR) {
      node->value = VIDX(rosetta[VIDX_R(node->value)]);

      /* ----------------------------------------------------------------------
       * Keep a list of all the variables in the tree
       * TODO: GITLAB 74
       * ---------------------------------------------------------------------- */

      int vidx = VIDX_R(node->value);
      for (size_t i = 0; i < v_list->idx; ++i) {
         if (vidx == v_list->pool[i]) {
            goto _no_add;
         }
      }

      if (v_list->idx >= v_list->max) {
         v_list->max = MAX(v_list->max*2, 2);
         REALLOC_(v_list->pool, int, v_list->max);
      }
      v_list->pool[v_list->idx++] = vidx;
   }

_no_add: ;
   NlNode ** restrict children = node->children;
   for (unsigned i = 0, len = node->children_max; i < len; ++i) {
      if (!children[i]) { continue; }
      S_CHECK(nlnode_apply_rosetta(children[i], v_list, rosetta));
   }

   return OK;
}
