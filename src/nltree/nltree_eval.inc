EVAL_DEF(const NlNode * restrict node, double * restrict val);

GETVALUE_DEF(const NlNode * restrict node, double * restrict v)
{
  /*  TODO(xhub) review optype field usage */
  /*  TODO(xhub) document this */

   DPRINT("%s :: key = %s\n", __func__, opcode_names[node->op]);

   switch (node->oparg) {
      case NLNODE_OPARG_CST:
      {
         if (node->op == NlNode_Cst) {
            *v = POOL_VAL(node->value);
            DPRINT("%s :: pool %d = %e\n", __func__, _CIDX_R(node->value), *v);
         } else {
            S_CHECK(EVAL(node, v));
         }
         break;
      }
      case NLNODE_OPARG_VAR:
      {
         double vv = VAR_VAL(node->value);
         _DPRINT_VAR(node->value, vv);

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
            DPRINT("%s :: pool %d = %e for FMA\n", __func__, _CIDX_R(node->value), vv);
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
      case NlNode_Sub:
      {
         double vv = 0;
         double v1 = NAN;
         ptrdiff_t start = 0;

         for (ptrdiff_t i = node->children_max-1; i >= 0; --i) {
            NlNode *child = node->children[i];
            if (!child) continue;
            start = i-1;
            S_CHECK(GET_VALUE(child, &vv));
            DPRINT("%s :: value = %e\n", __func__, vv);
            break;
         }

         DPRINT("%s :: start = %d\n", __func__, start);
         for (ptrdiff_t i = start; i >= 0; --i) {
            NlNode *child = node->children[i];
            if (!child) continue;
            S_CHECK(GET_VALUE(child, &v1));

            switch (key) {
            case NlNode_Add:
               DPRINT("%s :: %e + %e = %e\n", __func__, vv, v1, vv+v1);
               vv += v1;
               break;
            case NlNode_Mul:
               DPRINT("%s :: %e * %e = %e\n", __func__, vv, v1, vv*v1);
               vv *= v1;
               break;
            case NlNode_Sub:
               DPRINT("%s :: %e - %e = %e\n", __func__, vv, v1, vv-v1);
               vv -= v1;
               break;
            default:
               ;
            }
         }
         if (node->oparg == NLNODE_OPARG_CST) {
            v1 = POOL_VAL(node->value);
            DPRINT("%s :: pool %d = %e\n", __func__, _CIDX_R(node->value), v1);

         } else if (node->oparg == NLNODE_OPARG_VAR) {
            v1 = VAR_VAL(node->value);
            _DPRINT_VAR(node->value, v1);

         } else {
            *val = vv;
            DPRINT("%s :: key = %s; value = %e\n", __func__, opcode_names[key], *val);
            return OK;
         }

         switch (key) {
         case NlNode_Add:
            DPRINT("%s :: %e + %e = %e\n", __func__, vv, v1, vv+v1);
            vv += v1;
            break;
         case NlNode_Mul:
            DPRINT("%s :: %e * %e = %e\n", __func__, vv, v1, vv*v1);
            vv *= v1;
            break;
         case NlNode_Sub:
           DPRINT("%s :: %e - %e = %e\n", __func__, vv, v1, vv-v1);
           vv -= v1;
           break;
         default:
           ;
         }

         *val = vv;
         DPRINT("%s :: key = %s @ %p; value = %e\n", __func__, opcode_names[key],
                (void*)node, *val);
         return OK;
      }

      case NlNode_Umin:
         if (node->oparg == NLNODE_OPARG_VAR) /*  we have a variable */ {
            (*val) = - VAR_VAL(node->value);
            DPRINT("%s :: -var %d = %e\n", __func__, VIDX_R(node->value), *val);
         } else {
            assert(node->oparg == NLNODE_OPARG_UNSET);
            S_CHECK(EVAL(node->children[0], val));
            (*val) = -*val;
         }
         DPRINT("%s :: key = %s @ %p; value = %e\n", __func__, opcode_names[key],
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
            DPRINT("%s :: pool %d = %e\n", __func__, _CIDX_R(node->value), x2);
            x1_idx = 0;
         } else if (node->oparg == NLNODE_OPARG_VAR) {
            x2 = VAR_VAL(node->value);
            DPRINT("%s :: var %d = %e\n", __func__, VIDX_R(node->value), x2);
            x1_idx = 0;
         } else {
            S_CHECK(EVAL(node->children[0],  &x2));
            x1_idx = 1;
         }
         S_CHECK(EVAL(node->children[x1_idx],  &x1));
         *val = x1/x2;
         DPRINT("%s :: %e / %e = %e\n", __func__, x1, x2, x1/x2);
         DPRINT("%s :: key = %s @ %p; value = %e\n", __func__, opcode_names[key],
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
         DPRINT("%s :: calling %s on %e = %e\n", __func__,
                func_code_name[node->value], x1, *val);
         S_CHECK(_check_math_error1(node->value, x1));
         DPRINT("%s :: key = %s @ %p; value = %e\n", __func__, opcode_names[key],
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
         DPRINT("%s :: calling %s on (%e,%e) = %e\n", __func__,
                func_code_name[node->value], x1, x2, *val);
         S_CHECK(_check_math_error2(node->value, x1, x2));
         DPRINT("%s :: key = %s @ %p; value = %e\n", __func__, opcode_names[key],
                (void*)node, *val);
         return OK;
      }

      case NlNode_Var:
         (*val) = VAR_VAL(node->value);
         _DPRINT_VAR(node->value, *val);
         DPRINT("%s :: key = %s @ %p; value = %e\n", __func__, opcode_names[key],
                (void*)node, *val);
         return OK;

      case NlNode_Cst:
         (*val) = POOL_VAL(node->value);
         DPRINT("%s :: pool %d = %e\n", __func__, _CIDX_R(node->value), *val);
         DPRINT("%s :: key = %s @ %p; value = %e\n", __func__, opcode_names[key],
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
