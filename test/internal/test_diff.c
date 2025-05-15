#include <stdio.h>

#include "gams_nlutils.h"
#include "instr.h"
#include "macros.h"
#include "open_lib.h"
#include "opcode_diff_ops.h"
#include "printout.h"
#include "status.h"
//#include "tlsdef.h"

#include "test_instr.h"
#include "test_diff_answer.h"

#define compute_opcode_diff opcode_diff

#if 0
/* This is from gams_diff_ops.c  */
static tlsvar int (*compute_opcode_diff)(int **, int **, int *, const int *, const int *, int, char *) = NULL;
static tlsvar void* libopcodediff_handle = NULL;

#if defined(_WIN32) && defined(_MSC_VER) && !defined(__clang__)

  int __stdcall DllMain(HINSTANCE hinstDLL, DWORD fdwReason, LPVOID lpReserved)
  {
      if (libopcodediff_handle)
      {
        FreeLibrary(libopcodediff_handle);
        libopcodediff_handle = NULL;
      }
    }
    return true;
  }

#elif defined(__GNUC__) & !defined(__APPLE__)
#include <dlfcn.h>

  __attribute__ ((destructor)) static void cleanup_opcode_diff(void) 
  {
    if (libopcodediff_handle)
    {
       dlclose(libopcodediff_handle);
       libopcodediff_handle = NULL;
    }
  }
#endif

static int _load_opcode_diff(void)
{
   if (!compute_opcode_diff) {
      if (!libopcodediff_handle) {
         libopcodediff_handle = open_library(DLL_FROM_NAME("opcode_diff"), 0);
         if (!libopcodediff_handle) {
            error("%s :: Could not find "DLL_FROM_NAME("opcode_diff")". "
                     "Some functionalities may not be available\n", __func__);
            return Error_SystemError;
         }
      }

      *(void **) (&compute_opcode_diff) = get_function_address(libopcodediff_handle, "opcode_diff");
      if (!compute_opcode_diff) {
         error("%s :: Could not find function named opcode_diff"
                  " in " DLL_FROM_NAME("opcode_diff")". Some functionalities may not "
                  "be available\n", __func__);
         return Error_SystemError;
      }
   }

   return OK;
}
#endif

static void opcode_print(int *instrs, int *args, int len)
{
   for (int i = 0; i < len; ++i) {
      printf("Indx [%5d]\t  Instr: %10s, Field: %5d, \n", i,
             nlinstr2str(instrs[i]), args[i]);

   }
}

static int run_ex(int *instrs, int *args, UNUSED int codelen, int vidx,
                  int *ex_diff_instrs, int *ex_diff_args,
                  unsigned dcode_size)
{
   int status = OK;

   int *d_instrs = NULL;
   int *d_args = NULL;
   int  d_codelen = 0;

   char errmsg[256];

   int rc = compute_opcode_diff(&d_instrs, &d_args, &d_codelen, instrs, args, vidx, errmsg);

   if (rc != EXIT_SUCCESS) {
      printf("ERROR: return code %d from compute_opcode_diff. Message is %s\n", rc, errmsg);
      status = Error_InvalidValue;
      goto _exit;
   }

   d_codelen = d_args[0];

   if (dcode_size == 0) return OK;

   if (d_codelen != (int)dcode_size) {
      printf("ERROR: d_codelen = %5d, dcode_size = %5u\n",
             d_codelen, dcode_size);
      opcode_print(d_instrs, d_args, d_codelen);
      status = Error_InvalidValue;
      goto _exit;
   }
   if (!ex_diff_instrs) { goto _exit; }

   for (size_t i = 0; i < dcode_size; i++) {
      if ((d_instrs[i] != ex_diff_instrs[i])
          || (d_args[i] != ex_diff_args[i])) {
         printf("ERROR:\n");
         printf("   opcode = %5d field = %5d\n",
                d_instrs[i], d_args[i]);
         printf("   opcode = %5d field = %5d\n",
                ex_diff_instrs[i], ex_diff_args[i]);
         status = Error_InvalidValue;
         goto _exit;
      }
   }

_exit:
   FREE(d_instrs);
   FREE(d_args);

   return status;
}

static int run_ex2(int *instrs, int *args)
{
   int codelen = args[0];

   for (int i = 0; i < codelen; ++i)
   {
      switch (instrs[i])
      {
      case nlPushV:
      case nlAddV:
      case nlSubV:
      case nlMulV:
      case nlDivV:
      case nlUMinV:
         run_ex(instrs, args, codelen, args[i], NULL, NULL, 0);
         break;
      default:
         break;
   }
   }

   return OK;
}

int main(int argc, const char **argv)
{
   double *pool, *pool2;
   int size, dcode_size, status;

#if 0
   S_CHECK(_load_opcode_diff());
#endif
   MALLOC(pool, double, 100);
   pool[0] = 1;
   pool[5] = 2;
   pool[14] = 3;

   /* Open files  */
   if (argc > 1)
   {
      for (int i = 1; i < argc; ++i)
      {
         struct gams_opcodes_file* equs = gams_read_opcode(argv[i], &pool2);
         if (!equs) { status = EXIT_FAILURE; goto _exit; };

         for (size_t j = 0; j < equs->nb_equs; ++j)
         {
            run_ex2(&equs->instrs[equs->indx_start[j]], &equs->args[equs->indx_start[j]]);
         }
         gams_opcodes_file_dealloc(equs);
      }
   }


   printf("Testing Ex1: d(-x2*x2) / dx2                  . . .  ");
   size = sizeof(ex1_opcodes) / sizeof(int);
   dcode_size = sizeof(ex1_diff_opcodes) / sizeof(int);
   status = run_ex(ex1_opcodes, ex1_args, size, 2,
                   ex1_diff_opcodes, ex1_diff_fields, dcode_size);
   printf("%s\n", (status == OK) ? "OK" : "FAIL");

   printf("Testing Ex2: d(-(x2*x2 + x2*x3)) / dx2        . . .  ");
   size = sizeof(ex2_opcodes) / sizeof(int);
   dcode_size = sizeof(ex2_diff_opcodes) / sizeof(int);
   status = run_ex(ex2_opcodes, ex2_args, size, 2,
                   ex2_diff_opcodes, ex2_diff_fields, dcode_size);
   printf("%s\n", (status == OK) ? "OK" : "FAIL");

   printf("Testing Ex3: d(-(x2*x2 + exp(x2))) / dx2      . . .  ");
   size = sizeof(ex3_opcodes) / sizeof(int);
   dcode_size = sizeof(ex3_diff_opcodes) / sizeof(int);
   status = run_ex(ex3_opcodes, ex3_args, size, 2,
                   ex3_diff_opcodes, ex3_diff_fields, dcode_size);
   printf("%s\n", (status == OK) ? "OK" : "FAIL");

   printf("Testing Ex4: d(-(3*x2**2)) / dx2              . . .  ");
   size = sizeof(ex4_opcodes) / sizeof(int);
   dcode_size = sizeof(ex4_diff_opcodes) / sizeof(int);
   status = run_ex(ex4_opcodes, ex4_args, size, 2,
                   ex4_diff_opcodes, ex4_diff_fields, dcode_size);
   printf("%s\n", (status == OK) ? "OK" : "FAIL");

   printf("Testing Ex5: d(-(3*x2**2 + exp(x2*x3))) / dx2 . . .  ");
   size = sizeof(ex5_opcodes) / sizeof(int);
   dcode_size = sizeof(ex5_diff_opcodes) / sizeof(int);
   status = run_ex(ex5_opcodes, ex5_args, size, 2,
                   ex5_diff_opcodes, ex5_diff_fields, dcode_size);
   printf("%s\n", (status == OK) ? "OK" : "FAIL");

   printf("Testing Ex6: d(-(x2/(1+x3)) / dx3             . . .  ");
   size = sizeof(ex6_opcodes) / sizeof(int);
   dcode_size = sizeof(ex6_diff_opcodes) / sizeof(int);
   status = run_ex(ex6_opcodes, ex6_args, size, 3,
                   ex6_diff_opcodes, ex6_diff_fields, dcode_size);
   printf("%s\n", (status == OK) ? "OK" : "FAIL");

   printf("Testing Ex7: d(-(log(x2)) / dx2               . . .  ");
   size = sizeof(ex7_opcodes) / sizeof(int);
   dcode_size = sizeof(ex7_diff_opcodes) / sizeof(int);
   status = run_ex(ex7_opcodes, ex7_args, size, 2,
                   ex7_diff_opcodes, ex7_diff_fields, dcode_size);
   printf("%s\n", (status == OK) ? "OK" : "FAIL");

_exit:

   FREE(pool);
   return status;
}
