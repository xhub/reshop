#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <stdbool.h>

#include "compat.h"
#include "container.h"
#include "consts.h"
#include "equ.h"
#include "nltree.h"
#include "gams_nlutils.h"
#include "lequ.h"
#include "macros.h"
#include "mdl.h"
#include "pool.h"
#include "status.h"

#include "test_instr.h"
//#define DOT_PRINT

#ifdef __GNUC__
_Pragma("GCC diagnostic push")
_Pragma("GCC diagnostic ignored \"-Wfree-nonheap-object\"")
#endif

#define OPCODE_SIZE(opcode) (sizeof(opcode) / sizeof(unsigned))

/** @brief copy an equation and store the node location of some variable if asked.
 *
 * When nb_var is positive and var_indices non-null, the node where any of the
 * variable listing in var_indices appears is saved in an internal structure of
 * the tree. This makes the search for those variable very quick.
 * 
 * @param equ equation to be copied
 * @param nb_var number of variable to look for
 * @param var_indices list of variable index to look for
 *
 * @return a copy of the equation
 */
/*  TODO(xhub) use an avar */
static struct equ* equ_copy(const struct equ *equ, unsigned nb_var, const unsigned * var_indices)
{
   struct equ *copy;
   AA_CHECK(copy, equ_alloc(equ->lequ->max));

   copy->basis = equ->basis;
   copy->cone = equ->cone;
   copy->is_quad = equ->is_quad;
   /*  TODO(xhub) CONE support */
   copy->p = equ->p;
   /* \TODO(xhub) make sure we really want to do that */
   copy->value = equ->value;
   copy->multiplier = equ->multiplier;

   /* We don't copy the nonlinear opcode, they can't be modified */
   SN_CHECK_EXIT(lequ_copy(copy->lequ, equ->lequ))
   AA_CHECK_EXIT(copy->tree, nltree_dup(equ->tree, var_indices, nb_var));

   /* This is a tiny bit weird, but */
   copy->idx = equ->idx;
   copy->tree->idx = equ->idx;

   return copy;

_exit:
   equ_dealloc(&copy);
   return NULL ;
}

const int * const opcodes[] = {
      ex1_opcodes, ex2_opcodes, ex3_opcodes, ex4_opcodes, ex5_opcodes,
      ex6_opcodes, ex7_opcodes, ex8_opcodes,
};

const int* const args[] = {
      ex1_fields, ex2_fields, ex3_fields, ex4_fields, ex5_fields,
      ex6_fields, ex7_fields, ex8_fields,
   };

const unsigned sizes[] = {
      sizeof(ex1_opcodes), sizeof(ex2_opcodes), sizeof(ex3_opcodes),
      sizeof(ex4_opcodes), sizeof(ex5_opcodes), sizeof(ex6_opcodes),
      sizeof(ex7_opcodes), sizeof(ex8_opcodes),
   };

static bool is_oppower(int instr, int arg)
{
   if (instr != nlCallArg2) {
      return false;
   }

   switch (arg) {
   case fnrpower:
   case fncvpower:
   case fnvcpower:
      return true;

   default:
      return false;
   }
}

static unsigned _get_10_var(int *instrs_, int* args_, rhp_idx* v_list, int min_var_indx)
{

   unsigned n_var = 0;

   size_t codelen = args_[0];

   for (size_t i = 0; i < codelen; ++i) {
      switch (instrs_[i]) {
      case nlPushV:
      case nlAddV:
      case nlSubV:
      case nlMulV:
      case nlDivV:
         if (min_var_indx < args_[i]) {
            v_list[n_var++] = args_[i];
            if (n_var >= 10) goto _exit;
         }
         break;
      default:
         break;
      }
   }

_exit:
   return n_var;
}

static bool isdiveq(UNUSED unsigned op1, UNUSED unsigned op2, UNUSED unsigned opp2)
{
//   if (op1 == nlDivV && op2 == nlPushV && opp2 == nlDiv) return true;
   return false;
}

static int _check_no_diff(const int *instrs1, const int *args1, const int *instrs2,
                          const int *args2)
{
   /* We skip the first   */
   size_t i = 1;
   size_t j = 1;

   size_t codelen1 = (size_t)args1[0];
   size_t codelen2 = (size_t)args2[0];

   while(i < codelen1 && j < codelen2)
   {
      if ((instrs1[i] != instrs2[j])
          || (args1[i] != args2[j])) {
         /* power type opcode are ``equivalent''  */
         if (is_oppower(instrs1[i], args1[i]) && is_oppower(instrs2[j], args2[j])) goto _incr;
         if (j+1 < codelen2 && isdiveq(instrs1[i],
                           instrs2[j], instrs2[j+1])) {
            i++; j += 2;
            continue;
         }
         else if (i+1 < codelen1 && isdiveq(instrs2[j],
                           instrs1[i], instrs1[i+1])) {
            i += 2; ++j;
         }

         goto _error;
      }
_incr:
   i++; j++;

   }

   if (i == codelen1 && j == codelen2) return OK;

_error:

   printf("ERROR: (codelen1  = %zu, codelen2 = %zu\n", codelen1, codelen2);
   printf("failure at indx1 = %zu and indx2 = %zu\n", i, j);
   printf("   instr = %s arg = %5d\n", instr_code_name(instrs1[i]), args1[i]);
   printf("   instr = %s arg = %5d\n", instr_code_name(instrs2[j]), args2[j]);

   return Error_InvalidValue;
}

static int _run_ex(Container *ctr, const int *instrs1, const int *args1)
{
   int status = OK;

   struct equ* equ2;
   A_CHECK_EXIT(equ2, equ_alloc(0));
   struct equ* equbis = NULL;
   struct equ* equter = NULL;
   struct equ* small_equ = NULL;
   status = equ_nltree_fromgams(equ2, args1[0], instrs1, args1);
   if (status) goto _exit;

   char *dot_print = getenv("DOT_PRINT");
   char dotname[256];

   if (dot_print) {
      sprintf(dotname, "equ%p.dot", (void*)instrs1);
      nltree_print_dot(equ2->tree, dotname, NULL);
   }

   int *instrs2;
   int *args2;
   int codelen2;
   status = nltree_buildopcode(ctr, equ2, &instrs2, &args2, &codelen2);
   if (status) goto _exit;

   status = _check_no_diff(instrs1, args1, instrs2, args2);
   if (status != OK) goto _exit;

   equbis = equ_copy(equ2, 0, NULL);
   if (!equbis) { status = -1; goto _exit; }

   int *instrs3;
   int *args3;
   int codelen3;
   status = nltree_buildopcode(ctr, equbis, &instrs3, &args3, &codelen3);
   if (status != OK) goto _exit;

   if (dot_print) {
      sprintf(dotname, "equ3%p.dot", (void*)instrs1);
      nltree_print_dot(equbis->tree, dotname, NULL);
   }

   status = _check_no_diff(instrs1, args1, instrs3, args3);
   if (status != OK) goto _exit;

   rhp_idx var_list[10];
   unsigned var_len = _get_10_var(instrs3, args3, var_list, 3);
//   equter = equ_copy(equ2, var_len, var_list);


   for (size_t i = 0; i < 8; ++i) {
      size_t size = sizes[i]/sizeof(int);
      A_CHECK_EXIT(small_equ, equ_alloc(0));

      S_CHECK_EXIT(equ_nltree_fromgams(small_equ, size, opcodes[i], args[i]));

      if (dot_print) {
         sprintf(dotname, "small_equ_%zu.dot", i);
         nltree_print_dot(small_equ->tree, dotname, NULL);
      }

      for (unsigned j = 0; j < var_len ; ++j)
      {
         rhp_idx vi = var_list[j];
         equter = equ_copy(equ2, 0, NULL);
         if (!equter) { status = -1; goto _exit; }
         S_CHECK_EXIT(nltree_replacevarbytree(equter->tree, vi, small_equ->tree));

         int *instrd;
         int *argsd;
         int codelend;

         S_CHECK_EXIT(nltree_buildopcode(ctr, equter, &instrd, &argsd, &codelend));

         if (dot_print) {
            sprintf(dotname, "equ%p__var%d_p%zu.dot", (void*)instrd, vi, i);
            nltree_print_dot(equter->tree, dotname, NULL);
         }

         equ_dealloc(&equter);
      }

      equ_dealloc(&small_equ);


   }

   goto _exit;

   //S_CHECK(_check_no_diff(instrs1, args1, instrs2, args2));

_exit:
   equ_dealloc(&equ2);
   equ_dealloc(&equbis);
   equ_dealloc(&equter);
   equ_dealloc(&small_equ);
   return status;
}

#if 0
static int run_ex(Container *ctr, unsigned *ex_opcodes, int *ex_fields,
                  unsigned size)
{
   return _run_ex(ctr, (int*)ex_opcodes, ex_fields);
}
#endif

static int run_ex2(Container *ctr, const int *isntrs_, const int *args_)
{
   int status = _run_ex(ctr, isntrs_, args_);

   return status;
}

int main(int argc, char **argv)
{
   double *pool2;
   struct rhp_mdl *mdl;
   A_CHECK(mdl, rhp_mdl_new(RHP_BACKEND_RHP));
   Container *ctr = &mdl->ctr;

   int status = EXIT_SUCCESS;
   for (unsigned i = 0; i < 8; ++i) {
      printf("Testing small example %u\n", i+1);
      status = run_ex2(ctr, opcodes[i], args[i]);
      if (status != OK) goto _exit;
   }

   /* Open files  */
   if (argc > 1) {
      for (int i = 1; i < argc; ++i) {
         struct gams_opcodes_file* equs = gams_read_opcode(argv[i], &pool2);
         if (!equs) { status = EXIT_FAILURE; goto _exit; };

         struct nltree_pool pool = {equs->pool, equs->pool_len, equs->pool_len, RHP_BACKEND_GAMS_GMO, 1, true };
         ctr->pool = &pool;
         for (size_t j = 0; j < equs->nb_equs; ++j) {
            size_t start = equs->indx_start[j];
            int _status = run_ex2(ctr, &equs->instrs[start], &equs->args[start]);
            if (_status != OK) { status = _status; }
         }
         gams_opcodes_file_dealloc(equs);
         ctr->pool = NULL;
      }
   }

_exit:
   rhp_mdl_free(mdl);

   return status;
}
