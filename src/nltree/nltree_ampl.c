#include <math.h>

#include "ampl_opcode.hd"
#include "container.h"
#include "equ.h"
#include "nltree.h"
#include "nltree_priv.h"
#include "var.h"

static int _ampl_translate_opcode(FILE *stream, enum NLNODE_OP key)
{
  int ampl_opcode;

   switch (key) {

   case NLNODE_ADD:
      ampl_opcode = OPPLUS;
      break;
   case NLNODE_SUB:
      ampl_opcode = OPMINUS;
      break;
   case NLNODE_MUL:
      ampl_opcode = OPMULT;
      break;
   case NLNODE_DIV:
      ampl_opcode = OPDIV;
      break;
   default:
      error("%s :: unexpected error\n", __func__);
      return Error_UnExpectedData;
   }

   fprintf(stream, "o%d\n", ampl_opcode);

   return OK;
}

static int _ampl_opcode(unsigned value)
{
   switch (value) {
   case fnmapval:
      return -1;
   case fnceil:
      return CEIL;
   case fnfloor:
      return FLOOR;
   case fnround:
      return OPround;
   case fnmod:
      return -1;
   case fntrunc:
      return OPtrunc;             /*  5 */
   case fnsign:
      return -1;
   case fnmin:
      return MINLIST;
   case fnmax:
      return MAXLIST;
   case fnsqr:
      return OP2POW;
   case fnexp:
      return OP_exp;               /* 10 */
   case fnlog:
      return OP_log;
   case fnlog10:
      return OP_log10;
   case fnsqrt:
      return OP_sqrt;
   case fnabs:
      return ABS;
   case fncos:
      return OP_cos;               /* 15 */
   case fnsin:
      return OP_sin;
   case fnarctan:
      return OP_atan; /* TODO(xhub) or atan2?  */
   case fnerrf:
      return -1;
   case fndunfm:
      return -1;
   case fndnorm:
      return -1;             /* 20 */
   case fnpower:
      return OPPOW;
   case fnjdate:
      return -1;
   case fnjtime:
      return -1;
   case fnjstart:
      return -1;
   case fnjnow:
      return -1;              /* 25 */
   case fnerror:
      return -1;
   case fngyear:
      return -1;
   case fngmonth:
      return -1;
   case fngday:
      return -1;
   case fngdow:
      return -1;              /* 30 */
   case fngleap:
      return -1;
   case fnghour:
      return -1;
   case fngminute:
      return -1;
   case fngsecond:
      return -1;
   case fncurseed:
      return -1;           /* 35 */
   case fntimest:
      return -1;
   case fntimeco:
      return -1;
   case fntimeex:
      return -1;
   case fntimecl:
      return -1;
   case fnfrac:
      return -1;              /* 40 */
   case fnerrorl:
      return -1;
   case fnheaps:
      return -1;
   case fnfact:
      return -1;
   case fnunfmi:
      return -1;
   case fnpi:
      return -1;                /* 45 */
   case fnncpf:
      return -1;
   case fnncpcm:
      return -1;
   case fnentropy:
      return -1;
   case fnsigmoid:
      return -1;
   case fnlog2:
      return -1;              /* 50 */
   case fnboolnot:
      return -1;
   case fnbooland:
      return -1;
   case fnboolor:
      return -1;
   case fnboolxor:
      return -1;
   case fnboolimp:
      return -1;           /* 55 */
   case fnbooleqv:
      return -1;
   case fnrelopeq:
      return -1;
   case fnrelopgt:
      return -1;
   case fnrelopge:
      return -1;
   case fnreloplt:
      return -1;           /* 60 */
   case fnrelople:
      return -1;
   case fnrelopne:
      return -1;
   case fnifthen:
      return -1;
   case fnrpower:
      return -1;
   case fnedist:
      return -1;             /* 65 */
   case fndiv:
      return -1;
   case fndiv0:
      return -1;
   case fnsllog10:
      return -1;
   case fnsqlog10:
      return -1;
   case fnslexp:
      return -1;             /* 70 */
   case fnsqexp:
      return -1;
   case fnslrec:
      return -1;
   case fnsqrec:
      return -1;
   case fncvpower:
      return -1;
   case fnvcpower:
      return -1;           /* 75 */
   case fncentropy:
      return -1;
   case fngmillisec:
      return -1;
   case fnmaxerror:
      return -1;
   case fntimeel:
      return -1;
   case fngamma:
      return -1;             /* 80 */
   case fnloggamma:
      return -1;
   case fnbeta:
      return -1;
   case fnlogbeta:
      return -1;
   case fngammareg:
      return -1;
   case fnbetareg:
      return -1;           /* 85 */
   case fnsinh:
      return OP_sinh;
   case fncosh:
      return OP_cosh;
   case fntanh:
      return OP_tanh;
   case fnmathlastrc:
      return -1;
   case fnmathlastec:
      return -1;        /* 90 */
   case fnmathoval:
      return -1;
   case fnsignpower:
      return -1;
   case fnhandle:
      return -1;
   case fnncpvusin:
      return -1;
   case fnncpvupow:
      return -1;          /* 95 */
   case fnbinomial:
      return -1;
   case fnrehandle:
      return -1;
   case fngamsver:
      return -1;
   case fndelhandle:
      return -1;
   case fntan:
      return OP_tan;               /* 100 */
   case fnarccos:
      return OP_acos;
   case fnarcsin:
      return OP_asin;
   case fnarctan2:
      return OP_atan2;
   case fnsleep:
      return -1;
   case fnheapf:
      return -1;             /* 105 */
   case fncohandle:
      return -1;
   case fngamsrel:
      return -1;
   case fnpoly:
      return -1;
   case fnlicensestatus:
      return -1;
   case fnlicenselevel:
      return -1;      /* 110 */
   case fnheaplimit:
      return -1;
   case fndummy:
      return -1;
   default:
      return -1;
   }
}

static inline int _ampl_cst(FILE *stream, const NlNode *node,
                            Container *ctr)
{
   double val = ctr_poolval(ctr, _CIDX_R(node->value));
   if (!isfinite(val)) {
     return Error_InvalidValue;
   }
   fprintf(stream, "n%e\n", val);

   return OK;
}

static int _ampl_optype(FILE *stream, const NlNode *node,
                        Container *ctr)
{
   if (node->oparg == NLNODE_OPARG_CST) {
      return _ampl_cst(stream, node, ctr);
   }
   if (node->oparg == NLNODE_OPARG_VAR) {
      fprintf(stream, "v%u\n", VIDX_R(node->value));
   }

   return OK;
}

static int _build_ampl_opcode(const NlNode *node, FILE *stream,
                              Container *ctr)
{
   int status = OK;

   if (!node) {
      return OK;
   }

   const enum NLNODE_OP key = node->op;

   unsigned explore_next_max = 20;
   NlNode **explore_next;

   /* TODO(xhub) keep a cache of explore_next to pass OR
    *     keep explore_next constant do that we don't overflow the stack*/
   MALLOC_(explore_next, NlNode *, explore_next_max);

   switch (key) {

      case NLNODE_VAR:
         if (node->children_max > 0) {
           goto _no_child_error;
         }
         fprintf(stream, "v%u\n", VIDX_R(node->value));
         break;

      case NLNODE_CST:
         if (node->children_max > 0) {
           goto _no_child_error;
         }
         if (node->value > 0) {
            /* TODO(xhub) support max precision */
            S_CHECK_EXIT(_ampl_cst(stream, node, ctr));
         } else {
            fputs("n0\n", stream);
         }
         break;

      case NLNODE_ADD:
      case NLNODE_SUB:
      case NLNODE_MUL:
      case NLNODE_DIV:
      {
         bool valid_optype = (node->oparg == NLNODE_OPARG_CST ||
                              node->oparg == NLNODE_OPARG_VAR);
         unsigned nb_children = valid_optype ? node->children_max + 1 :
                                               node->children_max;
         size_t explore_indx = 0;

         if (nb_children > 2) {
            if (key != NLNODE_ADD) {
               error("%s :: node has more then 2 children\n",
                               __func__);
            } else {
               fprintf(stream, "o%u\n%u\n", (unsigned)OPSUMLIST, nb_children);
            }
         } else {
            S_CHECK_EXIT(_ampl_translate_opcode(stream, key));
         }

         if (explore_next_max < node->children_max) {
            explore_next_max = node->children_max;
            FREE(explore_next);
            MALLOC_(explore_next, NlNode *, explore_next_max);
         }

         S_CHECK_EXIT(_ampl_optype(stream, node, ctr));

         for (int i = node->children_max-1; i >= 2; --i) {
            NlNode *child = node->children[i];
            if (!child) continue;
            switch (child->oparg) {
            case NLNODE_OPARG_CST:
            case NLNODE_OPARG_VAR:
            {
               /*  XXX review that */
               if (child->children_max == 0) {
                 /*  TODO(Xhub) why are we using optype and not opcode?*/
                  S_CHECK_EXIT(_ampl_optype(stream, child, ctr));
                  break;
                  /*  TODO(Xhub) this is really weird */
               }
               if (key != NLNODE_ADD && child->children_max != 1) {
                  error("%s :: unsupported case: ", __func__);
                  nlnode_print(child, PO_ERROR, true);
                  status = Error_UnExpectedData;
                  goto _exit;
               }
               goto _default1;
            }
            case NLNODE_OPARG_FMA:
            {
               if (key == NLNODE_ADD) {
                  assert(child->children_max == 1);
                  fputs("o" MACRO_STR(OPMULT) "\n", stream);
                  S_CHECK_EXIT(_ampl_cst(stream, child, ctr));
   //               if (_print_now(child->ppty)) {
                     status = _build_ampl_opcode(child->children[0], stream, ctr);
                     if (status) goto _exit;
               } else {
                  goto _default1;
               }
               break;
            }
_default1:
            default:
            {
               S_CHECK_EXIT(_ampl_translate_opcode(stream, key));
               if (_print_now(child->ppty)) {
                  status = _build_ampl_opcode(child, stream, ctr);
                  /*  TODO(xhub) HIGH   this looks to be wrong, should be if(status) ??? */
                  if (status < 0) goto _exit;
               } else {
                  explore_next[explore_indx++] = child;
                  if (explore_indx >= 3) {
                    printout(PO_DEBUG, "%s :: a lot of children to explore, "
                                       "emptying ...\n", __func__);
                    // Now visit the children in order to print in the correct order
                    for (int j = explore_indx-1; j >= 0; --j) {
                      status = _build_ampl_opcode(explore_next[j], stream, ctr);
                      if (status < 0) goto _exit;
                    }
                    explore_indx = 0;
                  }
               }
            }
            }
         }
         /*  \TODO(xhub) remove this hack */
         if (node->children_max <= 1 || !node->children[1]) {
            goto _skip_child_1;
         }
         switch (node->children[1]->oparg) {
         case NLNODE_OPARG_CST:
         case NLNODE_OPARG_VAR:
            if (node->children[1]->children_max == 0) {
              S_CHECK_EXIT(_ampl_optype(stream, node->children[1], ctr));
               break;
            } else if (key != NLNODE_ADD && node->children[1]->children_max != 1) {
               error("%s :: unsupported case: ", __func__);
               nlnode_print(node->children[1], PO_ERROR, true);
               status = Error_UnExpectedData;
               goto _exit;
            }
            goto _default2;
         case NLNODE_OPARG_FMA:
            if (key == NLNODE_ADD) {
               assert(node->children[1]->children_max == 1);
               fputs("o" MACRO_STR(OPMULT) "\n", stream);
               S_CHECK_EXIT(_ampl_cst(stream, node->children[1], ctr));
            //TODO(xhub) document why we explore this child now
//            if (_print_now(node->children[1]->ppty)) {
//             
               status = _build_ampl_opcode(node->children[1]->children[0], stream,
                                         ctr);
               if (status < 0) goto _exit;
            } else {
               goto _default2;
//               explore_next[explore_indx++] = node->children[1];
//               indx = _build_ampl_opcode(node->children[1], code, indx, ctr);
            }
#if 0
            } else {
               explore_next[explore_indx++] = node->children[1]->children[0];
            }
#endif
            break;
_default2:
         default:
            /* The last child should have no opcode */
            if (node->children[0]) {
               S_CHECK_EXIT(_ampl_translate_opcode(stream, key));
            }
            if (_print_now(node->children[1]->ppty)) {
               status = _build_ampl_opcode(node->children[1], stream, ctr);
               if (status < 0) goto _exit;
            } else {
               explore_next[explore_indx++] = node->children[1];
            }
         }
_skip_child_1:
         // Always visit the child except if it is  linear term
         if (node->children[0]) {
            explore_next[explore_indx++] = node->children[0];
         } else if ((node->children_max < 2 || !node->children[1])
                    && explore_indx <= 2) {
            if (explore_indx == 0) {
               printout(PO_ALLDEST, "%s :: node %s with no child!\n", __func__, 
                        opcode_names[key]);
               goto _exit;

            }
            error("%s :: the first two children of the node are empty. This is "
                  "known to produce broken OPCODE\n", __func__);
            nlnode_print(node, PO_ERROR, true);
            status = Error_InvalidOpCode;
            goto _exit;
         }
         // Now visit the children in order to print in the correct order
         for (int i = explore_indx-1; i >= 0; --i) {
            status = _build_ampl_opcode(explore_next[i], stream, ctr);
            if (status < 0) goto _exit;
         }
         break;
      }

      case NLNODE_UMIN:
         if (node->value > 0) /*  we have a variable */
         {
           if (node->children_max > 0) {
             goto _no_child_error;
           }
           fprintf(stream, "o" MACRO_STR(OPUMINUS) "\nv%u\n", VIDX_R(node->value));
         }
         else
         {
           if (node->children_max != 1) {
             goto _one_child_error;
           }
           fputs("o" MACRO_STR(OPUMINUS) "\n", stream);
            // go visit the child
            S_CHECK_EXIT(_build_ampl_opcode(node->children[0], stream, ctr));
         }
         break;

      case NLNODE_CALL1:
         if (node->children_max != 1) {
           goto _one_child_error;
         }
         fprintf(stream, "o%d\n", _ampl_opcode(node->value));
         // go visit the child
         S_CHECK_EXIT(_build_ampl_opcode(node->children[0], stream, ctr));
         break;

      case NLNODE_CALL2:
         if (node->children_max != 2) {
           goto _two_children_error;
         }
         fprintf(stream, "o%d\n", _ampl_opcode(node->value));
         // go visit the child[1]
         S_CHECK_EXIT(_build_ampl_opcode(node->children[1], stream, ctr));
         // go visit the child[0]
         S_CHECK_EXIT(_build_ampl_opcode(node->children[0], stream, ctr));
         break;

      case NLNODE_CALLN:
         if (node->children_max <= 2) {
           goto _at_least_3_children_error;
         }

         fprintf(stream, "o%d\n%u\n", _ampl_opcode(node->value), node->children_max);

         /* ----------------------------------------------------------------
          * This is a bit special, we need to inject an instruction that
          * indicates the number of children
          * ---------------------------------------------------------------- */

         for (unsigned i = 0, j = node->children_max-1; i < node->children_max; ++i, --j) {
            S_CHECK_EXIT(_build_ampl_opcode(node->children[j], stream, ctr));
         }

         break;

      default:
         error("%s :: unknown opcode %d; node->oparg = %d, "
                  "node->ppty = %d, node->value = %d; node->children_max = %d\n"
                  , __func__, key, node->oparg, node->ppty, node->value,
                  node->children_max);
         status = Error_UnExpectedData;
         goto _exit;
      }

_exit:
   FREE(explore_next);

   return status;

_no_child_error:
   error("%s :: %s node has %d children instead of none!\n",
            __func__, opcode_names[key], node->children_max);
   nlnode_print(node, PO_ERROR, true);
   status = Error_InvalidValue;
   goto _exit;

_one_child_error:
   error("%s :: %s node has %d children instead of one!\n",
            __func__, opcode_names[key], node->children_max);
   nlnode_print(node, PO_ERROR, true);
   status = Error_InvalidValue;
   goto _exit;

_two_children_error:
   error("%s :: %s node has %d children instead of two!\n",
            __func__, opcode_names[key], node->children_max);
   nlnode_print(node, PO_ERROR, true);
   status = Error_InvalidValue;
   goto _exit;
_at_least_3_children_error:
   error("%s :: %s node has %d children but should have at least "
                      "3!\n", __func__, opcode_names[key], node->children_max);
   status = Error_InvalidValue;
   goto _exit;
}

/**
 * @brief Output the nonlinear part of the equation as an NL file
 *
 * @param ctr     the model
 * @param e       the equation
 * @param stream  the stream where to output the NL format
 *
 * @return        the error code
 */
static int nltree_outputNL(Container *ctr, Equ * e, FILE *stream)
{
   assert(e->tree);
   assert(e->tree->root);
   assert(stream);


   assert(valid_ei(e->idx));

   /* ----------------------------------------------------------------------
    *
    * This function does all the heavy lifting of converting the opcode
    *
    * - If the return value is positive, it is equal to the length of the
    * generated opcode
    * - Otherwise, it contains the opposite of the error code and we bail out
    *
    * ---------------------------------------------------------------------- */

   return _build_ampl_opcode(e->tree->root, stream, ctr);

}
