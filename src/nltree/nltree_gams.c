#include "container.h"
#include "equ.h"
#include "nltree.h"
#include "nltree_priv.h"

struct rhp_stack {
   void *ptr;
};

static NONNULL unsigned count_children(const NlNode * restrict node)
{
   unsigned cnt = 0;
   enum NLNODE_OPARG optype = node->oparg;
   switch (optype) {
   case NLNODE_OPARG_CST:
   case NLNODE_OPARG_VAR:
   case NLNODE_OPARG_FMA:
      cnt++;
      break;
   default:
      ;
   }

   for (unsigned i = 0, len = node->children_max; i < len; ++i) {
      if (node->children[i]) cnt++;
   }

   return cnt;
}

static NONNULL
int build_gams_opcode_v2(const NlNode * restrict node,
                         int * restrict instrs,
                         int * restrict args,
                         int indx);

static enum NLNODE_OP get_op_class(int opcode)
{
   switch (opcode) {
      case nlPushI:
      case nlPushZero:
         return NlNode_Cst;
      case nlPushV:
         return NlNode_Var;
      case nlAddV:
      case nlAddI:
      case nlAdd:
      case nlMulIAdd:
         return NlNode_Add;
      case nlSubV:
      case nlSubI:
      case nlSub:
         return NlNode_Sub;
      case nlMulV:
      case nlMulI:
      case nlMul:
         return NlNode_Mul;
      case nlDivV:
      case nlDivI:
      case nlDiv:
         return NlNode_Div;

      case nlUMin:
      case nlUMinV:
         return NlNode_Umin;

      case nlCallArg1:
         return NlNode_Call1;
      case nlCallArg2:
         return NlNode_Call2;
         /*  This is legit since in the greedy approach  */
      case nlStore:
         return __OPCODE_LEN;
      default:
         error("%s :: Unsupported opcode %d :: %s\n", __func__,
                  opcode, instr_code_name(opcode));
         return -1;
   }
}

enum NLNODE_OPARG gams_get_optype(int opcode)
{
   switch (opcode) {
      case nlPushI:
      case nlPushZero:
      case nlAddI:
      case nlSubI:
      case nlMulI:
      case nlDivI:
         return NLNODE_OPARG_CST;
      case nlPushV:
      case nlAddV:
      case nlSubV:
      case nlMulV:
      case nlDivV:
      case nlUMinV:
         return NLNODE_OPARG_VAR;
      case nlMulIAdd:
         return NLNODE_OPARG_FMA;
      case nlAdd:
      case nlSub:
      case nlMul:
      case nlDiv:
      case nlUMin:
      case nlCallArg1:
      case nlCallArg2:
      case nlCallArgN:
      case nlFuncArgN:
      case nlStore:
      case nlHeader:
         return NLNODE_OPARG_UNSET;
      default:
         error("%s :: Unsupported opcode %d :: %s\n", __func__,
                  opcode, instr_code_name(opcode));
         return -1;
   }
}

static size_t _greedy_build(const int * restrict instrs, const int *args,
                            int opcode, size_t k, size_t kmax)
{
   size_t k_orig = ++k;
   enum NLNODE_OP op_class = get_op_class(opcode);

   while (k < kmax) {
      int instr = instrs[k];
      enum NLNODE_OP iclass = get_op_class(instr);
      if ((op_class == iclass) && (nlMulIAdd != instr)) { k++; }
      else if ((op_class == NlNode_Add) &&
            ((iclass == NlNode_Cst) || (iclass == NlNode_Var)) &&
            (nlMulIAdd == instrs[k+1])) { k += 2; }
      else { break; }
   }

   return k - k_orig;
}

/**
 * @brief Get a (large) upper bound on the size of the opcode
 *
 * @param e  the equation
 *
 * @return  0 if there is no nonlinear part, otherwise the upper bound
 */
static size_t _nltree_getsizeopcode(const Equ *e)
{
   if (!e->tree || !e->tree->root || e->tree->root->op == __OPCODE_LEN) {
      return 0;
   }

   /* ---------------------------------------------------------------------
    * There could be a tree with a childless ADD node
    * --------------------------------------------------------------------- */

   const NlNode *node = e->tree->root;
   if (node->op == NlNode_Add && count_children(node) == 0) {
      return 0;
   }

   return 3*(e->tree->nodes.bucket_idx+1)*(e->tree->nodes.pool_max+1);
}

#define PUSH_ON_STACK(s, nodestack, max_len) s++; if ((*max_len) < s) { \
      (*max_len) *= 2; \
      REALLOC_EXIT((*nodestack), NlNode *, *max_len); \
      if (!(*nodestack)) { error("%s :: insufficient memory!\n", __func__); \
        status = Error_InsufficientMemory; goto _exit; } }

#define POP_FROM_STACK(s) if (s) { --(s); } else { \
  error("%s :: invalid opcode stack", __func__); \
           status = Error_InvalidOpCode; goto _exit; }

static int _gams_opcode(enum NLNODE_OP code)
{
   switch (code) {
      case NlNode_Var:
         return nlPushV;
      case NlNode_Call1:
         return nlCallArg1;
      case NlNode_Call2:
         return nlCallArg2;
      case NlNode_CallN:
         return nlCallArgN;
      default:
         assert(0);
         error("%s :: Unsupported opcode %d\n", __func__, code);
         return -1;
   }
}

static NONNULL DBGUSED
bool chk_gms_opcode(int *instr, int *args, unsigned len, const char* equname)
{
   bool res = true;

   if (instr[0] != nlHeader) {
      error("[%s] opcode @0 is not %s, rather %s\n", __func__,
            instr_code_name(nlHeader), instr_code_name(instr[0]));
      res = false;
   }

   if (args[0] != len) {
      error("[%s] ERROR: nlcode length is inconsistent: args[0] = %d vs %d\n",
            __func__, args[0], len);
      res = false;
   }

   for (int i = 0; i < len; ++i) {
      if (instr[i] >= nlOpcode_size) {
         error("[%s] opcode @%d has value %d (max is %d)\n", __func__, i,
               instr[i], nlOpcode_size-1);
         res = false;
      }

      if (instr[i] == nlNoOp) {
         error("[%s] opcode @%d is NO-OP, this should be happen!\n", __func__, i);
         res = false;
      }
      if (instr[i] >= nlChk) {
         error("[%s] opcode @%d is outside os the usual range, double check!\n",
               __func__, i);
         res = false;
      }
   }

   if (!res) {
      error("[%s] ERRORs encountered while checking the nlcode for equation '%s'",
            __func__, equname);
   }

   return res;
}


/** 
 *  @brief build an expression tree
 *
 *  If the equation contains a nonlinear expression, translate it into an
 *  expression tree. Otherwise, just create an empty expression tree
 *
 *  @param  codelen  the length of the opcode
 *  @param  instrs   the instructions
 *  @param  args     the arguments
 *
 *  @return          the error code
 */
NlTree* nltree_buildfromgams(unsigned codelen, const int * restrict instrs,
                          const int * restrict args)
{
   int status = OK;

   if (codelen == 0) {
      return NULL;
   }

   NlTree *tree;
   AA_CHECK(tree, nltree_alloc(codelen > 0 ? (size_t)1.8*codelen : 4));

   /* ----------------------------------------------------------------------
    * Set the equation index
    * ---------------------------------------------------------------------- */

   unsigned s = 0;

   NlNode **nodestack, *curnode = NULL;

   /* TODO(xhub) This could benefit from ctr_getmem  */
   MALLOC_NULL(nodestack, NlNode *, codelen);
   nodestack[0] = NULL;

   /* TODO: this is unsused, why? */
   UNUSED size_t nb_nodes = 1;

   size_t k = 0;
   while (k < codelen) {
skip_k_incr:
     {
      const enum nlopcode key = instrs[k];

      switch (key) {
      case nlNoOp:
      case nlHeader:
         break;

      case nlStore:
         /* completly trust the index in the bytecode */
         tree->idx = args[k] - 1;
         break;

      case nlPushV:
      case nlPushI:
      case nlPushZero:
         PUSH_ON_STACK(s, &nodestack, &codelen);
         A_CHECK_EXIT(curnode, nlnode_alloc_nochild(tree));
         nb_nodes++;
         curnode->op = get_op_class(key);
         curnode->oparg = NLNODE_OPARG_UNSET;
//         _set_print_now(curnode);
         curnode->value = (key != nlPushZero) ? args[k] : 0;
         nodestack[s] = curnode;
         break;

      case nlAdd:
      case nlSub:
      case nlMul:
      case nlDiv:
      case nlAddV:
      case nlSubV:
      case nlMulV:
      case nlDivV:
      case nlAddI:
      case nlSubI:
      case nlMulI:
      case nlDivI:
      case nlMulIAdd:
      {
         // TODO(xhub) memory assignment is not great, since if we
         // have terms like (pushV MulIAdd)
         size_t fwd = _greedy_build(instrs, args, key, k, codelen);
         A_CHECK_EXIT(curnode, nlnode_alloc(tree, fwd+2));
         nb_nodes++;
         curnode->op = get_op_class(key);
         /*  TODO(Xhub) this NLNODE_OPARG_FMA case is really a mess */
         if (fwd == 0 && gams_get_optype(instrs[k]) == NLNODE_OPARG_FMA) {
            curnode->oparg = NLNODE_OPARG_FMA;
            curnode->value = args[k];
         } else {
            curnode->oparg = NLNODE_OPARG_UNSET; //gams_get_optype(key);
            curnode->value = 0;
         }
         size_t offset = k;
         S_CHECK_EXIT(nlnode_add_child(curnode, nodestack[s], 0));
         POP_FROM_STACK(s);
         size_t d = 0;
         for (size_t i = 1; i + d < fwd + 2; ++i) {
            bool inner_prod_case = false;
            size_t kk = offset + i - 1 + d;
            if (get_op_class(instrs[kk]) != curnode->op) {
               assert(get_op_class(instrs[kk+1]) == curnode->op);
               assert(instrs[kk+1] == nlMulIAdd);
               kk++;
               d++;
               inner_prod_case = true;
            }
            enum NLNODE_OPARG optype = gams_get_optype(instrs[kk]);
            switch (optype) {
            case NLNODE_OPARG_CST:
            {
               S_CHECK_EXIT(nlnode_add_child(curnode, nlnode_alloc_nochild(tree), i));
               nb_nodes++;
               curnode->children[i]->op = NlNode_Cst;
               curnode->children[i]->oparg = optype;
               curnode->children[i]->value = args[kk];
               break;
            }
            case NLNODE_OPARG_VAR: {
               S_CHECK_EXIT(nlnode_add_child(curnode, nlnode_alloc_nochild(tree), i));
               nb_nodes++;
               curnode->children[i]->op = NlNode_Var;
               curnode->children[i]->oparg = optype;
               curnode->children[i]->value = args[kk];
               break;
            }
            case NLNODE_OPARG_FMA: {
               // mulI part
               NlNode *tmpnode = nlnode_alloc_fixed(tree, 1);
               nb_nodes++;
               tmpnode->op = NlNode_Mul;
               tmpnode->oparg = optype;
               if (inner_prod_case) nlnode_print_now(tmpnode);
               tmpnode->value = args[kk];

               if (inner_prod_case) {
                  NlNode *pushnode = nlnode_alloc_nochild(tree);
                  nb_nodes++;
                  pushnode->op = get_op_class(instrs[kk-1]);
                  pushnode->oparg = NLNODE_OPARG_UNSET;
                  pushnode->value = args[kk-1];

                  tmpnode->children[0] = pushnode;
               } else {
                  // Add part
                  /* The quantity that get multiplied goes there */
                  if (i  < 1) {
                     error("%s :: optype = NLNODE_OPARG_FMA\n", __func__);
                  }
                  tmpnode->children[0] = curnode->children[i-1];
                  curnode->children[i-1] = nodestack[s];
                  POP_FROM_STACK(s);
               }
               S_CHECK_EXIT(nlnode_add_child(curnode, tmpnode, i));

               break;
            }
            default:
            {
               S_CHECK_EXIT(nlnode_add_child(curnode, nodestack[s], i));
               POP_FROM_STACK(s);
            }
            }
            k += inner_prod_case ? 2 : 1 ;
         }
         for (size_t i = curnode->children_max - d; i < curnode->children_max;
               ++i) {
            curnode->children[i] = NULL;
         }
         PUSH_ON_STACK(s, &nodestack, &codelen);
         nodestack[s] = curnode;
         goto skip_k_incr;
      }
         break;

      case nlUMin:
         curnode = nlnode_alloc_fixed(tree, 1);
         nb_nodes++;
         curnode->op = get_op_class(key);
         curnode->oparg = NLNODE_OPARG_UNSET;
         curnode->value = 0;
         S_CHECK_EXIT(nlnode_add_child(curnode, nodestack[s], 0));
         nodestack[s] = curnode;
         break;

      case nlUMinV:
         PUSH_ON_STACK(s, &nodestack, &codelen);
         curnode = nlnode_alloc_nochild(tree);
         nb_nodes++;
         curnode->op = get_op_class(key);
         curnode->oparg = NLNODE_OPARG_VAR;
         curnode->value = args[k];
         nodestack[s] = curnode;
         break;

      case nlCallArg1:
         curnode = nlnode_alloc_fixed(tree, 1);
         nb_nodes++;
         curnode->op = get_op_class(key);
         curnode->oparg = NLNODE_OPARG_UNSET;
         curnode->value = args[k];
         S_CHECK_EXIT(nlnode_add_child(curnode, nodestack[s], 0));
         nodestack[s] = curnode;
         break;

      case nlCallArg2:
         assert(s >= 1);
         curnode = nlnode_alloc_fixed(tree, 2);
         nb_nodes++;
         curnode->op = get_op_class(key);
         curnode->oparg = NLNODE_OPARG_UNSET;
         S_CHECK_EXIT(nlnode_add_child(curnode, nodestack[s-1], 0));
         S_CHECK_EXIT(nlnode_add_child(curnode, nodestack[s], 1));
         if ((args[k] == fnvcpower) || (args[k] == fncvpower)) {
            curnode->value = fnrpower;
         } else {
            curnode->value = args[k];
         }
         POP_FROM_STACK(s);
         nodestack[s] = curnode;
         break;

      default:
         error("%s :: unexpected opcode %d :: %s\n", __func__, key,
             instr_code_name(key));
         status = Error_UnExpectedData;
         goto _exit;
      }

      k++;

   }
   }

_exit:
   tree->root = curnode;

   FREE(nodestack);

   if (status != OK) {
      nltree_dealloc(tree);
      return NULL;
   }

   return tree;
}

static void _translate_instr(enum NLNODE_OP key, int *instr,
                             enum NLNODE_OP child_opcode)
{
   bool doV = child_opcode == NlNode_Var ? true : false;
   bool doI = child_opcode == NlNode_Cst ? true : false;
   switch (key)
   {
      case NlNode_Add:
         *instr = doV ? nlAddV : doI ? nlAddI : nlAdd;
         break;
      case NlNode_Sub:
         *instr = doV ? nlSubV : doI ? nlSubI : nlSub;
         break;
      case NlNode_Mul:
         *instr = doV ? nlMulV : doI ? nlMulI : nlMul;
         break;
      case NlNode_Div:
         *instr = doV ? nlDivV : doI ? nlDivI : nlDiv;
         break;
      default:
         error("[gams_genopcode/translate opcode] Unsupported opcode %u\n", key);
   }
}

#ifdef UNUSED_20240322
NONNULL static inline
int process_arithm_child(const NlNode * restrict child,
                         int * restrict instrs,
                         int * restrict args,
                         int indx,
                         int key)
{
   enum NLNODE_OP op = child->op;
   if (op == NLNODE_VAR || op == NlNode_Cst) {
      _translate_instr(key, &instrs[indx], op);
      args[indx] = child->value;
      return indx+1;
   }

   if (child->oparg == NLNODE_OPARG_FMA && key == NLNODE_ADD && child->op == NlNode_Mul) {
      assert(child->children_max == 1 && child->children[0]);
      instrs[indx] = nlMulIAdd;
      args[indx] = child->value;
      indx++;
      return build_gams_opcode_v2(child->children[0], instrs, args, indx);
   }

   _translate_instr(key, &instrs[indx], __OPCODE_LEN);
   args[indx] = 0;
   indx++;
   return build_gams_opcode_v2(child, instrs, args, indx);

}
#endif

/* ---------------------------------------------------------------------
 * This function processed the last 2 children of a arithmetic OP node
 * --------------------------------------------------------------------- */

static NONNULL int arithm_last_children(const NlNode * restrict children[2],
                                        int * restrict instrs,
                                        int * restrict args,
                                        int indx,
                                        enum NLNODE_OP key)
{
   const NlNode *child1 = children[1];
   const NlNode *child0 = children[0];

   switch (child1->oparg) {
   case NLNODE_OPARG_CST:
   case NLNODE_OPARG_VAR:
      if (child1->children_max == 0) {
         _translate_instr(key, &instrs[indx], child1->op);
         args[indx] = child1->value;
         indx++;
         return build_gams_opcode_v2(child0, instrs, args, indx);

      } else if (key != NlNode_Add && child1->children_max != 1) {
         error("%s :: unsupported case: ", __func__);
         nlnode_print(child1, PO_ERROR, true);
         return -Error_UnExpectedData;
      }
      break;
   case NLNODE_OPARG_FMA:
      if (key == NlNode_Add && child1->op == NlNode_Mul) {
         assert(child1->children_max == 1);
         instrs[indx] = nlMulIAdd;
         args[indx] = child1->value;
         indx++;
         //TODO(xhub) document why we explore this child now
         indx = build_gams_opcode_v2(child1->children[0], instrs, args, indx);
         if (indx < 0) return indx;
         return build_gams_opcode_v2(child0, instrs, args, indx);
      }
      break;
   default: ;
   }

   /* The last child should have no opcode */
   _translate_instr(key, &instrs[indx], __OPCODE_LEN);
   args[indx] = 0;
   indx++;

   if (_print_now(child1->ppty)) {
      indx = build_gams_opcode_v2(child1, instrs, args, indx);
      if (indx < 0) return indx;
      return build_gams_opcode_v2(child0, instrs, args, indx);
   }

   indx = build_gams_opcode_v2(child0, instrs, args, indx);
   if (indx < 0) return indx;
   return build_gams_opcode_v2(child1, instrs, args, indx);
}

static NONNULL
int build_gams_opcode_v2(const NlNode * restrict node,
                         int * restrict instrs,
                         int * restrict args,
                         int indx)
{

   const enum NLNODE_OP key = node->op;

   switch (key) {

   case NlNode_Var:
      if (node->children_max > 0) { goto _no_child_error; }

      instrs[indx] = _gams_opcode(key);
      args[indx] = node->value;
      indx++;
      break;

   case NlNode_Cst:
      if (node->children_max > 0) { goto _no_child_error; }

      if (node->value > 0) {
         instrs[indx] = nlPushI;
         args[indx] = node->value;
      } else {
         instrs[indx] = nlPushZero;
         args[indx] = 0;
      }

      indx++;
      break;

   case NlNode_Div: {

      /* ---------------------------------------------------------------------
       * SUB and DIV require 2 children, either be local + 1 child or 2 children
       * --------------------------------------------------------------------- */

      if (node->children_max == 0) { goto _child_expected_error; }

      enum NLNODE_OPARG optype = node->oparg;
      if (node->children_max != 2 &&
        !(node->children_max == 1 && (optype == NLNODE_OPARG_CST || optype == NLNODE_OPARG_VAR))) {
         goto _two_children_error;
      }

      /* ---------------------------------------------------------------------
       * See whether we have a CST or VAR attached to the node. If yes, we emit a
       * XxxI or XxxV instruction.
       * --------------------------------------------------------------------- */

      bool optype_has_instr;
      switch(optype) {
      case NLNODE_OPARG_CST: case NLNODE_OPARG_VAR: {
         assert(key == NlNode_Add || key == NlNode_Mul);
         enum NLNODE_OP op = optype == NLNODE_OPARG_CST ? NlNode_Cst : NlNode_Var;
         _translate_instr(key, &instrs[indx], op);
         args[indx] = node->value;
         indx++;
         optype_has_instr = true;
         break;
      }
      case NLNODE_OPARG_UNSET: /* the node has no parameter, only children*/
         optype_has_instr = false;
         break;
      default:
         error("%s :: ERROR: unexpected optype value %d for op %s \n", __func__,
               optype, opcode_names[key]);
         nlnode_print(node, PO_ERROR, true);
         return -Error_RuntimeError;
      }

      /* ---------------------------------------------------------------------
       * Explore the children
       * --------------------------------------------------------------------- */

      const NlNode * restrict children[2];
      if (!node->children[0]) {
         error("%s :: ERROR: for a node of type %s, the first child is NULL!\n",
               __func__, opcode_names[key]);
         nlnode_print(node, PO_ERROR, true);
         return -Error_InvalidOpCode;
      }

      children[0] = node->children[0];

      if (!optype_has_instr) {
         if(!node->children[1]) {
            error("%s :: ERROR: for a node of type %s, the second child is NULL "
                  "and optype has no instruction!\n", __func__, opcode_names[key]);
            nlnode_print(node, PO_ERROR, true);
            return -Error_InvalidOpCode;
         }
         children[1] = node->children[1];
         return arithm_last_children(children, instrs, args, indx, key);
      }

      /* ---------------------------------------------------------------------
       * If we have only one child, we already emitted a XxxI or XxxV instruction
       * We can just emit the rest.
       * --------------------------------------------------------------------- */

      return build_gams_opcode_v2(children[0], instrs, args, indx);
   }

   case NlNode_Add:
   case NlNode_Mul: {
   case NlNode_Sub:
      if (node->children_max == 0) { goto _child_expected_error; }

      /* ---------------------------------------------------------------------
       *
       * --------------------------------------------------------------------- */
      #define MAX_CHILDREN_ON_STACK 5
      const NlNode * explore_next[MAX_CHILDREN_ON_STACK];

      unsigned explore_indx = 0;
      bool optype_has_instr = true;

      /* TODO: 
       * - case NLNODE_ADD and only 1 operandis bad, but should be explicitely handled
       */

      /* ---------------------------------------------------------------------
       * If there is already an argument to the opcode attached to the node,
       * we process it.
       * --------------------------------------------------------------------- */

      enum NLNODE_OPARG optype = node->oparg;
      switch(optype) {
      case NLNODE_OPARG_CST: case NLNODE_OPARG_VAR: {
         enum NLNODE_OP op = optype == NLNODE_OPARG_CST ? NlNode_Cst : NlNode_Var;
         _translate_instr(key, &instrs[indx], op);
         args[indx] = node->value;
         indx++;
         break;
      }
      case NLNODE_OPARG_FMA: /* Do nothing in the FMA case for an ADD node */
      case NLNODE_OPARG_UNSET: /* the node has no parameter, only children*/
         optype_has_instr = false;
         break;
      default:
         error("%s :: ERROR: unexpected optype value %d\n", __func__, optype);
         return -Error_RuntimeError;
      }

      /* ---------------------------------------------------------------------
       * Find the first 2 children
       * --------------------------------------------------------------------- */

      int idx1 = -1, idx0 = -1;
      for (int i = 0, len = node->children_max, cnt = 0; i < len; ++i) {
         if (node->children[i]) {
            cnt++;

            if (cnt == 1) {
               idx0 = i;
            } else if (cnt == 2) {
               idx1 = i;
               break;
            }
         }
      }

      /* ---------------------------------------------------------------------
       * Check if we have at least 2 children
       * HACK: if we have an ADD node and only 1 true child, we still go ahead.
       * --------------------------------------------------------------------- */

      if (idx1 == -1)  {
         if (optype_has_instr) {
            if (idx0 >= 0) {
               indx = build_gams_opcode_v2(node->children[idx0], instrs, args, indx);
               goto _exit;
            }
         } else if (key == NlNode_Add && idx0 >= 0) {
#ifndef NDEBUG
            printout(PO_DEBUG, "%s: NLNODE_ADD with only 1 child!\n", __func__);
#endif
            indx = build_gams_opcode_v2(node->children[idx0], instrs, args, indx);
            goto _exit;
         }

         goto _two_children_error;
      }

      /* ---------------------------------------------------------------------
       * We now explore the children of the node. First we deal with the latest,
       * until we have 2 left.
       * --------------------------------------------------------------------- */

      for (int i = node->children_max-1; i > idx1; --i) {
         const NlNode *child = node->children[i];
         if (!child) continue;

         /* ---------------------------------------------------------------------
          * If we have a CST or VAR, just emit the right instruction XxxI or XxxV
          * --------------------------------------------------------------------- */

         if (child->children_max == 0) {
            assert(child->op == NlNode_Cst || child->op == NlNode_Var);
            _translate_instr(key, &instrs[indx], child->op);
            args[indx] = child->value;
            indx++;
            continue;
         }

         /* ---------------------------------------------------------------------
          * Child optype is FMA. If it is a MUL node, then emit nlMulIAdd.
          * TODO If it is an ADD node, do nothing
          * --------------------------------------------------------------------- */

         if (child->oparg == NLNODE_OPARG_FMA) { 
            if (child->op == NlNode_Mul) {/* parent is an ADD node, child is MUL*/
               assert(child->children_max == 1 && child->children[0]);
               instrs[indx] = nlMulIAdd;
               args[indx] = child->value;
               indx++;
               indx = build_gams_opcode_v2(child->children[0], instrs, args, indx);
               if (indx < 0) goto _exit;
               continue;
            }
         }

         /* ---------------------------------------------------------------
          * We emit generic Add or Mul
          * --------------------------------------------------------------- */

         _translate_instr(key, &instrs[indx], __OPCODE_LEN);
         args[indx] = 0;
         indx++;

         if (_print_now(child->ppty)) {
            indx = build_gams_opcode_v2(child, instrs, args, indx);
            if (indx < 0) goto _exit;
         } else {

            explore_next[explore_indx++] = child;

            if (explore_indx >= MAX_CHILDREN_ON_STACK-1) {
               printout(PO_DEBUG, "%s :: a lot of children to explore, "
                                 "emptying ...\n", __func__);

              // Now visit the children in order to print in the correct order
               for (int j = explore_indx-1; j >= 0; --j) {
                  indx = build_gams_opcode_v2(explore_next[j], instrs, args, indx);
                  if (indx < 0) goto _exit;
               }
               explore_indx = 0;
            }
         }
      }

      /* ---------------------------------------------------------------------
       * Visit the 2nd child node
       * --------------------------------------------------------------------- */
      const NlNode *child1 = node->children[idx1];
      const NlNode *child0 = node->children[idx0];

      switch (child1->oparg) {
      case NLNODE_OPARG_CST:
      case NLNODE_OPARG_VAR:

         if (child1->children_max == 0) {
            _translate_instr(key, &instrs[indx], child1->op);
            args[indx] = child1->value;
            indx++;
            break;

         } else if (key != NlNode_Add && child1->children_max != 1) {
            error("%s :: unsupported case: ", __func__);
            nlnode_print(child1, PO_ERROR, true);
            indx = -Error_UnExpectedData;
            goto _exit;
         }
      goto default_;
      case NLNODE_OPARG_FMA:
         if (key == NlNode_Add && child1->op == NlNode_Mul) {
            assert(child1->children_max == 1);
            instrs[indx] = nlMulIAdd;
            args[indx] = child1->value;
            indx++;
            //TODO(xhub) document why we explore this child now
            indx = build_gams_opcode_v2(child1->children[0], instrs, args, indx);
            if (indx < 0) goto _exit;
            break;
         }
         FALLTHRU;
      default: 
default_:

         /* The last child should have no opcode */
         _translate_instr(key, &instrs[indx], __OPCODE_LEN);
         args[indx] = 0;
         indx++;

         if (_print_now(child1->ppty)) {
            indx = build_gams_opcode_v2(child1, instrs, args, indx);
            if (indx < 0) goto _exit;
         } else {
            explore_next[explore_indx++] = child1;
         }
      }

      explore_next[explore_indx++] = child0;
      assert(MAX_CHILDREN_ON_STACK >= explore_indx);

      /* Now visit the children in order to print in the correct order */
      for (int i = explore_indx-1; i >= 0; --i) {
         indx = build_gams_opcode_v2(explore_next[i], instrs, args, indx);
         if (indx < 0) goto _exit;
      }
      break;
   }

   case NlNode_Umin:
      if (node->value > 0) { /*  we have a variable */
        if (node->children_max > 0) { goto _no_child_error; }

         instrs[indx] = nlUMinV;
         args[indx] = node->value;
         indx++;

      } else {
        if (node->children_max != 1) { goto _one_child_error; }

         instrs[indx] = nlUMin;
         args[indx] = 0;
         indx++;

         // go visit the child
         indx = build_gams_opcode_v2(node->children[0], instrs, args, indx);
         if (indx < 0) goto _exit;
      }
      break;

   case NlNode_Call1:
      if (node->children_max != 1) { goto _one_child_error; }
      assert(node->children[0]);

      instrs[indx] = _gams_opcode(key);
      args[indx] = node->value;
      indx++;

      indx = build_gams_opcode_v2(node->children[0], instrs, args, indx);
      if (indx < 0) goto _exit;
      break;

   case NlNode_Call2:
      if (node->children_max != 2) { goto _two_children_error; }
      assert(node->children[0] && node->children[1]);

      instrs[indx] = _gams_opcode(key);
      args[indx] = node->value;
      indx++;

      // go visit the child[1]
      indx = build_gams_opcode_v2(node->children[1], instrs, args, indx);
      if (indx < 0) goto _exit;

      // go visit the child[0]
      indx = build_gams_opcode_v2(node->children[0], instrs, args, indx);
      if (indx < 0) goto _exit;
      break;

   case NlNode_CallN:
      if (node->children_max <= 2) { goto _at_least_3_children_error; }

      instrs[indx] = _gams_opcode(key);
      args[indx] = node->value;
      indx++;

      /* ----------------------------------------------------------------
       * This is a bit special, we need to inject an instruction that
       * indicates the number of children
       * ---------------------------------------------------------------- */

      instrs[indx] = nlFuncArgN;
      int arg_Nval_idx = indx;
      indx++;
      int cnt = 0;

      for (unsigned len = node->children_max, j = len-1; j < len; --j) {
         if (!node->children[j]) continue;

         indx = build_gams_opcode_v2(node->children[j], instrs, args, indx);
         if (indx < 0) goto _exit;
         cnt++;
      }

      /* set the number of children */
      args[arg_Nval_idx] = cnt;
      break;

   default:
      error("%s :: unknown opcode %d; node->oparg = %d, "
               "node->ppty = %d, node->value = %d; node->children_max = %d\n"
               , __func__, key, node->oparg, node->ppty, node->value,
               node->children_max);
      indx = -Error_UnExpectedData;
      goto _exit;
   }

_exit:
   return indx;

_no_child_error:
   error("%s :: %s node has %u children instead of none!\n",
            __func__, opcode_names[key], count_children(node));
   nlnode_print(node, PO_ERROR, true);
   indx = -Error_InvalidValue;
   goto _exit;

_one_child_error:
   error("%s :: %s node has %u children instead of one!\n",
            __func__, opcode_names[key], count_children(node));
   nlnode_print(node, PO_ERROR, true);
   indx = -Error_InvalidValue;
   goto _exit;

_two_children_error:
   error("%s :: %s node has %u children instead of two!\n",
            __func__, opcode_names[key], count_children(node));
   nlnode_print(node, PO_ERROR, true);
   indx = -Error_InvalidValue;
   goto _exit;

_at_least_3_children_error:
   error("%s :: %s node has %u children but should have at least "
                      "3!\n", __func__, opcode_names[key], count_children(node));
   indx = -Error_InvalidValue;
   goto _exit;

_child_expected_error:
   error("%s :: %s has no child when at least one is expected!\n",
         __func__, opcode_names[key]);
   indx = -Error_InvalidValue;
   goto _exit;
}

/**
 * @brief Build the GAMS opcode corresponding to an expression tree
 *
 * @param       ctr      the container object
 * @param       e        the mapping to translate
 * @param[out]  instrs   the instruction array
 * @param[out]  args     the argument array
 * @param[out]  codelen  the length of the opcode
 *
 * @return               the error code
 */
int nltree_buildopcode(Container *ctr, const Equ *e, int **instrs, int **args,
                       int *codelen)
{
   assert(e->tree && e->tree->root);

   /* ----------------------------------------------------------------------
    * Get an upper bound of the opcode length and get a memory large enough to
    * store it.
    *
    * If the estimated size if 0, then there is no opcode and we exit.
    * ---------------------------------------------------------------------- */

   size_t nlcode_size_ub = _nltree_getsizeopcode(e);

   if (nlcode_size_ub == 0) {
      *codelen = 0;
      *instrs = NULL;
      *args = NULL;

      return OK;
   }

   struct ctrmem working_mem = {.ptr = NULL, .ctr = ctr};
   A_CHECK(working_mem.ptr, (int*)ctr_getmem(ctr, (2*sizeof(int)*nlcode_size_ub)));
   *instrs = (int*)working_mem.ptr;
   *args = &(*instrs)[nlcode_size_ub];

   int *linstrs = *instrs;
   int *largs = *args;

   assert(valid_ei(e->idx));
   linstrs[0] = nlStore;
   largs[0] = e->idx + 1;
   int indx = 1;

   /* ----------------------------------------------------------------------
    *
    * This function does all the heavy lifting of converting the opcode
    *
    * - If the return value is positive, it is equal to the length of the
    *   generated opcode
    * - Otherwise, it contains the opposite of the error code and we bail out
    *
    * ---------------------------------------------------------------------- */

   indx = build_gams_opcode_v2(e->tree->root, linstrs, largs, indx);

   if (indx >= 0) {
      assert(indx <= nlcode_size_ub);

      linstrs[indx] = nlHeader;
      largs[indx] = indx+1;
      indx++;
   } else {
      error("[gams_genopcode] ERROR: failed to build GAMS opcode for equation '%s'\n",
            ctr_printequname(ctr, e->idx));
      return -indx;
   }

   /* ----------------------------------------------------------------------
    * Reverse the list of instructions and opcode
    * \TODO(xhub) see if we can  optimize that part
    * ---------------------------------------------------------------------- */

   for (unsigned i = 0, j = indx-1; i < j; ++i, --j) {
     int instr = linstrs[i];
     int arg = largs[i];
     linstrs[i] = linstrs[j];
     largs[i] = largs[j];
     linstrs[j] = instr;
     largs[j] = arg;
   }

   assert(chk_gms_opcode(linstrs, largs, indx, /* look odd, just for test tree*/
                         ctr->equs ? ctr_printequname(ctr, e->idx) : "EMPTY CTR"));

   *codelen = indx;

   return OK;
}
