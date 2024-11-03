#define VISIT_OPARG(node, ctr, env, is_first) \
  if (node->oparg == NLNODE_OPARG_VAR) { \
     VAR_PRE(node, ctr, env); \
     VAR_IN(node, ctr, env); \
     VAR_POST(node, ctr, env); \
     is_first = false; \
  } else if (node->oparg == NLNODE_OPARG_CST) { \
     CST_PRE(node, ctr, env); \
     CST_IN(node, ctr, env); \
     CST_POST(node, ctr, env); \
     is_first = false; \
  } \



#define VISIT_N_CHILDREN(OP) { \
  OP ## _PRE(node, ctr, env); \
  bool is_first = true; \
  VISIT_OPARG(node, ctr, env, is_first) \
  for (size_t i = 0; i < node->children_max; ++i) { \
     if (!node->children[i]) { continue; } \
     if (!is_first) { \
       OP ## _IN(node, ctr, env); \
     } \
     VISIT_NODE(node->children[i], ctr, env); \
     is_first = false; \
  } \
  OP ## _POST(node, ctr, env); \
}

static int NONNULL_AT(2,3)
VISIT_NODE(const NlNode * restrict node,
           const Container *ctr,
           ENV_TYPE env)
{
   if (!node) {
      return OK;
   }

   const NlNodeOp key = node->op;

   switch (key) {

      case NlNode_Add:
         VISIT_N_CHILDREN(ADD);
         break;
      case NlNode_Sub:
         VISIT_N_CHILDREN(SUB);
         break;
      case NlNode_Mul:
         VISIT_N_CHILDREN(MUL);
         break;

      case NlNode_Umin:
      {
         bool dummy = true;
         UMIN_PRE(node, ctr, env);
         VISIT_OPARG(node, ctr, env, dummy);
         if (dummy) {
            VISIT_NODE(node->children[0], ctr, env);
         }
         UMIN_IN(node, ctr, env);
         UMIN_POST(node, ctr, env);
      }
         return OK;

      case NlNode_Div:
      {
         /*  DIV is a weird case */
         bool is_first = true;
         DIV_PRE(node, ctr, env);
         VISIT_OPARG(node, ctr, env, is_first);
         if (!is_first) {
            DIV_IN(node, ctr, env) 
         }
         VISIT_NODE(node->children[0], ctr, env);
         if (is_first) {
            assert(node->children_max == 2);
            DIV_IN(node, ctr, env) 
            VISIT_NODE(node->children[1], ctr, env);
         } else {
            assert(node->children_max == 1);
         }
         DIV_POST(node, ctr, env)
         return OK;
      }

      case NlNode_Call1:
         assert(node->value <= fndummy);
         CALL1_PRE(node, ctr, env);
         S_CHECK(VISIT_NODE(node->children[0], ctr, env));
         CALL1_IN(node, ctr, env);
         CALL1_POST(node, ctr, env);
         return OK;

      case NlNode_Call2:
         assert(node->value <= fndummy);
         CALL2_PRE(node, ctr, env);
         S_CHECK(VISIT_NODE(node->children[0], ctr, env));
         CALL2_IN(node, ctr, env);
         S_CHECK(VISIT_NODE(node->children[1], ctr, env));
         CALL2_POST(node, ctr, env);
         return OK;

      case NlNode_Var:
         VAR_PRE(node, ctr, env);
         VAR_IN(node, ctr, env);
         VAR_POST(node, ctr, env);
         return OK;

      case NlNode_Cst:
         CST_PRE(node, ctr, env);
         CST_IN(node, ctr, env);
         CST_POST(node, ctr, env);
         return OK;

      default:
         error("%s :: unknown opcode %s (%d); node->oparg = %d, "
                  "node->ppty = %d, node->value = %d; node->children_max = %d\n"
                  , __func__, opcode_names[key], key, node->oparg, node->ppty, node->value,
                  node->children_max);
         return Error_UnExpectedData;
      }

   return OK;
}
