#define NLTREE_PRINT DPRINT

EVAL_DEF(const NlNode * restrict node, double * restrict val);

GETVALUE_DEF(const NlNode * restrict node, double * restrict v)
{
  /*  TODO(xhub) review optype field usage */
  /*  TODO(xhub) document this */

   NLTREE_PRINT("%s :: key = %s\n", __func__, opcode_names[node->op]);

   switch (node->oparg) {
      case NLNODE_OPARG_CST:
      {
         if (node->op == NlNode_Cst) {
            *v = POOL_VAL(node->value);
            NLTREE_PRINT("%s :: pool %d = %e\n", __func__, _CIDX_R(node->value), *v);
         } else {
            S_CHECK(EVAL(node, v));
         }
         break;
      }
      case NLNODE_OPARG_VAR:
      {
         double vv = VAR_VAL(node->value);
         NLTREE_PRINT_VAR(node->value, vv);

         if (node->op == NlNode_Umin) {
            *v = -vv;
         } else if (node->op == NlNode_Var) {
            *v = vv;
         } else {
            S_CHECK(EVAL(node, v));
         }
         break;
      }

      case NLNODE_OPARG_FMA:
      {
         if (node->op == NlNode_Mul) {
            double vv = POOL_VAL(node->value);
            NLTREE_PRINT("%s :: pool %d = %e for FMA\n", __func__, _CIDX_R(node->value), vv);
            double vnode;
            S_CHECK(EVAL(node->children[0], &vnode));
            *v = vnode*vv;
            break;
         }
      }
         FALLTHRU

      default:
         S_CHECK(EVAL(node, v));
   }

   return OK;
}

EVAL_DEF(const NlNode * restrict node, double * restrict val)
{
   if (!node) {
      return OK;
   }

   const NlNodeOp key = node->op;

   switch (key) {

      case NlNode_Add:
      case NlNode_Mul:
      {
         double vv = 0;
         double v1 = NAN;
         ptrdiff_t start = 0;

         for (ptrdiff_t i = node->children_max-1; i >= 0; --i) {
            NlNode *child = node->children[i];
            if (!child) continue;
            start = i-1;
            S_CHECK(GET_VALUE(child, &vv));
            NLTREE_PRINT("%s :: key %s; value = %e\n", __func__, opcode_names[key], vv);
            break;
         }

         NLTREE_PRINT("%s :: start = %zd\n", __func__, start);
         for (ptrdiff_t i = start; i >= 0; --i) {
            NlNode *child = node->children[i];
            if (!child) continue;
            S_CHECK(GET_VALUE(child, &v1));

            switch (key) {
            case NlNode_Add:
               NLTREE_PRINT("%s :: %e + %e = %e\n", __func__, vv, v1, vv+v1);
               vv += v1;
         break;
            case NlNode_Mul:
               NLTREE_PRINT("%s :: %e * %e = %e\n", __func__, vv, v1, vv*v1);
               vv *= v1;
               break;
         default:
               ;
            }
         }

         if (node->oparg == NLNODE_OPARG_CST) {
            v1 = POOL_VAL(node->value);
            NLTREE_PRINT("%s :: pool %d = %e\n", __func__, _CIDX_R(node->value), v1);

         } else if (node->oparg == NLNODE_OPARG_VAR) {
            v1 = VAR_VAL(node->value);
            NLTREE_PRINT_VAR(node->value, v1);

         } else {
            *val = vv;
            NLTREE_PRINT("%s :: key = %s; value = %e\n", __func__, opcode_names[key], *val);
            return OK;
         }

         switch (key) {
         case NlNode_Add:
            NLTREE_PRINT("%s :: %e + %e = %e\n", __func__, vv, v1, vv+v1);
            vv += v1;
            break;
         case NlNode_Mul:
            NLTREE_PRINT("%s :: %e * %e = %e\n", __func__, vv, v1, vv*v1);
            vv *= v1;
            break;
         default:
           ;
         }

         *val = vv;
         NLTREE_PRINT("%s :: key = %s @ %p; value = %e\n", __func__, opcode_names[key],
                (void*)node, *val);
         return OK;
      }

      case NlNode_Sub: {
         double res;
         double v1 = NAN;
         unsigned len = node->children_max, start = len-1; assert(len > 0);
        /* ----------------------------------------------------------------------
         * SUB has the folllowing convention: child1 - child0.
         *
         * FIXME: This is broken for more than 2 children
         *
         * - child0 is loaded in res
         * - 
         *
         * ---------------------------------------------------------------------- */

         if (len > 2) {
            error("[nltree] ERROR: for sub node, only 2 children are supported for now, got %u\n", len);
            return Error_NotImplemented;
         }

         NLTREE_PRINT("START: key = %s @ %p;\n", opcode_names[key], (void*)node);

         if (node->oparg == NLNODE_OPARG_CST) {
            res = POOL_VAL(node->value);
            NLTREE_PRINT("%s :: oparg pool %d = %e\n", __func__, _CIDX_R(node->value), res);

         } else if (node->oparg == NLNODE_OPARG_VAR) {
            res = VAR_VAL(node->value);
            NLTREE_PRINT_VAR(node->value, res);

         } else {

            for (; start < len; --start) {
               NlNode *child = node->children[start];
               if (!child) continue;
               S_CHECK(GET_VALUE(child, &res));
               start--;
               break;
            }

            if (start >= len) {
               errormsg("[nltree] ERROR: for sub node, only found 1 valid child, need 2\n");
               return Error_RuntimeError;
            }
         }

         NLTREE_PRINT("%s :: start = %zd\n", __func__, start);
         for (; start < len; --start) {
            NlNode *child = node->children[start];
            if (!child) continue;
            S_CHECK(GET_VALUE(child, &v1));

            NLTREE_PRINT("%s :: %e - %e = %e\n", __func__, res, v1, res-v1);
            res -= v1;
         }

         *val = res;
         NLTREE_PRINT("END: key = %s @ %p; value = %e\n", opcode_names[key],
                (void*)node, *val);
         return OK;
      }
 
      case NlNode_Umin:
         if (node->oparg == NLNODE_OPARG_VAR) /*  we have a variable */ {
            (*val) = - VAR_VAL(node->value);
            NLTREE_PRINT("%s :: -var %d = %e\n", __func__, VIDX_R(node->value), *val);
         } else {
            assert(node->oparg == NLNODE_OPARG_UNSET);
            S_CHECK(EVAL(node->children[0], val));
            (*val) = -*val;
         }
         NLTREE_PRINT("%s :: key = %s @ %p; value = %e\n", __func__, opcode_names[key],
                (void*)node, *val);
         return OK;

      case NlNode_Div:
      {
         /*  DIV is a weird case */
         assert(key != NlNode_Div || node->children_max <= 2);
         double x1 = NAN, x2 = NAN;
         unsigned char x1_idx;

         if (node->oparg == NLNODE_OPARG_CST) {
            x2 = POOL_VAL(node->value);
            NLTREE_PRINT("%s :: pool %d = %e\n", __func__, _CIDX_R(node->value), x2);
            x1_idx = 0;
         } else if (node->oparg == NLNODE_OPARG_VAR) {
            x2 = VAR_VAL(node->value);
            NLTREE_PRINT("%s :: var %d = %e\n", __func__, VIDX_R(node->value), x2);
            x1_idx = 0;
         } else {
            S_CHECK(EVAL(node->children[0],  &x2));
            x1_idx = 1;
         }
         S_CHECK(EVAL(node->children[x1_idx],  &x1));
         *val = x1/x2;
         NLTREE_PRINT("%s :: %e / %e = %e\n", __func__, x1, x2, x1/x2);
         NLTREE_PRINT("%s :: key = %s @ %p; value = %e\n", __func__, opcode_names[key],
                (void*)node, *val);
         return OK;
      }

      case NlNode_Call1:
      {
         assert(node->value <= fndummy);
         fnarg1 fn = (fnarg1)func_call[node->value];
         double x1 = NAN;
         S_CHECK(EVAL(node->children[0],  &x1));
         errno = 0;
         feclearexcept(FE_ALL_EXCEPT);
         (*val) = (*fn)(x1);
         NLTREE_PRINT("%s :: calling %s on %e = %e\n", __func__,
                func_code_name[node->value], x1, *val);
         S_CHECK(_check_math_error1(node->value, x1));
         NLTREE_PRINT("%s :: key = %s @ %p; value = %e\n", __func__, opcode_names[key],
                (void*)node, *val);
         return OK;
      }

      case NlNode_Call2:
      {
         assert(node->value <= fndummy);
         fnarg2 fn = (fnarg2)func_call[node->value];
         double x1 = NAN, x2 = NAN;
         S_CHECK(EVAL(node->children[0],  &x1));
         S_CHECK(EVAL(node->children[1],  &x2));
         errno = 0;
         feclearexcept(FE_ALL_EXCEPT);
         (*val) = (*fn)(x1, x2);
         NLTREE_PRINT("%s :: calling %s on (%e,%e) = %e\n", __func__,
                func_code_name[node->value], x1, x2, *val);
         S_CHECK(_check_math_error2(node->value, x1, x2));
         NLTREE_PRINT("%s :: key = %s @ %p; value = %e\n", __func__, opcode_names[key],
                (void*)node, *val);
         return OK;
      }

      case NlNode_Var:
         (*val) = VAR_VAL(node->value);
         NLTREE_PRINT_VAR(node->value, *val);
         NLTREE_PRINT("%s :: key = %s @ %p; value = %e\n", __func__, opcode_names[key],
                (void*)node, *val);
         return OK;

      case NlNode_Cst:
         (*val) = POOL_VAL(node->value);
         NLTREE_PRINT("%s :: pool %d = %e\n", __func__, _CIDX_R(node->value), *val);
         NLTREE_PRINT("%s :: key = %s @ %p; value = %e\n", __func__, opcode_names[key],
                (void*)node, *val);
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
